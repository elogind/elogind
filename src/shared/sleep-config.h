/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

#include <linux/fiemap.h>
#include "time-util.h"

typedef struct SleepConfig {
        bool allow_suspend;         /* AllowSuspend */
        bool allow_hibernate;       /* AllowHibernation */
        bool allow_s2h;             /* AllowSuspendThenHibernate */
        bool allow_hybrid_sleep;    /* AllowHybridSleep */

        char **suspend_modes;       /* SuspendMode */
        char **suspend_states;      /* SuspendState */
        char **hibernate_modes;     /* HibernateMode */
        char **hibernate_states;    /* HibernateState */
        char **hybrid_modes;        /* HybridSleepMode */
        char **hybrid_states;       /* HybridSleepState */

        usec_t hibernate_delay_sec; /* HibernateDelaySec */
} SleepConfig;

#if 0 /// UNNEEDED by elogind
void free_sleep_config(SleepConfig *sc);
DEFINE_TRIVIAL_CLEANUP_FUNC(SleepConfig*, free_sleep_config);

int sleep_settings(const char *verb, const SleepConfig *sleep_config, bool *ret_allow, char ***ret_modes, char ***ret_states);
#endif // 0

int read_fiemap(int fd, struct fiemap **ret);
#if 0 /// UNNEEDED by elogind
int parse_sleep_config(SleepConfig **sleep_config);
#endif // 0
int find_hibernate_location(char **device, char **type, uint64_t *size, uint64_t *used);

#if 0 /// elogind has to transport its manager instance
int can_sleep(const char *verb);
int can_sleep_disk(char **types);
int can_sleep_state(char **types);
#else // 0
#include <logind.h>
int can_sleep(Manager *m, const char *verb);
#endif // 0
