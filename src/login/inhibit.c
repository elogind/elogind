/* SPDX-License-Identifier: LGPL-2.1+ */

#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "sd-bus.h"

#include "alloc-util.h"
#include "bus-error.h"
#include "bus-util.h"
#include "fd-util.h"
#include "format-util.h"
#include "pager.h"
#include "process-util.h"
#include "signal-util.h"
#include "strv.h"
#include "user-util.h"
#include "util.h"

/// Additional includes needed by elogind
#include "musl_missing.h"

static const char* arg_what = "idle:sleep:shutdown";
static const char* arg_who = NULL;
static const char* arg_why = "Unknown reason";
static const char* arg_mode = NULL;
static bool arg_no_pager = false;

static enum {
        ACTION_INHIBIT,
        ACTION_LIST
} arg_action = ACTION_INHIBIT;

static int inhibit(sd_bus *bus, sd_bus_error *error) {
        _cleanup_(sd_bus_message_unrefp) sd_bus_message *reply = NULL;
        int r;
        int fd;

        r = sd_bus_call_method(
                        bus,
                        "org.freedesktop.login1",
                        "/org/freedesktop/login1",
                        "org.freedesktop.login1.Manager",
                        "Inhibit",
                        error,
                        &reply,
                        "ssss", arg_what, arg_who, arg_why, arg_mode);
        if (r < 0)
                return r;

        r = sd_bus_message_read_basic(reply, SD_BUS_TYPE_UNIX_FD, &fd);
        if (r < 0)
                return r;

        r = fcntl(fd, F_DUPFD_CLOEXEC, 3);
        if (r < 0)
                return -errno;

        return r;
}

static int print_inhibitors(sd_bus *bus, sd_bus_error *error) {
        _cleanup_(sd_bus_message_unrefp) sd_bus_message *reply = NULL;
        const char *what, *who, *why, *mode;
        unsigned int uid, pid;
        unsigned n = 0;
        int r;

        (void) pager_open(arg_no_pager, false);

        r = sd_bus_call_method(
                        bus,
                        "org.freedesktop.login1",
                        "/org/freedesktop/login1",
                        "org.freedesktop.login1.Manager",
                        "ListInhibitors",
                        error,
                        &reply,
                        "");
        if (r < 0)
                return r;

        r = sd_bus_message_enter_container(reply, SD_BUS_TYPE_ARRAY, "(ssssuu)");
        if (r < 0)
                return bus_log_parse_error(r);

        while ((r = sd_bus_message_read(reply, "(ssssuu)", &what, &who, &why, &mode, &uid, &pid)) > 0) {
                _cleanup_free_ char *comm = NULL, *u = NULL;

                if (arg_mode && !streq(mode, arg_mode))
                        continue;

                get_process_comm(pid, &comm);
                u = uid_to_name(uid);

                printf("     Who: %s (UID "UID_FMT"/%s, PID "PID_FMT"/%s)\n"
                       "    What: %s\n"
                       "     Why: %s\n"
                       "    Mode: %s\n\n",
                       who, uid, strna(u), pid, strna(comm),
                       what,
                       why,
                       mode);

                n++;
        }
        if (r < 0)
                return bus_log_parse_error(r);

        r = sd_bus_message_exit_container(reply);
        if (r < 0)
                return bus_log_parse_error(r);

        printf("%u inhibitors listed.\n", n);
        return 0;
}

static void help(void) {
        printf("%s [OPTIONS...] {COMMAND} ...\n\n"
               "Execute a process while inhibiting shutdown/sleep/idle.\n\n"
               "  -h --help               Show this help\n"
               "     --version            Show package version\n"
               "     --no-pager           Do not pipe output into a pager\n"
               "     --what=WHAT          Operations to inhibit, colon separated list of:\n"
               "                          shutdown, sleep, idle, handle-power-key,\n"
               "                          handle-suspend-key, handle-hibernate-key,\n"
               "                          handle-lid-switch\n"
               "     --who=STRING         A descriptive string who is inhibiting\n"
               "     --why=STRING         A descriptive string why is being inhibited\n"
               "     --mode=MODE          One of block or delay\n"
               "     --list               List active inhibitors\n"
               , program_invocation_short_name);
}

static int parse_argv(int argc, char *argv[]) {

        enum {
                ARG_VERSION = 0x100,
                ARG_WHAT,
                ARG_WHO,
                ARG_WHY,
                ARG_MODE,
                ARG_LIST,
                ARG_NO_PAGER,
        };

        static const struct option options[] = {
                { "help",         no_argument,       NULL, 'h'              },
                { "version",      no_argument,       NULL, ARG_VERSION      },
                { "what",         required_argument, NULL, ARG_WHAT         },
                { "who",          required_argument, NULL, ARG_WHO          },
                { "why",          required_argument, NULL, ARG_WHY          },
                { "mode",         required_argument, NULL, ARG_MODE         },
                { "list",         no_argument,       NULL, ARG_LIST         },
                { "no-pager",     no_argument,       NULL, ARG_NO_PAGER     },
                {}
        };

        int c;

        assert(argc >= 0);
        assert(argv);

        while ((c = getopt_long(argc, argv, "+h", options, NULL)) >= 0)

                switch (c) {

                case 'h':
                        help();
                        return 0;

                case ARG_VERSION:
                        return version();

                case ARG_WHAT:
                        arg_what = optarg;
                        break;

                case ARG_WHO:
                        arg_who = optarg;
                        break;

                case ARG_WHY:
                        arg_why = optarg;
                        break;

                case ARG_MODE:
                        arg_mode = optarg;
                        break;

                case ARG_LIST:
                        arg_action = ACTION_LIST;
                        break;

                case ARG_NO_PAGER:
                        arg_no_pager = true;
                        break;

                case '?':
                        return -EINVAL;

                default:
                        assert_not_reached("Unhandled option");
                }

        if (arg_action == ACTION_INHIBIT && optind == argc)
                arg_action = ACTION_LIST;

        else if (arg_action == ACTION_INHIBIT && optind >= argc) {
                log_error("Missing command line to execute.");
                return -EINVAL;
        }

        return 1;
}

int main(int argc, char *argv[]) {
        _cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
        _cleanup_(sd_bus_flush_close_unrefp) sd_bus *bus = NULL;
        int r;

        elogind_set_program_name(argv[0]);
        log_parse_environment();
        log_open();

        r = parse_argv(argc, argv);
        if (r < 0)
                return EXIT_FAILURE;
        if (r == 0)
                return EXIT_SUCCESS;

        r = sd_bus_default_system(&bus);
        if (r < 0) {
                log_error_errno(r, "Failed to connect to bus: %m");
                return EXIT_FAILURE;
        }

        if (arg_action == ACTION_LIST) {

                r = print_inhibitors(bus, &error);
                pager_close();
                if (r < 0) {
                        log_error("Failed to list inhibitors: %s", bus_error_message(&error, -r));
                        return EXIT_FAILURE;
                }

        } else {
                _cleanup_close_ int fd = -1;
                _cleanup_free_ char *w = NULL;
                pid_t pid;

                /* Ignore SIGINT and allow the forked process to receive it */
                (void) ignore_signals(SIGINT, -1);

                if (!arg_who)
                        arg_who = w = strv_join(argv + optind, " ");

                if (!arg_mode)
                        arg_mode = "block";

                fd = inhibit(bus, &error);
                if (fd < 0) {
                        log_error("Failed to inhibit: %s", bus_error_message(&error, fd));
                        return EXIT_FAILURE;
                }

                r = safe_fork("(inhibit)", FORK_RESET_SIGNALS|FORK_DEATHSIG|FORK_CLOSE_ALL_FDS|FORK_LOG, &pid);
                if (r < 0)
                        return EXIT_FAILURE;
                if (r == 0) {
                        /* Child */
                        execvp(argv[optind], argv + optind);
                        log_open();
                        log_error_errno(errno, "Failed to execute %s: %m", argv[optind]);
                        _exit(EXIT_FAILURE);
                }

                r = wait_for_terminate_and_check(argv[optind], pid, WAIT_LOG);
                return r < 0 ? EXIT_FAILURE : r;
        }

        return EXIT_SUCCESS;
}
