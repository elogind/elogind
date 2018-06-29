/* SPDX-License-Identifier: LGPL-2.1+ */
/***
  This file is part of systemd.

  Copyright 2014 Michael Marineau
***/

#include <stdarg.h>
#include <stdio.h>

#include "alloc-util.h"
#include "conf-files.h"
#include "fs-util.h"
#include "macro.h"
#include "parse-util.h"
#include "rm-rf.h"
#include "string-util.h"
#include "strv.h"
#include "user-util.h"
#include "util.h"

static void setup_test_dir(char *tmp_dir, const char *files, ...) {
        va_list ap;

        assert_se(mkdtemp(tmp_dir) != NULL);

        va_start(ap, files);
        while (files != NULL) {
                _cleanup_free_ char *path = strappend(tmp_dir, files);
                assert_se(touch_file(path, true, USEC_INFINITY, UID_INVALID, GID_INVALID, MODE_INVALID) == 0);
                files = va_arg(ap, const char *);
        }
        va_end(ap);
}

static void test_conf_files_list(bool use_root) {
        char tmp_dir[] = "/tmp/test-conf-files-XXXXXX";
        _cleanup_strv_free_ char **found_files = NULL, **found_files2 = NULL;
        const char *root_dir, *search_1, *search_2, *expect_a, *expect_b, *expect_c;

        log_debug("/* %s */", __func__);

        setup_test_dir(tmp_dir,
                       "/dir1/a.conf",
                       "/dir2/a.conf",
                       "/dir2/b.conf",
                       "/dir2/c.foo",
                       NULL);

        if (use_root) {
                root_dir = tmp_dir;
                search_1 = "/dir1";
                search_2 = "/dir2";
        } else {
                root_dir = NULL;
                search_1 = strjoina(tmp_dir, "/dir1");
                search_2 = strjoina(tmp_dir, "/dir2");
        }

        expect_a = strjoina(tmp_dir, "/dir1/a.conf");
        expect_b = strjoina(tmp_dir, "/dir2/b.conf");
        expect_c = strjoina(tmp_dir, "/dir2/c.foo");

        log_debug("/* Check when filtered by suffix */");

        assert_se(conf_files_list(&found_files, ".conf", root_dir, 0, search_1, search_2, NULL) == 0);
        strv_print(found_files);

        assert_se(found_files);
        assert_se(streq_ptr(found_files[0], expect_a));
        assert_se(streq_ptr(found_files[1], expect_b));
        assert_se(found_files[2] == NULL);

        log_debug("/* Check when unfiltered */");
        assert_se(conf_files_list(&found_files2, NULL, root_dir, 0, search_1, search_2, NULL) == 0);
        strv_print(found_files2);

        assert_se(found_files2);
        assert_se(streq_ptr(found_files2[0], expect_a));
        assert_se(streq_ptr(found_files2[1], expect_b));
        assert_se(streq_ptr(found_files2[2], expect_c));
        assert_se(found_files2[3] == NULL);

        assert_se(rm_rf(tmp_dir, REMOVE_ROOT|REMOVE_PHYSICAL) == 0);
}

int main(int argc, char **argv) {
        log_set_max_level(LOG_DEBUG);
        log_parse_environment();
        log_open();

        test_conf_files_list(false);
        test_conf_files_list(true);
        return 0;
}
