/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

typedef struct Session Session;
typedef enum KillWho KillWho;

#include "list.h"
#include "login-util.h"
#include "logind-user.h"
#include "string-util.h"

typedef enum SessionState {
        SESSION_OPENING,  /* Session scope is being created */
        SESSION_ONLINE,   /* Logged in */
        SESSION_ACTIVE,   /* Logged in and in the fg */
        SESSION_CLOSING,  /* Logged out, but scope is still there */
        _SESSION_STATE_MAX,
        _SESSION_STATE_INVALID = -EINVAL,
} SessionState;

typedef enum SessionClass {
        SESSION_USER,
        SESSION_GREETER,
        SESSION_LOCK_SCREEN,
        SESSION_BACKGROUND,
        _SESSION_CLASS_MAX,
        _SESSION_CLASS_INVALID = -EINVAL,
} SessionClass;

typedef enum SessionType {
        SESSION_UNSPECIFIED,
        SESSION_TTY,
        SESSION_X11,
        SESSION_WAYLAND,
        SESSION_MIR,
        SESSION_WEB,
        _SESSION_TYPE_MAX,
        _SESSION_TYPE_INVALID = -EINVAL,
} SessionType;

#define SESSION_TYPE_IS_GRAPHICAL(type) IN_SET(type, SESSION_X11, SESSION_WAYLAND, SESSION_MIR)

enum KillWho {
        KILL_LEADER,
        KILL_ALL,
        _KILL_WHO_MAX,
        _KILL_WHO_INVALID = -EINVAL,
};

typedef enum TTYValidity {
        TTY_FROM_PAM,
        TTY_FROM_UTMP,
        TTY_UTMP_INCONSISTENT, /* may happen on ssh sessions with multiplexed TTYs */
        _TTY_VALIDITY_MAX,
        _TTY_VALIDITY_INVALID = -EINVAL,
} TTYValidity;

struct Session {
        Manager *manager;

        const char *id;
        unsigned position;
        SessionType type;
        SessionType original_type;
        SessionClass class;

        char *state_file;

        User *user;

        dual_timestamp timestamp;

        char *display;
        char *tty;
        TTYValidity tty_validity;

        bool remote;
        char *remote_user;
        char *remote_host;
        char *service;
        char *desktop;

        char *scope;
#if 0 /// elogind does not support systemd scope jobsd
        char *scope_job;
#endif // 0

        Seat *seat;
        unsigned vtnr;
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

        /* Set up when a client requested to release the session via the bus */
        sd_event_source *timer_event_source;

        char *controller;
        Hashmap *devices;
        sd_bus_track *track;

        sd_event_source *stop_on_idle_event_source;

        LIST_FIELDS(Session, sessions_by_user);
        LIST_FIELDS(Session, sessions_by_seat);

        LIST_FIELDS(Session, gc_queue);
};

int session_new(Session **ret, Manager *m, const char *id);
Session* session_free(Session *s);

DEFINE_TRIVIAL_CLEANUP_FUNC(Session *, session_free);

void session_set_user(Session *s, User *u);
int session_set_leader(Session *s, pid_t pid);
bool session_may_gc(Session *s, bool drop_not_started);
void session_add_to_gc_queue(Session *s);
int session_activate(Session *s);
bool session_is_active(Session *s);
int session_get_idle_hint(Session *s, dual_timestamp *t);
int session_set_idle_hint(Session *s, bool b);
int session_get_locked_hint(Session *s);
void session_set_locked_hint(Session *s, bool b);
void session_set_type(Session *s, SessionType t);
int session_set_display(Session *s, const char *display);
int session_set_tty(Session *s, const char *tty);
int session_create_fifo(Session *s);
int session_start(Session *s, sd_bus_message *properties, sd_bus_error *error);
int session_stop(Session *s, bool force);
int session_finalize(Session *s);
int session_release(Session *s);
int session_save(Session *s);
int session_load(Session *s);
int session_kill(Session *s, KillWho who, int signo);

SessionState session_get_state(Session *u);

const char* session_state_to_string(SessionState t) _const_;
SessionState session_state_from_string(const char *s) _pure_;

const char* session_type_to_string(SessionType t) _const_;
SessionType session_type_from_string(const char *s) _pure_;

const char* session_class_to_string(SessionClass t) _const_;
SessionClass session_class_from_string(const char *s) _pure_;

const char *kill_who_to_string(KillWho k) _const_;
KillWho kill_who_from_string(const char *s) _pure_;

const char* tty_validity_to_string(TTYValidity t) _const_;
TTYValidity tty_validity_from_string(const char *s) _pure_;

void session_leave_vt(Session *s);

bool session_is_controller(Session *s, const char *sender);
int session_set_controller(Session *s, const char *sender, bool force, bool prepare);
void session_drop_controller(Session *s);

static inline bool SESSION_IS_SELF(const char *name) {
        return isempty(name) || streq(name, "self");
}

static inline bool SESSION_IS_AUTO(const char *name) {
        return streq_ptr(name, "auto");
}
