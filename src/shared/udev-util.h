/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

/***
  Copyright © 2013 Zbigniew Jędrzejewski-Szmek
***/

#if 0 /// elogind needs the systems udev header
#include "udev.h"
#else
#include <libudev.h>
#endif // 0
#include "util.h"

DEFINE_TRIVIAL_CLEANUP_FUNC(struct udev*, udev_unref);
DEFINE_TRIVIAL_CLEANUP_FUNC(struct udev_device*, udev_device_unref);
DEFINE_TRIVIAL_CLEANUP_FUNC(struct udev_enumerate*, udev_enumerate_unref);
#if 0 /// UNNEEDED by elogind
DEFINE_TRIVIAL_CLEANUP_FUNC(struct udev_event*, udev_event_unref);
DEFINE_TRIVIAL_CLEANUP_FUNC(struct udev_rules*, udev_rules_unref);
DEFINE_TRIVIAL_CLEANUP_FUNC(struct udev_ctrl*, udev_ctrl_unref);
DEFINE_TRIVIAL_CLEANUP_FUNC(struct udev_ctrl_connection*, udev_ctrl_connection_unref);
DEFINE_TRIVIAL_CLEANUP_FUNC(struct udev_ctrl_msg*, udev_ctrl_msg_unref);
#endif // 0
DEFINE_TRIVIAL_CLEANUP_FUNC(struct udev_monitor*, udev_monitor_unref);

#if 0 /// UNNEEDED by elogind
#endif // 0
int udev_parse_config(void);

int udev_device_new_from_stat_rdev(struct udev *udev, const struct stat *st, struct udev_device **ret);
