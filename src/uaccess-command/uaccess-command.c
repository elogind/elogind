/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * manage device node user ACL
 *
 * Copyright 2010-2012 Kay Sievers <kay@vrfy.org>
 * Copyright 2010 Lennart Poettering
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "sd-login.h"

//#include "device-util.h"
#include "devnode-acl.h"
#include "login-util.h"
#include "log.h"
//#include "udev-builtin.h"
/// Additional includes needed by elogind
#include "musl_missing.h"
//#include "sd-device.h"
#include <string.h>
//#include <sys/stat.h>
//#include <sys/types.h>

/*
 * Copy of builtin_uaccess() from
 * systemd/src/udev/udev-builtin-uaccess.c
 */
#if 0 /// Within elogind a different set of parameters are used
static int builtin_uaccess(sd_device *dev, int argc, char *argv[], bool test) {
        const char *path = NULL, *seat;
#else // 0
static int dev_uaccess(const char *path, const char *seat) {
#endif // 0
        bool changed_acl = false;
        uid_t uid;
        int r;

        umask(0022);

#if 0 /// If elogind is not running, yet, dbus will start it when it is needed.
        /* don't muck around with ACLs when the system is not running systemd-logind */
        if (!logind_running())
                return 0;
#endif // 0

#if 0 /// With elogind both path and seat are delivered from main()
        r = sd_device_get_devname(dev, &path);
        if (r < 0) {
                log_device_error_errno(dev, r, "Failed to get device name: %m");
                goto finish;
        }

        if (sd_device_get_property_value(dev, "ID_SEAT", &seat) < 0)
                seat = "seat0";
#else // 0
        if (!seat || !strlen(seat))
                seat = "seat0";
#endif // 0

        r = sd_seat_get_active(seat, NULL, &uid);
        if (r < 0) {
                if (IN_SET(r, -ENXIO, -ENODATA))
                        /* No active session on this seat */
                        r = 0;
                else
#if 0 /// No sd-device available in this special instance for elogind, use a different log function
                        log_device_error_errno(dev, r, "Failed to determine active user on seat %s: %m", seat);
#else // 0
                        log_error_errno(r, "Failed to determine active user on seat %s: %m", seat);
#endif // 0

                goto finish;
        }

        r = devnode_acl(path, true, false, 0, true, uid);
        if (r < 0) {
#if 0 /// No sd-device available in this special instance for elogind, use a different log function
                log_device_full_errno(dev, r == -ENOENT ? LOG_DEBUG : LOG_ERR, r, "Failed to apply ACL: %m");
#else // 0
                log_full_errno(r == -ENOENT ? LOG_DEBUG : LOG_ERR, r, "Failed to apply ACL: %m");
#endif // 0
                goto finish;
        }

        changed_acl = true;
        r = 0;

        finish:
        if (path && !changed_acl) {
                int k;

                /* Better be safe than sorry and reset ACL */
                k = devnode_acl(path, true, false, 0, false, 0);
                if (k < 0) {
#if 0 /// No sd-device available in this special instance for elogind, use a different log function
                        log_device_full_errno(dev, k == -ENOENT ? LOG_DEBUG : LOG_ERR, k, "Failed to apply ACL: %m");
#else // 0
                        log_full_errno(k == -ENOENT ? LOG_DEBUG : LOG_ERR, k, "Failed to apply ACL: %m");
#endif // 0
                        if (r >= 0)
                                r = k;
                }
        }

        return r < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {

        if (argc < 2) {
                printf("Usage: %s DEVPATH [SEAT]\n", argv[0]);
                return 0;
        }

        elogind_set_program_name(argv[0]);
        log_set_target(LOG_TARGET_AUTO);
        log_parse_environment();
        log_open();

        return dev_uaccess(argv[1], argc > 2 ? argv[2] : NULL);
}
