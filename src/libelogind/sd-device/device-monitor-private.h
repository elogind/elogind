/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <errno.h>

#include "sd-device.h"

#include "socket-util.h"

typedef enum MonitorNetlinkGroup {
        MONITOR_GROUP_NONE,
        MONITOR_GROUP_KERNEL,
        MONITOR_GROUP_UDEV,
        _MONITOR_NETLINK_GROUP_MAX,
        _MONITOR_NETLINK_GROUP_INVALID = -EINVAL,
} MonitorNetlinkGroup;

int device_monitor_new_full(sd_device_monitor **ret, MonitorNetlinkGroup group, int fd);
int device_monitor_disconnect(sd_device_monitor *m);
int device_monitor_get_address(sd_device_monitor *m, union sockaddr_union *ret);
int device_monitor_allow_unicast_sender(sd_device_monitor *m, sd_device_monitor *sender);
#if 0 /// UNNEEDED by elogind
int device_monitor_get_fd(sd_device_monitor *m);
#endif // 0
int device_monitor_send_device(sd_device_monitor *m, sd_device_monitor *destination, sd_device *device);
int device_monitor_receive_device(sd_device_monitor *m, sd_device **ret);
