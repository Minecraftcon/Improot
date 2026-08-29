#include "shm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <unistd.h>

JitShmHeader *global_jit_shm = NULL;
int global_jit_shm_fd = -1;

#ifndef __NR_memfd_create
#if defined(__x86_64__)
#define __NR_memfd_create 319
#elif defined(__i386__)
#define __NR_memfd_create 356
#elif defined(__aarch64__)
#define __NR_memfd_create 279
#elif defined(__arm__)
#define __NR_memfd_create 385
#else
#warning "Unsupported architecture for memfd_create, will rely on fallbacks."
#endif
#endif

#define ASHMEM_SET_NAME _IOW(0x77, 1, char[256])
#define ASHMEM_SET_SIZE _IOW(0x77, 3, size_t)

static uint32_t hash_str(const char *str) {
    uint32_t hash = 2166136261u;
    while (*str) {
        hash ^= (uint8_t)(*str++);
        hash *= 16777619;
    }
    return hash;
}

int jit_shm_init(void) {
    int fd = -1;
    size_t size = sizeof(JitShmHeader);

#ifdef __NR_memfd_create
    fd = syscall(__NR_memfd_create, "improot_jit", 0);
#endif

    if (fd < 0) {
        /* Fallback to ashmem (Android) */
        fd = open("/dev/ashmem", O_RDWR);
        if (fd >= 0) {
            ioctl(fd, ASHMEM_SET_NAME, "improot_jit");
            ioctl(fd, ASHMEM_SET_SIZE, size);
        } else {
            /* Fallback to tmpfile */
            char template[] = "/tmp/improot_jit_XXXXXX";
            fd = mkstemp(template);
            if (fd >= 0) {
                unlink(template);
            }
        }
    }

    if (fd >= 0) {
        if (ftruncate(fd, size) < 0) {
            /* Ignored if ashmem, handled by ASHMEM_SET_SIZE */
        }
        
        JitShmHeader *hdr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (hdr != MAP_FAILED) {
            memset(hdr, 0, size);
            global_jit_shm = hdr;
            global_jit_shm_fd = fd;
        } else {
            close(fd);
            fd = -1;
        }
    }

    return fd;
}

JitShmHeader *jit_shm_map(int fd) {
    if (fd < 0) return NULL;
    void *ptr = mmap(NULL, sizeof(JitShmHeader), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) return NULL;
    return (JitShmHeader *)ptr;
}

void jit_shm_insert(JitShmHeader *hdr, const char *guest_path, const char *host_path) {
    if (!hdr) return;
    
    uint32_t hash = hash_str(guest_path);
    uint32_t idx = hash % JIT_SHM_ENTRIES;

    /* Start Writer Seqlock */
    uint32_t v = __atomic_load_n(&hdr->seqlock, __ATOMIC_SEQ_CST);
    __atomic_store_n(&hdr->seqlock, v + 1, __ATOMIC_SEQ_CST);

    /* Very simple insertion: just overwrite whatever is at idx (Cache eviction) */
    hdr->entries[idx].hash = hash;
    strncpy(hdr->entries[idx].guest_path, guest_path, JIT_SHM_PATH_MAX - 1);
    hdr->entries[idx].guest_path[JIT_SHM_PATH_MAX - 1] = '\0';
    strncpy(hdr->entries[idx].host_path, host_path, JIT_SHM_PATH_MAX - 1);
    hdr->entries[idx].host_path[JIT_SHM_PATH_MAX - 1] = '\0';
    hdr->entries[idx].active = 1;

    /* End Writer Seqlock */
    __atomic_store_n(&hdr->seqlock, v + 2, __ATOMIC_SEQ_CST);
}

bool jit_shm_lookup(JitShmHeader *hdr, const char *guest_path, char *host_path_out) {
    if (!hdr) return false;

    uint32_t hash = hash_str(guest_path);
    uint32_t idx = hash % JIT_SHM_ENTRIES;

    uint32_t seq1, seq2;
    int retries = 0;
    JitShmEntry entry_copy;

    do {
        seq1 = __atomic_load_n(&hdr->seqlock, __ATOMIC_SEQ_CST);
        if (seq1 & 1) { 
            /* Writer is writing, yield and retry */
            usleep(1);
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
        strcmp(entry_copy.guest_path, guest_path) == 0) {
        strcpy(host_path_out, entry_copy.host_path);
        return true;
    }

    return false;
}

void jit_shm_clear(JitShmHeader *hdr) {
    if (!hdr) return;

    /* Start Writer Seqlock */
    uint32_t v = __atomic_load_n(&hdr->seqlock, __ATOMIC_SEQ_CST);
    __atomic_store_n(&hdr->seqlock, v + 1, __ATOMIC_SEQ_CST);

    /* Clear */
    for (int i = 0; i < JIT_SHM_ENTRIES; i++) {
        hdr->entries[i].active = 0;
    }

    /* End Writer Seqlock */
    __atomic_store_n(&hdr->seqlock, v + 2, __ATOMIC_SEQ_CST);
}
