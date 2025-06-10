/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/types.h>
#include <sys/vfs.h>

#include "fs-util.h"
#include "macro.h"
#include "missing_stat.h"
#include "siphash24.h"
#include "time-util.h"

int stat_verify_regular(const struct stat *st);
int verify_regular_at(int fd, const char *path, bool follow);
int fd_verify_regular(int fd);

int stat_verify_directory(const struct stat *st);
int fd_verify_directory(int fd);
int is_dir_at(int fd, const char *path, bool follow);
int is_dir(const char *path, bool follow);

int stat_verify_symlink(const struct stat *st);
#if 0 /// UNNEEDED by elogind
int is_symlink(const char *path);
#endif // 0

int stat_verify_linked(const struct stat *st);
int fd_verify_linked(int fd);

#if 0 /// UNNEEDED by elogind
int stat_verify_device_node(const struct stat *st);
int is_device_node(const char *path);

int dir_is_empty_at(int dir_fd, const char *path, bool ignore_hidden_or_backup);
static inline int dir_is_empty(const char *path, bool ignore_hidden_or_backup) {
        return dir_is_empty_at(AT_FDCWD, path, ignore_hidden_or_backup);
}

#endif // 0

bool null_or_empty(struct stat *st) _pure_;
int null_or_empty_path_with_root(const char *fn, const char *root);

static inline int null_or_empty_path(const char *fn) {
        return null_or_empty_path_with_root(fn, NULL);
}

int fd_is_read_only_fs(int fd);
int path_is_read_only_fs(const char *path);

int inode_same_at(int fda, const char *filea, int fdb, const char *fileb, int flags);
static inline int inode_same(const char *filea, const char *fileb, int flags) {
        return inode_same_at(AT_FDCWD, filea, AT_FDCWD, fileb, flags);
}
static inline int fd_inode_same(int fda, int fdb) {
        return inode_same_at(fda, NULL, fdb, NULL, AT_EMPTY_PATH);
}

/* The .f_type field of struct statfs is really weird defined on
 * different archs. Let's give its type a name. */
typedef typeof(((struct statfs*)NULL)->f_type) statfs_f_type_t;

bool is_fs_type(const struct statfs *s, statfs_f_type_t magic_value) _pure_;
int is_fs_type_at(int dir_fd, const char *path, statfs_f_type_t magic_value);
static inline int fd_is_fs_type(int fd, statfs_f_type_t magic_value) {
        return is_fs_type_at(fd, NULL, magic_value);
}
static inline int path_is_fs_type(const char *path, statfs_f_type_t magic_value) {
        return is_fs_type_at(AT_FDCWD, path, magic_value);
}

bool is_temporary_fs(const struct statfs *s) _pure_;
bool is_network_fs(const struct statfs *s) _pure_;

#if 0 /// UNNEEDED by elogind
int fd_is_temporary_fs(int fd);
int fd_is_network_fs(int fd);

int path_is_temporary_fs(const char *path);
#endif // 0
int path_is_network_fs(const char *path);

/* Because statfs.t_type can be int on some architectures, we have to cast
 * the const magic to the type, otherwise the compiler warns about
 * signed/unsigned comparison, because the magic can be 32 bit unsigned.
 */
#define F_TYPE_EQUAL(a, b) (a == (typeof(a)) b)

int proc_mounted(void);

bool stat_inode_same(const struct stat *a, const struct stat *b);
bool stat_inode_unmodified(const struct stat *a, const struct stat *b);

bool statx_inode_same(const struct statx *a, const struct statx *b);
bool statx_mount_same(const struct new_statx *a, const struct new_statx *b);

int statx_fallback(int dfd, const char *path, int flags, unsigned mask, struct statx *sx);

int xstatfsat(int dir_fd, const char *path, struct statfs *ret);

#if HAS_FEATURE_MEMORY_SANITIZER
#  warning "Explicitly initializing struct statx, to work around msan limitation. Please remove as soon as msan has been updated to not require this."
#  define STRUCT_STATX_DEFINE(var)              \
        struct statx var = {}
#  define STRUCT_NEW_STATX_DEFINE(var)          \
        union {                                 \
                struct statx sx;                \
                struct new_statx nsx;           \
        } var = {}
#else
#  define STRUCT_STATX_DEFINE(var)              \
        struct statx var
#  define STRUCT_NEW_STATX_DEFINE(var)          \
        union {                                 \
                struct statx sx;                \
                struct new_statx nsx;           \
        } var
#endif

#if 0 /// UNNEEDED by elogind
static inline usec_t statx_timestamp_load(const struct statx_timestamp *ts) {
        return timespec_load(&(const struct timespec) { .tv_sec = ts->tv_sec, .tv_nsec = ts->tv_nsec });
}
static inline nsec_t statx_timestamp_load_nsec(const struct statx_timestamp *ts) {
        return timespec_load_nsec(&(const struct timespec) { .tv_sec = ts->tv_sec, .tv_nsec = ts->tv_nsec });
}
#endif // 0

void inode_hash_func(const struct stat *q, struct siphash *state);
int inode_compare_func(const struct stat *a, const struct stat *b);
extern const struct hash_ops inode_hash_ops;

const char* inode_type_to_string(mode_t m);
mode_t inode_type_from_string(const char *s);

/* Macros that check whether the stat/statx structures have been initialized already. For "struct stat" we
 * use a check for .st_dev being non-zero, since the kernel unconditionally fills that in, mapping the file
 * to its originating superblock, regardless if the fs is block based or virtual (we also check for .st_mode
 * being MODE_INVALID, since we use that as an invalid marker for separate mode_t fields). For "struct statx"
 * we use the .stx_mask field, which must be non-zero if any of the fields have already been initialized. */
static inline bool stat_is_set(const struct stat *st) {
        return st && st->st_dev != 0 && st->st_mode != MODE_INVALID;
}
static inline bool statx_is_set(const struct statx *sx) {
        return sx && sx->stx_mask != 0;
}
static inline bool new_statx_is_set(const struct new_statx *sx) {
        return sx && sx->stx_mask != 0;
}
