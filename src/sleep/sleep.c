/* SPDX-License-Identifier: LGPL-2.1-or-later */
/***
  Copyright © 2010-2017 Canonical
  Copyright © 2018 Dell Inc.
***/

#include <errno.h>
#include <fcntl.h>
//#include <getopt.h>
#include <poll.h>
#include <sys/timerfd.h>
//#include <sys/types.h>
//#include <sys/utsname.h>
#include <unistd.h>

#include "sd-bus.h"
#include "sd-device.h"
#include "sd-id128.h"
#include "sd-json.h"
#include "sd-messages.h"

#include "battery-capacity.h"
#include "battery-util.h"
//#include "build.h"
#include "bus-error.h"
//#include "bus-locator.h"
//#include "bus-unit-util.h"
#include "bus-util.h"
#include "constants.h"
//#include "devnum-util.h"
#include "efivars.h"
#include "env-util.h"
#include "exec-util.h"
#include "fd-util.h"
#include "fileio.h"
//#include "format-util.h"
#include "hibernate-util.h"
//#include "id128-util.h"
#include "io-util.h"
#include "log.h"
//#include "main-func.h"
//#include "os-util.h"
//#include "parse-util.h"
//#include "pretty-print.h"
#include "sleep-config.h"
//#include "special.h"
//#include "stdio-util.h"
//#include "string-util.h"
#include "strv.h"
#include "time-util.h"

/// Additional includes needed by elogind
#include <stdio.h>
#include "exec-elogind.h"
#include "process-util.h"
#include "sd-login.h"
#include "sleep.h"
#include "terminal-util.h"
#include "wall.h"

#define DEFAULT_HIBERNATE_DELAY_USEC_NO_BATTERY (2 * USEC_PER_HOUR)

static SleepOperation arg_operation = _SLEEP_OPERATION_INVALID;

#if 1 /// If an nvidia card is present, elogind informs its driver about suspend/resume actions
static int nvidia_sleep(Manager* m, int fd, SleepOperation operation, unsigned* vtnr) {
        static char const* drv_suspend = "/proc/driver/nvidia/suspend";
        _cleanup_fclose_ FILE *f = NULL;
        int k, r;
        char** sessions;
        unsigned vt = 0;

        assert(m);
        assert(fd >= 0);
        assert(operation >= 0);
        assert(operation <= _SLEEP_OPERATION_MAX);
        assert(operation != SLEEP_SUSPEND_THEN_HIBERNATE); /* execute_s2h() calls execute() with suspend/hibernate */
        assert(vtnr);

        k = fdopen_independent(fd, "we", &f);
        if (k < 0)
                return RET_GATHER(r, k);

        if (operation == _SLEEP_OPERATION_MAX) {
                // Wakeup the device
                log_debug_elogind("Writing 'resume' into %s", drv_suspend);
                k = write_string_stream(f, "resume", WRITE_STRING_FILE_DISABLE_BUFFER);
                if (k >= 0) {
                        return 1;
                }
                RET_GATHER(r, log_debug_errno(k, "Failed to write '%s' to %s: %m", "resume", drv_suspend));

                // Then try to change back
                if (*vtnr > 0) {
                        log_debug_elogind("Switching back to VT %u", *vtnr);
                        (void) chvt((int)(*vtnr));
                }

                return 1;
        }

        // Find the (active) sessions of the sleep sender
        *vtnr = 0;
        r = sd_uid_get_sessions(m ? m->scheduled_sleep_uid : 0, 1, &sessions);
#if ENABLE_DEBUG_ELOGIND
        char *t = strv_join(sessions, " ");
        log_debug_elogind("sd_uid_get_sessions() returned %d, result is: %s", r, strnull(t));
        free(t);
#endif // elogind debug
        if (r < 0)
                return 0;

        // Find one with a VT (Really, sessions should hold only one active session anyway!)
        STRV_FOREACH(session, sessions) {
                int s = sd_session_get_vt(*session, &vt);
                if (s >= 0) {
                        log_debug_elogind("Active session %s with VT %u found", *session, vt);
                        break;
                }
        }

        strv_free(sessions);

        // Get to a safe non-gui VT
        if ( (vt > 0) && (vt < 63) ) {
                log_debug_elogind("Storing VT %u and switching to VT %d", vt, 63);
                *vtnr = vt;
                (void) chvt(63);
        }

        // Okay, go to sleep.

        if (operation == SLEEP_SUSPEND) {
                log_debug_elogind("Writing 'suspend' into %s", drv_suspend);
                k = write_string_stream(f, "suspend", WRITE_STRING_FILE_DISABLE_BUFFER);
                if (k >= 0) {
                        return 1;
                }
                RET_GATHER(r, log_debug_errno(k, "Failed to write '%s' to %s: %m", "suspend", drv_suspend));
        } else {
                log_debug_elogind("Writing 'hibernate' into %s", drv_suspend);
                k = write_string_stream(f, "hibernate", WRITE_STRING_FILE_DISABLE_BUFFER);
                if (k >= 0) {
                        return 1;
                }
                RET_GATHER(r, log_debug_errno(k, "Failed to write '%s' to %s: %m", "hibernate", drv_suspend));
        }

        return 0;
}
#endif // 1

#if 1 /// If an external program is configured to suspend/hibernate, elogind calls that one first.
static int execute_external(Manager* m, const SleepConfig* sleep_config, SleepOperation operation) {
        int r = -1;
        char **tools = NULL;

        if ((SLEEP_SUSPEND == operation) && !strv_isempty(sleep_config->suspend_by_using))
                tools = sleep_config->suspend_by_using;

        if (((SLEEP_HIBERNATE == operation) || (SLEEP_HYBRID_SLEEP == operation)) && !strv_isempty(sleep_config->hibernate_by_using))
                tools = sleep_config->hibernate_by_using;

        if (tools) {
                STRV_FOREACH(tool, tools) {
                        int k;

                        log_debug_elogind("Calling '%s' to '%s'...", *tool, sleep_operation_to_string(operation));
                        k = safe_fork(*tool, FORK_LOG|FORK_RESET_SIGNALS|FORK_CLOSE_ALL_FDS, &m->tool_fork_pid);

                        if (k < 0) {
                                r = log_error_errno(errno, "Failed to fork run %s: %m", *tool);
                                continue;
                        }

                        if (0 == k) {
                                /* Child */
                                execlp(*tool, *tool, NULL);
                                log_error_errno(errno, "Failed to execute %s: %m", *tool);
                                _exit(EXIT_FAILURE);
                        }

                        log_debug_elogind( "Forking off '%s' returned %d and set PID %d", *tool, k, m->tool_fork_pid );

                        return 0;
                }
        }

        return r;
}
#endif // 1

static int write_efi_hibernate_location(const HibernationDevice *hibernation_device, bool required) {
        int log_level = required ? LOG_ERR : LOG_DEBUG;

#if ENABLE_EFI
        _cleanup_(sd_json_variant_unrefp) sd_json_variant *v = NULL;
        _cleanup_free_ char *formatted = NULL, *id = NULL, *image_id = NULL,
                       *version_id = NULL, *image_version = NULL;
        _cleanup_(sd_device_unrefp) sd_device *device = NULL;
        const char *uuid_str;
        sd_id128_t uuid;
        struct utsname uts = {};
        int r, log_level_ignore = required ? LOG_WARNING : LOG_DEBUG;

        assert(hibernation_device);

        if (!is_efi_boot())
                return log_full_errno(log_level, SYNTHETIC_ERRNO(EOPNOTSUPP),
                                      "Not an EFI boot, passing HibernateLocation via EFI variable is not possible.");

        r = sd_device_new_from_devnum(&device, 'b', hibernation_device->devno);
        if (r < 0)
                return log_full_errno(log_level, r, "Failed to create sd-device object for '%s': %m",
                                      hibernation_device->path);

        r = sd_device_get_property_value(device, "ID_FS_UUID", &uuid_str);
        if (r < 0)
                return log_full_errno(log_level, r, "Failed to get filesystem UUID for device '%s': %m",
                                      hibernation_device->path);

        r = sd_id128_from_string(uuid_str, &uuid);
        if (r < 0)
                return log_full_errno(log_level, r, "Failed to parse ID_FS_UUID '%s' for device '%s': %m",
                                      uuid_str, hibernation_device->path);

        if (uname(&uts) < 0)
                log_full_errno(log_level_ignore, errno, "Failed to get kernel info, ignoring: %m");

        r = parse_os_release(NULL,
                             "ID", &id,
                             "IMAGE_ID", &image_id,
                             "VERSION_ID", &version_id,
                             "IMAGE_VERSION", &image_version);
        if (r < 0)
                log_full_errno(log_level_ignore, r, "Failed to parse os-release, ignoring: %m");

        r = sd_json_buildo(
                        &v,
                        SD_JSON_BUILD_PAIR_UUID("uuid", uuid),
                        SD_JSON_BUILD_PAIR_UNSIGNED("offset", hibernation_device->offset),
                        SD_JSON_BUILD_PAIR_CONDITION(!isempty(uts.release), "kernelVersion", SD_JSON_BUILD_STRING(uts.release)),
                        SD_JSON_BUILD_PAIR_CONDITION(!!id, "osReleaseId", SD_JSON_BUILD_STRING(id)),
                        SD_JSON_BUILD_PAIR_CONDITION(!!image_id, "osReleaseImageId", SD_JSON_BUILD_STRING(image_id)),
                        SD_JSON_BUILD_PAIR_CONDITION(!!version_id, "osReleaseVersionId", SD_JSON_BUILD_STRING(version_id)),
                        SD_JSON_BUILD_PAIR_CONDITION(!!image_version, "osReleaseImageVersion", SD_JSON_BUILD_STRING(image_version)));
        if (r < 0)
                return log_full_errno(log_level, r, "Failed to build JSON object: %m");

        r = sd_json_variant_format(v, 0, &formatted);
        if (r < 0)
                return log_full_errno(log_level, r, "Failed to format JSON object: %m");

        r = efi_set_variable_string(EFI_SYSTEMD_VARIABLE_STR("HibernateLocation"), formatted);
        if (r < 0)
                return log_full_errno(log_level, r, "Failed to set EFI variable HibernateLocation: %m");

        log_debug("Set EFI variable HibernateLocation to '%s'.", formatted);
        return 0;
#else
        return log_full_errno(log_level, SYNTHETIC_ERRNO(EOPNOTSUPP),
                              "EFI support not enabled, passing HibernateLocation via EFI variable is not possible.");
#endif
}

static int write_state(int fd, char * const *states) {
        int r = 0;

        assert(fd >= 0);
        assert(states);

        STRV_FOREACH(state, states) {
                _cleanup_fclose_ FILE *f = NULL;
                int k;

                k = fdopen_independent(fd, "we", &f);
                if (k < 0)
                        return RET_GATHER(r, k);

                k = write_string_stream(f, *state, WRITE_STRING_FILE_DISABLE_BUFFER);
                if (k >= 0) {
                        log_debug("Using sleep state '%s'.", *state);
                        return 0;
                }

                RET_GATHER(r, log_debug_errno(k, "Failed to write '%s' to /sys/power/state: %m", *state));
        }

        return r;
}

static int write_mode(const char *path, char * const *modes) {
        int r, ret = 0;

        assert(path);

        STRV_FOREACH(mode, modes) {
                r = write_string_file(path, *mode, WRITE_STRING_FILE_DISABLE_BUFFER);
                if (r >= 0) {
                        log_debug("Using sleep mode '%s' for %s.", *mode, path);
                        return 0;
                }

                RET_GATHER(ret, log_debug_errno(r, "Failed to write '%s' to %s: %m", *mode, path));
        }

        return ret;
}

#if 0 /// elogind does neither ship homed nor supports any substitution right now
static int lock_all_homes(void) {
        _cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
        _cleanup_(sd_bus_message_unrefp) sd_bus_message *m = NULL;
        _cleanup_(sd_bus_flush_close_unrefp) sd_bus *bus = NULL;
        int r;

        /* Let's synchronously lock all home directories managed by homed that have been marked for it. This
         * way the key material required to access these volumes is hopefully removed from memory. */

        r = sd_bus_open_system(&bus);
        if (r < 0)
                return log_error_errno(r, "Failed to connect to system bus: %m");

        r = bus_message_new_method_call(bus, &m, bus_home_mgr, "LockAllHomes");
        if (r < 0)
                return bus_log_create_error(r);

        /* If homed is not running it can't have any home directories active either. */
        r = sd_bus_message_set_auto_start(m, false);
        if (r < 0)
                return log_error_errno(r, "Failed to disable auto-start of LockAllHomes() message: %m");

        r = sd_bus_call(bus, m, DEFAULT_TIMEOUT_USEC, &error, NULL);
        if (r < 0) {
                if (!bus_error_is_unknown_service(&error))
                        return log_error_errno(r, "Failed to lock home directories: %s", bus_error_message(&error, r));

                log_debug("systemd-homed is not running, locking of home directories skipped.");
        } else
                log_debug("Successfully requested locking of all home directories.");
        return 0;
}
#endif // 0

static int execute(
                const SleepConfig *sleep_config,
                SleepOperation operation,
                const char *action) {

#if 1 /// elogind has to use a trick here: Our manager is parked in the sleep config struct
        Manager* m = (Manager*)sleep_config->manager;

        /* We also need a read/write pointer into the sleep config */
        SleepConfig *sc = (SleepConfig*)sleep_config;
#endif // 1

        const char *arguments[] = {
                NULL,
                "pre",
                /* NB: we use 'arg_operation' instead of 'operation' here, as we want to communicate the overall
                 * operation here, not the specific one, in case of s2h. */
                sleep_operation_to_string(arg_operation),
                NULL
        };
        static const char* const dirs[] = {
                SYSTEM_SLEEP_PATH,
#if 1 /// elogind also supports a hook dir in etc
                PKGSYSCONFDIR "/system-sleep",
#endif // 1
                NULL
        };

#if 1 /// elogind has to check hooks itself and tries to work around a missing nvidia-suspend script (if needed)
        void* gather_args[] = {
                [STDOUT_GENERATE] = sc,
                [STDOUT_COLLECT] = sc,
                [STDOUT_CONSUME] = sc,
        };
        int have_nvidia = 0;
        unsigned vtnr = 0;
        int e;
        _cleanup_free_ char *l = NULL;

        log_debug_elogind("Called for '%s' (Manager is %s)", sleep_operation_to_string(operation), m ? "Set" : "NULL");
#endif // 1

        _cleanup_close_ int state_fd = -EBADF;
#if 1 /// If nvidia suspend is to be used, elogind has to write to its drivers proc place
        _cleanup_close_ int driver_fd = -EBADF;
#endif // 1
        int r;

        assert(sleep_config);
        assert(operation >= 0);
        assert(operation < _SLEEP_OPERATION_CONFIG_MAX); /* Others are handled by execute_s2h() instead */

        if (strv_isempty(sleep_config->states[operation]))
                return log_error_errno(SYNTHETIC_ERRNO(EINVAL),
                                       "No sleep states configured for sleep operation %s, can't sleep.",
                                       sleep_operation_to_string(operation));

        /* This file is opened first, so that if we hit an error, we can abort before modifying any state. */
        state_fd = open("/sys/power/state", O_WRONLY|O_CLOEXEC);
        if (state_fd < 0)
                return log_error_errno(errno, "Failed to open /sys/power/state: %m");

#if 1 /// The same with nvidia handling. elogind opens the write-to path now, so we can exit early on errors
        if (sleep_config->handle_nvidia_sleep) {
                /* This file is opened first, so that if we hit an error, we can abort before modifying any state. */
                driver_fd = open("/proc/driver/nvidia/suspend", O_WRONLY | O_CLOEXEC);
                if (driver_fd < 0)
                        return log_error_errno(errno, "Failed to open /proc/driver/nvidia/suspend: %m");
        }
#endif // 1

        if (SLEEP_NEEDS_MEM_SLEEP(sleep_config, operation)) {
                r = write_mode("/sys/power/mem_sleep", sleep_config->mem_modes);
                if (r < 0)
                        return log_error_errno(r, "Failed to write mode to /sys/power/mem_sleep: %m");
        }

        /* Configure hibernation settings if we are supposed to hibernate */
        if (SLEEP_OPERATION_IS_HIBERNATION(operation)) {
                _cleanup_(hibernation_device_done) HibernationDevice hibernation_device = {};
                bool resume_set;

                r = find_suitable_hibernation_device(&hibernation_device);
                if (r < 0)
                        return log_error_errno(r, "Failed to find location to hibernate to: %m");
                resume_set = r > 0;

                r = write_efi_hibernate_location(&hibernation_device, !resume_set);
                if (!resume_set) {
                        if (r == -EOPNOTSUPP)
                                return log_error_errno(r, "No valid 'resume=' option found, refusing to hibernate.");
                        if (r < 0)
                                return r;

                        r = write_resume_config(hibernation_device.devno, hibernation_device.offset, hibernation_device.path);
                        if (r < 0)
                                goto fail;
                }

                r = write_mode("/sys/power/disk", sleep_config->modes[operation]);
                if (r < 0) {
                        log_error_errno(r, "Failed to write mode to /sys/power/disk: %m");
                        goto fail;
                }
        }

        /* Pass an action string to the call-outs. This is mostly our operation string, except if the
         * hibernate step of s-t-h fails, in which case we communicate that with a separate action. */
        if (!action)
                action = sleep_operation_to_string(operation);

        if (setenv("SYSTEMD_SLEEP_ACTION", action, /* overwrite = */ 1) < 0)
                log_warning_errno(errno, "Failed to set SYSTEMD_SLEEP_ACTION=%s, ignoring: %m", action);

#if 0 /// elogind allows admins to configure that hook scripts must succeed. The systemd default does not cut it here
        (void) execute_directories(dirs, DEFAULT_TIMEOUT_USEC, NULL, NULL, (char **) arguments, NULL, EXEC_DIR_PARALLEL | EXEC_DIR_IGNORE_ERRORS);
        (void) lock_all_homes();
#else // 0
        sc->callback_failed = false;
        sc->callback_must_succeed = sleep_config->allow_suspend_interrupts;

        log_debug_elogind("Executing suspend hook scripts... (Must succeed: %s)",
                          sleep_config->callback_must_succeed ? "YES" : "no");

        r = execute_directories(dirs, DEFAULT_TIMEOUT_USEC, gather_output, gather_args, (char **) arguments, NULL, EXEC_DIR_NONE);

        log_debug_elogind("Result is %d (callback_failed: %s)", r, sleep_config->callback_failed ? "true" : "false");

        if (sleep_config->callback_must_succeed && (r || sleep_config->callback_failed)) {
                e = asprintf(&l, "A sleep script in %s or %s failed! [%d]\nThe system %s has been cancelled!",
                             SYSTEM_SLEEP_PATH, PKGSYSCONFDIR "/system-sleep", r, sleep_operation_to_string(operation));
                if (e < 0) {
                        log_oom();
                        return -ENOMEM;
                }

                if ( sleep_config->broadcast_suspend_interrupts )
                        wall(l, "root", "n/a", logind_wall_tty_filter, sc);

                log_error_errno(r, "MESSAGE_ID=" SD_MESSAGE_SLEEP_STOP_STR " %s", l);

                return -ECANCELED;
        }

        /* If this was successful and hook scripts were allowed to interrupt, we have
         * to signal everybody that a sleep is imminent, now.
         * Note: send_prepare_for() can not be used from here, so we do it by hand.
         */
        if ( sleep_config->allow_suspend_interrupts )
                (void) sd_bus_emit_signal(m->bus,
                                "/org/freedesktop/login1",
                                "org.freedesktop.login1.Manager",
                                "PrepareForSleep",
                                "b",
                                1);
#endif // 0

        log_struct(LOG_INFO,
                   "MESSAGE_ID=" SD_MESSAGE_SLEEP_START_STR,
                   LOG_MESSAGE("Performing sleep operation '%s'...", sleep_operation_to_string(operation)),
                   "SLEEP=%s", sleep_operation_to_string(arg_operation));

#if 1 /// elogind may try to send a suspend signal to an nvidia card
        if ( sleep_config->handle_nvidia_sleep )
                have_nvidia = nvidia_sleep(m, driver_fd, operation, &vtnr);
#endif // 1

#if 0 /// Instead of only writing to /sys/power/state, elogind offers the possibility to call an extra program instead
        r = write_state(state_fd, sleep_config->states[operation]);
#else // 0
        r = execute_external(m, sleep_config, operation);
        if (r < 0)
                r = write_state(state_fd, sleep_config->states[operation]);
#endif // 0
        if (r < 0)
                log_struct_errno(LOG_ERR, r,
                                 "MESSAGE_ID=" SD_MESSAGE_SLEEP_STOP_STR,
                                 LOG_MESSAGE("Failed to put system to sleep. System resumed again: %m"),
                                 "SLEEP=%s", sleep_operation_to_string(arg_operation));
        else
                log_struct(LOG_INFO,
                           "MESSAGE_ID=" SD_MESSAGE_SLEEP_STOP_STR,
                           LOG_MESSAGE("System returned from sleep operation '%s'.", sleep_operation_to_string(arg_operation)),
                           "SLEEP=%s", sleep_operation_to_string(arg_operation));

#if 1 /// if put to sleep, elogind also has to wakeup an nvidia card
        if (have_nvidia)
                nvidia_sleep(m, driver_fd, _SLEEP_OPERATION_MAX, &vtnr);
#endif // 1

        arguments[1] = "post";
#if 0 /// elogind does not execute wakeup hook scripts in parallel, they might be order relevant
        (void) execute_directories(dirs, DEFAULT_TIMEOUT_USEC, NULL, NULL, (char **) arguments, NULL, EXEC_DIR_PARALLEL | EXEC_DIR_IGNORE_ERRORS);
#else // 0
        (void) execute_directories(dirs, DEFAULT_TIMEOUT_USEC, NULL, NULL, (char **) arguments, NULL, EXEC_DIR_IGNORE_ERRORS);
#endif // 0

        if (r >= 0)
                return 0;

fail:
        if (SLEEP_OPERATION_IS_HIBERNATION(operation))
                (void) clear_efi_hibernate_location_and_warn();

        return r;
}

/* Return true if wakeup type is APM timer */
static int check_wakeup_type(void) {
        static const char dmi_object_path[] = "/sys/firmware/dmi/entries/1-0/raw";
        uint8_t wakeup_type_byte, tablesize;
        _cleanup_free_ char *buf = NULL;
        size_t bufsize;
        int r;

        /* implementation via dmi/entries */
        r = read_full_virtual_file(dmi_object_path, &buf, &bufsize);
        if (r < 0)
                return log_debug_errno(r, "Unable to read %s: %m", dmi_object_path);
        if (bufsize < 25)
                return log_debug_errno(SYNTHETIC_ERRNO(EINVAL),
                                       "Only read %zu bytes from %s (expected 25)",
                                       bufsize, dmi_object_path);

        /* index 1 stores the size of table */
        tablesize = (uint8_t) buf[1];
        if (tablesize < 25)
                return log_debug_errno(SYNTHETIC_ERRNO(EINVAL),
                                       "Table size less than the index[0x18] where waketype byte is available.");

        wakeup_type_byte = (uint8_t) buf[24];
        /* 0 is Reserved and 8 is AC Power Restored. As per table 12 in
         * https://www.dmtf.org/sites/default/files/standards/documents/DSP0134_3.4.0.pdf */
        if (wakeup_type_byte >= 128)
                return log_debug_errno(SYNTHETIC_ERRNO(EINVAL), "Expected value in range 0-127");

        if (wakeup_type_byte == 3) {
                log_debug("DMI BIOS System Information indicates wakeup type is APM Timer");
                return true;
        }

        return false;
}

static int custom_timer_suspend(const SleepConfig *sleep_config) {
        usec_t hibernate_timestamp;
        int r;

        assert(sleep_config);

        hibernate_timestamp = usec_add(now(CLOCK_BOOTTIME), sleep_config->hibernate_delay_usec);

        while (battery_is_discharging_and_low() == 0) {
                _cleanup_hashmap_free_ Hashmap *last_capacity = NULL, *current_capacity = NULL;
                _cleanup_close_ int tfd = -EBADF;
                struct itimerspec ts = {};
                usec_t suspend_interval;
                bool woken_by_timer;

                tfd = timerfd_create(CLOCK_BOOTTIME_ALARM, TFD_NONBLOCK|TFD_CLOEXEC);
                if (tfd < 0)
                        return log_error_errno(errno, "Error creating timerfd: %m");

                /* Store current battery capacity before suspension */
                r = fetch_batteries_capacity_by_name(&last_capacity);
                if (r < 0)
                        return log_error_errno(r, "Error fetching battery capacity percentage: %m");

                if (hashmap_isempty(last_capacity))
                        /* In case of no battery, system suspend interval will be set to HibernateDelaySec= or 2 hours. */
                        suspend_interval = timestamp_is_set(hibernate_timestamp)
                                           ? sleep_config->hibernate_delay_usec : DEFAULT_HIBERNATE_DELAY_USEC_NO_BATTERY;
                else {
                        r = get_total_suspend_interval(last_capacity, &suspend_interval);
                        if (r < 0) {
                                log_debug_errno(r, "Failed to estimate suspend interval using previous discharge rate, ignoring: %m");
                                /* In case of any errors, especially when we do not know the battery
                                 * discharging rate, system suspend interval will be set to
                                 * SuspendEstimationSec=. */
                                suspend_interval = sleep_config->suspend_estimation_usec;
                        }
                }

                /* Do not suspend more than HibernateDelaySec= unless HibernateOnACPower=no and currently on AC power */
                if (!sleep_config->hibernate_on_ac_power) {
                        /* Do not allow "decay" to suspend if the system has no battery. */
                        if (hashmap_isempty(last_capacity))
                                log_once(LOG_WARNING, "HibernateOnACPower=no was ignored because the system does not have a battery.");
                        else if (on_ac_power() > 0)
                                hibernate_timestamp = usec_add(now(CLOCK_BOOTTIME), sleep_config->hibernate_delay_usec);
                }

                usec_t before_timestamp = now(CLOCK_BOOTTIME);
                suspend_interval = MIN(suspend_interval, usec_sub_unsigned(hibernate_timestamp, before_timestamp));
                if (suspend_interval <= 0)
                        break; /* system should hibernate */

                log_debug("Set timerfd wake alarm for %s", FORMAT_TIMESPAN(suspend_interval, USEC_PER_SEC));
                /* Wake alarm for system with or without battery to hibernate or estimate discharge rate whichever is applicable */
                timespec_store(&ts.it_value, suspend_interval);

                if (timerfd_settime(tfd, 0, &ts, NULL) < 0)
                        return log_error_errno(errno, "Error setting battery estimate timer: %m");

                r = execute(sleep_config, SLEEP_SUSPEND, NULL);
                if (r < 0)
                        return r;

                r = fd_wait_for_event(tfd, POLLIN, 0);
                if (r < 0)
                        return log_error_errno(r, "Error polling timerfd: %m");
                /* Store fd_wait status */
                woken_by_timer = FLAGS_SET(r, POLLIN);

                r = fetch_batteries_capacity_by_name(&current_capacity);
                if (r < 0 || hashmap_isempty(current_capacity)) {
                        /* In case of no battery or error while getting charge level, no need to measure
                         * discharge rate. Instead the system should wake up if it is manual wakeup or
                         * hibernate if this is a timer wakeup. */
                        if (r < 0)
                                log_debug_errno(r, "Battery capacity percentage unavailable, cannot estimate discharge rate: %m");
                        else
                                log_debug("No battery found.");
                        if (!woken_by_timer)
                                return 0;
                        break;
                }

                usec_t after_timestamp = now(CLOCK_BOOTTIME);
                log_debug("Attempting to estimate battery discharge rate after wakeup from %s sleep",
                          FORMAT_TIMESPAN(after_timestamp - before_timestamp, USEC_PER_HOUR));

                if (after_timestamp != before_timestamp) {
                        r = estimate_battery_discharge_rate_per_hour(last_capacity, current_capacity, before_timestamp, after_timestamp);
                        if (r < 0)
                                log_warning_errno(r, "Failed to estimate and update battery discharge rate, ignoring: %m");
                } else
                        log_debug("System woke up too early to estimate discharge rate.");

                if (!woken_by_timer)
                        /* Return as manual wakeup done. This also will return in case battery was charged during suspension */
                        return 0;

                r = check_wakeup_type();
                if (r > 0) {
                        log_debug("wakeup type is APM timer");
                        /* system should hibernate */
                        break;
                }
        }

        return 1;
}

static int execute_s2h(const SleepConfig *sleep_config) {
        int r;

        assert(sleep_config);

        /* Only check if we have automated battery alarms if HibernateDelaySec= is not set, as in that case
         * we'll busy poll for the configured interval instead */
        if (!timestamp_is_set(sleep_config->hibernate_delay_usec)) {
                r = check_wakeup_type();
                if (r < 0)
                        log_warning_errno(r, "Failed to check hardware wakeup type, ignoring: %m");
                else {
                        r = battery_trip_point_alarm_exists();
                        if (r < 0)
                                log_warning_errno(r, "Failed to check whether acpi_btp support is enabled or not, ignoring: %m");
                }
        } else
                r = 0;  /* Force fallback path */

        if (r > 0) { /* If we have both wakeup alarms and battery trip point support, use them */
                log_debug("Attempting to suspend...");
                r = execute(sleep_config, SLEEP_SUSPEND, NULL);
                if (r < 0)
                        return r;

                r = check_wakeup_type();
                if (r < 0)
                        return log_error_errno(r, "Failed to check hardware wakeup type: %m");

                if (r == 0)
                        /* For APM Timer wakeup, system should hibernate else wakeup */
                        return 0;
        } else {
                r = custom_timer_suspend(sleep_config);
                if (r < 0)
                        return log_debug_errno(r, "Suspend cycle with manual battery discharge rate estimation failed: %m");
                if (r == 0)
                        /* manual wakeup */
                        return 0;
        }
        /* For above custom timer, if 1 is returned, system will directly hibernate */

        log_debug("Attempting to hibernate");
        r = execute(sleep_config, SLEEP_HIBERNATE, NULL);
        if (r < 0) {
                log_notice("Couldn't hibernate, will try to suspend again.");

                r = execute(sleep_config, SLEEP_SUSPEND, "suspend-after-failed-hibernate");
                if (r < 0)
                        return r;
        }

        return 0;
}

#if 0 /// elogind calls execute() by itself and does not need another binary
static int help(void) {
        _cleanup_free_ char *link = NULL;
        int r;

        r = terminal_urlify_man("systemd-suspend.service", "8", &link);
        if (r < 0)
                return log_oom();

        printf("%s COMMAND\n\n"
               "Suspend the system, hibernate the system, or both.\n\n"
               "  -h --help              Show this help and exit\n"
               "  --version              Print version string and exit\n"
               "\nCommands:\n"
               "  suspend                Suspend the system\n"
               "  hibernate              Hibernate the system\n"
               "  hybrid-sleep           Both hibernate and suspend the system\n"
               "  suspend-then-hibernate Initially suspend and then hibernate\n"
               "                         the system after a fixed period of time or\n"
               "                         when battery is low\n"
               "\nSee the %s for details.\n",
               program_invocation_short_name,
               link);

        return 0;
}

static int parse_argv(int argc, char *argv[]) {

        enum {
                ARG_VERSION = 0x100,
        };

        static const struct option options[] = {
                { "help",         no_argument,       NULL, 'h'           },
                { "version",      no_argument,       NULL, ARG_VERSION   },
                {}
        };

        int c;

        assert(argc >= 0);
        assert(argv);

        while ((c = getopt_long(argc, argv, "h", options, NULL)) >= 0)
                switch (c) {

                case 'h':
                        return help();

                case ARG_VERSION:
                        return version();

                case '?':
                        return -EINVAL;

                default:
                        assert_not_reached();

                }

        if (argc - optind != 1)
                return log_error_errno(SYNTHETIC_ERRNO(EINVAL),
                                       "Usage: %s COMMAND",
                                       program_invocation_short_name);

        arg_operation = sleep_operation_from_string(argv[optind]);
        if (arg_operation < 0)
                return log_error_errno(SYNTHETIC_ERRNO(EINVAL), "Unknown command '%s'.", argv[optind]);

        return 1 /* work to do */;
}

static int run(int argc, char *argv[]) {
        _cleanup_(unit_freezer_freep) UnitFreezer *user_slice_freezer = NULL;
        _cleanup_(sleep_config_freep) SleepConfig *sleep_config = NULL;
        int r;

        log_setup();

        r = parse_argv(argc, argv);
        if (r <= 0)
                return r;
#else // 0
int do_sleep(Manager* m, SleepOperation operation) {
        _cleanup_(sleep_config_freep) SleepConfig *sleep_config = NULL;
        int r;

        assert(operation < _SLEEP_OPERATION_MAX);
        assert(sleep_config);

        arg_operation = operation;

        log_debug_elogind("Called for '%s'", sleep_operation_to_string(operation));
#endif // 0

        r = parse_sleep_config(&sleep_config);
        if (r < 0)
                return r;

        if (!sleep_config->allow[arg_operation])
                return log_error_errno(SYNTHETIC_ERRNO(EACCES),
                                       "Sleep operation \"%s\" is disabled by configuration, refusing.",
                                       sleep_operation_to_string(arg_operation));

#if 0 /// This freezes the user slice, which is not supported by elogind
        /* Freeze the user sessions */
        r = getenv_bool("SYSTEMD_SLEEP_FREEZE_USER_SESSIONS");
        if (r < 0 && r != -ENXIO)
                log_warning_errno(r, "Cannot parse value of $SYSTEMD_SLEEP_FREEZE_USER_SESSIONS, ignoring: %m");
#endif // 0
        if (r != 0) {
                r = unit_freezer_new(SPECIAL_USER_SLICE, &user_slice_freezer);
                if (r < 0)
                        return r;

                (void) unit_freezer_freeze(user_slice_freezer);
        } else
                log_notice("User sessions remain unfrozen on explicit request ($SYSTEMD_SLEEP_FREEZE_USER_SESSIONS=0).\n"
                           "This is not recommended, and might result in unexpected behavior, particularly\n"
                           "in suspend-then-hibernate operations or setups with encrypted home directories.");

        switch (arg_operation) {

        case SLEEP_SUSPEND_THEN_HIBERNATE:
                r = execute_s2h(sleep_config);
                break;

        case SLEEP_HYBRID_SLEEP:
                r = execute(sleep_config, SLEEP_HYBRID_SLEEP, NULL);
                if (r < 0) {
                        /* If we can't hybrid sleep, then let's try to suspend at least. After all, the user
                         * asked us to do both: suspend + hibernate, and it's almost certainly the
                         * hibernation that failed, hence still do the other thing, the suspend. */

                        log_notice_errno(r, "Couldn't hybrid sleep, will try to suspend instead: %m");

                        r = execute(sleep_config, SLEEP_SUSPEND, "suspend-after-failed-hybrid-sleep");
                }

                break;

        case SLEEP_SUSPEND:
        case SLEEP_HIBERNATE:
                r = execute(sleep_config, arg_operation, NULL);
                break;

        default:
                assert_not_reached();
        }

#if 0 /// elogind does not support user slices
        if (user_slice_freezer)
                RET_GATHER(r, unit_freezer_thaw(user_slice_freezer));
#endif // 0

        return r;
}

#if 0 /// No main function needed by elogind
DEFINE_MAIN_FUNCTION(run);
#endif // 0
