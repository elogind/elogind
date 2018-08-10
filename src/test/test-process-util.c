/* SPDX-License-Identifier: LGPL-2.1+ */
/***
  This file is part of systemd.

  Copyright 2010 Lennart Poettering
  Copyright 2013 Thomas H.P. Andersen
***/

#include <sched.h>
#include <sys/mount.h>
#include <sys/personality.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#if HAVE_VALGRIND_VALGRIND_H
#include <valgrind/valgrind.h>
#endif

#include "alloc-util.h"
//#include "architecture.h"
#include "fd-util.h"
#include "log.h"
#include "macro.h"
#include "parse-util.h"
#include "process-util.h"
#include "signal-util.h"
#include "stdio-util.h"
#include "string-util.h"
#include "terminal-util.h"
#include "test-helper.h"
#include "util.h"
#include "virt.h"

static void test_get_process_comm(pid_t pid) {
        struct stat st;
        _cleanup_free_ char *a = NULL, *c = NULL, *d = NULL, *f = NULL, *i = NULL;
        _cleanup_free_ char *env = NULL;
        char path[STRLEN("/proc//comm") + DECIMAL_STR_MAX(pid_t)];
#if 0 /// UNNEEDED by elogind
        pid_t e;
        uid_t u;
        gid_t g;
#endif // 0
        dev_t h;
        int r;

        xsprintf(path, "/proc/"PID_FMT"/comm", pid);

        if (stat(path, &st) == 0) {
                assert_se(get_process_comm(pid, &a) >= 0);
                log_info("PID"PID_FMT" comm: '%s'", pid, a);
        } else
                log_warning("%s not exist.", path);

        assert_se(get_process_cmdline(pid, 0, true, &c) >= 0);
        log_info("PID"PID_FMT" cmdline: '%s'", pid, c);

        assert_se(get_process_cmdline(pid, 8, false, &d) >= 0);
        log_info("PID"PID_FMT" cmdline truncated to 8: '%s'", pid, d);

        free(d);
        assert_se(get_process_cmdline(pid, 1, false, &d) >= 0);
        log_info("PID"PID_FMT" cmdline truncated to 1: '%s'", pid, d);

#if 0 /// UNNEEDED by elogind
        assert_se(get_process_ppid(pid, &e) >= 0);
        log_info("PID"PID_FMT" PPID: "PID_FMT, pid, e);
        assert_se(pid == 1 ? e == 0 : e > 0);
#endif // 0

        assert_se(is_kernel_thread(pid) == 0 || pid != 1);

        r = get_process_exe(pid, &f);
        assert_se(r >= 0 || r == -EACCES);
        log_info("PID"PID_FMT" exe: '%s'", pid, strna(f));

#if 0 /// UNNEEDED by elogind
        assert_se(get_process_uid(pid, &u) == 0);
        log_info("PID"PID_FMT" UID: "UID_FMT, pid, u);
        assert_se(u == 0 || pid != 1);

        assert_se(get_process_gid(pid, &g) == 0);
        log_info("PID"PID_FMT" GID: "GID_FMT, pid, g);
        assert_se(g == 0 || pid != 1);

        r = get_process_environ(pid, &env);
        assert_se(r >= 0 || r == -EACCES);
        log_info("PID"PID_FMT" strlen(environ): %zi", pid, env ? (ssize_t)strlen(env) : (ssize_t)-errno);
#endif // 0

        if (!detect_container())
                assert_se(get_ctty_devnr(pid, &h) == -ENXIO || pid != 1);

        (void) getenv_for_pid(pid, "PATH", &i);
        log_info("PID"PID_FMT" $PATH: '%s'", pid, strna(i));
}

static void test_pid_is_unwaited(void) {
        pid_t pid;

        pid = fork();
        assert_se(pid >= 0);
        if (pid == 0) {
                _exit(EXIT_SUCCESS);
        } else {
                int status;

                waitpid(pid, &status, 0);
                assert_se(!pid_is_unwaited(pid));
        }
        assert_se(pid_is_unwaited(getpid_cached()));
        assert_se(!pid_is_unwaited(-1));
}

static void test_pid_is_alive(void) {
        pid_t pid;

        pid = fork();
        assert_se(pid >= 0);
        if (pid == 0) {
                _exit(EXIT_SUCCESS);
        } else {
                int status;

                waitpid(pid, &status, 0);
                assert_se(!pid_is_alive(pid));
        }
        assert_se(pid_is_alive(getpid_cached()));
        assert_se(!pid_is_alive(-1));
}

#if 0 /// UNNEEDED by elogind
static void test_personality(void) {

        assert_se(personality_to_string(PER_LINUX));
        assert_se(!personality_to_string(PERSONALITY_INVALID));

        assert_se(streq(personality_to_string(PER_LINUX), architecture_to_string(native_architecture())));

        assert_se(personality_from_string(personality_to_string(PER_LINUX)) == PER_LINUX);
        assert_se(personality_from_string(architecture_to_string(native_architecture())) == PER_LINUX);

#ifdef __x86_64__
        assert_se(streq_ptr(personality_to_string(PER_LINUX), "x86-64"));
        assert_se(streq_ptr(personality_to_string(PER_LINUX32), "x86"));

        assert_se(personality_from_string("x86-64") == PER_LINUX);
        assert_se(personality_from_string("x86") == PER_LINUX32);
        assert_se(personality_from_string("ia64") == PERSONALITY_INVALID);
        assert_se(personality_from_string(NULL) == PERSONALITY_INVALID);

        assert_se(personality_from_string(personality_to_string(PER_LINUX32)) == PER_LINUX32);
#endif
}
#endif // 0

static void test_get_process_cmdline_harder(void) {
        char path[] = "/tmp/test-cmdlineXXXXXX";
        _cleanup_close_ int fd = -1;
        _cleanup_free_ char *line = NULL;
        pid_t pid;

        if (geteuid() != 0)
                return;

#if HAVE_VALGRIND_VALGRIND_H
        /* valgrind patches open(/proc//cmdline)
         * so, test_get_process_cmdline_harder fails always
         * See https://github.com/systemd/systemd/pull/3555#issuecomment-226564908 */
        if (RUNNING_ON_VALGRIND)
                return;
#endif

        pid = fork();
        if (pid > 0) {
                siginfo_t si;

                (void) wait_for_terminate(pid, &si);

                assert_se(si.si_code == CLD_EXITED);
                assert_se(si.si_status == 0);

                return;
        }

        assert_se(pid == 0);
        assert_se(unshare(CLONE_NEWNS) >= 0);

        assert_se(mount(NULL, "/", NULL, MS_PRIVATE|MS_REC, NULL) >= 0);

        fd = mkostemp(path, O_CLOEXEC);
        assert_se(fd >= 0);

        if (mount(path, "/proc/self/cmdline", "bind", MS_BIND, NULL) < 0) {
                /* This happens under selinux… Abort the test in this case. */
                log_warning_errno(errno, "mount(..., \"/proc/self/cmdline\", \"bind\", ...) failed: %m");
                assert(errno == EACCES);
                return;
        }

        assert_se(unlink(path) >= 0);

        assert_se(prctl(PR_SET_NAME, "testa") >= 0);

        assert_se(get_process_cmdline(getpid_cached(), 0, false, &line) == -ENOENT);

        assert_se(get_process_cmdline(getpid_cached(), 0, true, &line) >= 0);
        assert_se(streq(line, "[testa]"));
        line = mfree(line);

        assert_se(get_process_cmdline(getpid_cached(), 1, true, &line) >= 0);
        assert_se(streq(line, ""));
        line = mfree(line);

        assert_se(get_process_cmdline(getpid_cached(), 2, true, &line) >= 0);
        assert_se(streq(line, "["));
        line = mfree(line);

        assert_se(get_process_cmdline(getpid_cached(), 3, true, &line) >= 0);
        assert_se(streq(line, "[."));
        line = mfree(line);

        assert_se(get_process_cmdline(getpid_cached(), 4, true, &line) >= 0);
        assert_se(streq(line, "[.."));
        line = mfree(line);

        assert_se(get_process_cmdline(getpid_cached(), 5, true, &line) >= 0);
        assert_se(streq(line, "[..."));
        line = mfree(line);

        assert_se(get_process_cmdline(getpid_cached(), 6, true, &line) >= 0);
        assert_se(streq(line, "[...]"));
        line = mfree(line);

        assert_se(get_process_cmdline(getpid_cached(), 7, true, &line) >= 0);
        assert_se(streq(line, "[t...]"));
        line = mfree(line);

        assert_se(get_process_cmdline(getpid_cached(), 8, true, &line) >= 0);
        assert_se(streq(line, "[testa]"));
        line = mfree(line);

        assert_se(write(fd, "\0\0\0\0\0\0\0\0\0", 10) == 10);

        assert_se(get_process_cmdline(getpid_cached(), 0, false, &line) == -ENOENT);

        assert_se(get_process_cmdline(getpid_cached(), 0, true, &line) >= 0);
        assert_se(streq(line, "[testa]"));
        line = mfree(line);

        assert_se(write(fd, "foo\0bar\0\0\0\0\0", 10) == 10);

        assert_se(get_process_cmdline(getpid_cached(), 0, false, &line) >= 0);
        assert_se(streq(line, "foo bar"));
        line = mfree(line);

        assert_se(get_process_cmdline(getpid_cached(), 0, true, &line) >= 0);
        assert_se(streq(line, "foo bar"));
        line = mfree(line);

        assert_se(write(fd, "quux", 4) == 4);
        assert_se(get_process_cmdline(getpid_cached(), 0, false, &line) >= 0);
        assert_se(streq(line, "foo bar quux"));
        line = mfree(line);

        assert_se(get_process_cmdline(getpid_cached(), 0, true, &line) >= 0);
        assert_se(streq(line, "foo bar quux"));
        line = mfree(line);

        assert_se(get_process_cmdline(getpid_cached(), 1, true, &line) >= 0);
        assert_se(streq(line, ""));
        line = mfree(line);

        assert_se(get_process_cmdline(getpid_cached(), 2, true, &line) >= 0);
        assert_se(streq(line, "."));
        line = mfree(line);

        assert_se(get_process_cmdline(getpid_cached(), 3, true, &line) >= 0);
        assert_se(streq(line, ".."));
        line = mfree(line);

        assert_se(get_process_cmdline(getpid_cached(), 4, true, &line) >= 0);
        assert_se(streq(line, "..."));
        line = mfree(line);

        assert_se(get_process_cmdline(getpid_cached(), 5, true, &line) >= 0);
        assert_se(streq(line, "f..."));
        line = mfree(line);

        assert_se(get_process_cmdline(getpid_cached(), 6, true, &line) >= 0);
        assert_se(streq(line, "fo..."));
        line = mfree(line);

        assert_se(get_process_cmdline(getpid_cached(), 7, true, &line) >= 0);
        assert_se(streq(line, "foo..."));
        line = mfree(line);

        assert_se(get_process_cmdline(getpid_cached(), 8, true, &line) >= 0);
        assert_se(streq(line, "foo..."));
        line = mfree(line);

        assert_se(get_process_cmdline(getpid_cached(), 9, true, &line) >= 0);
        assert_se(streq(line, "foo b..."));
        line = mfree(line);

        assert_se(get_process_cmdline(getpid_cached(), 10, true, &line) >= 0);
        assert_se(streq(line, "foo ba..."));
        line = mfree(line);

        assert_se(get_process_cmdline(getpid_cached(), 11, true, &line) >= 0);
        assert_se(streq(line, "foo bar..."));
        line = mfree(line);

        assert_se(get_process_cmdline(getpid_cached(), 12, true, &line) >= 0);
        assert_se(streq(line, "foo bar..."));
        line = mfree(line);

        assert_se(get_process_cmdline(getpid_cached(), 13, true, &line) >= 0);
        assert_se(streq(line, "foo bar quux"));
        line = mfree(line);

        assert_se(get_process_cmdline(getpid_cached(), 14, true, &line) >= 0);
        assert_se(streq(line, "foo bar quux"));
        line = mfree(line);

        assert_se(get_process_cmdline(getpid_cached(), 1000, true, &line) >= 0);
        assert_se(streq(line, "foo bar quux"));
        line = mfree(line);

        assert_se(ftruncate(fd, 0) >= 0);
        assert_se(prctl(PR_SET_NAME, "aaaa bbbb cccc") >= 0);

        assert_se(get_process_cmdline(getpid_cached(), 0, false, &line) == -ENOENT);

        assert_se(get_process_cmdline(getpid_cached(), 0, true, &line) >= 0);
        assert_se(streq(line, "[aaaa bbbb cccc]"));
        line = mfree(line);

        assert_se(get_process_cmdline(getpid_cached(), 10, true, &line) >= 0);
        assert_se(streq(line, "[aaaa...]"));
        line = mfree(line);

        assert_se(get_process_cmdline(getpid_cached(), 11, true, &line) >= 0);
        assert_se(streq(line, "[aaaa...]"));
        line = mfree(line);

        assert_se(get_process_cmdline(getpid_cached(), 12, true, &line) >= 0);
        assert_se(streq(line, "[aaaa b...]"));
        line = mfree(line);

        safe_close(fd);
        _exit(EXIT_SUCCESS);
}

#if 0 /// UNNEEDED by elogind
static void test_rename_process_now(const char *p, int ret) {
        _cleanup_free_ char *comm = NULL, *cmdline = NULL;
        int r;

        r = rename_process(p);
        assert_se(r == ret ||
                  (ret == 0 && r >= 0) ||
                  (ret > 0 && r > 0));

        if (r < 0)
                return;

#if HAVE_VALGRIND_VALGRIND_H
        /* see above, valgrind is weird, we can't verify what we are doing here */
        if (RUNNING_ON_VALGRIND)
                return;
#endif

        assert_se(get_process_comm(0, &comm) >= 0);
        log_info("comm = <%s>", comm);
        assert_se(strneq(comm, p, TASK_COMM_LEN-1));

        assert_se(get_process_cmdline(0, 0, false, &cmdline) >= 0);
        /* we cannot expect cmdline to be renamed properly without privileges */
        if (geteuid() == 0) {
                log_info("cmdline = <%s>", cmdline);
                assert_se(strneq(p, cmdline, STRLEN("test-process-util")));
                assert_se(startswith(p, cmdline));
        } else
                log_info("cmdline = <%s> (not verified)", cmdline);
}

static void test_rename_process_one(const char *p, int ret) {
        siginfo_t si;
        pid_t pid;

        pid = fork();
        assert_se(pid >= 0);

        if (pid == 0) {
                /* child */
                test_rename_process_now(p, ret);
                _exit(EXIT_SUCCESS);
        }

        assert_se(wait_for_terminate(pid, &si) >= 0);
        assert_se(si.si_code == CLD_EXITED);
        assert_se(si.si_status == EXIT_SUCCESS);
}

static void test_rename_process_multi(void) {
        pid_t pid;

        pid = fork();
        assert_se(pid >= 0);

        if (pid > 0) {
                siginfo_t si;

                assert_se(wait_for_terminate(pid, &si) >= 0);
                assert_se(si.si_code == CLD_EXITED);
                assert_se(si.si_status == EXIT_SUCCESS);

                return;
        }

        /* child */
        test_rename_process_now("one", 1);
        test_rename_process_now("more", 0); /* longer than "one", hence truncated */
        (void) setresuid(99, 99, 99); /* change uid when running privileged */
        test_rename_process_now("time!", 0);
        test_rename_process_now("0", 1); /* shorter than "one", should fit */
        test_rename_process_one("", -EINVAL);
        test_rename_process_one(NULL, -EINVAL);
        _exit(EXIT_SUCCESS);
}

static void test_rename_process(void) {
        test_rename_process_one(NULL, -EINVAL);
        test_rename_process_one("", -EINVAL);
        test_rename_process_one("foo", 1); /* should always fit */
        test_rename_process_one("this is a really really long process name, followed by some more words", 0); /* unlikely to fit */
        test_rename_process_one("1234567", 1); /* should always fit */
        test_rename_process_multi(); /* multiple invocations and dropped privileges */
}
#endif // 0

static void test_getpid_cached(void) {
        siginfo_t si;
        pid_t a, b, c, d, e, f, child;

        a = raw_getpid();
        b = getpid_cached();
        c = getpid();

        assert_se(a == b && a == c);

        child = fork();
        assert_se(child >= 0);

        if (child == 0) {
                /* In child */
                a = raw_getpid();
                b = getpid_cached();
                c = getpid();

                assert_se(a == b && a == c);
                _exit(EXIT_SUCCESS);
        }

        d = raw_getpid();
        e = getpid_cached();
        f = getpid();

        assert_se(a == d && a == e && a == f);

        assert_se(wait_for_terminate(child, &si) >= 0);
        assert_se(si.si_status == 0);
        assert_se(si.si_code == CLD_EXITED);
}

#define MEASURE_ITERATIONS (10000000LLU)

static void test_getpid_measure(void) {
        unsigned long long i;
        usec_t t, q;

        t = now(CLOCK_MONOTONIC);
        for (i = 0; i < MEASURE_ITERATIONS; i++)
                (void) getpid();
        q = now(CLOCK_MONOTONIC) - t;

        log_info(" glibc getpid(): %llu/s\n", (unsigned long long) (MEASURE_ITERATIONS*USEC_PER_SEC/q));

        t = now(CLOCK_MONOTONIC);
        for (i = 0; i < MEASURE_ITERATIONS; i++)
                (void) getpid_cached();
        q = now(CLOCK_MONOTONIC) - t;

        log_info("getpid_cached(): %llu/s\n", (unsigned long long) (MEASURE_ITERATIONS*USEC_PER_SEC/q));
}

static void test_safe_fork(void) {
        siginfo_t status;
        pid_t pid;
        int r;

        BLOCK_SIGNALS(SIGCHLD);

        r = safe_fork("(test-child)", FORK_RESET_SIGNALS|FORK_CLOSE_ALL_FDS|FORK_DEATHSIG|FORK_NULL_STDIO|FORK_REOPEN_LOG, &pid);
        assert_se(r >= 0);

        if (r == 0) {
                /* child */
                usleep(100 * USEC_PER_MSEC);

                _exit(88);
        }

        assert_se(wait_for_terminate(pid, &status) >= 0);
        assert_se(status.si_code == CLD_EXITED);
        assert_se(status.si_status == 88);
}

static void test_pid_to_ptr(void) {

        assert_se(PTR_TO_PID(NULL) == 0);
        assert_se(PID_TO_PTR(0) == NULL);

        assert_se(PTR_TO_PID(PID_TO_PTR(1)) == 1);
        assert_se(PTR_TO_PID(PID_TO_PTR(2)) == 2);
        assert_se(PTR_TO_PID(PID_TO_PTR(-1)) == -1);
        assert_se(PTR_TO_PID(PID_TO_PTR(-2)) == -2);

        assert_se(PTR_TO_PID(PID_TO_PTR(INT16_MAX)) == INT16_MAX);
        assert_se(PTR_TO_PID(PID_TO_PTR(INT16_MIN)) == INT16_MIN);

#if SIZEOF_PID_T >= 4
        assert_se(PTR_TO_PID(PID_TO_PTR(INT32_MAX)) == INT32_MAX);
        assert_se(PTR_TO_PID(PID_TO_PTR(INT32_MIN)) == INT32_MIN);
#endif
}

static void test_ioprio_class_from_to_string_one(const char *val, int expected) {
        assert_se(ioprio_class_from_string(val) == expected);
        if (expected >= 0) {
                _cleanup_free_ char *s = NULL;
                unsigned ret;

                assert_se(ioprio_class_to_string_alloc(expected, &s) == 0);
                /* We sometimes get a class number and sometimes a number back */
                assert_se(streq(s, val) ||
                          safe_atou(val, &ret) == 0);
        }
}

static void test_ioprio_class_from_to_string(void) {
        test_ioprio_class_from_to_string_one("none", IOPRIO_CLASS_NONE);
        test_ioprio_class_from_to_string_one("realtime", IOPRIO_CLASS_RT);
        test_ioprio_class_from_to_string_one("best-effort", IOPRIO_CLASS_BE);
        test_ioprio_class_from_to_string_one("idle", IOPRIO_CLASS_IDLE);
        test_ioprio_class_from_to_string_one("0", 0);
        test_ioprio_class_from_to_string_one("1", 1);
        test_ioprio_class_from_to_string_one("7", 7);
        test_ioprio_class_from_to_string_one("8", 8);
        test_ioprio_class_from_to_string_one("9", -1);
        test_ioprio_class_from_to_string_one("-1", -1);
}

int main(int argc, char *argv[]) {
        log_set_max_level(LOG_DEBUG);
        log_parse_environment();
        log_open();

        saved_argc = argc;
        saved_argv = argv;

        if (argc > 1) {
                pid_t pid = 0;

                (void) parse_pid(argv[1], &pid);
                test_get_process_comm(pid);
        } else {
                TEST_REQ_RUNNING_SYSTEMD(test_get_process_comm(1));
                test_get_process_comm(getpid());
        }

        test_pid_is_unwaited();
        test_pid_is_alive();
#if 0 /// UNNEEDED by elogind
        test_personality();
#endif // 0
        test_get_process_cmdline_harder();
#if 0 /// UNNEEDED by elogind
        test_rename_process();
#endif // 0
        test_getpid_cached();
        test_getpid_measure();
        test_safe_fork();
        test_pid_to_ptr();
        test_ioprio_class_from_to_string();

        return 0;
}
