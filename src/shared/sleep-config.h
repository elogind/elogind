/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

#include <linux/fiemap.h>
//#include "time-util.h"

int read_fiemap(int fd, struct fiemap **ret);
#if 0 /// UNNEEDED by elogind
int parse_sleep_config(const char *verb, char ***modes, char ***states, usec_t *delay);
#endif // 0
int find_hibernate_location(char **device, char **type, size_t *size, size_t *used);

#if 0 /// UNNEEDED by elogind
int can_sleep(const char *verb);
int can_sleep_disk(char **types);
int can_sleep_state(char **types);
#else
#include <logind.h>
int can_sleep(Manager* m, const char *verb);
#endif // 0
