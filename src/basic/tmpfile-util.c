/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <sys/mman.h>

#include "alloc-util.h"
#include "fd-util.h"
#include "fileio.h"
#include "fs-util.h"
//#include "hexdecoct.h"
#include "macro.h"
//#include "memfd-util.h"
#include "missing_fcntl.h"
//#include "missing_syscall.h"
#include "path-util.h"
//#include "process-util.h"
#include "random-util.h"
#include "stat-util.h"
#include "stdio-util.h"
#include "string-util.h"
#include "tmpfile-util.h"
#include "umask-util.h"

static int fopen_temporary_internal(int dir_fd, const char *path, FILE **ret_file) {
        _cleanup_fclose_ FILE *f = NULL;
        _cleanup_close_ int fd = -EBADF;
        int r;

        assert(dir_fd >= 0 || dir_fd == AT_FDCWD);
        assert(path);

        fd = openat(dir_fd, path, O_CLOEXEC|O_NOCTTY|O_RDWR|O_CREAT|O_EXCL, 0600);
        if (fd < 0)
                return -errno;

        /* This assumes that returned FILE object is short-lived and used within the same single-threaded
         * context and never shared externally, hence locking is not necessary. */

        r = take_fdopen_unlocked(&fd, "w", &f);
        if (r < 0) {
                (void) unlinkat(dir_fd, path, 0);
                return r;
        }

        if (ret_file)
                *ret_file = TAKE_PTR(f);

        return 0;
}

int fopen_temporary_at(int dir_fd, const char *path, FILE **ret_file, char **ret_path) {
        _cleanup_free_ char *t = NULL;
        int r;

        assert(dir_fd >= 0 || dir_fd == AT_FDCWD);
        assert(path);

        r = tempfn_random(path, NULL, &t);
        if (r < 0)
                return r;

        r = fopen_temporary_internal(dir_fd, t, ret_file);
        if (r < 0)
                return r;

        if (ret_path)
                *ret_path = TAKE_PTR(t);

        return 0;
}

int fopen_temporary_child_at(int dir_fd, const char *path, FILE **ret_file, char **ret_path) {
        _cleanup_free_ char *t = NULL;
        int r;

        assert(dir_fd >= 0 || dir_fd == AT_FDCWD);

        if (!path) {
                r = tmp_dir(&path);
                if (r < 0)
                        return r;
        }

        r = tempfn_random_child(path, NULL, &t);
        if (r < 0)
                return r;

        r = fopen_temporary_internal(dir_fd, t, ret_file);
        if (r < 0)
                return r;

        if (ret_path)
                *ret_path = TAKE_PTR(t);

        return 0;
}

/* This is much like mkostemp() but is subject to umask(). */
int mkostemp_safe(char *pattern) {
        assert(pattern);
        BLOCK_WITH_UMASK(0077);
        return RET_NERRNO(mkostemp(pattern, O_CLOEXEC));
}

int fmkostemp_safe(char *pattern, const char *mode, FILE **ret_f) {
        _cleanup_close_ int fd = -EBADF;
        FILE *f;

        fd = mkostemp_safe(pattern);
        if (fd < 0)
                return fd;

        f = take_fdopen(&fd, mode);
        if (!f)
                return -errno;

        *ret_f = f;
        return 0;
}

static int tempfn_build(const char *p, const char *pre, const char *post, bool child, char **ret) {
        _cleanup_free_ char *d = NULL, *fn = NULL, *nf = NULL, *result = NULL;
        size_t len_pre, len_post, len_add;
        int r;

        assert(p);
        assert(ret);

        /*
         * Turns this:
         *         /foo/bar/waldo
         *
         * Into this :
         *         /foo/bar/waldo/.#<pre><post> (child == true)
         *         /foo/bar/.#<pre>waldo<post> (child == false)
         */

        if (pre && strchr(pre, '/'))
                return -EINVAL;

        if (post && strchr(post, '/'))
                return -EINVAL;

        len_pre = strlen_ptr(pre);
        len_post = strlen_ptr(post);
        /* NAME_MAX is counted *without* the trailing NUL byte. */
        if (len_pre > NAME_MAX - STRLEN(".#") ||
            len_post > NAME_MAX - STRLEN(".#") - len_pre)
                return -EINVAL;

        len_add = len_pre + len_post + STRLEN(".#");

        if (child) {
                d = strdup(p);
                if (!d)
                        return -ENOMEM;
        } else {
                r = path_extract_directory(p, &d);
                if (r < 0 && r != -EDESTADDRREQ) /* EDESTADDRREQ → No directory specified, just a filename */
                        return r;

                r = path_extract_filename(p, &fn);
                if (r < 0)
                        return r;

                if (strlen(fn) > NAME_MAX - len_add)
                        /* We cannot simply prepend and append strings to the filename. Let's truncate the filename. */
                        fn[NAME_MAX - len_add] = '\0';
        }

        nf = strjoin(".#", strempty(pre), strempty(fn), strempty(post));
        if (!nf)
                return -ENOMEM;

        if (d) {
                if (!path_extend(&d, nf))
                        return -ENOMEM;

                result = path_simplify(TAKE_PTR(d));
        } else
                result = TAKE_PTR(nf);

        if (!path_is_valid(result)) /* New path is not valid? (Maybe because too long?) Refuse. */
                return -EINVAL;

        *ret = TAKE_PTR(result);
        return 0;
}

int tempfn_xxxxxx(const char *p, const char *extra, char **ret) {
        /*
         * Turns this:
         *         /foo/bar/waldo
         *
         * Into this:
         *         /foo/bar/.#<extra>waldoXXXXXX
         */

        return tempfn_build(p, extra, "XXXXXX", /* child = */ false, ret);
}

#if 0 /// UNNEEDED by elogind
int tempfn_random(const char *p, const char *extra, char **ret) {
        _cleanup_free_ char *s = NULL;

        assert(p);
        assert(ret);

        /*
         * Turns this:
         *         /foo/bar/waldo
         *
         * Into this:
         *         /foo/bar/.#<extra>waldobaa2a261115984a9
         */

        if (asprintf(&s, "%016" PRIx64, random_u64()) < 0)
                return -ENOMEM;

        return tempfn_build(p, extra, s, /* child = */ false, ret);
}

int tempfn_random_child(const char *p, const char *extra, char **ret) {
        _cleanup_free_ char *s = NULL;
        int r;

        assert(ret);

        /* Turns this:
         *         /foo/bar/waldo
         * Into this:
         *         /foo/bar/waldo/.#<extra>3c2b6219aa75d7d0
         */

        if (!p) {
                r = tmp_dir(&p);
                if (r < 0)
                        return r;
        }

        if (asprintf(&s, "%016" PRIx64, random_u64()) < 0)
                return -ENOMEM;

        return tempfn_build(p, extra, s, /* child = */ true, ret);
}
#endif // 0

int open_tmpfile_unlinkable(const char *directory, int flags) {
        char *p;
        int fd, r;

        if (!directory) {
                r = tmp_dir(&directory);
                if (r < 0)
                        return r;
        } else if (isempty(directory))
                return -EINVAL;

        /* Returns an unlinked temporary file that cannot be linked into the file system anymore */

        /* Try O_TMPFILE first, if it is supported */
        fd = open(directory, flags|O_TMPFILE|O_EXCL, S_IRUSR|S_IWUSR);
        if (fd >= 0)
                return fd;

        /* Fall back to unguessable name + unlinking */
        p = strjoina(directory, "/elogind-tmp-XXXXXX");

        fd = mkostemp_safe(p);
        if (fd < 0)
                return fd;

        (void) unlink(p);

        return fd;
}

#if 0 /// UNNEEDED by elogind
int open_tmpfile_linkable_at(int dir_fd, const char *target, int flags, char **ret_path) {
        _cleanup_free_ char *tmp = NULL;
        int r, fd;

        assert(target);
        assert(ret_path);

        /* Don't allow O_EXCL, as that has a special meaning for O_TMPFILE */
        assert((flags & O_EXCL) == 0);

        /* Creates a temporary file, that shall be renamed to "target" later. If possible, this uses O_TMPFILE – in
         * which case "ret_path" will be returned as NULL. If not possible the temporary path name used is returned in
         * "ret_path". Use link_tmpfile() below to rename the result after writing the file in full. */

        fd = open_parent_at(dir_fd, target, O_TMPFILE|flags, 0640);
        if (fd >= 0) {
                *ret_path = NULL;
                return fd;
        }

        log_debug_errno(fd, "Failed to use O_TMPFILE for %s: %m", target);

        r = tempfn_random(target, NULL, &tmp);
        if (r < 0)
                return r;

        fd = openat(dir_fd, tmp, O_CREAT|O_EXCL|O_NOFOLLOW|O_NOCTTY|flags, 0640);
        if (fd < 0)
                return -errno;

        *ret_path = TAKE_PTR(tmp);

        return fd;
}

int fopen_tmpfile_linkable(const char *target, int flags, char **ret_path, FILE **ret_file) {
        _cleanup_free_ char *path = NULL;
        _cleanup_fclose_ FILE *f = NULL;
        _cleanup_close_ int fd = -EBADF;

        assert(target);
        assert(ret_file);
        assert(ret_path);

        fd = open_tmpfile_linkable(target, flags, &path);
        if (fd < 0)
                return fd;

        f = take_fdopen(&fd, "w");
        if (!f)
                return -ENOMEM;

        *ret_path = TAKE_PTR(path);
        *ret_file = TAKE_PTR(f);
        return 0;
}

static int link_fd(int fd, int newdirfd, const char *newpath) {
        int r;

        assert(fd >= 0);
        assert(newdirfd >= 0 || newdirfd == AT_FDCWD);
        assert(newpath);

        /* Try symlinking via /proc/fd/ first. */
        r = RET_NERRNO(linkat(AT_FDCWD, FORMAT_PROC_FD_PATH(fd), newdirfd, newpath, AT_SYMLINK_FOLLOW));
        if (r != -ENOENT)
                return r;

        /* Fall back to symlinking via AT_EMPTY_PATH as fallback (this requires CAP_DAC_READ_SEARCH and a
         * more recent kernel, but does not require /proc/ mounted) */
        if (proc_mounted() != 0)
                return r;

        return RET_NERRNO(linkat(fd, "", newdirfd, newpath, AT_EMPTY_PATH));
}

int link_tmpfile_at(int fd, int dir_fd, const char *path, const char *target, bool replace) {
        _cleanup_free_ char *tmp = NULL;
        int r;

        assert(fd >= 0);
        assert(dir_fd >= 0 || dir_fd == AT_FDCWD);
        assert(target);

        /* Moves a temporary file created with open_tmpfile() above into its final place. If "path" is NULL
         * an fd created with O_TMPFILE is assumed, and linkat() is used. Otherwise it is assumed O_TMPFILE
         * is not supported on the directory, and renameat2() is used instead. */

        if (path) {
                if (replace)
                        return RET_NERRNO(renameat(dir_fd, path, dir_fd, target));

                return rename_noreplace(dir_fd, path, dir_fd, target);
        }

        r = link_fd(fd, dir_fd, target);
        if (r != -EEXIST || !replace)
                return r;

        /* So the target already exists and we were asked to replace it. That sucks a bit, since the kernel's
         * linkat() logic does not allow that. We work-around this by linking the file to a random name
         * first, and then renaming that to the final name. This reintroduces the race O_TMPFILE kinda is
         * trying to fix, but at least the vulnerability window (i.e. where the file is linked into the file
         * system under a temporary name) is very short. */

        r = tempfn_random(target, NULL, &tmp);
        if (r < 0)
                return r;

        if (link_fd(fd, dir_fd, tmp) < 0)
                return -EEXIST; /* propagate original error */

        r = RET_NERRNO(renameat(dir_fd, tmp, dir_fd, target));
        if (r < 0) {
                (void) unlinkat(dir_fd, tmp, 0);
                return r;
        }

        return 0;
}

int flink_tmpfile(FILE *f, const char *path, const char *target, bool replace) {
        int fd, r;

        assert(f);
        assert(target);

        fd = fileno(f);
        if (fd < 0) /* Not all FILE* objects encapsulate fds */
                return -EBADF;

        r = fflush_sync_and_check(f);
        if (r < 0)
                return r;

        return link_tmpfile(fd, path, target, replace);
}
#endif // 0

int mkdtemp_malloc(const char *template, char **ret) {
        _cleanup_free_ char *p = NULL;
        int r;

        assert(ret);

        if (template)
                p = strdup(template);
        else {
                const char *tmp;

                r = tmp_dir(&tmp);
                if (r < 0)
                        return r;

                p = path_join(tmp, "XXXXXX");
        }
        if (!p)
                return -ENOMEM;

        if (!mkdtemp(p))
                return -errno;

        *ret = TAKE_PTR(p);
        return 0;
}

int mkdtemp_open(const char *template, int flags, char **ret) {
        _cleanup_free_ char *p = NULL;
        int fd, r;

        r = mkdtemp_malloc(template, &p);
        if (r < 0)
                return r;

        fd = RET_NERRNO(open(p, O_DIRECTORY|O_CLOEXEC|flags));
        if (fd < 0) {
                (void) rmdir(p);
                return fd;
        }

        if (ret)
                *ret = TAKE_PTR(p);

        return fd;
}
