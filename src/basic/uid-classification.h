/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <stdbool.h>
#include <sys/types.h>

/* The container base should have the last 16 bit set to zero */
assert_cc((CONTAINER_UID_BASE_MIN & 0xFFFFU) == 0);
assert_cc((CONTAINER_UID_BASE_MAX & 0xFFFFU) == 0);

/* Given we assign 64K UIDs to containers, the last container UID is 0xFFFF larger than the base */
#define CONTAINER_UID_MIN (CONTAINER_UID_BASE_MIN)
#define CONTAINER_UID_MAX (CONTAINER_UID_BASE_MAX + 0xFFFFU)

bool uid_is_system(uid_t uid);
bool gid_is_system(gid_t gid);

#if 0 /// UNNEEDED by elogind
static inline bool uid_is_dynamic(uid_t uid) {
        return DYNAMIC_UID_MIN <= uid && uid <= DYNAMIC_UID_MAX;
}

static inline bool gid_is_dynamic(gid_t gid) {
        return uid_is_dynamic((uid_t) gid);
}

static inline bool uid_is_container(uid_t uid) {
        return CONTAINER_UID_MIN <= uid && uid <= CONTAINER_UID_MAX;
}

static inline bool gid_is_container(gid_t gid) {
        return uid_is_container((uid_t) gid);
}
#endif // 0

typedef struct UGIDAllocationRange {
        uid_t system_alloc_uid_min;
        uid_t system_uid_max;
        gid_t system_alloc_gid_min;
        gid_t system_gid_max;
} UGIDAllocationRange;

int read_login_defs(UGIDAllocationRange *ret_defs, const char *path, const char *root);
const UGIDAllocationRange *acquire_ugid_allocation_range(void);

#if 0 /// UNNEEDED by elogind
bool uid_for_system_journal(uid_t uid);
#endif // 0
