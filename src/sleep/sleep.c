/* SPDX-License-Identifier: LGPL-2.1+ */
/***
  Copyright © 2010-2017 Canonical
  Copyright © 2018 Dell Inc.
***/

#include <errno.h>
#include <fcntl.h>
//#include <getopt.h>
//#include <linux/fiemap.h>
#include <poll.h>
#include <sys/stat.h>
//#include <sys/types.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include "sd-messages.h"

//#include "btrfs-util.h"
//#include "bus-common-errors.h"
//#include "bus-error.h"
#include "def.h"
#include "exec-util.h"
#include "fd-util.h"
#include "fileio.h"
//#include "format-util.h"
#include "io-util.h"
#include "log.h"
#include "main-func.h"
//#include "parse-util.h"
//#include "pretty-print.h"
#include "sleep-config.h"
#include "stdio-util.h"
#include "string-util.h"
#include "strv.h"
#include "time-util.h"
//#include "util.h"

/// Additional includes needed by elogind
#include "exec-elogind.h"
#include "sd-login.h"
#include "sleep.h"
#include "terminal-util.h"
#include "utmp-wtmp.h"

static char* arg_verb = NULL;

STATIC_DESTRUCTOR_REGISTER(arg_verb, freep);

#if 1 /// If an nvidia card is present, elogind informs its driver about suspend/resume actions
static int nvidia_sleep(Manager* m, char const* verb, unsigned* vtnr) {
        static char const* drv_suspend = "/proc/driver/nvidia/suspend";
        struct stat std;
        int r, x11 = 0;
        unsigned vt = 0;
        char** session;
        char** sessions;
        char* type;

        assert(verb);
        assert(vtnr);

        // See whether an nvidia suspension is possible
        r = stat(drv_suspend, &std);
        if (r)
                return 0;

        if (STR_IN_SET(verb, "suspend", "hibernate", "hybrid-sleep", "suspend-then-hibernate")) {
                *vtnr = 0;

                // Find the (active) sessions of the sleep sender
                r = sd_uid_get_sessions(m->scheduled_sleep_uid, 1, &sessions);
#if ENABLE_DEBUG_ELOGIND
                char *t = strv_join(sessions, " ");
                log_debug_elogind("sd_uid_get_sessions() returned %d, result is: %s", r, strnull(t));
                free(t);
#endif // elogind debug
                if (r < 0)
                        return 0;

                // Find one with a VT (Really, sessions should hold only one active session anyway!)
                STRV_FOREACH(session, sessions) {
                        int k;
                        k = sd_session_get_vt(*session, &vt);
                        if (k >= 0) {
                                log_debug_elogind("Active session %s with VT %u found", *session, vt);
                                break;
                        }
                }

                // See whether the type of any active session is not "tty"
                STRV_FOREACH(session, sessions) {
                        int k;
                        k = sd_session_get_type(*session, &type);
                        if ((k >= 0) && strcmp("tty", strnull(type))) {
                                log_debug_elogind("Session %s with Type %s counted as non-tty", *session, type);
                                ++x11;
                        }
                }

                strv_free(sessions);

                // Get to a safe non-gui VT if we are on any GUI
                if ( x11 > 0 ) {
                        log_debug_elogind("Storing VT %u and switching to VT 0", vt);
                        *vtnr = vt;
                        (void) chvt(0);
                }

                // Okay, go to sleep.
                if (streq(verb, "suspend")) {
                        log_debug_elogind("Writing 'suspend' into %s", drv_suspend);
                        r = write_string_file(drv_suspend, "suspend", WRITE_STRING_FILE_DISABLE_BUFFER);
                } else {
                        log_debug_elogind("Writing 'hibernate' into %s", drv_suspend);
                        r = write_string_file(drv_suspend, "hibernate", WRITE_STRING_FILE_DISABLE_BUFFER);
                }

                if (r)
                        return 0;
        } else if (streq(verb, "resume")) {
                // Wakeup the device
                log_debug_elogind("Writing '%s' into %s", verb, drv_suspend);
                (void) write_string_file(drv_suspend, verb, WRITE_STRING_FILE_DISABLE_BUFFER);
                // Then try to change back
                if (*vtnr > 0) {
                        log_debug_elogind("Switching back to VT %u", *vtnr);
                        (void) chvt(*vtnr);
                }
        }

        return 1;
}
#endif // 1

static int write_hibernate_location_info(const HibernateLocation* hibernate_location) {
        char offset_str[DECIMAL_STR_MAX(uint64_t)];
        char resume_str[DECIMAL_STR_MAX(unsigned) * 2 + STRLEN(":")];
        int r;
#if 1 /// To support LVM setups, elogind uses device numbers
        char device_num_str[DECIMAL_STR_MAX(uint32_t) * 2 + 2];
        struct stat stb;
#endif // 1

        assert(hibernate_location);
        assert(hibernate_location->swap);

        xsprintf(resume_str, "%u:%u", major(hibernate_location->devno), minor(hibernate_location->devno));
        r = write_string_file("/sys/power/resume", resume_str, WRITE_STRING_FILE_DISABLE_BUFFER);

#if 1 /// To support LVM setups, elogind uses device numbers if the direct approach failed
        if (r < 0) {
                r = stat(hibernate_location->swap->device, &stb);
                if (r < 0)
                        return log_debug_errno(errno, "Error while trying to get stats for %s: %m",
                                               hibernate_location->swap->device);

                (void) snprintf(device_num_str, DECIMAL_STR_MAX(uint32_t) * 2 + 2,
                                "%u:%u",
                                major(stb.st_rdev), minor(stb.st_rdev));
                r = write_string_file("/sys/power/resume", device_num_str, 0);
        }
#endif // 1

        if (r < 0)
                return log_debug_errno(r, "Failed to write partition device to /sys/power/resume for '%s': '%s': %m",
                                       hibernate_location->swap->device, resume_str);

        log_debug("Wrote resume= value for %s to /sys/power/resume: %s", hibernate_location->swap->device, resume_str);

        /* if it's a swap partition, we're done */
        if (streq(hibernate_location->swap->type, "partition"))
                return r;

        if (!streq(hibernate_location->swap->type, "file"))
                return log_debug_errno(SYNTHETIC_ERRNO(EINVAL),
                                       "Invalid hibernate type: %s", hibernate_location->swap->type);

        /* Only available in 4.17+ */
        if (hibernate_location->offset > 0 && access("/sys/power/resume_offset", W_OK) < 0) {
                if (errno == ENOENT) {
                        log_debug("Kernel too old, can't configure resume_offset for %s, ignoring: %" PRIu64,
                                  hibernate_location->swap->device, hibernate_location->offset);
                        return 0;
                }

                return log_debug_errno(errno, "/sys/power/resume_offset not writable: %m");
        }

        xsprintf(offset_str, "%" PRIu64, hibernate_location->offset);
        r = write_string_file("/sys/power/resume_offset", offset_str, WRITE_STRING_FILE_DISABLE_BUFFER);
        if (r < 0)
                return log_debug_errno(r, "Failed to write swap file offset to /sys/power/resume_offset for '%s': '%s': %m",
                                       hibernate_location->swap->device, offset_str);

        log_debug("Wrote resume_offset= value for %s to /sys/power/resume_offset: %s", hibernate_location->swap->device, offset_str);

        return 0;
}

static int write_mode(char **modes) {
        int r = 0;
        char **mode;
#if 1 /// Heeding elogind configuration for SuspendMode, we need to know where to write what
        char const* mode_location = strcmp( arg_verb, "suspend") ? "/sys/power/disk" : "/sys/power/mem_sleep";
        log_debug_elogind("mode_location is: '%s'", mode_location);
#endif // 1

        STRV_FOREACH(mode, modes) {
                int k;

#if 0 /// elogind uses an adapting target
                k = write_string_file("/sys/power/disk", *mode, WRITE_STRING_FILE_DISABLE_BUFFER);
#else // 0
                log_debug_elogind("Writing '%s' to '%s'...", *mode, mode_location);
                k = write_string_file(mode_location, *mode, WRITE_STRING_FILE_DISABLE_BUFFER);
#endif // 0
                if (k >= 0)
                        return 0;

#if 0 /// elogind uses an adapting target
                log_debug_errno(k, "Failed to write '%s' to /sys/power/disk: %m", *mode);
#else // 0
                log_debug_errno(k, "Failed to write '%s' to %s: %m", *mode, mode_location);
#endif // 0
                if (r >= 0)
                        r = k;
        }

        return r;
}

static int write_state(FILE **f, char **states) {
        char **state;
        int r = 0;

        assert(f);
        assert(*f);

        STRV_FOREACH(state, states) {
                int k;

                log_debug_elogind("Writing '%s' into /sys/power/state", *state);

                k = write_string_stream(*f, *state, WRITE_STRING_FILE_DISABLE_BUFFER);
                if (k >= 0)
                        return 0;
                log_debug_errno(k, "Failed to write '%s' to /sys/power/state: %m", *state);
                if (r >= 0)
                        r = k;

                fclose(*f);
                *f = fopen("/sys/power/state", "we");
                if (!*f)
                        return -errno;
        }

        return r;
}

#if 0 /// UNNEEDED by elogind
static int lock_all_homes(void) {
        _cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
        _cleanup_(sd_bus_message_unrefp) sd_bus_message *m = NULL;
        _cleanup_(sd_bus_flush_close_unrefp) sd_bus *bus = NULL;
        int r;

        /* Let's synchronously lock all home directories managed by homed that have been marked for it. This
         * way the key material required to access these volumes is hopefully removed from memory. */

        r = sd_bus_open_system(&bus);
        if (r < 0)
                return log_warning_errno(r, "Failed to connect to system bus, ignoring: %m");

        r = sd_bus_message_new_method_call(
                        bus,
                        &m,
                        "org.freedesktop.home1",
                        "/org/freedesktop/home1",
                        "org.freedesktop.home1.Manager",
                        "LockAllHomes");
        if (r < 0)
                return bus_log_create_error(r);

        /* If homed is not running it can't have any home directories active either. */
        r = sd_bus_message_set_auto_start(m, false);
        if (r < 0)
                return log_error_errno(r, "Failed to disable auto-start of LockAllHomes() message: %m");

        r = sd_bus_call(bus, m, DEFAULT_TIMEOUT_USEC, &error, NULL);
        if (r < 0) {
                if (sd_bus_error_has_name(&error, SD_BUS_ERROR_SERVICE_UNKNOWN) ||
                    sd_bus_error_has_name(&error, SD_BUS_ERROR_NAME_HAS_NO_OWNER) ||
                    sd_bus_error_has_name(&error, BUS_ERROR_NO_SUCH_UNIT))
                        return log_debug("systemd-homed is not running, skipping locking of home directories.");

                return log_error_errno(r, "Failed to lock home directories: %s", bus_error_message(&error, r));
        }

        log_debug("Successfully requested for all home directories to be locked.");
        return 0;
}
#endif // 0

#if 0 /// Changes needed by elogind are too vast to do this inline
static int execute(char **modes, char **states) {
        char* arguments[] = {
                NULL,
                (char*) "pre",
                arg_verb,
                NULL
        };
        static const char* const dirs[] = {
                SYSTEM_SLEEP_PATH,
                NULL
        };

        _cleanup_fclose_ FILE *f = NULL;
        _cleanup_(hibernate_location_freep) HibernateLocation *hibernate_location = NULL;
        int r;

        /* This file is opened first, so that if we hit an error,
         * we can abort before modifying any state. */
        f = fopen("/sys/power/state", "we");
        if (!f)
                return log_error_errno(errno, "Failed to open /sys/power/state: %m");

        setvbuf(f, NULL, _IONBF, 0);

        /* Configure hibernation settings if we are supposed to hibernate */
        if (!strv_isempty(modes)) {
                r = find_hibernate_location(&hibernate_location);
                if (r < 0)
                        return log_error_errno(r, "Failed to find location to hibernate to: %m");
                if (r == 0) { /* 0 means: no hibernation location was configured in the kernel so far, let's
               * do it ourselves then. > 0 means: kernel already had a configured hibernation
               * location which we shouldn't touch. */
                        r = write_hibernate_location_info(hibernate_location);
                        if (r < 0)
                                return log_error_errno(r, "Failed to prepare for hibernation: %m");
                }
                r = write_mode(modes);
                if (r < 0)
                        return log_error_errno(r, "Failed to write mode to /sys/power/disk: %m");;
        }

        (void) execute_directories(dirs, DEFAULT_TIMEOUT_USEC, NULL, NULL, arguments, NULL, EXEC_DIR_PARALLEL | EXEC_DIR_IGNORE_ERRORS);
        (void) lock_all_homes();

        log_struct(LOG_INFO,
                   "MESSAGE_ID=" SD_MESSAGE_SLEEP_START_STR,
                LOG_MESSAGE("Suspending system..."),
                "SLEEP=%s", arg_verb);

        r = write_state(&f, states);
        if (r < 0)
                log_struct_errno(LOG_ERR, r,
                                 "MESSAGE_ID=" SD_MESSAGE_SLEEP_STOP_STR,
                LOG_MESSAGE("Failed to suspend system. System resumed again: %m"),
                "SLEEP=%s", arg_verb);
        else
        log_struct(LOG_INFO,
                   "MESSAGE_ID=" SD_MESSAGE_SLEEP_STOP_STR,
                LOG_MESSAGE("System resumed."),
                "SLEEP=%s", arg_verb);

        arguments[1] = (char*) "post";
        (void) execute_directories(dirs, DEFAULT_TIMEOUT_USEC, NULL, NULL, arguments, NULL, EXEC_DIR_IGNORE_ERRORS);

        return r;
}
#else // 0
static int execute(Manager* m, char const* verb, char **modes, char **states) {
        char* arguments[] = {
                NULL,
                (char*) "pre",
                (char*) verb,
                NULL
        };
        static const char* const dirs[] = {
                SYSTEM_SLEEP_PATH,
                PKGSYSCONFDIR "/system-sleep",
                NULL
        };
        void* gather_args[] = {
                [STDOUT_GENERATE] = m,
                [STDOUT_COLLECT] = m,
                [STDOUT_CONSUME] = m,
        };
        int have_nvidia;
        unsigned vtnr = 0;
        int e;
        _cleanup_free_ char *l = NULL;
        char const* mode_location = strcmp( verb, "suspend") ? "/sys/power/disk" : "/sys/power/mem_sleep";

        assert(m);

        _cleanup_fclose_ FILE *f = NULL;
        _cleanup_(hibernate_location_freep) HibernateLocation *hibernate_location = NULL;
        int r;

        /* This file is opened first, so that if we hit an error,
         * we can abort before modifying any state. */
        f = fopen("/sys/power/state", "we");
        if (!f)
                return log_error_errno(errno, "Failed to open /sys/power/state: %m");

        setvbuf(f, NULL, _IONBF, 0);

        /* Configure hibernation settings if we are supposed to hibernate */
        if (!strv_isempty(modes)) {
                if (strcmp( verb, "suspend")) {
                        log_debug_elogind("%s", "Trying to find a hibernation location...");
                        r = find_hibernate_location(&hibernate_location);
                        if (r < 0)
                                return log_error_errno(r, "Failed to find location to hibernate to: %m");
                        if (r == 0) {
                                /* 0 means: no hibernation location was configured in the kernel so far, let's
                                * do it ourselves then. > 0 means: kernel already had a configured hibernation
                                * location which we shouldn't touch. */
                                log_debug_elogind("%s", "Writing new hibernation location...");
                                r = write_hibernate_location_info(hibernate_location);
                                if (r < 0)
                                        return log_error_errno(r, "Failed to prepare for hibernation: %m");
                        }
                }
                r = write_mode(modes);
                if (r < 0)
                        return log_error_errno(r, "Failed to write mode to %s: %m", mode_location);
        }

        m->callback_failed = false;
        m->callback_must_succeed = m->allow_suspend_interrupts;

        log_debug_elogind("Executing suspend hook scripts... (Must succeed: %s)",
                          m->callback_must_succeed ? "YES" : "no");

        r = execute_directories(dirs, DEFAULT_TIMEOUT_USEC, gather_output, gather_args, arguments, NULL, EXEC_DIR_NONE);

        log_debug_elogind("Result is %d (callback_failed: %s)", r, m->callback_failed ? "true" : "false");

        if (m->callback_must_succeed && (r || m->callback_failed)) {
                e = asprintf(&l, "A sleep script in %s or %s failed! [%d]\n"
                                 "The system %s has been cancelled!",
                             SYSTEM_SLEEP_PATH, PKGSYSCONFDIR "/system-sleep",
                             r, verb);
                if (e < 0) {
                        log_oom();
                        return -ENOMEM;
                }

                utmp_wall(l, "root", "n/a", logind_wall_tty_filter, m);

                log_struct_errno(LOG_ERR, r, "MESSAGE_ID=" SD_MESSAGE_SLEEP_STOP_STR,
                                 LOG_MESSAGE("A sleep script in %s or %s failed [%d]: %m\n"
                                             "The system %s has been cancelled!",
                                             SYSTEM_SLEEP_PATH, PKGSYSCONFDIR "/system-sleep",
                                             r, verb),
                                 "SLEEP=%s", verb);

                return -ECANCELED;
        }

        /* See whether we have an nvidia card to put to sleep */
        have_nvidia = nvidia_sleep(m, verb, &vtnr);

        log_struct(LOG_INFO,
                   "MESSAGE_ID=" SD_MESSAGE_SLEEP_START_STR,
                   LOG_MESSAGE("Suspending system..."),
                   "SLEEP=%s", verb);

        r = write_state(&f, states);
        if (r < 0)
                log_struct_errno(LOG_ERR, r,
                                 "MESSAGE_ID=" SD_MESSAGE_SLEEP_STOP_STR,
                                 LOG_MESSAGE("Failed to suspend system. System resumed again: %m"),
                                 "SLEEP=%s", verb);
        else
                log_struct(LOG_INFO,
                        "MESSAGE_ID=" SD_MESSAGE_SLEEP_STOP_STR,
                        LOG_MESSAGE("System resumed."),
                        "SLEEP=%s", verb);

        /* Wakeup a possibly put to sleep nvidia card */
        if (have_nvidia)
                nvidia_sleep(m, "resume", &vtnr);

        arguments[1] = (char*) "post";
        (void) execute_directories(dirs, DEFAULT_TIMEOUT_USEC, NULL, NULL, arguments, NULL, EXEC_DIR_IGNORE_ERRORS);

        return r;
}
#endif // 0

#if 0 /// elogind uses the values stored in its manager instance
static int execute_s2h(const SleepConfig *sleep_config) {
#else // 0
static int execute_s2h(Manager *sleep_config) {
#endif // 0
        _cleanup_close_ int tfd = -1;
        char buf[FORMAT_TIMESPAN_MAX];
        struct itimerspec ts = {};
        int r;

        assert(sleep_config);

        tfd = timerfd_create(CLOCK_BOOTTIME_ALARM, TFD_NONBLOCK|TFD_CLOEXEC);
        if (tfd < 0)
                return log_error_errno(errno, "Error creating timerfd: %m");

        log_debug("Set timerfd wake alarm for %s",
                  format_timespan(buf, sizeof(buf), sleep_config->hibernate_delay_sec, USEC_PER_SEC));

        timespec_store(&ts.it_value, sleep_config->hibernate_delay_sec);

        r = timerfd_settime(tfd, 0, &ts, NULL);
        if (r < 0)
                return log_error_errno(errno, "Error setting hibernate timer: %m");

#if 0 /// For the elogind extra stuff, we have to submit the manager instance
        r = execute(sleep_config->suspend_modes, sleep_config->suspend_states);
#else // 0
        r = execute(sleep_config, "suspend", sleep_config->suspend_modes, sleep_config->suspend_states);
#endif // 0
        if (r < 0)
                return r;

        r = fd_wait_for_event(tfd, POLLIN, 0);
        if (r < 0)
                return log_error_errno(r, "Error polling timerfd: %m");
        if (!FLAGS_SET(r, POLLIN)) /* We woke up before the alarm time, we are done. */
                return 0;

        tfd = safe_close(tfd);

        /* If woken up after alarm time, hibernate */
        log_debug("Attempting to hibernate after waking from %s timer",
                  format_timespan(buf, sizeof(buf), sleep_config->hibernate_delay_sec, USEC_PER_SEC));

#if 0 /// For the elogind extra stuff, we have to submit the manager instance
        r = execute(sleep_config->hibernate_modes, sleep_config->hibernate_states);
#else // 0
        r = execute(sleep_config, "hibernate", sleep_config->hibernate_modes, sleep_config->hibernate_states);
#endif // 0
        if (r < 0) {
                log_notice_errno(r, "Couldn't hibernate, will try to suspend again: %m");

#if 0 /// For the elogind extra stuff, we have to submit the manager instance
                r = execute(sleep_config->suspend_modes, sleep_config->suspend_states);
#else // 0
                r = execute(sleep_config, "suspend", sleep_config->suspend_modes, sleep_config->suspend_states);
#endif // 0
                if (r < 0)
                        return log_error_errno(r, "Could neither hibernate nor suspend, giving up: %m");
        }

        return 0;
}

#if 0 /// elogind calls execute() by itself and does not need another binary
static int help(void) {
        _cleanup_free_ char *link = NULL;
        int r;

        r = terminal_urlify_man("systemd-suspend.service", "8", &link);
        if (r < 0)
                return log_oom();

        printf("%s COMMAND\n\n"
               "Suspend the system, hibernate the system, or both.\n\n"
               "  -h --help              Show this help and exit\n"
               "  --version              Print version string and exit\n"
               "\nCommands:\n"
               "  suspend                Suspend the system\n"
               "  hibernate              Hibernate the system\n"
               "  hybrid-sleep           Both hibernate and suspend the system\n"
               "  suspend-then-hibernate Initially suspend and then hibernate\n"
               "                         the system after a fixed period of time\n"
               "\nSee the %s for details.\n"
               , program_invocation_short_name
               , link
        );

        return 0;
}

static int parse_argv(int argc, char *argv[]) {
        enum {
                ARG_VERSION = 0x100,
        };

        static const struct option options[] = {
                { "help",         no_argument,       NULL, 'h'           },
                { "version",      no_argument,       NULL, ARG_VERSION   },
                {}
        };

        int c;

        assert(argc >= 0);
        assert(argv);

        while ((c = getopt_long(argc, argv, "h", options, NULL)) >= 0)
                switch(c) {
                case 'h':
                        return help();

                case ARG_VERSION:
                        return version();

                case '?':
                        return -EINVAL;

                default:
                        assert_not_reached("Unhandled option");
                }

        if (argc - optind != 1)
                return log_error_errno(SYNTHETIC_ERRNO(EINVAL),
                                       "Usage: %s COMMAND",
                                       program_invocation_short_name);

        arg_verb = strdup(argv[optind]);
        if (!arg_verb)
                return log_oom();

        if (!STR_IN_SET(arg_verb, "suspend", "hibernate", "hybrid-sleep", "suspend-then-hibernate"))
                return log_error_errno(SYNTHETIC_ERRNO(EINVAL),
                                       "Unknown command '%s'.", arg_verb);

        return 1 /* work to do */;
}

static int run(int argc, char *argv[]) {
        bool allow;
        char **modes = NULL, **states = NULL;
        _cleanup_(free_sleep_configp) SleepConfig *sleep_config = NULL;
        int r;

        log_setup_service();

        r = parse_argv(argc, argv);
        if (r <= 0)
                return r;

        r = parse_sleep_config(&sleep_config);
        if (r < 0)
                return r;

        r = sleep_settings(arg_verb, sleep_config, &allow, &modes, &states);
        if (r < 0)
                return r;

        if (!allow)
                return log_error_errno(SYNTHETIC_ERRNO(EACCES),
                                       "Sleep mode \"%s\" is disabled by configuration, refusing.",
                                       arg_verb);

        if (streq(arg_verb, "suspend-then-hibernate"))
                return execute_s2h(sleep_config);
        else
                return execute(modes, states);
}

DEFINE_MAIN_FUNCTION(run);
#else // 0

int do_sleep(Manager *m, const char *verb) {
        bool allow;
        char** modes = NULL;
        char** states = NULL;
        int r;

        assert(verb);
        assert(m);

        arg_verb = (char*) verb;

        log_debug_elogind("%s called for %s", __func__, strnull(verb));

        /* Re-load the sleep configuration, so users can change their options
         * on-the-fly without having to reload elogind
         */
        r = parse_sleep_config(&m);
        if (r < 0)
                return r;

        /* Prepare the sleep configuration to execute */
        r = sleep_settings(arg_verb, m, &allow, &modes, &states);
        if (r < 0)
                return r;

        if (!allow)
                return log_error_errno(SYNTHETIC_ERRNO(EACCES),
                                       "Sleep mode \"%s\" is disabled by configuration, refusing.",
                                       arg_verb);

        /* Now execute either s2h or the regular sleep */
        if (streq(arg_verb, "suspend-then-hibernate"))
                r = execute_s2h(m);
        else
                r = execute(m, arg_verb, modes, states);

        return r;
}

#endif // 0
