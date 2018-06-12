/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

/***
  Copyright 2010 Lennart Poettering
***/

#include <stdbool.h>

#if 0 /// UNNEEDED by elogind
int mount_setup_early(void);
#endif // 0
int mount_setup(bool loaded_policy);

#if 0 /// UNNEEDED by elogind
int mount_cgroup_controllers(char ***join_controllers);

bool mount_point_is_api(const char *path);
bool mount_point_ignore(const char *path);
#endif // 0
