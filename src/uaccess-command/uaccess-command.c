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
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "build.h"
#include "devnode-acl.h"
#include "login-util.h"
#include "log.h"
#include "macro.h"
#include "main-func.h"
#include "musl_missing.h"
#include "sd-login.h"
#include "verbs.h"

/*
 * Copy of builtin_uaccess() from
 * systemd/src/udev/udev-builtin-uaccess.c
 * adapted for elogind
 */
static int dev_uaccess(const char *path, const char *seat) {
        bool changed_acl = false;
        uid_t uid;
        int r;

        umask(0022);

        if (!seat || !strlen(seat))
                seat = "seat0";

        r = sd_seat_get_active(seat, NULL, &uid);
        if (r < 0) {
                if (IN_SET(r, -ENXIO, -ENODATA))
                        /* No active session on this seat */
                        r = 0;
                else
                        log_error_errno(r, "Failed to determine active user on seat %s: %m", seat);

                goto finish;
        }

        r = devnode_acl(path, true, false, 0, true, uid);
        if (r < 0) {
                log_full_errno(r == -ENOENT ? LOG_DEBUG : LOG_ERR, r, "Failed to apply ACL: %m");
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
                        log_full_errno(k == -ENOENT ? LOG_DEBUG : LOG_ERR, k, "Failed to apply ACL: %m");
                        if (r >= 0)
                                r = k;
                }
        }

        return r < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}

static int help(void) {
        printf("%s DEVPATH [SEAT]\n\n"
               "Handle uaccess for a device\n\n"
               "  -h --help              Show this help and exit\n"
               "  --version              Print version string and exit\n",
               program_invocation_short_name);

        return 0;
}

static int parse_argv(int argc, char *argv[]) {

        enum {
                ARG_VERSION = 0x100,
        };

        static const struct option options[] = {
                { "help",         no_argument,       NULL, 'h'           },
                { "version",      no_argument,       NULL, ARG_VERSION   },
                {}
        };

        int c;

        assert(argc >= 0);
        assert(argv);

        while ((c = getopt_long(argc, argv, "h", options, NULL)) >= 0)
                switch (c) {

                case 'h':
                        return help();

                case ARG_VERSION:
                        return version();

                case '?':
                        return -EINVAL;

                default:
                        assert_not_reached();

                }

        if (argc - optind != 1)
                return log_error_errno(SYNTHETIC_ERRNO(EINVAL),
                                       "Usage: %s DEVPATH [SEAT]",
                                       program_invocation_short_name);

        return 1 /* work to do */;
}

static int uaccess_command_main(int argc, char *argv[]) {

        if (argc < 2) {
                printf("Usage: %s DEVPATH [SEAT]\n", argv[0]);
                return 0;
        }

        return dev_uaccess(argv[1], argc > 2 ? argv[2] : NULL);
}

static int run(int argc, char *argv[]) {
        int r;

        elogind_set_program_name(argv[0]);
        log_setup();

        r = parse_argv(argc, argv);
        if (r <= 0)
                return r;

        return uaccess_command_main(argc, argv);
}

DEFINE_MAIN_FUNCTION(run);
