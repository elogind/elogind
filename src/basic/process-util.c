/***
  This file is part of systemd.

  Copyright 2010 Lennart Poettering

  systemd is free software; you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation; either version 2.1 of the License, or
  (at your option) any later version.

  systemd is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with systemd; If not, see <http://www.gnu.org/licenses/>.
***/

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <linux/oom.h>
#include <sched.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/personality.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>
#if HAVE_VALGRIND_VALGRIND_H
#include <valgrind/valgrind.h>
#endif

#include "alloc-util.h"
//#include "architecture.h"
#include "escape.h"
#include "fd-util.h"
#include "fileio.h"
#include "fs-util.h"
//#include "ioprio.h"
#include "log.h"
#include "macro.h"
#include "missing.h"
#include "process-util.h"
//#include "raw-clone.h"
#include "signal-util.h"
//#include "stat-util.h"
#include "string-table.h"
#include "string-util.h"
#include "user-util.h"
#include "util.h"

int get_process_state(pid_t pid) {
        const char *p;
        char state;
        int r;
        _cleanup_free_ char *line = NULL;

        assert(pid >= 0);

        p = procfs_file_alloca(pid, "stat");

        r = read_one_line_file(p, &line);
        if (r == -ENOENT)
                return -ESRCH;
        if (r < 0)
                return r;

        p = strrchr(line, ')');
        if (!p)
                return -EIO;

        p++;

        if (sscanf(p, " %c", &state) != 1)
                return -EIO;

        return (unsigned char) state;
}

int get_process_comm(pid_t pid, char **name) {
        const char *p;
        int r;

        assert(name);
        assert(pid >= 0);

        p = procfs_file_alloca(pid, "comm");

        r = read_one_line_file(p, name);
        if (r == -ENOENT)
                return -ESRCH;

        return r;
}

int get_process_cmdline(pid_t pid, size_t max_length, bool comm_fallback, char **line) {
        _cleanup_fclose_ FILE *f = NULL;
        bool space = false;
        char *k, *ans = NULL;
        const char *p;
        int c;

        assert(line);
        assert(pid >= 0);

        /* Retrieves a process' command line. Replaces unprintable characters while doing so by whitespace (coalescing
         * multiple sequential ones into one). If max_length is != 0 will return a string of the specified size at most
         * (the trailing NUL byte does count towards the length here!), abbreviated with a "..." ellipsis. If
         * comm_fallback is true and the process has no command line set (the case for kernel threads), or has a
         * command line that resolves to the empty string will return the "comm" name of the process instead.
         *
         * Returns -ESRCH if the process doesn't exist, and -ENOENT if the process has no command line (and
         * comm_fallback is false). Returns 0 and sets *line otherwise. */

        p = procfs_file_alloca(pid, "cmdline");

        f = fopen(p, "re");
        if (!f) {
                if (errno == ENOENT)
                        return -ESRCH;
                return -errno;
        }

        if (max_length == 1) {

                /* If there's only room for one byte, return the empty string */
                ans = new0(char, 1);
                if (!ans)
                        return -ENOMEM;

                *line = ans;
                return 0;

        } else if (max_length == 0) {
                size_t len = 0, allocated = 0;

                while ((c = getc(f)) != EOF) {

                        if (!GREEDY_REALLOC(ans, allocated, len+3)) {
                                free(ans);
                                return -ENOMEM;
                        }

                        if (isprint(c)) {
                                if (space) {
                                        ans[len++] = ' ';
                                        space = false;
                                }

                                ans[len++] = c;
                        } else if (len > 0)
                                space = true;
               }

                if (len > 0)
                        ans[len] = '\0';
                else
                        ans = mfree(ans);

        } else {
                bool dotdotdot = false;
                size_t left;

                ans = new(char, max_length);
                if (!ans)
                        return -ENOMEM;

                k = ans;
                left = max_length;
                while ((c = getc(f)) != EOF) {

                        if (isprint(c)) {

                                if (space) {
                                        if (left <= 2) {
                                                dotdotdot = true;
                                                break;
                                        }

                                        *(k++) = ' ';
                                        left--;
                                        space = false;
                                }

                                if (left <= 1) {
                                        dotdotdot = true;
                                        break;
                                }

                                *(k++) = (char) c;
                                left--;
                        } else if (k > ans)
                                space = true;
                }

                if (dotdotdot) {
                        if (max_length <= 4) {
                                k = ans;
                                left = max_length;
                        } else {
                                k = ans + max_length - 4;
                                left = 4;

                                /* Eat up final spaces */
                                while (k > ans && isspace(k[-1])) {
                                        k--;
                                        left++;
                                }
                        }

                        strncpy(k, "...", left-1);
                        k[left-1] = 0;
                } else
                        *k = 0;
        }

        /* Kernel threads have no argv[] */
        if (isempty(ans)) {
                _cleanup_free_ char *t = NULL;
                int h;

                free(ans);

                if (!comm_fallback)
                        return -ENOENT;

                h = get_process_comm(pid, &t);
                if (h < 0)
                        return h;

                if (max_length == 0)
                        ans = strjoin("[", t, "]");
                else {
                        size_t l;

                        l = strlen(t);

                        if (l + 3 <= max_length)
                                ans = strjoin("[", t, "]");
                        else if (max_length <= 6) {

                                ans = new(char, max_length);
                                if (!ans)
                                        return -ENOMEM;

                                memcpy(ans, "[...]", max_length-1);
                                ans[max_length-1] = 0;
                        } else {
                                char *e;

                                t[max_length - 6] = 0;

                                /* Chop off final spaces */
                                e = strchr(t, 0);
                                while (e > t && isspace(e[-1]))
                                        e--;
                                *e = 0;

                                ans = strjoin("[", t, "...]");
                        }
                }
                if (!ans)
                        return -ENOMEM;
        }

        *line = ans;
        return 0;
}

#if 0 /// UNNEEDED by elogind
int rename_process(const char name[]) {
        static size_t mm_size = 0;
        static char *mm = NULL;
        bool truncated = false;
        size_t l;

        /* This is a like a poor man's setproctitle(). It changes the comm field, argv[0], and also the glibc's
         * internally used name of the process. For the first one a limit of 16 chars applies; to the second one in
         * many cases one of 10 (i.e. length of "/sbin/init") — however if we have CAP_SYS_RESOURCES it is unbounded;
         * to the third one 7 (i.e. the length of "systemd". If you pass a longer string it will likely be
         * truncated.
         *
         * Returns 0 if a name was set but truncated, > 0 if it was set but not truncated. */

        if (isempty(name))
                return -EINVAL; /* let's not confuse users unnecessarily with an empty name */

        l = strlen(name);

        /* First step, change the comm field. */
        (void) prctl(PR_SET_NAME, name);
        if (l > 15) /* Linux process names can be 15 chars at max */
                truncated = true;

        /* Second step, change glibc's ID of the process name. */
        if (program_invocation_name) {
                size_t k;

                k = strlen(program_invocation_name);
                strncpy(program_invocation_name, name, k);
                if (l > k)
                        truncated = true;
        }

        /* Third step, completely replace the argv[] array the kernel maintains for us. This requires privileges, but
         * has the advantage that the argv[] array is exactly what we want it to be, and not filled up with zeros at
         * the end. This is the best option for changing /proc/self/cmdline. */

        /* Let's not bother with this if we don't have euid == 0. Strictly speaking we should check for the
         * CAP_SYS_RESOURCE capability which is independent of the euid. In our own code the capability generally is
         * present only for euid == 0, hence let's use this as quick bypass check, to avoid calling mmap() if
         * PR_SET_MM_ARG_{START,END} fails with EPERM later on anyway. After all geteuid() is dead cheap to call, but
         * mmap() is not. */
        if (geteuid() != 0)
                log_debug("Skipping PR_SET_MM, as we don't have privileges.");
        else if (mm_size < l+1) {
                size_t nn_size;
                char *nn;

                nn_size = PAGE_ALIGN(l+1);
                nn = mmap(NULL, nn_size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
                if (nn == MAP_FAILED) {
                        log_debug_errno(errno, "mmap() failed: %m");
                        goto use_saved_argv;
                }

                strncpy(nn, name, nn_size);

                /* Now, let's tell the kernel about this new memory */
                if (prctl(PR_SET_MM, PR_SET_MM_ARG_START, (unsigned long) nn, 0, 0) < 0) {
                        log_debug_errno(errno, "PR_SET_MM_ARG_START failed, proceeding without: %m");
                        (void) munmap(nn, nn_size);
                        goto use_saved_argv;
                }

                /* And update the end pointer to the new end, too. If this fails, we don't really know what to do, it's
                 * pretty unlikely that we can rollback, hence we'll just accept the failure, and continue. */
                if (prctl(PR_SET_MM, PR_SET_MM_ARG_END, (unsigned long) nn + l + 1, 0, 0) < 0)
                        log_debug_errno(errno, "PR_SET_MM_ARG_END failed, proceeding without: %m");

                if (mm)
                        (void) munmap(mm, mm_size);

                mm = nn;
                mm_size = nn_size;
        } else {
                strncpy(mm, name, mm_size);

                /* Update the end pointer, continuing regardless of any failure. */
                if (prctl(PR_SET_MM, PR_SET_MM_ARG_END, (unsigned long) mm + l + 1, 0, 0) < 0)
                        log_debug_errno(errno, "PR_SET_MM_ARG_END failed, proceeding without: %m");
        }

use_saved_argv:
        /* Fourth step: in all cases we'll also update the original argv[], so that our own code gets it right too if
         * it still looks here */

        if (saved_argc > 0) {
                int i;

                if (saved_argv[0]) {
                        size_t k;

                        k = strlen(saved_argv[0]);
                        strncpy(saved_argv[0], name, k);
                        if (l > k)
                                truncated = true;
                }

                for (i = 1; i < saved_argc; i++) {
                        if (!saved_argv[i])
                                break;

                        memzero(saved_argv[i], strlen(saved_argv[i]));
                }
        }

        return !truncated;
}
#endif // 0

int is_kernel_thread(pid_t pid) {
        const char *p;
        size_t count;
        char c;
        bool eof;
        FILE *f;

        if (IN_SET(pid, 0, 1) || pid == getpid_cached()) /* pid 1, and we ourselves certainly aren't a kernel thread */
                return 0;

        assert(pid > 1);

        p = procfs_file_alloca(pid, "cmdline");
        f = fopen(p, "re");
        if (!f) {
                if (errno == ENOENT)
                        return -ESRCH;
                return -errno;
        }

        count = fread(&c, 1, 1, f);
        eof = feof(f);
        fclose(f);

        /* Kernel threads have an empty cmdline */

        if (count <= 0)
                return eof ? 1 : -errno;

        return 0;
}

#if 0 /// UNNEEDED by elogind
int get_process_capeff(pid_t pid, char **capeff) {
        const char *p;
        int r;

        assert(capeff);
        assert(pid >= 0);

        p = procfs_file_alloca(pid, "status");

        r = get_proc_field(p, "CapEff", WHITESPACE, capeff);
        if (r == -ENOENT)
                return -ESRCH;

        return r;
}
#endif // 0

static int get_process_link_contents(const char *proc_file, char **name) {
        int r;

        assert(proc_file);
        assert(name);

        r = readlink_malloc(proc_file, name);
        if (r == -ENOENT)
                return -ESRCH;
        if (r < 0)
                return r;

        return 0;
}

int get_process_exe(pid_t pid, char **name) {
        const char *p;
        char *d;
        int r;

        assert(pid >= 0);

        p = procfs_file_alloca(pid, "exe");
        r = get_process_link_contents(p, name);
        if (r < 0)
                return r;

        d = endswith(*name, " (deleted)");
        if (d)
                *d = '\0';

        return 0;
}

#if 0 /// UNNEEDED by elogind
static int get_process_id(pid_t pid, const char *field, uid_t *uid) {
        _cleanup_fclose_ FILE *f = NULL;
        char line[LINE_MAX];
        const char *p;

        assert(field);
        assert(uid);

        if (pid < 0)
                return -EINVAL;

        p = procfs_file_alloca(pid, "status");
        f = fopen(p, "re");
        if (!f) {
                if (errno == ENOENT)
                        return -ESRCH;
                return -errno;
        }

        FOREACH_LINE(line, f, return -errno) {
                char *l;

                l = strstrip(line);

                if (startswith(l, field)) {
                        l += strlen(field);
                        l += strspn(l, WHITESPACE);

                        l[strcspn(l, WHITESPACE)] = 0;

                        return parse_uid(l, uid);
                }
        }

        return -EIO;
}

int get_process_uid(pid_t pid, uid_t *uid) {

        if (pid == 0 || pid == getpid_cached()) {
                *uid = getuid();
                return 0;
        }

        return get_process_id(pid, "Uid:", uid);
}

int get_process_gid(pid_t pid, gid_t *gid) {

        if (pid == 0 || pid == getpid_cached()) {
                *gid = getgid();
                return 0;
        }

        assert_cc(sizeof(uid_t) == sizeof(gid_t));
        return get_process_id(pid, "Gid:", gid);
}

int get_process_cwd(pid_t pid, char **cwd) {
        const char *p;

        assert(pid >= 0);

        p = procfs_file_alloca(pid, "cwd");

        return get_process_link_contents(p, cwd);
}

int get_process_root(pid_t pid, char **root) {
        const char *p;

        assert(pid >= 0);

        p = procfs_file_alloca(pid, "root");

        return get_process_link_contents(p, root);
}

int get_process_environ(pid_t pid, char **env) {
        _cleanup_fclose_ FILE *f = NULL;
        _cleanup_free_ char *outcome = NULL;
        int c;
        const char *p;
        size_t allocated = 0, sz = 0;

        assert(pid >= 0);
        assert(env);

        p = procfs_file_alloca(pid, "environ");

        f = fopen(p, "re");
        if (!f) {
                if (errno == ENOENT)
                        return -ESRCH;
                return -errno;
        }

        while ((c = fgetc(f)) != EOF) {
                if (!GREEDY_REALLOC(outcome, allocated, sz + 5))
                        return -ENOMEM;

                if (c == '\0')
                        outcome[sz++] = '\n';
                else
                        sz += cescape_char(c, outcome + sz);
        }

        if (!outcome) {
                outcome = strdup("");
                if (!outcome)
                        return -ENOMEM;
        } else
                outcome[sz] = '\0';

        *env = outcome;
        outcome = NULL;

        return 0;
}

int get_process_ppid(pid_t pid, pid_t *_ppid) {
        int r;
        _cleanup_free_ char *line = NULL;
        long unsigned ppid;
        const char *p;

        assert(pid >= 0);
        assert(_ppid);

        if (pid == 0 || pid == getpid_cached()) {
                *_ppid = getppid();
                return 0;
        }

        p = procfs_file_alloca(pid, "stat");
        r = read_one_line_file(p, &line);
        if (r == -ENOENT)
                return -ESRCH;
        if (r < 0)
                return r;

        /* Let's skip the pid and comm fields. The latter is enclosed
         * in () but does not escape any () in its value, so let's
         * skip over it manually */

        p = strrchr(line, ')');
        if (!p)
                return -EIO;

        p++;

        if (sscanf(p, " "
                   "%*c "  /* state */
                   "%lu ", /* ppid */
                   &ppid) != 1)
                return -EIO;

        if ((long unsigned) (pid_t) ppid != ppid)
                return -ERANGE;

        *_ppid = (pid_t) ppid;

        return 0;
}
#endif // 0

int wait_for_terminate(pid_t pid, siginfo_t *status) {
        siginfo_t dummy;

        assert(pid >= 1);

        if (!status)
                status = &dummy;

        for (;;) {
                zero(*status);

                if (waitid(P_PID, pid, status, WEXITED) < 0) {

                        if (errno == EINTR)
                                continue;

                        return negative_errno();
                }

                return 0;
        }
}

/*
 * Return values:
 * < 0 : wait_for_terminate() failed to get the state of the
 *       process, the process was terminated by a signal, or
 *       failed for an unknown reason.
 * >=0 : The process terminated normally, and its exit code is
 *       returned.
 *
 * That is, success is indicated by a return value of zero, and an
 * error is indicated by a non-zero value.
 *
 * A warning is emitted if the process terminates abnormally,
 * and also if it returns non-zero unless check_exit_code is true.
 */
int wait_for_terminate_and_warn(const char *name, pid_t pid, bool check_exit_code) {
        int r;
        siginfo_t status;

        assert(name);
        assert(pid > 1);

        r = wait_for_terminate(pid, &status);
        if (r < 0)
                return log_warning_errno(r, "Failed to wait for %s: %m", name);

        if (status.si_code == CLD_EXITED) {
                if (status.si_status != 0)
                        log_full(check_exit_code ? LOG_WARNING : LOG_DEBUG,
                                 "%s failed with error code %i.", name, status.si_status);
                else
                        log_debug("%s succeeded.", name);

                return status.si_status;
        } else if (IN_SET(status.si_code, CLD_KILLED, CLD_DUMPED)) {

                log_warning("%s terminated by signal %s.", name, signal_to_string(status.si_status));
                return -EPROTO;
        }

        log_warning("%s failed due to unknown reason.", name);
        return -EPROTO;
}

#if 0 /// UNNEEDED by elogind
void sigkill_wait(pid_t pid) {
        assert(pid > 1);

        if (kill(pid, SIGKILL) > 0)
                (void) wait_for_terminate(pid, NULL);
}

void sigkill_waitp(pid_t *pid) {
        if (!pid)
                return;
        if (*pid <= 1)
                return;

        sigkill_wait(*pid);
}

int kill_and_sigcont(pid_t pid, int sig) {
        int r;

        r = kill(pid, sig) < 0 ? -errno : 0;

        /* If this worked, also send SIGCONT, unless we already just sent a SIGCONT, or SIGKILL was sent which isn't
         * affected by a process being suspended anyway. */
        if (r >= 0 && !IN_SET(sig, SIGCONT, SIGKILL))
                (void) kill(pid, SIGCONT);

        return r;
}
#endif // 0

int getenv_for_pid(pid_t pid, const char *field, char **_value) {
        _cleanup_fclose_ FILE *f = NULL;
        char *value = NULL;
        int r;
        bool done = false;
        size_t l;
        const char *path;

        assert(pid >= 0);
        assert(field);
        assert(_value);

        path = procfs_file_alloca(pid, "environ");

        f = fopen(path, "re");
        if (!f) {
                if (errno == ENOENT)
                        return -ESRCH;
                return -errno;
        }

        l = strlen(field);
        r = 0;

        do {
                char line[LINE_MAX];
                unsigned i;

                for (i = 0; i < sizeof(line)-1; i++) {
                        int c;

                        c = getc(f);
                        if (_unlikely_(c == EOF)) {
                                done = true;
                                break;
                        } else if (c == 0)
                                break;

                        line[i] = c;
                }
                line[i] = 0;

                if (strneq(line, field, l) && line[l] == '=') {
                        value = strdup(line + l + 1);
                        if (!value)
                                return -ENOMEM;

                        r = 1;
                        break;
                }

        } while (!done);

        *_value = value;
        return r;
}

bool pid_is_unwaited(pid_t pid) {
        /* Checks whether a PID is still valid at all, including a zombie */

        if (pid < 0)
                return false;

        if (pid <= 1) /* If we or PID 1 would be dead and have been waited for, this code would not be running */
                return true;

        if (pid == getpid_cached())
                return true;

        if (kill(pid, 0) >= 0)
                return true;

        return errno != ESRCH;
}

bool pid_is_alive(pid_t pid) {
        int r;

        /* Checks whether a PID is still valid and not a zombie */

        if (pid < 0)
                return false;

        if (pid <= 1) /* If we or PID 1 would be a zombie, this code would not be running */
                return true;

        if (pid == getpid_cached())
                return true;

        r = get_process_state(pid);
        if (IN_SET(r, -ESRCH, 'Z'))
                return false;

        return true;
}

#if 0 /// UNNEEDED by elogind
int pid_from_same_root_fs(pid_t pid) {
        const char *root;

        if (pid < 0)
                return false;

        if (pid == 0 || pid == getpid_cached())
                return true;

        root = procfs_file_alloca(pid, "root");

        return files_same(root, "/proc/1/root", 0);
}
#endif // 0

bool is_main_thread(void) {
        static thread_local int cached = 0;

        if (_unlikely_(cached == 0))
                cached = getpid_cached() == gettid() ? 1 : -1;

        return cached > 0;
}

#if 0 /// UNNEEDED by elogind
noreturn void freeze(void) {

        log_close();

        /* Make sure nobody waits for us on a socket anymore */
        close_all_fds(NULL, 0);

        sync();

        for (;;)
                pause();
}

bool oom_score_adjust_is_valid(int oa) {
        return oa >= OOM_SCORE_ADJ_MIN && oa <= OOM_SCORE_ADJ_MAX;
}

unsigned long personality_from_string(const char *p) {
        int architecture;

        if (!p)
                return PERSONALITY_INVALID;

        /* Parse a personality specifier. We use our own identifiers that indicate specific ABIs, rather than just
         * hints regarding the register size, since we want to keep things open for multiple locally supported ABIs for
         * the same register size. */

        architecture = architecture_from_string(p);
        if (architecture < 0)
                return PERSONALITY_INVALID;

        if (architecture == native_architecture())
                return PER_LINUX;
#ifdef SECONDARY_ARCHITECTURE
        if (architecture == SECONDARY_ARCHITECTURE)
                return PER_LINUX32;
#endif

        return PERSONALITY_INVALID;
}

const char* personality_to_string(unsigned long p) {
        int architecture = _ARCHITECTURE_INVALID;

        if (p == PER_LINUX)
                architecture = native_architecture();
#ifdef SECONDARY_ARCHITECTURE
        else if (p == PER_LINUX32)
                architecture = SECONDARY_ARCHITECTURE;
#endif

        if (architecture < 0)
                return NULL;

        return architecture_to_string(architecture);
}

int safe_personality(unsigned long p) {
        int ret;

        /* So here's the deal, personality() is weirdly defined by glibc. In some cases it returns a failure via errno,
         * and in others as negative return value containing an errno-like value. Let's work around this: this is a
         * wrapper that uses errno if it is set, and uses the return value otherwise. And then it sets both errno and
         * the return value indicating the same issue, so that we are definitely on the safe side.
         *
         * See https://github.com/elogind/elogind/issues/6737 */

        errno = 0;
        ret = personality(p);
        if (ret < 0) {
                if (errno != 0)
                        return -errno;

                errno = -ret;
        }

        return ret;
}

int opinionated_personality(unsigned long *ret) {
        int current;

        /* Returns the current personality, or PERSONALITY_INVALID if we can't determine it. This function is a bit
         * opinionated though, and ignores all the finer-grained bits and exotic personalities, only distinguishing the
         * two most relevant personalities: PER_LINUX and PER_LINUX32. */

        current = safe_personality(PERSONALITY_INVALID);
        if (current < 0)
                return current;

        if (((unsigned long) current & 0xffff) == PER_LINUX32)
                *ret = PER_LINUX32;
        else
                *ret = PER_LINUX;

        return 0;
}

void valgrind_summary_hack(void) {
#if HAVE_VALGRIND_VALGRIND_H
        if (getpid_cached() == 1 && RUNNING_ON_VALGRIND) {
                pid_t pid;
                pid = raw_clone(SIGCHLD);
                if (pid < 0)
                        log_emergency_errno(errno, "Failed to fork off valgrind helper: %m");
                else if (pid == 0)
                        exit(EXIT_SUCCESS);
                else {
                        log_info("Spawned valgrind helper as PID "PID_FMT".", pid);
                        (void) wait_for_terminate(pid, NULL);
                }
        }
#endif
}

int pid_compare_func(const void *a, const void *b) {
        const pid_t *p = a, *q = b;

        /* Suitable for usage in qsort() */

        if (*p < *q)
                return -1;
        if (*p > *q)
                return 1;
        return 0;
}

int ioprio_parse_priority(const char *s, int *ret) {
        int i, r;

        assert(s);
        assert(ret);

        r = safe_atoi(s, &i);
        if (r < 0)
                return r;

        if (!ioprio_priority_is_valid(i))
                return -EINVAL;

        *ret = i;
        return 0;
}
#endif // 0

/* The cached PID, possible values:
 *
 *     == UNSET [0]  → cache not initialized yet
 *     == BUSY [-1]  → some thread is initializing it at the moment
 *     any other     → the cached PID
 */

#define CACHED_PID_UNSET ((pid_t) 0)
#define CACHED_PID_BUSY ((pid_t) -1)

static pid_t cached_pid = CACHED_PID_UNSET;

static void reset_cached_pid(void) {
        /* Invoked in the child after a fork(), i.e. at the first moment the PID changed */
        cached_pid = CACHED_PID_UNSET;
}

/* We use glibc __register_atfork() + __dso_handle directly here, as they are not included in the glibc
 * headers. __register_atfork() is mostly equivalent to pthread_atfork(), but doesn't require us to link against
 * libpthread, as it is part of glibc anyway. */
#ifdef __GLIBC__
extern int __register_atfork(void (*prepare) (void), void (*parent) (void), void (*child) (void), void * __dso_handle);
extern void* __dso_handle __attribute__ ((__weak__));
#endif // ifdef __GLIBC__

pid_t getpid_cached(void) {
        pid_t current_value;

        /* getpid_cached() is much like getpid(), but caches the value in local memory, to avoid having to invoke a
         * system call each time. This restores glibc behaviour from before 2.24, when getpid() was unconditionally
         * cached. Starting with 2.24 getpid() started to become prohibitively expensive when used for detecting when
         * objects were used across fork()s. With this caching the old behaviour is somewhat restored.
         *
         * https://bugzilla.redhat.com/show_bug.cgi?id=1443976
         * https://sourceware.org/git/gitweb.cgi?p=glibc.git;h=c579f48edba88380635ab98cb612030e3ed8691e
         */

        current_value = __sync_val_compare_and_swap(&cached_pid, CACHED_PID_UNSET, CACHED_PID_BUSY);

        switch (current_value) {

        case CACHED_PID_UNSET: { /* Not initialized yet, then do so now */
                pid_t new_pid;

                new_pid = getpid();

                if (__register_atfork(NULL, NULL, reset_cached_pid, __dso_handle) != 0) {
                        /* OOM? Let's try again later */
                        cached_pid = CACHED_PID_UNSET;
                        return new_pid;
                }

                cached_pid = new_pid;
                return new_pid;
        }

        case CACHED_PID_BUSY: /* Somebody else is currently initializing */
                return getpid();

        default: /* Properly initialized */
                return current_value;
        }
}

#if 0 /// UNNEEDED by elogind
static const char *const ioprio_class_table[] = {
        [IOPRIO_CLASS_NONE] = "none",
        [IOPRIO_CLASS_RT] = "realtime",
        [IOPRIO_CLASS_BE] = "best-effort",
        [IOPRIO_CLASS_IDLE] = "idle"
};

DEFINE_STRING_TABLE_LOOKUP_WITH_FALLBACK(ioprio_class, int, INT_MAX);

static const char *const sigchld_code_table[] = {
        [CLD_EXITED] = "exited",
        [CLD_KILLED] = "killed",
        [CLD_DUMPED] = "dumped",
        [CLD_TRAPPED] = "trapped",
        [CLD_STOPPED] = "stopped",
        [CLD_CONTINUED] = "continued",
};

DEFINE_STRING_TABLE_LOOKUP(sigchld_code, int);

static const char* const sched_policy_table[] = {
        [SCHED_OTHER] = "other",
        [SCHED_BATCH] = "batch",
        [SCHED_IDLE] = "idle",
        [SCHED_FIFO] = "fifo",
        [SCHED_RR] = "rr"
};

DEFINE_STRING_TABLE_LOOKUP_WITH_FALLBACK(sched_policy, int, INT_MAX);
#endif // 0
