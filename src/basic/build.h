/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include "version.h"


/// elogind empty mask removed (UNSUPPORTED by elogind)

/// elogind empty mask removed (UNSUPPORTED by elogind)

/// elogind empty mask removed (UNSUPPORTED by elogind)

/// elogind empty mask removed (UNSUPPORTED by elogind)

#if 0 /// elogind has a much shorter list
extern const char* const systemd_features;

enum {
        BUILD_MODE_DEVELOPER,
        BUILD_MODE_RELEASE,
};
#else // 0
#define SYSTEMD_FEATURES                                                \
        _PAM_FEATURE_ " "                                               \
        _AUDIT_FEATURE_ " "                                             \
        _SELINUX_FEATURE_ " "                                           \
        _SMACK_FEATURE_ " "                                             \
        _UTMP_FEATURE_ " "                                              \
        _ACL_FEATURE_ " "                                               \
        _CGROUP_HIERARCHY_
#endif // 0
