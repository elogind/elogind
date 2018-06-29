/* SPDX-License-Identifier: LGPL-2.1+ */
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

#include <unistd.h>

#include "alloc-util.h"
#include "fd-util.h"
//#include "fd-util.h"
#include "fileio.h"
#include "fs-util.h"
#include "id128-util.h"
#include "macro.h"
#include "mkdir.h"
#include "path-util.h"
#include "rm-rf.h"
#include "stdio-util.h"
#include "string-util.h"
#include "strv.h"
#include "user-util.h"
#include "util.h"

static void test_chase_symlinks(void) {
        _cleanup_free_ char *result = NULL;
        char temp[] = "/tmp/test-chase.XXXXXX";
        const char *top, *p, *pslash, *q, *qslash;
        int r, pfd;

        assert_se(mkdtemp(temp));

        top = strjoina(temp, "/top");
        assert_se(mkdir(top, 0700) >= 0);

        p = strjoina(top, "/dot");
        assert_se(symlink(".", p) >= 0);

        p = strjoina(top, "/dotdot");
        assert_se(symlink("..", p) >= 0);

        p = strjoina(top, "/dotdota");
        assert_se(symlink("../a", p) >= 0);

        p = strjoina(temp, "/a");
        assert_se(symlink("b", p) >= 0);

        p = strjoina(temp, "/b");
        assert_se(symlink("/usr", p) >= 0);

        p = strjoina(temp, "/start");
        assert_se(symlink("top/dot/dotdota", p) >= 0);

        /* Paths that use symlinks underneath the "root" */

        r = chase_symlinks(p, NULL, 0, &result);
        assert_se(r > 0);
        assert_se(path_equal(result, "/usr"));
        result = mfree(result);

        pslash = strjoina(p, "/");
        r = chase_symlinks(pslash, NULL, 0, &result);
        assert_se(r > 0);
        assert_se(path_equal(result, "/usr/"));
        result = mfree(result);

        r = chase_symlinks(p, temp, 0, &result);
        assert_se(r == -ENOENT);

        r = chase_symlinks(pslash, temp, 0, &result);
        assert_se(r == -ENOENT);

        q = strjoina(temp, "/usr");

        r = chase_symlinks(p, temp, CHASE_NONEXISTENT, &result);
        assert_se(r == 0);
        assert_se(path_equal(result, q));
        result = mfree(result);

        qslash = strjoina(q, "/");

        r = chase_symlinks(pslash, temp, CHASE_NONEXISTENT, &result);
        assert_se(r == 0);
        assert_se(path_equal(result, qslash));
        result = mfree(result);

        assert_se(mkdir(q, 0700) >= 0);

        r = chase_symlinks(p, temp, 0, &result);
        assert_se(r > 0);
        assert_se(path_equal(result, q));
        result = mfree(result);

        r = chase_symlinks(pslash, temp, 0, &result);
        assert_se(r > 0);
        assert_se(path_equal(result, qslash));
        result = mfree(result);

        p = strjoina(temp, "/slash");
        assert_se(symlink("/", p) >= 0);

        r = chase_symlinks(p, NULL, 0, &result);
        assert_se(r > 0);
        assert_se(path_equal(result, "/"));
        result = mfree(result);

        r = chase_symlinks(p, temp, 0, &result);
        assert_se(r > 0);
        assert_se(path_equal(result, temp));
        result = mfree(result);

        /* Paths that would "escape" outside of the "root" */

        p = strjoina(temp, "/6dots");
        assert_se(symlink("../../..", p) >= 0);

        r = chase_symlinks(p, temp, 0, &result);
        assert_se(r > 0 && path_equal(result, temp));
        result = mfree(result);

        p = strjoina(temp, "/6dotsusr");
        assert_se(symlink("../../../usr", p) >= 0);

        r = chase_symlinks(p, temp, 0, &result);
        assert_se(r > 0 && path_equal(result, q));
        result = mfree(result);

        p = strjoina(temp, "/top/8dotsusr");
        assert_se(symlink("../../../../usr", p) >= 0);

        r = chase_symlinks(p, temp, 0, &result);
        assert_se(r > 0 && path_equal(result, q));
        result = mfree(result);

        /* Paths that contain repeated slashes */

        p = strjoina(temp, "/slashslash");
        assert_se(symlink("///usr///", p) >= 0);

        r = chase_symlinks(p, NULL, 0, &result);
        assert_se(r > 0);
        assert_se(path_equal(result, "/usr"));
        result = mfree(result);

        r = chase_symlinks(p, temp, 0, &result);
        assert_se(r > 0);
        assert_se(path_equal(result, q));
        result = mfree(result);

        /* Paths using . */

        r = chase_symlinks("/etc/./.././", NULL, 0, &result);
        assert_se(r > 0);
        assert_se(path_equal(result, "/"));
        result = mfree(result);

        r = chase_symlinks("/etc/./.././", "/etc", 0, &result);
        assert_se(r > 0 && path_equal(result, "/etc"));
        result = mfree(result);

        r = chase_symlinks("/../.././//../../etc", NULL, 0, &result);
        assert_se(r > 0);
        assert_se(streq(result, "/etc"));
        result = mfree(result);

        r = chase_symlinks("/../.././//../../test-chase.fsldajfl", NULL, CHASE_NONEXISTENT, &result);
        assert_se(r == 0);
        assert_se(streq(result, "/test-chase.fsldajfl"));
        result = mfree(result);

        r = chase_symlinks("/../.././//../../etc", "/", CHASE_PREFIX_ROOT, &result);
        assert_se(r > 0);
        assert_se(streq(result, "/etc"));
        result = mfree(result);

        r = chase_symlinks("/../.././//../../test-chase.fsldajfl", "/", CHASE_PREFIX_ROOT|CHASE_NONEXISTENT, &result);
        assert_se(r == 0);
        assert_se(streq(result, "/test-chase.fsldajfl"));
        result = mfree(result);

        r = chase_symlinks("/etc/machine-id/foo", NULL, 0, &result);
        assert_se(r == -ENOTDIR);
        result = mfree(result);

        /* Path that loops back to self */

        p = strjoina(temp, "/recursive-symlink");
        assert_se(symlink("recursive-symlink", p) >= 0);
        r = chase_symlinks(p, NULL, 0, &result);
        assert_se(r == -ELOOP);

        /* Path which doesn't exist */

        p = strjoina(temp, "/idontexist");
        r = chase_symlinks(p, NULL, 0, &result);
        assert_se(r == -ENOENT);

        r = chase_symlinks(p, NULL, CHASE_NONEXISTENT, &result);
        assert_se(r == 0);
        assert_se(path_equal(result, p));
        result = mfree(result);

        p = strjoina(temp, "/idontexist/meneither");
        r = chase_symlinks(p, NULL, 0, &result);
        assert_se(r == -ENOENT);

        r = chase_symlinks(p, NULL, CHASE_NONEXISTENT, &result);
        assert_se(r == 0);
        assert_se(path_equal(result, p));
        result = mfree(result);

        /* Path which doesn't exist, but contains weird stuff */

        p = strjoina(temp, "/idontexist/..");
        r = chase_symlinks(p, NULL, 0, &result);
        assert_se(r == -ENOENT);

        r = chase_symlinks(p, NULL, CHASE_NONEXISTENT, &result);
        assert_se(r == -ENOENT);

        p = strjoina(temp, "/target");
        q = strjoina(temp, "/top");
        assert_se(symlink(q, p) >= 0);
        p = strjoina(temp, "/target/idontexist");
        r = chase_symlinks(p, NULL, 0, &result);
        assert_se(r == -ENOENT);

        if (geteuid() == 0) {
                p = strjoina(temp, "/priv1");
                assert_se(mkdir(p, 0755) >= 0);

                q = strjoina(p, "/priv2");
                assert_se(mkdir(q, 0755) >= 0);

                assert_se(chase_symlinks(q, NULL, CHASE_SAFE, NULL) >= 0);

                assert_se(chown(q, UID_NOBODY, GID_NOBODY) >= 0);
                assert_se(chase_symlinks(q, NULL, CHASE_SAFE, NULL) >= 0);

                assert_se(chown(p, UID_NOBODY, GID_NOBODY) >= 0);
                assert_se(chase_symlinks(q, NULL, CHASE_SAFE, NULL) >= 0);

                assert_se(chown(q, 0, 0) >= 0);
                assert_se(chase_symlinks(q, NULL, CHASE_SAFE, NULL) == -EPERM);

                assert_se(rmdir(q) >= 0);
                assert_se(symlink("/etc/passwd", q) >= 0);
                assert_se(chase_symlinks(q, NULL, CHASE_SAFE, NULL) == -EPERM);

                assert_se(chown(p, 0, 0) >= 0);
                assert_se(chase_symlinks(q, NULL, CHASE_SAFE, NULL) >= 0);
        }

        p = strjoina(temp, "/machine-id-test");
        assert_se(symlink("/usr/../etc/./machine-id", p) >= 0);

        pfd = chase_symlinks(p, NULL, CHASE_OPEN, NULL);
        if (pfd != -ENOENT) {
                char procfs[STRLEN("/proc/self/fd/") + DECIMAL_STR_MAX(pfd) + 1];
                _cleanup_close_ int fd = -1;
                sd_id128_t a, b;

                assert_se(pfd >= 0);

                xsprintf(procfs, "/proc/self/fd/%i", pfd);

                fd = open(procfs, O_RDONLY|O_CLOEXEC);
                assert_se(fd >= 0);

                safe_close(pfd);

                assert_se(id128_read_fd(fd, ID128_PLAIN, &a) >= 0);
                assert_se(sd_id128_get_machine(&b) >= 0);
                assert_se(sd_id128_equal(a, b));
        }

        assert_se(rm_rf(temp, REMOVE_ROOT|REMOVE_PHYSICAL) >= 0);
}

static void test_unlink_noerrno(void) {
        char name[] = "/tmp/test-close_nointr.XXXXXX";
        int fd;

        fd = mkostemp_safe(name);
        assert_se(fd >= 0);
        assert_se(close_nointr(fd) >= 0);

        {
                PROTECT_ERRNO;
                errno = -42;
                assert_se(unlink_noerrno(name) >= 0);
                assert_se(errno == -42);
                assert_se(unlink_noerrno(name) < 0);
                assert_se(errno == -42);
        }
}

static void test_readlink_and_make_absolute(void) {
        char tempdir[] = "/tmp/test-readlink_and_make_absolute";
        char name[] = "/tmp/test-readlink_and_make_absolute/original";
        char name2[] = "test-readlink_and_make_absolute/original";
        char name_alias[] = "/tmp/test-readlink_and_make_absolute-alias";
        char *r = NULL;
        _cleanup_free_ char *pwd = NULL;

        assert_se(mkdir_safe(tempdir, 0755, getuid(), getgid(), 0) >= 0);
        assert_se(touch(name) >= 0);

        assert_se(symlink(name, name_alias) >= 0);
        assert_se(readlink_and_make_absolute(name_alias, &r) >= 0);
        assert_se(streq(r, name));
        free(r);
        assert_se(unlink(name_alias) >= 0);

        assert_se(safe_getcwd(&pwd) >= 0);

        assert_se(chdir(tempdir) >= 0);
        assert_se(symlink(name2, name_alias) >= 0);
        assert_se(readlink_and_make_absolute(name_alias, &r) >= 0);
        assert_se(streq(r, name));
        free(r);
        assert_se(unlink(name_alias) >= 0);

        assert_se(chdir(pwd) >= 0);

        assert_se(rm_rf(tempdir, REMOVE_ROOT|REMOVE_PHYSICAL) >= 0);
}

static void test_get_files_in_directory(void) {
        _cleanup_strv_free_ char **l = NULL, **t = NULL;

        assert_se(get_files_in_directory("/tmp", &l) >= 0);
        assert_se(get_files_in_directory(".", &t) >= 0);
        assert_se(get_files_in_directory(".", NULL) >= 0);
}

#if 0 /// UNNEEDED by elogind
static void test_var_tmp(void) {
        _cleanup_free_ char *tmpdir_backup = NULL, *temp_backup = NULL, *tmp_backup = NULL;
        const char *tmp_dir = NULL, *t;

        t = getenv("TMPDIR");
        if (t) {
                tmpdir_backup = strdup(t);
                assert_se(tmpdir_backup);
        }

        t = getenv("TEMP");
        if (t) {
                temp_backup = strdup(t);
                assert_se(temp_backup);
        }

        t = getenv("TMP");
        if (t) {
                tmp_backup = strdup(t);
                assert_se(tmp_backup);
        }

        assert_se(unsetenv("TMPDIR") >= 0);
        assert_se(unsetenv("TEMP") >= 0);
        assert_se(unsetenv("TMP") >= 0);

        assert_se(var_tmp_dir(&tmp_dir) >= 0);
        assert_se(streq(tmp_dir, "/var/tmp"));

        assert_se(setenv("TMPDIR", "/tmp", true) >= 0);
        assert_se(streq(getenv("TMPDIR"), "/tmp"));

        assert_se(var_tmp_dir(&tmp_dir) >= 0);
        assert_se(streq(tmp_dir, "/tmp"));

        assert_se(setenv("TMPDIR", "/88_does_not_exist_88", true) >= 0);
        assert_se(streq(getenv("TMPDIR"), "/88_does_not_exist_88"));

        assert_se(var_tmp_dir(&tmp_dir) >= 0);
        assert_se(streq(tmp_dir, "/var/tmp"));

        if (tmpdir_backup)  {
                assert_se(setenv("TMPDIR", tmpdir_backup, true) >= 0);
                assert_se(streq(getenv("TMPDIR"), tmpdir_backup));
        }

        if (temp_backup)  {
                assert_se(setenv("TEMP", temp_backup, true) >= 0);
                assert_se(streq(getenv("TEMP"), temp_backup));
        }

        if (tmp_backup)  {
                assert_se(setenv("TMP", tmp_backup, true) >= 0);
                assert_se(streq(getenv("TMP"), tmp_backup));
        }
}
#endif // 0

static void test_dot_or_dot_dot(void) {
        assert_se(!dot_or_dot_dot(NULL));
        assert_se(!dot_or_dot_dot(""));
        assert_se(!dot_or_dot_dot("xxx"));
        assert_se(dot_or_dot_dot("."));
        assert_se(dot_or_dot_dot(".."));
        assert_se(!dot_or_dot_dot(".foo"));
        assert_se(!dot_or_dot_dot("..foo"));
}

#if 0 /// Uses functions that elogind does not need
static void test_access_fd(void) {
        _cleanup_(rmdir_and_freep) char *p = NULL;
        _cleanup_close_ int fd = -1;

        assert_se(mkdtemp_malloc("/tmp/access-fd.XXXXXX", &p) >= 0);

        fd = open(p, O_RDONLY|O_DIRECTORY|O_CLOEXEC);
        assert_se(fd >= 0);

        assert_se(access_fd(fd, R_OK) >= 0);
        assert_se(access_fd(fd, F_OK) >= 0);
        assert_se(access_fd(fd, W_OK) >= 0);

        assert_se(fchmod(fd, 0000) >= 0);

        assert_se(access_fd(fd, F_OK) >= 0);

        if (geteuid() == 0) {
                assert_se(access_fd(fd, R_OK) >= 0);
                assert_se(access_fd(fd, W_OK) >= 0);
        } else {
                assert_se(access_fd(fd, R_OK) == -EACCES);
                assert_se(access_fd(fd, W_OK) == -EACCES);
        }
}

static void test_touch_file(void) {
        uid_t test_uid, test_gid;
        _cleanup_(rm_rf_physical_and_freep) char *p = NULL;
        struct stat st;
        const char *a;
        usec_t test_mtime;

        test_uid = geteuid() == 0 ? 65534 : getuid();
        test_gid = geteuid() == 0 ? 65534 : getgid();

        test_mtime = usec_sub_unsigned(now(CLOCK_REALTIME), USEC_PER_WEEK);

        assert_se(mkdtemp_malloc("/dev/shm/touch-file-XXXXXX", &p) >= 0);

        a = strjoina(p, "/regular");
        assert_se(touch_file(a, false, test_mtime, test_uid, test_gid, 0640) >= 0);
        assert_se(lstat(a, &st) >= 0);
        assert_se(st.st_uid == test_uid);
        assert_se(st.st_gid == test_gid);
        assert_se(S_ISREG(st.st_mode));
        assert_se((st.st_mode & 0777) == 0640);
        assert_se(timespec_load(&st.st_mtim) == test_mtime);

        a = strjoina(p, "/dir");
        assert_se(mkdir(a, 0775) >= 0);
        assert_se(touch_file(a, false, test_mtime, test_uid, test_gid, 0640) >= 0);
        assert_se(lstat(a, &st) >= 0);
        assert_se(st.st_uid == test_uid);
        assert_se(st.st_gid == test_gid);
        assert_se(S_ISDIR(st.st_mode));
        assert_se((st.st_mode & 0777) == 0640);
        assert_se(timespec_load(&st.st_mtim) == test_mtime);

        a = strjoina(p, "/fifo");
        assert_se(mkfifo(a, 0775) >= 0);
        assert_se(touch_file(a, false, test_mtime, test_uid, test_gid, 0640) >= 0);
        assert_se(lstat(a, &st) >= 0);
        assert_se(st.st_uid == test_uid);
        assert_se(st.st_gid == test_gid);
        assert_se(S_ISFIFO(st.st_mode));
        assert_se((st.st_mode & 0777) == 0640);
        assert_se(timespec_load(&st.st_mtim) == test_mtime);

        a = strjoina(p, "/sock");
        assert_se(mknod(a, 0775 | S_IFSOCK, 0) >= 0);
        assert_se(touch_file(a, false, test_mtime, test_uid, test_gid, 0640) >= 0);
        assert_se(lstat(a, &st) >= 0);
        assert_se(st.st_uid == test_uid);
        assert_se(st.st_gid == test_gid);
        assert_se(S_ISSOCK(st.st_mode));
        assert_se((st.st_mode & 0777) == 0640);
        assert_se(timespec_load(&st.st_mtim) == test_mtime);

        if (geteuid() == 0) {
                a = strjoina(p, "/cdev");
                assert_se(mknod(a, 0775 | S_IFCHR, makedev(0, 0)) >= 0);
                assert_se(touch_file(a, false, test_mtime, test_uid, test_gid, 0640) >= 0);
                assert_se(lstat(a, &st) >= 0);
                assert_se(st.st_uid == test_uid);
                assert_se(st.st_gid == test_gid);
                assert_se(S_ISCHR(st.st_mode));
                assert_se((st.st_mode & 0777) == 0640);
                assert_se(timespec_load(&st.st_mtim) == test_mtime);

                a = strjoina(p, "/bdev");
                assert_se(mknod(a, 0775 | S_IFBLK, makedev(0, 0)) >= 0);
                assert_se(touch_file(a, false, test_mtime, test_uid, test_gid, 0640) >= 0);
                assert_se(lstat(a, &st) >= 0);
                assert_se(st.st_uid == test_uid);
                assert_se(st.st_gid == test_gid);
                assert_se(S_ISBLK(st.st_mode));
                assert_se((st.st_mode & 0777) == 0640);
                assert_se(timespec_load(&st.st_mtim) == test_mtime);
        }

        a = strjoina(p, "/lnk");
        assert_se(symlink("target", a) >= 0);
        assert_se(touch_file(a, false, test_mtime, test_uid, test_gid, 0640) >= 0);
        assert_se(lstat(a, &st) >= 0);
        assert_se(st.st_uid == test_uid);
        assert_se(st.st_gid == test_gid);
        assert_se(S_ISLNK(st.st_mode));
        assert_se((st.st_mode & 0777) == 0640);
        assert_se(timespec_load(&st.st_mtim) == test_mtime);
}

static void test_unlinkat_deallocate(void) {
        _cleanup_free_ char *p = NULL;
        _cleanup_close_ int fd = -1;
        struct stat st;

        assert_se(tempfn_random_child(NULL, "unlink-deallocation", &p) >= 0);

        fd = open(p, O_WRONLY|O_CLOEXEC|O_CREAT|O_EXCL, 0600);
        assert_se(fd >= 0);

        assert_se(write(fd, "hallo\n", 6) == 6);

        assert_se(fstat(fd, &st) >= 0);
        assert_se(st.st_size == 6);
        assert_se(st.st_blocks > 0);
        assert_se(st.st_nlink == 1);

        assert_se(unlinkat_deallocate(AT_FDCWD, p, 0) >= 0);

        assert_se(fstat(fd, &st) >= 0);
        assert_se(IN_SET(st.st_size, 0, 6)); /* depending on whether hole punching worked the size will be 6 (it worked) or 0 (we had to resort to truncation) */
        assert_se(st.st_blocks == 0);
        assert_se(st.st_nlink == 0);
}
#endif // 0

static void test_fsync_directory_of_file(void) {
        _cleanup_close_ int fd = -1;

        fd = open_tmpfile_unlinkable(NULL, O_RDWR);
        assert_se(fd >= 0);

        assert_se(fsync_directory_of_file(fd) >= 0);
}

int main(int argc, char *argv[]) {
        test_unlink_noerrno();
        test_get_files_in_directory();
        test_readlink_and_make_absolute();
#if 0 /// UNNEEDED by elogind
        test_var_tmp();
#endif // 0
        test_chase_symlinks();
        test_dot_or_dot_dot();
#if 0 /// Uses functions that elogind does not need
        test_access_fd();
        test_touch_file();
        test_unlinkat_deallocate();
#endif // 0
        test_fsync_directory_of_file();

        return 0;
}
