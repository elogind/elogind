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


#include "bus-error.h"
#include "bus-util.h"
#include "eloginctl.h"
#include "logind-action.h"
#include "parse-util.h"
#include "process-util.h"
#include "sd-login.h"
#include "sd-messages.h"
#include "spawn-polkit-agent.h"
#include "string-util.h"
#include "strv.h"
#include "terminal-util.h"
#include "user-util.h"
#include "virt.h"


elogind_action arg_action            = _ACTION_INVALID;
bool           arg_ask_password      = true;
bool           arg_ignore_inhibitors = false;
bool           arg_no_wall           = false;
BusTransport   arg_transport         = BUS_TRANSPORT_LOCAL;
char**         arg_wall              = NULL;
usec_t         arg_when              = 0;

static const struct {
        HandleAction action;
        const char*  verb;
} action_table[_ACTION_MAX] = {
        [ACTION_POWEROFF]     = { HANDLE_POWEROFF,     "poweroff",    },
        [ACTION_REBOOT]       = { HANDLE_REBOOT,       "reboot",      },
        [ACTION_SUSPEND]      = { HANDLE_SUSPEND,      "suspend",     },
        [ACTION_HIBERNATE]    = { HANDLE_HIBERNATE,    "hibernate",   },
        [ACTION_HYBRID_SLEEP] = { HANDLE_HYBRID_SLEEP, "hybrid-sleep" },
};

static int elogind_set_wall_message(sd_bus* bus, const char* msg);

static enum elogind_action verb_to_action(const char *verb) {
        enum elogind_action i;

        for (i = _ACTION_INVALID; i < _ACTION_MAX; i++)
                if (streq_ptr(action_table[i].verb, verb))
                        return i;

        return _ACTION_INVALID;
}

static int check_inhibitors(sd_bus* bus, enum elogind_action a) {
        _cleanup_(sd_bus_message_unrefp) sd_bus_message *reply = NULL;
        _cleanup_strv_free_ char **sessions = NULL;
        const char *what, *who, *why, *mode;
        uint32_t uid, pid;
        unsigned c = 0;
        char **s;
        int r;

        if (arg_ignore_inhibitors)
                return 0;

        if (arg_when > 0)
                return 0;

        if (geteuid() == 0)
                return 0;

        if (!on_tty())
                return 0;

        r = sd_bus_call_method(
                        bus,
                        "org.freedesktop.login1",
                        "/org/freedesktop/login1",
                        "org.freedesktop.login1.Manager",
                        "ListInhibitors",
                        NULL,
                        &reply,
                        NULL);
        if (r < 0)
                /* If logind is not around, then there are no inhibitors... */
                return 0;

        r = sd_bus_message_enter_container(reply, SD_BUS_TYPE_ARRAY, "(ssssuu)");
        if (r < 0)
                return bus_log_parse_error(r);

        while ((r = sd_bus_message_read(reply, "(ssssuu)", &what, &who, &why, &mode, &uid, &pid)) > 0) {
                _cleanup_free_ char *comm = NULL, *user = NULL;
                _cleanup_strv_free_ char **sv = NULL;

                if (!streq(mode, "block"))
                        continue;

                sv = strv_split(what, ":");
                if (!sv)
                        return log_oom();

                if ((pid_t) pid < 0)
                        return log_error_errno(ERANGE, "Bad PID %"PRIu32": %m", pid);

                if (!strv_contains(sv,
                                   IN_SET(a,
                                          ACTION_HALT,
                                          ACTION_POWEROFF,
                                          ACTION_REBOOT,
                                          ACTION_KEXEC) ? "shutdown" : "sleep"))
                        continue;

                get_process_comm(pid, &comm);
                user = uid_to_name(uid);

                log_warning("Operation inhibited by \"%s\" (PID "PID_FMT" \"%s\", user %s), reason is \"%s\".",
                            who, (pid_t) pid, strna(comm), strna(user), why);

                c++;
        }
        if (r < 0)
                return bus_log_parse_error(r);

        r = sd_bus_message_exit_container(reply);
        if (r < 0)
                return bus_log_parse_error(r);

        /* Check for current sessions */
        sd_get_sessions(&sessions);
        STRV_FOREACH(s, sessions) {
                _cleanup_free_ char *type = NULL, *tty = NULL, *seat = NULL, *user = NULL, *service = NULL, *class = NULL;

                if (sd_session_get_uid(*s, &uid) < 0 || uid == getuid())
                        continue;

                if (sd_session_get_class(*s, &class) < 0 || !streq(class, "user"))
                        continue;

                if (sd_session_get_type(*s, &type) < 0 || (!streq(type, "x11") && !streq(type, "tty")))
                        continue;

                sd_session_get_tty(*s, &tty);
                sd_session_get_seat(*s, &seat);
                sd_session_get_service(*s, &service);
                user = uid_to_name(uid);

                log_warning("User %s is logged in on %s.", strna(user), isempty(tty) ? (isempty(seat) ? strna(service) : seat) : tty);
                c++;
        }

        if (c <= 0)
                return 0;

        log_error("Please retry operation after closing inhibitors and logging out other users.\nAlternatively, ignore inhibitors and users with 'loginctl -i %s'.",
                  action_table[a].verb);

        return -EPERM;
}

int elogind_cancel_shutdown(sd_bus *bus) {
        _cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
        int r;

        r = elogind_set_wall_message(bus, NULL);

        if (r < 0) {
                log_warning_errno(r, "Failed to set wall message, ignoring: %s",
                                  bus_error_message(&error, r));
                sd_bus_error_free(&error);
        }

        r = sd_bus_call_method(
                        bus,
                        "org.freedesktop.login1",
                        "/org/freedesktop/login1",
                        "org.freedesktop.login1.Manager",
                        "CancelScheduledShutdown",
                        &error,
                        NULL, NULL);
        if (r < 0)
                return log_warning_errno(r, "Failed to talk to elogind, shutdown hasn't been cancelled: %s", bus_error_message(&error, r));

        return 0;
}

void elogind_cleanup(void) {
        polkit_agent_close();
        strv_free(arg_wall);
}

static void elogind_log_special(enum elogind_action a) {
#ifdef ENABLE_DEBUG_ELOGIND
        switch (a) {
        case ACTION_HALT:
                log_struct(LOG_INFO,
                           LOG_MESSAGE("Halt action called."),
                           LOG_MESSAGE_ID(SD_MESSAGE_SHUTDOWN),
                           NULL);
                break;
        case ACTION_POWEROFF:
                log_struct(LOG_INFO,
                           LOG_MESSAGE("Poweroff action called."),
                           LOG_MESSAGE_ID(SD_MESSAGE_SHUTDOWN),
                           NULL);
                break;
        case ACTION_REBOOT:
                log_struct(LOG_INFO,
                           LOG_MESSAGE("Reboot action called."),
                           LOG_MESSAGE_ID(SD_MESSAGE_SHUTDOWN),
                           NULL);
                break;
        case ACTION_KEXEC:
                log_struct(LOG_INFO,
                           LOG_MESSAGE("KExec action called."),
                           LOG_MESSAGE_ID(SD_MESSAGE_SHUTDOWN),
                           NULL);
                break;
        case ACTION_SUSPEND:
                log_struct(LOG_INFO,
                           LOG_MESSAGE("Suspend action called."),
                           LOG_MESSAGE_ID(SD_MESSAGE_SLEEP_START),
                           NULL);
                break;
        case ACTION_HIBERNATE:
                log_struct(LOG_INFO,
                           LOG_MESSAGE("Hibernate action called."),
                           LOG_MESSAGE_ID(SD_MESSAGE_SLEEP_START),
                           NULL);
        case ACTION_HYBRID_SLEEP:
                log_struct(LOG_INFO,
                           LOG_MESSAGE("Hybrid-Sleep action called."),
                           LOG_MESSAGE_ID(SD_MESSAGE_SLEEP_START),
                           NULL);
        case ACTION_CANCEL_SHUTDOWN:
                log_struct(LOG_INFO,
                           LOG_MESSAGE("Cancel Shutdown called."),
                           LOG_MESSAGE_ID(SD_MESSAGE_SHUTDOWN),
                           NULL);
                break;
        default:
                break;
        }
#endif // ENABLE_DEBUG_ELOGIND
}

static int elogind_reboot(sd_bus *bus, enum elogind_action a) {
        _cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
        const char *method  = NULL;
        int r;
        static const char *table[_ACTION_MAX] = {
                [ACTION_REBOOT]          = "The system is going down for reboot NOW!",
                [ACTION_POWEROFF]        = "The system is going down for power-off NOW!"
        };

        if (!bus)
                return -EIO;

        switch (a) {

        case ACTION_POWEROFF:
                method = "PowerOff";
                break;

        case ACTION_REBOOT:
                method = "Reboot";
                break;

        case ACTION_SUSPEND:
                method = "Suspend";
                break;

        case ACTION_HIBERNATE:
                method = "Hibernate";
                break;

        case ACTION_HYBRID_SLEEP:
                method = "HybridSleep";
                break;

        default:
                return -EINVAL;
        }

        polkit_agent_open_if_enabled();
        r = elogind_set_wall_message(bus, table[a]);

        if (r < 0) {
                log_warning_errno(r, "Failed to set wall message, ignoring: %s",
                                  bus_error_message(&error, r));
                sd_bus_error_free(&error);
        }

        /* Now call elogind itself to request the operation */
        r = sd_bus_call_method(
                        bus,
                        "org.freedesktop.login1",
                        "/org/freedesktop/login1",
                        "org.freedesktop.login1.Manager",
                        method,
                        &error,
                        NULL,
                        "b", arg_ask_password);

        if (r < 0)
                log_error("Failed to execute operation: %s", bus_error_message(&error, r));

        return r;
}

static int elogind_schedule_shutdown(sd_bus *bus, enum elogind_action a) {
        _cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
        const char *method  = NULL;
        int r;

        if (!bus)
                return -EIO;

        switch (a) {

        case ACTION_POWEROFF:
                method = "poweroff";
                break;

        case ACTION_REBOOT:
                method = "reboot";
                break;

        default:
                return -EINVAL;
        }

        r = elogind_set_wall_message(bus, NULL);

        if (r < 0) {
                log_warning_errno(r, "Failed to set wall message, ignoring: %s",
                                  bus_error_message(&error, r));
                sd_bus_error_free(&error);
        }

        r = sd_bus_call_method(
                        bus,
                        "org.freedesktop.login1",
                        "/org/freedesktop/login1",
                        "org.freedesktop.login1.Manager",
                        "ScheduleShutdown",
                        &error,
                        NULL,
                        "st",
                        method,
                        arg_when);

        if (r < 0)
                log_error("Failed to execute operation: %s", bus_error_message(&error, r));

        return r;
}

static int elogind_set_wall_message(sd_bus* bus, const char* msg) {
        _cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
        _cleanup_free_ char *m = NULL;
        int r;

        if (strv_extend(&arg_wall, msg) < 0)
                return log_oom();

        m = strv_join(arg_wall, " ");
        if (!m)
                return log_oom();

        r = sd_bus_call_method(
                        bus,
                        "org.freedesktop.login1",
                        "/org/freedesktop/login1",
                        "org.freedesktop.login1.Manager",
                        "SetWallMessage",
                        &error,
                        NULL,
                        "sb",
                        m,
                        !arg_no_wall);

        if (r < 0)
                return log_warning_errno(r, "Failed to set wall message, ignoring: %s", bus_error_message(&error, r));

        return 0;
}

static int parse_shutdown_time_spec(const char *t, usec_t *_u) {
        assert(t);
        assert(_u);

        if (streq(t, "now"))
                *_u = 0;
        else if (!strchr(t, ':')) {
                uint64_t u;

                if (safe_atou64(t, &u) < 0)
                        return -EINVAL;

                *_u = now(CLOCK_REALTIME) + USEC_PER_MINUTE * u;
        } else {
                char *e = NULL;
                long hour, minute;
                struct tm tm = {};
                time_t s;
                usec_t n;

                errno = 0;
                hour = strtol(t, &e, 10);
                if (errno > 0 || *e != ':' || hour < 0 || hour > 23)
                        return -EINVAL;

                minute = strtol(e+1, &e, 10);
                if (errno > 0 || *e != 0 || minute < 0 || minute > 59)
                        return -EINVAL;

                n = now(CLOCK_REALTIME);
                s = (time_t) (n / USEC_PER_SEC);

                assert_se(localtime_r(&s, &tm));

                tm.tm_hour = (int) hour;
                tm.tm_min = (int) minute;
                tm.tm_sec = 0;

                assert_se(s = mktime(&tm));

                *_u = (usec_t) s * USEC_PER_SEC;

                while (*_u <= n)
                        *_u += USEC_PER_DAY;
        }

        return 0;
}

void polkit_agent_open_if_enabled(void) {

        /* Open the polkit agent as a child process if necessary */

        if (!arg_ask_password)
                return;

        if (arg_transport != BUS_TRANSPORT_LOCAL)
                return;

        polkit_agent_open();
}

int start_special(int argc, char *argv[], void *userdata) {
        sd_bus *bus = userdata;
        enum elogind_action a;
        int r;
        char** wall = NULL;

        assert(argv);

        a = verb_to_action(argv[0]);

        elogind_log_special(a);

        /* No power off actions in chroot environments */
        if ( IN_SET(a, ACTION_POWEROFF, ACTION_REBOOT)
          && (running_in_chroot() > 0) ) {
                log_info("Running in chroot, ignoring request.");
                return 0;
        }

        /* Check time arguments */
        if ( IN_SET(a, ACTION_POWEROFF, ACTION_REBOOT)
          && (argc > 1)
          && (arg_action != ACTION_CANCEL_SHUTDOWN) ) {
                r = parse_shutdown_time_spec(argv[1], &arg_when);
                if (r < 0) {
                        log_error("Failed to parse time specification: %s", argv[optind]);
                        return r;
                }
        } else
                arg_when = now(CLOCK_REALTIME) + USEC_PER_MINUTE;

        /* The optional user wall message must be set */
        if ((argc > 1) && (arg_action == ACTION_CANCEL_SHUTDOWN) )
                /* No time argument for shutdown cancel */
                wall = argv + 1;
        else if (argc > 2)
                /* We skip the time argument */
                wall = argv + 2;

        if (wall) {
                arg_wall = strv_copy(wall);
                if (!arg_wall)
                        return log_oom();
        }

        /* Switch to cancel shutdown, if a shutdown action was requested,
           and the option to cancel it was set: */
        if ( IN_SET(a, ACTION_POWEROFF, ACTION_REBOOT)
          && (arg_action == ACTION_CANCEL_SHUTDOWN) )
                return elogind_cancel_shutdown(bus);

        r = check_inhibitors(bus, a);
        if (r < 0)
                return r;

        /* Perform requested action */
        if (IN_SET(a,
                   ACTION_POWEROFF,
                   ACTION_REBOOT,
                   ACTION_SUSPEND,
                   ACTION_HIBERNATE,
                   ACTION_HYBRID_SLEEP)) {
                if (arg_when > 0)
                        return elogind_schedule_shutdown(bus, a);
                else
                        return elogind_reboot(bus, a);
        }

        return -EOPNOTSUPP;
}

