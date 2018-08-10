/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

#include <stdbool.h>

int log_facility_unshifted_to_string_alloc(int i, char **s);
int log_facility_unshifted_from_string(const char *s);
#if 0 /// UNNEEDED by elogind
bool log_facility_unshifted_is_valid(int faciliy);
#endif // 0

int log_level_to_string_alloc(int i, char **s);
int log_level_from_string(const char *s);
#if 0 /// UNNEEDED by elogind
bool log_level_is_valid(int level);

int syslog_parse_priority(const char **p, int *priority, bool with_facility);
#endif // 0
