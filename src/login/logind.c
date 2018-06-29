/* SPDX-License-Identifier: LGPL-2.1+ */
/***
  This file is part of systemd.

  Copyright 2011 Lennart Poettering
***/

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#if 0 /// elogind needs the systems udev header
#include "libudev.h"
#else
#include <libudev.h>
#endif // 0
#include "sd-daemon.h"

#include "alloc-util.h"
#include "bus-error.h"
#include "bus-util.h"
//#include "cgroup-util.h"
#include "def.h"
#include "dirent-util.h"
#include "fd-util.h"
#include "format-util.h"
#include "fs-util.h"
#include "logind.h"
//#include "parse-util.h"
//#include "process-util.h"
#include "selinux-util.h"
#include "signal-util.h"
#include "strv.h"
#include "udev-util.h"
/// Additional includes needed by elogind
#include "cgroup.h"       // From src/core/
#include "elogind.h"
#include "label.h"
#include "musl_missing.h"
#include "process-util.h"
#include "cgroup-util.h"

static void manager_free(Manager *m);

#if 0 /// elogind does not support autospawning of vts
#endif // 0
static Manager *manager_new(void) {
        Manager *m;
        int r;

        m = new0(Manager, 1);
        if (!m)
                return NULL;

        m->console_active_fd = -1;
#if 0 /// UNNEEDED by elogind
        m->reserve_vt_fd = -1;
#endif // 0

        m->idle_action_not_before_usec = now(CLOCK_MONOTONIC);

        m->devices = hashmap_new(&string_hash_ops);
        m->seats = hashmap_new(&string_hash_ops);
        m->sessions = hashmap_new(&string_hash_ops);
        m->users = hashmap_new(NULL);
        m->inhibitors = hashmap_new(&string_hash_ops);
        m->buttons = hashmap_new(&string_hash_ops);

        m->user_units = hashmap_new(&string_hash_ops);
        m->session_units = hashmap_new(&string_hash_ops);

        if (!m->devices || !m->seats || !m->sessions || !m->users || !m->inhibitors || !m->buttons || !m->user_units || !m->session_units)
                goto fail;

#if 1 /// elogind needs some more data
        r = elogind_manager_new(m);
        if (r < 0)
                goto fail;
#endif // 1
        m->udev = udev_new();
        if (!m->udev)
                goto fail;

        r = sd_event_default(&m->event);
        if (r < 0)
                goto fail;

        sd_event_set_watchdog(m->event, true);

        manager_reset_config(m);

        return m;

fail:
        manager_free(m);
        return NULL;
}

static void manager_free(Manager *m) {
        Session *session;
        User *u;
        Device *d;
        Seat *s;
        Inhibitor *i;
        Button *b;

        if (!m)
                return;

        while ((session = hashmap_first(m->sessions)))
                session_free(session);

        while ((u = hashmap_first(m->users)))
                user_free(u);

        while ((d = hashmap_first(m->devices)))
                device_free(d);

        while ((s = hashmap_first(m->seats)))
                seat_free(s);

        while ((i = hashmap_first(m->inhibitors)))
                inhibitor_free(i);

        while ((b = hashmap_first(m->buttons)))
                button_free(b);

        hashmap_free(m->devices);
        hashmap_free(m->seats);
        hashmap_free(m->sessions);
        hashmap_free(m->users);
        hashmap_free(m->inhibitors);
        hashmap_free(m->buttons);

        hashmap_free(m->user_units);
        hashmap_free(m->session_units);

        sd_event_source_unref(m->idle_action_event_source);
        sd_event_source_unref(m->inhibit_timeout_source);
        sd_event_source_unref(m->scheduled_shutdown_timeout_source);
        sd_event_source_unref(m->nologin_timeout_source);
        sd_event_source_unref(m->wall_message_timeout_source);

        sd_event_source_unref(m->console_active_event_source);
        sd_event_source_unref(m->udev_seat_event_source);
        sd_event_source_unref(m->udev_device_event_source);
        sd_event_source_unref(m->udev_vcsa_event_source);
        sd_event_source_unref(m->udev_button_event_source);
        sd_event_source_unref(m->lid_switch_ignore_event_source);

        safe_close(m->console_active_fd);

        udev_monitor_unref(m->udev_seat_monitor);
        udev_monitor_unref(m->udev_device_monitor);
        udev_monitor_unref(m->udev_vcsa_monitor);
        udev_monitor_unref(m->udev_button_monitor);

        udev_unref(m->udev);

        if (m->unlink_nologin)
                (void) unlink_or_warn("/run/nologin");

        bus_verify_polkit_async_registry_free(m->polkit_registry);

        sd_bus_unref(m->bus);
        sd_event_unref(m->event);

#if 0 /// elogind does not support autospawning of vts
        safe_close(m->reserve_vt_fd);
#endif // 0
#if 1 /// elogind has to free its own data
        elogind_manager_free(m);
#endif // 1

        strv_free(m->kill_only_users);
        strv_free(m->kill_exclude_users);

        free(m->scheduled_shutdown_type);
        free(m->scheduled_shutdown_tty);
        free(m->wall_message);
#if 0 /// UNNEEDED by elogind
        free(m->action_job);
#endif // 0
        free(m);
}

static int manager_enumerate_devices(Manager *m) {
        struct udev_list_entry *item = NULL, *first = NULL;
        _cleanup_(udev_enumerate_unrefp) struct udev_enumerate *e = NULL;
        int r;

        assert(m);

        /* Loads devices from udev and creates seats for them as
         * necessary */

        e = udev_enumerate_new(m->udev);
        if (!e)
                return -ENOMEM;

        r = udev_enumerate_add_match_tag(e, "master-of-seat");
        if (r < 0)
                return r;

        r = udev_enumerate_add_match_is_initialized(e);
        if (r < 0)
                return r;

        r = udev_enumerate_scan_devices(e);
        if (r < 0)
                return r;

        first = udev_enumerate_get_list_entry(e);
        udev_list_entry_foreach(item, first) {
                _cleanup_(udev_device_unrefp) struct udev_device *d = NULL;
                int k;

                d = udev_device_new_from_syspath(m->udev, udev_list_entry_get_name(item));
                if (!d)
                        return -ENOMEM;

                k = manager_process_seat_device(m, d);
                if (k < 0)
                        r = k;
        }

        return r;
}

static int manager_enumerate_buttons(Manager *m) {
        _cleanup_(udev_enumerate_unrefp) struct udev_enumerate *e = NULL;
        struct udev_list_entry *item = NULL, *first = NULL;
        int r;

        assert(m);

        /* Loads buttons from udev */

        if (manager_all_buttons_ignored(m))
                return 0;

        e = udev_enumerate_new(m->udev);
        if (!e)
                return -ENOMEM;

        r = udev_enumerate_add_match_subsystem(e, "input");
        if (r < 0)
                return r;

        r = udev_enumerate_add_match_tag(e, "power-switch");
        if (r < 0)
                return r;

        r = udev_enumerate_add_match_is_initialized(e);
        if (r < 0)
                return r;

        r = udev_enumerate_scan_devices(e);
        if (r < 0)
                return r;

        first = udev_enumerate_get_list_entry(e);
        udev_list_entry_foreach(item, first) {
                _cleanup_(udev_device_unrefp) struct udev_device *d = NULL;
                int k;

                d = udev_device_new_from_syspath(m->udev, udev_list_entry_get_name(item));
                if (!d)
                        return -ENOMEM;

                k = manager_process_button_device(m, d);
                if (k < 0)
                        r = k;
        }

        return r;
}

static int manager_enumerate_seats(Manager *m) {
        _cleanup_closedir_ DIR *d = NULL;
        struct dirent *de;
        int r = 0;

        assert(m);

        /* This loads data about seats stored on disk, but does not
         * actually create any seats. Removes data of seats that no
         * longer exist. */

        d = opendir("/run/systemd/seats");
        if (!d) {
                if (errno == ENOENT)
                        return 0;

                return log_error_errno(errno, "Failed to open /run/systemd/seats: %m");
        }

        FOREACH_DIRENT(de, d, return -errno) {
                Seat *s;
                int k;

                if (!dirent_is_file(de))
                        continue;

                s = hashmap_get(m->seats, de->d_name);
                if (!s) {
                        if (unlinkat(dirfd(d), de->d_name, 0) < 0)
                                log_warning("Failed to remove /run/systemd/seats/%s: %m",
                                            de->d_name);
                        continue;
                }

                k = seat_load(s);
                if (k < 0)
                        r = k;
        }

        return r;
}

static int manager_enumerate_linger_users(Manager *m) {
        _cleanup_closedir_ DIR *d = NULL;
        struct dirent *de;
        int r = 0;

        assert(m);

        d = opendir("/var/lib/elogind/linger");
        if (!d) {
                if (errno == ENOENT)
                        return 0;

                return log_error_errno(errno, "Failed to open /var/lib/elogind/linger/: %m");
        }

        FOREACH_DIRENT(de, d, return -errno) {
                int k;

                if (!dirent_is_file(de))
                        continue;

                k = manager_add_user_by_name(m, de->d_name, NULL);
                if (k < 0) {
                        log_notice_errno(k, "Couldn't add lingering user %s: %m", de->d_name);
                        r = k;
                }
        }

        return r;
}

static int manager_enumerate_users(Manager *m) {
        _cleanup_closedir_ DIR *d = NULL;
        struct dirent *de;
        int r, k;

        assert(m);

        /* Add lingering users */
        r = manager_enumerate_linger_users(m);

        /* Read in user data stored on disk */
        d = opendir("/run/systemd/users");
        if (!d) {
                if (errno == ENOENT)
                        return 0;

                return log_error_errno(errno, "Failed to open /run/systemd/users: %m");
        }

        FOREACH_DIRENT(de, d, return -errno) {
                User *u;

                if (!dirent_is_file(de))
                        continue;

                k = manager_add_user_by_name(m, de->d_name, &u);
                if (k < 0) {
                        log_error_errno(k, "Failed to add user by file name %s: %m", de->d_name);

                        r = k;
                        continue;
                }

                user_add_to_gc_queue(u);

                k = user_load(u);
                if (k < 0)
                        r = k;
        }

        return r;
}

static int parse_fdname(const char *fdname, char **session_id, dev_t *dev) {
        _cleanup_strv_free_ char **parts = NULL;
        _cleanup_free_ char *id = NULL;
        unsigned int major, minor;
        int r;

        parts = strv_split(fdname, "-");
        if (!parts)
                return -ENOMEM;
        if (strv_length(parts) != 5)
                return -EINVAL;

        if (!streq(parts[0], "session"))
                return -EINVAL;
        id = strdup(parts[1]);
        if (!id)
                return -ENOMEM;

        if (!streq(parts[2], "device"))
                return -EINVAL;
        r = safe_atou(parts[3], &major) ||
            safe_atou(parts[4], &minor);
        if (r < 0)
                return r;

        *dev = makedev(major, minor);
        *session_id = TAKE_PTR(id);

        return 0;
}

static int manager_attach_fds(Manager *m) {
        _cleanup_strv_free_ char **fdnames = NULL;
        int n, i, fd;

        /* Upon restart, PID1 will send us back all fds of session devices
         * that we previously opened. Each file descriptor is associated
         * with a given session. The session ids are passed through FDNAMES. */

        n = sd_listen_fds_with_names(true, &fdnames);
        if (n <= 0)
                return n;

        for (i = 0; i < n; i++) {
                _cleanup_free_ char *id = NULL;
                dev_t dev;
                struct stat st;
                SessionDevice *sd;
                Session *s;
                int r;

                fd = SD_LISTEN_FDS_START + i;

                r = parse_fdname(fdnames[i], &id, &dev);
                if (r < 0) {
                        log_debug_errno(r, "Failed to parse fd name %s: %m", fdnames[i]);
                        close_nointr(fd);
                        continue;
                }

                s = hashmap_get(m->sessions, id);
                if (!s) {
                        /* If the session doesn't exist anymore, the associated session
                         * device attached to this fd doesn't either. Let's simply close
                         * this fd. */
                        log_debug("Failed to attach fd for unknown session: %s", id);
                        close_nointr(fd);
                        continue;
                }

                if (fstat(fd, &st) < 0) {
                        /* The device is allowed to go away at a random point, in which
                         * case fstat failing is expected. */
                        log_debug_errno(errno, "Failed to stat device fd for session %s: %m", id);
                        close_nointr(fd);
                        continue;
                }

                if (!S_ISCHR(st.st_mode) || st.st_rdev != dev) {
                        log_debug("Device fd doesn't point to the expected character device node");
                        close_nointr(fd);
                        continue;
                }

                sd = hashmap_get(s->devices, &dev);
                if (!sd) {
                        /* Weird, we got an fd for a session device which wasn't
                         * recorded in the session state file... */
                        log_warning("Got fd for missing session device [%u:%u] in session %s",
                                    major(dev), minor(dev), s->id);
                        close_nointr(fd);
                        continue;
                }

                log_debug("Attaching fd to session device [%u:%u] for session %s",
                          major(dev), minor(dev), s->id);

                session_device_attach_fd(sd, fd, s->was_active);
        }

        return 0;
}

static int manager_enumerate_sessions(Manager *m) {
        _cleanup_closedir_ DIR *d = NULL;
        struct dirent *de;
        int r = 0, k;

        assert(m);

        /* Read in session data stored on disk */
        d = opendir("/run/systemd/sessions");
        if (!d) {
                if (errno == ENOENT)
                        return 0;

                return log_error_errno(errno, "Failed to open /run/systemd/sessions: %m");
        }

        FOREACH_DIRENT(de, d, return -errno) {
                struct Session *s;

                if (!dirent_is_file(de))
                        continue;

                if (!session_id_valid(de->d_name)) {
                        log_warning("Invalid session file name '%s', ignoring.", de->d_name);
                        r = -EINVAL;
                        continue;
                }

                k = manager_add_session(m, de->d_name, &s);
                if (k < 0) {
                        log_error_errno(k, "Failed to add session by file name %s: %m", de->d_name);
                        r = k;
                        continue;
                }

                session_add_to_gc_queue(s);

                k = session_load(s);
                if (k < 0)
                        r = k;
        }

        /* We might be restarted and PID1 could have sent us back the
         * session device fds we previously saved. */
        k = manager_attach_fds(m);
        if (k < 0)
                log_warning_errno(k, "Failed to reattach session device fds: %m");

        return r;
}

static int manager_enumerate_inhibitors(Manager *m) {
        _cleanup_closedir_ DIR *d = NULL;
        struct dirent *de;
        int r = 0;

        assert(m);

        d = opendir("/run/systemd/inhibit");
        if (!d) {
                if (errno == ENOENT)
                        return 0;

                return log_error_errno(errno, "Failed to open /run/systemd/inhibit: %m");
        }

        FOREACH_DIRENT(de, d, return -errno) {
                int k;
                Inhibitor *i;

                if (!dirent_is_file(de))
                        continue;

                k = manager_add_inhibitor(m, de->d_name, &i);
                if (k < 0) {
                        log_notice_errno(k, "Couldn't add inhibitor %s: %m", de->d_name);
                        r = k;
                        continue;
                }

                k = inhibitor_load(i);
                if (k < 0)
                        r = k;
        }

        return r;
}

static int manager_dispatch_seat_udev(sd_event_source *s, int fd, uint32_t revents, void *userdata) {
        _cleanup_(udev_device_unrefp) struct udev_device *d = NULL;
        Manager *m = userdata;

        assert(m);

        d = udev_monitor_receive_device(m->udev_seat_monitor);
        if (!d)
                return -ENOMEM;

        manager_process_seat_device(m, d);
        return 0;
}

static int manager_dispatch_device_udev(sd_event_source *s, int fd, uint32_t revents, void *userdata) {
        _cleanup_(udev_device_unrefp) struct udev_device *d = NULL;
        Manager *m = userdata;

        assert(m);

        d = udev_monitor_receive_device(m->udev_device_monitor);
        if (!d)
                return -ENOMEM;

        manager_process_seat_device(m, d);
        return 0;
}

#if 0 /// UNNEEDED by elogind
static int manager_dispatch_vcsa_udev(sd_event_source *s, int fd, uint32_t revents, void *userdata) {
        _cleanup_(udev_device_unrefp) struct udev_device *d = NULL;
        Manager *m = userdata;
        const char *name;

        assert(m);

        d = udev_monitor_receive_device(m->udev_vcsa_monitor);
        if (!d)
                return -ENOMEM;

        name = udev_device_get_sysname(d);

        /* Whenever a VCSA device is removed try to reallocate our
         * VTs, to make sure our auto VTs never go away. */

        if (name && startswith(name, "vcsa") && streq_ptr(udev_device_get_action(d), "remove"))
                seat_preallocate_vts(m->seat0);

        return 0;
}
#endif // 0

static int manager_dispatch_button_udev(sd_event_source *s, int fd, uint32_t revents, void *userdata) {
        _cleanup_(udev_device_unrefp) struct udev_device *d = NULL;
        Manager *m = userdata;

        assert(m);

        d = udev_monitor_receive_device(m->udev_button_monitor);
        if (!d)
                return -ENOMEM;

        manager_process_button_device(m, d);
        return 0;
}

static int manager_dispatch_console(sd_event_source *s, int fd, uint32_t revents, void *userdata) {
        Manager *m = userdata;

        assert(m);
        assert(m->seat0);
        assert(m->console_active_fd == fd);

        seat_read_active_vt(m->seat0);
        return 0;
}

#if 0 /// UNNEEDED by elogind
static int manager_reserve_vt(Manager *m) {
        _cleanup_free_ char *p = NULL;

        assert(m);

        if (m->reserve_vt <= 0)
                return 0;

        if (asprintf(&p, "/dev/tty%u", m->reserve_vt) < 0)
                return log_oom();

        m->reserve_vt_fd = open(p, O_RDWR|O_NOCTTY|O_CLOEXEC|O_NONBLOCK);
        if (m->reserve_vt_fd < 0) {

                /* Don't complain on VT-less systems */
                if (errno != ENOENT)
                        log_warning_errno(errno, "Failed to pin reserved VT: %m");
                return -errno;
        }

        return 0;
}
#endif // 0

static int manager_connect_bus(Manager *m) {
        int r;

        assert(m);
        assert(!m->bus);

        r = sd_bus_default_system(&m->bus);
        if (r < 0)
                return log_error_errno(r, "Failed to connect to system bus: %m");

        r = sd_bus_add_object_vtable(m->bus, NULL, "/org/freedesktop/login1", "org.freedesktop.login1.Manager", manager_vtable, m);
        if (r < 0)
                return log_error_errno(r, "Failed to add manager object vtable: %m");

        r = sd_bus_add_fallback_vtable(m->bus, NULL, "/org/freedesktop/login1/seat", "org.freedesktop.login1.Seat", seat_vtable, seat_object_find, m);
        if (r < 0)
                return log_error_errno(r, "Failed to add seat object vtable: %m");

        r = sd_bus_add_node_enumerator(m->bus, NULL, "/org/freedesktop/login1/seat", seat_node_enumerator, m);
        if (r < 0)
                return log_error_errno(r, "Failed to add seat enumerator: %m");

        r = sd_bus_add_fallback_vtable(m->bus, NULL, "/org/freedesktop/login1/session", "org.freedesktop.login1.Session", session_vtable, session_object_find, m);
        if (r < 0)
                return log_error_errno(r, "Failed to add session object vtable: %m");

        r = sd_bus_add_node_enumerator(m->bus, NULL, "/org/freedesktop/login1/session", session_node_enumerator, m);
        if (r < 0)
                return log_error_errno(r, "Failed to add session enumerator: %m");

        r = sd_bus_add_fallback_vtable(m->bus, NULL, "/org/freedesktop/login1/user", "org.freedesktop.login1.User", user_vtable, user_object_find, m);
        if (r < 0)
                return log_error_errno(r, "Failed to add user object vtable: %m");

        r = sd_bus_add_node_enumerator(m->bus, NULL, "/org/freedesktop/login1/user", user_node_enumerator, m);
        if (r < 0)
                return log_error_errno(r, "Failed to add user enumerator: %m");

#if 0 /// elogind does not support systemd as PID 1
        r = sd_bus_match_signal_async(
                        m->bus,
                        NULL,
                        "org.freedesktop.systemd1",
                        "/org/freedesktop/systemd1",
                        "org.freedesktop.systemd1.Manager",
                        "JobRemoved",
                        match_job_removed, NULL, m);
        if (r < 0)
                return log_error_errno(r, "Failed to request match for JobRemoved: %m");

        r = sd_bus_match_signal_async(
                        m->bus,
                        NULL,
                        "org.freedesktop.systemd1",
                        "/org/freedesktop/systemd1",
                        "org.freedesktop.systemd1.Manager",
                        "UnitRemoved",
                        match_unit_removed, NULL, m);
        if (r < 0)
                return log_error_errno(r, "Failed to request match for UnitRemoved: %m");

        r = sd_bus_match_signal_async(
                        m->bus,
                        NULL,
                        "org.freedesktop.systemd1",
                        NULL,
                        "org.freedesktop.DBus.Properties",
                        "PropertiesChanged",
                        match_properties_changed, NULL, m);
        if (r < 0)
                return log_error_errno(r, "Failed to request match for PropertiesChanged: %m");

        r = sd_bus_match_signal_async(
                        m->bus,
                        NULL,
                        "org.freedesktop.systemd1",
                        "/org/freedesktop/systemd1",
                        "org.freedesktop.systemd1.Manager",
                        "Reloading",
                        match_reloading, NULL, m);
        if (r < 0)
                return log_error_errno(r, "Failed to request match for Reloading: %m");

        r = sd_bus_call_method_async(
                        m->bus,
                        NULL,
                        "org.freedesktop.systemd1",
                        "/org/freedesktop/systemd1",
                        "org.freedesktop.systemd1.Manager",
                        "Subscribe",
                        NULL, NULL,
                        NULL);
#endif // 0
        if (r < 0)
                return log_error_errno(r, "Failed to enable subscription: %m");

        r = sd_bus_request_name_async(m->bus, NULL, "org.freedesktop.login1", 0, NULL, NULL);
        if (r < 0)
                return log_error_errno(r, "Failed to request name: %m");

        r = sd_bus_attach_event(m->bus, m->event, SD_EVENT_PRIORITY_NORMAL);
        if (r < 0)
                return log_error_errno(r, "Failed to attach bus to event loop: %m");

#if 0 /// elogind has to setup its release agent
        return 0;
#else
        r = elogind_setup_cgroups_agent(m);

        return r;
#endif // 0
}

static int manager_vt_switch(sd_event_source *src, const struct signalfd_siginfo *si, void *data) {
        Manager *m = data;
        Session *active, *iter;

        /*
         * We got a VT-switch signal and we have to acknowledge it immediately.
         * Preferably, we'd just use m->seat0->active->vtfd, but unfortunately,
         * old user-space might run multiple sessions on a single VT, *sigh*.
         * Therefore, we have to iterate all sessions and find one with a vtfd
         * on the requested VT.
         * As only VTs with active controllers have VT_PROCESS set, our current
         * notion of the active VT might be wrong (for instance if the switch
         * happens while we setup VT_PROCESS). Therefore, read the current VT
         * first and then use s->active->vtnr as reference. Note that this is
         * not racy, as no further VT-switch can happen as long as we're in
         * synchronous VT_PROCESS mode.
         */

        assert(m->seat0);
        seat_read_active_vt(m->seat0);

        active = m->seat0->active;
        if (!active || active->vtnr < 1) {
                log_warning("Received VT_PROCESS signal without a registered session on that VT.");
                return 0;
        }

        if (active->vtfd >= 0) {
                session_leave_vt(active);
        } else {
                LIST_FOREACH(sessions_by_seat, iter, m->seat0->sessions) {
                        if (iter->vtnr == active->vtnr && iter->vtfd >= 0) {
                                session_leave_vt(iter);
                                break;
                        }
                }
        }

        return 0;
}

static int manager_connect_console(Manager *m) {
        int r;

        assert(m);
        assert(m->console_active_fd < 0);

        /* On certain architectures (S390 and Xen, and containers),
           /dev/tty0 does not exist, so don't fail if we can't open
           it. */
        if (access("/dev/tty0", F_OK) < 0)
                return 0;

        m->console_active_fd = open("/sys/class/tty/tty0/active", O_RDONLY|O_NOCTTY|O_CLOEXEC);
        if (m->console_active_fd < 0) {

                /* On some systems the device node /dev/tty0 may exist
                 * even though /sys/class/tty/tty0 does not. */
                if (errno == ENOENT)
                        return 0;

                return log_error_errno(errno, "Failed to open /sys/class/tty/tty0/active: %m");
        }

        r = sd_event_add_io(m->event, &m->console_active_event_source, m->console_active_fd, 0, manager_dispatch_console, m);
        if (r < 0) {
                log_error("Failed to watch foreground console");
                return r;
        }

        /*
         * SIGRTMIN is used as global VT-release signal, SIGRTMIN + 1 is used
         * as VT-acquire signal. We ignore any acquire-events (yes, we still
         * have to provide a valid signal-number for it!) and acknowledge all
         * release events immediately.
         */

        if (SIGRTMIN + 1 > SIGRTMAX) {
                log_error("Not enough real-time signals available: %u-%u", SIGRTMIN, SIGRTMAX);
                return -EINVAL;
        }

        assert_se(ignore_signals(SIGRTMIN + 1, -1) >= 0);
        assert_se(sigprocmask_many(SIG_BLOCK, NULL, SIGRTMIN, -1) >= 0);

        r = sd_event_add_signal(m->event, NULL, SIGRTMIN, manager_vt_switch, m);
        if (r < 0)
                return r;

        return 0;
}

static int manager_connect_udev(Manager *m) {
        int r;

        assert(m);
        assert(!m->udev_seat_monitor);
        assert(!m->udev_device_monitor);
        assert(!m->udev_vcsa_monitor);
        assert(!m->udev_button_monitor);

        m->udev_seat_monitor = udev_monitor_new_from_netlink(m->udev, "udev");
        if (!m->udev_seat_monitor)
                return -ENOMEM;

        r = udev_monitor_filter_add_match_tag(m->udev_seat_monitor, "master-of-seat");
        if (r < 0)
                return r;

        r = udev_monitor_enable_receiving(m->udev_seat_monitor);
        if (r < 0)
                return r;

        r = sd_event_add_io(m->event, &m->udev_seat_event_source, udev_monitor_get_fd(m->udev_seat_monitor), EPOLLIN, manager_dispatch_seat_udev, m);
        if (r < 0)
                return r;

        m->udev_device_monitor = udev_monitor_new_from_netlink(m->udev, "udev");
        if (!m->udev_device_monitor)
                return -ENOMEM;

        r = udev_monitor_filter_add_match_subsystem_devtype(m->udev_device_monitor, "input", NULL);
        if (r < 0)
                return r;

        r = udev_monitor_filter_add_match_subsystem_devtype(m->udev_device_monitor, "graphics", NULL);
        if (r < 0)
                return r;

        r = udev_monitor_filter_add_match_subsystem_devtype(m->udev_device_monitor, "drm", NULL);
        if (r < 0)
                return r;

        r = udev_monitor_enable_receiving(m->udev_device_monitor);
        if (r < 0)
                return r;

        r = sd_event_add_io(m->event, &m->udev_device_event_source, udev_monitor_get_fd(m->udev_device_monitor), EPOLLIN, manager_dispatch_device_udev, m);
        if (r < 0)
                return r;

        /* Don't watch keys if nobody cares */
        if (!manager_all_buttons_ignored(m)) {
                m->udev_button_monitor = udev_monitor_new_from_netlink(m->udev, "udev");
                if (!m->udev_button_monitor)
                        return -ENOMEM;

                r = udev_monitor_filter_add_match_tag(m->udev_button_monitor, "power-switch");
                if (r < 0)
                        return r;

                r = udev_monitor_filter_add_match_subsystem_devtype(m->udev_button_monitor, "input", NULL);
                if (r < 0)
                        return r;

                r = udev_monitor_enable_receiving(m->udev_button_monitor);
                if (r < 0)
                        return r;

                r = sd_event_add_io(m->event, &m->udev_button_event_source, udev_monitor_get_fd(m->udev_button_monitor), EPOLLIN, manager_dispatch_button_udev, m);
                if (r < 0)
                        return r;
        }

        /* Don't bother watching VCSA devices, if nobody cares */
#if 0 /// elogind does not support autospawning of vts
        if (m->n_autovts > 0 && m->console_active_fd >= 0) {

                m->udev_vcsa_monitor = udev_monitor_new_from_netlink(m->udev, "udev");
                if (!m->udev_vcsa_monitor)
                        return -ENOMEM;

                r = udev_monitor_filter_add_match_subsystem_devtype(m->udev_vcsa_monitor, "vc", NULL);
                if (r < 0)
                        return r;

                r = udev_monitor_enable_receiving(m->udev_vcsa_monitor);
                if (r < 0)
                        return r;

                r = sd_event_add_io(m->event, &m->udev_vcsa_event_source, udev_monitor_get_fd(m->udev_vcsa_monitor), EPOLLIN, manager_dispatch_vcsa_udev, m);
                if (r < 0)
                        return r;
        }
#endif // 0

        return 0;
}

static void manager_gc(Manager *m, bool drop_not_started) {
        Seat *seat;
        Session *session;
        User *user;

        assert(m);

        while ((seat = m->seat_gc_queue)) {
                LIST_REMOVE(gc_queue, m->seat_gc_queue, seat);
                seat->in_gc_queue = false;

                if (seat_may_gc(seat, drop_not_started)) {
                        seat_stop(seat, false);
                        seat_free(seat);
                }
        }

        while ((session = m->session_gc_queue)) {
                LIST_REMOVE(gc_queue, m->session_gc_queue, session);
                session->in_gc_queue = false;

                /* First, if we are not closing yet, initiate stopping */
                if (session_may_gc(session, drop_not_started) &&
                    session_get_state(session) != SESSION_CLOSING)
                        session_stop(session, false);

                /* Normally, this should make the session referenced
                 * again, if it doesn't then let's get rid of it
                 * immediately */
                if (session_may_gc(session, drop_not_started)) {
                        session_finalize(session);
                        session_free(session);
                }
        }

        while ((user = m->user_gc_queue)) {
                LIST_REMOVE(gc_queue, m->user_gc_queue, user);
                user->in_gc_queue = false;

                /* First step: queue stop jobs */
                if (user_may_gc(user, drop_not_started))
                        user_stop(user, false);

                /* Second step: finalize user */
                if (user_may_gc(user, drop_not_started)) {
                        user_finalize(user);
                        user_free(user);
                }
        }
}

static int manager_dispatch_idle_action(sd_event_source *s, uint64_t t, void *userdata) {
        Manager *m = userdata;
        struct dual_timestamp since;
        usec_t n, elapse;
        int r;

        assert(m);

        if (m->idle_action == HANDLE_IGNORE ||
            m->idle_action_usec <= 0)
                return 0;

        n = now(CLOCK_MONOTONIC);

        r = manager_get_idle_hint(m, &since);
        if (r <= 0)
                /* Not idle. Let's check if after a timeout it might be idle then. */
                elapse = n + m->idle_action_usec;
        else {
                /* Idle! Let's see if it's time to do something, or if
                 * we shall sleep for longer. */

                if (n >= since.monotonic + m->idle_action_usec &&
                    (m->idle_action_not_before_usec <= 0 || n >= m->idle_action_not_before_usec + m->idle_action_usec)) {
                        log_info("System idle. Taking action.");

                        manager_handle_action(m, 0, m->idle_action, false, false);
                        m->idle_action_not_before_usec = n;
                }

                elapse = MAX(since.monotonic, m->idle_action_not_before_usec) + m->idle_action_usec;
        }

        if (!m->idle_action_event_source) {

                r = sd_event_add_time(
                                m->event,
                                &m->idle_action_event_source,
                                CLOCK_MONOTONIC,
                                elapse, USEC_PER_SEC*30,
                                manager_dispatch_idle_action, m);
                if (r < 0)
                        return log_error_errno(r, "Failed to add idle event source: %m");

                r = sd_event_source_set_priority(m->idle_action_event_source, SD_EVENT_PRIORITY_IDLE+10);
                if (r < 0)
                        return log_error_errno(r, "Failed to set idle event source priority: %m");
        } else {
                r = sd_event_source_set_time(m->idle_action_event_source, elapse);
                if (r < 0)
                        return log_error_errno(r, "Failed to set idle event timer: %m");

                r = sd_event_source_set_enabled(m->idle_action_event_source, SD_EVENT_ONESHOT);
                if (r < 0)
                        return log_error_errno(r, "Failed to enable idle event timer: %m");
        }

        return 0;
}

#if 0 /// elogind parses its own config file
#else
         const char* logind_conf = getenv("ELOGIND_CONF_FILE");

         assert(m);

         if (!logind_conf)
                 logind_conf = PKGSYSCONFDIR "/logind.conf";

         return config_parse(NULL, logind_conf, NULL, "Login\0Sleep\0",
                             config_item_perf_lookup, logind_gperf_lookup,
#endif // 0
static int manager_dispatch_reload_signal(sd_event_source *s, const struct signalfd_siginfo *si, void *userdata) {
        Manager *m = userdata;
        int r;

        manager_reset_config(m);
        r = manager_parse_config_file(m);
        if (r < 0)
                log_warning_errno(r, "Failed to parse config file, using defaults: %m");
        else
                log_info("Config file reloaded.");

#if 1 /// elogind needs an Add-On for sleep configuration
        elogind_manager_reset_config(m);
#endif // 1
        return 0;
}

static int manager_startup(Manager *m) {
        int r;
        Seat *seat;
        Session *session;
        User *user;
        Button *button;
        Inhibitor *inhibitor;
        Iterator i;

        assert(m);

        assert_se(sigprocmask_many(SIG_SETMASK, NULL, SIGHUP, -1) >= 0);

        r = sd_event_add_signal(m->event, NULL, SIGHUP, manager_dispatch_reload_signal, m);
        if (r < 0)
                return log_error_errno(r, "Failed to register SIGHUP handler: %m");

#if 1 /// elogind needs some extra preparations before connecting...
        elogind_manager_startup(m);
#endif // 1
        /* Connect to console */
        r = manager_connect_console(m);
        if (r < 0)
                return r;

        /* Connect to udev */
        r = manager_connect_udev(m);
        if (r < 0)
                return log_error_errno(r, "Failed to create udev watchers: %m");

        /* Connect to the bus */
        r = manager_connect_bus(m);
        if (r < 0)
                return r;

        /* Instantiate magic seat 0 */
        r = manager_add_seat(m, "seat0", &m->seat0);
        if (r < 0)
                return log_error_errno(r, "Failed to add seat0: %m");

        r = manager_set_lid_switch_ignore(m, 0 + m->holdoff_timeout_usec);
        if (r < 0)
                log_warning_errno(r, "Failed to set up lid switch ignore event source: %m");

        /* Deserialize state */
        r = manager_enumerate_devices(m);
        if (r < 0)
                log_warning_errno(r, "Device enumeration failed: %m");

        r = manager_enumerate_seats(m);
        if (r < 0)
                log_warning_errno(r, "Seat enumeration failed: %m");

        r = manager_enumerate_users(m);
        if (r < 0)
                log_warning_errno(r, "User enumeration failed: %m");

        r = manager_enumerate_sessions(m);
        if (r < 0)
                log_warning_errno(r, "Session enumeration failed: %m");

        r = manager_enumerate_inhibitors(m);
        if (r < 0)
                log_warning_errno(r, "Inhibitor enumeration failed: %m");

        r = manager_enumerate_buttons(m);
        if (r < 0)
                log_warning_errno(r, "Button enumeration failed: %m");

        /* Remove stale objects before we start them */
        manager_gc(m, false);

        /* Reserve the special reserved VT */
#if 0 /// elogind does not support autospawning of vts
        manager_reserve_vt(m);
#endif // 0

        /* And start everything */
        HASHMAP_FOREACH(seat, m->seats, i)
                seat_start(seat);

        HASHMAP_FOREACH(user, m->users, i)
                user_start(user);

        HASHMAP_FOREACH(session, m->sessions, i)
                session_start(session, NULL);

        HASHMAP_FOREACH(inhibitor, m->inhibitors, i)
                inhibitor_start(inhibitor);

        HASHMAP_FOREACH(button, m->buttons, i)
                button_check_switches(button);

        manager_dispatch_idle_action(NULL, 0, m);

        return 0;
}

static int manager_run(Manager *m) {
        int r;

        assert(m);

        for (;;) {
                r = sd_event_get_state(m->event);
                if (r < 0)
                        return r;
                if (r == SD_EVENT_FINISHED)
                        return 0;

                manager_gc(m, true);

                r = manager_dispatch_delayed(m, false);
                if (r < 0)
                        return r;
                if (r > 0)
                        continue;

                r = sd_event_run(m->event, (uint64_t) -1);
                if (r < 0)
                        return r;
        }
}

int main(int argc, char *argv[]) {
        Manager *m = NULL;
        int r;

#if 1 /// perform extra checks for elogind startup
        r = elogind_startup(argc, argv);
        if (r)
                return r < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
#endif // 1
        elogind_set_program_name(argv[0]);
        log_set_target(LOG_TARGET_AUTO);
        log_set_facility(LOG_AUTH);
        log_parse_environment();
#if ENABLE_DEBUG_ELOGIND
        log_set_max_level(LOG_DEBUG);
        log_set_target(LOG_TARGET_SYSLOG_OR_KMSG);
#endif // ENABLE_DEBUG_ELOGIND
        log_open();

        umask(0022);

#if 0 /// elogind has some extra functionality at startup, argc can be != 1
        if (argc != 1) {
                log_error("This program takes no arguments.");
                r = -EINVAL;
                goto finish;
        }
#endif // 0

        r = mac_selinux_init();
        if (r < 0) {
                log_error_errno(r, "Could not initialize labelling: %m");
                goto finish;
        }

        /* Always create the directories people can create inotify
         * watches in. Note that some applications might check for the
         * existence of /run/systemd/seats/ to determine whether
         * logind is available, so please always make sure this check
         * stays in. */
#if 0 /// elogind can not rely on systemd to help, so we need a bit more effort than this
        mkdir_label("/run/systemd/seats", 0755);
        mkdir_label("/run/systemd/users", 0755);
        mkdir_label("/run/systemd/sessions", 0755);
#else
        r = mkdir_label("/run/systemd", 0755);
        if ( (r < 0) && (-EEXIST != r) )
                return log_error_errno(r, "Failed to create /run/systemd : %m");
        r = mkdir_label("/run/systemd/seats", 0755);
        if ( r < 0 && (-EEXIST != r) )
                return log_error_errno(r, "Failed to create /run/systemd/seats : %m");
        r = mkdir_label("/run/systemd/users", 0755);
        if ( r < 0 && (-EEXIST != r) )
                return log_error_errno(r, "Failed to create /run/systemd/users : %m");
        r = mkdir_label("/run/systemd/sessions", 0755);
        if ( r < 0 && (-EEXIST != r) )
                return log_error_errno(r, "Failed to create /run/systemd/sessions : %m");
        r = mkdir_label("/run/systemd/machines", 0755);
        if ( r < 0 && (-EEXIST != r) )
                return log_error_errno(r, "Failed to create /run/systemd/machines : %m");
#endif // 0

        m = manager_new();
        if (!m) {
                r = log_oom();
                goto finish;
        }

        manager_parse_config_file(m);

#if 1 /// elogind needs an Add-On for sleep configuration
        elogind_manager_reset_config(m);
#endif // 1
        r = manager_startup(m);
        if (r < 0) {
                log_error_errno(r, "Failed to fully start up daemon: %m");
                goto finish;
        }

        log_debug("elogind running as pid "PID_FMT, getpid_cached());

        sd_notify(false,
                  "READY=1\n"
                  "STATUS=Processing requests...");

        r = manager_run(m);

        log_debug("elogind stopped as pid "PID_FMT, getpid_cached());

finish:
        sd_notify(false,
                  "STOPPING=1\n"
                  "STATUS=Shutting down...");

        manager_free(m);

        return r < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
