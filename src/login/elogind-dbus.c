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


#include "elogind-dbus.h"
#include "process-util.h"
#include "sd-messages.h"
#include "sleep.h"
#include "string-util.h"
#include "strv.h"
#include "update-utmp.h"


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
        int   r   = 0;

        r = safe_fork_full(helper, NULL, 0, FORK_RESET_SIGNALS|FORK_REOPEN_LOG, NULL);

        if (r < 0)
                return log_error_errno(errno, "Failed to fork run %s: %m", helper);

        if (0 == r) {
                /* Child */
                execlp(helper, helper, NULL);
                log_error_errno(errno, "Failed to execute %s: %m", helper);
                _exit(EXIT_FAILURE);
        }

        return 0;
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
                return do_sleep("suspend", m->suspend_mode, m->suspend_state, 0);
        case HANDLE_HIBERNATE:
                return do_sleep("hibernate", m->hibernate_mode, m->hibernate_state, 0);
        case HANDLE_HYBRID_SLEEP:
                return do_sleep("hybrid-sleep", m->hybrid_sleep_mode, m->hybrid_sleep_state, 0);
        default:
                return -EINVAL;
        }
}

int execute_shutdown_or_sleep(
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
                update_utmp(2, argv_utmp);
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
                r = -EALREADY;
                log_error("Scheduled shutdown to %s failed: shutdown or sleep operation already in progress",
                          m->scheduled_shutdown_type);
                goto error;
        }

        if (m->shutdown_dry_run) {
                /* We do not process delay inhibitors here.  Otherwise, we
                 * would have to be considered "in progress" (like the check
                 * above) for some seconds after our admin has seen the final
                 * wall message. */

                bus_manager_log_shutdown(m, INHIBIT_SHUTDOWN, action);
                log_info("Running in dry run, suppressing action.");
                reset_scheduled_shutdown(m);

                return 0;
        }

        r = execute_shutdown_or_sleep(m, 0, action, &error);
        if (r < 0) {
                log_error_errno(r, "Scheduled shutdown to %s failed: %m",
                                       m->scheduled_shutdown_type);
                goto error;
        }

        return 0;

error:
        reset_scheduled_shutdown(m);
        return r;
}
