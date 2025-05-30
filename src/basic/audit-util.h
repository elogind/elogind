/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

#include "pidref.h"

#define AUDIT_SESSION_INVALID UINT32_MAX

int audit_session_from_pid(const PidRef *pid, uint32_t *id);
int audit_loginuid_from_pid(const PidRef *pid, uid_t *uid);

#if 0 /// UNNEEDED by elogind
bool use_audit(void);
#endif // 0

static inline bool audit_session_is_valid(uint32_t id) {
        return id > 0 && id != AUDIT_SESSION_INVALID;
}
