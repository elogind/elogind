/* SPDX-License-Identifier: LGPL-2.1+ */
/***
  This file is part of systemd.

  Copyright 2015 Lennart Poettering
***/

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <unistd.h>

//#include "alloc-util.h"
//#include "btrfs-util.h"
#include "cgroup-util.h"
#include "dirent-util.h"
#include "fd-util.h"
#include "log.h"
#include "macro.h"
#include "mount-util.h"
#include "path-util.h"
#include "rm-rf.h"
#include "stat-util.h"
#include "string-util.h"

static bool is_physical_fs(const struct statfs *sfs) {
        return !is_temporary_fs(sfs) && !is_cgroup_fs(sfs);
}

int rm_rf_children(int fd, RemoveFlags flags, struct stat *root_dev) {
        _cleanup_closedir_ DIR *d = NULL;
        struct dirent *de;
        int ret = 0, r;
        struct statfs sfs;

        assert(fd >= 0);

        /* This returns the first error we run into, but nevertheless
         * tries to go on. This closes the passed fd. */

        if (!(flags & REMOVE_PHYSICAL)) {

                r = fstatfs(fd, &sfs);
                if (r < 0) {
                        safe_close(fd);
                        return -errno;
                }

                if (is_physical_fs(&sfs)) {
                        /* We refuse to clean physical file systems with this call,
                         * unless explicitly requested. This is extra paranoia just
                         * to be sure we never ever remove non-state data. */
                        _cleanup_free_ char *path = NULL;

                        (void) fd_get_path(fd, &path);
                        log_error("Attempted to remove disk file system under \"%s\", and we can't allow that.",
                                  strna(path));

                        safe_close(fd);
                        return -EPERM;
                }
        }

        d = fdopendir(fd);
        if (!d) {
                safe_close(fd);
                return errno == ENOENT ? 0 : -errno;
        }

        FOREACH_DIRENT_ALL(de, d, return -errno) {
                bool is_dir;
                struct stat st;

                if (dot_or_dot_dot(de->d_name))
                        continue;

                if (de->d_type == DT_UNKNOWN ||
                    (de->d_type == DT_DIR && (root_dev || (flags & REMOVE_SUBVOLUME)))) {
                        if (fstatat(fd, de->d_name, &st, AT_SYMLINK_NOFOLLOW) < 0) {
                                if (ret == 0 && errno != ENOENT)
                                        ret = -errno;
                                continue;
                        }

                        is_dir = S_ISDIR(st.st_mode);
                } else
                        is_dir = de->d_type == DT_DIR;

                if (is_dir) {
                        int subdir_fd;

                        /* if root_dev is set, remove subdirectories only if device is same */
                        if (root_dev && st.st_dev != root_dev->st_dev)
                                continue;

                        subdir_fd = openat(fd, de->d_name, O_RDONLY|O_NONBLOCK|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW|O_NOATIME);
                        if (subdir_fd < 0) {
                                if (ret == 0 && errno != ENOENT)
                                        ret = -errno;
                                continue;
                        }

                        /* Stop at mount points */
                        r = fd_is_mount_point(fd, de->d_name, 0);
                        if (r < 0) {
                                if (ret == 0 && r != -ENOENT)
                                        ret = r;

                                safe_close(subdir_fd);
                                continue;
                        }
                        if (r) {
                                safe_close(subdir_fd);
                                continue;
                        }

#if 0 /// elogind does not support BTRFS this directly
                        if ((flags & REMOVE_SUBVOLUME) && st.st_ino == 256) {

                                /* This could be a subvolume, try to remove it */

                                r = btrfs_subvol_remove_fd(fd, de->d_name, BTRFS_REMOVE_RECURSIVE|BTRFS_REMOVE_QUOTA);
                                if (r < 0) {
                                        if (!IN_SET(r, -ENOTTY, -EINVAL)) {
                                                if (ret == 0)
                                                        ret = r;

                                                safe_close(subdir_fd);
                                                continue;
                                        }

                                        /* ENOTTY, then it wasn't a
                                         * btrfs subvolume, continue
                                         * below. */
                                } else {
                                        /* It was a subvolume, continue. */
                                        safe_close(subdir_fd);
                                        continue;
                                }
                        }
#endif // 0

                        /* We pass REMOVE_PHYSICAL here, to avoid
                         * doing the fstatfs() to check the file
                         * system type again for each directory */
                        r = rm_rf_children(subdir_fd, flags | REMOVE_PHYSICAL, root_dev);
                        if (r < 0 && ret == 0)
                                ret = r;

                        if (unlinkat(fd, de->d_name, AT_REMOVEDIR) < 0) {
                                if (ret == 0 && errno != ENOENT)
                                        ret = -errno;
                        }

                } else if (!(flags & REMOVE_ONLY_DIRECTORIES)) {

                        if (unlinkat(fd, de->d_name, 0) < 0) {
                                if (ret == 0 && errno != ENOENT)
                                        ret = -errno;
                        }
                }
        }
        return ret;
}

int rm_rf(const char *path, RemoveFlags flags) {
        int fd, r;
        struct statfs s;

        assert(path);

        /* We refuse to clean the root file system with this
         * call. This is extra paranoia to never cause a really
         * seriously broken system. */
        if (path_equal_or_files_same(path, "/", AT_SYMLINK_NOFOLLOW)) {
                log_error("Attempted to remove entire root file system (\"%s\"), and we can't allow that.", path);
                return -EPERM;
        }

#if 0 /// elogind does not support BTRFS this directly
        if ((flags & (REMOVE_SUBVOLUME|REMOVE_ROOT|REMOVE_PHYSICAL)) == (REMOVE_SUBVOLUME|REMOVE_ROOT|REMOVE_PHYSICAL)) {
                /* Try to remove as subvolume first */
                r = btrfs_subvol_remove(path, BTRFS_REMOVE_RECURSIVE|BTRFS_REMOVE_QUOTA);
                if (r >= 0)
                        return r;

                if (!IN_SET(r, -ENOTTY, -EINVAL, -ENOTDIR))
                        return r;

                /* Not btrfs or not a subvolume */
        }
#endif // 0

        fd = open(path, O_RDONLY|O_NONBLOCK|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW|O_NOATIME);
        if (fd < 0) {
                if (!IN_SET(errno, ENOTDIR, ELOOP))
                        return -errno;

                if (!(flags & REMOVE_PHYSICAL)) {
                        if (statfs(path, &s) < 0)
                                return -errno;

                        if (is_physical_fs(&s)) {
                                log_error("Attempted to remove files from a disk file system under \"%s\", refusing.", path);
                                return -EPERM;
                        }
                }

                if ((flags & REMOVE_ROOT) && !(flags & REMOVE_ONLY_DIRECTORIES))
                        if (unlink(path) < 0 && errno != ENOENT)
                                return -errno;

                return 0;
        }

        r = rm_rf_children(fd, flags, NULL);

        if (flags & REMOVE_ROOT) {
                if (rmdir(path) < 0) {
                        if (r == 0 && errno != ENOENT)
                                r = -errno;
                }
        }

        return r;
}
