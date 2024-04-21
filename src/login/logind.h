/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <stdbool.h>
#include <sys/stat.h>

#include "sd-bus.h"
#include "sd-device.h"
#include "sd-event.h"

#include "conf-parser.h"
#include "hashmap.h"
#include "list.h"
#include "set.h"
#include "time-util.h"
#include "user-record.h"

typedef struct Manager Manager;

#include "logind-action.h"
#include "logind-button.h"
#include "logind-device.h"
#include "logind-inhibit.h"

/// Additional includes needed by elogind
#include "cgroup-util.h"
#include "elogind.h"
#include "musl_missing.h"
#include "sleep-config.h"

#if 1 /// elogind has to ident itself
#define MANAGER_IS_SYSTEM(m)   (  (m)->is_system)
#define MANAGER_IS_TEST_RUN(m) (  (m)->test_run_flags != 0)
#define MANAGER_IS_USER(m)     (!((m)->is_system))
#endif // 1
struct Manager {
        sd_event *event;
        sd_bus *bus;

        Hashmap *devices;
        Hashmap *seats;
        Hashmap *sessions;
        Hashmap *sessions_by_leader;
        Hashmap *users;  /* indexed by UID */
        Hashmap *inhibitors;
        Hashmap *buttons;
        Hashmap *brightness_writers;

        LIST_HEAD(Seat, seat_gc_queue);
        LIST_HEAD(Session, session_gc_queue);
        LIST_HEAD(User, user_gc_queue);

        sd_device_monitor *device_seat_monitor, *device_monitor, *device_vcsa_monitor, *device_button_monitor;

        sd_event_source *console_active_event_source;

#if ENABLE_UTMP
        sd_event_source *utmp_event_source;
#endif

        int console_active_fd;

#if 0 /// elogind does not support autospawning of vts
        unsigned n_autovts;

        unsigned reserve_vt;
        int reserve_vt_fd;
#else // 0
        /* Make sure the user cannot accidentally unmount our cgroup
         * file system */
        int pin_cgroupfs_fd;

        /* fd for handling cgroup socket if elogind is its own cgroups manager */
        int cgroups_agent_fd;
        sd_event_source *cgroups_agent_event_source;

        /* Flags */
        unsigned test_run_flags;
        bool is_system:1; /* true if elogind is its own cgroups manager */
        bool do_interrupt:1;  /* true if SIGINT is used to stop elogind. See elogind_signal_handler() */

        /* Data specific to the cgroup subsystem */
        CGroupMask cgroup_supported;
        char *cgroup_root;

        /* elogind might spawn processes to suspend/hibernate, so we need their PIDs to end them properly */
        pid_t sleep_fork_pid; /* for suspend/hibernate fork */
        pid_t tool_fork_pid;  /* for external tool fork */

        /* To wake up sleeping consumers using the right operation, the manager must know what is going on. */
        const HandleActionData *sleep_fork_action;
#endif // 0

        Seat *seat0;

        char **kill_only_users, **kill_exclude_users;
        bool kill_user_processes;

        unsigned long session_counter;
        unsigned long inhibit_counter;

#if 0 /// elogind does not support session units
        Hashmap *session_units;
#endif // 0
        Hashmap *user_units;

        usec_t inhibit_delay_max;
        usec_t user_stop_delay;

        /* If a shutdown/suspend was delayed due to an inhibitor this contains the action we are supposed to
         * start after the delay is over */
        const HandleActionData *delayed_action;

#if 0 /// elogind does all relevant actions on its own. No systemd jobs and units.
        /* If a shutdown/suspend is currently executed, then this is the job of it */
        char *action_job;
#else // 0
        /* Suspension and hibernation can be disabled in logind.conf. */
        bool allow[_SLEEP_OPERATION_MAX];
        char **modes[_SLEEP_OPERATION_MAX];
        char **states[_SLEEP_OPERATION_MAX];
        usec_t hibernate_delay_usec; /* HibernateDelaySec */
        usec_t suspend_estimation_usec; /* SuspendEstimationSec */


        /* If an admin puts scripts into SYSTEM_SLEEP_PATH and/or
           SYSTEM_POWEROFF_PATH that fail, the ongoing suspend/poweroff
           action will be cancelled if any of these are set to true. */
        bool allow_poweroff_interrupts, allow_suspend_interrupts;
        bool broadcast_poweroff_interrupts, broadcast_suspend_interrupts;
        bool callback_failed, callback_must_succeed;

        /* Allow elogind to put Nvidia cards to sleep */
        bool handle_nvidia_sleep;

        /* To allow elogind to put nvidia cards to sleep on suspend/hibernate,
           we store the users uid to get the right VT information */
        uid_t scheduled_sleep_uid;

        /* Allow users to set programs which do the suspend/hibernation */
        char **suspend_by_using;
        char **hibernate_by_using;
#endif // 0
        sd_event_source *inhibit_timeout_source;

        const HandleActionData *scheduled_shutdown_action;
        usec_t scheduled_shutdown_timeout;
        sd_event_source *scheduled_shutdown_timeout_source;
        uid_t scheduled_shutdown_uid;
        char *scheduled_shutdown_tty;
        sd_event_source *nologin_timeout_source;
        bool unlink_nologin;

        char *wall_message;
        bool enable_wall_messages;
        sd_event_source *wall_message_timeout_source;

        bool shutdown_dry_run;

        sd_event_source *idle_action_event_source;
        usec_t idle_action_usec;
        usec_t idle_action_not_before_usec;
        HandleAction idle_action;
        bool was_idle;

        usec_t stop_idle_session_usec;

        HandleAction handle_power_key;
        HandleAction handle_power_key_long_press;
        HandleAction handle_reboot_key;
        HandleAction handle_reboot_key_long_press;
        HandleAction handle_suspend_key;
        HandleAction handle_suspend_key_long_press;
        HandleAction handle_hibernate_key;
        HandleAction handle_hibernate_key_long_press;

        HandleAction handle_lid_switch;
        HandleAction handle_lid_switch_ep;
        HandleAction handle_lid_switch_docked;

        bool power_key_ignore_inhibited;
        bool suspend_key_ignore_inhibited;
        bool hibernate_key_ignore_inhibited;
        bool lid_switch_ignore_inhibited;
        bool reboot_key_ignore_inhibited;

        bool remove_ipc;

        Hashmap *polkit_registry;

        usec_t holdoff_timeout_usec;
        sd_event_source *lid_switch_ignore_event_source;

        sd_event_source *power_key_long_press_event_source;
        sd_event_source *reboot_key_long_press_event_source;
        sd_event_source *suspend_key_long_press_event_source;
        sd_event_source *hibernate_key_long_press_event_source;

        uint64_t runtime_dir_size;
        uint64_t runtime_dir_inodes;
        uint64_t sessions_max;
        uint64_t inhibitors_max;

        char **efi_boot_loader_entries;
        bool efi_boot_loader_entries_set;

        char *efi_loader_entry_one_shot;
        struct stat efi_loader_entry_one_shot_stat;
};

void manager_reset_config(Manager *m);
int manager_parse_config_file(Manager *m);

int manager_add_device(Manager *m, const char *sysfs, bool master, Device **ret_device);
int manager_add_button(Manager *m, const char *name, Button **ret_button);
int manager_add_seat(Manager *m, const char *id, Seat **ret_seat);
int manager_add_session(Manager *m, const char *id, Session **ret_session);
int manager_add_user(Manager *m, UserRecord *ur, User **ret_user);
int manager_add_user_by_name(Manager *m, const char *name, User **ret_user);
int manager_add_user_by_uid(Manager *m, uid_t uid, User **ret_user);
int manager_add_inhibitor(Manager *m, const char* id, Inhibitor **ret_inhibitor);

int manager_process_seat_device(Manager *m, sd_device *d);
int manager_process_button_device(Manager *m, sd_device *d);

#if 0 /// elogind does not spawn VTs.
int manager_spawn_autovt(Manager *m, unsigned vtnr);
#endif // 0

bool manager_shall_kill(Manager *m, const char *user);

int manager_get_idle_hint(Manager *m, dual_timestamp *t);

int manager_get_user_by_pid(Manager *m, pid_t pid, User **user);
int manager_get_session_by_pid(Manager *m, pid_t pid, Session **session);

bool manager_is_lid_closed(Manager *m);
bool manager_is_docked_or_external_displays(Manager *m);
bool manager_is_on_external_power(void);
bool manager_all_buttons_ignored(Manager *m);

int manager_read_utmp(Manager *m);
void manager_connect_utmp(Manager *m);
void manager_reconnect_utmp(Manager *m);

/* gperf lookup function */
const struct ConfigPerfItem* logind_gperf_lookup(const char *key, GPERF_LEN_TYPE length);

int manager_set_lid_switch_ignore(Manager *m, usec_t until);

CONFIG_PARSER_PROTOTYPE(config_parse_n_autovts);
CONFIG_PARSER_PROTOTYPE(config_parse_tmpfs_size);

int manager_setup_wall_message_timer(Manager *m);
bool logind_wall_tty_filter(const char *tty, bool is_local, void *userdata);

int manager_read_efi_boot_loader_entries(Manager *m);
