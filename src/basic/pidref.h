/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include "macro.h"

/* An embeddable structure carrying a reference to a process. Supposed to be used when tracking processes continuously. */
typedef struct PidRef {
        pid_t pid; /* always valid */
        int fd;    /* only valid if pidfd are available in the kernel, and we manage to get an fd */
} PidRef;

#define PIDREF_NULL (const PidRef) { .fd = -EBADF }

/* Turns a pid_t into a PidRef structure on-the-fly *without* acquiring a pidfd for it. (As opposed to
 * pidref_set_pid() which does so *with* acquiring one, see below) */
#define PIDREF_MAKE_FROM_PID(x) (PidRef) { .pid = (x), .fd = -EBADF }

static inline bool pidref_is_set(const PidRef *pidref) {
        return pidref && pidref->pid > 0;
}

bool pidref_equal(const PidRef *a, const PidRef *b);

/* This turns a pid_t into a PidRef structure, and acquires a pidfd for it, if possible. (As opposed to
 * PIDREF_MAKE_FROM_PID() above, which does not acquire a pidfd.) */
int pidref_set_pid(PidRef *pidref, pid_t pid);
int pidref_set_pidstr(PidRef *pidref, const char *pid);
int pidref_set_pidfd(PidRef *pidref, int fd);
int pidref_set_pidfd_take(PidRef *pidref, int fd); /* takes ownership of the passed pidfd on success*/
int pidref_set_pidfd_consume(PidRef *pidref, int fd); /* takes ownership of the passed pidfd in both success and failure */

#if 0 /// UNNEEDED by elogind
int pidref_set_parent(PidRef *ret);
#endif // 0
static inline int pidref_set_self(PidRef *pidref) {
        return pidref_set_pid(pidref, 0);
}

bool pidref_is_self(const PidRef *pidref);

void pidref_done(PidRef *pidref);
PidRef *pidref_free(PidRef *pidref);
DEFINE_TRIVIAL_CLEANUP_FUNC(PidRef*, pidref_free);

#if 0 /// UNNEEDED by elogind
int pidref_copy(const PidRef *pidref, PidRef *dest);
int pidref_dup(const PidRef *pidref, PidRef **ret);

int pidref_new_from_pid(pid_t pid, PidRef **ret);
#endif // 0

int pidref_kill(const PidRef *pidref, int sig);
#if 0 /// UNNEEDED by elogind
int pidref_kill_and_sigcont(const PidRef *pidref, int sig);
#endif // 0
int pidref_sigqueue(const PidRef *pidref, int sig, int value);

int pidref_wait(const PidRef *pidref, siginfo_t *siginfo, int options);
int pidref_wait_for_terminate(const PidRef *pidref, siginfo_t *ret);

static inline void pidref_done_sigkill_wait(PidRef *pidref) {
        if (!pidref_is_set(pidref))
                return;

        (void) pidref_kill(pidref, SIGKILL);
        (void) pidref_wait_for_terminate(pidref, NULL);
        pidref_done(pidref);
}

int pidref_verify(const PidRef *pidref);

#define TAKE_PIDREF(p) TAKE_GENERIC((p), PidRef, PIDREF_NULL)

extern const struct hash_ops pidref_hash_ops;
extern const struct hash_ops pidref_hash_ops_free; /* Has destructor call for pidref_free(), i.e. expects heap allocated PidRef as keys */
