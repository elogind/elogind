/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <stdbool.h>

//#include "bus-print-properties.h"
#include "bus-util.h"
//#include "image-policy.h"
//#include "install.h"
//#include "output-mode.h"
//#include "pager.h"

enum action {
        ACTION_SYSTEMCTL,
        ACTION_HALT,
        ACTION_POWEROFF,
        ACTION_REBOOT,
        ACTION_KEXEC,
#if 0 /// elogind can not soft-reboot, the system/service manager could
        ACTION_SOFT_REBOOT,
#endif // 0
        ACTION_EXIT,
        ACTION_SLEEP,
        ACTION_SUSPEND,
        ACTION_HIBERNATE,
        ACTION_HYBRID_SLEEP,
        ACTION_SUSPEND_THEN_HIBERNATE,
        ACTION_RUNLEVEL2,
        ACTION_RUNLEVEL3,
        ACTION_RUNLEVEL4,
        ACTION_RUNLEVEL5,
        ACTION_RESCUE,
        ACTION_EMERGENCY,
        ACTION_DEFAULT,
        ACTION_RELOAD,
        ACTION_REEXEC,
        ACTION_RUNLEVEL,
        ACTION_CANCEL_SHUTDOWN,
        ACTION_SHOW_SHUTDOWN,
        ACTION_SYSTEMCTL_SHOW_SHUTDOWN,
        _ACTION_MAX,
        _ACTION_INVALID = -EINVAL,
};

enum dependency {
        DEPENDENCY_FORWARD,
        DEPENDENCY_REVERSE,
        DEPENDENCY_AFTER,
        DEPENDENCY_BEFORE,
        _DEPENDENCY_MAX
};

#if 0 /// UNNEEDED by elogind - or already included in loginctl.c
extern char **arg_types;
extern char **arg_states;
extern char **arg_properties;
extern bool arg_all;
extern enum dependency arg_dependency;
extern const char *_arg_job_mode;
#endif // 0
extern RuntimeScope arg_runtime_scope;
#if 0 /// UNNEEDED by elogind - or already included in loginctl.c
extern bool arg_wait;
#endif // 0
extern bool arg_no_block;
#if 0 /// UNNEEDED by elogind - or already included in loginctl.c
extern int arg_legend;
extern PagerFlags arg_pager_flags;
extern bool arg_no_wtmp;
#endif // 0
extern bool arg_no_sync;
extern bool arg_no_wall;
#if 0 /// UNNEEDED by elogind - or already included in loginctl.c
extern bool arg_no_reload;
extern BusPrintPropertyFlags arg_print_flags;
extern bool arg_show_types;
#endif // 0
extern int arg_check_inhibitors;
extern bool arg_dry_run;
extern bool arg_quiet;
#if 0 /// UNNEEDED by elogind - or already included in loginctl.c
extern bool arg_no_warn;
extern bool arg_full;
extern bool arg_recursive;
extern bool arg_with_dependencies;
extern bool arg_show_transaction;
#endif // 0
extern int arg_force;
extern bool arg_ask_password;
#if 0 /// UNNEEDED by elogind - or already included in loginctl.c
extern bool arg_runtime;
#endif // 0
#if 0 /// UNNEEDED by elogind
extern UnitFilePresetMode arg_preset_mode;
#endif // 0
extern char **arg_wall;
#if 0 /// UNNEEDED by elogind - or already included in loginctl.c
extern const char *arg_kill_whom;
extern int arg_signal;
extern int arg_kill_value;
extern bool arg_kill_value_set;
extern char *arg_root;
#endif // 0
extern usec_t arg_when;
#if 0 /// UNNEEDED by elogind - or already included in loginctl.c
extern bool arg_stdin;
#endif // 0
extern const char *arg_reboot_argument;
extern enum action arg_action;
extern BusTransport arg_transport;
extern const char *arg_host;
#if 0 /// UNNEEDED by elogind - or already included in loginctl.c
extern unsigned arg_lines;
extern OutputMode arg_output;
extern bool arg_plain;
#endif // 0
extern bool arg_firmware_setup;
extern usec_t arg_boot_loader_menu;
extern const char *arg_boot_loader_entry;
#if 0 /// UNNEEDED by elogind - or already included in loginctl.c
extern bool arg_now;
extern bool arg_jobs_before;
extern bool arg_jobs_after;
extern char **arg_clean_what;
#endif // 0
extern TimestampStyle arg_timestamp_style;
#if 0 /// UNNEEDED by elogind - or already included in loginctl.c
extern bool arg_read_only;
extern bool arg_mkdir;
extern bool arg_marked;
extern const char *arg_drop_in;
#endif // 0
#if 0 /// UNNEEDED by elogind
extern ImagePolicy *arg_image_policy;
#endif // 0

#if 0 /// UNNEEDED by elogind
static inline const char* arg_job_mode(void) {
        return _arg_job_mode ?: "replace";
}

int systemctl_dispatch_parse_argv(int argc, char *argv[]);
#endif // 0
