/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include "cgroup-util.h"
#include "conf-parser.h"
#include "logind.h"
#include "path-util.h"
#include "string-util.h"
#include "tests.h"

static void test_prefix_one(const char *initial, const char *rvalue, const char *expected) {
        _cleanup_free_ char *prefix = NULL;
        int r;

        if (initial) {
                prefix = strdup(initial);
                assert_se(prefix);
        }

        r = config_parse_session_cgroup_prefix("elogind", "test.conf", 1, "Login", 1,
                                               "SessionCgroupPrefix", 0, rvalue, &prefix, NULL);
        assert_se(r >= 0);
        assert_se(streq_ptr(expected, prefix));
}

TEST(session_cgroup_prefix_parse) {
        test_prefix_one(NULL, "", NULL);
        test_prefix_one(NULL, "host", "host");
        test_prefix_one(NULL, "a/b/c", "a/b/c");
        test_prefix_one(NULL, "a//b/./c/", "a/b/c");
        test_prefix_one("keep", "/abs", "keep");
        test_prefix_one("keep", "../up", "keep");
        test_prefix_one("keep", "a/../../x", "keep");
        test_prefix_one("keep", ".", "keep");
        test_prefix_one("old", "", NULL);
}

TEST(session_cgroup_prefix_gperf) {
        ConfigParserCallback func = NULL;
        Manager m;
        void *data = NULL;
        int ltype = -1;

        zero(m);

        assert_se(config_item_perf_lookup(logind_gperf_lookup, "Login", "SessionCgroupPrefix",
                                          &func, &ltype, &data, &m) > 0);
        assert_se(func == config_parse_session_cgroup_prefix);
        assert_se(data == (uint8_t*) &m + offsetof(Manager, session_cgroup_prefix));

        assert_se(func("elogind", "test.conf", 1, "Login", 1, "SessionCgroupPrefix", ltype,
                       "host", data, &m) >= 0);
        assert_se(streq_ptr(m.session_cgroup_prefix, "host"));

        m.session_cgroup_prefix = mfree(m.session_cgroup_prefix);
}

static void test_strip_one(const char *shifted, const char *prefix, const char *expected_session, int expected_code) {
        _cleanup_free_ char *prefixed = NULL, *stripped_path = NULL, *session = NULL;
        const char *stripped;
        int r;

        if (isempty(prefix)) {
                r = cg_path_get_session(shifted, &session);
                assert_se(r == expected_code);
                assert_se(streq_ptr(session, expected_session));
                return;
        }

        prefixed = path_join("/", prefix);
        assert_se(prefixed);

        stripped = path_startswith(shifted, prefixed);
        if (!stripped || stripped[0] == '\0') {
                assert_se(expected_code == -ENXIO);
                return;
        }

        stripped_path = path_join("/", stripped);
        assert_se(stripped_path);

        r = cg_path_get_session(stripped_path, &session);
        assert_se(r == expected_code);
        assert_se(streq_ptr(session, expected_session));
}

TEST(session_cgroup_prefix_strip) {
        /* empty prefix keeps root layout */
        test_strip_one("/5", "", "5", 0);
        test_strip_one("/5/session", "", "5", 0);
        test_strip_one("/c2", "", "c2", 0);

        /* single-component prefix */
        test_strip_one("/leroy/5", "leroy", "5", 0);
        test_strip_one("/leroy/5/session", "leroy", "5", 0);
        test_strip_one("/leroy/c2/delegated", "leroy", "c2", 0);

        /* multi-component prefix */
        test_strip_one("/a/b/c5", "a/b", "c5", 0);
        test_strip_one("/a/b/c5/session", "a/b", "c5", 0);
        test_strip_one("/host/elogind/c5", "host/elogind", "c5", 0);

        /* component-boundary: "host" must not match "hostile" */
        test_strip_one("/hostile/5", "host", NULL, -ENXIO);
        test_strip_one("/hostile", "host", NULL, -ENXIO);

        /* exact prefix match has no session below */
        test_strip_one("/leroy", "leroy", NULL, -ENXIO);
        test_strip_one("/a/b", "a/b", NULL, -ENXIO);

        /* non-matching prefix */
        test_strip_one("/other/5", "leroy", NULL, -ENXIO);
        test_strip_one("/5", "leroy", NULL, -ENXIO);
}

DEFINE_TEST_MAIN(LOG_INFO);
