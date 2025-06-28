#pragma once
#ifndef ELOGIND_SRC_LOGIN_ELOGINCTL_H_INCLUDED
#define ELOGIND_SRC_LOGIN_ELOGINCTL_H_INCLUDED

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

#include "systemctl.h"
#include "systemctl-daemon-reload.h"
#include "systemctl-logind.h"
#include "systemctl-start-special.h"
#include "systemctl-util.h"

#if 0 /// Replaced by systemctl.h (REMOVEME)
#include "sd-bus.h"

typedef enum elogind_action {
        _ACTION_INVALID,
        ACTION_HALT,
        ACTION_POWEROFF,
        ACTION_REBOOT,
        ACTION_KEXEC,
        ACTION_SUSPEND,
        ACTION_HIBERNATE,
        ACTION_HYBRID_SLEEP,
        ACTION_SUSPEND_THEN_HIBERNATE,
        ACTION_CANCEL_SHUTDOWN,
        _ACTION_MAX
} elogind_action;
#endif // 0

#if 0 /// Replaced by systemctl-logind.h (REMOVEME)
int  elogind_cancel_shutdown(sd_bus *bus);
#endif // 0
void elogind_cleanup(void);
#if 0 /// Replaced by systemctl-start-special.h (REMOVEME)
int  start_special(int argc, char *argv[], void *userdata);
#endif // 0

void elogind_log_special(enum action a);

#endif // ELOGIND_SRC_LOGIN_ELOGINCTL_H_INCLUDED
