/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

#include "sd-device.h"

int device_enumerator_scan_devices(sd_device_enumerator *enumeartor);
int device_enumerator_scan_subsystems(sd_device_enumerator *enumeartor);
int device_enumerator_add_device(sd_device_enumerator *enumerator, sd_device *device);
#if 0 /// UNNEEDED by elogind
int device_enumerator_add_match_is_initialized(sd_device_enumerator *enumerator);
#endif // 0
int device_enumerator_add_match_parent_incremental(sd_device_enumerator *enumerator, sd_device *parent);
#if 0 /// UNNEEDED by elogind
sd_device *device_enumerator_get_first(sd_device_enumerator *enumerator);
sd_device *device_enumerator_get_next(sd_device_enumerator *enumerator);
#endif // 0
sd_device **device_enumerator_get_devices(sd_device_enumerator *enumerator, size_t *ret_n_devices);

#if 0 /// UNNEEDED by elogind
#define FOREACH_DEVICE_AND_SUBSYSTEM(enumerator, device)       \
        for (device = device_enumerator_get_first(enumerator); \
             device;                                           \
             device = device_enumerator_get_next(enumerator))
#endif // 0
