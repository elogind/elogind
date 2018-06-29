/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

/***
  This file is part of systemd.

  Copyright 2010 Lennart Poettering

  systemd is free software; you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation; either version 2.1 of the License, or
  (at your option) any later version.

  systemd is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with systemd; If not, see <http://www.gnu.org/licenses/>.
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
