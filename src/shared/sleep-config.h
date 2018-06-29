/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

/***
  This file is part of systemd.

  Copyright 2013 Zbigniew Jędrzejewski-Szmek

  systemd is free software; you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation; either version 2.1 of the License, or
  (at your option) any later version.

  systemd is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with systemd; If not, see <http://www.gnu.org/licenses/>.
***/

#if 0 /// UNNEEDED by elogind
//#include <linux/fiemap.h>
//#include "time-util.h"

int read_fiemap(int fd, struct fiemap **ret);
int parse_sleep_config(const char *verb, char ***modes, char ***states, usec_t *delay);
int find_hibernate_location(char **device, char **type, size_t *size, size_t *used);

int can_sleep(const char *verb);
int can_sleep_disk(char **types);
int can_sleep_state(char **types);
#else
#include <logind.h>
int can_sleep(Manager* m, const char *verb);
#endif // 0
