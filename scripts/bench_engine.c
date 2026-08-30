#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/syscall.h>

static double get_time_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static void detect_environment(char *out_engine, size_t max_len, bool *out_jit, bool *out_vdisk) {
    *out_jit = false;
    *out_vdisk = false;
    snprintf(out_engine, max_len, "Native / Unknown");

    // Check maps for JIT library
    FILE *maps = fopen("/proc/self/maps", "r");
    if (maps) {
        char line[512];
        while (fgets(line, sizeof(line), maps)) {
            if (strstr(line, "libimproot-jit") || strstr(line, ".proot.jit.so")) {
                *out_jit = true;
            }
            if (strstr(line, "vdisk") || strstr(line, ".vdisk_cache")) {
                *out_vdisk = true;
            }
        }
        fclose(maps);
    }

    // Check TracerPid in /proc/self/status
    FILE *status = fopen("/proc/self/status", "r");
    pid_t tracer_pid = 0;
    if (status) {
        char line[256];
        while (fgets(line, sizeof(line), status)) {
            if (strncmp(line, "TracerPid:", 10) == 0) {
                tracer_pid = atoi(line + 10);
                break;
            }
        }
        fclose(status);
    }

    if (tracer_pid > 0) {
        char cmdpath[64], cmdline[512] = {0};
        snprintf(cmdpath, sizeof(cmdpath), "/proc/%d/cmdline", tracer_pid);
        int fd = open(cmdpath, O_RDONLY);
        if (fd >= 0) {
            ssize_t n = read(fd, cmdline, sizeof(cmdline) - 1);
            close(fd);
            if (n > 0) {
                cmdline[n] = '\0';
                if (strstr(cmdline, "improot") || strstr(cmdline, "Improot")) {
                    snprintf(out_engine, max_len, "Improot (PID %d)", tracer_pid);
                } else if (strstr(cmdline, "proot")) {
                    snprintf(out_engine, max_len, "PRoot (PID %d)", tracer_pid);
                } else {
                    snprintf(out_engine, max_len, "Ptraced (%s, PID %d)", cmdline, tracer_pid);
                }
            }
        } else {
            snprintf(out_engine, max_len, "Ptraced (PID %d)", tracer_pid);
        }
    } else {
        snprintf(out_engine, max_len, "Native Linux (No Ptrace)");
    }
}

// 1. Syscall Latency Test
static void bench_syscalls(int iterations, double *out_time, double *out_ops_sec) {
    double t0 = get_time_sec();
    for (int i = 0; i < iterations; i++) {
        syscall(SYS_getpid);
    }
    double t1 = get_time_sec();
    *out_time = (t1 - t0);
    *out_ops_sec = (double)iterations / *out_time;
}

// 2. File Metadata (Create, Stat, Close, Unlink)
static void bench_file_metadata(int iterations, double *out_time, double *out_ops_sec) {
    char path[128];
    snprintf(path, sizeof(path), "/tmp/bench_meta_%d.tmp", getpid());
    
    double t0 = get_time_sec();
    for (int i = 0; i < iterations; i++) {
        int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd >= 0) {
            struct stat st;
            fstat(fd, &st);
            close(fd);
            stat(path, &st);
            unlink(path);
        }
    }
    double t1 = get_time_sec();
    *out_time = (t1 - t0);
    *out_ops_sec = (double)iterations / *out_time;
}

// 3. Path Resolution / Readlink / Canonicalization
static void bench_path_resolution(int iterations, double *out_time, double *out_ops_sec) {
    char link_path[128], target_path[128], buf[256];
    snprintf(link_path, sizeof(link_path), "/tmp/bench_link_%d.lnk", getpid());
    snprintf(target_path, sizeof(target_path), "/tmp/bench_target_%d.txt", getpid());
    
    int fd = open(target_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) close(fd);
    (void)symlink(target_path, link_path);

    double t0 = get_time_sec();
    for (int i = 0; i < iterations; i++) {
        struct stat st;
        lstat(link_path, &st);
        ssize_t len = readlink(link_path, buf, sizeof(buf) - 1);
        (void)len;
    }
    double t1 = get_time_sec();
    unlink(link_path);
    unlink(target_path);

    *out_time = (t1 - t0);
    *out_ops_sec = (double)iterations / *out_time;
}

// 4. Sequential I/O Throughput (Write & Read 32MB in 64KB blocks)
static void bench_io_throughput(size_t total_bytes, size_t block_size, double *out_write_mb_s, double *out_read_mb_s) {
    char path[128];
    snprintf(path, sizeof(path), "/tmp/bench_io_%d.dat", getpid());
    
    char *buf = malloc(block_size);
    memset(buf, 0x5a, block_size);
    size_t blocks = total_bytes / block_size;

    // Write test
    double t0 = get_time_sec();
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        for (size_t i = 0; i < blocks; i++) {
            ssize_t written = write(fd, buf, block_size);
            (void)written;
        }
        close(fd);
    }
    double t1 = get_time_sec();
    *out_write_mb_s = ((double)total_bytes / (1024.0 * 1024.0)) / (t1 - t0);

    // Read test
    double t2 = get_time_sec();
    fd = open(path, O_RDONLY);
    if (fd >= 0) {
        for (size_t i = 0; i < blocks; i++) {
            ssize_t read_bytes = read(fd, buf, block_size);
            (void)read_bytes;
        }
        close(fd);
    }
    double t3 = get_time_sec();
    *out_read_mb_s = ((double)total_bytes / (1024.0 * 1024.0)) / (t3 - t2);

    unlink(path);
    free(buf);
}

// 5. Fork + Wait Process Creation Overhead
static void bench_fork_latency(int iterations, double *out_time, double *out_forks_sec) {
    double t0 = get_time_sec();
    for (int i = 0; i < iterations; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            _exit(0);
        } else if (pid > 0) {
            int status;
            waitpid(pid, &status, 0);
        }
    }
    double t1 = get_time_sec();
    *out_time = (t1 - t0);
    *out_forks_sec = (double)iterations / *out_time;
}

int main(int argc, char **argv) {
    int syscall_iters = 500000;
    int meta_iters = 20000;
    int path_iters = 50000;
    int fork_iters = 2000;
    size_t io_bytes = 32 * 1024 * 1024; // 32 MB

    if (argc > 1 && strcmp(argv[1], "--quick") == 0) {
        syscall_iters /= 5;
        meta_iters /= 5;
        path_iters /= 5;
        fork_iters /= 5;
        io_bytes /= 4;
    }

    char engine_name[128];
    bool jit_active = false, vdisk_active = false;
    detect_environment(engine_name, sizeof(engine_name), &jit_active, &vdisk_active);

    printf("========================================================================\n");
    printf("                  IMPROOT & PROOT BENCHMARK SUITE                       \n");
    printf("========================================================================\n");
    printf(" Environment Detected : %s\n", engine_name);
    printf(" JIT Engine Active    : %s\n", jit_active ? "YES (Hardware Accelerated)" : "NO (PTRACE Fallback)");
    printf(" Virtual Disk (vdisk) : %s\n", vdisk_active ? "YES (Userspace VFS)" : "NO (Host Bound)");
    printf("========================================================================\n\n");

    printf("[1/5] Measuring Syscall Latency (%d getpid iterations)...\n", syscall_iters);
    double sys_time, sys_ops;
    bench_syscalls(syscall_iters, &sys_time, &sys_ops);
    printf("      -> Time: %.4f s | Throughput: %.2f ops/sec | Latency: %.2f ns/op\n\n",
           sys_time, sys_ops, (sys_time / (double)syscall_iters) * 1e9);

    printf("[2/5] Measuring File Metadata (%d create+stat+unlink iterations)...\n", meta_iters);
    double meta_time, meta_ops;
    bench_file_metadata(meta_iters, &meta_time, &meta_ops);
    printf("      -> Time: %.4f s | Throughput: %.2f ops/sec | Latency: %.2f us/op\n\n",
           meta_time, meta_ops, (meta_time / (double)meta_iters) * 1e6);

    printf("[3/5] Measuring Path Canonicalization (%d lstat+readlink iterations)...\n", path_iters);
    double path_time, path_ops;
    bench_path_resolution(path_iters, &path_time, &path_ops);
    printf("      -> Time: %.4f s | Throughput: %.2f ops/sec | Latency: %.2f us/op\n\n",
           path_time, path_ops, (path_time / (double)path_iters) * 1e6);

    printf("[4/5] Measuring I/O Bandwidth (%.1f MB payload)...\n", (double)io_bytes / (1024.0 * 1024.0));
    double write_mb, read_mb;
    bench_io_throughput(io_bytes, 64 * 1024, &write_mb, &read_mb);
    printf("      -> Write Throughput: %.2f MB/s | Read Throughput: %.2f MB/s\n\n", write_mb, read_mb);

    printf("[5/5] Measuring Process Creation (%d fork+exit iterations)...\n", fork_iters);
    double fork_time, fork_ops;
    bench_fork_latency(fork_iters, &fork_time, &fork_ops);
    printf("      -> Time: %.4f s | Throughput: %.2f forks/sec | Latency: %.2f us/fork\n\n",
           fork_time, fork_ops, (fork_time / (double)fork_iters) * 1e6);

    printf("========================================================================\n");
    printf("                         BENCHMARK RESULTS TABLE                        \n");
    printf("========================================================================\n");
    printf(" %-30s | %-16s | %-16s\n", "Benchmark Test", "Throughput", "Latency");
    printf("--------------------------------+------------------+--------------------\n");
    printf(" %-30s | %13.2f ops/s | %13.2f ns\n", "Raw Syscalls (getpid)", sys_ops, (sys_time / (double)syscall_iters) * 1e9);
    printf(" %-30s | %13.2f ops/s | %13.2f us\n", "File Metadata (create/stat)", meta_ops, (meta_time / (double)meta_iters) * 1e6);
    printf(" %-30s | %13.2f ops/s | %13.2f us\n", "Path Resolution (symlink/stat)", path_ops, (path_time / (double)path_iters) * 1e6);
    printf(" %-30s | %13.2f MB/s  | %13s\n", "Sequential Write (64KB block)", write_mb, "-");
    printf(" %-30s | %13.2f MB/s  | %13s\n", "Sequential Read  (64KB block)", read_mb, "-");
    printf(" %-30s | %13.2f forks/s| %13.2f us\n", "Process Creation (fork+wait)", fork_ops, (fork_time / (double)fork_iters) * 1e6);
    printf("========================================================================\n");

    return 0;
}
