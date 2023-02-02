/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <errno.h>
#include <fcntl.h>
#include <linux/kd.h>
#include <linux/vt.h>
#include <signal.h>
#include <sys/ioctl.h>
//#include <sys/stat.h>
#include <unistd.h>

#include "sd-messages.h"

#include "alloc-util.h"
#include "audit-util.h"
#include "bus-error.h"
#include "bus-util.h"
#include "devnum-util.h"
#include "env-file.h"
#include "escape.h"
#include "fd-util.h"
#include "fileio.h"
#include "format-util.h"
#include "io-util.h"
#include "logind-dbus.h"
#include "logind-seat-dbus.h"
#include "logind-session-dbus.h"
#include "logind-session.h"
#include "logind-user-dbus.h"
#include "mkdir-label.h"
#include "parse-util.h"
#include "path-util.h"
#include "process-util.h"
#include "serialize.h"
#include "string-table.h"
#include "strv.h"
#include "terminal-util.h"
#include "tmpfile-util.h"
#include "uid-alloc-range.h"
#include "user-util.h"
//#include "util.h"
/// Additional includes needed by elogind
#include "cgroup-setup.h"
#include "extract-word.h"

#define RELEASE_USEC (20*USEC_PER_SEC)

static void session_remove_fifo(Session *s);
static void session_restore_vt(Session *s);

int session_new(Session **ret, Manager *m, const char *id) {
        _cleanup_(session_freep) Session *s = NULL;
        int r;

        assert(ret);
        assert(m);
        assert(id);

        if (!session_id_valid(id))
                return -EINVAL;

        s = new(Session, 1);
        if (!s)
                return -ENOMEM;

        *s = (Session) {
                .manager = m,
                .fifo_fd = -1,
                .vtfd = -1,
                .audit_id = AUDIT_SESSION_INVALID,
                .tty_validity = _TTY_VALIDITY_INVALID,
        };

        s->state_file = path_join("/run/systemd/sessions", id);
        if (!s->state_file)
                return -ENOMEM;

        s->id = basename(s->state_file);

        s->devices = hashmap_new(&devt_hash_ops);
        if (!s->devices)
                return -ENOMEM;

        r = hashmap_put(m->sessions, s->id, s);
        if (r < 0)
                return r;

        *ret = TAKE_PTR(s);
        return 0;
}

Session* session_free(Session *s) {
        SessionDevice *sd;

        log_debug_elogind("Freeing Session %s ...", s->id);
        if (!s)
                return NULL;

        if (s->in_gc_queue)
                LIST_REMOVE(gc_queue, s->manager->session_gc_queue, s);

        s->timer_event_source = sd_event_source_unref(s->timer_event_source);

        session_drop_controller(s);

        while ((sd = hashmap_first(s->devices)))
                session_device_free(sd);

        hashmap_free(s->devices);

        if (s->user) {
                LIST_REMOVE(sessions_by_user, s->user->sessions, s);

                if (s->user->display == s)
                        s->user->display = NULL;

                user_update_last_session_timer(s->user);
        }

        if (s->seat) {
                if (s->seat->active == s)
                        s->seat->active = NULL;
                if (s->seat->pending_switch == s)
                        s->seat->pending_switch = NULL;

                seat_evict_position(s->seat, s);
                LIST_REMOVE(sessions_by_seat, s->seat->sessions, s);
        }

#if 0 /// elogind does not support systemd units and scope_jobs
        if (s->scope) {
                hashmap_remove(s->manager->session_units, s->scope);
                free(s->scope);
        }
#endif // 0

        if (pid_is_valid(s->leader))
                (void) hashmap_remove_value(s->manager->sessions_by_leader, PID_TO_PTR(s->leader), s);

#if 0 /// elogind does not support systemd scope_jobs
        free(s->scope_job);
#endif // 0

        sd_bus_message_unref(s->create_message);

        free(s->tty);
        free(s->display);
        free(s->remote_host);
        free(s->remote_user);
        free(s->service);
        free(s->desktop);

        hashmap_remove(s->manager->sessions, s->id);

        sd_event_source_unref(s->fifo_event_source);
        safe_close(s->fifo_fd);

        /* Note that we remove neither the state file nor the fifo path here, since we want both to survive
         * daemon restarts */
        free(s->state_file);
        free(s->fifo_path);

        sd_event_source_unref(s->stop_on_idle_event_source);

        return mfree(s);
}

void session_set_user(Session *s, User *u) {
        assert(s);
        assert(!s->user);

        s->user = u;
        LIST_PREPEND(sessions_by_user, u->sessions, s);

        user_update_last_session_timer(u);
}

int session_set_leader(Session *s, pid_t pid) {
        int r;

        assert(s);

        if (!pid_is_valid(pid))
                return -EINVAL;

        if (s->leader == pid)
                return 0;

        r = hashmap_put(s->manager->sessions_by_leader, PID_TO_PTR(pid), s);
        if (r < 0)
                return r;

        if (pid_is_valid(s->leader))
                (void) hashmap_remove_value(s->manager->sessions_by_leader, PID_TO_PTR(s->leader), s);

        s->leader = pid;
        (void) audit_session_from_pid(pid, &s->audit_id);

        return 1;
}

static void session_save_devices(Session *s, FILE *f) {
        SessionDevice *sd;

        if (!hashmap_isempty(s->devices)) {
                fprintf(f, "DEVICES=");
                HASHMAP_FOREACH(sd, s->devices)
                        fprintf(f, "%u:%u ", major(sd->dev), minor(sd->dev));
                fprintf(f, "\n");
        }
}

int session_save(Session *s) {
        _cleanup_free_ char *temp_path = NULL;
        _cleanup_fclose_ FILE *f = NULL;
        int r;

        assert(s);

        if (!s->user)
                return -ESTALE;

        if (!s->started)
                return 0;

        r = mkdir_safe_label("/run/systemd/sessions", 0755, 0, 0, MKDIR_WARN_MODE);
        if (r < 0)
                goto fail;

        r = fopen_temporary(s->state_file, &f, &temp_path);
        if (r < 0)
                goto fail;

        (void) fchmod(fileno(f), 0644);

        fprintf(f,
                "# This is private data. Do not parse.\n"
                "UID="UID_FMT"\n"
                "USER=%s\n"
                "ACTIVE=%i\n"
                "IS_DISPLAY=%i\n"
                "STATE=%s\n"
                "REMOTE=%i\n",
                s->user->user_record->uid,
                s->user->user_record->user_name,
                session_is_active(s),
                s->user->display == s,
                session_state_to_string(session_get_state(s)),
                s->remote);

        if (s->type >= 0)
                fprintf(f, "TYPE=%s\n", session_type_to_string(s->type));

        if (s->original_type >= 0)
                fprintf(f, "ORIGINAL_TYPE=%s\n", session_type_to_string(s->original_type));

        if (s->class >= 0)
                fprintf(f, "CLASS=%s\n", session_class_to_string(s->class));

        if (s->scope)
                fprintf(f, "SCOPE=%s\n", s->scope);
#if 0 /// elogind does not support systemd scope_jobs
        if (s->scope_job)
                fprintf(f, "SCOPE_JOB=%s\n", s->scope_job);
#endif // 0

        if (s->fifo_path)
                fprintf(f, "FIFO=%s\n", s->fifo_path);

        if (s->seat)
                fprintf(f, "SEAT=%s\n", s->seat->id);

        if (s->tty)
                fprintf(f, "TTY=%s\n", s->tty);

        if (s->tty_validity >= 0)
                fprintf(f, "TTY_VALIDITY=%s\n", tty_validity_to_string(s->tty_validity));

        if (s->display)
                fprintf(f, "DISPLAY=%s\n", s->display);

        if (s->remote_host) {
                _cleanup_free_ char *escaped = NULL;

                escaped = cescape(s->remote_host);
                if (!escaped) {
                        r = -ENOMEM;
                        goto fail;
                }

                fprintf(f, "REMOTE_HOST=%s\n", escaped);
        }

        if (s->remote_user) {
                _cleanup_free_ char *escaped = NULL;

                escaped = cescape(s->remote_user);
                if (!escaped) {
                        r = -ENOMEM;
                        goto fail;
                }

                fprintf(f, "REMOTE_USER=%s\n", escaped);
        }

        if (s->service) {
                _cleanup_free_ char *escaped = NULL;

                escaped = cescape(s->service);
                if (!escaped) {
                        r = -ENOMEM;
                        goto fail;
                }

                fprintf(f, "SERVICE=%s\n", escaped);
        }

        if (s->desktop) {
                _cleanup_free_ char *escaped = NULL;

                escaped = cescape(s->desktop);
                if (!escaped) {
                        r = -ENOMEM;
                        goto fail;
                }

                fprintf(f, "DESKTOP=%s\n", escaped);
        }

        if (s->seat && seat_has_vts(s->seat))
                fprintf(f, "VTNR=%u\n", s->vtnr);

        if (!s->vtnr)
                fprintf(f, "POSITION=%u\n", s->position);

        if (pid_is_valid(s->leader))
                fprintf(f, "LEADER="PID_FMT"\n", s->leader);

        if (audit_session_is_valid(s->audit_id))
                fprintf(f, "AUDIT=%"PRIu32"\n", s->audit_id);

        if (dual_timestamp_is_set(&s->timestamp))
                fprintf(f,
                        "REALTIME="USEC_FMT"\n"
                        "MONOTONIC="USEC_FMT"\n",
                        s->timestamp.realtime,
                        s->timestamp.monotonic);

        if (s->controller) {
                fprintf(f, "CONTROLLER=%s\n", s->controller);
                session_save_devices(s, f);
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

        return log_error_errno(r, "Failed to save session data %s: %m", s->state_file);
}

static int session_load_devices(Session *s, const char *devices) {
        int r = 0;

        assert(s);

        for (const char *p = devices;;) {
                _cleanup_free_ char *word = NULL;
                SessionDevice *sd;
                dev_t dev;
                int k;

                k = extract_first_word(&p, &word, NULL, 0);
                if (k == 0)
                        break;
                if (k < 0) {
                        r = k;
                        break;
                }

                k = parse_devnum(word, &dev);
                if (k < 0) {
                        r = k;
                        continue;
                }

                /* The file descriptors for loaded devices will be reattached later. */
                k = session_device_new(s, dev, false, &sd);
                if (k < 0)
                        r = k;
        }

        if (r < 0)
                log_error_errno(r, "Loading session devices for session %s failed: %m", s->id);

        return r;
}

int session_load(Session *s) {
        _cleanup_free_ char *remote = NULL,
                *seat = NULL,
                *tty_validity = NULL,
                *vtnr = NULL,
                *state = NULL,
                *position = NULL,
                *leader = NULL,
                *type = NULL,
                *original_type = NULL,
                *class = NULL,
                *uid = NULL,
                *realtime = NULL,
                *monotonic = NULL,
                *controller = NULL,
                *active = NULL,
                *devices = NULL,
                *is_display = NULL;

        int k, r;

        assert(s);

        r = parse_env_file(NULL, s->state_file,
                           "REMOTE",         &remote,
                           "SCOPE",          &s->scope,
#if 0 /// elogind does not support systemd scope_jobs
                           "SCOPE_JOB",      &s->scope_job,
#endif // 0
                           "FIFO",           &s->fifo_path,
                           "SEAT",           &seat,
                           "TTY",            &s->tty,
                           "TTY_VALIDITY",   &tty_validity,
                           "DISPLAY",        &s->display,
                           "REMOTE_HOST",    &s->remote_host,
                           "REMOTE_USER",    &s->remote_user,
                           "SERVICE",        &s->service,
                           "DESKTOP",        &s->desktop,
                           "VTNR",           &vtnr,
                           "STATE",          &state,
                           "POSITION",       &position,
                           "LEADER",         &leader,
                           "TYPE",           &type,
                           "ORIGINAL_TYPE",  &original_type,
                           "CLASS",          &class,
                           "UID",            &uid,
                           "REALTIME",       &realtime,
                           "MONOTONIC",      &monotonic,
                           "CONTROLLER",     &controller,
                           "ACTIVE",         &active,
                           "DEVICES",        &devices,
                           "IS_DISPLAY",     &is_display);
        if (r < 0)
                return log_error_errno(r, "Failed to read %s: %m", s->state_file);

        if (!s->user) {
                uid_t u;
                User *user;

                if (!uid)
                        return log_error_errno(SYNTHETIC_ERRNO(ENOENT),
                                               "UID not specified for session %s",
                                               s->id);

                r = parse_uid(uid, &u);
                if (r < 0)  {
                        log_error("Failed to parse UID value %s for session %s.", uid, s->id);
                        return r;
                }

                user = hashmap_get(s->manager->users, UID_TO_PTR(u));
                if (!user)
                        return log_error_errno(SYNTHETIC_ERRNO(ENOENT),
                                               "User of session %s not known.",
                                               s->id);

                log_debug_elogind("Attaching session %s to user %u", s->id, user->user_record->uid);
                session_set_user(s, user);
        }

        if (remote) {
                k = parse_boolean(remote);
                if (k >= 0)
                        s->remote = k;
        }

        if (vtnr)
                safe_atou(vtnr, &s->vtnr);

        if (seat && !s->seat) {
                Seat *o;

                o = hashmap_get(s->manager->seats, seat);
                if (o)
                        r = seat_attach_session(o, s);
                if (!o || r < 0)
                        log_error("Cannot attach session %s to seat %s", s->id, seat);
        }

        if (!s->seat || !seat_has_vts(s->seat))
                s->vtnr = 0;

        if (position && s->seat) {
                unsigned npos;

                safe_atou(position, &npos);
                seat_claim_position(s->seat, s, npos);
        }

        if (tty_validity) {
                TTYValidity v;

                v = tty_validity_from_string(tty_validity);
                if (v < 0)
                        log_debug("Failed to parse TTY validity: %s", tty_validity);
                else
                        s->tty_validity = v;
        }

        if (leader) {
                pid_t pid;

                r = parse_pid(leader, &pid);
                if (r < 0)
                        log_debug_errno(r, "Failed to parse leader PID of session: %s", leader);
                else {
                        r = session_set_leader(s, pid);
                        if (r < 0)
                                log_warning_errno(r, "Failed to set session leader PID, ignoring: %m");
                }
        }

        if (type) {
                SessionType t;

                t = session_type_from_string(type);
                if (t >= 0)
                        s->type = t;
        }

        if (original_type) {
                SessionType ot;

                ot = session_type_from_string(original_type);
                if (ot >= 0)
                        s->original_type = ot;
        } else
                /* Pre-v246 compat: initialize original_type if not set in the state file */
                s->original_type = s->type;

        if (class) {
                SessionClass c;

                c = session_class_from_string(class);
                if (c >= 0)
                        s->class = c;
        }

        if (streq_ptr(state, "closing"))
                s->stopping = true;

        if (s->fifo_path) {
                int fd;

#if 1 /// elogind has no systemd in the background preserving FIFOs ...
#if ENABLE_DEBUG_ELOGIND
                struct stat st;
                bool exists = stat(s->fifo_path, &st) ? false : true;
#endif
                log_debug_elogind("Recreating fifo for session %s at %s (%s)",
                                  s->id, s->fifo_path,
                                  exists ? "exists" : "must be created");

                if (mkfifo(s->fifo_path, 0600) < 0 && errno != EEXIST) {
                        /* Do not let anyone try to open it for writing when the
                         * file no longer exists. session_create_fifo() only
                         * creates a new file when session->fifo_path is NULL.
                         */
                        log_debug_elogind("Recreation of session %s fifo failed: %d %s",
                                          s->id, errno, strerror(errno));
                        s->fifo_path = mfree(s->fifo_path);
                }
#endif // 1
                /* If we open an unopened pipe for reading we will not
                   get an EOF. to trigger an EOF we hence open it for
                   writing, but close it right away which then will
                   trigger the EOF. This will happen immediately if no
                   other process has the FIFO open for writing, i. e.
                   when the session died before logind (re)started. */

                fd = session_create_fifo(s);
                safe_close(fd);
        }

        if (realtime)
                (void) deserialize_usec(realtime, &s->timestamp.realtime);
        if (monotonic)
                (void) deserialize_usec(monotonic, &s->timestamp.monotonic);

        if (active) {
                k = parse_boolean(active);
                if (k >= 0)
                        s->was_active = k;
        }

        if (is_display) {
                /* Note that when enumerating users are loaded before sessions, hence the display session to use is
                 * something we have to store along with the session and not the user, as in that case we couldn't
                 * apply it at the time we load the user. */

                k = parse_boolean(is_display);
                if (k < 0)
                        log_warning_errno(k, "Failed to parse IS_DISPLAY session property: %m");
                else if (k > 0)
                        s->user->display = s;
        }

        if (controller) {
                if (bus_name_has_owner(s->manager->bus, controller, NULL) > 0) {
                        session_set_controller(s, controller, false, false);
                        session_load_devices(s, devices);
                } else
                        session_restore_vt(s);
        }

        return r;
}

int session_activate(Session *s) {
        unsigned num_pending;

        assert(s);
        assert(s->user);

        if (!s->seat)
                return -EOPNOTSUPP;

        if (s->seat->active == s)
                return 0;

        /* on seats with VTs, we let VTs manage session-switching */
        if (seat_has_vts(s->seat)) {
                if (s->vtnr == 0)
                        return -EOPNOTSUPP;

                return chvt(s->vtnr);
        }

        /* On seats without VTs, we implement session-switching in logind. We
         * try to pause all session-devices and wait until the session
         * controller acknowledged them. Once all devices are asleep, we simply
         * switch the active session and be done.
         * We save the session we want to switch to in seat->pending_switch and
         * seat_complete_switch() will perform the final switch. */

        s->seat->pending_switch = s;

        /* if no devices are running, immediately perform the session switch */
        num_pending = session_device_try_pause_all(s);
        if (!num_pending)
                seat_complete_switch(s->seat);

        return 0;
}

#if 0 /// UNNEEDED by elogind
static int session_start_scope(Session *s, sd_bus_message *properties, sd_bus_error *error) {
        int r;

        assert(s);
        assert(s->user);

        if (!s->scope) {
                _cleanup_strv_free_ char **after = NULL;
                _cleanup_free_ char *scope = NULL;
                const char *description;

                s->scope_job = mfree(s->scope_job);

                scope = strjoin("session-", s->id, ".scope");
                if (!scope)
                        return log_oom();

                description = strjoina("Session ", s->id, " of User ", s->user->user_record->user_name);

                /* We usually want to order session scopes after systemd-user-sessions.service since the
                 * latter unit is used as login session barrier for unprivileged users. However the barrier
                 * doesn't apply for root as sysadmin should always be able to log in (and without waiting
                 * for any timeout to expire) in case something goes wrong during the boot process. Since
                 * ordering after systemd-user-sessions.service and the user instance is optional we make use
                 * of STRV_IGNORE with strv_new() to skip these order constraints when needed. */
                after = strv_new("systemd-logind.service",
                                 s->user->runtime_dir_service,
                                 !uid_is_system(s->user->user_record->uid) ? "systemd-user-sessions.service" : STRV_IGNORE,
                                 s->user->service);
                if (!after)
                        return log_oom();

                r = manager_start_scope(
                                s->manager,
                                scope,
                                s->leader,
                                s->user->slice,
                                description,
                                /* These two have StopWhenUnneeded= set, hence add a dep towards them */
                                STRV_MAKE(s->user->runtime_dir_service,
                                          s->user->service),
                                after,
                                user_record_home_directory(s->user->user_record),
                                properties,
                                error,
                                &s->scope_job);
                if (r < 0)
                        return log_error_errno(r, "Failed to start session scope %s: %s",
                                               scope, bus_error_message(error, r));

                s->scope = TAKE_PTR(scope);
        }

        (void) hashmap_put(s->manager->session_units, s->scope, s);

        return 0;
}

static int session_dispatch_stop_on_idle(sd_event_source *source, uint64_t t, void *userdata) {
        Session *s = userdata;
        dual_timestamp ts;
        int r, idle;

        assert(s);

        if (s->stopping)
                return 0;

        idle = session_get_idle_hint(s, &ts);
        if (idle) {
                log_debug("Session \"%s\" of user \"%s\" is idle, stopping.", s->id, s->user->user_record->user_name);

                return session_stop(s, /* force */ true);
        }

        r = sd_event_source_set_time(
                        source,
                        usec_add(dual_timestamp_is_set(&ts) ? ts.monotonic : now(CLOCK_MONOTONIC),
                                 s->manager->stop_idle_session_usec));
        if (r < 0)
                return log_error_errno(r, "Failed to configure stop on idle session event source: %m");

        r = sd_event_source_set_enabled(source, SD_EVENT_ONESHOT);
        if (r < 0)
                return log_error_errno(r, "Failed to enable stop on idle session event source: %m");

        return 1;
}

static int session_setup_stop_on_idle_timer(Session *s) {
        int r;

        assert(s);

        if (s->manager->stop_idle_session_usec == USEC_INFINITY)
                return 0;

        r = sd_event_add_time_relative(
                        s->manager->event,
                        &s->stop_on_idle_event_source,
                        CLOCK_MONOTONIC,
                        s->manager->stop_idle_session_usec,
                        0,
                        session_dispatch_stop_on_idle, s);
        if (r < 0)
                return log_error_errno(r, "Failed to add stop on idle session event source: %m");

        return 0;
}
#else // 0
static int session_start_cgroup(Session *s) {
        int r;

        assert(s);
        assert(s->user);
        assert(s->leader > 0);

        /* First, create our own group */
        r = cg_create(SYSTEMD_CGROUP_CONTROLLER, s->id);
        if (r < 0)
                return log_error_errno(r, "Failed to create cgroup %s: %m", s->id);

        r = cg_attach(SYSTEMD_CGROUP_CONTROLLER, s->id, s->leader);
        if (r < 0)
                log_warning_errno(r, "Failed to attach PID %d to cgroup %s: %m", s->leader, s->id);

        return 0;
}
#endif // 0

int session_start(Session *s, sd_bus_message *properties, sd_bus_error *error) {
        int r;

        assert(s);

        if (!s->user)
                return -ESTALE;

        if (s->stopping)
                return -EINVAL;

        if (s->started)
                return 0;

        r = user_start(s->user);
        if (r < 0)
                return r;

#if 0 /// elogind does its own session management
        r = session_start_scope(s, properties, error);
        if (r < 0)
                return r;

        r = session_setup_stop_on_idle_timer(s);
#else // 0
        r = session_start_cgroup(s);
#endif // 0
        if (r < 0)
                return r;

        log_struct(s->class == SESSION_BACKGROUND ? LOG_DEBUG : LOG_INFO,
                   "MESSAGE_ID=" SD_MESSAGE_SESSION_START_STR,
                   "SESSION_ID=%s", s->id,
                   "USER_ID=%s", s->user->user_record->user_name,
                   "LEADER="PID_FMT, s->leader,
                   LOG_MESSAGE("New session %s of user %s.", s->id, s->user->user_record->user_name));

        if (!dual_timestamp_is_set(&s->timestamp))
                dual_timestamp_get(&s->timestamp);

        if (s->seat)
                seat_read_active_vt(s->seat);

        s->started = true;

        user_elect_display(s->user);

        /* Save data */
        session_save(s);
        user_save(s->user);
        if (s->seat)
                seat_save(s->seat);

        /* Send signals */
        session_send_signal(s, true);
        user_send_changed(s->user, "Display", NULL);

        if (s->seat && s->seat->active == s)
                seat_send_changed(s->seat, "ActiveSession", NULL);

        return 0;
}

#if 0 /// UNNEEDED by elogind
static int session_stop_scope(Session *s, bool force) {
        _cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
        int r;

        assert(s);

        if (!s->scope)
                return 0;

        /* Let's always abandon the scope first. This tells systemd that we are not interested anymore, and everything
         * that is left in the scope is "left-over". Informing systemd about this has the benefit that it will log
         * when killing any processes left after this point. */
        r = manager_abandon_scope(s->manager, s->scope, &error);
        if (r < 0) {
                log_warning_errno(r, "Failed to abandon session scope, ignoring: %s", bus_error_message(&error, r));
                sd_bus_error_free(&error);
        }

        s->scope_job = mfree(s->scope_job);

        /* Optionally, let's kill everything that's left now. */
        if (force ||
            (s->user->user_record->kill_processes != 0 &&
             (s->user->user_record->kill_processes > 0 ||
              manager_shall_kill(s->manager, s->user->user_record->user_name)))) {

                r = manager_stop_unit(s->manager, s->scope, force ? "replace" : "fail", &error, &s->scope_job);
                if (r < 0) {
                        if (force)
                                return log_error_errno(r, "Failed to stop session scope: %s", bus_error_message(&error, r));

                        log_warning_errno(r, "Failed to stop session scope, ignoring: %s", bus_error_message(&error, r));
                }
        } else {

                /* With no killing, this session is allowed to persist in "closing" state indefinitely.
                 * Therefore session stop and session removal may be two distinct events.
                 * Session stop is quite significant on its own, let's log it. */
                log_struct(s->class == SESSION_BACKGROUND ? LOG_DEBUG : LOG_INFO,
                           "SESSION_ID=%s", s->id,
                           "USER_ID=%s", s->user->user_record->user_name,
                           "LEADER="PID_FMT, s->leader,
                           LOG_MESSAGE("Session %s logged out. Waiting for processes to exit.", s->id));
        }

        return 0;
}
#else // 0
static int session_stop_cgroup(Session *s, bool force) {
        _cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
        int r;

        assert(s);

        // elogind must not kill lingering user processes alive
        if ( (force || manager_shall_kill(s->manager, s->user->user_record->user_name) )
            && (user_check_linger_file(s->user) < 1) ) {
                r = session_kill(s, KILL_ALL, SIGTERM);
                if (r < 0)
                        return r;
        }

        return 0;
}
#endif // 0

int session_stop(Session *s, bool force) {
        int r;

        assert(s);

        /* This is called whenever we begin with tearing down a session record. It's called in four cases: explicit API
         * request via the bus (either directly for the session object or for the seat or user object this session
         * belongs to; 'force' is true), or due to automatic GC (i.e. scope vanished; 'force' is false), or because the
         * session FIFO saw an EOF ('force' is false), or because the release timer hit ('force' is false). */

        if (!s->user)
                return -ESTALE;
        log_debug_elogind("Stopping session %s %s ...", s->id, force ? "(forced)" : "");
        if (!s->started)
                return 0;
        if (s->stopping)
                return 0;

        s->timer_event_source = sd_event_source_unref(s->timer_event_source);

        if (s->seat)
                seat_evict_position(s->seat, s);

        /* We are going down, don't care about FIFOs anymore */
        session_remove_fifo(s);

        /* Kill cgroup */
#if 0 /// elogind does not start scopes, but sessions
        r = session_stop_scope(s, force);
#else // 0
        r = session_stop_cgroup(s, force);
        session_add_to_gc_queue(s);
#endif // 0

        s->stopping = true;

        user_elect_display(s->user);

        session_save(s);
        user_save(s->user);

        return r;
}

int session_finalize(Session *s) {
        SessionDevice *sd;

        assert(s);

        if (!s->user)
                return -ESTALE;

        if (s->started)
                log_struct(s->class == SESSION_BACKGROUND ? LOG_DEBUG : LOG_INFO,
                           "MESSAGE_ID=" SD_MESSAGE_SESSION_STOP_STR,
                           "SESSION_ID=%s", s->id,
                           "USER_ID=%s", s->user->user_record->user_name,
                           "LEADER="PID_FMT, s->leader,
                           LOG_MESSAGE("Removed session %s.", s->id));
#if 1 /// let elogind at least put out a debug message if s was not started
        else
                log_debug_elogind("Session %s not started, finalizing...", s->id);
#endif // 1

        s->timer_event_source = sd_event_source_unref(s->timer_event_source);

        if (s->seat)
                seat_evict_position(s->seat, s);

        /* Kill session devices */
        while ((sd = hashmap_first(s->devices)))
                session_device_free(sd);

        (void) unlink(s->state_file);
        session_add_to_gc_queue(s);
        user_add_to_gc_queue(s->user);

        if (s->started) {
                session_send_signal(s, false);
                s->started = false;
        }

        if (s->seat) {
                if (s->seat->active == s)
                        seat_set_active(s->seat, NULL);

                seat_save(s->seat);
        }

        user_save(s->user);
        user_send_changed(s->user, "Display", NULL);

        return 0;
}

static int release_timeout_callback(sd_event_source *es, uint64_t usec, void *userdata) {
        Session *s = ASSERT_PTR(userdata);

        assert(es);

        log_debug_elogind("Session release timeout reached, stopping session %s", s->id);
        session_stop(s, /* force = */ false);
        return 0;
}

int session_release(Session *s) {
        assert(s);

        log_debug_elogind("Session %s release? %s", s->id,
                          !s->started ? "No, not started, yet." :
                          s->stopping ? "No, already stopping." :
                          s->timer_event_source ? "No, timer already running." :
                          "Yes, releasing in 20 seconds."); /* RELEASE_USEC is a formula :( */
        if (!s->started || s->stopping)
                return 0;

        if (s->timer_event_source)
                return 0;

        return sd_event_add_time_relative(
                        s->manager->event,
                        &s->timer_event_source,
                        CLOCK_MONOTONIC,
                        RELEASE_USEC, 0,
                        release_timeout_callback, s);
}

bool session_is_active(Session *s) {
        assert(s);

        if (!s->seat)
                return true;

        return s->seat->active == s;
}

static int get_tty_atime(const char *tty, usec_t *atime) {
        _cleanup_free_ char *p = NULL;
        struct stat st;

        assert(tty);
        assert(atime);

        if (!path_is_absolute(tty)) {
                p = path_join("/dev", tty);
                if (!p)
                        return -ENOMEM;

                tty = p;
        } else if (!path_startswith(tty, "/dev/"))
                return -ENOENT;

        if (lstat(tty, &st) < 0)
                return -errno;

        *atime = timespec_load(&st.st_atim);
        return 0;
}

static int get_process_ctty_atime(pid_t pid, usec_t *atime) {
        _cleanup_free_ char *p = NULL;
        int r;

        assert(pid > 0);
        assert(atime);

        r = get_ctty(pid, NULL, &p);
        if (r < 0)
                return r;

        return get_tty_atime(p, atime);
}

int session_get_idle_hint(Session *s, dual_timestamp *t) {
        usec_t atime = 0, dtime = 0;
        int r;

        assert(s);

        /* Graphical sessions have an explicit idle hint */
        if (SESSION_TYPE_IS_GRAPHICAL(s->type)) {
                if (t)
                        *t = s->idle_hint_timestamp;

                return s->idle_hint;
        }

        /* For sessions with an explicitly configured tty, let's check its atime */
        if (s->tty) {
                r = get_tty_atime(s->tty, &atime);
                if (r >= 0)
                        goto found_atime;
        }

        /* For sessions with a leader but no explicitly configured tty, let's check the controlling tty of
         * the leader */
        if (pid_is_valid(s->leader)) {
                r = get_process_ctty_atime(s->leader, &atime);
                if (r >= 0)
                        goto found_atime;
        }

        if (t)
                *t = DUAL_TIMESTAMP_NULL;

        return false;

found_atime:
        if (t)
                dual_timestamp_from_realtime(t, atime);

        if (s->manager->idle_action_usec > 0 && s->manager->stop_idle_session_usec != USEC_INFINITY)
                dtime = MIN(s->manager->idle_action_usec, s->manager->stop_idle_session_usec);
        else if (s->manager->idle_action_usec > 0)
                dtime = s->manager->idle_action_usec;
        else if (s->manager->stop_idle_session_usec != USEC_INFINITY)
                dtime = s->manager->stop_idle_session_usec;
        else
                return false;

        return usec_add(atime, dtime) <= now(CLOCK_REALTIME);
}

int session_set_idle_hint(Session *s, bool b) {
        assert(s);

        if (!SESSION_TYPE_IS_GRAPHICAL(s->type))
                return -ENOTTY;

        if (s->idle_hint == b)
                return 0;

        s->idle_hint = b;
        dual_timestamp_get(&s->idle_hint_timestamp);

        session_send_changed(s, "IdleHint", "IdleSinceHint", "IdleSinceHintMonotonic", NULL);

        if (s->seat)
                seat_send_changed(s->seat, "IdleHint", "IdleSinceHint", "IdleSinceHintMonotonic", NULL);

        user_send_changed(s->user, "IdleHint", "IdleSinceHint", "IdleSinceHintMonotonic", NULL);
        manager_send_changed(s->manager, "IdleHint", "IdleSinceHint", "IdleSinceHintMonotonic", NULL);

        return 1;
}

int session_get_locked_hint(Session *s) {
        assert(s);

        return s->locked_hint;
}

void session_set_locked_hint(Session *s, bool b) {
        assert(s);

        if (s->locked_hint == b)
                return;

        s->locked_hint = b;

        session_send_changed(s, "LockedHint", NULL);
}

void session_set_type(Session *s, SessionType t) {
        assert(s);

        if (s->type == t)
                return;

        s->type = t;
        session_save(s);

        session_send_changed(s, "Type", NULL);
}

int session_set_display(Session *s, const char *display) {
        int r;

        assert(s);
        assert(display);

        r = free_and_strdup(&s->display, display);
        if (r <= 0)  /* 0 means the strings were equal */
                return r;

        session_save(s);

        session_send_changed(s, "Display", NULL);

        return 1;
}

static int session_dispatch_fifo(sd_event_source *es, int fd, uint32_t revents, void *userdata) {
        Session *s = ASSERT_PTR(userdata);

        assert(s->fifo_fd == fd);

        /* EOF on the FIFO means the session died abnormally. */

        log_debug_elogind("EOF on Session %s FIFO: Session died abnormally, stopping...", s->id);
        session_remove_fifo(s);
        session_stop(s, /* force = */ false);

        return 1;
}

int session_create_fifo(Session *s) {
        int r;

        assert(s);

        /* Create FIFO */
        if (!s->fifo_path) {
                r = mkdir_safe_label("/run/systemd/sessions", 0755, 0, 0, MKDIR_WARN_MODE);
                if (r < 0)
                        return r;

                s->fifo_path = strjoin("/run/systemd/sessions/", s->id, ".ref");
                if (!s->fifo_path)
                        return -ENOMEM;

                log_debug_elogind("Creating fifo for session %s at %s", s->id, s->fifo_path);
                if (mkfifo(s->fifo_path, 0600) < 0 && errno != EEXIST)
                        return -errno;
        }

        /* Open reading side */
        if (s->fifo_fd < 0) {
                log_debug_elogind("Opening reading side of fifo for session %s", s->id);
                s->fifo_fd = open(s->fifo_path, O_RDONLY|O_CLOEXEC|O_NONBLOCK);
                if (s->fifo_fd < 0)
                        return -errno;
        }

        if (!s->fifo_event_source) {
                log_debug_elogind("Adding event source to fifo for session %s", s->id);
                r = sd_event_add_io(s->manager->event, &s->fifo_event_source, s->fifo_fd, 0, session_dispatch_fifo, s);
                if (r < 0)
                        return r;

                log_debug_elogind("Raising event priority for session %s fifo", s->id);
                /* Let's make sure we noticed dead sessions before we process new bus requests (which might
                 * create new sessions). */
                r = sd_event_source_set_priority(s->fifo_event_source, SD_EVENT_PRIORITY_NORMAL-10);
                if (r < 0)
                        return r;
        }

        /* Open writing side */
        log_debug_elogind("Opening writing side of fifo for session %s", s->id);
        return RET_NERRNO(open(s->fifo_path, O_WRONLY|O_CLOEXEC|O_NONBLOCK));
}

static void session_remove_fifo(Session *s) {
#if 1 /// Don't keep invalid FIFOs on elogind restart
        int current_fifo_fd = s->fifo_fd;
#endif // 1
        assert(s);

        s->fifo_event_source = sd_event_source_unref(s->fifo_event_source);
        s->fifo_fd = safe_close(s->fifo_fd);

        if (s->fifo_path) {
#if 1 /// Do not remove the fifo if elogind is to be restarted
                if (s->manager->do_interrupt && (current_fifo_fd >= 0)) {
                        log_debug_elogind("Keeping FIFO %d at %s for session %s", current_fifo_fd, s->fifo_path, s->id);
                } else {
                        log_debug_elogind("Removing FIFO %d at %s for session %s", current_fifo_fd, s->fifo_path, s->id);
#endif // 1
                (void) unlink(s->fifo_path);
                s->fifo_path = mfree(s->fifo_path);
#if 1 /// end elogind extra if
                }
#endif // 1
        }
}

bool session_may_gc(Session *s, bool drop_not_started) {
#if 0 /// elogind supports neither scopes nor jobs
        int r;
#endif // 0

        assert(s);

        log_debug_elogind("Session %s may gc ?", s->id);
        log_debug_elogind("  dns && !started: %s", yes_no(drop_not_started && !s->started));
        log_debug_elogind("  is userless    : %s", yes_no(!s->user));
        log_debug_elogind("  dns or stopping: %s", yes_no(drop_not_started || s->stopping));
        log_debug_elogind("  FIFO state     : %s",
                          s->fifo_fd < 0           ? "No FIFO opened"   :
                          pipe_eof(s->fifo_fd) > 0 ? "FIFO reached EOF" :
                          "FIFO up and running!");
        if (drop_not_started && !s->started)
                return true;

        if (!s->user)
                return true;

        if (s->fifo_fd >= 0) {
                if (pipe_eof(s->fifo_fd) <= 0)
                        return false;
        }

#if 0 /// elogind supports neither scopes nor jobs
        if (s->scope_job) {
                _cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;

                r = manager_job_is_active(s->manager, s->scope_job, &error);
                if (r < 0)
                        log_debug_errno(r, "Failed to determine whether job '%s' is pending, ignoring: %s", s->scope_job, bus_error_message(&error, r));
                if (r != 0)
                        return false;
        }

        if (s->scope) {
                _cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;

                r = manager_unit_is_active(s->manager, s->scope, &error);
                if (r < 0)
                        log_debug_errno(r, "Failed to determine whether unit '%s' is active, ignoring: %s", s->scope, bus_error_message(&error, r));
                if (r != 0)
                        return false;
        }

        return true;
#else // 0
        // elogind has to rely on drop_not_started, and that the state is correctly loaded
        return (drop_not_started || s->stopping);
#endif // 0
}

void session_add_to_gc_queue(Session *s) {
        assert(s);

        if (s->in_gc_queue)
                return;

        LIST_PREPEND(gc_queue, s->manager->session_gc_queue, s);
        s->in_gc_queue = true;
}

SessionState session_get_state(Session *s) {
        assert(s);

        /* always check closing first */
        if (s->stopping || s->timer_event_source)
                return SESSION_CLOSING;

#if 0 /// elogind does not support systemd scope_jobs
        if (s->scope_job || s->fifo_fd < 0)
#else // 0
        if (s->fifo_fd < 0)
#endif // 0
                return SESSION_OPENING;

        if (session_is_active(s))
                return SESSION_ACTIVE;

        return SESSION_ONLINE;
}

int session_kill(Session *s, KillWho who, int signo) {
        assert(s);

#if 0 /// Without direct cgroup support, elogind can not kill sessions
        if (!s->scope)
                return -ESRCH;

        return manager_kill_unit(s->manager, s->scope, who, signo, NULL);
#else // 0
        if (who == KILL_LEADER) {
                if (s->leader <= 0)
                        return -ESRCH;

                /* FIXME: verify that leader is in cgroup?  */

                if (kill(s->leader, signo) < 0) {
                        return log_error_errno(errno, "Failed to kill process leader %d for session %s: %m", s->leader, s->id);
                }
                return 0;
        } else
                return cg_kill_recursive (SYSTEMD_CGROUP_CONTROLLER, s->id, signo,
                                          CGROUP_IGNORE_SELF | CGROUP_REMOVE,
                                          NULL, NULL, NULL);
#endif // 0
}

static int session_open_vt(Session *s) {
        char path[sizeof("/dev/tty") + DECIMAL_STR_MAX(s->vtnr)];

        if (s->vtnr < 1)
                return -ENODEV;

        if (s->vtfd >= 0)
                return s->vtfd;

        sprintf(path, "/dev/tty%u", s->vtnr);
        s->vtfd = open_terminal(path, O_RDWR | O_CLOEXEC | O_NONBLOCK | O_NOCTTY);
        if (s->vtfd < 0)
                return log_error_errno(s->vtfd, "cannot open VT %s of session %s: %m", path, s->id);

        return s->vtfd;
}

static int session_prepare_vt(Session *s) {
        int vt, r;
        struct vt_mode mode = {};

        if (s->vtnr < 1)
                return 0;

        vt = session_open_vt(s);
        if (vt < 0)
                return vt;

        r = fchown(vt, s->user->user_record->uid, -1);
        if (r < 0) {
                r = log_error_errno(errno,
                                    "Cannot change owner of /dev/tty%u: %m",
                                    s->vtnr);
                goto error;
        }

        r = ioctl(vt, KDSKBMODE, K_OFF);
        if (r < 0) {
                r = log_error_errno(errno,
                                    "Cannot set K_OFF on /dev/tty%u: %m",
                                    s->vtnr);
                goto error;
        }

        r = ioctl(vt, KDSETMODE, KD_GRAPHICS);
        if (r < 0) {
                r = log_error_errno(errno,
                                    "Cannot set KD_GRAPHICS on /dev/tty%u: %m",
                                    s->vtnr);
                goto error;
        }

        /* Oh, thanks to the VT layer, VT_AUTO does not work with KD_GRAPHICS.
         * So we need a dummy handler here which just acknowledges *all* VT
         * switch requests. */
        mode.mode = VT_PROCESS;
        mode.relsig = SIGRTMIN;
        mode.acqsig = SIGRTMIN + 1;
        r = ioctl(vt, VT_SETMODE, &mode);
        if (r < 0) {
                r = log_error_errno(errno,
                                    "Cannot set VT_PROCESS on /dev/tty%u: %m",
                                    s->vtnr);
                goto error;
        }

        return 0;

error:
        session_restore_vt(s);
        return r;
}

static void session_restore_vt(Session *s) {
        int r;

        r = vt_restore(s->vtfd);
        if (r == -EIO) {
                int vt, old_fd;

                /* It might happen if the controlling process exited before or while we were
                 * restoring the VT as it would leave the old file-descriptor in a hung-up
                 * state. In this case let's retry with a fresh handle to the virtual terminal. */

                /* We do a little dance to avoid having the terminal be available
                 * for reuse before we've cleaned it up. */
                old_fd = TAKE_FD(s->vtfd);

                vt = session_open_vt(s);
                safe_close(old_fd);

                if (vt >= 0)
                        r = vt_restore(vt);
        }

        if (r < 0)
                log_warning_errno(r, "Failed to restore VT, ignoring: %m");

        s->vtfd = safe_close(s->vtfd);
}

void session_leave_vt(Session *s) {
        int r;

        assert(s);

        /* This is called whenever we get a VT-switch signal from the kernel.
         * We acknowledge all of them unconditionally. Note that session are
         * free to overwrite those handlers and we only register them for
         * sessions with controllers. Legacy sessions are not affected.
         * However, if we switch from a non-legacy to a legacy session, we must
         * make sure to pause all device before acknowledging the switch. We
         * process the real switch only after we are notified via sysfs, so the
         * legacy session might have already started using the devices. If we
         * don't pause the devices before the switch, we might confuse the
         * session we switch to. */

        if (s->vtfd < 0)
                return;

        session_device_pause_all(s);
        r = vt_release(s->vtfd, false);
        if (r < 0)
                log_debug_errno(r, "Cannot release VT of session %s: %m", s->id);
}

bool session_is_controller(Session *s, const char *sender) {
        return streq_ptr(ASSERT_PTR(s)->controller, sender);
}

static void session_release_controller(Session *s, bool notify) {
        _unused_ _cleanup_free_ char *name = NULL;
        SessionDevice *sd;

        if (!s->controller)
                return;

        name = s->controller;

        /* By resetting the controller before releasing the devices, we won't send notification signals.
         * This avoids sending useless notifications if the controller is released on disconnects. */
        if (!notify)
                s->controller = NULL;

        while ((sd = hashmap_first(s->devices)))
                session_device_free(sd);

        s->controller = NULL;
        s->track = sd_bus_track_unref(s->track);
}

static int on_bus_track(sd_bus_track *track, void *userdata) {
        Session *s = ASSERT_PTR(userdata);

        assert(track);

        session_drop_controller(s);

        return 0;
}

int session_set_controller(Session *s, const char *sender, bool force, bool prepare) {
        _cleanup_free_ char *name = NULL;
        int r;

        assert(s);
        assert(sender);

        if (session_is_controller(s, sender))
                return 0;
        if (s->controller && !force)
                return -EBUSY;

        name = strdup(sender);
        if (!name)
                return -ENOMEM;

        s->track = sd_bus_track_unref(s->track);
        r = sd_bus_track_new(s->manager->bus, &s->track, on_bus_track, s);
        if (r < 0)
                return r;

        r = sd_bus_track_add_name(s->track, name);
        if (r < 0)
                return r;

        /* When setting a session controller, we forcibly mute the VT and set
         * it into graphics-mode. Applications can override that by changing
         * VT state after calling TakeControl(). However, this serves as a good
         * default and well-behaving controllers can now ignore VTs entirely.
         * Note that we reset the VT on ReleaseControl() and if the controller
         * exits.
         * If logind crashes/restarts, we restore the controller during restart
         * (without preparing the VT since the controller has probably overridden
         * VT state by now) or reset the VT in case it crashed/exited, too. */
        if (prepare) {
                r = session_prepare_vt(s);
                if (r < 0) {
                        s->track = sd_bus_track_unref(s->track);
                        return r;
                }
        }

        session_release_controller(s, true);
        s->controller = TAKE_PTR(name);
        session_save(s);

        return 0;
}

void session_drop_controller(Session *s) {
        assert(s);

        if (!s->controller)
                return;

        s->track = sd_bus_track_unref(s->track);
        session_set_type(s, s->original_type);
        session_release_controller(s, false);
        session_save(s);
        session_restore_vt(s);
}

static const char* const session_state_table[_SESSION_STATE_MAX] = {
        [SESSION_OPENING] = "opening",
        [SESSION_ONLINE]  = "online",
        [SESSION_ACTIVE]  = "active",
        [SESSION_CLOSING] = "closing",
};

DEFINE_STRING_TABLE_LOOKUP(session_state, SessionState);

static const char* const session_type_table[_SESSION_TYPE_MAX] = {
        [SESSION_UNSPECIFIED] = "unspecified",
        [SESSION_TTY]         = "tty",
        [SESSION_X11]         = "x11",
        [SESSION_WAYLAND]     = "wayland",
        [SESSION_MIR]         = "mir",
        [SESSION_WEB]         = "web",
};

DEFINE_STRING_TABLE_LOOKUP(session_type, SessionType);

static const char* const session_class_table[_SESSION_CLASS_MAX] = {
        [SESSION_USER]        = "user",
        [SESSION_GREETER]     = "greeter",
        [SESSION_LOCK_SCREEN] = "lock-screen",
        [SESSION_BACKGROUND]  = "background",
};

DEFINE_STRING_TABLE_LOOKUP(session_class, SessionClass);

static const char* const kill_who_table[_KILL_WHO_MAX] = {
        [KILL_LEADER] = "leader",
        [KILL_ALL]    = "all",
};

DEFINE_STRING_TABLE_LOOKUP(kill_who, KillWho);

static const char* const tty_validity_table[_TTY_VALIDITY_MAX] = {
        [TTY_FROM_PAM]          = "from-pam",
        [TTY_FROM_UTMP]         = "from-utmp",
        [TTY_UTMP_INCONSISTENT] = "utmp-inconsistent",
};

DEFINE_STRING_TABLE_LOOKUP(tty_validity, TTYValidity);
