/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

//#include <stdbool.h>

#include "sd-event.h"

#if 0 /// UNNEEDED by elogind
int event_reset_time(sd_event *e, sd_event_source **s,
                     clockid_t clock, uint64_t usec, uint64_t accuracy,
                     sd_event_time_handler_t callback, void *userdata,
                     int64_t priority, const char *description, bool force_reset);
#endif // 0
int event_source_disable(sd_event_source *s);
int event_source_is_enabled(sd_event_source *s);
