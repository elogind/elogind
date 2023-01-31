/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <signal.h>

#include "parse-argument.h"
#include "stdio-util.h"
#include "tests.h"

TEST(parse_json_argument) {
        JsonFormatFlags flags = JSON_FORMAT_PRETTY;

        assert_se(parse_json_argument("help", &flags) == 0);
        assert_se(flags == JSON_FORMAT_PRETTY);

        assert_se(parse_json_argument("off", &flags) == 1);
        assert_se(flags == JSON_FORMAT_OFF);
}

#if 0 /// UNNEEDED by elogind
TEST(parse_path_argument) {
        _cleanup_free_ char *path = NULL;

        assert_se(parse_path_argument("help", false, &path) == 0);
        assert_se(streq(basename(path), "help"));

        assert_se(parse_path_argument("/", false, &path) == 0);
        assert_se(streq(path, "/"));

        assert_se(parse_path_argument("/", true, &path) == 0);
        assert_se(path == NULL);
}
#endif // 0

TEST(parse_signal_argument) {
        int  signal = -1;

        assert_se(parse_signal_argument("help", &signal) == 0);
        assert_se(signal == -1);

        assert_se(parse_signal_argument("list", &signal) == 0);
        assert_se(signal == -1);

        assert_se(parse_signal_argument("SIGABRT", &signal) == 1);
        assert_se(signal == SIGABRT);

        assert_se(parse_signal_argument("ABRT", &signal) == 1);
        assert_se(signal == SIGABRT);

        char buf[DECIMAL_STR_MAX(int)];
        xsprintf(buf, "%d", SIGABRT);
        assert_se(parse_signal_argument(buf, &signal) == 1);
        assert_se(signal == SIGABRT);
}

DEFINE_TEST_MAIN(LOG_INFO);
