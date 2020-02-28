/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

#include <stdbool.h>
#include <sys/types.h>

typedef enum LabelFixFlags {
        LABEL_IGNORE_ENOENT = 1 << 0,
        LABEL_IGNORE_EROFS  = 1 << 1,
} LabelFixFlags;

int label_fix(const char *path, LabelFixFlags flags);

int mkdir_label(const char *path, mode_t mode);
#if 0 /// UNNEEDED by elogind
int mkdirat_label(int dirfd, const char *path, mode_t mode);
#endif // 0
int symlink_label(const char *old_path, const char *new_path);

#if 0 /// UNNEEDED by elogind
int btrfs_subvol_make_label(const char *path);
#endif // 0
