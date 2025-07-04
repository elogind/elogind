/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "alloc-util.h"
#include "errno-util.h"
#include "lock-util.h"
#include "time-util.h"
#include "user-util.h"

#define MODE_INVALID ((mode_t) -1)

/* The following macros add 1 when converting things, since 0 is a valid mode, while the pointer
 * NULL is special */
#define PTR_TO_MODE(p) ((mode_t) ((uintptr_t) (p)-1))
#define MODE_TO_PTR(u) ((void *) ((uintptr_t) (u)+1))


#if 0 /// UNNEEDED by elogind
int rmdir_parents(const char *path, const char *stop);
#endif // 0

int rename_noreplace(int olddirfd, const char *oldpath, int newdirfd, const char *newpath);

int readlinkat_malloc(int fd, const char *p, char **ret);
static inline int readlink_malloc(const char *p, char **ret) {
        return readlinkat_malloc(AT_FDCWD, p, ret);
}
int readlink_value(const char *p, char **ret);
int readlink_and_make_absolute(const char *p, char **ret);

int chmod_and_chown_at(int dir_fd, const char *path, mode_t mode, uid_t uid, gid_t gid);
static inline int chmod_and_chown(const char *path, mode_t mode, uid_t uid, gid_t gid) {
        return chmod_and_chown_at(AT_FDCWD, path, mode, uid, gid);
}
int fchmod_and_chown_with_fallback(int fd, const char *path, mode_t mode, uid_t uid, gid_t gid);
static inline int fchmod_and_chown(int fd, mode_t mode, uid_t uid, gid_t gid) {
        return fchmod_and_chown_with_fallback(fd, NULL, mode, uid, gid); /* no fallback */
}

int fchmod_umask(int fd, mode_t mode);
int fchmod_opath(int fd, mode_t m);

int futimens_opath(int fd, const struct timespec ts[2]);

#if 0 /// UNNEEDED by elogind
int fd_warn_permissions(const char *path, int fd);
#endif // 0
int stat_warn_permissions(const char *path, const struct stat *st);

#ifdef __GLIBC__ /// elogind supports musl-libc, where AT_SYMLINK_FOLLOW isn't available
#define access_nofollow(path, mode)                                             \
        RET_NERRNO(faccessat(AT_FDCWD, (path), (mode), AT_SYMLINK_NOFOLLOW))
#else // __GLIBC__
#define laccess(path, mode)                                             \
        RET_NERRNO(faccessat(AT_FDCWD, (path), (mode), 0))
#endif // __GLIBC__

int touch_fd(int fd, usec_t stamp);

int touch_file(const char *path, bool parents, usec_t stamp, uid_t uid, gid_t gid, mode_t mode);

static inline int touch(const char *path) {
        return touch_file(path, false, USEC_INFINITY, UID_INVALID, GID_INVALID, MODE_INVALID);
}

int symlinkat_idempotent(const char *from, int atfd, const char *to, bool make_relative);
static inline int symlink_idempotent(const char *from, const char *to, bool make_relative) {
        return symlinkat_idempotent(from, AT_FDCWD, to, make_relative);
}

#if 0 /// UNNEEDED by elogind
int symlinkat_atomic_full(const char *from, int atfd, const char *to, bool make_relative);
static inline int symlink_atomic(const char *from, const char *to) {
        return symlinkat_atomic_full(from, AT_FDCWD, to, false);
}

int mknodat_atomic(int atfd, const char *path, mode_t mode, dev_t dev);
static inline int mknod_atomic(const char *path, mode_t mode, dev_t dev) {
        return mknodat_atomic(AT_FDCWD, path, mode, dev);
}

int mkfifoat_atomic(int dir_fd, const char *path, mode_t mode);
static inline int mkfifo_atomic(const char *path, mode_t mode) {
        return mkfifoat_atomic(AT_FDCWD, path, mode);
}
#endif // 0

int get_files_in_directory(const char *path, char ***list);

int tmp_dir(const char **ret);
int var_tmp_dir(const char **ret);

int unlink_or_warn(const char *filename);

/* Useful for usage with _cleanup_(), removes a directory and frees the pointer */
static inline char *rmdir_and_free(char *p) {
        PROTECT_ERRNO;

        if (!p)
                return NULL;

        (void) rmdir(p);
        return mfree(p);
}
DEFINE_TRIVIAL_CLEANUP_FUNC(char*, rmdir_and_free);

static inline char* unlink_and_free(char *p) {
        if (!p)
                return NULL;

        (void) unlink(p);
        return mfree(p);
}
DEFINE_TRIVIAL_CLEANUP_FUNC(char*, unlink_and_free);

int access_fd(int fd, int mode);

typedef enum UnlinkDeallocateFlags {
        UNLINK_REMOVEDIR = 1 << 0,
        UNLINK_ERASE     = 1 << 1,
} UnlinkDeallocateFlags;

#if 0 /// UNNEEDED by elogind
int unlinkat_deallocate(int fd, const char *name, UnlinkDeallocateFlags flags);
#endif // 0

int open_parent_at(int dir_fd, const char *path, int flags, mode_t mode);
static inline int open_parent(const char *path, int flags, mode_t mode) {
        return open_parent_at(AT_FDCWD, path, flags, mode);
}

#if 0 /// No need for encrypted devices in elogind
int conservative_renameat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath);
static inline int conservative_rename(const char *oldpath, const char *newpath) {
        return conservative_renameat(AT_FDCWD, oldpath, AT_FDCWD, newpath);
}

int posix_fallocate_loop(int fd, uint64_t offset, uint64_t size);

int parse_cifs_service(const char *s, char **ret_host, char **ret_service, char **ret_path);

#endif // 0

typedef enum XOpenFlags {
        XO_LABEL     = 1 << 0,
        XO_SUBVOLUME = 1 << 1,
        XO_NOCOW     = 1 << 2,
} XOpenFlags;

int open_mkdir_at_full(int dirfd, const char *path, int flags, XOpenFlags xopen_flags, mode_t mode);
static inline int open_mkdir_at(int dirfd, const char *path, int flags, mode_t mode) {
        return open_mkdir_at_full(dirfd, path, flags, 0, mode);
}
static inline int open_mkdir(const char *path, int flags, mode_t mode) {
        return open_mkdir_at_full(AT_FDCWD, path, flags, 0, mode);
}

int openat_report_new(int dirfd, const char *pathname, int flags, mode_t mode, bool *ret_newly_created);

int xopenat_full(int dir_fd, const char *path, int open_flags, XOpenFlags xopen_flags, mode_t mode);
static inline int xopenat(int dir_fd, const char *path, int open_flags) {
        return xopenat_full(dir_fd, path, open_flags, 0, 0);
}

int xopenat_lock_full(int dir_fd, const char *path, int open_flags, XOpenFlags xopen_flags, mode_t mode, LockType locktype, int operation);
static inline int xopenat_lock(int dir_fd, const char *path, int open_flags, LockType locktype, int operation) {
        return xopenat_lock_full(dir_fd, path, open_flags, 0, 0, locktype, operation);
}

int link_fd(int fd, int newdirfd, const char *newpath);

#if 0 /// UNNEEDED by elogind
int linkat_replace(int olddirfd, const char *oldpath, int newdirfd, const char *newpath);
#endif // 0
