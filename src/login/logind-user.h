/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

/***
  This file is part of systemd.

  Copyright 2011 Lennart Poettering
***/

typedef struct User User;

#include "list.h"
#include "logind.h"

typedef enum UserState {
        USER_OFFLINE,    /* Not logged in at all */
        USER_OPENING,    /* Is logging in */
        USER_LINGERING,  /* Lingering has been enabled by the admin for this user */
        USER_ONLINE,     /* User logged in */
        USER_ACTIVE,     /* User logged in and has a session in the fg */
        USER_CLOSING,    /* User logged out, but processes still remain and lingering is not enabled */
        _USER_STATE_MAX,
        _USER_STATE_INVALID = -1
} UserState;

struct User {
        Manager *manager;
        uid_t uid;
        gid_t gid;
        char *name;
        char *state_file;
        char *runtime_path;
        char *slice;
        char *service;

#if 0 /// UNNEEDED by elogind
        char *service_job;
        char *slice_job;
#endif // 0

        Session *display;

        dual_timestamp timestamp;

        bool in_gc_queue:1;
        bool started:1;
        bool stopping:1;

        LIST_HEAD(Session, sessions);
        LIST_FIELDS(User, gc_queue);
};

int user_new(User **out, Manager *m, uid_t uid, gid_t gid, const char *name);
User *user_free(User *u);

DEFINE_TRIVIAL_CLEANUP_FUNC(User *, user_free);

bool user_may_gc(User *u, bool drop_not_started);
void user_add_to_gc_queue(User *u);
int user_start(User *u);
int user_stop(User *u, bool force);
int user_finalize(User *u);
UserState user_get_state(User *u);
int user_get_idle_hint(User *u, dual_timestamp *t);
int user_save(User *u);
int user_load(User *u);
int user_kill(User *u, int signo);
int user_check_linger_file(User *u);
void user_elect_display(User *u);

extern const sd_bus_vtable user_vtable[];
int user_node_enumerator(sd_bus *bus, const char *path, void *userdata, char ***nodes, sd_bus_error *error);
int user_object_find(sd_bus *bus, const char *path, const char *interface, void *userdata, void **found, sd_bus_error *error);
char *user_bus_path(User *s);

int user_send_signal(User *u, bool new_user);
int user_send_changed(User *u, const char *properties, ...) _sentinel_;

const char* user_state_to_string(UserState s) _const_;
UserState user_state_from_string(const char *s) _pure_;

int bus_user_method_terminate(sd_bus_message *message, void *userdata, sd_bus_error *error);
int bus_user_method_kill(sd_bus_message *message, void *userdata, sd_bus_error *error);
