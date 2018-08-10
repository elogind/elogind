/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

/***
  This file is part of systemd.

  Copyright 2010-2012 Lennart Poettering
***/

#include <stdbool.h>
#include <stddef.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/types.h>
#include <sys/vfs.h>

#include "macro.h"

#if 0 /// UNNEEDED by elogind
int is_symlink(const char *path);
#endif // 0
int is_dir(const char *path, bool follow);
#if 0 /// UNNEEDED by elogind
int is_device_node(const char *path);

int dir_is_empty(const char *path);

static inline int dir_is_populated(const char *path) {
        int r;
        r = dir_is_empty(path);
        if (r < 0)
                return r;
        return !r;
}
#endif // 0

bool null_or_empty(struct stat *st) _pure_;
int null_or_empty_path(const char *fn);
#if 0 /// UNNEEDED by elogind
int null_or_empty_fd(int fd);
#endif // 0

int path_is_read_only_fs(const char *path);
#if 0 /// UNNEEDED by elogind
#endif // 0

int files_same(const char *filea, const char *fileb, int flags);

/* The .f_type field of struct statfs is really weird defined on
 * different archs. Let's give its type a name. */
typedef typeof(((struct statfs*)NULL)->f_type) statfs_f_type_t;

bool is_fs_type(const struct statfs *s, statfs_f_type_t magic_value) _pure_;
#if 0 /// UNNEEDED by elogind
#endif // 0
int fd_is_fs_type(int fd, statfs_f_type_t magic_value);
int path_is_fs_type(const char *path, statfs_f_type_t magic_value);

bool is_temporary_fs(const struct statfs *s) _pure_;
bool is_network_fs(const struct statfs *s) _pure_;

#if 0 /// UNNEEDED by elogind
int fd_is_temporary_fs(int fd);
int fd_is_network_fs(int fd);

int fd_is_network_ns(int fd);

int path_is_temporary_fs(const char *path);
#endif // 0

/* Because statfs.t_type can be int on some architectures, we have to cast
 * the const magic to the type, otherwise the compiler warns about
 * signed/unsigned comparison, because the magic can be 32 bit unsigned.
 */
#define F_TYPE_EQUAL(a, b) (a == (typeof(a)) b)

int stat_verify_regular(const struct stat *st);
int fd_verify_regular(int fd);
