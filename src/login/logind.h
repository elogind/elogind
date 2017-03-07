/*-*- Mode: C; c-basic-offset: 8; indent-tabs-mode: nil -*-*/

#pragma once

/***
  This file is part of systemd.

  Copyright 2011 Lennart Poettering

  systemd is free software; you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation; either version 2.1 of the License, or
  (at your option) any later version.

  systemd is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with systemd; If not, see <http://www.gnu.org/licenses/>.
***/

#include <stdbool.h>
#include <libudev.h>

#include "sd-event.h"
#include "sd-bus.h"
#include "list.h"
#include "hashmap.h"
#include "set.h"

typedef struct Manager Manager;

#include "logind-device.h"
#include "logind-inhibit.h"
#include "logind-button.h"
#include "logind-action.h"

struct Manager {
        sd_event *event;
        sd_bus *bus;

        Hashmap *devices;
        Hashmap *seats;
        Hashmap *sessions;
        Hashmap *users;
        Hashmap *inhibitors;
        Hashmap *buttons;

        Set *busnames;

        LIST_HEAD(Seat, seat_gc_queue);
        LIST_HEAD(Session, session_gc_queue);
        LIST_HEAD(User, user_gc_queue);

        struct udev *udev;
        struct udev_monitor *udev_seat_monitor, *udev_device_monitor, *udev_vcsa_monitor, *udev_button_monitor;

        sd_event_source *console_active_event_source;
        sd_event_source *udev_seat_event_source;
        sd_event_source *udev_device_event_source;
        sd_event_source *udev_vcsa_event_source;
        sd_event_source *udev_button_event_source;

        /* Make sure the user cannot accidentally unmount our cgroup
         * file system */
        int pin_cgroupfs_fd;

        /* Data specific to the cgroup subsystem */
        char *cgroup_root;

        int console_active_fd;

        Seat *seat0;

        char **kill_only_users, **kill_exclude_users;
        bool kill_user_processes;

        unsigned long session_counter;
        unsigned long inhibit_counter;

        usec_t inhibit_delay_max;

        /* If an action is currently being executed or is delayed,
         * this is != 0 and encodes what is being done */
        InhibitWhat action_what;

        /* If a shutdown/suspend was delayed due to a inhibitor this
           contains the action we are supposed to perform after the
           delay is over */
        HandleAction pending_action;
        usec_t action_timestamp;

        sd_event_source *idle_action_event_source;
        usec_t idle_action_usec;
        usec_t idle_action_not_before_usec;
        HandleAction idle_action;

        HandleAction handle_power_key;
        HandleAction handle_suspend_key;
        HandleAction handle_hibernate_key;
        HandleAction handle_lid_switch;
        HandleAction handle_lid_switch_docked;

        bool power_key_ignore_inhibited;
        bool suspend_key_ignore_inhibited;
        bool hibernate_key_ignore_inhibited;
        bool lid_switch_ignore_inhibited;

        bool remove_ipc;

        char **suspend_state, **suspend_mode;
        char **hibernate_state, **hibernate_mode;
        char **hybrid_sleep_state, **hybrid_sleep_mode;

        Hashmap *polkit_registry;

        usec_t holdoff_timeout_usec;
        sd_event_source *lid_switch_ignore_event_source;

        size_t runtime_dir_size;
};

Manager *manager_new(void);
void manager_free(Manager *m);

int manager_add_device(Manager *m, const char *sysfs, bool master, Device **_device);
int manager_add_button(Manager *m, const char *name, Button **_button);
int manager_add_seat(Manager *m, const char *id, Seat **_seat);
int manager_add_session(Manager *m, const char *id, Session **_session);
int manager_add_user(Manager *m, uid_t uid, gid_t gid, const char *name, User **_user);
int manager_add_user_by_name(Manager *m, const char *name, User **_user);
int manager_add_user_by_uid(Manager *m, uid_t uid, User **_user);
int manager_add_inhibitor(Manager *m, const char* id, Inhibitor **_inhibitor);

int manager_process_seat_device(Manager *m, struct udev_device *d);
int manager_process_button_device(Manager *m, struct udev_device *d);

int manager_startup(Manager *m);
int manager_run(Manager *m);

void manager_gc(Manager *m, bool drop_not_started);

bool manager_shall_kill(Manager *m, const char *user);

int manager_get_idle_hint(Manager *m, dual_timestamp *t);

int manager_get_user_by_pid(Manager *m, pid_t pid, User **user);
int manager_get_session_by_pid(Manager *m, pid_t pid, Session **session);

bool manager_is_docked(Manager *m);
int manager_count_displays(Manager *m);
bool manager_is_docked_or_multiple_displays(Manager *m);

extern const sd_bus_vtable manager_vtable[];

int bus_manager_shutdown_or_sleep_now_or_later(Manager *m, HandleAction action, InhibitWhat w, sd_bus_error *error);
int shutdown_or_sleep(Manager *m, HandleAction action);

int manager_send_changed(Manager *manager, const char *property, ...) _sentinel_;

int manager_dispatch_delayed(Manager *manager);

/* gperf lookup function */
const struct ConfigPerfItem* logind_gperf_lookup(const char *key, size_t length);

int manager_watch_busname(Manager *manager, const char *name);
void manager_drop_busname(Manager *manager, const char *name);

int manager_set_lid_switch_ignore(Manager *m, usec_t until);

int config_parse_tmpfs_size(const char *unit, const char *filename, unsigned line, const char *section, unsigned section_line, const char *lvalue, int ltype, const char *rvalue, void *data, void *userdata);

int manager_get_session_from_creds(Manager *m, sd_bus_message *message, const char *name, sd_bus_error *error, Session **ret);
int manager_get_user_from_creds(Manager *m, sd_bus_message *message, uid_t uid, sd_bus_error *error, User **ret);
int manager_get_seat_from_creds(Manager *m, sd_bus_message *message, const char *name, sd_bus_error *error, Seat **ret);
