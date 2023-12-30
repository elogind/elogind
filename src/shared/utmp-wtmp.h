/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <stdbool.h>
#include <sys/types.h>

#include "time-util.h"

#if ENABLE_UTMP
#include <utmpx.h>

#if 0 /// UNNEEDED by elogind
int utmp_get_runlevel(int *runlevel, int *previous);
#endif // 0

int utmp_put_shutdown(void);
int utmp_put_reboot(usec_t timestamp);
#if 0 /// UNNEEDED by elogind
int utmp_put_runlevel(int runlevel, int previous);

int utmp_put_dead_process(const char *id, pid_t pid, int code, int status);
int utmp_put_init_process(const char *id, pid_t pid, pid_t sid, const char *line, int ut_type, const char *user);
#endif // 0

static inline bool utxent_start(void) {
        setutxent();
        return true;
}
static inline void utxent_cleanup(bool *initialized) {
        if (initialized)
                endutxent();
}

#else /* ENABLE_UTMP */

#if 0 /// UNNEEDED by elogind
static inline int utmp_get_runlevel(int *runlevel, int *previous) {
        return -ESRCH;
}
#endif // 0
static inline int utmp_put_shutdown(void) {
        return 0;
}
static inline int utmp_put_reboot(usec_t timestamp) {
        return 0;
}
#if 0 /// UNNEEDED by elogind
static inline int utmp_put_runlevel(int runlevel, int previous) {
        return 0;
}
static inline int utmp_put_dead_process(const char *id, pid_t pid, int code, int status) {
        return 0;
}
static inline int utmp_put_init_process(const char *id, pid_t pid, pid_t sid, const char *line, int ut_type, const char *user) {
        return 0;
}
#endif // 0

#endif /* ENABLE_UTMP */
