/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

/***
  Copyright 2011 Lennart Poettering
***/

typedef struct Session Session;
typedef enum KillWho KillWho;

#include "list.h"
#include "login-util.h"
#include "logind-user.h"

typedef enum SessionState {
        SESSION_OPENING,  /* Session scope is being created */
        SESSION_ONLINE,   /* Logged in */
        SESSION_ACTIVE,   /* Logged in and in the fg */
        SESSION_CLOSING,  /* Logged out, but scope is still there */
        _SESSION_STATE_MAX,
        _SESSION_STATE_INVALID = -1
} SessionState;

typedef enum SessionClass {
        SESSION_USER,
        SESSION_GREETER,
        SESSION_LOCK_SCREEN,
        SESSION_BACKGROUND,
        _SESSION_CLASS_MAX,
        _SESSION_CLASS_INVALID = -1
} SessionClass;

typedef enum SessionType {
        SESSION_UNSPECIFIED,
        SESSION_TTY,
        SESSION_X11,
        SESSION_WAYLAND,
        SESSION_MIR,
        SESSION_WEB,
        _SESSION_TYPE_MAX,
        _SESSION_TYPE_INVALID = -1
} SessionType;

#define SESSION_TYPE_IS_GRAPHICAL(type) IN_SET(type, SESSION_X11, SESSION_WAYLAND, SESSION_MIR)

enum KillWho {
        KILL_LEADER,
        KILL_ALL,
        _KILL_WHO_MAX,
        _KILL_WHO_INVALID = -1
};

struct Session {
        Manager *manager;

        const char *id;
        unsigned int position;
        SessionType type;
        SessionClass class;

        char *state_file;

        User *user;

        dual_timestamp timestamp;

        char *tty;
        char *display;

        bool remote;
        char *remote_user;
        char *remote_host;
        char *service;
        char *desktop;

        char *scope;
#if 0 /// UNNEEDED by elogind
        char *scope_job;
#endif // 0

        Seat *seat;
        unsigned int vtnr;
        int vtfd;

        pid_t leader;
        uint32_t audit_id;

        int fifo_fd;
        char *fifo_path;

        sd_event_source *fifo_event_source;

        bool idle_hint;
        dual_timestamp idle_hint_timestamp;

        bool locked_hint;

        bool in_gc_queue:1;
        bool started:1;
        bool stopping:1;

        bool was_active:1;

        sd_bus_message *create_message;

        sd_event_source *timer_event_source;

        char *controller;
        Hashmap *devices;
        sd_bus_track *track;

        LIST_FIELDS(Session, sessions_by_user);
        LIST_FIELDS(Session, sessions_by_seat);

        LIST_FIELDS(Session, gc_queue);
};

Session *session_new(Manager *m, const char *id);
void session_free(Session *s);
void session_set_user(Session *s, User *u);
bool session_may_gc(Session *s, bool drop_not_started);
void session_add_to_gc_queue(Session *s);
int session_activate(Session *s);
bool session_is_active(Session *s);
int session_get_idle_hint(Session *s, dual_timestamp *t);
void session_set_idle_hint(Session *s, bool b);
int session_get_locked_hint(Session *s);
void session_set_locked_hint(Session *s, bool b);
int session_create_fifo(Session *s);
int session_start(Session *s, sd_bus_message *properties);
int session_stop(Session *s, bool force);
int session_finalize(Session *s);
int session_release(Session *s);
int session_save(Session *s);
int session_load(Session *s);
int session_kill(Session *s, KillWho who, int signo);

SessionState session_get_state(Session *u);

extern const sd_bus_vtable session_vtable[];
int session_node_enumerator(sd_bus *bus, const char *path,void *userdata, char ***nodes, sd_bus_error *error);
int session_object_find(sd_bus *bus, const char *path, const char *interface, void *userdata, void **found, sd_bus_error *error);
char *session_bus_path(Session *s);

int session_send_signal(Session *s, bool new_session);
int session_send_changed(Session *s, const char *properties, ...) _sentinel_;
int session_send_lock(Session *s, bool lock);
int session_send_lock_all(Manager *m, bool lock);

int session_send_create_reply(Session *s, sd_bus_error *error);

const char* session_state_to_string(SessionState t) _const_;
SessionState session_state_from_string(const char *s) _pure_;

const char* session_type_to_string(SessionType t) _const_;
SessionType session_type_from_string(const char *s) _pure_;

const char* session_class_to_string(SessionClass t) _const_;
SessionClass session_class_from_string(const char *s) _pure_;

const char *kill_who_to_string(KillWho k) _const_;
KillWho kill_who_from_string(const char *s) _pure_;

int session_prepare_vt(Session *s);
void session_restore_vt(Session *s);
void session_leave_vt(Session *s);

bool session_is_controller(Session *s, const char *sender);
int session_set_controller(Session *s, const char *sender, bool force, bool prepare);
void session_drop_controller(Session *s);

int bus_session_method_activate(sd_bus_message *message, void *userdata, sd_bus_error *error);
int bus_session_method_lock(sd_bus_message *message, void *userdata, sd_bus_error *error);
int bus_session_method_terminate(sd_bus_message *message, void *userdata, sd_bus_error *error);
int bus_session_method_kill(sd_bus_message *message, void *userdata, sd_bus_error *error);
