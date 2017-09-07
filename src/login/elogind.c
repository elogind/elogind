/***
  This file is part of elogind.

  Copyright 2017 Sven Eden

  elogind is free software; you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation; either version 2.1 of the License, or
  (at your option) any later version.

  elogind is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with elogind; If not, see <http://www.gnu.org/licenses/>.
***/


#include "bus-util.h"
#include "cgroup.h"
#include "elogind.h"
#include "fd-util.h"
#include "fileio.h"
#include "fs-util.h"
#include "mount-setup.h"
#include "parse-util.h"
#include "process-util.h"
#include "signal-util.h"
#include "socket-util.h"
#include "stdio-util.h"
#include "string-util.h"
#include "strv.h"
#include "umask-util.h"


#define CGROUPS_AGENT_RCVBUF_SIZE (8*1024*1024)
#ifndef ELOGIND_PID_FILE
#  define ELOGIND_PID_FILE "/run/elogind.pid"
#endif // ELOGIND_PID_FILE


static int elogind_signal_handler(sd_event_source *s,
                                   const struct signalfd_siginfo *si,
                                   void *userdata) {
        Manager *m = userdata;
        int r;

        log_warning("Received signal %u [%s]", si->ssi_signo,
                    signal_to_string(si->ssi_signo));

        r = sd_event_get_state(m->event);

        if (r != SD_EVENT_FINISHED)
                sd_event_exit(m->event, si->ssi_signo);

        return 0;
}


static void remove_pid_file(void) {
        if (access(ELOGIND_PID_FILE, F_OK) == 0)
                unlink_noerrno(ELOGIND_PID_FILE);
}


static void write_pid_file(void) {
        char c[DECIMAL_STR_MAX(pid_t) + 2];
        pid_t pid;
        int   r;

        pid = getpid();

        xsprintf(c, PID_FMT "\n", pid);

        r = write_string_file(ELOGIND_PID_FILE, c,
                              WRITE_STRING_FILE_CREATE |
                              WRITE_STRING_FILE_VERIFY_ON_FAILURE);
        if (r < 0)
                log_error_errno(-r, "Failed to write PID file %s: %m",
                                ELOGIND_PID_FILE);

        /* Make sure the PID file gets cleaned up on exit! */
        atexit(remove_pid_file);
}


/** daemonize elogind by double forking.
  * The grand child returns 0.
  * The parent and child return their forks PID.
  * On error, a value < 0 is returned.
**/
static int elogind_daemonize(void) {
        pid_t child;
        pid_t grandchild;
        pid_t SID;
        int r;

#ifdef ENABLE_DEBUG_ELOGIND
        log_notice("Double forking elogind");
        log_notice("Parent PID     : %5d", getpid());
        log_notice("Parent SID     : %5d", getsid(getpid()));
#endif // ENABLE_DEBUG_ELOGIND

        child = fork();

        if (child < 0)
                return log_error_errno(errno, "Failed to fork: %m");

        if (child) {
                /* Wait for the child to terminate, so the decoupling
                 * is guaranteed to succeed.
                 */
                r = wait_for_terminate_and_warn("elogind control child", child, true);
                if (r < 0)
                        return r;
                return child;
        }

#ifdef ENABLE_DEBUG_ELOGIND
        log_notice("Child PID      : %5d", getpid());
        log_notice("Child SID      : %5d", getsid(getpid()));
#endif // ENABLE_DEBUG_ELOGIND

        /* The first child has to become a new session leader. */
        close_all_fds(NULL, 0);
        SID = setsid();
        if ((pid_t)-1 == SID)
                return log_error_errno(errno, "Failed to create new SID: %m");

#ifdef ENABLE_DEBUG_ELOGIND
        log_notice("Child new SID  : %5d", getsid(getpid()));
#endif // ENABLE_DEBUG_ELOGIND

        umask(0022);

        /* Now the grandchild, the true daemon, can be created. */
        grandchild = fork();

        if (grandchild < 0)
                return log_error_errno(errno, "Failed to double fork: %m");

        if (grandchild)
                /* Exit immediately! */
                return grandchild;

        close_all_fds(NULL, 0);
        umask(0022);

#ifdef ENABLE_DEBUG_ELOGIND
        log_notice("Grand child PID: %5d", getpid());
        log_notice("Grand child SID: %5d", getsid(getpid()));
#endif // ENABLE_DEBUG_ELOGIND

        /* Take care of our PID-file now */
        write_pid_file();

        return 0;
}


/// Simple tool to see, if elogind is already running
static pid_t elogind_is_already_running(bool need_pid_file) {
        _cleanup_free_ char *s = NULL;
        pid_t pid;
        int r;

        r = read_one_line_file(ELOGIND_PID_FILE, &s);

        if (r < 0)
                goto we_are_alone;

        r = safe_atoi32(s, &pid);

        if (r < 0)
                goto we_are_alone;

        if ( (pid != getpid()) && pid_is_alive(pid))
                return pid;

we_are_alone:

        /* Take care of our PID-file now.
           If the user is going to fork elogind, the PID file
           will be overwritten. */
        if (need_pid_file)
                write_pid_file();

        return 0;
}


/** Extra functionality at startup, exclusive to elogind
  * return < 0 on error, exit with failure.
  * return = 0 on success, continue normal operation.
  * return > 0 if elogind is already running or forked, exit with success.
**/
int elogind_startup(int argc, char *argv[]) {
        bool  daemonize = false;
        pid_t pid;
        int   r         = 0;
        bool  show_help = false;
        bool  wrong_arg = false;

        /* add a -h/--help and a -d/--daemon argument. */
        if ( (argc == 2) && argv[1] && strlen(argv[1]) ) {
                if ( streq(argv[1], "-D") || streq(argv[1], "--daemon") )
                        daemonize = true;
                else if ( streq(argv[1], "-h") || streq(argv[1], "--help") ) {
                        show_help = true;
                        r = 1;
                } else
                        wrong_arg = true;
        } else if (argc > 2)
                wrong_arg = true;

        /* Note: At this point, the logging is not initialized, so we can not
                 use log_debug_elogind(). */
#ifdef ENABLE_DEBUG_ELOGIND
        log_notice("elogind startup: Daemonize: %s, Show Help: %s, Wrong arg: %s",
                daemonize ? "True" : "False",
                show_help ? "True" : "False",
                wrong_arg ? "True" : "False");
#endif // ENABLE_DEBUG_ELOGIND

        /* try to get some meaningful output in case of an error */
        if (wrong_arg) {
                log_error("Unknown arguments");
                show_help = true;
                r = -EINVAL;
        }
        if (show_help) {
                log_info("%s [<-D|--daemon>|<-h|--help>]", basename(argv[0]));
                return r;
        }

        /* Do not continue if elogind is already running */
        pid = elogind_is_already_running(!daemonize);
        if (pid) {
                log_error("elogind is already running as PID " PID_FMT, pid);
                return pid;
        }

        /* elogind allows to be daemonized using one argument "-D" / "--daemon" */
        if (daemonize)
                r = elogind_daemonize();

        return r;
}


/// Add-On for manager_startup()
int elogind_manager_startup(Manager *m) {
        int r;

        assert(m);

        assert_se(sigprocmask_many(SIG_SETMASK, NULL, SIGINT, -1) >= 0);
        r = sd_event_add_signal(m->event, NULL, SIGINT, elogind_signal_handler, m);
        if (r < 0)
                return log_error_errno(r, "Failed to register SIGINT handler: %m");

        assert_se(sigprocmask_many(SIG_SETMASK, NULL, SIGQUIT, -1) >= 0);
        r = sd_event_add_signal(m->event, NULL, SIGQUIT, elogind_signal_handler, m);
        if (r < 0)
                return log_error_errno(r, "Failed to register SIGQUIT handler: %m");

        assert_se(sigprocmask_many(SIG_SETMASK, NULL, SIGTERM, -1) >= 0);
        r = sd_event_add_signal(m->event, NULL, SIGTERM, elogind_signal_handler, m);
        if (r < 0)
                return log_error_errno(r, "Failed to register SIGTERM handler: %m");

        return 0;
}
