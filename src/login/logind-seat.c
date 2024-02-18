/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <errno.h>
#include <fcntl.h>
//#include <sys/stat.h>
#include <unistd.h>

#include "sd-messages.h"

#include "alloc-util.h"
#include "devnode-acl.h"
#include "errno-util.h"
#include "fd-util.h"
#include "fileio.h"
#include "format-util.h"
#include "logind-seat-dbus.h"
#include "logind-seat.h"
#include "logind-session-dbus.h"
#include "mkdir-label.h"
#include "parse-util.h"
#include "path-util.h"
#include "stdio-util.h"
#include "string-util.h"
#include "terminal-util.h"
#include "tmpfile-util.h"
#include "util.h"

int seat_new(Seat** ret, Manager *m, const char *id) {
        _cleanup_(seat_freep) Seat *s = NULL;
        int r;

        assert(ret);
        assert(m);
        assert(id);

        if (!seat_name_is_valid(id))
                return -EINVAL;

        s = new(Seat, 1);
        if (!s)
                return -ENOMEM;

        *s = (Seat) {
                .manager = m,
        };

        s->state_file = path_join("/run/systemd/seats", id);
        if (!s->state_file)
                return -ENOMEM;

        s->id = basename(s->state_file);

        r = hashmap_put(m->seats, s->id, s);
        if (r < 0)
                return r;

        *ret = TAKE_PTR(s);
        return 0;
}

Seat* seat_free(Seat *s) {
        if (!s)
                return NULL;

        log_debug_elogind("Freeing Seat %s ...", s->id);
        if (s->in_gc_queue)
                LIST_REMOVE(gc_queue, s->manager->seat_gc_queue, s);

        while (s->sessions)
                session_free(s->sessions);

        assert(!s->active);

        while (s->devices)
                device_free(s->devices);

        hashmap_remove(s->manager->seats, s->id);

        free(s->positions);
        free(s->state_file);

        return mfree(s);
}

int seat_save(Seat *s) {
        _cleanup_free_ char *temp_path = NULL;
        _cleanup_fclose_ FILE *f = NULL;
        int r;

        assert(s);

        if (!s->started)
                return 0;

        r = mkdir_safe_label("/run/systemd/seats", 0755, 0, 0, MKDIR_WARN_MODE);
        if (r < 0)
                goto fail;

        r = fopen_temporary(s->state_file, &f, &temp_path);
        if (r < 0)
                goto fail;

        (void) fchmod(fileno(f), 0644);

        fprintf(f,
                "# This is private data. Do not parse.\n"
                "IS_SEAT0=%i\n"
                "CAN_MULTI_SESSION=1\n"
                "CAN_TTY=%i\n"
                "CAN_GRAPHICAL=%i\n",
                seat_is_seat0(s),
                seat_can_tty(s),
                seat_can_graphical(s));

        if (s->active) {
                assert(s->active->user);

                fprintf(f,
                        "ACTIVE=%s\n"
                        "ACTIVE_UID="UID_FMT"\n",
                        s->active->id,
                        s->active->user->user_record->uid);
        }

        if (s->sessions) {
                fputs("SESSIONS=", f);
                LIST_FOREACH(sessions_by_seat, i, s->sessions) {
                        fprintf(f,
                                "%s%c",
                                i->id,
                                i->sessions_by_seat_next ? ' ' : '\n');
                }

                fputs("UIDS=", f);
                LIST_FOREACH(sessions_by_seat, i, s->sessions)
                        fprintf(f,
                                UID_FMT"%c",
                                i->user->user_record->uid,
                                i->sessions_by_seat_next ? ' ' : '\n');
        }

        r = fflush_and_check(f);
        if (r < 0)
                goto fail;

        if (rename(temp_path, s->state_file) < 0) {
                r = -errno;
                goto fail;
        }

        return 0;

fail:
        (void) unlink(s->state_file);

        if (temp_path)
                (void) unlink(temp_path);

        return log_error_errno(r, "Failed to save seat data %s: %m", s->state_file);
}

int seat_load(Seat *s) {
        assert(s);

        /* There isn't actually anything to read here ... */

        return 0;
}

#if 0 /// UNNEEDED by elogind
static int vt_allocate(unsigned vtnr) {
        char p[sizeof("/dev/tty") + DECIMAL_STR_MAX(unsigned)];
        _cleanup_close_ int fd = -1;

        assert(vtnr >= 1);

        xsprintf(p, "/dev/tty%u", vtnr);
        fd = open_terminal(p, O_RDWR|O_NOCTTY|O_CLOEXEC);
        if (fd < 0)
                return fd;

        return 0;
}

int seat_preallocate_vts(Seat *s) {
        int r = 0;
        unsigned i;

        assert(s);
        assert(s->manager);

        if (s->manager->n_autovts <= 0)
                return 0;

        if (!seat_has_vts(s))
                return 0;

        log_debug("Preallocating VTs...");

        for (i = 1; i <= s->manager->n_autovts; i++) {
                int q;

                q = vt_allocate(i);
                if (q < 0)
                        r = log_error_errno(q, "Failed to preallocate VT %u: %m", i);
        }

        return r;
}
#endif // 0

int seat_apply_acls(Seat *s, Session *old_active) {
        int r;

        assert(s);

        r = devnode_acl_all(s->id,
                            false,
                            !!old_active, old_active ? old_active->user->user_record->uid : 0,
                            !!s->active, s->active ? s->active->user->user_record->uid : 0);

        if (r < 0)
                return log_error_errno(r, "Failed to apply ACLs: %m");

        return 0;
}

int seat_set_active(Seat *s, Session *session) {
        Session *old_active;

        assert(s);
        assert(!session || session->seat == s);

        /* When logind receives the SIGRTMIN signal from the kernel, it will
         * execute session_leave_vt and stop all devices of the session; at
         * this time, if the session is active and there is no change in the
         * session, then the session does not have the permissions of the device,
         * and the machine will have a black screen and suspended animation.
         * Therefore, if the active session has executed session_leave_vt ,
         * A resume is required here. */
        if (session == s->active) {
                if (session) {
                        log_debug("Active session remains unchanged, resuming session devices.");
                        session_device_resume_all(session);
                }
                return 0;
        }

        old_active = s->active;
        s->active = session;

        if (old_active) {
                session_device_pause_all(old_active);
                session_send_changed(old_active, "Active", NULL);
        }

        (void) seat_apply_acls(s, old_active);

        if (session && session->started) {
                session_send_changed(session, "Active", NULL);
                session_device_resume_all(session);
        }

        if (!session || session->started)
                seat_send_changed(s, "ActiveSession", NULL);

        seat_save(s);

        if (session) {
                session_save(session);
                user_save(session->user);
        }

        if (old_active) {
                session_save(old_active);
                if (!session || session->user != old_active->user)
                        user_save(old_active->user);
        }

        return 0;
}

static Session* seat_get_position(Seat *s, unsigned pos) {
        assert(s);

        if (pos >= MALLOC_ELEMENTSOF(s->positions))
                return NULL;

        return s->positions[pos];
}

int seat_switch_to(Seat *s, unsigned num) {
        Session *session;

        /* Public session positions skip 0 (there is only F1-F12). Maybe it
         * will get reassigned in the future, so return error for now. */
        if (num == 0)
                return -EINVAL;

        session = seat_get_position(s, num);
        if (!session) {
                /* allow switching to unused VTs to trigger auto-activate */
                if (seat_has_vts(s) && num < 64)
                        return chvt(num);

                return -EINVAL;
        }

        return session_activate(session);
}

int seat_switch_to_next(Seat *s) {
        unsigned start, i;
        Session *session;

        if (MALLOC_ELEMENTSOF(s->positions) == 0)
                return -EINVAL;

        start = 1;
        if (s->active && s->active->position > 0)
                start = s->active->position;

        for (i = start + 1; i < MALLOC_ELEMENTSOF(s->positions); ++i) {
                session = seat_get_position(s, i);
                if (session)
                        return session_activate(session);
        }

        for (i = 1; i < start; ++i) {
                session = seat_get_position(s, i);
                if (session)
                        return session_activate(session);
        }

        return -EINVAL;
}

int seat_switch_to_previous(Seat *s) {
        if (MALLOC_ELEMENTSOF(s->positions) == 0)
                return -EINVAL;

        size_t start = s->active && s->active->position > 0 ? s->active->position : 1;

        for (size_t i = start - 1; i > 0; i--) {
                Session *session = seat_get_position(s, i);
                if (session)
                        return session_activate(session);
        }

        for (size_t i = MALLOC_ELEMENTSOF(s->positions) - 1; i > start; i--) {
                Session *session = seat_get_position(s, i);
                if (session)
                        return session_activate(session);
        }

        return -EINVAL;
}

int seat_active_vt_changed(Seat *s, unsigned vtnr) {
        Session *new_active = NULL;
        int r;

        assert(s);
        assert(vtnr >= 1);

        if (!seat_has_vts(s))
                return -EINVAL;

        log_debug("VT changed to %u", vtnr);

        /* we might have earlier closing sessions on the same VT, so try to
         * find a running one first */
        LIST_FOREACH(sessions_by_seat, i, s->sessions)
                if (i->vtnr == vtnr && !i->stopping) {
                        new_active = i;
                        break;
                }

        if (!new_active)
                /* no running one? then we can't decide which one is the
                 * active one, let the first one win */
                LIST_FOREACH(sessions_by_seat, i, s->sessions)
                        if (i->vtnr == vtnr) {
                                new_active = i;
                                break;
                        }

        r = seat_set_active(s, new_active);
#if 0 /// elogind does not spawn autovt
        manager_spawn_autovt(s->manager, vtnr);
#endif // 0

        return r;
}

int seat_read_active_vt(Seat *s) {
        char t[64];
        ssize_t k;
        int vtnr;

        assert(s);

        if (!seat_has_vts(s))
                return 0;

        if (lseek(s->manager->console_active_fd, SEEK_SET, 0) < 0)
                return log_error_errno(errno, "lseek on console_active_fd failed: %m");

        errno = 0;
        k = read(s->manager->console_active_fd, t, sizeof(t)-1);
        if (k <= 0)
                return log_error_errno(errno ?: EIO,
                                       "Failed to read current console: %s", STRERROR_OR_EOF(errno));

        t[k] = 0;
        truncate_nl(t);

        vtnr = vtnr_from_tty(t);
        if (vtnr < 0) {
                log_error_errno(vtnr, "Hm, /sys/class/tty/tty0/active is badly formatted: %m");
                return -EIO;
        }

        return seat_active_vt_changed(s, vtnr);
}

int seat_start(Seat *s) {
        assert(s);

        if (s->started)
                return 0;

        log_struct(LOG_INFO,
                   "MESSAGE_ID=" SD_MESSAGE_SEAT_START_STR,
                   "SEAT_ID=%s", s->id,
                   LOG_MESSAGE("New seat %s.", s->id));

        /* Initialize VT magic stuff */
#if 0 /// elogind does not support autospawning vts
        seat_preallocate_vts(s);
#endif // 0

        /* Read current VT */
        seat_read_active_vt(s);

        s->started = true;

        /* Save seat data */
        seat_save(s);

        seat_send_signal(s, true);

        return 0;
}

int seat_stop(Seat *s, bool force) {
        int r;

        assert(s);

        if (s->started)
                log_struct(LOG_INFO,
                           "MESSAGE_ID=" SD_MESSAGE_SEAT_STOP_STR,
                           "SEAT_ID=%s", s->id,
                           LOG_MESSAGE("Removed seat %s.", s->id));

        r = seat_stop_sessions(s, force);

        (void) unlink(s->state_file);
        seat_add_to_gc_queue(s);

        if (s->started)
                seat_send_signal(s, false);

        s->started = false;

        return r;
}

int seat_stop_sessions(Seat *s, bool force) {
        int r = 0, k;

        assert(s);

        log_debug_elogind("Stopping all sessions of seat %s %s",
                          s->id, force ? "(forced)" : "");
        LIST_FOREACH(sessions_by_seat, session, s->sessions) {
                k = session_stop(session, force);
                if (k < 0)
                        r = k;
        }

        return r;
}

void seat_evict_position(Seat *s, Session *session) {
        unsigned pos = session->position;

        session->position = 0;

        if (pos == 0)
                return;

        if (pos < MALLOC_ELEMENTSOF(s->positions) && s->positions[pos] == session) {
                s->positions[pos] = NULL;

                /* There might be another session claiming the same
                 * position (eg., during gdm->session transition), so let's look
                 * for it and set it on the free slot. */
                LIST_FOREACH(sessions_by_seat, iter, s->sessions)
                        if (iter->position == pos && session_get_state(iter) != SESSION_CLOSING) {
                                s->positions[pos] = iter;
                                break;
                        }
        }
}

void seat_claim_position(Seat *s, Session *session, unsigned pos) {
        /* with VTs, the position is always the same as the VTnr */
        if (seat_has_vts(s))
                pos = session->vtnr;

        if (!GREEDY_REALLOC0(s->positions, pos + 1))
                return;

        seat_evict_position(s, session);

        session->position = pos;
        if (pos > 0)
                s->positions[pos] = session;
}

static void seat_assign_position(Seat *s, Session *session) {
        unsigned pos;

        if (session->position > 0)
                return;

        for (pos = 1; pos < MALLOC_ELEMENTSOF(s->positions); ++pos)
                if (!s->positions[pos])
                        break;

        seat_claim_position(s, session, pos);
}

int seat_attach_session(Seat *s, Session *session) {
        assert(s);
        assert(session);
        assert(!session->seat);

        if (!seat_has_vts(s) != !session->vtnr)
                return -EINVAL;

        session->seat = s;
        LIST_PREPEND(sessions_by_seat, s->sessions, session);
        seat_assign_position(s, session);

        /* On seats with VTs, the VT logic defines which session is active. On
         * seats without VTs, we automatically activate new sessions. */
        if (!seat_has_vts(s))
                seat_set_active(s, session);

        return 0;
}

void seat_complete_switch(Seat *s) {
        Session *session;

        assert(s);

        /* if no session-switch is pending or if it got canceled, do nothing */
        if (!s->pending_switch)
                return;

        session = TAKE_PTR(s->pending_switch);

        seat_set_active(s, session);
}

bool seat_has_vts(Seat *s) {
        assert(s);

        return seat_is_seat0(s) && s->manager->console_active_fd >= 0;
}

bool seat_is_seat0(Seat *s) {
        assert(s);

        return s->manager->seat0 == s;
}

bool seat_can_tty(Seat *s) {
        assert(s);

        return seat_has_vts(s);
}

bool seat_has_master_device(Seat *s) {
        assert(s);

        /* device list is ordered by "master" flag */
        return !!s->devices && s->devices->master;
}

bool seat_can_graphical(Seat *s) {
        assert(s);

        return seat_has_master_device(s);
}

int seat_get_idle_hint(Seat *s, dual_timestamp *t) {
        bool idle_hint = true;
        dual_timestamp ts = DUAL_TIMESTAMP_NULL;

        assert(s);

        LIST_FOREACH(sessions_by_seat, session, s->sessions) {
                dual_timestamp k;
                int ih;

                ih = session_get_idle_hint(session, &k);
                if (ih < 0)
                        return ih;

                if (!ih) {
                        if (!idle_hint) {
                                if (k.monotonic > ts.monotonic)
                                        ts = k;
                        } else {
                                idle_hint = false;
                                ts = k;
                        }
                } else if (idle_hint) {

                        if (k.monotonic > ts.monotonic)
                                ts = k;
                }
        }

        if (t)
                *t = ts;

        return idle_hint;
}

bool seat_may_gc(Seat *s, bool drop_not_started) {
        assert(s);

        log_debug_elogind("Seat %s may gc ?", s->id);
        log_debug_elogind("  dns && !started: %s", yes_no(drop_not_started && !s->started));
        log_debug_elogind("  is not seat0   : %s", yes_no(!seat_is_seat0(s)));
        log_debug_elogind("  no master dev  : %s", yes_no(!seat_has_master_device(s)));
        if (drop_not_started && !s->started)
                return true;

        if (seat_is_seat0(s))
                return false;

        return !seat_has_master_device(s);
}

void seat_add_to_gc_queue(Seat *s) {
        assert(s);

        if (s->in_gc_queue)
                return;

        LIST_PREPEND(gc_queue, s->manager->seat_gc_queue, s);
        s->in_gc_queue = true;
}

static bool seat_name_valid_char(char c) {
        return
                ascii_isalpha(c) ||
                ascii_isdigit(c) ||
                IN_SET(c, '-', '_');
}

bool seat_name_is_valid(const char *name) {
        const char *p;

        assert(name);

        if (!startswith(name, "seat"))
                return false;

        if (!name[4])
                return false;

        for (p = name; *p; p++)
                if (!seat_name_valid_char(*p))
                        return false;

        if (strlen(name) > 255)
                return false;

        return true;
}
