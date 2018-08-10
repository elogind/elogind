/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

/***
  This file is part of systemd.

  Copyright 2016 Lennart Poettering
***/

#include <sched.h>

#include "missing.h"

/* The combination of all namespace flags defined by the kernel. The right type for this isn't clear. setns() and
 * unshare() expect these flags to be passed as (signed) "int", while clone() wants them as "unsigned long". The latter
 * is definitely more appropriate for a flags parameter, and also the larger type of the two, hence let's stick to that
 * here. */
#define NAMESPACE_FLAGS_ALL                                             \
        ((unsigned long) (CLONE_NEWCGROUP|                              \
                          CLONE_NEWIPC|                                 \
                          CLONE_NEWNET|                                 \
                          CLONE_NEWNS|                                  \
                          CLONE_NEWPID|                                 \
                          CLONE_NEWUSER|                                \
                          CLONE_NEWUTS))

#if 0 /// UNNEEDED by elogind
#endif // 0
#define NAMESPACE_FLAGS_INITIAL  ((unsigned long) -1)

int namespace_flags_from_string(const char *name, unsigned long *ret);
int namespace_flags_to_string(unsigned long flags, char **ret);

struct namespace_flag_map {
        unsigned long flag;
        const char *name;
};

extern const struct namespace_flag_map namespace_flag_map[];
