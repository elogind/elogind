/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <signal.h>

#include "macro.h"

int reset_all_signal_handlers(void);
int reset_signal_mask(void);

int sigaction_many_internal(const struct sigaction *sa, ...);

#define ignore_signals(...)                                             \
        sigaction_many_internal(                                        \
                        &(const struct sigaction) {                     \
                                .sa_handler = SIG_IGN,                  \
                                .sa_flags = SA_RESTART                  \
                        },                                              \
                        __VA_ARGS__,                                    \
                        -1)

#define default_signals(...)                                            \
        sigaction_many_internal(                                        \
                        &(const struct sigaction) {                     \
                                .sa_handler = SIG_DFL,                  \
                                .sa_flags = SA_RESTART                  \
                        },                                              \
                        __VA_ARGS__,                                    \
                        -1)

#if 0 /// UNNEEDED by elogind
#define sigaction_many(sa, ...)                                         \
        sigaction_many_internal(sa, __VA_ARGS__, -1)

#endif // 0
int sigset_add_many_internal(sigset_t *ss, ...);
#define sigset_add_many(...) sigset_add_many_internal(__VA_ARGS__, -1)

int sigprocmask_many_internal(int how, sigset_t *old, ...);
#define sigprocmask_many(...) sigprocmask_many_internal(__VA_ARGS__, -1)

const char *signal_to_string(int i) _const_;
int signal_from_string(const char *s) _pure_;

#if 0 /// UNNEEDED by elogind
void nop_signal_handler(int sig);
#endif // 0

static inline void block_signals_reset(sigset_t *ss) {
        assert_se(sigprocmask(SIG_SETMASK, ss, NULL) >= 0);
}

#define BLOCK_SIGNALS(...)                                                         \
        _cleanup_(block_signals_reset) _unused_ sigset_t _saved_sigset = ({        \
                sigset_t _t;                                                       \
                assert_se(sigprocmask_many(SIG_BLOCK, &_t, __VA_ARGS__) >= 0);     \
                _t;                                                                \
        })

static inline bool SIGNAL_VALID(int signo) {
        return signo > 0 && signo < _NSIG;
}

#if 0 /// UNNEEDED by elogind
static inline const char* signal_to_string_with_check(int n) {
        if (!SIGNAL_VALID(n))
                return NULL;

        return signal_to_string(n);
}
#endif // 0

int signal_is_blocked(int sig);

int pop_pending_signal_internal(int sig, ...);
#define pop_pending_signal(...) pop_pending_signal_internal(__VA_ARGS__, -1)

void propagate_signal(int sig, siginfo_t *siginfo);
