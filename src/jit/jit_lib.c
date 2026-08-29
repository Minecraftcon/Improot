#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>

#include "shm.h"

#define MAGIC_DIRFD 0x7BADF00D
#define AT_FDCWD    -100
#define AT_SYMLINK_NOFOLLOW 0x100

typedef unsigned int mode_t;
typedef long ssize_t;

/* Forward declare errno functions across glibc, musl, and bionic */
extern int *__errno_location(void) __attribute__((weak));
extern int *__errno(void) __attribute__((weak));

static inline void set_errno(long err) {
    if (err < 0 && err >= -4095) {
        if (__errno_location) {
            *__errno_location() = (int)-err;
        } else if (__errno) {
            *__errno() = (int)-err;
        }
    }
}

/* --------------------------------------------------------------------------
 *  Architecture-Specific Raw Syscalls (Zero libc dependency)
 * -------------------------------------------------------------------------- */

#if defined(__x86_64__)
#define SYS_READ        0
#define SYS_CLOSE       3
#define SYS_OPENAT      257
#define SYS_FSTATAT     262
#define SYS_FACCESSAT   269
#define SYS_FACCESSAT2  439
#define SYS_READLINKAT  267
#define SYS_MMAP        9

static inline long raw_syscall1(long n, long a1) {
    long ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(n), "D"(a1) : "rcx", "r11", "memory");
    return ret;
}
static inline long raw_syscall2(long n, long a1, long a2) {
    long ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2) : "rcx", "r11", "memory");
    return ret;
}
static inline long raw_syscall3(long n, long a1, long a2, long a3) {
    long ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2), "d"(a3) : "rcx", "r11", "memory");
    return ret;
}
static inline long raw_syscall4(long n, long a1, long a2, long a3, long a4) {
    long ret;
    register long r10 __asm__("r10") = a4;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r10) : "rcx", "r11", "memory");
    return ret;
}
static inline long raw_syscall5(long n, long a1, long a2, long a3, long a4, long a5) {
    long ret;
    register long r10 __asm__("r10") = a4;
    register long r8 __asm__("r8") = a5;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8) : "rcx", "r11", "memory");
    return ret;
}
static inline long raw_syscall6(long n, long a1, long a2, long a3, long a4, long a5, long a6) {
    long ret;
    register long r10 __asm__("r10") = a4;
    register long r8 __asm__("r8") = a5;
    register long r9 __asm__("r9") = a6;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8), "r"(r9) : "rcx", "r11", "memory");
    return ret;
}

#elif defined(__aarch64__)
#define SYS_READ        63
#define SYS_CLOSE       57
#define SYS_OPENAT      56
#define SYS_FSTATAT     79
#define SYS_FACCESSAT   48
#define SYS_FACCESSAT2  439
#define SYS_READLINKAT  78
#define SYS_MMAP        222

static inline long raw_syscall1(long n, long a1) {
    register long x8 __asm__("x8") = n;
    register long x0 __asm__("x0") = a1;
    __asm__ volatile ("svc #0" : "=r"(x0) : "r"(x8), "0"(x0) : "memory");
    return x0;
}
static inline long raw_syscall2(long n, long a1, long a2) {
    register long x8 __asm__("x8") = n;
    register long x0 __asm__("x0") = a1;
    register long x1 __asm__("x1") = a2;
    __asm__ volatile ("svc #0" : "=r"(x0) : "r"(x8), "0"(x0), "r"(x1) : "memory");
    return x0;
}
static inline long raw_syscall3(long n, long a1, long a2, long a3) {
    register long x8 __asm__("x8") = n;
    register long x0 __asm__("x0") = a1;
    register long x1 __asm__("x1") = a2;
    register long x2 __asm__("x2") = a3;
    __asm__ volatile ("svc #0" : "=r"(x0) : "r"(x8), "0"(x0), "r"(x1), "r"(x2) : "memory");
    return x0;
}
static inline long raw_syscall4(long n, long a1, long a2, long a3, long a4) {
    register long x8 __asm__("x8") = n;
    register long x0 __asm__("x0") = a1;
    register long x1 __asm__("x1") = a2;
    register long x2 __asm__("x2") = a3;
    register long x3 __asm__("x3") = a4;
    __asm__ volatile ("svc #0" : "=r"(x0) : "r"(x8), "0"(x0), "r"(x1), "r"(x2), "r"(x3) : "memory");
    return x0;
}
static inline long raw_syscall5(long n, long a1, long a2, long a3, long a4, long a5) {
    register long x8 __asm__("x8") = n;
    register long x0 __asm__("x0") = a1;
    register long x1 __asm__("x1") = a2;
    register long x2 __asm__("x2") = a3;
    register long x3 __asm__("x3") = a4;
    register long x4 __asm__("x4") = a5;
    __asm__ volatile ("svc #0" : "=r"(x0) : "r"(x8), "0"(x0), "r"(x1), "r"(x2), "r"(x3), "r"(x4) : "memory");
    return x0;
}
static inline long raw_syscall6(long n, long a1, long a2, long a3, long a4, long a5, long a6) {
    register long x8 __asm__("x8") = n;
    register long x0 __asm__("x0") = a1;
    register long x1 __asm__("x1") = a2;
    register long x2 __asm__("x2") = a3;
    register long x3 __asm__("x3") = a4;
    register long x4 __asm__("x4") = a5;
    register long x5 __asm__("x5") = a6;
    __asm__ volatile ("svc #0" : "=r"(x0) : "r"(x8), "0"(x0), "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5) : "memory");
    return x0;
}

#elif defined(__arm__)
#define SYS_READ        3
#define SYS_CLOSE       6
#define SYS_OPENAT      322
#define SYS_FSTATAT     327
#define SYS_FACCESSAT   334
#define SYS_FACCESSAT2  439
#define SYS_READLINKAT  332
#define SYS_MMAP        192

static inline long raw_syscall1(long n, long a1) {
    register long r7 __asm__("r7") = n;
    register long r0 __asm__("r0") = a1;
    __asm__ volatile ("svc #0" : "=r"(r0) : "r"(r7), "0"(r0) : "memory");
    return r0;
}
static inline long raw_syscall2(long n, long a1, long a2) {
    register long r7 __asm__("r7") = n;
    register long r0 __asm__("r0") = a1;
    register long r1 __asm__("r1") = a2;
    __asm__ volatile ("svc #0" : "=r"(r0) : "r"(r7), "0"(r0), "r"(r1) : "memory");
    return r0;
}
static inline long raw_syscall3(long n, long a1, long a2, long a3) {
    register long r7 __asm__("r7") = n;
    register long r0 __asm__("r0") = a1;
    register long r1 __asm__("r1") = a2;
    register long r2 __asm__("r2") = a3;
    __asm__ volatile ("svc #0" : "=r"(r0) : "r"(r7), "0"(r0), "r"(r1), "r"(r2) : "memory");
    return r0;
}
static inline long raw_syscall4(long n, long a1, long a2, long a3, long a4) {
    register long r7 __asm__("r7") = n;
    register long r0 __asm__("r0") = a1;
    register long r1 __asm__("r1") = a2;
    register long r2 __asm__("r2") = a3;
    register long r3 __asm__("r3") = a4;
    __asm__ volatile ("svc #0" : "=r"(r0) : "r"(r7), "0"(r0), "r"(r1), "r"(r2), "r"(r3) : "memory");
    return r0;
}
static inline long raw_syscall5(long n, long a1, long a2, long a3, long a4, long a5) {
    register long r7 __asm__("r7") = n;
    register long r0 __asm__("r0") = a1;
    register long r1 __asm__("r1") = a2;
    register long r2 __asm__("r2") = a3;
    register long r3 __asm__("r3") = a4;
    register long r4 __asm__("r4") = a5;
    __asm__ volatile ("svc #0" : "=r"(r0) : "r"(r7), "0"(r0), "r"(r1), "r"(r2), "r"(r3), "r"(r4) : "memory");
    return r0;
}
static inline long raw_syscall6(long n, long a1, long a2, long a3, long a4, long a5, long a6) {
    register long r7 __asm__("r7") = n;
    register long r0 __asm__("r0") = a1;
    register long r1 __asm__("r1") = a2;
    register long r2 __asm__("r2") = a3;
    register long r3 __asm__("r3") = a4;
    register long r4 __asm__("r4") = a5;
    register long r5 __asm__("r5") = a6;
    __asm__ volatile ("svc #0" : "=r"(r0) : "r"(r7), "0"(r0), "r"(r1), "r"(r2), "r"(r3), "r"(r4), "r"(r5) : "memory");
    return r0;
}
#endif

/* --------------------------------------------------------------------------
 *  Freestanding Environment & Init Helpers
 * -------------------------------------------------------------------------- */

static uint32_t hash_str(const char *str) {
    uint32_t hash = 2166136261u;
    while (*str) {
        hash ^= (uint8_t)(*str++);
        hash *= 16777619;
    }
    return hash;
}

static int my_strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

static int my_strncmp(const char *s1, const char *s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (s1[i] != s2[i] || s1[i] == '\0') {
            return (unsigned char)s1[i] - (unsigned char)s2[i];
        }
    }
    return 0;
}

static void my_strcpy(char *dest, const char *src) {
    while ((*dest++ = *src++));
}

static int my_atoi(const char *str) {
    int val = 0;
    while (*str >= '0' && *str <= '9') {
        val = val * 10 + (*str++ - '0');
    }
    return val;
}

extern char **environ __attribute__((weak));

static int get_jit_fd(void) {
    if (environ) {
        const char *key = "IMPROOT_JIT_FD=";
        for (char **env = environ; *env; env++) {
            if (my_strncmp(*env, key, 15) == 0) {
                return my_atoi(*env + 15);
            }
        }
    }

    /* Fallback to /proc/self/environ via direct syscalls */
    long fd = raw_syscall3(SYS_OPENAT, AT_FDCWD, (long)"/proc/self/environ", 0);
    if (fd >= 0) {
        char buf[1024];
        long n = raw_syscall3(SYS_READ, fd, (long)buf, sizeof(buf) - 1);
        raw_syscall1(SYS_CLOSE, fd);
        if (n > 0) {
            buf[n] = '\0';
            for (long i = 0; i + 15 < n; ) {
                if (my_strncmp(&buf[i], "IMPROOT_JIT_FD=", 15) == 0) {
                    return my_atoi(&buf[i + 15]);
                }
                while (i < n && buf[i] != '\0') i++;
                i++;
            }
        }
    }
    return -1;
}

static bool local_jit_shm_lookup(JitShmHeader *hdr, const char *guest_path, char *host_path_out) {
    if (!hdr || !guest_path || !host_path_out) return false;

    uint32_t hash = hash_str(guest_path);
    uint32_t idx = hash & (JIT_SHM_ENTRIES - 1);

    uint32_t seq1, seq2;
    int retries = 0;
    JitShmEntry entry_copy;

    do {
        seq1 = __atomic_load_n(&hdr->seqlock, __ATOMIC_SEQ_CST);
        if (seq1 & 1) { 
            if (retries++ > 10) return false;
            continue; 
        }

        entry_copy = hdr->entries[idx];
        seq2 = __atomic_load_n(&hdr->seqlock, __ATOMIC_SEQ_CST);
        
        if (seq1 == seq2) {
            break;
        }
        if (retries++ > 10) return false;
    } while (1);

    if (entry_copy.active && entry_copy.hash == hash && 
        my_strcmp(entry_copy.guest_path, guest_path) == 0) {
        my_strcpy(host_path_out, entry_copy.host_path);
        return true;
    }

    return false;
}

static JitShmHeader *jit_shm = NULL;
static bool jit_initialized = false;

static void ensure_jit_init(void) {
    if (jit_initialized) return;
    jit_initialized = true;

    int fd = get_jit_fd();
    if (fd >= 0) {
        long addr = raw_syscall6(SYS_MMAP, 0, sizeof(JitShmHeader), 3, 1, fd, 0);
        if (addr > 0 && addr != -1) {
            jit_shm = (JitShmHeader *)addr;
        }
    }
}

__attribute__((constructor))
static void jit_lib_init(void) {
    ensure_jit_init();
}

/* --------------------------------------------------------------------------
 *  Hooks for open / open64 / openat / openat64
 * -------------------------------------------------------------------------- */

int openat(int dirfd, const char *pathname, int flags, ...) {
    ensure_jit_init();
    mode_t mode = 0;
    va_list args;
    va_start(args, flags);
    mode = va_arg(args, mode_t);
    va_end(args);

    char host_path[JIT_SHM_PATH_MAX];
    if (jit_shm && pathname && pathname[0] == '/' && local_jit_shm_lookup(jit_shm, pathname, host_path)) {
        long ret = raw_syscall4(SYS_OPENAT, MAGIC_DIRFD, (long)host_path, flags, mode);
        if (ret < 0) {
            set_errno(ret);
            return -1;
        }
        return (int)ret;
    }

    long ret = raw_syscall4(SYS_OPENAT, dirfd, (long)pathname, flags, mode);
    if (ret < 0) {
        set_errno(ret);
        return -1;
    }
    return (int)ret;
}

int openat64(int dirfd, const char *pathname, int flags, ...) {
    ensure_jit_init();
    mode_t mode = 0;
    va_list args;
    va_start(args, flags);
    mode = va_arg(args, mode_t);
    va_end(args);

    char host_path[JIT_SHM_PATH_MAX];
    if (jit_shm && pathname && pathname[0] == '/' && local_jit_shm_lookup(jit_shm, pathname, host_path)) {
        long ret = raw_syscall4(SYS_OPENAT, MAGIC_DIRFD, (long)host_path, flags, mode);
        if (ret < 0) {
            set_errno(ret);
            return -1;
        }
        return (int)ret;
    }

    long ret = raw_syscall4(SYS_OPENAT, dirfd, (long)pathname, flags, mode);
    if (ret < 0) {
        set_errno(ret);
        return -1;
    }
    return (int)ret;
}

int open(const char *pathname, int flags, ...) {
    ensure_jit_init();
    mode_t mode = 0;
    va_list args;
    va_start(args, flags);
    mode = va_arg(args, mode_t);
    va_end(args);

    char host_path[JIT_SHM_PATH_MAX];
    if (jit_shm && pathname && pathname[0] == '/' && local_jit_shm_lookup(jit_shm, pathname, host_path)) {
        long ret = raw_syscall4(SYS_OPENAT, MAGIC_DIRFD, (long)host_path, flags, mode);
        if (ret < 0) {
            set_errno(ret);
            return -1;
        }
        return (int)ret;
    }

    long ret = raw_syscall4(SYS_OPENAT, AT_FDCWD, (long)pathname, flags, mode);
    if (ret < 0) {
        set_errno(ret);
        return -1;
    }
    return (int)ret;
}

int open64(const char *pathname, int flags, ...) {
    ensure_jit_init();
    mode_t mode = 0;
    va_list args;
    va_start(args, flags);
    mode = va_arg(args, mode_t);
    va_end(args);

    char host_path[JIT_SHM_PATH_MAX];
    if (jit_shm && pathname && pathname[0] == '/' && local_jit_shm_lookup(jit_shm, pathname, host_path)) {
        long ret = raw_syscall4(SYS_OPENAT, MAGIC_DIRFD, (long)host_path, flags, mode);
        if (ret < 0) {
            set_errno(ret);
            return -1;
        }
        return (int)ret;
    }

    long ret = raw_syscall4(SYS_OPENAT, AT_FDCWD, (long)pathname, flags, mode);
    if (ret < 0) {
        set_errno(ret);
        return -1;
    }
    return (int)ret;
}

/* --------------------------------------------------------------------------
 *  Hooks for stat / lstat / fstatat / stat64 / lstat64 / fstatat64
 * -------------------------------------------------------------------------- */

int fstatat(int dirfd, const char *pathname, void *statbuf, int flags) {
    ensure_jit_init();
    char host_path[JIT_SHM_PATH_MAX];
    if (jit_shm && pathname && pathname[0] == '/' && local_jit_shm_lookup(jit_shm, pathname, host_path)) {
        long ret = raw_syscall4(SYS_FSTATAT, MAGIC_DIRFD, (long)host_path, (long)statbuf, flags);
        if (ret < 0) {
            set_errno(ret);
            return -1;
        }
        return (int)ret;
    }

    long ret = raw_syscall4(SYS_FSTATAT, dirfd, (long)pathname, (long)statbuf, flags);
    if (ret < 0) {
        set_errno(ret);
        return -1;
    }
    return (int)ret;
}

int fstatat64(int dirfd, const char *pathname, void *statbuf, int flags) {
    return fstatat(dirfd, pathname, statbuf, flags);
}

int stat(const char *pathname, void *statbuf) {
    return fstatat(AT_FDCWD, pathname, statbuf, 0);
}

int stat64(const char *pathname, void *statbuf) {
    return fstatat(AT_FDCWD, pathname, statbuf, 0);
}

int lstat(const char *pathname, void *statbuf) {
    return fstatat(AT_FDCWD, pathname, statbuf, AT_SYMLINK_NOFOLLOW);
}

int lstat64(const char *pathname, void *statbuf) {
    return fstatat(AT_FDCWD, pathname, statbuf, AT_SYMLINK_NOFOLLOW);
}

/* --------------------------------------------------------------------------
 *  Hooks for access / faccessat
 * -------------------------------------------------------------------------- */

int faccessat(int dirfd, const char *pathname, int mode, int flags) {
    ensure_jit_init();
    char host_path[JIT_SHM_PATH_MAX];
    if (jit_shm && pathname && pathname[0] == '/' && local_jit_shm_lookup(jit_shm, pathname, host_path)) {
#ifdef SYS_FACCESSAT2
        if (flags != 0) {
            long ret = raw_syscall5(SYS_FACCESSAT2, MAGIC_DIRFD, (long)host_path, mode, flags, 0);
            if (ret < 0) {
                set_errno(ret);
                return -1;
            }
            return (int)ret;
        }
#endif
        long ret = raw_syscall3(SYS_FACCESSAT, MAGIC_DIRFD, (long)host_path, mode);
        if (ret < 0) {
            set_errno(ret);
            return -1;
        }
        return (int)ret;
    }

#ifdef SYS_FACCESSAT2
    if (flags != 0) {
        long ret = raw_syscall5(SYS_FACCESSAT2, dirfd, (long)pathname, mode, flags, 0);
        if (ret < 0) {
            set_errno(ret);
            return -1;
        }
        return (int)ret;
    }
#endif
    long ret = raw_syscall3(SYS_FACCESSAT, dirfd, (long)pathname, mode);
    if (ret < 0) {
        set_errno(ret);
        return -1;
    }
    return (int)ret;
}

int access(const char *pathname, int mode) {
    return faccessat(AT_FDCWD, pathname, mode, 0);
}

/* --------------------------------------------------------------------------
 *  Hooks for readlink / readlinkat
 * -------------------------------------------------------------------------- */

ssize_t readlinkat(int dirfd, const char *pathname, char *buf, size_t bufsiz) {
    ensure_jit_init();
    char host_path[JIT_SHM_PATH_MAX];
    if (jit_shm && pathname && pathname[0] == '/' && local_jit_shm_lookup(jit_shm, pathname, host_path)) {
        long ret = raw_syscall4(SYS_READLINKAT, MAGIC_DIRFD, (long)host_path, (long)buf, bufsiz);
        if (ret < 0) {
            set_errno(ret);
            return -1;
        }
        return (ssize_t)ret;
    }

    long ret = raw_syscall4(SYS_READLINKAT, dirfd, (long)pathname, (long)buf, bufsiz);
    if (ret < 0) {
        set_errno(ret);
        return -1;
    }
    return (ssize_t)ret;
}

ssize_t readlink(const char *pathname, char *buf, size_t bufsiz) {
    return readlinkat(AT_FDCWD, pathname, buf, bufsiz);
}
