/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <unistd.h>

#include "sd-messages.h"

#include "alloc-util.h"
#include "bus-error.h"
//#include "bus-util.h"
#include "conf-parser.h"
#include "format-util.h"
#include "logind-action.h"
#include "logind-dbus.h"
#include "logind-session-dbus.h"
#include "process-util.h"
#include "special.h"
#include "string-table.h"
#include "terminal-util.h"
#include "user-util.h"

/// Additional includes needed by elogind
#include "fd-util.h"

static const HandleActionData handle_action_data_table[_HANDLE_ACTION_MAX] = {
        [HANDLE_POWEROFF] = {
                .handle                          = HANDLE_POWEROFF,
                .target                          = SPECIAL_POWEROFF_TARGET,
                .inhibit_what                    = INHIBIT_SHUTDOWN,
                .polkit_action                   = "org.freedesktop.login1.power-off",
                .polkit_action_multiple_sessions = "org.freedesktop.login1.power-off-multiple-sessions",
                .polkit_action_ignore_inhibit    = "org.freedesktop.login1.power-off-ignore-inhibit",
                .sleep_operation                 = _SLEEP_OPERATION_INVALID,
                .message_id                      = SD_MESSAGE_SHUTDOWN_STR,
                .message                         = "System is powering down",
                .log_verb                        = "power-off",
        },
        [HANDLE_REBOOT] = {
                .handle                          = HANDLE_REBOOT,
                .target                          = SPECIAL_REBOOT_TARGET,
                .inhibit_what                    = INHIBIT_SHUTDOWN,
                .polkit_action                   = "org.freedesktop.login1.reboot",
                .polkit_action_multiple_sessions = "org.freedesktop.login1.reboot-multiple-sessions",
                .polkit_action_ignore_inhibit    = "org.freedesktop.login1.reboot-ignore-inhibit",
                .sleep_operation                 = _SLEEP_OPERATION_INVALID,
                .message_id                      = SD_MESSAGE_SHUTDOWN_STR,
                .message                         = "System is rebooting",
                .log_verb                        = "reboot",
        },
        [HANDLE_HALT] = {
                .handle                          = HANDLE_HALT,
                .target                          = SPECIAL_HALT_TARGET,
                .inhibit_what                    = INHIBIT_SHUTDOWN,
                .polkit_action                   = "org.freedesktop.login1.halt",
                .polkit_action_multiple_sessions = "org.freedesktop.login1.halt-multiple-sessions",
                .polkit_action_ignore_inhibit    = "org.freedesktop.login1.halt-ignore-inhibit",
                .sleep_operation                 = _SLEEP_OPERATION_INVALID,
                .message_id                      = SD_MESSAGE_SHUTDOWN_STR,
                .message                         = "System is halting",
                .log_verb                        = "halt",
        },
        [HANDLE_KEXEC] = {
                .handle                          = HANDLE_KEXEC,
                .target                          = SPECIAL_KEXEC_TARGET,
                .inhibit_what                    = INHIBIT_SHUTDOWN,
                .polkit_action                   = "org.freedesktop.login1.reboot",
                .polkit_action_multiple_sessions = "org.freedesktop.login1.reboot-multiple-sessions",
                .polkit_action_ignore_inhibit    = "org.freedesktop.login1.reboot-ignore-inhibit",
                .sleep_operation                 = _SLEEP_OPERATION_INVALID,
                .message_id                      = SD_MESSAGE_SHUTDOWN_STR,
                .message                         = "System is rebooting with kexec",
                .log_verb                        = "kexec",
        },
        [HANDLE_SOFT_REBOOT] = {
                .handle                          = HANDLE_SOFT_REBOOT,
                .target                          = SPECIAL_SOFT_REBOOT_TARGET,
                .inhibit_what                    = INHIBIT_SHUTDOWN,
                .polkit_action                   = "org.freedesktop.login1.reboot",
                .polkit_action_multiple_sessions = "org.freedesktop.login1.reboot-multiple-sessions",
                .polkit_action_ignore_inhibit    = "org.freedesktop.login1.reboot-ignore-inhibit",
                .sleep_operation                 = _SLEEP_OPERATION_INVALID,
                .message_id                      = SD_MESSAGE_SHUTDOWN_STR,
                .message                         = "System userspace is rebooting",
                .log_verb                        = "soft-reboot",
        },
        [HANDLE_SUSPEND] = {
                .handle                          = HANDLE_SUSPEND,
                .target                          = SPECIAL_SUSPEND_TARGET,
                .inhibit_what                    = INHIBIT_SLEEP,
                .polkit_action                   = "org.freedesktop.login1.suspend",
                .polkit_action_multiple_sessions = "org.freedesktop.login1.suspend-multiple-sessions",
                .polkit_action_ignore_inhibit    = "org.freedesktop.login1.suspend-ignore-inhibit",
                .sleep_operation                 = SLEEP_SUSPEND,
        },
        [HANDLE_HIBERNATE] = {
                .handle                          = HANDLE_HIBERNATE,
                .target                          = SPECIAL_HIBERNATE_TARGET,
                .inhibit_what                    = INHIBIT_SLEEP,
                .polkit_action                   = "org.freedesktop.login1.hibernate",
                .polkit_action_multiple_sessions = "org.freedesktop.login1.hibernate-multiple-sessions",
                .polkit_action_ignore_inhibit    = "org.freedesktop.login1.hibernate-ignore-inhibit",
                .sleep_operation                 = SLEEP_HIBERNATE,
        },
        [HANDLE_HYBRID_SLEEP] = {
                .handle                          = HANDLE_HYBRID_SLEEP,
                .target                          = SPECIAL_HYBRID_SLEEP_TARGET,
                .inhibit_what                    = INHIBIT_SLEEP,
                .polkit_action                   = "org.freedesktop.login1.hibernate",
                .polkit_action_multiple_sessions = "org.freedesktop.login1.hibernate-multiple-sessions",
                .polkit_action_ignore_inhibit    = "org.freedesktop.login1.hibernate-ignore-inhibit",
                .sleep_operation                 = SLEEP_HYBRID_SLEEP,
        },
        [HANDLE_SUSPEND_THEN_HIBERNATE] = {
                .handle                          = HANDLE_SUSPEND_THEN_HIBERNATE,
                .target                          = SPECIAL_SUSPEND_THEN_HIBERNATE_TARGET,
                .inhibit_what                    = INHIBIT_SLEEP,
                .polkit_action                   = "org.freedesktop.login1.hibernate",
                .polkit_action_multiple_sessions = "org.freedesktop.login1.hibernate-multiple-sessions",
                .polkit_action_ignore_inhibit    = "org.freedesktop.login1.hibernate-ignore-inhibit",
                .sleep_operation                 = SLEEP_SUSPEND_THEN_HIBERNATE,
        },
        [HANDLE_FACTORY_RESET] = {
                .handle                          = HANDLE_FACTORY_RESET,
                .target                          = SPECIAL_FACTORY_RESET_TARGET,
                .inhibit_what                    = _INHIBIT_WHAT_INVALID,
                .sleep_operation                 = _SLEEP_OPERATION_INVALID,
                .message_id                      = SD_MESSAGE_FACTORY_RESET_STR,
                .message                         = "System is performing factory reset",
        },
};

const HandleActionData* handle_action_lookup(HandleAction action) {

        if (action < 0 || (size_t) action >= ELEMENTSOF(handle_action_data_table))
                return NULL;

        return &handle_action_data_table[action];
}

int manager_handle_action(
                Manager *m,
                InhibitWhat inhibit_key,
                HandleAction handle,
                bool ignore_inhibited,
                bool is_edge) {

        static const char * const message_table[_HANDLE_ACTION_MAX] = {
                [HANDLE_POWEROFF]               = "Powering off...",
                [HANDLE_REBOOT]                 = "Rebooting...",
                [HANDLE_HALT]                   = "Halting...",
                [HANDLE_KEXEC]                  = "Rebooting via kexec...",
                [HANDLE_SOFT_REBOOT]            = "Rebooting userspace...",
                [HANDLE_SUSPEND]                = "Suspending...",
                [HANDLE_HIBERNATE]              = "Hibernating...",
                [HANDLE_HYBRID_SLEEP]           = "Hibernating and suspending...",
                [HANDLE_SUSPEND_THEN_HIBERNATE] = "Suspending, then hibernating...",
                [HANDLE_FACTORY_RESET]          = "Performing factory reset...",
        };

        _cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
        InhibitWhat inhibit_operation;
        Inhibitor *offending = NULL;
        bool supported;
        int r;

        assert(m);

        /* If the key handling is turned off, don't do anything */
        if (handle == HANDLE_IGNORE) {
                log_debug("Handling of %s (%s) is disabled, taking no action.",
                          inhibit_key == 0 ? "idle timeout" : inhibit_what_to_string(inhibit_key),
                          is_edge ? "edge" : "level");
                return 0;
        }

        if (inhibit_key == INHIBIT_HANDLE_LID_SWITCH) {
                /* If the last system suspend or startup is too close,
                 * let's not suspend for now, to give USB docking
                 * stations some time to settle so that we can
                 * properly watch its displays. */
                if (m->lid_switch_ignore_event_source) {
                        log_debug("Ignoring lid switch request, system startup or resume too close.");
                        return 0;
                }
        }

        /* If the key handling is inhibited, don't do anything */
        if (inhibit_key > 0) {
                if (manager_is_inhibited(m, inhibit_key, INHIBIT_BLOCK, NULL, true, false, 0, NULL)) {
                        log_debug("Refusing %s operation, %s is inhibited.",
                                  handle_action_to_string(handle),
                                  inhibit_what_to_string(inhibit_key));
                        return 0;
                }
        }

        /* Locking is handled differently from the rest. */
        if (handle == HANDLE_LOCK) {
                if (!is_edge)
                        return 0;

                log_info("Locking sessions...");
                session_send_lock_all(m, true);
                return 1;
        }

#if 0 /// elogind needs its own can_sleep() variant.
        if (handle == HANDLE_SUSPEND)
                supported = can_sleep(SLEEP_SUSPEND) > 0;
        else if (handle == HANDLE_HIBERNATE)
                supported = can_sleep(SLEEP_HIBERNATE) > 0;
        else if (handle == HANDLE_HYBRID_SLEEP)
                supported = can_sleep(SLEEP_HYBRID_SLEEP) > 0;
        else if (handle == HANDLE_SUSPEND_THEN_HIBERNATE)
                supported = can_sleep(SLEEP_SUSPEND_THEN_HIBERNATE) > 0;
#else // 0
        if (handle == HANDLE_SUSPEND)
                supported = can_sleep(m, SLEEP_SUSPEND) > 0;
        else if (handle == HANDLE_HIBERNATE)
                supported = can_sleep(m, SLEEP_HIBERNATE) > 0;
        else if (handle == HANDLE_HYBRID_SLEEP)
                supported = can_sleep(m, SLEEP_HYBRID_SLEEP) > 0;
        else if (handle == HANDLE_SUSPEND_THEN_HIBERNATE)
                supported = can_sleep(m, SLEEP_SUSPEND_THEN_HIBERNATE) > 0;
#endif // 0
        else if (handle == HANDLE_KEXEC)
                supported = access(KEXEC, X_OK) >= 0;
        else
                supported = true;

        if (!supported && IN_SET(handle, HANDLE_HIBERNATE, HANDLE_HYBRID_SLEEP, HANDLE_SUSPEND_THEN_HIBERNATE)) {
#if 0 /// elogind needs the manager
                supported = can_sleep(SLEEP_SUSPEND) > 0;
#else // 0
                supported = can_sleep(m, SLEEP_SUSPEND) > 0;
#endif // 0
                if (supported) {
                        log_notice("Requested %s operation is not supported, using regular suspend instead.",
                                   handle_action_to_string(handle));
                        handle = HANDLE_SUSPEND;
                }
        }

        if (!supported)
                return log_warning_errno(SYNTHETIC_ERRNO(EOPNOTSUPP),
                                         "Requested %s operation not supported, ignoring.", handle_action_to_string(handle));

        if (m->delayed_action)
                return log_debug_errno(SYNTHETIC_ERRNO(EALREADY),
                                       "Action already in progress (%s), ignoring requested %s operation.",
                                       inhibit_what_to_string(m->delayed_action->inhibit_what),
                                       handle_action_to_string(handle));

        inhibit_operation = handle_action_lookup(handle)->inhibit_what;

        /* If the actual operation is inhibited, warn and fail */
        if (!ignore_inhibited &&
            manager_is_inhibited(m, inhibit_operation, INHIBIT_BLOCK, NULL, false, false, 0, &offending)) {
                _cleanup_free_ char *comm = NULL, *u = NULL;

                (void) get_process_comm(offending->pid, &comm);
                u = uid_to_name(offending->uid);

                /* If this is just a recheck of the lid switch then don't warn about anything */
                log_full(is_edge ? LOG_ERR : LOG_DEBUG,
                         "Refusing %s operation, %s is inhibited by UID "UID_FMT"/%s, PID "PID_FMT"/%s.",
                         handle_action_to_string(handle),
                         inhibit_what_to_string(inhibit_operation),
                         offending->uid, strna(u),
                         offending->pid, strna(comm));

                return is_edge ? -EPERM : 0;
        }

        log_info("%s", message_table[handle]);

        r = bus_manager_shutdown_or_sleep_now_or_later(m, handle_action_lookup(handle), &error);
        if (r < 0)
                return log_error_errno(r, "Failed to execute %s operation: %s",
                                       handle_action_to_string(handle),
                                       bus_error_message(&error, r));

        return 1;
}

static const char* const handle_action_verb_table[_HANDLE_ACTION_MAX] = {
        [HANDLE_IGNORE]                 = "do nothing",
        [HANDLE_POWEROFF]               = "power off",
        [HANDLE_REBOOT]                 = "reboot",
        [HANDLE_HALT]                   = "halt",
        [HANDLE_KEXEC]                  = "kexec",
        [HANDLE_SOFT_REBOOT]            = "soft-reboot",
        [HANDLE_SUSPEND]                = "suspend",
        [HANDLE_HIBERNATE]              = "hibernate",
        [HANDLE_HYBRID_SLEEP]           = "enter hybrid sleep",
        [HANDLE_SUSPEND_THEN_HIBERNATE] = "suspend and later hibernate",
        [HANDLE_FACTORY_RESET]          = "perform a factory reset",
        [HANDLE_LOCK]                   = "be locked",
};

DEFINE_STRING_TABLE_LOOKUP_TO_STRING(handle_action_verb, HandleAction);

static const char* const handle_action_table[_HANDLE_ACTION_MAX] = {
        [HANDLE_IGNORE]                 = "ignore",
        [HANDLE_POWEROFF]               = "poweroff",
        [HANDLE_REBOOT]                 = "reboot",
        [HANDLE_HALT]                   = "halt",
        [HANDLE_KEXEC]                  = "kexec",
        [HANDLE_SOFT_REBOOT]            = "soft-reboot",
        [HANDLE_SUSPEND]                = "suspend",
        [HANDLE_HIBERNATE]              = "hibernate",
        [HANDLE_HYBRID_SLEEP]           = "hybrid-sleep",
        [HANDLE_SUSPEND_THEN_HIBERNATE] = "suspend-then-hibernate",
        [HANDLE_FACTORY_RESET]          = "factory-reset",
        [HANDLE_LOCK]                   = "lock",
};

DEFINE_STRING_TABLE_LOOKUP(handle_action, HandleAction);
DEFINE_CONFIG_PARSE_ENUM(config_parse_handle_action, handle_action, HandleAction, "Failed to parse handle action setting");
