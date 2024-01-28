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
#include "reboot-util.h"
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
bool           arg_dry_run           = false;
bool           arg_ignore_inhibitors = false;
bool           arg_no_wall           = false;
BusTransport   arg_transport         = BUS_TRANSPORT_LOCAL;
char**         arg_wall              = NULL;
usec_t         arg_when              = 0;
bool           arg_firmware_setup    = false;
usec_t         arg_boot_loader_menu  = USEC_INFINITY;
const char*    arg_boot_loader_entry = NULL;

static const struct {
        HandleAction action;
        const char*  verb;
} action_table[_ACTION_MAX] = {
        [ACTION_HALT]                   = { HANDLE_HALT,                   "halt"         },
        [ACTION_POWEROFF]               = { HANDLE_POWEROFF,               "poweroff",    },
        [ACTION_REBOOT]                 = { HANDLE_REBOOT,                 "reboot",      },
        [ACTION_KEXEC]                  = { HANDLE_KEXEC,                  "kexec",       },
        [ACTION_SUSPEND]                = { HANDLE_SUSPEND,                "suspend",     },
        [ACTION_HIBERNATE]              = { HANDLE_HIBERNATE,              "hibernate",   },
        [ACTION_HYBRID_SLEEP]           = { HANDLE_HYBRID_SLEEP,           "hybrid-sleep" },
        [ACTION_SUSPEND_THEN_HIBERNATE] = { HANDLE_SUSPEND_THEN_HIBERNATE, "suspend-then-hibernate" }
        /* ACTION_CANCEL_SHUTDOWN is handled differently */
};

static int elogind_set_wall_message(sd_bus* bus, const char* msg);

static enum elogind_action verb_to_action(const char *verb) {
        enum elogind_action i;

        for (i = _ACTION_INVALID; i < _ACTION_MAX; i++)
                if (streq_ptr(action_table[i].verb, verb))
                        return i;

        return _ACTION_INVALID;
}

/* Original:
 * systemctl/systemctl.c:3314:logind_check_inhibitors()
 */
static int check_inhibitors(sd_bus* bus, enum elogind_action a) {
        _cleanup_(sd_bus_message_unrefp) sd_bus_message *reply = NULL;
        _cleanup_strv_free_ char **sessions = NULL;
        const char *what, *who, *why, *mode;
        uint32_t uid, pid;
        unsigned c = 0;
        int r;

        if (arg_ignore_inhibitors)
                return 0;

        if (arg_when > 0)
                return 0;

        if (geteuid() == 0)
                return 0;

        if (!on_tty())
                return 0;

        if (arg_transport != BUS_TRANSPORT_LOCAL)
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

                if (!pid_is_valid((pid_t) pid)) {
                        log_error("Invalid PID "PID_FMT".", (pid_t) pid);
                        return -ERANGE;
                }

                if (!strv_contains(sv,
                                   IN_SET(a,
                                          ACTION_HALT,
                                          ACTION_POWEROFF,
                                          ACTION_REBOOT,
                                          ACTION_KEXEC) ? "shutdown" : "sleep"))
                        continue;

                pid_get_comm(pid, &comm);
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

                if (sd_session_get_type(*s, &type) < 0 || !STR_IN_SET(type, "x11", "wayland", "tty", "mir"))
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

        log_error("Please retry operation after closing inhibitors and logging out other users.\nAlternatively, ignore inhibitors and users with 'loginctl %s -i'.",
                  action_table[a].verb);

        return -EPERM;
}

/* Original:
 * systemctl/systemctl.c:8683:logind_cancel_shutdown()
 */
int elogind_cancel_shutdown(sd_bus *bus) {
        _cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
        int r;

        (void) elogind_set_wall_message(bus, NULL);

        r = sd_bus_call_method(
                        bus,
                        "org.freedesktop.login1",
                        "/org/freedesktop/login1",
                        "org.freedesktop.login1.Manager",
                        "CancelScheduledShutdown",
                        &error,
                        NULL, NULL);
        if (r < 0)
                return log_warning_errno(r,
                        "Failed to talk to elogind, shutdown hasn't been cancelled: %s",
                        bus_error_message(&error, r));

        return 0;
}

/* Only a little helper for cleaning up elogind specific extra stuff. */
void elogind_cleanup(void) {
        polkit_agent_close();
        strv_free(arg_wall);
}

/* Little debug log helper, helps debugging systemctl comands we mimic. */
static void elogind_log_special(enum elogind_action a) {
#if ENABLE_DEBUG_ELOGIND
        switch (a) {
        case ACTION_HALT:
                log_struct(LOG_INFO,
                           LOG_MESSAGE("Halt action called."),
                           "MESSAGE_ID=" SD_MESSAGE_SHUTDOWN_STR,
                           NULL);
                break;
        case ACTION_POWEROFF:
                log_struct(LOG_INFO,
                           LOG_MESSAGE("Poweroff action called."),
                           "MESSAGE_ID=" SD_MESSAGE_SHUTDOWN_STR,
                           NULL);
                break;
        case ACTION_REBOOT:
                log_struct(LOG_INFO,
                           LOG_MESSAGE("Reboot action called."),
                           "MESSAGE_ID=" SD_MESSAGE_SHUTDOWN_STR,
                           NULL);
                break;
        case ACTION_KEXEC:
                log_struct(LOG_INFO,
                           LOG_MESSAGE("KExec action called."),
                           "MESSAGE_ID=" SD_MESSAGE_SHUTDOWN_STR,
                           NULL);
                break;
        case ACTION_SUSPEND:
                log_struct(LOG_INFO,
                           LOG_MESSAGE("Suspend action called."),
                           "MESSAGE_ID=" SD_MESSAGE_SLEEP_START_STR,
                           NULL);
                break;
        case ACTION_HIBERNATE:
                log_struct(LOG_INFO,
                           LOG_MESSAGE("Hibernate action called."),
                           "MESSAGE_ID=" SD_MESSAGE_SLEEP_START_STR,
                           NULL);
                break;
        case ACTION_HYBRID_SLEEP:
                log_struct(LOG_INFO,
                           LOG_MESSAGE("Hybrid-Sleep action called."),
                           "MESSAGE_ID=" SD_MESSAGE_SLEEP_START_STR,
                           NULL);
                break;
        case ACTION_SUSPEND_THEN_HIBERNATE:
                log_struct(LOG_INFO,
                           LOG_MESSAGE("Suspend-Then-Hibernate action called."),
                           "MESSAGE_ID=" SD_MESSAGE_SLEEP_START_STR,
                           NULL);
                break;
        case ACTION_CANCEL_SHUTDOWN:
                log_struct(LOG_INFO,
                           LOG_MESSAGE("Cancel Shutdown called."),
                           "MESSAGE_ID=" SD_MESSAGE_SHUTDOWN_STR,
                           NULL);
                break;
        default:
                break;
        }
#endif // ENABLE_DEBUG_ELOGIND
}

/* Original:
 * systemctl/systemctl.c:3242:logind_reboot()
 */
static int elogind_reboot(sd_bus *bus, enum elogind_action a) {
        _cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
        const char *method  = NULL, *description = NULL;
        int r;
        static const char *table[_ACTION_MAX] = {
                [ACTION_HALT]     = "The system is going down for halt NOW!",
                [ACTION_REBOOT]   = "The system is going down for reboot NOW!",
                [ACTION_POWEROFF] = "The system is going down for power-off NOW!"
        };

        if (!bus)
                return -EIO;

        switch (a) {

        case ACTION_HALT:
                method = "Halt";
                description = "halt system";
                break;

        case ACTION_POWEROFF:
                method = "PowerOff";
                description = "power off system";
                break;

        case ACTION_REBOOT:
                method = "Reboot";
                description = "reboot system";
                break;

        case ACTION_SUSPEND:
                method = "Suspend";
                description = "suspend system";
                break;

        case ACTION_HIBERNATE:
                method = "Hibernate";
                description = "hibernate system";
                break;

        case ACTION_HYBRID_SLEEP:
                method = "HybridSleep";
                description = "put system into hybrid sleep";
                break;

        case ACTION_SUSPEND_THEN_HIBERNATE:
                method = "SuspendThenHibernate";
                description = "put system into suspend followed by hibernate";
                break;

        default:
                return -EINVAL;
        }

        /* No need for polkit_agent_open_maybe() in elogind. Do it directly. */
        polkit_agent_open_if_enabled(arg_transport, arg_ask_password);

        if ( IN_SET(a, ACTION_HALT, ACTION_POWEROFF, ACTION_REBOOT) )
                (void) elogind_set_wall_message(bus, table[a]);

        log_debug("%s org.freedesktop.login1.Manager %s dbus call.", arg_dry_run ? "Would execute" : "Executing", method);

        if (arg_dry_run)
                return 0;

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
                return log_error_errno(r, "Failed to %s via elogind: %s",
                                       description,
                                       bus_error_message(&error, r));

        return 0;
}

/* Original:
 * systemctl/systemctl.c:8553:logind_schedule_shutdown()
 */
static int elogind_schedule_shutdown(sd_bus *bus, enum elogind_action a) {
        _cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
        const char *action;
        int r;

        if (!bus)
                return -EIO;

        switch (a) {
        case ACTION_HALT:
                action = "halt";
                break;
        case ACTION_POWEROFF:
                action = "poweroff";
                break;
        case ACTION_KEXEC:
                action = "kexec";
                break;
        case ACTION_REBOOT:
        default:
                action = "reboot";
                break;
        }

        if (arg_dry_run)
                action = strjoina("dry-", action);

        (void) elogind_set_wall_message(bus, NULL);

        r = sd_bus_call_method(
                        bus,
                        "org.freedesktop.login1",
                        "/org/freedesktop/login1",
                        "org.freedesktop.login1.Manager",
                        "ScheduleShutdown",
                        &error,
                        NULL,
                        "st",
                        action,
                        arg_when);

        if (r < 0)
                return log_warning_errno(r,
                                "Failed to call ScheduleShutdown in logind, proceeding with immediate shutdown: %s",
                                bus_error_message(&error, r));

        return 0;
}

/* Original:
 * systemctl/systemctl.c:3204:logind_set_wall_message()
 * (Tweaked to allow an extra message to be appended.)
 */
static int elogind_set_wall_message(sd_bus* bus, const char* msg) {
        _cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
        _cleanup_free_ char *m = NULL;
        int r;

        if (strv_extend(&arg_wall, msg) < 0)
                return log_oom();

        m = strv_join(arg_wall, " ");
        if (!m)
                return log_oom();

        log_debug("%s wall message \"%s\".", arg_dry_run ? "Would set" : "Setting", m);
        if (arg_dry_run)
                return 0;

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

#ifdef ENABLE_EFI_TODO /// @todo EFI - needs change to support UEFI boot.
/* Original:
 * systemctl/systemctl.c:7956:help_boot_loader_entry()
 */
static int help_boot_loader_entry(sd_bus *bus) {
        _cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
        _cleanup_free_ char **l = NULL;
        char **i;
        int r;

        r = sd_bus_get_property_strv(
                        bus,
                        "org.freedesktop.login1",
                        "/org/freedesktop/login1",
                        "org.freedesktop.login1.Manager",
                        "BootLoaderEntries",
                        &error,
                        &l);
        if (r < 0)
                return log_error_errno(r, "Failed to enumerate boot loader entries: %s", bus_error_message(&error, r));

        if (strv_isempty(l))
                return log_error_errno(SYNTHETIC_ERRNO(ENODATA), "No boot loader entries discovered.");

        STRV_FOREACH(i, l)
                puts(*i);

        return 0;
}
#endif // ENABLE_EFI_TODO

/* Original:
 * systemctl/systemctl.c:7743:parse_shutdown_time_spec()
 */
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

#if ENABLE_EFI
/* Original:
 * systemctl/systemctl.c:3383:prepare_firmware_setup()
**/
static int prepare_firmware_setup(sd_bus* bus) {

        if (!arg_firmware_setup)
                return 0;

        _cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
        int r;

        r = sd_bus_call_method(
                        bus,
                        "org.freedesktop.login1",
                        "/org/freedesktop/login1",
                        "org.freedesktop.login1.Manager",
                        "SetRebootToFirmwareSetup",
                        &error,
                        NULL,
                        "b", true);
        if (r < 0)
                return log_error_errno(r, "Cannot indicate to EFI to boot into setup mode: %s", bus_error_message(&error, r));

        return 0;
}
#else // 0
static int prepare_firmware_setup(sd_bus* bus) {
        return 0;
}
#endif // ENABLE_EFI

#ifdef ENABLE_EFI_TODO /// @todo EFI - needs change to support UEFI boot.
/* Original:
 * systemctl/systemctl.c:3416:prepare_boot_loader_menu()
**/
static int prepare_boot_loader_menu(sd_bus* bus) {

        if (arg_boot_loader_menu == USEC_INFINITY)
                return 0;

        _cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
        int r;

        r = sd_bus_call_method(
                        bus,
                        "org.freedesktop.login1",
                        "/org/freedesktop/login1",
                        "org.freedesktop.login1.Manager",
                        "SetRebootToBootLoaderMenu",
                        &error,
                        NULL,
                        "t", arg_boot_loader_menu);
        if (r < 0)
                return log_error_errno(r, "Cannot indicate to boot loader to enter boot loader entry menu: %s", bus_error_message(&error, r));

        return 0;
}

/* Original:
 * systemctl/systemctl.c:3449:prepare_boot_loader_entry()
**/
static int prepare_boot_loader_entry(sd_bus* bus) {

        if (!arg_boot_loader_entry)
                return 0;

        _cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
        int r;

        if (streq(arg_boot_loader_entry, "help")) { /* Yes, this means, "help" is not a valid boot loader entry name we can deal with */
                r = help_boot_loader_entry(bus);
                if (r < 0)
                        return r;

                return 1; // no execution after asking for help
        }

        r = sd_bus_call_method(
                        bus,
                        "org.freedesktop.login1",
                        "/org/freedesktop/login1",
                        "org.freedesktop.login1.Manager",
                        "SetRebootToBootLoaderEntry",
                        &error,
                        NULL,
                        "s", arg_boot_loader_entry);
        if (r < 0)
                return log_error_errno(r, "Cannot set boot into loader entry '%s': %s", arg_boot_loader_entry, bus_error_message(&error, r));

        return 0;
}
#else // 0
static int prepare_boot_loader_menu(sd_bus* bus) {
        return 0;
}
static int prepare_boot_loader_entry(sd_bus* bus) {
        return 0;
}
#endif // ENABLE_EFI_TODO

/* Original:
 * systemctl/systemctl.c:3482:start_special()
 * However, this elogind variant is very different from the original.
 */
int start_special(int argc, char *argv[], void *userdata) {
        sd_bus *bus = userdata;
        enum elogind_action a;
        int r;
        char** wall = NULL;

        assert(argv);

        a = verb_to_action(argv[0]);

        elogind_log_special(a);

        /* For poweroff and reboot, some extra checks are performed: */
        if ( IN_SET(a, ACTION_POWEROFF, ACTION_REBOOT) ) {

                /* No power off actions in chroot environments */
                if ( running_in_chroot() > 0 ) {
                        log_info("Running in chroot, ignoring request.");
                        return 0;
                }

                /* Check time argument */
                if ( (argc > 1) && (ACTION_CANCEL_SHUTDOWN != arg_action)) {
                        r = parse_shutdown_time_spec(argv[1], &arg_when);
                        if (r < 0) {
                                log_error("Failed to parse time specification: %s", argv[optind]);
                                return r;
                        }
                }

                /* The optional user wall message must be set */
                if ( (argc > 1)
                  && ( (arg_action == ACTION_CANCEL_SHUTDOWN)
                    || (0 == arg_when) ) )
                        /* No time argument for shutdown cancel, or no
                         * time argument given. */
                        wall = argv + 1;
                else if (argc > 2)
                        /* We skip the time argument */
                        wall = argv + 2;

                if (wall) {
                        arg_wall = strv_copy(wall);
                        if (!arg_wall)
                                return log_oom();
                }
        }

        /* Switch to cancel shutdown, if a shutdown action was requested,
           and the option to cancel it was set: */
        if ( IN_SET(a, ACTION_POWEROFF, ACTION_REBOOT)
          && (arg_action == ACTION_CANCEL_SHUTDOWN) )
                return elogind_cancel_shutdown(bus);

        r = check_inhibitors(bus, a);
        if (r < 0)
                return r;

        r = prepare_firmware_setup(bus);
        if (r < 0)
                return r;

        r = prepare_boot_loader_menu(bus);
        if (r < 0)
                return r;

        r = prepare_boot_loader_entry(bus);
        if (r < 0)
                return r;
        if (r > 0)
                return 0; // Asked for help, no execution, then

        if (a == ACTION_REBOOT && argc > 1) {
                r = update_reboot_parameter_and_warn(argv[1], false);
                if (r < 0)
                        return r;
        }

        /* Perform requested action */
        if (IN_SET(a,
                   ACTION_POWEROFF,
                   ACTION_REBOOT,
                   ACTION_SUSPEND,
                   ACTION_HIBERNATE,
                   ACTION_HYBRID_SLEEP,
                   ACTION_SUSPEND_THEN_HIBERNATE)) {
                if (arg_when > 0)
                        return elogind_schedule_shutdown(bus, a);
                else
                        return elogind_reboot(bus, a);
        }

        return -EOPNOTSUPP;
}

