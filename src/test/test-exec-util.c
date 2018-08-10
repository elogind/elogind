/* SPDX-License-Identifier: LGPL-2.1+ */

#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "alloc-util.h"
#include "copy.h"
#include "def.h"
#include "env-util.h"
#include "exec-util.h"
#include "fd-util.h"
#include "fileio.h"
#include "fs-util.h"
#include "log.h"
#include "macro.h"
#include "rm-rf.h"
#include "string-util.h"
#include "strv.h"

static int here = 0, here2 = 0, here3 = 0;
void *ignore_stdout_args[] = {&here, &here2, &here3};

/* noop handlers, just check that arguments are passed correctly */
static int ignore_stdout_func(int fd, void *arg) {
        assert(fd >= 0);
        assert(arg == &here);
        safe_close(fd);

        return 0;
}
static int ignore_stdout_func2(int fd, void *arg) {
        assert(fd >= 0);
        assert(arg == &here2);
        safe_close(fd);

        return 0;
}
static int ignore_stdout_func3(int fd, void *arg) {
        assert(fd >= 0);
        assert(arg == &here3);
        safe_close(fd);

        return 0;
}

static const gather_stdout_callback_t ignore_stdout[] = {
        ignore_stdout_func,
        ignore_stdout_func2,
        ignore_stdout_func3,
};

static void test_execute_directory(bool gather_stdout) {
        char template_lo[] = "/tmp/test-exec-util.lo.XXXXXXX";
        char template_hi[] = "/tmp/test-exec-util.hi.XXXXXXX";
        const char * dirs[] = {template_hi, template_lo, NULL};
        const char *name, *name2, *name3,
                *overridden, *override,
                *masked, *mask,
                *masked2, *mask2,   /* the mask is non-executable */
                *masked2e, *mask2e; /* the mask is executable */

        log_info("/* %s (%s) */", __func__, gather_stdout ? "gathering stdout" : "asynchronous");

        assert_se(mkdtemp(template_lo));
        assert_se(mkdtemp(template_hi));

        name = strjoina(template_lo, "/script");
        name2 = strjoina(template_hi, "/script2");
        name3 = strjoina(template_lo, "/useless");
        overridden = strjoina(template_lo, "/overridden");
        override = strjoina(template_hi, "/overridden");
        masked = strjoina(template_lo, "/masked");
        mask = strjoina(template_hi, "/masked");
        masked2 = strjoina(template_lo, "/masked2");
        mask2 = strjoina(template_hi, "/masked2");
        masked2e = strjoina(template_lo, "/masked2e");
        mask2e = strjoina(template_hi, "/masked2e");

        assert_se(write_string_file(name,
                                    "#!/bin/sh\necho 'Executing '$0\ntouch $(dirname $0)/it_works",
                                    WRITE_STRING_FILE_CREATE) == 0);
        assert_se(write_string_file(name2,
                                    "#!/bin/sh\necho 'Executing '$0\ntouch $(dirname $0)/it_works2",
                                    WRITE_STRING_FILE_CREATE) == 0);
        assert_se(write_string_file(overridden,
                                    "#!/bin/sh\necho 'Executing '$0\ntouch $(dirname $0)/failed",
                                    WRITE_STRING_FILE_CREATE) == 0);
        assert_se(write_string_file(override,
                                    "#!/bin/sh\necho 'Executing '$0",
                                    WRITE_STRING_FILE_CREATE) == 0);
        assert_se(write_string_file(masked,
                                    "#!/bin/sh\necho 'Executing '$0\ntouch $(dirname $0)/failed",
                                    WRITE_STRING_FILE_CREATE) == 0);
        assert_se(write_string_file(masked2,
                                    "#!/bin/sh\necho 'Executing '$0\ntouch $(dirname $0)/failed",
                                    WRITE_STRING_FILE_CREATE) == 0);
        assert_se(write_string_file(masked2e,
                                    "#!/bin/sh\necho 'Executing '$0\ntouch $(dirname $0)/failed",
                                    WRITE_STRING_FILE_CREATE) == 0);
        assert_se(symlink("/dev/null", mask) == 0);
        assert_se(touch(mask2) == 0);
        assert_se(touch(mask2e) == 0);
        assert_se(touch(name3) >= 0);

        assert_se(chmod(name, 0755) == 0);
        assert_se(chmod(name2, 0755) == 0);
        assert_se(chmod(overridden, 0755) == 0);
        assert_se(chmod(override, 0755) == 0);
        assert_se(chmod(masked, 0755) == 0);
        assert_se(chmod(masked2, 0755) == 0);
        assert_se(chmod(masked2e, 0755) == 0);
        assert_se(chmod(mask2e, 0755) == 0);

        if (gather_stdout)
                execute_directories(dirs, DEFAULT_TIMEOUT_USEC, ignore_stdout, ignore_stdout_args, NULL);
        else
                execute_directories(dirs, DEFAULT_TIMEOUT_USEC, NULL, NULL, NULL);

        assert_se(chdir(template_lo) == 0);
        assert_se(access("it_works", F_OK) >= 0);
        assert_se(access("failed", F_OK) < 0);

        assert_se(chdir(template_hi) == 0);
        assert_se(access("it_works2", F_OK) >= 0);
        assert_se(access("failed", F_OK) < 0);

        (void) rm_rf(template_lo, REMOVE_ROOT|REMOVE_PHYSICAL);
        (void) rm_rf(template_hi, REMOVE_ROOT|REMOVE_PHYSICAL);
}

static void test_execution_order(void) {
        char template_lo[] = "/tmp/test-exec-util-lo.XXXXXXX";
        char template_hi[] = "/tmp/test-exec-util-hi.XXXXXXX";
        const char *dirs[] = {template_hi, template_lo, NULL};
        const char *name, *name2, *name3, *overridden, *override, *masked, *mask;
        const char *output, *t;
        _cleanup_free_ char *contents = NULL;

        assert_se(mkdtemp(template_lo));
        assert_se(mkdtemp(template_hi));

        output = strjoina(template_hi, "/output");

        log_info("/* %s >>%s */", __func__, output);

        /* write files in "random" order */
        name2 = strjoina(template_lo, "/90-bar");
        name = strjoina(template_hi, "/80-foo");
        name3 = strjoina(template_lo, "/last");
        overridden = strjoina(template_lo, "/30-override");
        override = strjoina(template_hi, "/30-override");
        masked = strjoina(template_lo, "/10-masked");
        mask = strjoina(template_hi, "/10-masked");

        t = strjoina("#!/bin/sh\necho $(basename $0) >>", output);
        assert_se(write_string_file(name, t, WRITE_STRING_FILE_CREATE) == 0);

        t = strjoina("#!/bin/sh\necho $(basename $0) >>", output);
        assert_se(write_string_file(name2, t, WRITE_STRING_FILE_CREATE) == 0);

        t = strjoina("#!/bin/sh\necho $(basename $0) >>", output);
        assert_se(write_string_file(name3, t, WRITE_STRING_FILE_CREATE) == 0);

        t = strjoina("#!/bin/sh\necho OVERRIDDEN >>", output);
        assert_se(write_string_file(overridden, t, WRITE_STRING_FILE_CREATE) == 0);

        t = strjoina("#!/bin/sh\necho $(basename $0) >>", output);
        assert_se(write_string_file(override, t, WRITE_STRING_FILE_CREATE) == 0);

        t = strjoina("#!/bin/sh\necho MASKED >>", output);
        assert_se(write_string_file(masked, t, WRITE_STRING_FILE_CREATE) == 0);

        assert_se(symlink("/dev/null", mask) == 0);

        assert_se(chmod(name, 0755) == 0);
        assert_se(chmod(name2, 0755) == 0);
        assert_se(chmod(name3, 0755) == 0);
        assert_se(chmod(overridden, 0755) == 0);
        assert_se(chmod(override, 0755) == 0);
        assert_se(chmod(masked, 0755) == 0);

        execute_directories(dirs, DEFAULT_TIMEOUT_USEC, ignore_stdout, ignore_stdout_args, NULL);

        assert_se(read_full_file(output, &contents, NULL) >= 0);
        assert_se(streq(contents, "30-override\n80-foo\n90-bar\nlast\n"));

        (void) rm_rf(template_lo, REMOVE_ROOT|REMOVE_PHYSICAL);
        (void) rm_rf(template_hi, REMOVE_ROOT|REMOVE_PHYSICAL);
}

static int gather_stdout_one(int fd, void *arg) {
        char ***s = arg, *t;
        char buf[128] = {};

        assert_se(s);
        assert_se(read(fd, buf, sizeof buf) >= 0);
        safe_close(fd);

        assert_se(t = strndup(buf, sizeof buf));
        assert_se(strv_push(s, t) >= 0);

        return 0;
}
static int gather_stdout_two(int fd, void *arg) {
        char ***s = arg, **t;

        STRV_FOREACH(t, *s)
                assert_se(write(fd, *t, strlen(*t)) == (ssize_t) strlen(*t));
        safe_close(fd);

        return 0;
}
static int gather_stdout_three(int fd, void *arg) {
        char **s = arg;
        char buf[128] = {};

        assert_se(read(fd, buf, sizeof buf - 1) > 0);
        safe_close(fd);
        assert_se(*s = strndup(buf, sizeof buf));

        return 0;
}

const gather_stdout_callback_t gather_stdout[] = {
        gather_stdout_one,
        gather_stdout_two,
        gather_stdout_three,
};

static void test_stdout_gathering(void) {
        char template[] = "/tmp/test-exec-util.XXXXXXX";
        const char *dirs[] = {template, NULL};
        const char *name, *name2, *name3;
        int r;

        char **tmp = NULL; /* this is only used in the forked process, no cleanup here */
        _cleanup_free_ char *output = NULL;

        void* args[] = {&tmp, &tmp, &output};

        assert_se(mkdtemp(template));

        log_info("/* %s */", __func__);

        /* write files */
        name = strjoina(template, "/10-foo");
        name2 = strjoina(template, "/20-bar");
        name3 = strjoina(template, "/30-last");

        assert_se(write_string_file(name,
                                    "#!/bin/sh\necho a\necho b\necho c\n",
                                    WRITE_STRING_FILE_CREATE) == 0);
        assert_se(write_string_file(name2,
                                    "#!/bin/sh\necho d\n",
                                    WRITE_STRING_FILE_CREATE) == 0);
        assert_se(write_string_file(name3,
                                    "#!/bin/sh\nsleep 1",
                                    WRITE_STRING_FILE_CREATE) == 0);

        assert_se(chmod(name, 0755) == 0);
        assert_se(chmod(name2, 0755) == 0);
        assert_se(chmod(name3, 0755) == 0);

        r = execute_directories(dirs, DEFAULT_TIMEOUT_USEC, gather_stdout, args, NULL);
        assert_se(r >= 0);

        log_info("got: %s", output);

        assert_se(streq(output, "a\nb\nc\nd\n"));
}

#if 0 /// UNNEEDED by elogind
static void test_environment_gathering(void) {
        char template[] = "/tmp/test-exec-util.XXXXXXX", **p;
        const char *dirs[] = {template, NULL};
        const char *name, *name2, *name3;
        int r;

        char **tmp = NULL; /* this is only used in the forked process, no cleanup here */
        _cleanup_strv_free_ char **env = NULL;

        void* const args[] = { &tmp, &tmp, &env };

        assert_se(mkdtemp(template));

        log_info("/* %s */", __func__);

        /* write files */
        name = strjoina(template, "/10-foo");
        name2 = strjoina(template, "/20-bar");
        name3 = strjoina(template, "/30-last");

        assert_se(write_string_file(name,
                                    "#!/bin/sh\n"
                                    "echo A=23\n",
                                    WRITE_STRING_FILE_CREATE) == 0);
        assert_se(write_string_file(name2,
                                    "#!/bin/sh\n"
                                    "echo A=22:$A\n\n\n",            /* substitution from previous generator */
                                    WRITE_STRING_FILE_CREATE) == 0);
        assert_se(write_string_file(name3,
                                    "#!/bin/sh\n"
                                    "echo A=$A:24\n"
                                    "echo B=12\n"
                                    "echo C=000\n"
                                    "echo C=001\n"                    /* variable overwriting */
                                     /* various invalid entries */
                                    "echo unset A\n"
                                    "echo unset A=\n"
                                    "echo unset A=B\n"
                                    "echo unset \n"
                                    "echo A B=C\n"
                                    "echo A\n"
                                    /* test variable assignment without newline */
                                    "echo PATH=$PATH:/no/such/file",   /* no newline */
                                    WRITE_STRING_FILE_CREATE) == 0);

        assert_se(chmod(name, 0755) == 0);
        assert_se(chmod(name2, 0755) == 0);
        assert_se(chmod(name3, 0755) == 0);

        r = execute_directories(dirs, DEFAULT_TIMEOUT_USEC, gather_environment, args, NULL);
        assert_se(r >= 0);

        STRV_FOREACH(p, env)
                log_info("got env: \"%s\"", *p);

        assert_se(streq(strv_env_get(env, "A"), "22:23:24"));
        assert_se(streq(strv_env_get(env, "B"), "12"));
        assert_se(streq(strv_env_get(env, "C"), "001"));
        assert_se(endswith(strv_env_get(env, "PATH"), ":/no/such/file"));
}
#endif // 0

int main(int argc, char *argv[]) {
        log_set_max_level(LOG_DEBUG);
        log_parse_environment();
        log_open();

        test_execute_directory(true);
        test_execute_directory(false);
        test_execution_order();
        test_stdout_gathering();
#if 0 /// UNNEEDED by elogind
        test_environment_gathering();
#endif // 0

        return 0;
}
