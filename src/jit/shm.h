#ifndef JIT_SHM_H
#define JIT_SHM_H

#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>

#define JIT_SHM_ENTRIES 1024
#define JIT_SHM_PATH_MAX 256

typedef struct {
    uint32_t active;
    uint32_t hash;
    char guest_path[JIT_SHM_PATH_MAX];
    char host_path[JIT_SHM_PATH_MAX];
} JitShmEntry;

typedef struct {
    uint32_t seqlock;
    JitShmEntry entries[JIT_SHM_ENTRIES];
} JitShmHeader;

/* Global reference to the JIT cache for the Tracer process. */
extern JitShmHeader *global_jit_shm;
extern int global_jit_shm_fd;

/* Initialize the SHM manager (Tracer only). Returns the file descriptor. */
int jit_shm_init(void);

/* Map the SHM from an existing file descriptor (Tracer and Tracee). */
JitShmHeader *jit_shm_map(int fd);

/* Insert a mapping into the JIT SHM (Tracer only). */
void jit_shm_insert(JitShmHeader *hdr, const char *guest_path, const char *host_path);

/* Lookup a mapping in the JIT SHM (Tracee only). 
 * Returns true if found and copies to host_path_out. */
bool jit_shm_lookup(JitShmHeader *hdr, const char *guest_path, char *host_path_out);

/* Clear the entire cache (Tracer only). */
void jit_shm_clear(JitShmHeader *hdr);

#endif
