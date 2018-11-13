/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

//#include "time-util.h"

#if 0 /// elogind needs the systems udev header
int udev_parse_config_full(
                unsigned *ret_children_max,
                usec_t *ret_exec_delay_usec,
                usec_t *ret_event_timeout_usec);
#else
#include <libudev.h>
#endif // 0

#if 0 /// UNNEEDED by elogind
#endif // 0

#if 0 /// UNNEEDED by elogind
#endif // 0
static inline int udev_parse_config(void) {
        return udev_parse_config_full(NULL, NULL, NULL);
}
