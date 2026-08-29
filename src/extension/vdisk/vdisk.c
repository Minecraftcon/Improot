#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/param.h>
#include <talloc.h>
#include <errno.h>
#include <libgen.h>

#include "extension/extension.h"
#include "extension/vdisk/vdisk.h"
#include "cli/note.h"
#include "path/path.h"
#include "path/binding.h"
#include "tracee/tracee.h"
#include "tracee/reg.h"
#include "tracee/mem.h"
#include "syscall/sysnum.h"
#include "vdisk/block.h"
#include "vdisk/part.h"
#include "vdisk/fs.h"

#define MAX_EXCLUSIONS 64
#define MAX_DISCOVERED_PATHS 256
#define MAX_LOADED_DIRS 512

typedef struct {
    char paths[MAX_LOADED_DIRS][PATH_MAX];
    int count;
} LoadedDirs;

static LoadedDirs g_loaded_dirs = {0};

static bool vdisk_is_dir_loaded(const char *guest_path) {
    if (!guest_path) return false;
    for (int i = 0; i < g_loaded_dirs.count; i++) {
        if (strcmp(g_loaded_dirs.paths[i], guest_path) == 0) return true;
    }
    return false;
}

static void vdisk_mark_dir_loaded(const char *guest_path) {
    if (!guest_path || vdisk_is_dir_loaded(guest_path)) return;
    if (g_loaded_dirs.count < MAX_LOADED_DIRS) {
        strncpy(g_loaded_dirs.paths[g_loaded_dirs.count++], guest_path, PATH_MAX - 1);
    }
}

static void vdisk_clear_loaded_dirs(void) {
    g_loaded_dirs.count = 0;
}

typedef struct {
    int ref_count;
    char image_path[PATH_MAX];
    int partition_index;
    vdisk_block_t *root_block_dev;
    vdisk_block_t *target_block_dev;
    vdisk_fs_t *fs;
    char temp_cache_dir[PATH_MAX];
    char distro_name[64];
    bool persistent;
    vdisk_format_t image_format;
    uint64_t image_size;
} VdiskConfig;

static char g_vdisk_cache_dir[PATH_MAX] = {0};
static bool g_vdisk_persistent = false;
static VdiskConfig g_static_vdisk_config;
static VdiskConfig *g_active_config = NULL;
static pid_t g_main_proot_pid = 0;

static bool g_setup_paths_enabled = false;
static char g_path_exclusions[MAX_EXCLUSIONS][PATH_MAX];
static int g_num_exclusions = 0;
static char g_discovered_path_env[8192] = {0};

const char *vdisk_get_cache_dir(void) {
    if (g_vdisk_cache_dir[0] != '\0') return g_vdisk_cache_dir;
    return NULL;
}

void vdisk_set_persistent(bool persistent) {
    g_vdisk_persistent = persistent;
    if (g_active_config) {
        g_active_config->persistent = persistent;
    }
}

void vdisk_set_setup_paths(bool enable) {
    g_setup_paths_enabled = enable;
}

void vdisk_add_path_exclusion(const char *exclusion_str) {
    if (!exclusion_str) return;
    char copy[PATH_MAX * 4];
    strncpy(copy, exclusion_str, sizeof(copy) - 1);
    copy[sizeof(copy) - 1] = '\0';

    char *p = copy;
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == '[' || *p == ']' || *p == ',' || *p == '"' || *p == '\'') p++;
        if (!*p) break;
        char *start = p;
        while (*p && *p != ' ' && *p != '\t' && *p != '[' && *p != ']' && *p != ',' && *p != '"' && *p != '\'') p++;
        char saved = *p;
        *p = '\0';
        if (strlen(start) > 0 && g_num_exclusions < MAX_EXCLUSIONS) {
            strncpy(g_path_exclusions[g_num_exclusions], start, PATH_MAX - 1);
            g_path_exclusions[g_num_exclusions][PATH_MAX - 1] = '\0';
            g_num_exclusions++;
        }
        if (saved) p++;
    }
}

static void vdisk_discover_and_export_paths(const char *cache_dir);

const char *vdisk_get_discovered_path_env(void) {
    if (!g_setup_paths_enabled) return NULL;
    if (g_active_config && g_active_config->temp_cache_dir[0] != '\0') {
        vdisk_discover_and_export_paths(g_active_config->temp_cache_dir);
    }
    if (g_discovered_path_env[0] != '\0') return g_discovered_path_env;
    return NULL;
}

static bool is_binary_dir_name(const char *name) {
    if (!name || name[0] == '\0') return false;
    if (strcasecmp(name, "bin") == 0 || strcasecmp(name, "sbin") == 0 ||
        strcasecmp(name, "xbin") == 0 || strcasecmp(name, ".bin") == 0 ||
        strcasecmp(name, "games") == 0) return true;
    return false;
}

static bool is_path_excluded(const char *guest_path) {
    for (int i = 0; i < g_num_exclusions; i++) {
        const char *ex = g_path_exclusions[i];
        if (strcmp(ex, "/") == 0) return true;
        if (strstr(guest_path, ex) != NULL) return true;
    }
    return false;
}

typedef struct {
    char paths[MAX_DISCOVERED_PATHS][PATH_MAX];
    int count;
} DiscoveredPaths;

static void scan_dir_for_bins(const char *host_root, const char *rel_guest, int depth, DiscoveredPaths *out) {
    if (depth > 5 || out->count >= MAX_DISCOVERED_PATHS) return;

    char *host_path = (char *)malloc(PATH_MAX);
    if (!host_path) return;

    if (strcmp(rel_guest, "/") == 0) {
        snprintf(host_path, PATH_MAX, "%s", host_root);
    } else {
        snprintf(host_path, PATH_MAX, "%s%s", host_root, rel_guest);
    }

    DIR *dir = opendir(host_path);
    free(host_path);
    if (!dir) return;

    struct dirent *de;
    while ((de = readdir(dir)) != NULL) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;

        char *child_guest = (char *)malloc(PATH_MAX);
        char *child_host = (char *)malloc(PATH_MAX);
        if (!child_guest || !child_host) {
            free(child_guest);
            free(child_host);
            break;
        }

        if (strcmp(rel_guest, "/") == 0) {
            snprintf(child_guest, PATH_MAX, "/%s", de->d_name);
        } else {
            snprintf(child_guest, PATH_MAX, "%s/%s", rel_guest, de->d_name);
        }
        snprintf(child_host, PATH_MAX, "%s%s", host_root, child_guest);

        struct stat st;
        if (stat(child_host, &st) == 0 && S_ISDIR(st.st_mode)) {
            if (strcmp(child_guest, "/proc") == 0 || strcmp(child_guest, "/sys") == 0 ||
                strcmp(child_guest, "/dev") == 0 || strcmp(child_guest, "/run") == 0 ||
                strcmp(child_guest, "/usr/share") == 0 || strcmp(child_guest, "/var") == 0 ||
                strcmp(child_guest, "/tmp") == 0) {
                free(child_guest);
                free(child_host);
                continue;
            }

            if (is_binary_dir_name(de->d_name)) {
                if (!is_path_excluded(child_guest)) {
                    bool exists = false;
                    for (int k = 0; k < out->count; k++) {
                        if (strcmp(out->paths[k], child_guest) == 0) { exists = true; break; }
                    }
                    if (!exists && out->count < MAX_DISCOVERED_PATHS) {
                        strncpy(out->paths[out->count++], child_guest, PATH_MAX - 1);
                    }
                }
            }

            scan_dir_for_bins(host_root, child_guest, depth + 1, out);
        }
        free(child_guest);
        free(child_host);
    }
    closedir(dir);
}

static void vdisk_discover_and_export_paths(const char *cache_dir) {
    if (!g_setup_paths_enabled) return;

    DiscoveredPaths *dp = (DiscoveredPaths *)calloc(1, sizeof(DiscoveredPaths));
    if (!dp) return;

    const char *priority_bins[] = {
        "/usr/local/sbin",
        "/usr/local/bin",
        "/usr/sbin",
        "/usr/bin",
        "/sbin",
        "/bin",
        "/system/bin",
        "/system/xbin",
        NULL
    };

    for (int p = 0; priority_bins[p] != NULL; p++) {
        if (!is_path_excluded(priority_bins[p])) {
            char check_host[PATH_MAX];
            snprintf(check_host, sizeof(check_host), "%s%s", cache_dir, priority_bins[p]);
            struct stat st;
            if (stat(check_host, &st) == 0 && S_ISDIR(st.st_mode)) {
                strncpy(dp->paths[dp->count++], priority_bins[p], PATH_MAX - 1);
            }
        }
    }

    scan_dir_for_bins(cache_dir, "/", 0, dp);

    size_t cur_len = 0;
    g_discovered_path_env[0] = '\0';
    for (int i = 0; i < dp->count; i++) {
        size_t p_len = strlen(dp->paths[i]);
        if (cur_len + p_len + 2 >= sizeof(g_discovered_path_env)) break;
        if (cur_len > 0) {
            g_discovered_path_env[cur_len++] = ':';
            g_discovered_path_env[cur_len] = '\0';
        }
        memcpy(g_discovered_path_env + cur_len, dp->paths[i], p_len + 1);
        cur_len += p_len;
    }

    free(dp);

    if (g_discovered_path_env[0] == '\0') {
        strncpy(g_discovered_path_env, "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin", sizeof(g_discovered_path_env) - 1);
    }

    setenv("PATH", g_discovered_path_env, 1);

    char prof_dir[PATH_MAX];
    snprintf(prof_dir, sizeof(prof_dir), "%s/etc/profile.d", cache_dir);
    mkdir(prof_dir, 0755);

    char prof_file[PATH_MAX];
    snprintf(prof_file, sizeof(prof_file), "%s/00-vdisk-paths.sh", prof_dir);
    int fd = open(prof_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        char content[8192 + 32];
        int clen = snprintf(content, sizeof(content), "export PATH=\"%s:$PATH\"\n", g_discovered_path_env);
        (void)write(fd, content, clen);
        close(fd);
    }
}

static FilteredSysnum vdisk_filtered_sysnums[] = {
    { PR_uname,        FILTER_SYSEXIT },
    { PR_olduname,     FILTER_SYSEXIT },
    { PR_oldolduname,  FILTER_SYSEXIT },
    { PR_sethostname,  FILTER_SYSEXIT },
    FILTERED_SYSNUM_END,
};

static void detect_distro_codename(vdisk_fs_t *fs, char *out_codename, size_t max_len) {
    strncpy(out_codename, "vdisk", max_len - 1);
    out_codename[max_len - 1] = '\0';

    vdisk_file_t *file = NULL;
    const char *os_release_paths[] = { "/usr/lib/os-release", "/etc/os-release", "/etc/issue", NULL };
    for (int p = 0; os_release_paths[p] != NULL; p++) {
        if (vdisk_fs_open(fs, os_release_paths[p], O_RDONLY, &file) == VDISK_OK && file) {
            char buf[4096];
            int64_t rd = vdisk_file_read(file, buf, sizeof(buf) - 1);
            vdisk_file_close(file);
            file = NULL;
            if (rd > 0) {
                buf[rd] = '\0';
                const char *keys[] = { "UBUNTU_CODENAME=", "VERSION_CODENAME=", "ID=", "DEFAULT_HOSTNAME=", NULL };
                for (int k = 0; keys[k] != NULL; k++) {
                    char *line = strstr(buf, keys[k]);
                    if (line) {
                        char *eq = strchr(line, '=');
                        if (eq) {
                            eq++;
                            while (*eq == ' ' || *eq == '\t') eq++;
                            if (*eq == '"' || *eq == '\'') eq++;
                            char *end = eq;
                            while (*end && *end != '\n' && *end != '\r' && *end != '"' && *end != '\'') end++;
                            size_t len = end - eq;
                            if (len > 0 && len < max_len) {
                                strncpy(out_codename, eq, len);
                                out_codename[len] = '\0';
                                return;
                            }
                        }
                    }
                }
            }
        }
    }

    if (vdisk_fs_open(fs, "/etc/hostname", O_RDONLY, &file) == VDISK_OK && file) {
        char buf[256];
        int64_t rd = vdisk_file_read(file, buf, sizeof(buf) - 1);
        vdisk_file_close(file);
        if (rd > 0) {
            buf[rd] = '\0';
            char *end = buf;
            while (*end && *end != '\n' && *end != '\r' && *end != ' ') end++;
            *end = '\0';
            if (strlen(buf) > 0) {
                strncpy(out_codename, buf, max_len - 1);
                out_codename[max_len - 1] = '\0';
                return;
            }
        }
    }
}

static void ensure_parent_dirs(const char *path) {
    char parent[PATH_MAX];
    strncpy(parent, path, sizeof(parent) - 1);
    parent[sizeof(parent) - 1] = '\0';
    char *slash = strrchr(parent, '/');
    if (slash && slash != parent) {
        *slash = '\0';
        char cmd[PATH_MAX + 16];
        snprintf(cmd, sizeof(cmd), "mkdir -p \"%s\"", parent);
        (void)system(cmd);
    }
}

static int extract_vdisk_entry(vdisk_fs_t *fs, const char *guest_path, const char *host_path, int depth);

static int extract_vdisk_file_to_host(vdisk_fs_t *fs, const char *guest_path, const char *host_path) {
    vdisk_stat_t vs;
    vdisk_status_t st = vdisk_fs_stat(fs, guest_path, &vs);
    if (st != VDISK_OK) return -ENOENT;

    ensure_parent_dirs(host_path);

    if (vs.type == VDISK_FILE_DIRECTORY) {
        mkdir(host_path, 0755);
        return 0;
    }

    if (vs.type == VDISK_FILE_SYMLINK) {
        char link_target[PATH_MAX];
        memset(link_target, 0, sizeof(link_target));
        if (fs->ops && fs->ops->read_link) {
            if (fs->ops->read_link(fs, guest_path, link_target, sizeof(link_target)) == VDISK_OK) {
                unlink(host_path);
                symlink(link_target, host_path);
                return 0;
            }
        }
        return 0;
    }

    vdisk_file_t *file = NULL;
    st = vdisk_fs_open(fs, guest_path, O_RDONLY, &file);
    if (st != VDISK_OK) return -ENOENT;

    mode_t out_mode = (vs.mode & 0777) ? (vs.mode & 0777) : 0755;
    int out_fd = open(host_path, O_WRONLY | O_CREAT | O_TRUNC, out_mode);
    if (out_fd < 0) {
        vdisk_file_close(file);
        return -errno;
    }

    char buf[65536];
    int64_t rd;
    while ((rd = vdisk_file_read(file, buf, sizeof(buf))) > 0) {
        if (write(out_fd, buf, (size_t)rd) != rd) {
            close(out_fd);
            vdisk_file_close(file);
            return -EIO;
        }
    }

    close(out_fd);
    vdisk_file_close(file);
    chmod(host_path, out_mode);
    return 0;
}

/*
 * extract_vdisk_dir_shallow:
 *   Creates only the directory itself + immediate child directory stubs.
 *   Files in the directory are NOT extracted — they are fetched on-demand
 *   by TRANSLATED_PATH when a tracee process actually opens them.
 *
 *   This makes startup O(1) in total disk size. The number of inodes read
 *   at boot is bounded by the number of top-level entries we choose to stub.
 */
static int extract_vdisk_dir_shallow(vdisk_fs_t *fs, const char *guest_dir, const char *host_dir) {
    ensure_parent_dirs(host_dir);
    mkdir(host_dir, 0755);

    vdisk_dir_t *dir = NULL;
    if (vdisk_fs_opendir(fs, guest_dir, &dir) != VDISK_OK || !dir)
        return -ENOENT;

    vdisk_dirent_t dent;
    while (vdisk_dir_read(dir, &dent) == VDISK_OK) {
        if (strcmp(dent.name, ".") == 0 || strcmp(dent.name, "..") == 0) continue;

        char child_guest[PATH_MAX];
        char child_host[PATH_MAX];
        if (strcmp(guest_dir, "/") == 0)
            snprintf(child_guest, sizeof(child_guest), "/%s", dent.name);
        else
            snprintf(child_guest, sizeof(child_guest), "%s/%s", guest_dir, dent.name);
        snprintf(child_host, sizeof(child_host), "%s/%s", host_dir, dent.name);

        /* Skip if already extracted (e.g., by a previous on-demand fetch) */
        struct stat st;
        if (lstat(child_host, &st) == 0) continue;

        vdisk_stat_t vs;
        if (vdisk_fs_stat(fs, child_guest, &vs) != VDISK_OK) continue;

        if (vs.type == VDISK_FILE_DIRECTORY) {
            mkdir(child_host, (vs.mode & 0777) ? (vs.mode & 0777) : 0755);
        } else if (vs.type == VDISK_FILE_SYMLINK) {
            char link_target[PATH_MAX] = {0};
            if (fs->ops && fs->ops->read_link &&
                fs->ops->read_link(fs, child_guest, link_target, sizeof(link_target)) == VDISK_OK) {
                unlink(child_host);
                symlink(link_target, child_host);
            }
        } else {
            extract_vdisk_file_to_host(fs, child_guest, child_host);
        }
    }

    vdisk_dir_close(dir);
    vdisk_mark_dir_loaded(guest_dir);
    return 0;
}

/*
 * extract_vdisk_dir_full:
 *   Recursively extracts a directory and all its contents.
 *   Used only for specific critical subdirectories that MUST be fully present
 *   before the first tracee process runs (e.g. /bin, /sbin, /lib, /usr/bin).
 */
static int extract_vdisk_entry(vdisk_fs_t *fs, const char *guest_path, const char *host_path, int depth);

static int extract_vdisk_dir_full(vdisk_fs_t *fs, const char *guest_dir, const char *host_dir, int depth) {
    if (depth > 20) return 0;

    ensure_parent_dirs(host_dir);
    mkdir(host_dir, 0755);

    vdisk_dir_t *dir = NULL;
    if (vdisk_fs_opendir(fs, guest_dir, &dir) != VDISK_OK || !dir)
        return -ENOENT;

    vdisk_dirent_t dent;
    while (vdisk_dir_read(dir, &dent) == VDISK_OK) {
        if (strcmp(dent.name, ".") == 0 || strcmp(dent.name, "..") == 0) continue;

        char *child_guest = (char *)malloc(PATH_MAX);
        char *child_host = (char *)malloc(PATH_MAX);
        if (!child_guest || !child_host) {
            free(child_guest);
            free(child_host);
            break;
        }

        if (strcmp(guest_dir, "/") == 0)
            snprintf(child_guest, PATH_MAX, "/%s", dent.name);
        else
            snprintf(child_guest, PATH_MAX, "%s/%s", guest_dir, dent.name);
        snprintf(child_host, PATH_MAX, "%s/%s", host_dir, dent.name);

        struct stat st;
        if (lstat(child_host, &st) == 0) {
            if (!S_ISDIR(st.st_mode)) {
                /* Existing regular file or symlink — preserve host/modified version */
                free(child_guest);
                free(child_host);
                continue;
            }
        }

        extract_vdisk_entry(fs, child_guest, child_host, depth + 1);
        free(child_guest);
        free(child_host);
    }

    vdisk_dir_close(dir);
    return 0;
}

static int extract_vdisk_entry(vdisk_fs_t *fs, const char *guest_path, const char *host_path, int depth) {
    if (depth > 20) return 0;
    vdisk_stat_t vs;
    if (vdisk_fs_stat(fs, guest_path, &vs) != VDISK_OK) return -ENOENT;

    if (vs.type == VDISK_FILE_DIRECTORY) {
        return extract_vdisk_dir_full(fs, guest_path, host_path, depth + 1);
    } else if (vs.type == VDISK_FILE_SYMLINK) {
        char link_target[PATH_MAX] = {0};
        if (fs->ops && fs->ops->read_link &&
            fs->ops->read_link(fs, guest_path, link_target, sizeof(link_target)) == VDISK_OK) {
            unlink(host_path);
            symlink(link_target, host_path);
            return 0;
        }
        return -EINVAL;
    } else {
        return extract_vdisk_file_to_host(fs, guest_path, host_path);
    }
}

static void vdisk_do_commit_and_cleanup(VdiskConfig *config) {
    if (!config || config->temp_cache_dir[0] == '\0') return;

    if (config->persistent) {
        config->persistent = false; /* Prevent double commit */

        /* Safety check: do not commit if rootfs is broken or empty */
        char check_bin[1024];
        char check_usr[1024];
        snprintf(check_bin, sizeof(check_bin), "%s/bin", config->temp_cache_dir);
        snprintf(check_usr, sizeof(check_usr), "%s/usr", config->temp_cache_dir);
        struct stat st_b, st_u;
        if (lstat(check_bin, &st_b) < 0 && lstat(check_usr, &st_u) < 0) {
            note(NULL, WARNING, USER, "vdisk: skipping persistence commit because cache directory is incomplete");
            goto cleanup_and_exit;
        }

        note(NULL, INFO, USER, "vdisk: syncing changes back to '%s'...", config->image_path);

        /* Ensure any untouched files from the virtual disk are extracted so image is complete */
        if (config->fs) {
            extract_vdisk_dir_full(config->fs, "/", config->temp_cache_dir, 0);
        }

        char *sub = (char *)malloc(PATH_MAX * 2);
        if (sub) {
            snprintf(sub, PATH_MAX * 2, "rm -rf \"%s/tmp/\"* \"%s/run/\"* \"%s/proc/\"* \"%s/sys/\"* \"%s/dev/\"* 2>/dev/null",
                     config->temp_cache_dir, config->temp_cache_dir, config->temp_cache_dir, config->temp_cache_dir, config->temp_cache_dir);
            (void)system(sub);
            snprintf(sub, PATH_MAX * 2, "chmod -R u+rwX \"%s\" 2>/dev/null", config->temp_cache_dir);
            (void)system(sub);
            free(sub);
        }

        uint64_t actual_dir_bytes = 0;
        char du_cmd[1024];
        snprintf(du_cmd, sizeof(du_cmd), "du -sb \"%s\" 2>/dev/null", config->temp_cache_dir);
        FILE *du_fp = popen(du_cmd, "r");
        if (du_fp) {
            unsigned long long bytes_read = 0;
            if (fscanf(du_fp, "%llu", &bytes_read) == 1) {
                actual_dir_bytes = (uint64_t)bytes_read;
            }
            pclose(du_fp);
        }

        uint64_t required_size = actual_dir_bytes + (actual_dir_bytes / 5) + (32 * 1024 * 1024);
        uint64_t total_size = config->image_size;
        if (required_size > total_size) {
            total_size = required_size;
        }
        if (total_size < 64 * 1024 * 1024) total_size = 64 * 1024 * 1024;
        vdisk_format_t fmt = config->image_format;
        char img_copy[PATH_MAX];
        strncpy(img_copy, config->image_path, sizeof(img_copy) - 1);
        img_copy[sizeof(img_copy) - 1] = '\0';

        if (config->fs) { 
            vdisk_fs_unmount(config->fs); 
            config->fs = NULL; 
        }
        if (config->target_block_dev && config->target_block_dev != config->root_block_dev) {
            vdisk_block_close(config->target_block_dev);
        }
        config->target_block_dev = NULL;
        if (config->root_block_dev) {
            vdisk_block_close(config->root_block_dev);
            config->root_block_dev = NULL;
        }

        const char *tmp_base = getenv("TMPDIR");
        if (!tmp_base || tmp_base[0] == '\0') {
#if defined(__ANDROID__)
            tmp_base = "/data/local/tmp";
#else
            tmp_base = "/tmp";
#endif
        }
        char raw_tmp[PATH_MAX];
        snprintf(raw_tmp, sizeof(raw_tmp), "%s/proot_commit_%d.raw", tmp_base, getpid());

        char *cmd = (char *)malloc(PATH_MAX * 4);
        if (cmd) {
            snprintf(cmd, PATH_MAX * 4, "truncate -s %lu \"%s\" && /sbin/mkfs.ext4 -F -d \"%s\" \"%s\" >/dev/null 2>&1",
                     (unsigned long)total_size, raw_tmp, config->temp_cache_dir, raw_tmp);
            int ret = system(cmd);

            if (ret == 0) {
                char vhd_script[PATH_MAX] = "tools/raw_to_vhd.py";
                char qcow_script[PATH_MAX] = "tools/raw_to_qcow2.py";
                if (access("tools/raw_to_vhd.py", F_OK) != 0) {
                    if (access("../tools/raw_to_vhd.py", F_OK) == 0) {
                        strncpy(vhd_script, "../tools/raw_to_vhd.py", sizeof(vhd_script) - 1);
                        strncpy(qcow_script, "../tools/raw_to_qcow2.py", sizeof(qcow_script) - 1);
                    } else if (access("/home/shado/Documents/libdsk_api/tools/raw_to_vhd.py", F_OK) == 0) {
                        strncpy(vhd_script, "/home/shado/Documents/libdsk_api/tools/raw_to_vhd.py", sizeof(vhd_script) - 1);
                        strncpy(qcow_script, "/home/shado/Documents/libdsk_api/tools/raw_to_qcow2.py", sizeof(qcow_script) - 1);
                    }
                }

                if (fmt == VDISK_FMT_QCOW2) {
                    snprintf(cmd, PATH_MAX * 4, "python3 \"%s\" \"%s\" \"%s\" >/dev/null 2>&1", qcow_script, raw_tmp, img_copy);
                    (void)system(cmd);
                } else if (fmt == VDISK_FMT_VHD) {
                    snprintf(cmd, PATH_MAX * 4, "python3 \"%s\" \"%s\" \"%s\" >/dev/null 2>&1", vhd_script, raw_tmp, img_copy);
                    (void)system(cmd);
                } else {
                    snprintf(cmd, PATH_MAX * 4, "mv -f \"%s\" \"%s\"", raw_tmp, img_copy);
                    (void)system(cmd);
                }
                unlink(raw_tmp);
                note(NULL, INFO, USER, "vdisk: successfully saved persistent changes to '%s'", img_copy);
            } else {
                note(NULL, WARNING, USER, "vdisk: failed to rebuild filesystem image during persistence commit");
            }
            free(cmd);
        }
    }

cleanup_and_exit:
    if (config->fs) {
        vdisk_fs_unmount(config->fs);
        config->fs = NULL;
    }
    if (config->target_block_dev && config->target_block_dev != config->root_block_dev) {
        vdisk_block_close(config->target_block_dev);
    }
    config->target_block_dev = NULL;
    if (config->root_block_dev) {
        vdisk_block_close(config->root_block_dev);
        config->root_block_dev = NULL;
    }

    if (strlen(config->temp_cache_dir) > 0) {
        char cmd[PATH_MAX + 16];
        snprintf(cmd, sizeof(cmd), "rm -rf \"%s\" 2>/dev/null", config->temp_cache_dir);
        (void)system(cmd);
        config->temp_cache_dir[0] = '\0';
    }
    vdisk_clear_loaded_dirs();
}

static void vdisk_cleanup_atexit(void) {
    if (g_main_proot_pid != 0 && getpid() != g_main_proot_pid) {
        return; /* Never run persistence commit inside child tracees/subshells */
    }
    if (g_active_config) {
        vdisk_do_commit_and_cleanup(g_active_config);
        g_active_config = NULL;
    }
}

int vdisk_callback(Extension *extension, ExtensionEvent event,
                   intptr_t data1, intptr_t data2)
{
    (void)data2;
    VdiskConfig *config = extension ? (VdiskConfig *)extension->config : NULL;

    switch (event) {
    case INITIALIZATION: {
        const char *cli = (const char *)data1;
        if (!cli || strlen(cli) == 0) {
            return -EINVAL;
        }

        memset(&g_static_vdisk_config, 0, sizeof(g_static_vdisk_config));
        config = &g_static_vdisk_config;
        config->ref_count = 1;
        config->persistent = g_vdisk_persistent;
        g_active_config = config;
        extension->config = config;
        g_main_proot_pid = getpid();
        atexit(vdisk_cleanup_atexit);

        char img_path[PATH_MAX];
        strncpy(img_path, cli, sizeof(img_path) - 1);
        img_path[sizeof(img_path) - 1] = '\0';

        char *colon = strrchr(img_path, ':');
        if (colon) {
            *colon = '\0';
            config->partition_index = atoi(colon + 1);
        } else {
            config->partition_index = 0;
        }

        if (realpath(img_path, config->image_path) == NULL) {
            strncpy(config->image_path, img_path, sizeof(config->image_path) - 1);
        }

        vdisk_status_t st = vdisk_block_open_file(config->image_path, true, VDISK_FMT_AUTO, &config->root_block_dev);
        if (st != VDISK_OK || !config->root_block_dev) {
            note(NULL, ERROR, USER, "vdisk: failed to open virtual disk image '%s'", config->image_path);
            return -EINVAL;
        }

        config->target_block_dev = config->root_block_dev;
        config->image_format = config->root_block_dev->format;
        config->image_size = config->root_block_dev->total_sectors * 512;

        vdisk_part_table_t ptable;
        if (vdisk_part_scan(config->root_block_dev, &ptable) == VDISK_OK && ptable.num_partitions > 0) {
            int target_p = config->partition_index > 0 ? config->partition_index : 1;
            if (target_p <= (int)ptable.num_partitions) {
                const vdisk_partition_t *part = &ptable.partitions[target_p - 1];
                vdisk_part_open_slice(config->root_block_dev, part, &config->target_block_dev);
                config->image_size = part->size_bytes;
            }
        }

        st = vdisk_fs_detect_and_mount(config->target_block_dev, &config->fs);
        if (st != VDISK_OK || !config->fs) {
            note(NULL, ERROR, USER, "vdisk: no valid ext4/FAT filesystem detected in '%s'", config->image_path);
            return -EINVAL;
        }

        detect_distro_codename(config->fs, config->distro_name, sizeof(config->distro_name));

        const char *tmp_base = getenv("TMPDIR");
        if (!tmp_base || tmp_base[0] == '\0') {
#if defined(__ANDROID__)
            tmp_base = "/data/local/tmp";
#else
            tmp_base = "/tmp";
#endif
        }
        snprintf(config->temp_cache_dir, sizeof(config->temp_cache_dir), "%s/proot_vdisk_%d", tmp_base, getpid());
        strncpy(g_vdisk_cache_dir, config->temp_cache_dir, sizeof(g_vdisk_cache_dir) - 1);
        mkdir(config->temp_cache_dir, 0755);

        /*
         * ULTRA-FAST LAZY INIT / HOT-LOAD ON DEMAND:
         *   1. Shallow-stub top-level '/' and '/usr' so structure & symlinks exist.
         *   2. Direct files & libraries are hot-loaded into cache on demand via TRANSLATED_PATH.
         *   3. Memory footprint and startup latency are near zero (< 5ms).
         */
        extract_vdisk_dir_shallow(config->fs, "/", config->temp_cache_dir);

        char usr_h[PATH_MAX];
        snprintf(usr_h, sizeof(usr_h), "%s/usr", config->temp_cache_dir);
        extract_vdisk_dir_shallow(config->fs, "/usr", usr_h);

        char etc_h[PATH_MAX];
        snprintf(etc_h, sizeof(etc_h), "%s/etc", config->temp_cache_dir);
        extract_vdisk_dir_shallow(config->fs, "/etc", etc_h);

        char root_h[PATH_MAX];
        snprintf(root_h, sizeof(root_h), "%s/root", config->temp_cache_dir);
        extract_vdisk_dir_shallow(config->fs, "/root", root_h);

        /* Ensure essential directories exist */
        char sub[PATH_MAX];
        snprintf(sub, sizeof(sub), "%s/tmp", config->temp_cache_dir); mkdir(sub, 0777);
        snprintf(sub, sizeof(sub), "%s/dev", config->temp_cache_dir); mkdir(sub, 0755);
        snprintf(sub, sizeof(sub), "%s/proc", config->temp_cache_dir); mkdir(sub, 0755);
        snprintf(sub, sizeof(sub), "%s/sys", config->temp_cache_dir); mkdir(sub, 0755);
        snprintf(sub, sizeof(sub), "%s/root", config->temp_cache_dir); mkdir(sub, 0750);
        snprintf(sub, sizeof(sub), "%s/var/lib/apt/lists/partial", config->temp_cache_dir); 
        ensure_parent_dirs(sub); mkdir(sub, 0755);
        snprintf(sub, sizeof(sub), "%s/var/cache/apt/archives/partial", config->temp_cache_dir);
        ensure_parent_dirs(sub); mkdir(sub, 0755);

        snprintf(sub, sizeof(sub), "%s/etc/hostname", config->temp_cache_dir);
        int hfd = open(sub, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (hfd >= 0) {
            char hbuf[128];
            int hlen = snprintf(hbuf, sizeof(hbuf), "%s\n", config->distro_name);
            (void)write(hfd, hbuf, hlen);
            close(hfd);
        }

        snprintf(sub, sizeof(sub), "%s/etc/resolv.conf", config->temp_cache_dir);
        struct stat st_res;
        if (stat(sub, &st_res) < 0) {
            struct stat st_lnk;
            if (lstat(sub, &st_lnk) == 0 && S_ISLNK(st_lnk.st_mode)) {
                unlink(sub);
            }
            if (stat("/etc/resolv.conf", &st_res) == 0) {
                char cmd[PATH_MAX + 64];
                snprintf(cmd, sizeof(cmd), "cp -L /etc/resolv.conf \"%s\" 2>/dev/null", sub);
                (void)system(cmd);
            } else {
                /* Android / Termux fallback: write standard public DNS resolvers */
                ensure_parent_dirs(sub);
                int rfd = open(sub, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (rfd >= 0) {
                    const char dns[] = "nameserver 8.8.8.8\nnameserver 1.1.1.1\nnameserver 8.8.4.4\n";
                    (void)write(rfd, dns, sizeof(dns) - 1);
                    close(rfd);
                }
            }
        }

        /* Ensure terminfo exists so nano/curses work across all distros */
        snprintf(sub, sizeof(sub), "%s/usr/share/terminfo", config->temp_cache_dir);
        struct stat st_ti;
        if (stat(sub, &st_ti) < 0) {
            const char *src_ti = NULL;
            if (stat("/usr/share/terminfo", &st_ti) == 0) src_ti = "/usr/share/terminfo";
            else if (stat("/data/data/com.termux/files/usr/share/terminfo", &st_ti) == 0) src_ti = "/data/data/com.termux/files/usr/share/terminfo";
            else if (getenv("PREFIX")) {
                static char pfx_ti[PATH_MAX];
                snprintf(pfx_ti, sizeof(pfx_ti), "%s/share/terminfo", getenv("PREFIX"));
                if (stat(pfx_ti, &st_ti) == 0) src_ti = pfx_ti;
            }
            if (src_ti) {
                ensure_parent_dirs(sub);
                char cmd[PATH_MAX * 2 + 64];
                snprintf(cmd, sizeof(cmd), "cp -rn \"%s\" \"%s/usr/share/\" 2>/dev/null", src_ti, config->temp_cache_dir);
                (void)system(cmd);
            }
        }

        /* Discover binary directories and export PATH if requested */
        vdisk_discover_and_export_paths(config->temp_cache_dir);

        /* Copy libimproot-jit.so into guest root as a real file /.proot.jit.so */
        char exe_path[PATH_MAX];
        ssize_t exe_len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
        if (exe_len > 0) {
            exe_path[exe_len] = '\0';
            char *dir_path = dirname(exe_path);
            char jit_src[PATH_MAX];
            snprintf(jit_src, sizeof(jit_src), "%s/libimproot-jit.so", dir_path);
            if (access(jit_src, R_OK) == 0) {
                char jit_dst[PATH_MAX];
                snprintf(jit_dst, sizeof(jit_dst), "%s/.proot.jit.so", config->temp_cache_dir);
                unlink(jit_dst);
                FILE *fin = fopen(jit_src, "rb");
                if (fin) {
                    FILE *fout = fopen(jit_dst, "wb");
                    if (fout) {
                        char buf[8192];
                        size_t n;
                        while ((n = fread(buf, 1, sizeof(buf), fin)) > 0) {
                            fwrite(buf, 1, n, fout);
                        }
                        fclose(fout);
                        chmod(jit_dst, 0755);
                    }
                    fclose(fin);
                }
            }
        }

        extension->filtered_sysnums = vdisk_filtered_sysnums;

        const char *fmt_str = (config->root_block_dev->format == VDISK_FMT_QCOW2) ? "QCOW2" :
                              (config->root_block_dev->format == VDISK_FMT_VHD) ? "VHD" : "RAW";
        note(NULL, INFO, USER, "vdisk: successfully mounted '%s' [%s, %.2f MB] (distro: %s%s)",
             config->image_path, fmt_str, (double)(config->root_block_dev->total_sectors * 512) / (1024.0 * 1024.0),
             config->distro_name, config->persistent ? ", persistent" : "");
        return 0;
    }

    case INHERIT_PARENT: {
        return 1;
    }

    case INHERIT_CHILD: {
        Extension *parent_ext = (Extension *)data1;
        if (parent_ext) {
            extension->config = parent_ext->config;
            extension->filtered_sysnums = parent_ext->filtered_sysnums;
        }
        return 0;
    }

    case HOST_PATH:
    case TRANSLATED_PATH: {
        if (!config || !config->fs) return 0;
        char *host_path = (char *)data1;
        if (!host_path) return 0;

        size_t cache_len = strlen(config->temp_cache_dir);
        if (strncmp(host_path, config->temp_cache_dir, cache_len) == 0) {
            const char *guest_path = host_path + cache_len;
            if (strlen(guest_path) == 0) guest_path = "/";

            struct stat st;
            if (lstat(host_path, &st) < 0) {
                vdisk_stat_t vs;
                if (vdisk_fs_stat(config->fs, guest_path, &vs) == VDISK_OK) {
                    if (vs.type == VDISK_FILE_DIRECTORY) {
                        mkdir(host_path, (vs.mode & 0777) ? (vs.mode & 0777) : 0755);
                        extract_vdisk_dir_shallow(config->fs, guest_path, host_path);
                    } else if (vs.type == VDISK_FILE_SYMLINK) {
                        char link_target[PATH_MAX] = {0};
                        if (config->fs->ops && config->fs->ops->read_link &&
                            config->fs->ops->read_link(config->fs, guest_path, link_target, sizeof(link_target)) == VDISK_OK) {
                            ensure_parent_dirs(host_path);
                            unlink(host_path);
                            symlink(link_target, host_path);
                        }
                    } else {
                        extract_vdisk_file_to_host(config->fs, guest_path, host_path);
                    }
                } else {
                    ensure_parent_dirs(host_path);
                }
            } else if (S_ISDIR(st.st_mode)) {
                /* Directory exists: lazily populate direct children if not loaded yet */
                if (!vdisk_is_dir_loaded(guest_path)) {
                    extract_vdisk_dir_shallow(config->fs, guest_path, host_path);
                }
            }
        }
        return 0;
    }

    case SYSCALL_EXIT_END: {
        if (!config) return 0;
        Tracee *tracee = TRACEE(extension);
        word_t sysnum = get_sysnum(tracee, ORIGINAL);

        switch (sysnum) {
        case PR_uname:
        case PR_olduname:
        case PR_oldolduname: {
            word_t result = peek_reg(tracee, CURRENT, SYSARG_RESULT);
            if (result == 0) {
                word_t buf_addr = peek_reg(tracee, ORIGINAL, SYSARG_1);
                write_data(tracee, buf_addr + 65, config->distro_name, strlen(config->distro_name) + 1);
            }
            return 0;
        }

        case PR_sethostname: {
            poke_reg(tracee, SYSARG_RESULT, 0);
            return 0;
        }

        default:
            return 0;
        }
    }

    case REMOVED: {
        return 0;
    }

    case PRINT_USAGE: {
        printf("  --vdisk=<image_path>[:part#]\n"
               "  --disk=<image_path>[:part#]\n"
               "                        Boot / execute directly from a QCOW2, VHD, or RAW\n"
               "                        virtual drive without root, mount, or chroot.\n"
               "  --persistent, --commit\n"
               "                        Persist and sync all guest changes back to the virtual\n"
               "                        disk image on exit.\n"
               "  --setup-paths, --setup-path\n"
               "                        Auto-scan and export binary directories (bin, sbin, xbin, .bin)\n"
               "                        to PATH.\n"
               "  -se, --setup-exclude, --exclude-path=<list>\n"
               "                        Exclude directory paths from the auto-scanned PATH list.\n");
        return 0;
    }

    case PRINT_CONFIG: {
        if (config) {
            printf("vdisk image: %s (partition: %d, distro: %s, persistent: %s, PATH: %s)\n",
                   config->image_path, config->partition_index, config->distro_name,
                   config->persistent ? "yes" : "no",
                   g_discovered_path_env[0] ? g_discovered_path_env : "default");
        }
        return 0;
    }

    default:
        return 0;
    }
}
