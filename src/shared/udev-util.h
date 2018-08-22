/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once


#if 0 /// elogind needs the systems udev header
#include "udev.h"
#else
#include <libudev.h>
#endif // 0
#include "util.h"

#if 0 /// UNNEEDED by elogind
DEFINE_TRIVIAL_CLEANUP_FUNC(struct udev_event*, udev_event_unref);
DEFINE_TRIVIAL_CLEANUP_FUNC(struct udev_rules*, udev_rules_unref);
DEFINE_TRIVIAL_CLEANUP_FUNC(struct udev_ctrl*, udev_ctrl_unref);
DEFINE_TRIVIAL_CLEANUP_FUNC(struct udev_ctrl_connection*, udev_ctrl_connection_unref);
DEFINE_TRIVIAL_CLEANUP_FUNC(struct udev_ctrl_msg*, udev_ctrl_msg_unref);
#endif // 0

#if 0 /// UNNEEDED by elogind
int udev_parse_config(void);
#endif // 0
