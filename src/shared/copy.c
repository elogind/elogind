/* SPDX-License-Identifier: LGPL-2.1-or-later */

//#include <errno.h>
//#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/sendfile.h>
//#include <sys/xattr.h>
#ifndef __GLIBC__ /// elogind has to include <signal.h> for musl-based systems
#include <signal.h>
#endif // __GLIBC__
#include <unistd.h>

#include "alloc-util.h"
#include "btrfs-util.h"
//#include "chattr-util.h"
#include "copy.h"
//#include "dirent-util.h"
//#include "fd-util.h"
//#include "fileio.h"
//#include "fs-util.h"
//#include "io-util.h"
#include "macro.h"
//#include "missing_syscall.h"
#include "mkdir-label.h"
//#include "mountpoint-util.h"
#include "nulstr-util.h"
#include "rm-rf.h"
//#include "selinux-util.h"
#include "signal-util.h"
//#include "stat-util.h"
#include "stdio-util.h"
//#include "string-util.h"
//#include "strv.h"
//#include "sync-util.h"
#include "time-util.h"
//#include "tmpfile-util.h"
//#include "umask-util.h"
#include "user-util.h"
//#include "xattr-util.h"

#define COPY_BUFFER_SIZE (16U*1024U)

/* A safety net for descending recursively into file system trees to copy. On Linux PATH_MAX is 4096, which means the
 * deepest valid path one can build is around 2048, which we hence use as a safety net here, to not spin endlessly in
 * case of bind mount cycles and suchlike. */
#define COPY_DEPTH_MAX 2048U

static ssize_t try_copy_file_range(
                int fd_in, loff_t *off_in,
                int fd_out, loff_t *off_out,
                size_t len,
                unsigned flags) {

        static int have = -1;
        ssize_t r;

        if (have == 0)
                return -ENOSYS;

        r = copy_file_range(fd_in, off_in, fd_out, off_out, len, flags);
        if (have < 0)
                have = r >= 0 || errno != ENOSYS;
        if (r < 0)
                return -errno;

        return r;
}

enum {
        FD_IS_NO_PIPE,
        FD_IS_BLOCKING_PIPE,
        FD_IS_NONBLOCKING_PIPE,
};

static int fd_is_nonblock_pipe(int fd) {
        struct stat st;
        int flags;

        /* Checks whether the specified file descriptor refers to a pipe, and if so if O_NONBLOCK is set. */

        if (fstat(fd, &st) < 0)
                return -errno;

        if (!S_ISFIFO(st.st_mode))
                return FD_IS_NO_PIPE;

        flags = fcntl(fd, F_GETFL);
        if (flags < 0)
                return -errno;

        return FLAGS_SET(flags, O_NONBLOCK) ? FD_IS_NONBLOCKING_PIPE : FD_IS_BLOCKING_PIPE;
}

static int look_for_signals(CopyFlags copy_flags) {
        int r;

        if ((copy_flags & (COPY_SIGINT|COPY_SIGTERM)) == 0)
                return 0;

        r = pop_pending_signal(copy_flags & COPY_SIGINT ? SIGINT : 0,
                               copy_flags & COPY_SIGTERM ? SIGTERM : 0);
        if (r < 0)
                return r;
        if (r != 0)
                return log_debug_errno(SYNTHETIC_ERRNO(EINTR),
                                       "Got %s, cancelling copy operation.", signal_to_string(r));

        return 0;
}

static int create_hole(int fd, off_t size) {
        off_t offset;
        off_t end;

        offset = lseek(fd, 0, SEEK_CUR);
        if (offset < 0)
                return -errno;

        end = lseek(fd, 0, SEEK_END);
        if (end < 0)
                return -errno;

        /* If we're not at the end of the target file, try to punch a hole in the existing space using fallocate(). */

        if (offset < end &&
            fallocate(fd, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE, offset, MIN(size, end - offset)) < 0 &&
            !ERRNO_IS_NOT_SUPPORTED(errno))
                return -errno;

        if (end - offset >= size) {
                /* If we've created the full hole, set the file pointer to the end of the hole we created and exit. */
                if (lseek(fd, offset + size, SEEK_SET) < 0)
                        return -errno;

                return 0;
        }

        /* If we haven't created the full hole, use ftruncate() to grow the file (and the hole) to the
         * required size and move the file pointer to the end of the file. */

        size -= end - offset;

        if (ftruncate(fd, end + size) < 0)
                return -errno;

        if (lseek(fd, 0, SEEK_END) < 0)
                return -errno;

        return 0;
}

int copy_bytes_full(
                int fdf, int fdt,
                uint64_t max_bytes,
                CopyFlags copy_flags,
                void **ret_remains,
                size_t *ret_remains_size,
                copy_progress_bytes_t progress,
                void *userdata) {

        bool try_cfr = true, try_sendfile = true, try_splice = true, copied_something = false;
        int r, nonblock_pipe = -1;
        size_t m = SSIZE_MAX; /* that is the maximum that sendfile and c_f_r accept */

        assert(fdf >= 0);
        assert(fdt >= 0);

        /* Tries to copy bytes from the file descriptor 'fdf' to 'fdt' in the smartest possible way. Copies a maximum
         * of 'max_bytes', which may be specified as UINT64_MAX, in which no maximum is applied. Returns negative on
         * error, zero if EOF is hit before the bytes limit is hit and positive otherwise. If the copy fails for some
         * reason but we read but didn't yet write some data an ret_remains/ret_remains_size is not NULL, then it will
         * be initialized with an allocated buffer containing this "remaining" data. Note that these two parameters are
         * initialized with a valid buffer only on failure and only if there's actually data already read. Otherwise
         * these parameters if non-NULL are set to NULL. */

        if (ret_remains)
                *ret_remains = NULL;
        if (ret_remains_size)
                *ret_remains_size = 0;

        /* Try btrfs reflinks first. This only works on regular, seekable files, hence let's check the file offsets of
         * source and destination first. */
        if ((copy_flags & COPY_REFLINK)) {
                off_t foffset;

                foffset = lseek(fdf, 0, SEEK_CUR);
                if (foffset >= 0) {
                        off_t toffset;

                        toffset = lseek(fdt, 0, SEEK_CUR);
                        if (toffset >= 0) {

                                if (foffset == 0 && toffset == 0 && max_bytes == UINT64_MAX)
                                        r = btrfs_reflink(fdf, fdt); /* full file reflink */
                                else
                                        r = btrfs_clone_range(fdf, foffset, fdt, toffset, max_bytes == UINT64_MAX ? 0 : max_bytes); /* partial reflink */
                                if (r >= 0) {
                                        off_t t;

                                        /* This worked, yay! Now — to be fully correct — let's adjust the file pointers */
                                        if (max_bytes == UINT64_MAX) {

                                                /* We cloned to the end of the source file, let's position the read
                                                 * pointer there, and query it at the same time. */
                                                t = lseek(fdf, 0, SEEK_END);
                                                if (t < 0)
                                                        return -errno;
                                                if (t < foffset)
                                                        return -ESPIPE;

                                                /* Let's adjust the destination file write pointer by the same number
                                                 * of bytes. */
                                                t = lseek(fdt, toffset + (t - foffset), SEEK_SET);
                                                if (t < 0)
                                                        return -errno;

                                                return 0; /* we copied the whole thing, hence hit EOF, return 0 */
                                        } else {
                                                t = lseek(fdf, foffset + max_bytes, SEEK_SET);
                                                if (t < 0)
                                                        return -errno;

                                                t = lseek(fdt, toffset + max_bytes, SEEK_SET);
                                                if (t < 0)
                                                        return -errno;

                                                return 1; /* we copied only some number of bytes, which worked, but this means we didn't hit EOF, return 1 */
                                        }
                                }
                        }
                }
        }

        for (;;) {
                ssize_t n;

                if (max_bytes <= 0)
                        return 1; /* return > 0 if we hit the max_bytes limit */

                r = look_for_signals(copy_flags);
                if (r < 0)
                        return r;

                if (max_bytes != UINT64_MAX && m > max_bytes)
                        m = max_bytes;

                if (copy_flags & COPY_HOLES) {
                        off_t c, e;

                        c = lseek(fdf, 0, SEEK_CUR);
                        if (c < 0)
                                return -errno;

                        /* To see if we're in a hole, we search for the next data offset. */
                        e = lseek(fdf, c, SEEK_DATA);
                        if (e < 0 && errno == ENXIO)
                                /* If errno == ENXIO, that means we've reached the final hole of the file and
                                * that hole isn't followed by more data. */
                                e = lseek(fdf, 0, SEEK_END);
                        if (e < 0)
                                return -errno;

                        /* If we're in a hole (current offset is not a data offset), create a hole of the
                         * same size in the target file. */
                        if (e > c) {
                                r = create_hole(fdt, e - c);
                                if (r < 0)
                                        return r;
                        }

                        c = e; /* Set c to the start of the data segment. */

                        /* After copying a potential hole, find the end of the data segment by looking for
                         * the next hole. If we get ENXIO, we're at EOF. */
                        e = lseek(fdf, c, SEEK_HOLE);
                        if (e < 0) {
                                if (errno == ENXIO)
                                        break;
                                return -errno;
                        }

                        /* SEEK_HOLE modifies the file offset so we need to move back to the initial offset. */
                        if (lseek(fdf, c, SEEK_SET) < 0)
                                return -errno;

                        /* Make sure we're not copying more than the current data segment. */
                        m = MIN(m, (size_t) e - c);
                }

                /* First try copy_file_range(), unless we already tried */
                if (try_cfr) {
                        n = try_copy_file_range(fdf, NULL, fdt, NULL, m, 0u);
                        if (n < 0) {
                                if (!IN_SET(n, -EINVAL, -ENOSYS, -EXDEV, -EBADF))
                                        return n;

                                try_cfr = false;
                                /* use fallback below */
                        } else if (n == 0) { /* likely EOF */

                                if (copied_something)
                                        break;

                                /* So, we hit EOF immediately, without having copied a single byte. This
                                 * could indicate two things: the file is actually empty, or we are on some
                                 * virtual file system such as procfs/sysfs where the syscall actually
                                 * doesn't work but doesn't return an error. Try to handle that, by falling
                                 * back to simple read()s in case we encounter empty files.
                                 *
                                 * See: https://lwn.net/Articles/846403/ */
                                try_cfr = try_sendfile = try_splice = false;
                        } else
                                /* Success! */
                                goto next;
                }

                /* First try sendfile(), unless we already tried */
                if (try_sendfile) {
                        n = sendfile(fdt, fdf, NULL, m);
                        if (n < 0) {
                                if (!IN_SET(errno, EINVAL, ENOSYS))
                                        return -errno;

                                try_sendfile = false;
                                /* use fallback below */
                        } else if (n == 0) { /* likely EOF */

                                if (copied_something)
                                        break;

                                try_sendfile = try_splice = false; /* same logic as above for copy_file_range() */
                        } else
                                /* Success! */
                                goto next;
                }

                /* Then try splice, unless we already tried. */
                if (try_splice) {

                        /* splice()'s asynchronous I/O support is a bit weird. When it encounters a pipe file
                         * descriptor, then it will ignore its O_NONBLOCK flag and instead only honour the
                         * SPLICE_F_NONBLOCK flag specified in its flag parameter. Let's hide this behaviour
                         * here, and check if either of the specified fds are a pipe, and if so, let's pass
                         * the flag automatically, depending on O_NONBLOCK being set.
                         *
                         * Here's a twist though: when we use it to move data between two pipes of which one
                         * has O_NONBLOCK set and the other has not, then we have no individual control over
                         * O_NONBLOCK behaviour. Hence in that case we can't use splice() and still guarantee
                         * systematic O_NONBLOCK behaviour, hence don't. */

                        if (nonblock_pipe < 0) {
                                int a, b;

                                /* Check if either of these fds is a pipe, and if so non-blocking or not */
                                a = fd_is_nonblock_pipe(fdf);
                                if (a < 0)
                                        return a;

                                b = fd_is_nonblock_pipe(fdt);
                                if (b < 0)
                                        return b;

                                if ((a == FD_IS_NO_PIPE && b == FD_IS_NO_PIPE) ||
                                    (a == FD_IS_BLOCKING_PIPE && b == FD_IS_NONBLOCKING_PIPE) ||
                                    (a == FD_IS_NONBLOCKING_PIPE && b == FD_IS_BLOCKING_PIPE))

                                        /* splice() only works if one of the fds is a pipe. If neither is,
                                         * let's skip this step right-away. As mentioned above, if one of the
                                         * two fds refers to a blocking pipe and the other to a non-blocking
                                         * pipe, we can't use splice() either, hence don't try either. This
                                         * hence means we can only use splice() if either only one of the two
                                         * fds is a pipe, or if both are pipes with the same nonblocking flag
                                         * setting. */

                                        try_splice = false;
                                else
                                        nonblock_pipe = a == FD_IS_NONBLOCKING_PIPE || b == FD_IS_NONBLOCKING_PIPE;
                        }
                }

                if (try_splice) {
                        n = splice(fdf, NULL, fdt, NULL, m, nonblock_pipe ? SPLICE_F_NONBLOCK : 0);
                        if (n < 0) {
                                if (!IN_SET(errno, EINVAL, ENOSYS))
                                        return -errno;

                                try_splice = false;
                                /* use fallback below */
                        } else if (n == 0) { /* likely EOF */

                                if (copied_something)
                                        break;

                                try_splice = false; /* same logic as above for copy_file_range() + sendfile() */
                        } else
                                /* Success! */
                                goto next;
                }

                /* As a fallback just copy bits by hand */
                {
                        uint8_t buf[MIN(m, COPY_BUFFER_SIZE)], *p = buf;
                        ssize_t z;

                        n = read(fdf, buf, sizeof buf);
                        if (n < 0)
                                return -errno;
                        if (n == 0) /* EOF */
                                break;

                        z = (size_t) n;
                        do {
                                ssize_t k;

                                k = write(fdt, p, z);
                                if (k < 0) {
                                        r = -errno;

                                        if (ret_remains) {
                                                void *copy;

                                                copy = memdup(p, z);
                                                if (!copy)
                                                        return -ENOMEM;

                                                *ret_remains = copy;
                                        }

                                        if (ret_remains_size)
                                                *ret_remains_size = z;

                                        return r;
                                }

                                assert(k <= z);
                                z -= k;
                                p += k;
                        } while (z > 0);
                }

        next:
                if (progress) {
                        r = progress(n, userdata);
                        if (r < 0)
                                return r;
                }

                if (max_bytes != UINT64_MAX) {
                        assert(max_bytes >= (uint64_t) n);
                        max_bytes -= n;
                }

                /* sendfile accepts at most SSIZE_MAX-offset bytes to copy, so reduce our maximum by the
                 * amount we already copied, but don't go below our copy buffer size, unless we are close the
                 * limit of bytes we are allowed to copy. */
                m = MAX(MIN(COPY_BUFFER_SIZE, max_bytes), m - n);

                copied_something = true;
        }

        return 0; /* return 0 if we hit EOF earlier than the size limit */
}

#if 0 /// UNNEEDED by elogind
static int fd_copy_symlink(
                int df,
                const char *from,
                const struct stat *st,
                int dt,
                const char *to,
                uid_t override_uid,
                gid_t override_gid,
                CopyFlags copy_flags) {

        _cleanup_free_ char *target = NULL;
        int r;

        assert(from);
        assert(st);
        assert(to);

        r = readlinkat_malloc(df, from, &target);
        if (r < 0)
                return r;

        if (copy_flags & COPY_MAC_CREATE) {
                r = mac_selinux_create_file_prepare_at(dt, to, S_IFLNK);
                if (r < 0)
                        return r;
        }
        r = symlinkat(target, dt, to);
        if (copy_flags & COPY_MAC_CREATE)
                mac_selinux_create_file_clear();
        if (r < 0)
                return -errno;

        if (fchownat(dt, to,
                     uid_is_valid(override_uid) ? override_uid : st->st_uid,
                     gid_is_valid(override_gid) ? override_gid : st->st_gid,
                     AT_SYMLINK_NOFOLLOW) < 0)
                r = -errno;

        (void) utimensat(dt, to, (struct timespec[]) { st->st_atim, st->st_mtim }, AT_SYMLINK_NOFOLLOW);
        return r;
}

/* Encapsulates the database we store potential hardlink targets in */
typedef struct HardlinkContext {
        int dir_fd;    /* An fd to the directory we use as lookup table. Never AT_FDCWD. Lazily created, when
                        * we add the first entry. */

        /* These two fields are used to create the hardlink repository directory above — via
         * mkdirat(parent_fd, subdir) — and are kept so that we can automatically remove the directory again
         * when we are done. */
        int parent_fd; /* Possibly AT_FDCWD */
        char *subdir;
} HardlinkContext;

static int hardlink_context_setup(
                HardlinkContext *c,
                int dt,
                const char *to,
                CopyFlags copy_flags) {

        _cleanup_close_ int dt_copy = -1;
        int r;

        assert(c);
        assert(c->dir_fd < 0 && c->dir_fd != AT_FDCWD);
        assert(c->parent_fd < 0);
        assert(!c->subdir);

        /* If hardlink recreation is requested we have to maintain a database of inodes that are potential
         * hardlink sources. Given that generally disk sizes have to be assumed to be larger than what fits
         * into physical RAM we cannot maintain that database in dynamic memory alone. Here we opt to
         * maintain it on disk, to simplify things: inside the destination directory we'll maintain a
         * temporary directory consisting of hardlinks of every inode we copied that might be subject of
         * hardlinks. We can then use that as hardlink source later on. Yes, this means additional disk IO
         * but thankfully Linux is optimized for this kind of thing. If this ever becomes a performance
         * bottleneck we can certainly place an in-memory hash table in front of this, but for the beginning,
         * let's keep things simple, and just use the disk as lookup table for inodes.
         *
         * Note that this should have zero performance impact as long as .n_link of all files copied remains
         * <= 0, because in that case we will not actually allocate the hardlink inode lookup table directory
         * on disk (we do so lazily, when the first candidate with .n_link > 1 is seen). This means, in the
         * common case where hardlinks are not used at all or only for few files the fact that we store the
         * table on disk shouldn't matter perfomance-wise. */

        if (!FLAGS_SET(copy_flags, COPY_HARDLINKS))
                return 0;

        if (dt == AT_FDCWD)
                dt_copy = AT_FDCWD;
        else if (dt < 0)
                return -EBADF;
        else {
                dt_copy = fcntl(dt, F_DUPFD_CLOEXEC, 3);
                if (dt_copy < 0)
                        return -errno;
        }

        r = tempfn_random_child(to, "hardlink", &c->subdir);
        if (r < 0)
                return r;

        c->parent_fd = TAKE_FD(dt_copy);

        /* We don't actually create the directory we keep the table in here, that's done on-demand when the
         * first entry is added, using hardlink_context_realize() below. */
        return 1;
}

static int hardlink_context_realize(HardlinkContext *c) {
        if (!c)
                return 0;

        if (c->dir_fd >= 0) /* Already realized */
                return 1;

        if (c->parent_fd < 0 && c->parent_fd != AT_FDCWD) /* Not configured */
                return 0;

        assert(c->subdir);

        c->dir_fd = open_mkdir_at(c->parent_fd, c->subdir, O_EXCL|O_CLOEXEC, 0700);
        if (c->dir_fd < 0)
                return c->dir_fd;

        return 1;
}

static void hardlink_context_destroy(HardlinkContext *c) {
        int r;

        assert(c);

        /* Automatically remove the hardlink lookup table directory again after we are done. This is used via
         * _cleanup_() so that we really delete this, even on failure. */

        if (c->dir_fd >= 0) {
                r = rm_rf_children(TAKE_FD(c->dir_fd), REMOVE_PHYSICAL, NULL); /* consumes dir_fd in all cases, even on failure */
                if (r < 0)
                        log_debug_errno(r, "Failed to remove hardlink store (%s) contents, ignoring: %m", c->subdir);

                assert(c->parent_fd >= 0 || c->parent_fd == AT_FDCWD);
                assert(c->subdir);

                if (unlinkat(c->parent_fd, c->subdir, AT_REMOVEDIR) < 0)
                        log_debug_errno(errno, "Failed to remove hardlink store (%s) directory, ignoring: %m", c->subdir);
        }

        assert_cc(AT_FDCWD < 0);
        c->parent_fd = safe_close(c->parent_fd);

        c->subdir = mfree(c->subdir);
}

static int try_hardlink(
                HardlinkContext *c,
                const struct stat *st,
                int dt,
                const char *to) {

        char dev_ino[DECIMAL_STR_MAX(dev_t)*2 + DECIMAL_STR_MAX(uint64_t) + 4];

        assert(st);
        assert(dt >= 0 || dt == AT_FDCWD);
        assert(to);

        if (!c) /* No temporary hardlink directory, don't bother */
                return 0;

        if (st->st_nlink <= 1) /* Source not hardlinked, don't bother */
                return 0;

        if (c->dir_fd < 0) /* not yet realized, hence empty */
                return 0;

        xsprintf(dev_ino, "%u:%u:%" PRIu64, major(st->st_dev), minor(st->st_dev), (uint64_t) st->st_ino);
        if (linkat(c->dir_fd, dev_ino, dt, to, 0) < 0)  {
                if (errno != ENOENT) /* doesn't exist in store yet */
                        log_debug_errno(errno, "Failed to hardlink %s to %s, ignoring: %m", dev_ino, to);
                return 0;
        }

        return 1;
}

static int memorize_hardlink(
                HardlinkContext *c,
                const struct stat *st,
                int dt,
                const char *to) {

        char dev_ino[DECIMAL_STR_MAX(dev_t)*2 + DECIMAL_STR_MAX(uint64_t) + 4];
        int r;

        assert(st);
        assert(dt >= 0 || dt == AT_FDCWD);
        assert(to);

        if (!c) /* No temporary hardlink directory, don't bother */
                return 0;

        if (st->st_nlink <= 1) /* Source not hardlinked, don't bother */
                return 0;

        r = hardlink_context_realize(c); /* Create the hardlink store lazily */
        if (r < 0)
                return r;

        xsprintf(dev_ino, "%u:%u:%" PRIu64, major(st->st_dev), minor(st->st_dev), (uint64_t) st->st_ino);
        if (linkat(dt, to, c->dir_fd, dev_ino, 0) < 0) {
                log_debug_errno(errno, "Failed to hardlink %s to %s, ignoring: %m", to, dev_ino);
                return 0;
        }

        return 1;
}

static int fd_copy_tree_generic(
                int df,
                const char *from,
                const struct stat *st,
                int dt,
                const char *to,
                dev_t original_device,
                unsigned depth_left,
                uid_t override_uid,
                gid_t override_gid,
                CopyFlags copy_flags,
                HardlinkContext *hardlink_context,
                const char *display_path,
                copy_progress_path_t progress_path,
                copy_progress_bytes_t progress_bytes,
                void *userdata);

static int fd_copy_regular(
                int df,
                const char *from,
                const struct stat *st,
                int dt,
                const char *to,
                uid_t override_uid,
                gid_t override_gid,
                CopyFlags copy_flags,
                HardlinkContext *hardlink_context,
                copy_progress_bytes_t progress,
                void *userdata) {

        _cleanup_close_ int fdf = -1, fdt = -1;
        int r, q;

        assert(from);
        assert(st);
        assert(to);

        r = try_hardlink(hardlink_context, st, dt, to);
        if (r < 0)
                return r;
        if (r > 0) /* worked! */
                return 0;

        fdf = openat(df, from, O_RDONLY|O_CLOEXEC|O_NOCTTY|O_NOFOLLOW);
        if (fdf < 0)
                return -errno;

        if (copy_flags & COPY_MAC_CREATE) {
                r = mac_selinux_create_file_prepare_at(dt, to, S_IFREG);
                if (r < 0)
                        return r;
        }
        fdt = openat(dt, to, O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOCTTY|O_NOFOLLOW, st->st_mode & 07777);
        if (copy_flags & COPY_MAC_CREATE)
                mac_selinux_create_file_clear();
        if (fdt < 0)
                return -errno;

        r = copy_bytes_full(fdf, fdt, UINT64_MAX, copy_flags, NULL, NULL, progress, userdata);
        if (r < 0)
                goto fail;

        if (fchown(fdt,
                   uid_is_valid(override_uid) ? override_uid : st->st_uid,
                   gid_is_valid(override_gid) ? override_gid : st->st_gid) < 0)
                r = -errno;

        if (fchmod(fdt, st->st_mode & 07777) < 0)
                r = -errno;

        (void) futimens(fdt, (struct timespec[]) { st->st_atim, st->st_mtim });
        (void) copy_xattr(fdf, fdt, copy_flags);

        if (copy_flags & COPY_FSYNC) {
                if (fsync(fdt) < 0) {
                        r = -errno;
                        goto fail;
                }
        }

        q = close_nointr(TAKE_FD(fdt)); /* even if this fails, the fd is now invalidated */
        if (q < 0) {
                r = q;
                goto fail;
        }

        (void) memorize_hardlink(hardlink_context, st, dt, to);
        return r;

fail:
        (void) unlinkat(dt, to, 0);
        return r;
}

static int fd_copy_fifo(
                int df,
                const char *from,
                const struct stat *st,
                int dt,
                const char *to,
                uid_t override_uid,
                gid_t override_gid,
                CopyFlags copy_flags,
                HardlinkContext *hardlink_context) {
        int r;

        assert(from);
        assert(st);
        assert(to);

        r = try_hardlink(hardlink_context, st, dt, to);
        if (r < 0)
                return r;
        if (r > 0) /* worked! */
                return 0;

        if (copy_flags & COPY_MAC_CREATE) {
                r = mac_selinux_create_file_prepare_at(dt, to, S_IFIFO);
                if (r < 0)
                        return r;
        }
        r = mkfifoat(dt, to, st->st_mode & 07777);
        if (copy_flags & COPY_MAC_CREATE)
                mac_selinux_create_file_clear();
        if (r < 0)
                return -errno;

        if (fchownat(dt, to,
                     uid_is_valid(override_uid) ? override_uid : st->st_uid,
                     gid_is_valid(override_gid) ? override_gid : st->st_gid,
                     AT_SYMLINK_NOFOLLOW) < 0)
                r = -errno;

        if (fchmodat(dt, to, st->st_mode & 07777, 0) < 0)
                r = -errno;

        (void) utimensat(dt, to, (struct timespec[]) { st->st_atim, st->st_mtim }, AT_SYMLINK_NOFOLLOW);

        (void) memorize_hardlink(hardlink_context, st, dt, to);
        return r;
}

static int fd_copy_node(
                int df,
                const char *from,
                const struct stat *st,
                int dt,
                const char *to,
                uid_t override_uid,
                gid_t override_gid,
                CopyFlags copy_flags,
                HardlinkContext *hardlink_context) {
        int r;

        assert(from);
        assert(st);
        assert(to);

        r = try_hardlink(hardlink_context, st, dt, to);
        if (r < 0)
                return r;
        if (r > 0) /* worked! */
                return 0;

        if (copy_flags & COPY_MAC_CREATE) {
                r = mac_selinux_create_file_prepare_at(dt, to, st->st_mode & S_IFMT);
                if (r < 0)
                        return r;
        }
        r = mknodat(dt, to, st->st_mode, st->st_rdev);
        if (copy_flags & COPY_MAC_CREATE)
                mac_selinux_create_file_clear();
        if (r < 0)
                return -errno;

        if (fchownat(dt, to,
                     uid_is_valid(override_uid) ? override_uid : st->st_uid,
                     gid_is_valid(override_gid) ? override_gid : st->st_gid,
                     AT_SYMLINK_NOFOLLOW) < 0)
                r = -errno;

        if (fchmodat(dt, to, st->st_mode & 07777, 0) < 0)
                r = -errno;

        (void) utimensat(dt, to, (struct timespec[]) { st->st_atim, st->st_mtim }, AT_SYMLINK_NOFOLLOW);

        (void) memorize_hardlink(hardlink_context, st, dt, to);
        return r;
}

static int fd_copy_directory(
                int df,
                const char *from,
                const struct stat *st,
                int dt,
                const char *to,
                dev_t original_device,
                unsigned depth_left,
                uid_t override_uid,
                gid_t override_gid,
                CopyFlags copy_flags,
                HardlinkContext *hardlink_context,
                const char *display_path,
                copy_progress_path_t progress_path,
                copy_progress_bytes_t progress_bytes,
                void *userdata) {

        _cleanup_(hardlink_context_destroy) HardlinkContext our_hardlink_context = {
                .dir_fd = -1,
                .parent_fd = -1,
        };

        _cleanup_close_ int fdf = -1, fdt = -1;
        _cleanup_closedir_ DIR *d = NULL;
        bool exists, created;
        int r;

        assert(st);
        assert(to);

        if (depth_left == 0)
                return -ENAMETOOLONG;

        if (from)
                fdf = openat(df, from, O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOCTTY|O_NOFOLLOW);
        else
                fdf = fcntl(df, F_DUPFD_CLOEXEC, 3);
        if (fdf < 0)
                return -errno;

        if (!hardlink_context) {
                /* If recreating hardlinks is requested let's set up a context for that now. */
                r = hardlink_context_setup(&our_hardlink_context, dt, to, copy_flags);
                if (r < 0)
                        return r;
                if (r > 0) /* It's enabled and allocated, let's now use the same context for all recursive
                            * invocations from here down */
                        hardlink_context = &our_hardlink_context;
        }

        d = take_fdopendir(&fdf);
        if (!d)
                return -errno;

        exists = false;
        if (copy_flags & COPY_MERGE_EMPTY) {
                r = dir_is_empty_at(dt, to, /* ignore_hidden_or_backup= */ false);
                if (r < 0 && r != -ENOENT)
                        return r;
                else if (r == 1)
                        exists = true;
        }

        if (exists)
                created = false;
        else {
                if (copy_flags & COPY_MAC_CREATE)
                        r = mkdirat_label(dt, to, st->st_mode & 07777);
                else
                        r = mkdirat(dt, to, st->st_mode & 07777);
                if (r >= 0)
                        created = true;
                else if (errno == EEXIST && (copy_flags & COPY_MERGE))
                        created = false;
                else
                        return -errno;
        }

        fdt = openat(dt, to, O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOCTTY|O_NOFOLLOW);
        if (fdt < 0)
                return -errno;

        r = 0;

        FOREACH_DIRENT_ALL(de, d, return -errno) {
                const char *child_display_path = NULL;
                _cleanup_free_ char *dp = NULL;
                struct stat buf;
                int q;

                if (dot_or_dot_dot(de->d_name))
                        continue;

                r = look_for_signals(copy_flags);
                if (r < 0)
                        return r;

                if (fstatat(dirfd(d), de->d_name, &buf, AT_SYMLINK_NOFOLLOW) < 0) {
                        r = -errno;
                        continue;
                }

                if (progress_path) {
                        if (display_path)
                                child_display_path = dp = path_join(display_path, de->d_name);
                        else
                                child_display_path = de->d_name;

                        r = progress_path(child_display_path, &buf, userdata);
                        if (r < 0)
                                return r;
                }

                if (S_ISDIR(buf.st_mode)) {
                        /*
                         * Don't descend into directories on other file systems, if this is requested. We do a simple
                         * .st_dev check here, which basically comes for free. Note that we do this check only on
                         * directories, not other kind of file system objects, for two reason:
                         *
                         * • The kernel's overlayfs pseudo file system that overlays multiple real file systems
                         *   propagates the .st_dev field of the file system a file originates from all the way up
                         *   through the stack to stat(). It doesn't do that for directories however. This means that
                         *   comparing .st_dev on non-directories suggests that they all are mount points. To avoid
                         *   confusion we hence avoid relying on this check for regular files.
                         *
                         * • The main reason we do this check at all is to protect ourselves from bind mount cycles,
                         *   where we really want to avoid descending down in all eternity. However the .st_dev check
                         *   is usually not sufficient for this protection anyway, as bind mount cycles from the same
                         *   file system onto itself can't be detected that way. (Note we also do a recursion depth
                         *   check, which is probably the better protection in this regard, which is why
                         *   COPY_SAME_MOUNT is optional).
                         */

                        if (FLAGS_SET(copy_flags, COPY_SAME_MOUNT)) {
                                if (buf.st_dev != original_device)
                                        continue;

                                r = fd_is_mount_point(dirfd(d), de->d_name, 0);
                                if (r < 0)
                                        return r;
                                if (r > 0)
                                        continue;
                        }
                }

                q = fd_copy_tree_generic(dirfd(d), de->d_name, &buf, fdt, de->d_name, original_device, depth_left-1, override_uid, override_gid, copy_flags, hardlink_context, child_display_path, progress_path, progress_bytes, userdata);

                if (q == -EINTR) /* Propagate SIGINT/SIGTERM up instantly */
                        return q;
                if (q == -EEXIST && (copy_flags & COPY_MERGE))
                        q = 0;
                if (q < 0)
                        r = q;
        }

        if (created) {
                if (fchown(fdt,
                           uid_is_valid(override_uid) ? override_uid : st->st_uid,
                           gid_is_valid(override_gid) ? override_gid : st->st_gid) < 0)
                        r = -errno;

                if (fchmod(fdt, st->st_mode & 07777) < 0)
                        r = -errno;

                (void) copy_xattr(dirfd(d), fdt, copy_flags);
                (void) futimens(fdt, (struct timespec[]) { st->st_atim, st->st_mtim });
        }

        if (copy_flags & COPY_FSYNC_FULL) {
                if (fsync(fdt) < 0)
                        return -errno;
        }

        return r;
}

static int fd_copy_leaf(
                int df,
                const char *from,
                const struct stat *st,
                int dt,
                const char *to,
                uid_t override_uid,
                gid_t override_gid,
                CopyFlags copy_flags,
                HardlinkContext *hardlink_context,
                const char *display_path,
                copy_progress_bytes_t progress_bytes,
                void *userdata) {
        int r;

        if (S_ISREG(st->st_mode))
                r = fd_copy_regular(df, from, st, dt, to, override_uid, override_gid, copy_flags, hardlink_context, progress_bytes, userdata);
        else if (S_ISLNK(st->st_mode))
                r = fd_copy_symlink(df, from, st, dt, to, override_uid, override_gid, copy_flags);
        else if (S_ISFIFO(st->st_mode))
                r = fd_copy_fifo(df, from, st, dt, to, override_uid, override_gid, copy_flags, hardlink_context);
        else if (S_ISBLK(st->st_mode) || S_ISCHR(st->st_mode) || S_ISSOCK(st->st_mode))
                r = fd_copy_node(df, from, st, dt, to, override_uid, override_gid, copy_flags, hardlink_context);
        else
                r = -EOPNOTSUPP;

        return r;
}

static int fd_copy_tree_generic(
                int df,
                const char *from,
                const struct stat *st,
                int dt,
                const char *to,
                dev_t original_device,
                unsigned depth_left,
                uid_t override_uid,
                gid_t override_gid,
                CopyFlags copy_flags,
                HardlinkContext *hardlink_context,
                const char *display_path,
                copy_progress_path_t progress_path,
                copy_progress_bytes_t progress_bytes,
                void *userdata) {
        int r;

        if (S_ISDIR(st->st_mode))
                return fd_copy_directory(df, from, st, dt, to, original_device, depth_left-1, override_uid, override_gid, copy_flags, hardlink_context, display_path, progress_path, progress_bytes, userdata);

        r = fd_copy_leaf(df, from, st, dt, to, override_uid, override_gid, copy_flags, hardlink_context, display_path, progress_bytes, userdata);
        /* We just tried to copy a leaf node of the tree. If it failed because the node already exists *and* the COPY_REPLACE flag has been provided, we should unlink the node and re-copy. */
        if (r == -EEXIST && (copy_flags & COPY_REPLACE)) {
                /* This codepath is us trying to address an error to copy, if the unlink fails, lets just return the original error. */
                if (unlinkat(dt, to, 0) < 0)
                        return r;

                r = fd_copy_leaf(df, from, st, dt, to, override_uid, override_gid, copy_flags, hardlink_context, display_path, progress_bytes, userdata);
        }

        return r;
}

int copy_tree_at_full(
                int fdf,
                const char *from,
                int fdt,
                const char *to,
                uid_t override_uid,
                gid_t override_gid,
                CopyFlags copy_flags,
                copy_progress_path_t progress_path,
                copy_progress_bytes_t progress_bytes,
                void *userdata) {

        struct stat st;
        int r;

        assert(from);
        assert(to);

        if (fstatat(fdf, from, &st, AT_SYMLINK_NOFOLLOW) < 0)
                return -errno;

        r = fd_copy_tree_generic(fdf, from, &st, fdt, to, st.st_dev, COPY_DEPTH_MAX, override_uid, override_gid, copy_flags, NULL, NULL, progress_path, progress_bytes, userdata);
        if (r < 0)
                return r;

        if (S_ISDIR(st.st_mode) && (copy_flags & COPY_SYNCFS)) {
                /* If the top-level inode is a directory run syncfs() now. */
                r = syncfs_path(fdt, to);
                if (r < 0)
                        return r;
        } else if ((copy_flags & (COPY_FSYNC_FULL|COPY_SYNCFS)) != 0) {
                /* fsync() the parent dir of what we just copied if COPY_FSYNC_FULL is set. Also do this in
                 * case COPY_SYNCFS is set but the top-level inode wasn't actually a directory. We do this so that
                 * COPY_SYNCFS provides reasonable synchronization semantics on any kind of inode: when the
                 * copy operation is done the whole inode — regardless of its type — and all its children
                 * will be synchronized to disk. */
                r = fsync_parent_at(fdt, to);
                if (r < 0)
                        return r;
        }

        return 0;
}

static int sync_dir_by_flags(const char *path, CopyFlags copy_flags) {

        if (copy_flags & COPY_SYNCFS)
                return syncfs_path(AT_FDCWD, path);
        if (copy_flags & COPY_FSYNC_FULL)
                return fsync_parent_at(AT_FDCWD, path);

        return 0;
}

int copy_directory_fd_full(
                int dirfd,
                const char *to,
                CopyFlags copy_flags,
                copy_progress_path_t progress_path,
                copy_progress_bytes_t progress_bytes,
                void *userdata) {

        struct stat st;
        int r;

        assert(dirfd >= 0);
        assert(to);

        if (fstat(dirfd, &st) < 0)
                return -errno;

        r = stat_verify_directory(&st);
        if (r < 0)
                return r;

        r = fd_copy_directory(
                        dirfd, NULL,
                        &st,
                        AT_FDCWD, to,
                        st.st_dev,
                        COPY_DEPTH_MAX,
                        UID_INVALID, GID_INVALID,
                        copy_flags,
                        NULL, NULL,
                        progress_path,
                        progress_bytes,
                        userdata);
        if (r < 0)
                return r;

        r = sync_dir_by_flags(to, copy_flags);
        if (r < 0)
                return r;

        return 0;
}

int copy_directory_full(
                const char *from,
                const char *to,
                CopyFlags copy_flags,
                copy_progress_path_t progress_path,
                copy_progress_bytes_t progress_bytes,
                void *userdata) {

        struct stat st;
        int r;

        assert(from);
        assert(to);

        if (lstat(from, &st) < 0)
                return -errno;

        r = stat_verify_directory(&st);
        if (r < 0)
                return r;

        r = fd_copy_directory(
                        AT_FDCWD, from,
                        &st,
                        AT_FDCWD, to,
                        st.st_dev,
                        COPY_DEPTH_MAX,
                        UID_INVALID, GID_INVALID,
                        copy_flags,
                        NULL, NULL,
                        progress_path,
                        progress_bytes,
                        userdata);
        if (r < 0)
                return r;

        r = sync_dir_by_flags(to, copy_flags);
        if (r < 0)
                return r;

        return 0;
}

int copy_file_fd_full(
                const char *from,
                int fdt,
                CopyFlags copy_flags,
                copy_progress_bytes_t progress_bytes,
                void *userdata) {

        _cleanup_close_ int fdf = -1;
        struct stat st;
        int r;

        assert(from);
        assert(fdt >= 0);

        fdf = open(from, O_RDONLY|O_CLOEXEC|O_NOCTTY);
        if (fdf < 0)
                return -errno;

        r = fd_verify_regular(fdf);
        if (r < 0)
                return r;

        if (fstat(fdt, &st) < 0)
                return -errno;

        r = copy_bytes_full(fdf, fdt, UINT64_MAX, copy_flags, NULL, NULL, progress_bytes, userdata);
        if (r < 0)
                return r;

        /* Make sure to copy file attributes only over if target is a regular
         * file (so that copying a file to /dev/null won't alter the access
         * mode/ownership of that device node...) */
        if (S_ISREG(st.st_mode)) {
                (void) copy_times(fdf, fdt, copy_flags);
                (void) copy_xattr(fdf, fdt, copy_flags);
        }

        if (copy_flags & COPY_FSYNC_FULL) {
                r = fsync_full(fdt);
                if (r < 0)
                        return r;
        } else if (copy_flags & COPY_FSYNC) {
                if (fsync(fdt) < 0)
                        return -errno;
        }

        return 0;
}

int copy_file_full(
                const char *from,
                const char *to,
                int flags,
                mode_t mode,
                unsigned chattr_flags,
                unsigned chattr_mask,
                CopyFlags copy_flags,
                copy_progress_bytes_t progress_bytes,
                void *userdata) {

        _cleanup_close_ int fdf = -1, fdt = -1;
        struct stat st;
        int r;

        assert(from);
        assert(to);

        fdf = open(from, O_RDONLY|O_CLOEXEC|O_NOCTTY);
        if (fdf < 0)
                return -errno;

        if (fstat(fdf, &st) < 0)
                return -errno;

        r = stat_verify_regular(&st);
        if (r < 0)
                return r;

        RUN_WITH_UMASK(0000) {
                if (copy_flags & COPY_MAC_CREATE) {
                        r = mac_selinux_create_file_prepare(to, S_IFREG);
                        if (r < 0)
                                return r;
                }
                fdt = open(to, flags|O_WRONLY|O_CREAT|O_CLOEXEC|O_NOCTTY,
                           mode != MODE_INVALID ? mode : st.st_mode);
                if (copy_flags & COPY_MAC_CREATE)
                        mac_selinux_create_file_clear();
                if (fdt < 0)
                        return -errno;
        }

        if (!FLAGS_SET(flags, O_EXCL)) { /* if O_EXCL was used we created the thing as regular file, no need to check again */
                r = fd_verify_regular(fdt);
                if (r < 0)
                        goto fail;
        }

        if (chattr_mask != 0)
                (void) chattr_fd(fdt, chattr_flags, chattr_mask & CHATTR_EARLY_FL, NULL);

        r = copy_bytes_full(fdf, fdt, UINT64_MAX, copy_flags, NULL, NULL, progress_bytes, userdata);
        if (r < 0)
                goto fail;

        (void) copy_times(fdf, fdt, copy_flags);
        (void) copy_xattr(fdf, fdt, copy_flags);

        if (chattr_mask != 0)
                (void) chattr_fd(fdt, chattr_flags, chattr_mask & ~CHATTR_EARLY_FL, NULL);

        if (copy_flags & (COPY_FSYNC|COPY_FSYNC_FULL)) {
                if (fsync(fdt) < 0) {
                        r = -errno;
                        goto fail;
                }
        }

        r = close_nointr(TAKE_FD(fdt)); /* even if this fails, the fd is now invalidated */
        if (r < 0)
                goto fail;

        if (copy_flags & COPY_FSYNC_FULL) {
                r = fsync_parent_at(AT_FDCWD, to);
                if (r < 0)
                        goto fail;
        }

        return 0;

fail:
        /* Only unlink if we definitely are the ones who created the file */
        if (FLAGS_SET(flags, O_EXCL))
                (void) unlink(to);

        return r;
}

int copy_file_atomic_full(
                const char *from,
                const char *to,
                mode_t mode,
                unsigned chattr_flags,
                unsigned chattr_mask,
                CopyFlags copy_flags,
                copy_progress_bytes_t progress_bytes,
                void *userdata) {

        _cleanup_(unlink_and_freep) char *t = NULL;
        _cleanup_close_ int fdt = -1;
        int r;

        assert(from);
        assert(to);

        /* We try to use O_TMPFILE here to create the file if we can. Note that this only works if COPY_REPLACE is not
         * set though as we need to use linkat() for linking the O_TMPFILE file into the file system but that system
         * call can't replace existing files. Hence, if COPY_REPLACE is set we create a temporary name in the file
         * system right-away and unconditionally which we then can renameat() to the right name after we completed
         * writing it. */

        if (copy_flags & COPY_REPLACE) {
                _cleanup_free_ char *f = NULL;

                r = tempfn_random(to, NULL, &f);
                if (r < 0)
                        return r;

                if (copy_flags & COPY_MAC_CREATE) {
                        r = mac_selinux_create_file_prepare(to, S_IFREG);
                        if (r < 0)
                                return r;
                }
                fdt = open(f, O_CREAT|O_EXCL|O_NOFOLLOW|O_NOCTTY|O_WRONLY|O_CLOEXEC, 0600);
                if (copy_flags & COPY_MAC_CREATE)
                        mac_selinux_create_file_clear();
                if (fdt < 0)
                        return -errno;

                t = TAKE_PTR(f);
        } else {
                if (copy_flags & COPY_MAC_CREATE) {
                        r = mac_selinux_create_file_prepare(to, S_IFREG);
                        if (r < 0)
                                return r;
                }
                fdt = open_tmpfile_linkable(to, O_WRONLY|O_CLOEXEC, &t);
                if (copy_flags & COPY_MAC_CREATE)
                        mac_selinux_create_file_clear();
                if (fdt < 0)
                        return fdt;
        }

        if (chattr_mask != 0)
                (void) chattr_fd(fdt, chattr_flags, chattr_mask & CHATTR_EARLY_FL, NULL);

        r = copy_file_fd_full(from, fdt, copy_flags, progress_bytes, userdata);
        if (r < 0)
                return r;

        if (fchmod(fdt, mode) < 0)
                return -errno;

        if ((copy_flags & (COPY_FSYNC|COPY_FSYNC_FULL))) {
                /* Sync the file */
                if (fsync(fdt) < 0)
                        return -errno;
        }

        if (copy_flags & COPY_REPLACE) {
                if (renameat(AT_FDCWD, t, AT_FDCWD, to) < 0)
                        return -errno;
        } else {
                r = link_tmpfile(fdt, t, to);
                if (r < 0)
                        return r;
        }

        t = mfree(t);

        if (chattr_mask != 0)
                (void) chattr_fd(fdt, chattr_flags, chattr_mask & ~CHATTR_EARLY_FL, NULL);

        r = close_nointr(TAKE_FD(fdt)); /* even if this fails, the fd is now invalidated */
        if (r < 0)
                goto fail;

        if (copy_flags & COPY_FSYNC_FULL) {
                /* Sync the parent directory */
                r = fsync_parent_at(AT_FDCWD, to);
                if (r < 0)
                        goto fail;
        }

        return 0;

fail:
        (void) unlink(to);
        return r;
}

int copy_times(int fdf, int fdt, CopyFlags flags) {
        struct stat st;

        assert(fdf >= 0);
        assert(fdt >= 0);

        if (fstat(fdf, &st) < 0)
                return -errno;

        if (futimens(fdt, (struct timespec[2]) { st.st_atim, st.st_mtim }) < 0)
                return -errno;

        if (FLAGS_SET(flags, COPY_CRTIME)) {
                usec_t crtime;

                if (fd_getcrtime(fdf, &crtime) >= 0)
                        (void) fd_setcrtime(fdt, crtime);
        }

        return 0;
}

int copy_access(int fdf, int fdt) {
        struct stat st;

        assert(fdf >= 0);
        assert(fdt >= 0);

        /* Copies just the access mode (and not the ownership) from fdf to fdt */

        if (fstat(fdf, &st) < 0)
                return -errno;

        return RET_NERRNO(fchmod(fdt, st.st_mode & 07777));
}

int copy_rights_with_fallback(int fdf, int fdt, const char *patht) {
        struct stat st;

        assert(fdf >= 0);
        assert(fdt >= 0);

        /* Copies both access mode and ownership from fdf to fdt */

        if (fstat(fdf, &st) < 0)
                return -errno;

        return fchmod_and_chown_with_fallback(fdt, patht, st.st_mode & 07777, st.st_uid, st.st_gid);
}

int copy_xattr(int fdf, int fdt, CopyFlags copy_flags) {
        _cleanup_free_ char *names = NULL;
        int ret = 0, r;
        const char *p;

        r = flistxattr_malloc(fdf, &names);
        if (r < 0)
                return r;

        NULSTR_FOREACH(p, names) {
                _cleanup_free_ char *value = NULL;

                if (!FLAGS_SET(copy_flags, COPY_ALL_XATTRS) && !startswith(p, "user."))
                        continue;

                r = fgetxattr_malloc(fdf, p, &value);
                if (r == -ENODATA)
                        continue; /* gone by now */
                if (r < 0)
                        return r;

                if (fsetxattr(fdt, p, value, r, 0) < 0)
                        ret = -errno;
        }

        return ret;
}
#endif // 0
