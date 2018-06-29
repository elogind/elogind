/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

/***
  This file is part of systemd.

  Copyright 2010 Lennart Poettering
***/

#include <stdbool.h>
#include <sys/types.h>

typedef enum LabelFixFlags {
        LABEL_IGNORE_ENOENT = 1U << 0,
        LABEL_IGNORE_EROFS  = 1U << 1,
} LabelFixFlags;

int label_fix(const char *path, LabelFixFlags flags);

int mkdir_label(const char *path, mode_t mode);
#if 0 /// UNNEEDED by elogind
int symlink_label(const char *old_path, const char *new_path);

int btrfs_subvol_make_label(const char *path);
#endif // 0
