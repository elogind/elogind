/* SPDX-License-Identifier: LGPL-2.1+ */
/***
  This file is part of systemd.

  Copyright 2010 Lennart Poettering
***/

#include <errno.h>
#include <string.h>

#if 0 /// elogind needs the systems udev header
#include "libudev.h"
#else
#include <libudev.h>
#endif // 0

#include "alloc-util.h"
#include "locale-util.h"
#include "path-util.h"
#include "string-util.h"
#include "sysfs-show.h"
#include "terminal-util.h"
#include "udev-util.h"
#include "util.h"

static int show_sysfs_one(
                struct udev *udev,
                const char *seat,
                struct udev_list_entry **item,
                const char *sub,
                const char *prefix,
                unsigned n_columns,
                OutputFlags flags) {

        size_t max_width;

        assert(udev);
        assert(seat);
        assert(item);
        assert(prefix);

        if (flags & OUTPUT_FULL_WIDTH)
                max_width = (size_t) -1;
        else if (n_columns < 10)
                max_width = 10;
        else
                max_width = n_columns;

        while (*item) {
                _cleanup_(udev_device_unrefp) struct udev_device *d = NULL;
                struct udev_list_entry *next, *lookahead;
                const char *sn, *name, *sysfs, *subsystem, *sysname;
                _cleanup_free_ char *k = NULL, *l = NULL;
                bool is_master;

                sysfs = udev_list_entry_get_name(*item);
                if (!path_startswith(sysfs, sub))
                        return 0;

                d = udev_device_new_from_syspath(udev, sysfs);
                if (!d) {
                        *item = udev_list_entry_get_next(*item);
                        continue;
                }

                sn = udev_device_get_property_value(d, "ID_SEAT");
                if (isempty(sn))
                        sn = "seat0";

                /* Explicitly also check for tag 'seat' here */
                if (!streq(seat, sn) || !udev_device_has_tag(d, "seat")) {
                        *item = udev_list_entry_get_next(*item);
                        continue;
                }

                is_master = udev_device_has_tag(d, "master-of-seat");

                name = udev_device_get_sysattr_value(d, "name");
                if (!name)
                        name = udev_device_get_sysattr_value(d, "id");
                subsystem = udev_device_get_subsystem(d);
                sysname = udev_device_get_sysname(d);

                /* Look if there's more coming after this */
                lookahead = next = udev_list_entry_get_next(*item);
                while (lookahead) {
                        const char *lookahead_sysfs;

                        lookahead_sysfs = udev_list_entry_get_name(lookahead);

                        if (path_startswith(lookahead_sysfs, sub) &&
                            !path_startswith(lookahead_sysfs, sysfs)) {
                                _cleanup_(udev_device_unrefp) struct udev_device *lookahead_d = NULL;

                                lookahead_d = udev_device_new_from_syspath(udev, lookahead_sysfs);
                                if (lookahead_d) {
                                        const char *lookahead_sn;

                                        lookahead_sn = udev_device_get_property_value(d, "ID_SEAT");
                                        if (isempty(lookahead_sn))
                                                lookahead_sn = "seat0";

                                        if (streq(seat, lookahead_sn) && udev_device_has_tag(lookahead_d, "seat"))
                                                break;
                                }
                        }

                        lookahead = udev_list_entry_get_next(lookahead);
                }

                k = ellipsize(sysfs, max_width, 20);
                if (!k)
                        return -ENOMEM;

                printf("%s%s%s\n", prefix, special_glyph(lookahead ? TREE_BRANCH : TREE_RIGHT), k);

                if (asprintf(&l,
                             "%s%s:%s%s%s%s",
                             is_master ? "[MASTER] " : "",
                             subsystem, sysname,
                             name ? " \"" : "", strempty(name), name ? "\"" : "") < 0)
                        return -ENOMEM;

                free(k);
                k = ellipsize(l, max_width, 70);
                if (!k)
                        return -ENOMEM;

                printf("%s%s%s\n", prefix, lookahead ? special_glyph(TREE_VERTICAL) : "  ", k);

                *item = next;
                if (*item) {
                        _cleanup_free_ char *p = NULL;

                        p = strappend(prefix, lookahead ? special_glyph(TREE_VERTICAL) : "  ");
                        if (!p)
                                return -ENOMEM;

                        show_sysfs_one(udev, seat, item, sysfs, p,
                                       n_columns == (unsigned) -1 || n_columns < 2 ? n_columns : n_columns - 2,
                                       flags);
                }
        }

        return 0;
}

int show_sysfs(const char *seat, const char *prefix, unsigned n_columns, OutputFlags flags) {
        _cleanup_(udev_enumerate_unrefp) struct udev_enumerate *e = NULL;
        _cleanup_(udev_unrefp) struct udev *udev = NULL;
        struct udev_list_entry *first = NULL;
        int r;

        if (n_columns <= 0)
                n_columns = columns();

        prefix = strempty(prefix);

        if (isempty(seat))
                seat = "seat0";

        udev = udev_new();
        if (!udev)
                return -ENOMEM;

        e = udev_enumerate_new(udev);
        if (!e)
                return -ENOMEM;

        if (!streq(seat, "seat0"))
                r = udev_enumerate_add_match_tag(e, seat);
        else
                r = udev_enumerate_add_match_tag(e, "seat");
        if (r < 0)
                return r;

        r = udev_enumerate_add_match_is_initialized(e);
        if (r < 0)
                return r;

        r = udev_enumerate_scan_devices(e);
        if (r < 0)
                return r;

        first = udev_enumerate_get_list_entry(e);
        if (first)
                show_sysfs_one(udev, seat, &first, "/", prefix, n_columns, flags);
        else
                printf("%s%s%s\n", prefix, special_glyph(TREE_RIGHT), "(none)");

        return r;
}
