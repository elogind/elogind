/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <unistd.h>

#include "alloc-util.h"
#include "conf-parser.h"
#include "constants.h"
#include "device-util.h"
#include "devnum-util.h"
#include "errno-util.h"
#include "fd-util.h"
#include "fileio.h"
#include "hibernate-util.h"
#include "log.h"
#include "macro.h"
#include "path-util.h"
#include "sleep-config.h"
#include "stat-util.h"
#include "stdio-util.h"
#include "string-table.h"
#include "string-util.h"
#include "strv.h"
#include "time-util.h"
// Additional includes needed by elogind
#include "logind.h"

#define DEFAULT_SUSPEND_ESTIMATION_USEC (1 * USEC_PER_HOUR)

static const char* const sleep_operation_table[_SLEEP_OPERATION_MAX] = {
        [SLEEP_SUSPEND]                = "suspend",
        [SLEEP_HIBERNATE]              = "hibernate",
        [SLEEP_HYBRID_SLEEP]           = "hybrid-sleep",
        [SLEEP_SUSPEND_THEN_HIBERNATE] = "suspend-then-hibernate",
};

DEFINE_STRING_TABLE_LOOKUP(sleep_operation, SleepOperation);

static char* const* const sleep_default_state_table[_SLEEP_OPERATION_CONFIG_MAX] = {
        [SLEEP_SUSPEND]      = STRV_MAKE("mem", "standby", "freeze"),
        [SLEEP_HIBERNATE]    = STRV_MAKE("disk"),
        [SLEEP_HYBRID_SLEEP] = STRV_MAKE("disk"),
};

static char* const* const sleep_default_mode_table[_SLEEP_OPERATION_CONFIG_MAX] = {
#if 0 /// elogind supports suspend modes (deep s2idle) so we need defaults, too
        /* Not used by SLEEP_SUSPEND */
#else // 0
        [SLEEP_SUSPEND]      = STRV_MAKE("s2idle", "deep"),
#endif // 0
        [SLEEP_HIBERNATE]    = STRV_MAKE("platform", "shutdown"),
        [SLEEP_HYBRID_SLEEP] = STRV_MAKE("suspend"),
};

#if 0 /// UNNEEDED by elogind
SleepConfig* sleep_config_free(SleepConfig *sc) {
        if (!sc)
                return NULL;

        for (SleepOperation i = 0; i < _SLEEP_OPERATION_CONFIG_MAX; i++) {
                strv_free(sc->states[i]);
                strv_free(sc->modes[i]);
        }

        return mfree(sc);
}
#endif // 0

static int config_parse_sleep_mode(
                const char *unit,
                const char *filename,
                unsigned line,
                const char *section,
                unsigned section_line,
                const char *lvalue,
                int ltype,
                const char *rvalue,
                void *data,
                void *userdata) {

        _cleanup_strv_free_ char **modes = NULL;
        char ***sv = ASSERT_PTR(data);
        int r;

        assert(filename);
        assert(lvalue);
        assert(rvalue);

        if (isempty(rvalue)) {
                modes = strv_new(NULL);
                if (!modes)
                        return log_oom();
        } else {
                r = strv_split_full(&modes, rvalue, NULL, EXTRACT_UNQUOTE|EXTRACT_RETAIN_ESCAPE);
                if (r < 0)
                        return log_oom();
        }

        return free_and_replace(*sv, modes);
}

static void sleep_config_validate_state_and_mode(SleepConfig *sc) {
        assert(sc);

        /* So we should really not allow setting SuspendState= to 'disk', which means hibernation. We have
         * SLEEP_HIBERNATE for proper hibernation support, which includes checks for resume support (through
         * EFI variable or resume= kernel command line option). It's simply not sensible to call the suspend
         * operation but eventually do an unsafe hibernation. */
        if (strv_contains(sc->states[SLEEP_SUSPEND], "disk")) {
                strv_remove(sc->states[SLEEP_SUSPEND], "disk");
                log_warning("Sleep state 'disk' is not supported by operation %s, ignoring.",
                            sleep_operation_to_string(SLEEP_SUSPEND));
        }
#if 0 /// elogind does support setting suspend modes
        assert(!sc->modes[SLEEP_SUSPEND]);
#endif // 0

        /* People should use hybrid-sleep instead of setting HibernateMode=suspend. Warn about it but don't
         * drop it in this case. */
        if (strv_contains(sc->modes[SLEEP_HIBERNATE], "suspend"))
                log_warning("Sleep mode 'suspend' should not be used by operation %s. Please use %s instead.",
                            sleep_operation_to_string(SLEEP_HIBERNATE), sleep_operation_to_string(SLEEP_HYBRID_SLEEP));
}

int parse_sleep_config(SleepConfig **ret) {
#if 0 /// elogind uses its own manager
        _cleanup_(sleep_config_freep) SleepConfig *sc = NULL;
#else // 0
        Manager* sc = *ret;
#if ENABLE_DEBUG_ELOGIND
        int dbg_cnt;
#endif // ENABLE_DEBUG_ELOGIND
#endif // 0
        int allow_suspend = -1, allow_hibernate = -1, allow_s2h = -1, allow_hybrid_sleep = -1;

        assert(ret);

#if 0 /// elogind keeps its sleep config in memory. Just erase the modes and states so they can be read anew.
        sc = new(SleepConfig, 1);
        if (!sc)
                return log_oom();

        *sc = (SleepConfig) {
                .hibernate_delay_usec = USEC_INFINITY,
        };
#else // 0
        for (SleepOperation i = 0; i < _SLEEP_OPERATION_MAX; i++) {
                if (sc->modes[i]) {
                        sc->modes[i] = strv_free(sc->modes[i]);
                }

                if (sc->states[i]) {
                        sc->states[i] = strv_free(sc->states[i]);
                }
        }
        sc->suspend_by_using   = strv_free(sc->suspend_by_using);
        sc->hibernate_by_using = strv_free(sc->hibernate_by_using);
#endif // 0

        const ConfigTableItem items[] = {
#if 1 /// Additional options for elogind
                { "Sleep", "AllowPowerOffInterrupts",     config_parse_bool, 0, &sc->allow_poweroff_interrupts },
                { "Sleep", "BroadcastPowerOffInterrupts", config_parse_bool, 0, &sc->broadcast_poweroff_interrupts },
                { "Sleep", "AllowSuspendInterrupts",      config_parse_bool, 0, &sc->allow_suspend_interrupts },
                { "Sleep", "BroadcastSuspendInterrupts",  config_parse_bool, 0, &sc->broadcast_suspend_interrupts },
                { "Sleep", "HandleNvidiaSleep",           config_parse_bool, 0, &sc->handle_nvidia_sleep },
                { "Sleep", "SuspendByUsing",              config_parse_strv, 0, &sc->suspend_by_using },
                { "Sleep", "HibernateByUsing",            config_parse_strv, 0, &sc->hibernate_by_using },
#endif // 1
                { "Sleep", "AllowSuspend",              config_parse_tristate,    0,               &allow_suspend               },
                { "Sleep", "AllowHibernation",          config_parse_tristate,    0,               &allow_hibernate             },
                { "Sleep", "AllowSuspendThenHibernate", config_parse_tristate,    0,               &allow_s2h                   },
                { "Sleep", "AllowHybridSleep",          config_parse_tristate,    0,               &allow_hybrid_sleep          },

                { "Sleep", "SuspendState",              config_parse_strv,        0,               sc->states + SLEEP_SUSPEND   },
#if 0 /// elogind does support suspend modes
                { "Sleep", "SuspendMode",               config_parse_warn_compat, DISABLED_LEGACY, NULL                         },
#else // 0
                { "Sleep", "SuspendMode",               config_parse_sleep_mode,  0,               sc->modes + SLEEP_SUSPEND    },
#endif // 0

                { "Sleep", "HibernateState",            config_parse_warn_compat, DISABLED_LEGACY, NULL                         },
                { "Sleep", "HibernateMode",             config_parse_sleep_mode,  0,               sc->modes + SLEEP_HIBERNATE  },

                { "Sleep", "HybridSleepState",          config_parse_warn_compat, DISABLED_LEGACY, NULL                         },
                { "Sleep", "HybridSleepMode",           config_parse_warn_compat, DISABLED_LEGACY, NULL                         },

                { "Sleep", "HibernateDelaySec",         config_parse_sec,         0,               &sc->hibernate_delay_usec    },
                { "Sleep", "SuspendEstimationSec",      config_parse_sec,         0,               &sc->suspend_estimation_usec },
                {}
        };

        (void) config_parse_config_file("sleep.conf", "Sleep\0",
                                        config_item_table_lookup, items,
                                        CONFIG_PARSE_WARN, NULL);

        /* use default values unless set */
        sc->allow[SLEEP_SUSPEND] = allow_suspend != 0;
        sc->allow[SLEEP_HIBERNATE] = allow_hibernate != 0;
        sc->allow[SLEEP_HYBRID_SLEEP] = allow_hybrid_sleep >= 0 ? allow_hybrid_sleep
                : (allow_suspend != 0 && allow_hibernate != 0);
        sc->allow[SLEEP_SUSPEND_THEN_HIBERNATE] = allow_s2h >= 0 ? allow_s2h
                : (allow_suspend != 0 && allow_hibernate != 0);

        for (SleepOperation i = 0; i < _SLEEP_OPERATION_CONFIG_MAX; i++) {
                if (!sc->states[i] && sleep_default_state_table[i]) {
                        sc->states[i] = strv_copy(sleep_default_state_table[i]);
                        if (!sc->states[i])
                                return log_oom();
                }

                if (!sc->modes[i] && sleep_default_mode_table[i]) {
                        sc->modes[i] = strv_copy(sleep_default_mode_table[i]);
                        if (!sc->modes[i])
                                return log_oom();
                }
        }

        if (sc->suspend_estimation_usec == 0)
                sc->suspend_estimation_usec = DEFAULT_SUSPEND_ESTIMATION_USEC;

        sleep_config_validate_state_and_mode(sc);

#if ENABLE_DEBUG_ELOGIND
        dbg_cnt = -1;
        while (sc->modes[SLEEP_SUSPEND] && sc->modes[SLEEP_SUSPEND][++dbg_cnt])
                log_debug_elogind("modes[SLEEP_SUSPEND][%d] = %s", dbg_cnt, sc->modes[SLEEP_SUSPEND][dbg_cnt]);
        dbg_cnt = -1;
        while (sc->states[SLEEP_SUSPEND] && sc->states[SLEEP_SUSPEND][++dbg_cnt])
                log_debug_elogind("states[SLEEP_SUSPEND][%d] = %s", dbg_cnt, sc->states[SLEEP_SUSPEND][dbg_cnt]);
        dbg_cnt = -1;
        while (sc->modes[SLEEP_HIBERNATE] && sc->modes[SLEEP_HIBERNATE][++dbg_cnt])
                log_debug_elogind("modes[SLEEP_HIBERNATE][%d] = %s", dbg_cnt, sc->modes[SLEEP_HIBERNATE][dbg_cnt]);
        dbg_cnt = -1;
        while (sc->states[SLEEP_HIBERNATE] && sc->states[SLEEP_HIBERNATE][++dbg_cnt])
                log_debug_elogind("states[SLEEP_HIBERNATE][%d] = %s", dbg_cnt, sc->states[SLEEP_HIBERNATE][dbg_cnt]);
        dbg_cnt = -1;
        while (sc->modes[SLEEP_HYBRID_SLEEP] && sc->modes[SLEEP_HYBRID_SLEEP][++dbg_cnt])
                log_debug_elogind("modes[SLEEP_HYBRID_SLEEP][%d] = %s", dbg_cnt, sc->modes[SLEEP_HYBRID_SLEEP][dbg_cnt]);
        dbg_cnt = -1;
        while (sc->states[SLEEP_HYBRID_SLEEP] && sc->states[SLEEP_HYBRID_SLEEP][++dbg_cnt])
                log_debug_elogind("states[SLEEP_HYBRID_SLEEP][%d] = %s", dbg_cnt, sc->states[SLEEP_HYBRID_SLEEP][dbg_cnt]);
        log_debug_elogind("hibernate_delay_usec: %lu seconds (%lu minutes)",
                          sc->hibernate_delay_usec / USEC_PER_SEC, sc->hibernate_delay_usec / USEC_PER_MINUTE);
#endif // ENABLE_DEBUG_ELOGIND

#if 0 /// UNNEEDED by elogind
        *ret = TAKE_PTR(sc);
#endif // 0

        return 0;
}

int sleep_state_supported(char **states) {
        _cleanup_free_ char *supported_sysfs = NULL;
        const char *found;
        int r;

        if (strv_isempty(states))
                return log_debug_errno(SYNTHETIC_ERRNO(ENOMSG), "No sleep state configured.");

        if (access("/sys/power/state", W_OK) < 0)
                return log_debug_errno(errno, "/sys/power/state is not writable: %m");

        r = read_one_line_file("/sys/power/state", &supported_sysfs);
        if (r < 0)
                return log_debug_errno(r, "Failed to read /sys/power/state: %m");

        r = string_contains_word_strv(supported_sysfs, NULL, states, &found);
        if (r < 0)
                return log_debug_errno(r, "Failed to parse /sys/power/state: %m");
        if (r > 0) {
                log_debug("Sleep state '%s' is supported by kernel.", found);
                return true;
        }

        if (DEBUG_LOGGING) {
                _cleanup_free_ char *joined = strv_join(states, " ");
                log_debug("None of the configured sleep states are supported by kernel: %s", strnull(joined));
        }
        return false;
}

int sleep_mode_supported(char **modes) {
        _cleanup_free_ char *supported_sysfs = NULL;
        int r;

        /* Unlike state, kernel has its own default choice if not configured */
        if (strv_isempty(modes)) {
                log_debug("No sleep mode configured, using kernel default.");
                return true;
        }

        if (access("/sys/power/disk", W_OK) < 0)
                return log_debug_errno(errno, "/sys/power/disk is not writable: %m");

        r = read_one_line_file("/sys/power/disk", &supported_sysfs);
        if (r < 0)
                return log_debug_errno(r, "Failed to read /sys/power/disk: %m");

        for (const char *p = supported_sysfs;;) {
                _cleanup_free_ char *word = NULL;
                char *mode;
                size_t l;

                r = extract_first_word(&p, &word, NULL, 0);
                if (r < 0)
                        return log_debug_errno(r, "Failed to parse /sys/power/disk: %m");
                if (r == 0)
                        break;

                mode = word;
                l = strlen(word);

                if (mode[0] == '[' && mode[l - 1] == ']') {
                        mode[l - 1] = '\0';
                        mode++;
                }

                if (strv_contains(modes, mode)) {
                        log_debug("Disk sleep mode '%s' is supported by kernel.", mode);
                        return true;
                }
        }

        if (DEBUG_LOGGING) {
                _cleanup_free_ char *joined = strv_join(modes, " ");
                log_debug("None of the configured hibernation power modes are supported by kernel: %s", strnull(joined));
        }
        return false;
}

#if 1 /// elogind also supports setting suspend modes like s2idle and deep
/* Yes, this is Copy-Pasta from above. But manipulating sleep_mode_supported() with preprocessor
 * masks to enable it to handle both, would turn it into an unmaintainable mess. (I tried...)
 * - Sven */
static int suspend_mode_supported(char **modes) {
        _cleanup_free_ char *supported_sysfs = NULL;
        int r;

        /* Unlike state, kernel has its own default choice if not configured */
        if (strv_isempty(modes)) {
                log_debug("No sleep mode configured, using kernel default.");
                return true;
        }

        if (access("/sys/power/mem_sleep", W_OK) < 0)
                return log_debug_errno(errno, "/sys/power/mem_sleep is not writable: %m");

        r = read_one_line_file("/sys/power/mem_sleep", &supported_sysfs);
        if (r < 0)
                return log_debug_errno(r, "Failed to read /sys/power/mem_sleep: %m");

        for (const char *p = supported_sysfs;;) {
                _cleanup_free_ char *word = NULL;
                char *mode;
                size_t l;

                r = extract_first_word(&p, &word, NULL, 0);
                if (r < 0)
                        return log_debug_errno(r, "Failed to parse /sys/power/mem_sleep: %m");
                if (r == 0)
                        break;

                mode = word;
                l = strlen(word);

                if (mode[0] == '[' && mode[l - 1] == ']') {
                        mode[l - 1] = '\0';
                        mode++;
                }

                if (strv_contains(modes, mode)) {
                        log_debug("Mem sleep mode '%s' is supported by kernel.", mode);
                        return true;
                }
        }

        if (DEBUG_LOGGING) {
                _cleanup_free_ char *joined = strv_join(modes, " ");
                log_debug("None of the configured suspend modes are supported by kernel: %s", strnull(joined));
        }
        return false;
}
#endif // 1

static int sleep_supported_internal(
                const SleepConfig *sleep_config,
                SleepOperation operation,
                bool check_allowed,
                SleepSupport *ret_support);

static int s2h_supported(const SleepConfig *sleep_config, SleepSupport *ret_support) {

        static const SleepOperation operations[] = {
                SLEEP_SUSPEND,
                SLEEP_HIBERNATE,
        };

        SleepSupport support;
        int r;

        assert(sleep_config);
        assert(ret_support);

        if (!clock_supported(CLOCK_BOOTTIME_ALARM)) {
                log_debug("CLOCK_BOOTTIME_ALARM is not supported, can't perform %s.", sleep_operation_to_string(SLEEP_SUSPEND_THEN_HIBERNATE));
                *ret_support = SLEEP_ALARM_NOT_SUPPORTED;
                return false;
        }

        FOREACH_ARRAY(i, operations, ELEMENTSOF(operations)) {
                r = sleep_supported_internal(sleep_config, *i, /* check_allowed = */ false, &support);
                if (r < 0)
                        return r;
                if (r == 0) {
                        log_debug("Sleep operation %s is not supported, can't perform %s.",
                                  sleep_operation_to_string(*i), sleep_operation_to_string(SLEEP_SUSPEND_THEN_HIBERNATE));
                        *ret_support = support;
                        return false;
                }
        }

        assert(support == SLEEP_SUPPORTED);
        *ret_support = support;

        return true;
}

static int sleep_supported_internal(
                const SleepConfig *sleep_config,
                SleepOperation operation,
                bool check_allowed,
                SleepSupport *ret_support) {

        int r;

        assert(sleep_config);
        assert(operation >= 0);
        assert(operation < _SLEEP_OPERATION_MAX);
        assert(ret_support);

        if (check_allowed && !sleep_config->allow[operation]) {
                log_debug("Sleep operation %s is disabled by configuration.", sleep_operation_to_string(operation));
                *ret_support = SLEEP_DISABLED;
                return false;
        }

        if (operation == SLEEP_SUSPEND_THEN_HIBERNATE)
                return s2h_supported(sleep_config, ret_support);

        assert(operation < _SLEEP_OPERATION_CONFIG_MAX);

        r = sleep_state_supported(sleep_config->states[operation]);
        if (r == -ENOMSG) {
                *ret_support = SLEEP_NOT_CONFIGURED;
                return false;
        }
        if (r < 0)
                return r;
        if (r == 0) {
                *ret_support = SLEEP_STATE_OR_MODE_NOT_SUPPORTED;
                return false;
        }

        if (sleep_operation_is_hibernation(operation)) {
                r = sleep_mode_supported(sleep_config->modes[operation]);
                if (r < 0)
                        return r;
                if (r == 0) {
                        *ret_support = SLEEP_STATE_OR_MODE_NOT_SUPPORTED;
                        return false;
                }

                r = hibernation_is_safe();
                if (r == -ENOTRECOVERABLE) {
                        *ret_support = SLEEP_RESUME_NOT_SUPPORTED;
                        return false;
                }
                if (r == -ENOSPC) {
                        *ret_support = SLEEP_NOT_ENOUGH_SWAP_SPACE;
                        return false;
                }
                if (r < 0)
                        return r;
#if 0 /// elogind does support setting suspend modes
        } else
                assert(!sleep_config->modes[operation]);
#else // 0
        } else {
                r = suspend_mode_supported(sleep_config->modes[operation]);
                if (r < 0)
                        return r;
                if (r == 0) {
                        *ret_support = SLEEP_STATE_OR_MODE_NOT_SUPPORTED;
                        return false;
                }
        }
#endif // 0

        *ret_support = SLEEP_SUPPORTED;
        return true;
}

#if 0 /// elogind stores the sleep configuration in its Manager
int sleep_supported_full(SleepOperation operation, SleepSupport *ret_support) {
        _cleanup_(sleep_config_freep) SleepConfig *sleep_config = NULL;
#else // 0
int sleep_supported_full(SleepConfig *sleep_config, SleepOperation operation, SleepSupport *ret_support) {
#endif // 0
        SleepSupport support;
        int r;

        assert(operation >= 0);
        assert(operation < _SLEEP_OPERATION_MAX);

        r = parse_sleep_config(&sleep_config);
        if (r < 0)
                return r;

        r = sleep_supported_internal(sleep_config, operation, /* check_allowed = */ true, &support);
        if (r < 0)
                return r;

        assert((r > 0) == (support == SLEEP_SUPPORTED));

        if (ret_support)
                *ret_support = support;

        return r;
}
