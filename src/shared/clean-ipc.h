/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

/***
  Copyright 2014 Lennart Poettering
***/

#include <sys/types.h>

#include "user-util.h"

int clean_ipc_internal(uid_t uid, gid_t gid, bool rm);

/* Remove all IPC objects owned by the specified UID or GID */
int clean_ipc_by_uid(uid_t uid);
#if 0 /// UNNEEDED by elogind
int clean_ipc_by_gid(gid_t gid);
#endif // 0

/* Check if any IPC object owned by the specified UID or GID exists, returns > 0 if so, == 0 if not */
static inline int search_ipc(uid_t uid, gid_t gid) {
        return clean_ipc_internal(uid, gid, false);
}
