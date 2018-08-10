/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

typedef struct Inhibitor Inhibitor;

typedef enum InhibitWhat {
        INHIBIT_SHUTDOWN = 1,
        INHIBIT_SLEEP = 2,
        INHIBIT_IDLE = 4,
        INHIBIT_HANDLE_POWER_KEY = 8,
        INHIBIT_HANDLE_SUSPEND_KEY = 16,
        INHIBIT_HANDLE_HIBERNATE_KEY = 32,
        INHIBIT_HANDLE_LID_SWITCH = 64,
        _INHIBIT_WHAT_MAX = 128,
        _INHIBIT_WHAT_INVALID = -1
} InhibitWhat;

typedef enum InhibitMode {
        INHIBIT_BLOCK,
        INHIBIT_DELAY,
        _INHIBIT_MODE_MAX,
        _INHIBIT_MODE_INVALID = -1
} InhibitMode;

#include "logind.h"

struct Inhibitor {
        Manager *manager;

        sd_event_source *event_source;

        char *id;
        char *state_file;

        bool started;

        InhibitWhat what;
        char *who;
        char *why;
        InhibitMode mode;

        pid_t pid;
        uid_t uid;

        dual_timestamp since;

        char *fifo_path;
        int fifo_fd;
};

Inhibitor* inhibitor_new(Manager *m, const char *id);
void inhibitor_free(Inhibitor *i);

int inhibitor_save(Inhibitor *i);
int inhibitor_load(Inhibitor *i);

int inhibitor_start(Inhibitor *i);
int inhibitor_stop(Inhibitor *i);

int inhibitor_create_fifo(Inhibitor *i);
void inhibitor_remove_fifo(Inhibitor *i);

InhibitWhat manager_inhibit_what(Manager *m, InhibitMode mm);
bool manager_is_inhibited(Manager *m, InhibitWhat w, InhibitMode mm, dual_timestamp *since, bool ignore_inactive, bool ignore_uid, uid_t uid, Inhibitor **offending);

const char *inhibit_what_to_string(InhibitWhat k);
InhibitWhat inhibit_what_from_string(const char *s);

const char *inhibit_mode_to_string(InhibitMode k);
InhibitMode inhibit_mode_from_string(const char *s);
