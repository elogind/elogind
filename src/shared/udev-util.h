/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include "sd-device.h"

#include "conf-parser.h"
#include "hashmap.h"
#include "time-util.h"

#if 0 /// UNNEEDED by elogind
int udev_parse_config_full(const ConfigTableItem config_table[]);
int udev_parse_config(void);

int device_wait_for_initialization(sd_device *device, const char *subsystem, usec_t timeout_usec, sd_device **ret);
int device_wait_for_devlink(const char *path, const char *subsystem, usec_t timeout_usec, sd_device **ret);
int device_is_renaming(sd_device *dev);
#endif // 0
int device_is_processed(sd_device *dev);

bool device_for_action(sd_device *dev, sd_device_action_t action);

#if 0 /// UNNEEDED by elogind
void log_device_uevent(sd_device *device, const char *str);

size_t udev_replace_whitespace(const char *str, char *to, size_t len);
size_t udev_replace_chars(char *str, const char *allow);
#endif // 0

int udev_queue_is_empty(void);

void reset_cached_udev_availability(void);
bool udev_available(void);

#if 0 /// UNNEEDED by elogind
int device_get_vendor_string(sd_device *device, const char **ret);
int device_get_model_string(sd_device *device, const char **ret);

int device_get_property_value_with_fallback(
                sd_device *device,
                const char *prop,
                Hashmap *extra_props,
                const char **ret);
#endif // 0
