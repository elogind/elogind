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


#include "elogind-action.h"
#include "fd-util.h"
#include "process-util.h"
#include "sleep.h"
#include "sleep-config.h"


static int run_helper(const char *helper) {
        int pid = fork();
        if (pid < 0) {
                return log_error_errno(errno, "Failed to fork: %m");
        }

        if (pid == 0) {
                /* Child */

                close_all_fds(NULL, 0);

                execlp(helper, helper, NULL);
                log_error_errno(errno, "Failed to execute %s: %m", helper);
                _exit(EXIT_FAILURE);
        }

        return wait_for_terminate_and_warn(helper, pid, true);
}

int shutdown_or_sleep(Manager *m, HandleAction action) {

        assert(m);

        switch (action) {
        case HANDLE_POWEROFF:
                return run_helper(HALT);
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
