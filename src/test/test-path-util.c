/* SPDX-License-Identifier: LGPL-2.1+ */
/***
  This file is part of systemd.

  Copyright 2013 Zbigniew Jędrzejewski-Szmek
***/

#include <stdio.h>
#include <unistd.h>

#include "alloc-util.h"
#include "fd-util.h"
#include "macro.h"
#include "mount-util.h"
#include "path-util.h"
#include "rm-rf.h"
#include "stat-util.h"
#include "string-util.h"
#include "strv.h"
#include "util.h"

#define test_path_compare(a, b, result) {                 \
                assert_se(path_compare(a, b) == result);  \
                assert_se(path_compare(b, a) == -result); \
                assert_se(path_equal(a, b) == !result);   \
                assert_se(path_equal(b, a) == !result);   \
        }

static void test_path_simplify(const char *in, const char *out, const char *out_dot) {
        char *p;

        p = strdupa(in);
        assert_se(streq(path_simplify(p, false), out));

        p = strdupa(in);
        assert_se(streq(path_simplify(p, true), out_dot));
}

static void test_path(void) {
        _cleanup_close_ int fd = -1;

        test_path_compare("/goo", "/goo", 0);
        test_path_compare("/goo", "/goo", 0);
        test_path_compare("//goo", "/goo", 0);
        test_path_compare("//goo/////", "/goo", 0);
        test_path_compare("goo/////", "goo", 0);

        test_path_compare("/goo/boo", "/goo//boo", 0);
        test_path_compare("//goo/boo", "/goo/boo//", 0);

        test_path_compare("/", "///", 0);

        test_path_compare("/x", "x/", 1);
        test_path_compare("x/", "/", -1);

        test_path_compare("/x/./y", "x/y", 1);
        test_path_compare("x/.y", "x/y", -1);

        test_path_compare("foo", "/foo", -1);
        test_path_compare("/foo", "/foo/bar", -1);
        test_path_compare("/foo/aaa", "/foo/b", -1);
        test_path_compare("/foo/aaa", "/foo/b/a", -1);
        test_path_compare("/foo/a", "/foo/aaa", -1);
        test_path_compare("/foo/a/b", "/foo/aaa", -1);

        assert_se(path_is_absolute("/"));
        assert_se(!path_is_absolute("./"));

        assert_se(is_path("/dir"));
        assert_se(is_path("a/b"));
        assert_se(!is_path("."));

        assert_se(streq(basename("./aa/bb/../file.da."), "file.da."));
        assert_se(streq(basename("/aa///.file"), ".file"));
        assert_se(streq(basename("/aa///file..."), "file..."));
        assert_se(streq(basename("file.../"), ""));

        fd = open("/", O_RDONLY|O_CLOEXEC|O_DIRECTORY|O_NOCTTY);
        assert_se(fd >= 0);
        assert_se(fd_is_mount_point(fd, "/", 0) > 0);

        test_path_simplify("aaa/bbb////ccc", "aaa/bbb/ccc", "aaa/bbb/ccc");
        test_path_simplify("//aaa/.////ccc", "/aaa/./ccc", "/aaa/ccc");
        test_path_simplify("///", "/", "/");
        test_path_simplify("///.//", "/.", "/");
        test_path_simplify("///.//.///", "/./.", "/");
        test_path_simplify("////.././///../.", "/.././../.", "/../..");
        test_path_simplify(".", ".", "");
        test_path_simplify("./", ".", "");
        test_path_simplify(".///.//./.", "./././.", "");
        test_path_simplify(".///.//././/", "./././.", "");
        test_path_simplify("//./aaa///.//./.bbb/..///c.//d.dd///..eeee/.",
                           "/./aaa/././.bbb/../c./d.dd/..eeee/.",
                           "/aaa/.bbb/../c./d.dd/..eeee");
        test_path_simplify("//./aaa///.//./.bbb/..///c.//d.dd///..eeee/..",
                           "/./aaa/././.bbb/../c./d.dd/..eeee/..",
                           "/aaa/.bbb/../c./d.dd/..eeee/..");
        test_path_simplify(".//./aaa///.//./.bbb/..///c.//d.dd///..eeee/..",
                           "././aaa/././.bbb/../c./d.dd/..eeee/..",
                           "aaa/.bbb/../c./d.dd/..eeee/..");
        test_path_simplify("..//./aaa///.//./.bbb/..///c.//d.dd///..eeee/..",
                           ".././aaa/././.bbb/../c./d.dd/..eeee/..",
                           "../aaa/.bbb/../c./d.dd/..eeee/..");

        assert_se(PATH_IN_SET("/bin", "/", "/bin", "/foo"));
        assert_se(PATH_IN_SET("/bin", "/bin"));
        assert_se(PATH_IN_SET("/bin", "/foo/bar", "/bin"));
        assert_se(PATH_IN_SET("/", "/", "/", "/foo/bar"));
        assert_se(!PATH_IN_SET("/", "/abc", "/def"));

        assert_se(path_equal_ptr(NULL, NULL));
        assert_se(path_equal_ptr("/a", "/a"));
        assert_se(!path_equal_ptr("/a", "/b"));
        assert_se(!path_equal_ptr("/a", NULL));
        assert_se(!path_equal_ptr(NULL, "/a"));
}

static void test_path_equal_root(void) {
        /* Nail down the details of how path_equal("/", ...) works. */

        assert_se(path_equal("/", "/"));
        assert_se(path_equal("/", "//"));

        assert_se(!path_equal("/", "/./"));
        assert_se(!path_equal("/", "/../"));

        assert_se(!path_equal("/", "/.../"));

        /* Make sure that files_same works as expected. */

        assert_se(files_same("/", "/", 0) > 0);
        assert_se(files_same("/", "/", AT_SYMLINK_NOFOLLOW) > 0);
        assert_se(files_same("/", "//", 0) > 0);
        assert_se(files_same("/", "//", AT_SYMLINK_NOFOLLOW) > 0);

        assert_se(files_same("/", "/./", 0) > 0);
        assert_se(files_same("/", "/./", AT_SYMLINK_NOFOLLOW) > 0);
        assert_se(files_same("/", "/../", 0) > 0);
        assert_se(files_same("/", "/../", AT_SYMLINK_NOFOLLOW) > 0);

        assert_se(files_same("/", "/.../", 0) == -ENOENT);
        assert_se(files_same("/", "/.../", AT_SYMLINK_NOFOLLOW) == -ENOENT);

        /* The same for path_equal_or_files_same. */

        assert_se(path_equal_or_files_same("/", "/", 0));
        assert_se(path_equal_or_files_same("/", "/", AT_SYMLINK_NOFOLLOW));
        assert_se(path_equal_or_files_same("/", "//", 0));
        assert_se(path_equal_or_files_same("/", "//", AT_SYMLINK_NOFOLLOW));

        assert_se(path_equal_or_files_same("/", "/./", 0));
        assert_se(path_equal_or_files_same("/", "/./", AT_SYMLINK_NOFOLLOW));
        assert_se(path_equal_or_files_same("/", "/../", 0));
        assert_se(path_equal_or_files_same("/", "/../", AT_SYMLINK_NOFOLLOW));

        assert_se(!path_equal_or_files_same("/", "/.../", 0));
        assert_se(!path_equal_or_files_same("/", "/.../", AT_SYMLINK_NOFOLLOW));
}

static void test_find_binary(const char *self) {
        char *p;

        assert_se(find_binary("/bin/sh", &p) == 0);
        puts(p);
        assert_se(path_equal(p, "/bin/sh"));
        free(p);

        assert_se(find_binary(self, &p) == 0);
        puts(p);
        /* libtool might prefix the binary name with "lt-" */
        assert_se(endswith(p, "/lt-test-path-util") || endswith(p, "/test-path-util"));
        assert_se(path_is_absolute(p));
        free(p);

        assert_se(find_binary("sh", &p) == 0);
        puts(p);
        assert_se(endswith(p, "/sh"));
        assert_se(path_is_absolute(p));
        free(p);

        assert_se(find_binary("xxxx-xxxx", &p) == -ENOENT);
        assert_se(find_binary("/some/dir/xxxx-xxxx", &p) == -ENOENT);
}

static void test_prefixes(void) {
        static const char* values[] = { "/a/b/c/d", "/a/b/c", "/a/b", "/a", "", NULL};
        unsigned i;
        char s[PATH_MAX];
        bool b;

        i = 0;
        PATH_FOREACH_PREFIX_MORE(s, "/a/b/c/d") {
                log_error("---%s---", s);
                assert_se(streq(s, values[i++]));
        }
        assert_se(values[i] == NULL);

        i = 1;
        PATH_FOREACH_PREFIX(s, "/a/b/c/d") {
                log_error("---%s---", s);
                assert_se(streq(s, values[i++]));
        }
        assert_se(values[i] == NULL);

        i = 0;
        PATH_FOREACH_PREFIX_MORE(s, "////a////b////c///d///////")
                assert_se(streq(s, values[i++]));
        assert_se(values[i] == NULL);

        i = 1;
        PATH_FOREACH_PREFIX(s, "////a////b////c///d///////")
                assert_se(streq(s, values[i++]));
        assert_se(values[i] == NULL);

        PATH_FOREACH_PREFIX(s, "////")
                assert_not_reached("Wut?");

        b = false;
        PATH_FOREACH_PREFIX_MORE(s, "////") {
                assert_se(!b);
                assert_se(streq(s, ""));
                b = true;
        }
        assert_se(b);

        PATH_FOREACH_PREFIX(s, "")
                assert_not_reached("wut?");

        b = false;
        PATH_FOREACH_PREFIX_MORE(s, "") {
                assert_se(!b);
                assert_se(streq(s, ""));
                b = true;
        }
}

static void test_path_join(void) {

#define test_join(root, path, rest, expected) {  \
                _cleanup_free_ char *z = NULL;   \
                z = path_join(root, path, rest); \
                assert_se(streq(z, expected));   \
        }

        test_join("/root", "/a/b", "/c", "/root/a/b/c");
        test_join("/root", "a/b", "c", "/root/a/b/c");
        test_join("/root", "/a/b", "c", "/root/a/b/c");
        test_join("/root", "/", "c", "/root/c");
        test_join("/root", "/", NULL, "/root/");

        test_join(NULL, "/a/b", "/c", "/a/b/c");
        test_join(NULL, "a/b", "c", "a/b/c");
        test_join(NULL, "/a/b", "c", "/a/b/c");
        test_join(NULL, "/", "c", "/c");
        test_join(NULL, "/", NULL, "/");
}

#if 0 /// UNNEEDED by elogind
static void test_fsck_exists(void) {
        /* Ensure we use a sane default for PATH. */
        unsetenv("PATH");

        /* fsck.minix is provided by util-linux and will probably exist. */
        assert_se(fsck_exists("minix") == 1);

        assert_se(fsck_exists("AbCdE") == 0);
        assert_se(fsck_exists("/../bin/") == 0);
}

static void test_make_relative(void) {
        char *result;

        assert_se(path_make_relative("some/relative/path", "/some/path", &result) < 0);
        assert_se(path_make_relative("/some/path", "some/relative/path", &result) < 0);
        assert_se(path_make_relative("/some/dotdot/../path", "/some/path", &result) < 0);

#define test(from_dir, to_path, expected) {                \
                _cleanup_free_ char *z = NULL;             \
                path_make_relative(from_dir, to_path, &z); \
                assert_se(streq(z, expected));             \
        }

        test("/", "/", ".");
        test("/", "/some/path", "some/path");
        test("/some/path", "/some/path", ".");
        test("/some/path", "/some/path/in/subdir", "in/subdir");
        test("/some/path", "/", "../..");
        test("/some/path", "/some/other/path", "../other/path");
        test("/some/path/./dot", "/some/further/path", "../../further/path");
        test("//extra/////slashes///won't////fool///anybody//", "////extra///slashes////are/just///fine///", "../../../are/just/fine");
}
#endif // 0

static void test_strv_resolve(void) {
        char tmp_dir[] = "/tmp/test-path-util-XXXXXX";
        _cleanup_strv_free_ char **search_dirs = NULL;
        _cleanup_strv_free_ char **absolute_dirs = NULL;
        char **d;

        assert_se(mkdtemp(tmp_dir) != NULL);

        search_dirs = strv_new("/dir1", "/dir2", "/dir3", NULL);
        assert_se(search_dirs);
        STRV_FOREACH(d, search_dirs) {
                char *p = strappend(tmp_dir, *d);
                assert_se(p);
                assert_se(strv_push(&absolute_dirs, p) == 0);
        }

        assert_se(mkdir(absolute_dirs[0], 0700) == 0);
        assert_se(mkdir(absolute_dirs[1], 0700) == 0);
        assert_se(symlink("dir2", absolute_dirs[2]) == 0);

        path_strv_resolve(search_dirs, tmp_dir);
        assert_se(streq(search_dirs[0], "/dir1"));
        assert_se(streq(search_dirs[1], "/dir2"));
        assert_se(streq(search_dirs[2], "/dir2"));

        assert_se(rm_rf(tmp_dir, REMOVE_ROOT|REMOVE_PHYSICAL) == 0);
}

static void test_path_startswith(void) {
        const char *p;

        p = path_startswith("/foo/bar/barfoo/", "/foo");
        assert_se(streq_ptr(p, "bar/barfoo/"));

        p = path_startswith("/foo/bar/barfoo/", "/foo/");
        assert_se(streq_ptr(p, "bar/barfoo/"));

        p = path_startswith("/foo/bar/barfoo/", "/");
        assert_se(streq_ptr(p, "foo/bar/barfoo/"));

        p = path_startswith("/foo/bar/barfoo/", "////");
        assert_se(streq_ptr(p, "foo/bar/barfoo/"));

        p = path_startswith("/foo/bar/barfoo/", "/foo//bar/////barfoo///");
        assert_se(streq_ptr(p, ""));

        p = path_startswith("/foo/bar/barfoo/", "/foo/bar/barfoo////");
        assert_se(streq_ptr(p, ""));

        p = path_startswith("/foo/bar/barfoo/", "/foo/bar///barfoo/");
        assert_se(streq_ptr(p, ""));

        p = path_startswith("/foo/bar/barfoo/", "/foo////bar/barfoo/");
        assert_se(streq_ptr(p, ""));

        p = path_startswith("/foo/bar/barfoo/", "////foo/bar/barfoo/");
        assert_se(streq_ptr(p, ""));

        p = path_startswith("/foo/bar/barfoo/", "/foo/bar/barfoo");
        assert_se(streq_ptr(p, ""));

        assert_se(!path_startswith("/foo/bar/barfoo/", "/foo/bar/barfooa/"));
        assert_se(!path_startswith("/foo/bar/barfoo/", "/foo/bar/barfooa"));
        assert_se(!path_startswith("/foo/bar/barfoo/", ""));
        assert_se(!path_startswith("/foo/bar/barfoo/", "/bar/foo"));
        assert_se(!path_startswith("/foo/bar/barfoo/", "/f/b/b/"));
}

static void test_prefix_root_one(const char *r, const char *p, const char *expected) {
        _cleanup_free_ char *s = NULL;
        const char *t;

        assert_se(s = prefix_root(r, p));
        assert_se(streq_ptr(s, expected));

        t = prefix_roota(r, p);
        assert_se(t);
        assert_se(streq_ptr(t, expected));
}

static void test_prefix_root(void) {
        test_prefix_root_one("/", "/foo", "/foo");
        test_prefix_root_one(NULL, "/foo", "/foo");
        test_prefix_root_one("", "/foo", "/foo");
        test_prefix_root_one("///", "/foo", "/foo");
        test_prefix_root_one("/", "////foo", "/foo");
        test_prefix_root_one(NULL, "////foo", "/foo");

        test_prefix_root_one("/foo", "/bar", "/foo/bar");
        test_prefix_root_one("/foo", "bar", "/foo/bar");
        test_prefix_root_one("foo", "bar", "foo/bar");
        test_prefix_root_one("/foo/", "/bar", "/foo/bar");
        test_prefix_root_one("/foo/", "//bar", "/foo/bar");
        test_prefix_root_one("/foo///", "//bar", "/foo/bar");
}

static void test_file_in_same_dir(void) {
        char *t;

        t = file_in_same_dir("/", "a");
        assert_se(streq(t, "/a"));
        free(t);

        t = file_in_same_dir("/", "/a");
        assert_se(streq(t, "/a"));
        free(t);

        t = file_in_same_dir("", "a");
        assert_se(streq(t, "a"));
        free(t);

        t = file_in_same_dir("a/", "a");
        assert_se(streq(t, "a/a"));
        free(t);

        t = file_in_same_dir("bar/foo", "bar");
        assert_se(streq(t, "bar/bar"));
        free(t);
}

static void test_last_path_component(void) {
        assert_se(streq(last_path_component("a/b/c"), "c"));
        assert_se(streq(last_path_component("a/b/c/"), "c/"));
        assert_se(streq(last_path_component("/"), "/"));
        assert_se(streq(last_path_component("//"), "/"));
        assert_se(streq(last_path_component("///"), "/"));
        assert_se(streq(last_path_component("."), "."));
        assert_se(streq(last_path_component("./."), "."));
        assert_se(streq(last_path_component("././"), "./"));
        assert_se(streq(last_path_component("././/"), ".//"));
        assert_se(streq(last_path_component("/foo/a"), "a"));
        assert_se(streq(last_path_component("/foo/a/"), "a/"));
        assert_se(streq(last_path_component(""), ""));
        assert_se(streq(last_path_component("a"), "a"));
        assert_se(streq(last_path_component("a/"), "a/"));
        assert_se(streq(last_path_component("/a"), "a"));
        assert_se(streq(last_path_component("/a/"), "a/"));
}

static void test_filename_is_valid(void) {
        char foo[FILENAME_MAX+2];
        int i;

        assert_se(!filename_is_valid(""));
        assert_se(!filename_is_valid("/bar/foo"));
        assert_se(!filename_is_valid("/"));
        assert_se(!filename_is_valid("."));
        assert_se(!filename_is_valid(".."));

        for (i=0; i<FILENAME_MAX+1; i++)
                foo[i] = 'a';
        foo[FILENAME_MAX+1] = '\0';

        assert_se(!filename_is_valid(foo));

        assert_se(filename_is_valid("foo_bar-333"));
        assert_se(filename_is_valid("o.o"));
}

static void test_hidden_or_backup_file(void) {
        assert_se(hidden_or_backup_file(".hidden"));
        assert_se(hidden_or_backup_file("..hidden"));
        assert_se(!hidden_or_backup_file("hidden."));

        assert_se(hidden_or_backup_file("backup~"));
        assert_se(hidden_or_backup_file(".backup~"));

        assert_se(hidden_or_backup_file("lost+found"));
        assert_se(hidden_or_backup_file("aquota.user"));
        assert_se(hidden_or_backup_file("aquota.group"));

        assert_se(hidden_or_backup_file("test.rpmnew"));
        assert_se(hidden_or_backup_file("test.dpkg-old"));
        assert_se(hidden_or_backup_file("test.dpkg-remove"));
        assert_se(hidden_or_backup_file("test.swp"));

        assert_se(!hidden_or_backup_file("test.rpmnew."));
        assert_se(!hidden_or_backup_file("test.dpkg-old.foo"));
}

#if 0 /// UNNEEDED by elogind
static void test_systemd_installation_has_version(const char *path) {
        int r;
        const unsigned versions[] = {0, 231, atoi(PACKAGE_VERSION), 999};
        unsigned i;

        for (i = 0; i < ELEMENTSOF(versions); i++) {
                r = systemd_installation_has_version(path, versions[i]);
                assert_se(r >= 0);
                log_info("%s has systemd >= %u: %s",
                         path ?: "Current installation", versions[i], yes_no(r));
        }
}
#endif // 0

static void test_skip_dev_prefix(void) {

        assert_se(streq(skip_dev_prefix("/"), "/"));
        assert_se(streq(skip_dev_prefix("/dev"), ""));
        assert_se(streq(skip_dev_prefix("/dev/"), ""));
        assert_se(streq(skip_dev_prefix("/dev/foo"), "foo"));
        assert_se(streq(skip_dev_prefix("/dev/foo/bar"), "foo/bar"));
        assert_se(streq(skip_dev_prefix("//dev"), ""));
        assert_se(streq(skip_dev_prefix("//dev//"), ""));
        assert_se(streq(skip_dev_prefix("/dev///foo"), "foo"));
        assert_se(streq(skip_dev_prefix("///dev///foo///bar"), "foo///bar"));
        assert_se(streq(skip_dev_prefix("//foo"), "//foo"));
        assert_se(streq(skip_dev_prefix("foo"), "foo"));
}

static void test_empty_or_root(void) {
        assert_se(empty_or_root(NULL));
        assert_se(empty_or_root(""));
        assert_se(empty_or_root("/"));
        assert_se(empty_or_root("//"));
        assert_se(empty_or_root("///"));
        assert_se(empty_or_root("/////////////////"));
        assert_se(!empty_or_root("xxx"));
        assert_se(!empty_or_root("/xxx"));
        assert_se(!empty_or_root("/xxx/"));
        assert_se(!empty_or_root("//yy//"));
}

int main(int argc, char **argv) {
        log_set_max_level(LOG_DEBUG);
        log_parse_environment();
        log_open();

        test_path();
        test_path_equal_root();
        test_find_binary(argv[0]);
        test_prefixes();
        test_path_join();
#if 0 /// UNNEEDED by elogind
        test_fsck_exists();
        test_make_relative();
#endif // 0
        test_strv_resolve();
        test_path_startswith();
        test_prefix_root();
        test_file_in_same_dir();
        test_last_path_component();
        test_filename_is_valid();
        test_hidden_or_backup_file();
        test_skip_dev_prefix();
        test_empty_or_root();

#if 0 /// UNNEEDED by elogind
        test_systemd_installation_has_version(argv[1]); /* NULL is OK */
#endif // 0

        return 0;
}
