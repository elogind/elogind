/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <sys/xattr.h>
#include <unistd.h>

#include "alloc-util.h"
#include "chase-symlinks.h"
#include "copy.h"
#include "fd-util.h"
#include "fileio.h"
#include "fs-util.h"
#include "hexdecoct.h"
#include "io-util.h"
#include "log.h"
#include "macro.h"
#include "mkdir.h"
#include "path-util.h"
#include "random-util.h"
#include "rm-rf.h"
#include "string-util.h"
#include "strv.h"
#include "tests.h"
#include "tmpfile-util.h"
#include "user-util.h"
#include "util.h"
//#include "xattr-util.h"

#if 0 /// UNNEEDED by elogind
TEST(copy_file) {
        _cleanup_free_ char *buf = NULL;
        char fn[] = "/tmp/test-copy_file.XXXXXX";
        char fn_copy[] = "/tmp/test-copy_file.XXXXXX";
        size_t sz = 0;
        int fd;

        fd = mkostemp_safe(fn);
        assert_se(fd >= 0);
        close(fd);

        fd = mkostemp_safe(fn_copy);
        assert_se(fd >= 0);
        close(fd);

        assert_se(write_string_file(fn, "foo bar bar bar foo", WRITE_STRING_FILE_CREATE) == 0);

        assert_se(copy_file(fn, fn_copy, 0, 0644, 0, 0, COPY_REFLINK) == 0);

        assert_se(read_full_file(fn_copy, &buf, &sz) == 0);
        assert_se(streq(buf, "foo bar bar bar foo\n"));
        assert_se(sz == 20);

        unlink(fn);
        unlink(fn_copy);
}

static bool read_file_and_streq(const char* filepath, const char* expected_contents) {
        _cleanup_free_ char *buf = NULL;

        assert_se(read_full_file(filepath, &buf, NULL) == 0);
        return streq(buf, expected_contents);
}

TEST(copy_tree_replace_file) {
        _cleanup_free_ char *src = NULL, *dst = NULL;

        assert_se(tempfn_random("/tmp/test-copy_file.XXXXXX", NULL, &src) >= 0);
        assert_se(tempfn_random("/tmp/test-copy_file.XXXXXX", NULL, &dst) >= 0);

        assert_se(write_string_file(src, "bar bar", WRITE_STRING_FILE_CREATE) == 0);
        assert_se(write_string_file(dst, "foo foo foo", WRITE_STRING_FILE_CREATE) == 0);

        /* The file exists- now overwrite original contents, and test the COPY_REPLACE flag. */

        assert_se(copy_tree(src, dst, UID_INVALID, GID_INVALID, COPY_REFLINK) == -EEXIST);

        assert_se(read_file_and_streq(dst, "foo foo foo\n"));

        assert_se(copy_tree(src, dst, UID_INVALID, GID_INVALID, COPY_REFLINK|COPY_REPLACE) == 0);

        assert_se(read_file_and_streq(dst, "bar bar\n"));
}

TEST(copy_tree_replace_dirs) {
        _cleanup_free_ char *src_path1 = NULL, *src_path2 = NULL, *dst_path1 = NULL, *dst_path2 = NULL;
        _cleanup_(rm_rf_physical_and_freep) char *src_directory = NULL, *dst_directory = NULL;
        const char *file1 = "foo_file", *file2 = "bar_file";

        /* Create the random source/destination directories */
        assert_se(mkdtemp_malloc("/tmp/dirXXXXXX", &src_directory) >= 0);
        assert_se(mkdtemp_malloc("/tmp/dirXXXXXX", &dst_directory) >= 0);

        /* Construct the source/destination filepaths (should have different dir name, but same file names within) */
        assert_se(src_path1 = path_join(src_directory, file1));
        assert_se(src_path2 = path_join(src_directory, file2));
        assert_se(dst_path1 = path_join(dst_directory, file1));
        assert_se(dst_path2 = path_join(dst_directory, file2));

        /* Populate some data to differentiate the files. */
        assert_se(write_string_file(src_path1, "src file 1", WRITE_STRING_FILE_CREATE) == 0);
        assert_se(write_string_file(src_path2, "src file 2", WRITE_STRING_FILE_CREATE) == 0);

        assert_se(write_string_file(dst_path1, "dest file 1", WRITE_STRING_FILE_CREATE) == 0);
        assert_se(write_string_file(dst_path2, "dest file 2", WRITE_STRING_FILE_CREATE) == 0);

        /* Copying without COPY_REPLACE should fail because the destination file already exists. */
        assert_se(copy_tree(src_directory, dst_directory, UID_INVALID, GID_INVALID, COPY_REFLINK) == -EEXIST);

        {
                assert_se(read_file_and_streq(src_path1,  "src file 1\n"));
                assert_se(read_file_and_streq(src_path2,  "src file 2\n"));
                assert_se(read_file_and_streq(dst_path1,  "dest file 1\n"));
                assert_se(read_file_and_streq(dst_path2,  "dest file 2\n"));
        }

        assert_se(copy_tree(src_directory, dst_directory, UID_INVALID, GID_INVALID, COPY_REFLINK|COPY_REPLACE|COPY_MERGE) == 0);

        {
                assert_se(read_file_and_streq(src_path1,  "src file 1\n"));
                assert_se(read_file_and_streq(src_path2,  "src file 2\n"));
                assert_se(read_file_and_streq(dst_path1,  "src file 1\n"));
                assert_se(read_file_and_streq(dst_path2,  "src file 2\n"));
        }
}

TEST(copy_file_fd) {
        char in_fn[] = "/tmp/test-copy-file-fd-XXXXXX";
        char out_fn[] = "/tmp/test-copy-file-fd-XXXXXX";
        _cleanup_close_ int in_fd = -1, out_fd = -1;
        const char *text = "boohoo\nfoo\n\tbar\n";
        char buf[64] = {};

        in_fd = mkostemp_safe(in_fn);
        assert_se(in_fd >= 0);
        out_fd = mkostemp_safe(out_fn);
        assert_se(out_fd >= 0);

        assert_se(write_string_file(in_fn, text, WRITE_STRING_FILE_CREATE) == 0);
        assert_se(copy_file_fd("/a/file/which/does/not/exist/i/guess", out_fd, COPY_REFLINK) < 0);
        assert_se(copy_file_fd(in_fn, out_fd, COPY_REFLINK) >= 0);
        assert_se(lseek(out_fd, SEEK_SET, 0) == 0);

        assert_se(read(out_fd, buf, sizeof buf) == (ssize_t) strlen(text));
        assert_se(streq(buf, text));

        unlink(in_fn);
        unlink(out_fn);
}

TEST(copy_tree) {
        char original_dir[] = "/tmp/test-copy_tree/";
        char copy_dir[] = "/tmp/test-copy_tree-copy/";
        char **files = STRV_MAKE("file", "dir1/file", "dir1/dir2/file", "dir1/dir2/dir3/dir4/dir5/file");
        char **symlinks = STRV_MAKE("link", "file",
                                    "link2", "dir1/file");
        char **hardlinks = STRV_MAKE("hlink", "file",
                                     "hlink2", "dir1/file");
        const char *unixsockp;
        struct stat st;
        int xattr_worked = -1; /* xattr support is optional in temporary directories, hence use it if we can,
                                * but don't fail if we can't */

        (void) rm_rf(copy_dir, REMOVE_ROOT|REMOVE_PHYSICAL);
        (void) rm_rf(original_dir, REMOVE_ROOT|REMOVE_PHYSICAL);

        STRV_FOREACH(p, files) {
                _cleanup_free_ char *f = NULL, *c = NULL;
                int k;

                assert_se(f = path_join(original_dir, *p));

                assert_se(write_string_file(f, "file", WRITE_STRING_FILE_CREATE|WRITE_STRING_FILE_MKDIR_0755) == 0);

                assert_se(base64mem(*p, strlen(*p), &c) >= 0);

                k = setxattr(f, "user.testxattr", c, strlen(c), 0);
                assert_se(xattr_worked < 0 || ((k >= 0) == !!xattr_worked));
                xattr_worked = k >= 0;
        }

        STRV_FOREACH_PAIR(ll, p, symlinks) {
                _cleanup_free_ char *f = NULL, *l = NULL;

                assert_se(f = path_join(original_dir, *p));
                assert_se(l = path_join(original_dir, *ll));

                assert_se(mkdir_parents(l, 0755) >= 0);
                assert_se(symlink(f, l) == 0);
        }

        STRV_FOREACH_PAIR(ll, p, hardlinks) {
                _cleanup_free_ char *f = NULL, *l = NULL;

                assert_se(f = path_join(original_dir, *p));
                assert_se(l = path_join(original_dir, *ll));

                assert_se(mkdir_parents(l, 0755) >= 0);
                assert_se(link(f, l) == 0);
        }

        unixsockp = strjoina(original_dir, "unixsock");
        assert_se(mknod(unixsockp, S_IFSOCK|0644, 0) >= 0);

        assert_se(copy_tree(original_dir, copy_dir, UID_INVALID, GID_INVALID, COPY_REFLINK|COPY_MERGE|COPY_HARDLINKS) == 0);

        STRV_FOREACH(p, files) {
                _cleanup_free_ char *buf = NULL, *f = NULL, *c = NULL;
                size_t sz;
                int k;

                assert_se(f = path_join(copy_dir, *p));

                assert_se(access(f, F_OK) == 0);
                assert_se(read_full_file(f, &buf, &sz) == 0);
                assert_se(streq(buf, "file\n"));

                k = lgetxattr_malloc(f, "user.testxattr", &c);
                assert_se(xattr_worked < 0 || ((k >= 0) == !!xattr_worked));

                if (k >= 0) {
                        _cleanup_free_ char *d = NULL;

                        assert_se(base64mem(*p, strlen(*p), &d) >= 0);
                        assert_se(streq(d, c));
                }
        }

        STRV_FOREACH_PAIR(ll, p, symlinks) {
                _cleanup_free_ char *target = NULL, *f = NULL, *l = NULL;

                assert_se(f = strjoin(original_dir, *p));
                assert_se(l = strjoin(copy_dir, *ll));

                assert_se(chase_symlinks(l, NULL, 0, &target, NULL) == 1);
                assert_se(path_equal(f, target));
        }

        STRV_FOREACH_PAIR(ll, p, hardlinks) {
                _cleanup_free_ char *f = NULL, *l = NULL;
                struct stat a, b;

                assert_se(f = strjoin(copy_dir, *p));
                assert_se(l = strjoin(copy_dir, *ll));

                assert_se(lstat(f, &a) >= 0);
                assert_se(lstat(l, &b) >= 0);

                assert_se(a.st_ino == b.st_ino);
                assert_se(a.st_dev == b.st_dev);
        }

        unixsockp = strjoina(copy_dir, "unixsock");
        assert_se(stat(unixsockp, &st) >= 0);
        assert_se(S_ISSOCK(st.st_mode));

        assert_se(copy_tree(original_dir, copy_dir, UID_INVALID, GID_INVALID, COPY_REFLINK) < 0);
        assert_se(copy_tree("/tmp/inexistent/foo/bar/fsdoi", copy_dir, UID_INVALID, GID_INVALID, COPY_REFLINK) < 0);

        (void) rm_rf(copy_dir, REMOVE_ROOT|REMOVE_PHYSICAL);
        (void) rm_rf(original_dir, REMOVE_ROOT|REMOVE_PHYSICAL);
}
#endif // 0

TEST(copy_bytes) {
        _cleanup_close_pair_ int pipefd[2] = {-1, -1};
        _cleanup_close_ int infd = -1;
        int r, r2;
        char buf[1024], buf2[1024];

        infd = open("/usr/lib/os-release", O_RDONLY|O_CLOEXEC);
        if (infd < 0)
                infd = open("/etc/os-release", O_RDONLY|O_CLOEXEC);
        assert_se(infd >= 0);

        assert_se(pipe2(pipefd, O_CLOEXEC) == 0);

        r = copy_bytes(infd, pipefd[1], UINT64_MAX, 0);
        assert_se(r == 0);

        r = read(pipefd[0], buf, sizeof(buf));
        assert_se(r >= 0);

        assert_se(lseek(infd, 0, SEEK_SET) == 0);
        r2 = read(infd, buf2, sizeof(buf2));
        assert_se(r == r2);

        assert_se(strneq(buf, buf2, r));

        /* test copy_bytes with invalid descriptors */
        r = copy_bytes(pipefd[0], pipefd[0], 1, 0);
        assert_se(r == -EBADF);

        r = copy_bytes(pipefd[1], pipefd[1], 1, 0);
        assert_se(r == -EBADF);

        r = copy_bytes(pipefd[1], infd, 1, 0);
        assert_se(r == -EBADF);
}

static void test_copy_bytes_regular_file_one(const char *src, bool try_reflink, uint64_t max_bytes) {
        char fn2[] = "/tmp/test-copy-file-XXXXXX";
        char fn3[] = "/tmp/test-copy-file-XXXXXX";
        _cleanup_close_ int fd = -1, fd2 = -1, fd3 = -1;
        int r;
        struct stat buf, buf2, buf3;

        log_info("%s try_reflink=%s max_bytes=%" PRIu64, __func__, yes_no(try_reflink), max_bytes);

        fd = open(src, O_RDONLY | O_CLOEXEC | O_NOCTTY);
        assert_se(fd >= 0);

        fd2 = mkostemp_safe(fn2);
        assert_se(fd2 >= 0);

        fd3 = mkostemp_safe(fn3);
        assert_se(fd3 >= 0);

        r = copy_bytes(fd, fd2, max_bytes, try_reflink ? COPY_REFLINK : 0);
        if (max_bytes == UINT64_MAX)
                assert_se(r == 0);
        else
                assert_se(IN_SET(r, 0, 1));

        assert_se(fstat(fd, &buf) == 0);
        assert_se(fstat(fd2, &buf2) == 0);
        assert_se((uint64_t) buf2.st_size == MIN((uint64_t) buf.st_size, max_bytes));

        if (max_bytes < UINT64_MAX)
                /* Make sure the file is now higher than max_bytes */
                assert_se(ftruncate(fd2, max_bytes + 1) == 0);

        assert_se(lseek(fd2, 0, SEEK_SET) == 0);

        r = copy_bytes(fd2, fd3, max_bytes, try_reflink ? COPY_REFLINK : 0);
        if (max_bytes == UINT64_MAX)
                assert_se(r == 0);
        else
                /* We cannot distinguish between the input being exactly max_bytes
                 * or longer than max_bytes (without trying to read one more byte,
                 * or calling stat, or FION_READ, etc, and we don't want to do any
                 * of that). So we expect "truncation" since we know that file we
                 * are copying is exactly max_bytes bytes. */
                assert_se(r == 1);

        assert_se(fstat(fd3, &buf3) == 0);

        if (max_bytes == UINT64_MAX)
                assert_se(buf3.st_size == buf2.st_size);
        else
                assert_se((uint64_t) buf3.st_size == max_bytes);

        unlink(fn2);
        unlink(fn3);
}

TEST(copy_bytes_regular_file) {
        test_copy_bytes_regular_file_one(saved_argv[0], false, UINT64_MAX);
        test_copy_bytes_regular_file_one(saved_argv[0], true, UINT64_MAX);
        test_copy_bytes_regular_file_one(saved_argv[0], false, 1000); /* smaller than copy buffer size */
        test_copy_bytes_regular_file_one(saved_argv[0], true, 1000);
        test_copy_bytes_regular_file_one(saved_argv[0], false, 32000); /* larger than copy buffer size */
        test_copy_bytes_regular_file_one(saved_argv[0], true, 32000);
}

#if 0 /// UNNEEDED by elogind
TEST(copy_atomic) {
        _cleanup_(rm_rf_physical_and_freep) char *p = NULL;
        const char *q;
        int r;

        assert_se(mkdtemp_malloc(NULL, &p) >= 0);

        q = strjoina(p, "/fstab");

        r = copy_file_atomic("/etc/fstab", q, 0644, 0, 0, COPY_REFLINK);
        if (r == -ENOENT || ERRNO_IS_PRIVILEGE(r))
                return;

        assert_se(copy_file_atomic("/etc/fstab", q, 0644, 0, 0, COPY_REFLINK) == -EEXIST);

        assert_se(copy_file_atomic("/etc/fstab", q, 0644, 0, 0, COPY_REPLACE) >= 0);
}

TEST(copy_proc) {
        _cleanup_(rm_rf_physical_and_freep) char *p = NULL;
        _cleanup_free_ char *f = NULL, *a = NULL, *b = NULL;

        /* Check if copying data from /proc/ works correctly, i.e. let's see if https://lwn.net/Articles/846403/ is a problem for us */

        assert_se(mkdtemp_malloc(NULL, &p) >= 0);
        assert_se(f = path_join(p, "version"));
        assert_se(copy_file("/proc/version", f, 0, MODE_INVALID, 0, 0, 0) >= 0);

        assert_se(read_one_line_file("/proc/version", &a) >= 0);
        assert_se(read_one_line_file(f, &b) >= 0);
        assert_se(streq(a, b));
        assert_se(!isempty(a));
}
#endif // 0

TEST_RET(copy_holes) {
        char fn[] = "/var/tmp/test-copy-hole-fd-XXXXXX";
        char fn_copy[] = "/var/tmp/test-copy-hole-fd-XXXXXX";
        struct stat stat;
        off_t blksz;
        int r, fd, fd_copy;
        char *buf;

        fd = mkostemp_safe(fn);
        assert_se(fd >= 0);

        fd_copy = mkostemp_safe(fn_copy);
        assert_se(fd_copy >= 0);

        r = RET_NERRNO(fallocate(fd, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE, 0, 1));
        if (ERRNO_IS_NOT_SUPPORTED(r))
                return log_tests_skipped("Filesystem doesn't support hole punching");
        assert_se(r >= 0);

        assert_se(fstat(fd, &stat) >= 0);
        blksz = stat.st_blksize;
        buf = alloca_safe(blksz);
        memset(buf, 1, blksz);

        /* We need to make sure to create hole in multiples of the block size, otherwise filesystems (btrfs)
         * might silently truncate/extend the holes. */

        assert_se(lseek(fd, blksz, SEEK_CUR) >= 0);
        assert_se(write(fd, buf, blksz) >= 0);
        assert_se(lseek(fd, 0, SEEK_END) == 2 * blksz);
        /* Only ftruncate() can create holes at the end of a file. */
        assert_se(ftruncate(fd, 3 * blksz) >= 0);
        assert_se(lseek(fd, 0, SEEK_SET) >= 0);

        assert_se(copy_bytes(fd, fd_copy, UINT64_MAX, COPY_HOLES) >= 0);

        /* Test that the hole starts at the beginning of the file. */
        assert_se(lseek(fd_copy, 0, SEEK_HOLE) == 0);
        /* Test that the hole has the expected size. */
        assert_se(lseek(fd_copy, 0, SEEK_DATA) == blksz);
        assert_se(lseek(fd_copy, blksz, SEEK_HOLE) == 2 * blksz);
        assert_se(lseek(fd_copy, 2 * blksz, SEEK_DATA) < 0 && errno == ENXIO);

        /* Test that the copied file has the correct size. */
        assert_se(fstat(fd_copy, &stat) >= 0);
        assert_se(stat.st_size == 3 * blksz);

        close(fd);
        close(fd_copy);

        unlink(fn);
        unlink(fn_copy);

        return 0;
}

TEST_RET(copy_holes_with_gaps) {
        _cleanup_(rm_rf_physical_and_freep) char *t = NULL;
        _cleanup_close_ int tfd = -EBADF, fd = -EBADF, fd_copy = -EBADF;
        struct stat st;
        off_t blksz;
        char *buf;
        int r;

        assert_se(mkdtemp_malloc(NULL, &t) >= 0);
        assert_se((tfd = open(t, O_DIRECTORY | O_CLOEXEC)) >= 0);
        assert_se((fd = openat(tfd, "src", O_CREAT | O_RDWR, 0600)) >= 0);
        assert_se((fd_copy = openat(tfd, "dst", O_CREAT | O_WRONLY, 0600)) >= 0);

        assert_se(fstat(fd, &st) >= 0);
        blksz = st.st_blksize;
        buf = alloca_safe(blksz);
        memset(buf, 1, blksz);

        /* Create a file with:
         *  - hole of 1 block
         *  - data of 2 block
         *  - hole of 2 blocks
         *  - data of 1 block
         *
         * Since sparse files are based on blocks and not bytes, we need to make
         * sure that the holes are aligned to the block size.
         */

        r = RET_NERRNO(fallocate(fd, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE, 0, blksz));
        if (ERRNO_IS_NOT_SUPPORTED(r))
                return log_tests_skipped("Filesystem doesn't support hole punching");

        assert_se(lseek(fd, blksz, SEEK_CUR) >= 0);
        assert_se(loop_write(fd, buf, blksz, 0) >= 0);
        assert_se(loop_write(fd, buf, blksz, 0) >= 0);
        assert_se(lseek(fd, 2 * blksz, SEEK_CUR) >= 0);
        assert_se(loop_write(fd, buf, blksz, 0) >= 0);
        assert_se(lseek(fd, 0, SEEK_SET) >= 0);
        assert_se(fsync(fd) >= 0);

        /* Copy to the start of the second hole */
        assert_se(copy_bytes(fd, fd_copy, 3 * blksz, COPY_HOLES) >= 0);
        assert_se(fstat(fd_copy, &st) >= 0);
        assert_se(st.st_size == 3 * blksz);

        /* Copy to the middle of the second hole */
        assert_se(lseek(fd, 0, SEEK_SET) >= 0);
        assert_se(lseek(fd_copy, 0, SEEK_SET) >= 0);
        assert_se(ftruncate(fd_copy, 0) >= 0);
        assert_se(copy_bytes(fd, fd_copy, 4 * blksz, COPY_HOLES) >= 0);
        assert_se(fstat(fd_copy, &st) >= 0);
        assert_se(st.st_size == 4 * blksz);

        /* Copy to the end of the second hole */
        assert_se(lseek(fd, 0, SEEK_SET) >= 0);
        assert_se(lseek(fd_copy, 0, SEEK_SET) >= 0);
        assert_se(ftruncate(fd_copy, 0) >= 0);
        assert_se(copy_bytes(fd, fd_copy, 5 * blksz, COPY_HOLES) >= 0);
        assert_se(fstat(fd_copy, &st) >= 0);
        assert_se(st.st_size == 5 * blksz);

        /* Copy everything */
        assert_se(lseek(fd, 0, SEEK_SET) >= 0);
        assert_se(lseek(fd_copy, 0, SEEK_SET) >= 0);
        assert_se(ftruncate(fd_copy, 0) >= 0);
        assert_se(copy_bytes(fd, fd_copy, UINT64_MAX, COPY_HOLES) >= 0);
        assert_se(fstat(fd_copy, &st) >= 0);
        assert_se(st.st_size == 6 * blksz);

        return 0;
}

DEFINE_TEST_MAIN(LOG_DEBUG);
