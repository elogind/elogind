/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <unistd.h>

#include "cgroup-setup.h"
#include "cgroup-util.h"
#include "errno-util.h"
#include "fd-util.h"
#include "path-util.h"
#include "process-util.h"
#include "string-util.h"
#include "tests.h"

#if 0 /// UNNEEDED by elogind
TEST(cg_split_spec) {
        char *c, *p;

        assert_se(cg_split_spec("foobar:/", &c, &p) == 0);
        ASSERT_STREQ(c, "foobar");
        ASSERT_STREQ(p, "/");
        c = mfree(c);
        p = mfree(p);

        assert_se(cg_split_spec("foobar:", &c, &p) == 0);
        c = mfree(c);
        p = mfree(p);

        assert_se(cg_split_spec("foobar:asdfd", &c, &p) < 0);
        assert_se(cg_split_spec(":///", &c, &p) < 0);
        assert_se(cg_split_spec(":", &c, &p) < 0);
        assert_se(cg_split_spec("", &c, &p) < 0);
        assert_se(cg_split_spec("fo/obar:/", &c, &p) < 0);

        assert_se(cg_split_spec("/", &c, &p) >= 0);
        ASSERT_NULL(c);
        ASSERT_STREQ(p, "/");
        p = mfree(p);

        assert_se(cg_split_spec("foo", &c, &p) >= 0);
        ASSERT_STREQ(c, "foo");
        ASSERT_NULL(p);
        c = mfree(c);
}
#endif // 0

TEST(cg_create) {
        int r;

        r = cg_unified_cached(false);
        if (IN_SET(r, -ENOMEDIUM, -ENOENT)) {
                log_tests_skipped("cgroupfs is not mounted");
                return;
        }
        assert_se(r >= 0);

        _cleanup_free_ char *here = NULL;
        assert_se(cg_pid_get_path_shifted(0, NULL, &here) >= 0);

        const char *test_a = prefix_roota(here, "/test-a"),
                   *test_b = prefix_roota(here, "/test-b"),
                   *test_c = prefix_roota(here, "/test-b/test-c"),
                   *test_d = prefix_roota(here, "/test-b/test-d");
        char *path;

        log_info("Paths for test:\n%s\n%s", test_a, test_b);

        /* Possibly clean up left-overs from aboted previous runs */
        (void) cg_trim(SYSTEMD_CGROUP_CONTROLLER, test_a, /* delete_root= */ true);
        (void) cg_trim(SYSTEMD_CGROUP_CONTROLLER, test_b, /* delete_root= */ true);

        r = cg_create(SYSTEMD_CGROUP_CONTROLLER, test_a);
        if (IN_SET(r, -EPERM, -EACCES, -EROFS)) {
                log_info_errno(r, "Skipping %s: %m", __func__);
                return;
        }

#if 0 /// Continued tests might also result in 0 (already exists), so elogind shall be aware of that
        assert_se(r == 1);
        assert_se(cg_create(SYSTEMD_CGROUP_CONTROLLER, test_a) == 0);
        assert_se(cg_create(SYSTEMD_CGROUP_CONTROLLER, test_b) == 1);
        assert_se(cg_create(SYSTEMD_CGROUP_CONTROLLER, test_c) == 1);
#else // 0
        assert_se(r >= 0);
        assert_se(cg_create(SYSTEMD_CGROUP_CONTROLLER, test_a) >= 0);
        assert_se(cg_create(SYSTEMD_CGROUP_CONTROLLER, test_b) >= 0);
        assert_se(cg_create(SYSTEMD_CGROUP_CONTROLLER, test_c) >= 0);
#endif // 0
        assert_se(cg_create_and_attach(SYSTEMD_CGROUP_CONTROLLER, test_b, 0) == 0);

        assert_se(cg_pid_get_path(SYSTEMD_CGROUP_CONTROLLER, getpid_cached(), &path) == 0);
        ASSERT_STREQ(path, test_b);
        free(path);

        assert_se(cg_attach(SYSTEMD_CGROUP_CONTROLLER, test_a, 0) == 0);

        assert_se(cg_pid_get_path(SYSTEMD_CGROUP_CONTROLLER, getpid_cached(), &path) == 0);
        assert_se(path_equal(path, test_a));
        free(path);

        assert_se(cg_create_and_attach(SYSTEMD_CGROUP_CONTROLLER, test_d, 0) == 1);

        assert_se(cg_pid_get_path(SYSTEMD_CGROUP_CONTROLLER, getpid_cached(), &path) == 0);
        assert_se(path_equal(path, test_d));
        free(path);

        assert_se(cg_get_path(SYSTEMD_CGROUP_CONTROLLER, test_d, NULL, &path) == 0);
        log_debug("test_d: %s", path);
        const char *full_d;
        if (cg_all_unified())
                full_d = strjoina("/sys/fs/cgroup", test_d);
        else if (cg_hybrid_unified())
                full_d = strjoina("/sys/fs/cgroup/unified", test_d);
        else
#if 0 /// elogind uses the configured controller name, which can be different
                full_d = strjoina("/sys/fs/cgroup/systemd", test_d);
#else // 0
                full_d = strjoina("/sys/fs/cgroup/" CGROUP_CONTROLLER_NAME, test_d);
#endif // 0
        log_debug_elogind("full_d: %s", full_d);
        assert_se(path_equal(path, full_d));
        free(path);

        assert_se(cg_is_empty(SYSTEMD_CGROUP_CONTROLLER, test_a) > 0);
        assert_se(cg_is_empty(SYSTEMD_CGROUP_CONTROLLER, test_b) > 0);
        assert_se(cg_is_empty_recursive(SYSTEMD_CGROUP_CONTROLLER, test_a) > 0);
        assert_se(cg_is_empty_recursive(SYSTEMD_CGROUP_CONTROLLER, test_b) == 0);

        assert_se(cg_kill_recursive(test_a, 0, 0, NULL, NULL, NULL) == 0);
        assert_se(cg_kill_recursive(test_b, 0, 0, NULL, NULL, NULL) > 0);

        assert_se(cg_migrate_recursive(SYSTEMD_CGROUP_CONTROLLER, test_b, SYSTEMD_CGROUP_CONTROLLER, test_a, 0) > 0);

        assert_se(cg_is_empty_recursive(SYSTEMD_CGROUP_CONTROLLER, test_a) == 0);
        assert_se(cg_is_empty_recursive(SYSTEMD_CGROUP_CONTROLLER, test_b) > 0);

        assert_se(cg_kill_recursive(test_a, 0, 0, NULL, NULL, NULL) > 0);
        assert_se(cg_kill_recursive(test_b, 0, 0, NULL, NULL, NULL) == 0);

        (void) cg_trim(SYSTEMD_CGROUP_CONTROLLER, test_b, false);

        assert_se(cg_rmdir(SYSTEMD_CGROUP_CONTROLLER, test_b) == 0);
        assert_se(cg_rmdir(SYSTEMD_CGROUP_CONTROLLER, test_a) < 0);
        assert_se(cg_migrate_recursive(SYSTEMD_CGROUP_CONTROLLER, test_a, SYSTEMD_CGROUP_CONTROLLER, here, 0) > 0);
        assert_se(cg_rmdir(SYSTEMD_CGROUP_CONTROLLER, test_a) == 0);
}

#if 0 /// elogind does not open cgroup IDs anywhere
TEST(id) {
        _cleanup_free_ char *p = NULL, *p2 = NULL;
        _cleanup_close_ int fd = -EBADF, fd2 = -EBADF;
        uint64_t id, id2;
        int r;

        r = cg_all_unified();
        if (r == 0) {
                log_tests_skipped("skipping cgroupid test, not running in unified mode");
                return;
        }
        if (IN_SET(r, -ENOMEDIUM, -ENOENT)) {
                log_tests_skipped("cgroupfs is not mounted");
                return;
        }
        assert_se(r > 0);

        fd = cg_path_open(SYSTEMD_CGROUP_CONTROLLER, "/");
        assert_se(fd >= 0);

        assert_se(fd_get_path(fd, &p) >= 0);
        assert_se(path_equal(p, "/sys/fs/cgroup"));

        assert_se(cg_fd_get_cgroupid(fd, &id) >= 0);

        fd2 = cg_cgroupid_open(fd, id);

        if (ERRNO_IS_NEG_PRIVILEGE(fd2))
                log_notice("Skipping open-by-cgroup-id test because lacking privs.");
        else if (ERRNO_IS_NEG_NOT_SUPPORTED(fd2))
                log_notice("Skipping open-by-cgroup-id test because syscall is missing or blocked.");
        else {
                assert_se(fd2 >= 0);

                assert_se(fd_get_path(fd2, &p2) >= 0);
                assert_se(path_equal(p2, "/sys/fs/cgroup"));

                assert_se(cg_fd_get_cgroupid(fd2, &id2) >= 0);

                assert_se(id == id2);

                assert_se(inode_same_at(fd, NULL, fd2, NULL, AT_EMPTY_PATH) > 0);
        }
}
#endif // 0

DEFINE_TEST_MAIN(LOG_DEBUG);
