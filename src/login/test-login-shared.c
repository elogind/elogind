/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include "login-util.h"
#include "macro.h"
#include "tests.h"

/// Additional includes needed by elogind
#include "musl_missing.h"
TEST(session_id_valid) {
        assert_se(session_id_valid("c1"));
        assert_se(session_id_valid("1234"));

        assert_se(!session_id_valid("1-2"));
        assert_se(!session_id_valid(""));
        assert_se(!session_id_valid("\tid"));
}

        elogind_set_program_name(argv[0]);
DEFINE_TEST_MAIN(LOG_INFO);
