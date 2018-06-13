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


#include "bus-common-errors.h"
#include "bus-error.h"
#include "bus-util.h"
#include "elogind-dbus.h"
#include "fd-util.h"
#include "process-util.h"
#include "sd-messages.h"
#include "sleep.h"
#include "sleep-config.h"
#include "string-util.h"
#include "strv.h"
#include "update-utmp.h"
#include "user-util.h"


static int bus_manager_log_shutdown(
                Manager *m,
                InhibitWhat w,
                HandleAction action) {

         const char *p, *q;

         assert(m);

         if (w != INHIBIT_SHUTDOWN)
                 return 0;

         switch (action) {
         case HANDLE_POWEROFF:
                 p = "MESSAGE=System is powering down.";
                 q = "SHUTDOWN=power-off";
                 break;
         case HANDLE_HALT:
                 p = "MESSAGE=System is halting.";
                 q = "SHUTDOWN=halt";
                 break;
         case HANDLE_REBOOT:
                 p = "MESSAGE=System is rebooting.";
                 q = "SHUTDOWN=reboot";
                 break;
         case HANDLE_KEXEC:
                 p = "MESSAGE=System is rebooting with kexec.";
                 q = "SHUTDOWN=kexec";
                 break;
         default:
                 p = "MESSAGE=System is shutting down.";
                 q = NULL;
         }

        if (isempty(m->wall_message))
                p = strjoina(p, ".");
        else
                p = strjoina(p, " (", m->wall_message, ").");

        return log_struct(LOG_NOTICE,
                          "MESSAGE_ID=" SD_MESSAGE_SHUTDOWN_STR,
                          p,
                          q,
                          NULL);
}

/* elogind specific helper to make HALT and REBOOT possible. */
static int run_helper(const char *helper) {
        pid_t pid = 0;
        int   r   = 0;

        r = safe_fork_full(helper, NULL, 0, FORK_RESET_SIGNALS|FORK_DEATHSIG|FORK_CLOSE_ALL_FDS|FORK_WAIT, &pid);

        if (r < 0)
                return log_error_errno(errno, "Failed to fork run %s: %m", helper);

        if (pid == 0) {
                /* Child */

                execlp(helper, helper, NULL);
                log_error_errno(errno, "Failed to execute %s: %m", helper);
                _exit(EXIT_FAILURE);
        }

        return r;
}

/* elogind specific executor */
static int shutdown_or_sleep(Manager *m, HandleAction action) {

        assert(m);

        switch (action) {
        case HANDLE_POWEROFF:
                return run_helper(POWEROFF);
        case HANDLE_REBOOT:
                return run_helper(REBOOT);
        case HANDLE_HALT:
                return run_helper(HALT);
        case HANDLE_KEXEC:
                return run_helper(KEXEC);
        case HANDLE_SUSPEND:
                return do_sleep("suspend", m->suspend_mode, m->suspend_state);
        case HANDLE_HIBERNATE:
                return do_sleep("hibernate", m->hibernate_mode, m->hibernate_state);
        case HANDLE_HYBRID_SLEEP:
                return do_sleep("hybrid-sleep", m->hybrid_sleep_mode, m->hybrid_sleep_state);
        default:
                return -EINVAL;
        }
}

static int execute_shutdown_or_sleep(
                Manager *m,
                InhibitWhat w,
                HandleAction action,
                sd_bus_error *error) {

        char** argv_utmp = NULL;
        int r;

        assert(m);
        assert(w >= 0);
        assert(w < _INHIBIT_WHAT_MAX);

        bus_manager_log_shutdown(m, w, action);

        if (IN_SET(action, HANDLE_HALT, HANDLE_POWEROFF, HANDLE_REBOOT)) {

                /* As we have no systemd update-utmp daemon running, we have to
                 * set the relevant utmp/wtmp entries ourselves.
                 */

                if (strv_extend(&argv_utmp, "elogind") < 0)
                        return log_oom();

                if (HANDLE_REBOOT == action) {
                        if (strv_extend(&argv_utmp, "reboot") < 0)
                                return log_oom();
                } else {
                        if (strv_extend(&argv_utmp, "shutdown") < 0)
                                return log_oom();
                }

                 /* This comes from our patched update-utmp/update-utmp.c */
                update_utmp(2, argv_utmp, m->bus);
                strv_free(argv_utmp);
        }

        /* Now perform the requested action */
        r = shutdown_or_sleep(m, action);

        /* no more pending actions, whether this failed or not */
        m->pending_action = HANDLE_IGNORE;

        /* As elogind can not rely on a systemd manager to call all
         * sleeping processes to wake up, we have to tell them all
         * by ourselves. */
        if (w == INHIBIT_SLEEP) {
                (void) send_prepare_for(m, w, false);
                m->action_what = 0;
        } else
                m->action_what = w;

        if (r < 0)
                return r;

        /* Make sure the lid switch is ignored for a while */
        manager_set_lid_switch_ignore(m, now(CLOCK_MONOTONIC) + m->holdoff_timeout_usec);

        return 0;
}

int manager_dispatch_delayed(Manager *manager, bool timeout) {

        _cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
        Inhibitor *offending = NULL;
        int r;

        assert(manager);

        if ( (0 == manager->action_what) || (HANDLE_IGNORE == manager->pending_action) )
                return 0;

        if (manager_is_inhibited(manager, manager->action_what, INHIBIT_DELAY, NULL, false, false, 0, &offending)) {
                _cleanup_free_ char *comm = NULL, *u = NULL;

                if (!timeout)
                        return 0;

                (void) get_process_comm(offending->pid, &comm);
                u = uid_to_name(offending->uid);

                log_notice("Delay lock is active (UID "UID_FMT"/%s, PID "PID_FMT"/%s) but inhibitor timeout is reached.",
                           offending->uid, strna(u),
                           offending->pid, strna(comm));
        }

        /* Actually do the operation */
        r = execute_shutdown_or_sleep(manager, manager->action_what, manager->pending_action, &error);
        if (r < 0) {
                log_warning("Failed to send delayed message: %s", bus_error_message(&error, r));

                manager->pending_action = HANDLE_IGNORE;
                manager->action_what    = 0;
                /* It is not a critical error for elogind if suspending fails */
        }

        return 1;
}

static int delay_shutdown_or_sleep(
                Manager *m,
                InhibitWhat w,
                HandleAction action) {

        int r;
        usec_t timeout_val;

        assert(m);
        assert(w >= 0);
        assert(w < _INHIBIT_WHAT_MAX);

        timeout_val = now(CLOCK_MONOTONIC) + m->inhibit_delay_max;

        if (m->inhibit_timeout_source) {
                r = sd_event_source_set_time(m->inhibit_timeout_source, timeout_val);
                if (r < 0)
                        return log_error_errno(r, "sd_event_source_set_time() failed: %m");

                r = sd_event_source_set_enabled(m->inhibit_timeout_source, SD_EVENT_ONESHOT);
                if (r < 0)
                        return log_error_errno(r, "sd_event_source_set_enabled() failed: %m");
        } else {
                r = sd_event_add_time(m->event, &m->inhibit_timeout_source, CLOCK_MONOTONIC,
                                      timeout_val, 0, manager_inhibit_timeout_handler, m);
                if (r < 0)
                        return r;
        }

        m->pending_action = action;
        m->action_what = w;

        return 0;
}

int bus_manager_shutdown_or_sleep_now_or_later(
                Manager *m,
                HandleAction action,
                InhibitWhat w,
                sd_bus_error *error) {

        bool delayed;
        int r;

        assert(m);
        assert(w >= 0);
        assert(w <= _INHIBIT_WHAT_MAX);

        /* Tell everybody to prepare for shutdown/sleep */
        (void) send_prepare_for(m, w, true);

        delayed =
                m->inhibit_delay_max > 0 &&
                manager_is_inhibited(m, w, INHIBIT_DELAY, NULL, false, false, 0, NULL);

        log_debug_elogind("%s called for %s (%sdelayed)", __FUNCTION__,
                          handle_action_to_string(action),
                          delayed ? "" : "NOT ");

        if (delayed)
                /* Shutdown is delayed, keep in mind what we
                 * want to do, and start a timeout */
                r = delay_shutdown_or_sleep(m, w, action);
        else
                /* Shutdown is not delayed, execute it
                 * immediately */
                r = execute_shutdown_or_sleep(m, w, action, error);

        return r;
}

static int method_do_shutdown_or_sleep(
                Manager *m,
                sd_bus_message *message,
                HandleAction sleep_action,
                InhibitWhat w,
                const char *action,
                const char *action_multiple_sessions,
                const char *action_ignore_inhibit,
                const char *sleep_verb,
                sd_bus_error *error) {

        int interactive, r;

        assert(m);
        assert(message);
        assert(w >= 0);
        assert(w <= _INHIBIT_WHAT_MAX);

        r = sd_bus_message_read(message, "b", &interactive);
        if (r < 0)
                return r;

        log_debug_elogind("%s called with action '%s', sleep '%s' (%sinteractive)",
                          __FUNCTION__, action, sleep_verb,
                          interactive ? "" : "NOT ");

        /* Don't allow multiple jobs being executed at the same time */
        if (m->action_what)
                return sd_bus_error_setf(error, BUS_ERROR_OPERATION_IN_PROGRESS, "There's already a shutdown or sleep operation in progress");

        if (sleep_verb) {
                r = can_sleep(m, sleep_verb);
                if (r < 0)
                        return r;

                if (r == 0)
                        return sd_bus_error_setf(error, BUS_ERROR_SLEEP_VERB_NOT_SUPPORTED, "Sleep verb not supported");
        }

        if (IN_SET(sleep_action, HANDLE_HALT, HANDLE_POWEROFF, HANDLE_REBOOT)) {
                r = verify_shutdown_creds(m, message, w, interactive, action, action_multiple_sessions,
                                          action_ignore_inhibit, error);
                log_debug_elogind("verify_shutdown_creds() returned %d", r);
                if (r != 0)
                        return r;
        }

        r = bus_manager_shutdown_or_sleep_now_or_later(m, sleep_action, w, error);
        if (r < 0)
                return r;

        return sd_bus_reply_method_return(message, NULL);
}

int method_poweroff(sd_bus_message *message, void *userdata, sd_bus_error *error) {
        Manager *m = userdata;

        log_debug_elogind("%s called", __FUNCTION__);

        return method_do_shutdown_or_sleep(
                        m, message,
                        HANDLE_POWEROFF,
                        INHIBIT_SHUTDOWN,
                        "org.freedesktop.login1.power-off",
                        "org.freedesktop.login1.power-off-multiple-sessions",
                        "org.freedesktop.login1.power-off-ignore-inhibit",
                        NULL,
                        error);
}

int method_reboot(sd_bus_message *message, void *userdata, sd_bus_error *error) {
        Manager *m = userdata;

        log_debug_elogind("%s called", __FUNCTION__);

        return method_do_shutdown_or_sleep(
                        m, message,
                        HANDLE_REBOOT,
                        INHIBIT_SHUTDOWN,
                        "org.freedesktop.login1.reboot",
                        "org.freedesktop.login1.reboot-multiple-sessions",
                        "org.freedesktop.login1.reboot-ignore-inhibit",
                        NULL,
                        error);
}

int method_halt(sd_bus_message *message, void *userdata, sd_bus_error *error) {
        Manager *m = userdata;

        log_debug_elogind("%s called", __FUNCTION__);

        return method_do_shutdown_or_sleep(
                        m, message,
                        HANDLE_HALT,
                        INHIBIT_SHUTDOWN,
                        "org.freedesktop.login1.halt",
                        "org.freedesktop.login1.halt-multiple-sessions",
                        "org.freedesktop.login1.halt-ignore-inhibit",
                        NULL,
                        error);
}

int method_suspend(sd_bus_message *message, void *userdata, sd_bus_error *error) {
        Manager *m = userdata;

        log_debug_elogind("%s called", __FUNCTION__);

        return method_do_shutdown_or_sleep(
                        m, message,
                        HANDLE_SUSPEND,
                        INHIBIT_SLEEP,
                        "org.freedesktop.login1.suspend",
                        "org.freedesktop.login1.suspend-multiple-sessions",
                        "org.freedesktop.login1.suspend-ignore-inhibit",
                        "suspend",
                        error);
}

int manager_scheduled_shutdown_handler(
                        sd_event_source *s,
                        uint64_t usec,
                        void *userdata) {

        _cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
        Manager *m = userdata;
        HandleAction action;
        int r;

        assert(m);

        if (isempty(m->scheduled_shutdown_type))
                return 0;

        if (streq(m->scheduled_shutdown_type, "halt"))
                action = HANDLE_HALT;
        else if (streq(m->scheduled_shutdown_type, "poweroff"))
                action = HANDLE_POWEROFF;
        else
                action = HANDLE_REBOOT;

        /* Don't allow multiple jobs being executed at the same time */
        if (m->action_what) {
                log_error("Scheduled shutdown to %s failed: shutdown or sleep operation already in progress",
                          m->scheduled_shutdown_type);
                return -EALREADY;
        }

        if (m->shutdown_dry_run) {
                /* We do not process delay inhibitors here.  Otherwise, we
                 * would have to be considered "in progress" (like the check
                 * above) for some seconds after our admin has seen the final
                 * wall message. */

                bus_manager_log_shutdown(m, target);
                log_info("Running in dry run, suppressing action.");
                reset_scheduled_shutdown(m);

                return 0;
        }

        r = execute_shutdown_or_sleep(m, 0, action, &error);
        if (r < 0)
                return log_error_errno(r, "Scheduled shutdown to %s failed: %m",
                                       m->scheduled_shutdown_type);

        return 0;
}

int method_hibernate(sd_bus_message *message, void *userdata, sd_bus_error *error) {
        Manager *m = userdata;

        log_debug_elogind("%s called", __FUNCTION__);

        return method_do_shutdown_or_sleep(
                        m, message,
                        HANDLE_HIBERNATE,
                        INHIBIT_SLEEP,
                        "org.freedesktop.login1.hibernate",
                        "org.freedesktop.login1.hibernate-multiple-sessions",
                        "org.freedesktop.login1.hibernate-ignore-inhibit",
                        "hibernate",
                        error);
}

int method_hybrid_sleep(sd_bus_message *message, void *userdata, sd_bus_error *error) {
        Manager *m = userdata;

        log_debug_elogind("%s called", __FUNCTION__);

        return method_do_shutdown_or_sleep(
                        m, message,
                        HANDLE_HYBRID_SLEEP,
                        INHIBIT_SLEEP,
                        "org.freedesktop.login1.hibernate",
                        "org.freedesktop.login1.hibernate-multiple-sessions",
                        "org.freedesktop.login1.hibernate-ignore-inhibit",
                        "hybrid-sleep",
                        error);
}
