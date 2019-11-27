/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

#include <sys/types.h>

#if 0 /// UNNEEDED by elogind
int dev_setup(const char *prefix, uid_t uid, gid_t gid);
#endif // 0

int make_inaccessible_nodes(const char *root, uid_t uid, gid_t gid);
