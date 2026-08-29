#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <stdarg.h>

#include "shm.h"

/* Magic DIRFD recognized by the Improot Seccomp-BPF filter */
#define MAGIC_DIRFD 0x7BADF00D

static JitShmHeader *jit_shm = NULL;

/* Original function pointers */
static int (*real_open)(const char *pathname, int flags, ...);
static int (*real_open64)(const char *pathname, int flags, ...);
static int (*real_openat)(int dirfd, const char *pathname, int flags, ...);
static int (*real_openat64)(int dirfd, const char *pathname, int flags, ...);
static int (*real_stat)(const char *pathname, struct stat *statbuf);
static int (*real_lstat)(const char *pathname, struct stat *statbuf);
static int (*real_fstatat)(int dirfd, const char *pathname, struct stat *statbuf, int flags);
static int (*real_access)(const char *pathname, int mode);
static int (*real_faccessat)(int dirfd, const char *pathname, int mode, int flags);
static ssize_t (*real_readlink)(const char *pathname, char *buf, size_t bufsiz);
static ssize_t (*real_readlinkat)(int dirfd, const char *pathname, char *buf, size_t bufsiz);
/* 64-bit stat structs */
#if defined(_LARGEFILE64_SOURCE)
static int (*real_stat64)(const char *pathname, struct stat64 *statbuf);
static int (*real_lstat64)(const char *pathname, struct stat64 *statbuf);
static int (*real_fstatat64)(int dirfd, const char *pathname, struct stat64 *statbuf, int flags);
#endif

__attribute__((constructor))
static void jit_lib_init(void) {
    const char *fd_str = getenv("IMPROOT_JIT_FD");
    if (fd_str) {
        int fd = atoi(fd_str);
        if (fd >= 0) {
            jit_shm = jit_shm_map(fd);
        }
    }

    real_open = dlsym(RTLD_NEXT, "open");
    real_open64 = dlsym(RTLD_NEXT, "open64");
    real_openat = dlsym(RTLD_NEXT, "openat");
    real_openat64 = dlsym(RTLD_NEXT, "openat64");
    real_stat = dlsym(RTLD_NEXT, "stat");
    real_lstat = dlsym(RTLD_NEXT, "lstat");
    real_fstatat = dlsym(RTLD_NEXT, "fstatat");
    real_access = dlsym(RTLD_NEXT, "access");
    real_faccessat = dlsym(RTLD_NEXT, "faccessat");
    real_readlink = dlsym(RTLD_NEXT, "readlink");
    real_readlinkat = dlsym(RTLD_NEXT, "readlinkat");
#if defined(_LARGEFILE64_SOURCE)
    real_stat64 = dlsym(RTLD_NEXT, "stat64");
    real_lstat64 = dlsym(RTLD_NEXT, "lstat64");
    real_fstatat64 = dlsym(RTLD_NEXT, "fstatat64");
#endif
}

/* --------------------------------------------------------------------------
 *  open hooks 
 * -------------------------------------------------------------------------- */

int open(const char *pathname, int flags, ...) {
    mode_t mode = 0;
    if (flags & (O_CREAT | __O_TMPFILE)) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, mode_t);
        va_end(args);
    }
    char host_path[JIT_SHM_PATH_MAX];
    if (jit_shm && pathname && pathname[0] == '/' && jit_shm_lookup(jit_shm, pathname, host_path)) {
        return syscall(SYS_openat, MAGIC_DIRFD, host_path, flags, mode);
    }
    if (!real_open) real_open = dlsym(RTLD_NEXT, "open");
    return real_open(pathname, flags, mode);
}

int open64(const char *pathname, int flags, ...) {
    mode_t mode = 0;
    if (flags & (O_CREAT | __O_TMPFILE)) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, mode_t);
        va_end(args);
    }
    char host_path[JIT_SHM_PATH_MAX];
    if (jit_shm && pathname && pathname[0] == '/' && jit_shm_lookup(jit_shm, pathname, host_path)) {
        return syscall(SYS_openat, MAGIC_DIRFD, host_path, flags, mode);
    }
    if (!real_open64) real_open64 = dlsym(RTLD_NEXT, "open64");
    return real_open64(pathname, flags, mode);
}

int openat(int dirfd, const char *pathname, int flags, ...) {
    mode_t mode = 0;
    if (flags & (O_CREAT | __O_TMPFILE)) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, mode_t);
        va_end(args);
    }
    char host_path[JIT_SHM_PATH_MAX];
    if (jit_shm && pathname && pathname[0] == '/' && jit_shm_lookup(jit_shm, pathname, host_path)) {
        return syscall(SYS_openat, MAGIC_DIRFD, host_path, flags, mode);
    }
    if (!real_openat) real_openat = dlsym(RTLD_NEXT, "openat");
    return real_openat(dirfd, pathname, flags, mode);
}

int openat64(int dirfd, const char *pathname, int flags, ...) {
    mode_t mode = 0;
    if (flags & (O_CREAT | __O_TMPFILE)) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, mode_t);
        va_end(args);
    }
    char host_path[JIT_SHM_PATH_MAX];
    if (jit_shm && pathname && pathname[0] == '/' && jit_shm_lookup(jit_shm, pathname, host_path)) {
        return syscall(SYS_openat, MAGIC_DIRFD, host_path, flags, mode);
    }
    if (!real_openat64) real_openat64 = dlsym(RTLD_NEXT, "openat64");
    return real_openat64(dirfd, pathname, flags, mode);
}

/* --------------------------------------------------------------------------
 *  stat hooks 
 * -------------------------------------------------------------------------- */

int stat(const char *pathname, struct stat *statbuf) {
    char host_path[JIT_SHM_PATH_MAX];
    if (jit_shm && pathname && pathname[0] == '/' && jit_shm_lookup(jit_shm, pathname, host_path)) {
        return syscall(SYS_newfstatat, MAGIC_DIRFD, host_path, statbuf, 0);
    }
    if (!real_stat) real_stat = dlsym(RTLD_NEXT, "stat");
    return real_stat(pathname, statbuf);
}

int lstat(const char *pathname, struct stat *statbuf) {
    char host_path[JIT_SHM_PATH_MAX];
    if (jit_shm && pathname && pathname[0] == '/' && jit_shm_lookup(jit_shm, pathname, host_path)) {
        return syscall(SYS_newfstatat, MAGIC_DIRFD, host_path, statbuf, AT_SYMLINK_NOFOLLOW);
    }
    if (!real_lstat) real_lstat = dlsym(RTLD_NEXT, "lstat");
    return real_lstat(pathname, statbuf);
}

int fstatat(int dirfd, const char *pathname, struct stat *statbuf, int flags) {
    char host_path[JIT_SHM_PATH_MAX];
    if (jit_shm && pathname && pathname[0] == '/' && jit_shm_lookup(jit_shm, pathname, host_path)) {
        return syscall(SYS_newfstatat, MAGIC_DIRFD, host_path, statbuf, flags);
    }
    if (!real_fstatat) real_fstatat = dlsym(RTLD_NEXT, "fstatat");
    return real_fstatat(dirfd, pathname, statbuf, flags);
}

/* --------------------------------------------------------------------------
 *  access hooks 
 * -------------------------------------------------------------------------- */

int access(const char *pathname, int mode) {
    char host_path[JIT_SHM_PATH_MAX];
    if (jit_shm && pathname && pathname[0] == '/' && jit_shm_lookup(jit_shm, pathname, host_path)) {
        return syscall(SYS_faccessat, MAGIC_DIRFD, host_path, mode, 0);
    }
    if (!real_access) real_access = dlsym(RTLD_NEXT, "access");
    return real_access(pathname, mode);
}

int faccessat(int dirfd, const char *pathname, int mode, int flags) {
    char host_path[JIT_SHM_PATH_MAX];
    if (jit_shm && pathname && pathname[0] == '/' && jit_shm_lookup(jit_shm, pathname, host_path)) {
#ifdef SYS_faccessat2
        if (flags != 0) {
            return syscall(SYS_faccessat2, MAGIC_DIRFD, host_path, mode, flags);
        }
#endif
        return syscall(SYS_faccessat, MAGIC_DIRFD, host_path, mode, 0);
    }
    if (!real_faccessat) real_faccessat = dlsym(RTLD_NEXT, "faccessat");
    return real_faccessat(dirfd, pathname, mode, flags);
}

/* --------------------------------------------------------------------------
 *  readlink hooks 
 * -------------------------------------------------------------------------- */

ssize_t readlink(const char *pathname, char *buf, size_t bufsiz) {
    char host_path[JIT_SHM_PATH_MAX];
    if (jit_shm && pathname && pathname[0] == '/' && jit_shm_lookup(jit_shm, pathname, host_path)) {
        return syscall(SYS_readlinkat, MAGIC_DIRFD, host_path, buf, bufsiz);
    }
    if (!real_readlink) real_readlink = dlsym(RTLD_NEXT, "readlink");
    return real_readlink(pathname, buf, bufsiz);
}

ssize_t readlinkat(int dirfd, const char *pathname, char *buf, size_t bufsiz) {
    char host_path[JIT_SHM_PATH_MAX];
    if (jit_shm && pathname && pathname[0] == '/' && jit_shm_lookup(jit_shm, pathname, host_path)) {
        return syscall(SYS_readlinkat, MAGIC_DIRFD, host_path, buf, bufsiz);
    }
    if (!real_readlinkat) real_readlinkat = dlsym(RTLD_NEXT, "readlinkat");
    return real_readlinkat(dirfd, pathname, buf, bufsiz);
}
