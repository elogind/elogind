#pragma once
#ifndef ELOGIND_SRC_LOGIN_ELOGIN_H_INCLUDED
#define ELOGIND_SRC_LOGIN_ELOGIN_H_INCLUDED

/***
  This file is part of elogind.

  Copyright 2017-2018 Sven Eden

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
#include "logind.h"
#include "sd-bus.h"


/// Add-On for manager_connect_bus()
int elogind_setup_cgroups_agent(Manager *m);

/// elogind has some extra functionality at startup, as it is not hooked into systemd.
int elogind_startup(int argc, char *argv[]);

/// Add-On for manager_free()
void elogind_manager_free(Manager* m);

/// Add-On for manager_new()
int elogind_manager_new(Manager* m);

/** Add-On for manager_reset_config() */
void elogind_manager_reset_config(Manager* m);

/// Add-On for manager_startup()
int elogind_manager_startup(Manager* m);

#endif // ELOGIND_SRC_LOGIN_ELOGIN_H_INCLUDED
