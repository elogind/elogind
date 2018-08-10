/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

#include <stdbool.h>
#include <sys/types.h>

#include "time-util.h"
#include "util.h"

#if ENABLE_UTMP
int utmp_get_runlevel(int *runlevel, int *previous);

int utmp_put_shutdown(void);
int utmp_put_reboot(usec_t timestamp);
int utmp_put_runlevel(int runlevel, int previous);

int utmp_put_dead_process(const char *id, pid_t pid, int code, int status);
int utmp_put_init_process(const char *id, pid_t pid, pid_t sid, const char *line, int ut_type, const char *user);

int utmp_wall(
        const char *message,
        const char *username,
        const char *origin_tty,
        bool (*match_tty)(const char *tty, void *userdata),
        void *userdata);

#else /* ENABLE_UTMP */

static inline int utmp_get_runlevel(int *runlevel, int *previous) {
        return -ESRCH;
}
static inline int utmp_put_shutdown(void) {
        return 0;
}
static inline int utmp_put_reboot(usec_t timestamp) {
        return 0;
}
static inline int utmp_put_runlevel(int runlevel, int previous) {
        return 0;
}
static inline int utmp_put_dead_process(const char *id, pid_t pid, int code, int status) {
        return 0;
}
static inline int utmp_put_init_process(const char *id, pid_t pid, pid_t sid, const char *line, int ut_type, const char *user) {
        return 0;
}
static inline int utmp_wall(
                const char *message,
                const char *username,
                const char *origin_tty,
                bool (*match_tty)(const char *tty, void *userdata),
                void *userdata) {
        return 0;
}

#endif /* ENABLE_UTMP */
