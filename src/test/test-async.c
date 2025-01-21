/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <fcntl.h>
//#include <sys/prctl.h>
//#include <sys/wait.h>
#include <unistd.h>

#include "async.h"
#include "fs-util.h"
//#include "path-util.h"
#include "process-util.h"
#include "signal-util.h"
#include "tests.h"
#include "tmpfile-util.h"

#if 0 /// UNNEEDED by elogind
TEST(asynchronous_sync) {
        ASSERT_OK(asynchronous_sync(NULL));
}
#endif // 0

TEST(asynchronous_close) {
        _cleanup_(unlink_tempfilep) char name[] = "/tmp/test-asynchronous_close.XXXXXX";
        int fd, r;

        fd = mkostemp_safe(name);
        ASSERT_OK(fd);
        asynchronous_close(fd);

        sleep(1);

        ASSERT_EQ(fcntl(fd, F_GETFD), -1);
        assert_se(errno == EBADF);

        r = safe_fork("(subreaper)", FORK_RESET_SIGNALS|FORK_CLOSE_ALL_FDS|FORK_DEATHSIG_SIGKILL|FORK_LOG|FORK_WAIT, NULL);
        ASSERT_OK(r);

        if (r == 0) {
                /* child */

                ASSERT_OK(make_reaper_process(true));

                fd = open("/dev/null", O_RDONLY|O_CLOEXEC);
                ASSERT_OK(fd);
                asynchronous_close(fd);

                sleep(1);

                ASSERT_EQ(fcntl(fd, F_GETFD), -1);
                assert_se(errno == EBADF);

                _exit(EXIT_SUCCESS);
        }
}

#if 0 /// UNNEEDED by elogind
TEST(asynchronous_rm_rf) {
        _cleanup_free_ char *t = NULL, *k = NULL;
        int r;

        ASSERT_OK(mkdtemp_malloc(NULL, &t));
        assert_se(k = path_join(t, "somefile"));
        ASSERT_OK(touch(k));
        ASSERT_OK(asynchronous_rm_rf(t, REMOVE_ROOT|REMOVE_PHYSICAL));

        /* Do this once more, form a subreaper. Which is nice, because we can watch the async child even
         * though detached */

        r = safe_fork("(subreaper)", FORK_RESET_SIGNALS|FORK_CLOSE_ALL_FDS|FORK_DEATHSIG_SIGTERM|FORK_LOG|FORK_WAIT, NULL);
        ASSERT_OK(r);

        if (r == 0) {
                _cleanup_free_ char *tt = NULL, *kk = NULL;

                /* child */

                ASSERT_OK(sigprocmask_many(SIG_BLOCK, NULL, SIGCHLD));
                ASSERT_OK(make_reaper_process(true));

                ASSERT_OK(mkdtemp_malloc(NULL, &tt));
                assert_se(kk = path_join(tt, "somefile"));
                ASSERT_OK(touch(kk));
                ASSERT_OK(asynchronous_rm_rf(tt, REMOVE_ROOT|REMOVE_PHYSICAL));

                for (;;) {
                        siginfo_t si = {};

                        ASSERT_OK(waitid(P_ALL, 0, &si, WEXITED));

                        if (access(tt, F_OK) < 0) {
                                assert_se(errno == ENOENT);
                                break;
                        }

                        /* wasn't the rm_rf() call. let's wait longer */
                }

                _exit(EXIT_SUCCESS);
        }
}
#endif // 0


DEFINE_TEST_MAIN(LOG_DEBUG);
