/*-*- Mode: C; c-basic-offset: 8; indent-tabs-mode: nil -*-*/

/***
  This file is part of systemd.

  Copyright 2010 Lennart Poettering

  systemd is free software; you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation; either version 2.1 of the License, or
  (at your option) any later version.

  systemd is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with systemd; If not, see <http://www.gnu.org/licenses/>.
***/

#include <stdlib.h>

#include "sd-bus.h"
#include "bus-util.h"
#include "musl_missing.h"
#include "log.h"

int main(int argc, char *argv[]) {
        _cleanup_(sd_bus_flush_close_unrefp) sd_bus *bus = NULL;
        int r;

        if (argc != 2) {
                log_error("Incorrect number of arguments.");
                return EXIT_FAILURE;
        }

        elogind_set_program_name(argv[0]);
        log_set_target(LOG_TARGET_AUTO);
        log_parse_environment();
        log_open();

#if 0
        /* We send this event to the private D-Bus socket and then the
         * system instance will forward this to the system bus. We do
         * this to avoid an activation loop when we start dbus when we
         * are called when the dbus service is shut down. */

        r = bus_connect_system_systemd(&bus);
#else
        /* Unlike in systemd where this has to use a private socket,
           since elogind doesn't associate control groups with services
           and doesn't manage the dbus service, we can just use the
           system bus.  */
        r = sd_bus_open_system(&bus);
#endif // 0

        if (r < 0) {
#if 0
                /* If we couldn't connect we assume this was triggered
                 * while systemd got restarted/transitioned from
                 * initrd to the system, so let's ignore this */
                log_debug_errno(r, "Failed to get D-Bus connection: %m");
#else
                /* If dbus isn't running or responding, there is nothing
                 * we can do about it. */
                log_debug_errno(r, "Failed to open system bus: %m");
#endif // 0
                return EXIT_FAILURE;
        }

        r = sd_bus_emit_signal(bus,
                               "/org/freedesktop/elogind/agent",
                               "org.freedesktop.elogind.Agent",
                               "Released",
                               "s", argv[1]);
        if (r < 0) {
#if 0
                log_debug_errno(r, "Failed to send signal message on private connection: %m");
#else
                log_debug_errno(r, "Failed to send signal message: %m");
#endif // 0
                return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
}
