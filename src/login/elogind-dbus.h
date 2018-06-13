#pragma once
#ifndef ELOGIND_SRC_LOGIN_ELOGIND_DBUS_H_INCLUDED
#define ELOGIND_SRC_LOGIN_ELOGIND_DBUS_H_INCLUDED

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

#include "logind.h"

int execute_shutdown_or_sleep(Manager *m, InhibitWhat w,
                              HandleAction action, sd_bus_error *error);
int manager_scheduled_shutdown_handler(sd_event_source *s, uint64_t usec,
                                       void *userdata);
int method_halt        (sd_bus_message *message, void *userdata, sd_bus_error *error);
int method_hibernate   (sd_bus_message *message, void *userdata, sd_bus_error *error);
int method_hybrid_sleep(sd_bus_message *message, void *userdata, sd_bus_error *error);
int method_poweroff    (sd_bus_message *message, void *userdata, sd_bus_error *error);
int method_reboot      (sd_bus_message *message, void *userdata, sd_bus_error *error);
int method_suspend     (sd_bus_message *message, void *userdata, sd_bus_error *error);


/* prototypes for former static functions in logind-dbus.c */
int  manager_inhibit_timeout_handler(sd_event_source *s, uint64_t usec, void *userdata);
void reset_scheduled_shutdown(Manager *m);
int  send_prepare_for(Manager *m, InhibitWhat w, bool _active);
int  verify_shutdown_creds(Manager *m, sd_bus_message *message, InhibitWhat w,
                           bool interactive, const char *action,
                           const char *action_multiple_sessions,
                           const char *action_ignore_inhibit,
                           sd_bus_error *error);


#endif // ELOGIND_SRC_LOGIN_ELOGIND_DBUS_H_INCLUDED
