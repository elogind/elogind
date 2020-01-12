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

#include "sd-login.h"

#include "login-util.h"
#include "logind-acl.h"
#include "util.h"
/// Additional includes needed by elogind
#include "musl_missing.h"
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

/*
 * Copy of builtin_uaccess() from
 * systemd/src/udev/udev-builtin-uaccess.c
 */
static int dev_uaccess(const char *path, const char *seat) {
        int r;
        bool changed_acl = false;
        uid_t uid;

        umask(0022);

        /* don't muck around with ACLs when the system is not running logind */
        if (!logind_running())
                return 0;

        if (!seat || !strlen(seat))
                seat = "seat0";

        r = sd_seat_get_active(seat, NULL, &uid);
        if (IN_SET(r, -ENXIO, -ENODATA)) {
                /* No active session on this seat */
                r = 0;
                goto finish;
        } else if (r < 0) {
                log_error("Failed to determine active user on seat %s.", seat);
                goto finish;
        }

        r = devnode_acl(path, true, false, 0, true, uid);
        if (r < 0) {
                log_full_errno(r == -ENOENT ? LOG_DEBUG : LOG_ERR, r, "Failed to apply ACL on %s: %m", path);
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
                        log_full_errno(errno == ENOENT ? LOG_DEBUG : LOG_ERR, k, "Failed to reset ACL on %s: %m", path);
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
