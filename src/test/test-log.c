/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <stddef.h>
#include <unistd.h>

#include "format-util.h"
#include "log.h"
#include "process-util.h"
#include "string-util.h"
#include "util.h"

#define X10(x) x x x x x x x x x x
#define X100(x) X10(X10(x))
#define X1000(x) X100(X10(x))

static void test_file(void) {
        log_info("__FILE__: %s", __FILE__);
        log_info("RELATIVE_SOURCE_PATH: %s", RELATIVE_SOURCE_PATH);
        log_info("PROJECT_FILE: %s", PROJECT_FILE);

        assert_se(startswith(__FILE__, RELATIVE_SOURCE_PATH "/"));
}

static void test_log_struct(void) {
        log_struct(LOG_INFO,
                   "MESSAGE=Waldo PID="PID_FMT" (no errno)", getpid_cached(),
                   "SERVICE=piepapo");

        /* The same as above, just using LOG_MESSAGE(), which is generally recommended */
        log_struct(LOG_INFO,
                   LOG_MESSAGE("Waldo PID="PID_FMT" (no errno)", getpid_cached()),
                   "SERVICE=piepapo");

        log_struct_errno(LOG_INFO, EILSEQ,
                         LOG_MESSAGE("Waldo PID="PID_FMT": %m (normal)", getpid_cached()),
                         "SERVICE=piepapo");

        log_struct_errno(LOG_INFO, SYNTHETIC_ERRNO(EILSEQ),
                         LOG_MESSAGE("Waldo PID="PID_FMT": %m (synthetic)", getpid_cached()),
                         "SERVICE=piepapo");

        log_struct(LOG_INFO,
                   LOG_MESSAGE("Foobar PID="PID_FMT, getpid_cached()),
                   "FORMAT_STR_TEST=1=%i A=%c 2=%hi 3=%li 4=%lli 1=%p foo=%s 2.5=%g 3.5=%g 4.5=%Lg",
                   (int) 1, 'A', (short) 2, (long int) 3, (long long int) 4, (void*) 1, "foo", (float) 2.5f, (double) 3.5, (long double) 4.5,
                   "SUFFIX=GOT IT");
}

static void test_long_lines(void) {
        log_object_internal(LOG_NOTICE,
                            EUCLEAN,
                            X1000("abcd_") ".txt",
                            1000000,
                            X1000("fff") "unc",
                            "OBJECT=",
                            X1000("obj_") "ect",
                            "EXTRA=",
                            X1000("ext_") "tra",
                            "asdfasdf %s asdfasdfa", "foobar");
}

static void test_log_syntax(void) {
        assert_se(log_syntax("unit", LOG_ERR, "filename", 10, EINVAL, "EINVAL: %s: %m", "hogehoge") == -EINVAL);
        assert_se(log_syntax("unit", LOG_ERR, "filename", 10, -ENOENT, "ENOENT: %s: %m", "hogehoge") == -ENOENT);
        assert_se(log_syntax("unit", LOG_ERR, "filename", 10, SYNTHETIC_ERRNO(ENOTTY), "ENOTTY: %s: %m", "hogehoge") == -ENOTTY);
}

int main(int argc, char* argv[]) {
        assert_se(IS_SYNTHETIC_ERRNO(SYNTHETIC_ERRNO(EINVAL)));
        assert_se(IS_SYNTHETIC_ERRNO(SYNTHETIC_ERRNO(-EINVAL)));
        assert_cc(!IS_SYNTHETIC_ERRNO(EINVAL));
        assert_cc(!IS_SYNTHETIC_ERRNO(-EINVAL));
        assert_se(IS_SYNTHETIC_ERRNO(SYNTHETIC_ERRNO(0)));
        assert_cc(!IS_SYNTHETIC_ERRNO(0));
        assert_se(ERRNO_VALUE(EINVAL) == EINVAL);
        assert_se(ERRNO_VALUE(SYNTHETIC_ERRNO(-EINVAL)) == EINVAL);

        test_file();

        assert_se(log_info_errno(SYNTHETIC_ERRNO(EUCLEAN), "foo") == -EUCLEAN);

        for (int target = 0; target < _LOG_TARGET_MAX; target++) {
                log_set_target(target);
                log_open();

                test_log_struct();
                test_long_lines();
                test_log_syntax();
        }

        return 0;
}
