/* SPDX-License-Identifier: LGPL-2.1+ */

#include <errno.h>
#include <string.h>
#include <unistd.h>

#if HAVE_AUDIT
#include <libaudit.h>
#endif

#include "sd-bus.h"

//#include "alloc-util.h"
#include "bus-error.h"
//#include "bus-util.h"
//#include "format-util.h"
//#include "log.h"
//#include "macro.h"
#include "process-util.h"
//#include "special.h"
#include "strv.h"
//#include "unit-name.h"
//#include "util.h"
#include "utmp-wtmp.h"

/// Additional includes needed by elogind
#include "string-util.h"
#include "time-util.h"
#include "update-utmp.h"
typedef struct Context {
        sd_bus *bus;
#if HAVE_AUDIT
        int audit_fd;
#endif
} Context;

#if 0 /// UNNEEDED by elogind
static usec_t get_startup_time(Context *c) {
        _cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
        usec_t t = 0;
        int r;

        assert(c);

        r = sd_bus_get_property_trivial(
                        c->bus,
                        "org.freedesktop.systemd1",
                        "/org/freedesktop/systemd1",
                        "org.freedesktop.systemd1.Manager",
                        "UserspaceTimestamp",
                        &error,
                        't', &t);
        if (r < 0) {
                log_error_errno(r, "Failed to get timestamp: %s", bus_error_message(&error, r));
                return 0;
        }

        return t;
}

static int get_current_runlevel(Context *c) {
        static const struct {
                const int runlevel;
                const char *special;
        } table[] = {
                /* The first target of this list that is active or has
                 * a job scheduled wins. We prefer runlevels 5 and 3
                 * here over the others, since these are the main
                 * runlevels used on Fedora. It might make sense to
                 * change the order on some distributions. */
                { '5', SPECIAL_GRAPHICAL_TARGET  },
                { '3', SPECIAL_MULTI_USER_TARGET },
                { '1', SPECIAL_RESCUE_TARGET     },
        };

        _cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
        int r;
        unsigned i;

        assert(c);

        for (i = 0; i < ELEMENTSOF(table); i++) {
                _cleanup_free_ char *state = NULL, *path = NULL;

                path = unit_dbus_path_from_name(table[i].special);
                if (!path)
                        return log_oom();

                r = sd_bus_get_property_string(
                                c->bus,
                                "org.freedesktop.systemd1",
                                path,
                                "org.freedesktop.systemd1.Unit",
                                "ActiveState",
                                &error,
                                &state);
                if (r < 0)
                        return log_warning_errno(r, "Failed to get state: %s", bus_error_message(&error, r));

                if (STR_IN_SET(state, "active", "reloading"))
                        return table[i].runlevel;
        }

        return 0;
}
#endif // 0

static int on_reboot(Context *c) {
        int r = 0, q;
        usec_t t;

        assert(c);

        /* We finished start-up, so let's write the utmp
         * record and send the audit msg */

#if HAVE_AUDIT
        if (c->audit_fd >= 0)
                if (audit_log_user_comm_message(c->audit_fd, AUDIT_SYSTEM_BOOT, "", "systemd-update-utmp", NULL, NULL, NULL, 1) < 0 &&
                    errno != EPERM) {
                        r = log_error_errno(errno, "Failed to send audit message: %m");
                }
#endif

#if 0 /// systemd hasn't started the system, so elogind always uses NOW()
        /* If this call fails it will return 0, which
         * utmp_put_reboot() will then fix to the current time */
        t = get_startup_time(c);
#else
        t = now(CLOCK_REALTIME);
#endif // 0

        q = utmp_put_reboot(t);
        if (q < 0) {
                log_error_errno(q, "Failed to write utmp record: %m");
                r = q;
        }

        return r;
}

static int on_shutdown(Context *c) {
        int r = 0, q;

        assert(c);

        /* We started shut-down, so let's write the utmp
         * record and send the audit msg */

#if HAVE_AUDIT
        if (c->audit_fd >= 0)
                if (audit_log_user_comm_message(c->audit_fd, AUDIT_SYSTEM_SHUTDOWN, "", "systemd-update-utmp", NULL, NULL, NULL, 1) < 0 &&
                    errno != EPERM) {
                        r = log_error_errno(errno, "Failed to send audit message: %m");
                }
#endif

        q = utmp_put_shutdown();
        if (q < 0) {
                log_error_errno(q, "Failed to write utmp record: %m");
                r = q;
        }

        return r;
}

#if 0 /// UNNEEDED by elogind
static int on_runlevel(Context *c) {
        int r = 0, q, previous, runlevel;

        assert(c);

        /* We finished changing runlevel, so let's write the
         * utmp record and send the audit msg */

        /* First, get last runlevel */
        q = utmp_get_runlevel(&previous, NULL);

        if (q < 0) {
                if (!IN_SET(q, -ESRCH, -ENOENT))
                        return log_error_errno(q, "Failed to get current runlevel: %m");

                previous = 0;
        }

        /* Secondly, get new runlevel */
        runlevel = get_current_runlevel(c);

        if (runlevel < 0)
                return runlevel;

        if (previous == runlevel)
                return 0;

#if HAVE_AUDIT
        if (c->audit_fd >= 0) {
                _cleanup_free_ char *s = NULL;

                if (asprintf(&s, "old-level=%c new-level=%c",
                             previous > 0 ? previous : 'N',
                             runlevel > 0 ? runlevel : 'N') < 0)
                        return log_oom();

                if (audit_log_user_comm_message(c->audit_fd, AUDIT_SYSTEM_RUNLEVEL, s, "systemd-update-utmp", NULL, NULL, NULL, 1) < 0 && errno != EPERM)
                        r = log_error_errno(errno, "Failed to send audit message: %m");
        }
#endif

        q = utmp_put_runlevel(runlevel, previous);
        if (q < 0 && !IN_SET(q, -ESRCH, -ENOENT)) {
                log_error_errno(q, "Failed to write utmp record: %m");
                r = q;
        }

        return r;
}
#endif // 0

#if 0 /// elogind needs this to be a callable function
int main(int argc, char *argv[]) {
#else
void update_utmp(int argc, char* argv[]) {
#endif // 0
        Context c = {
#if HAVE_AUDIT
                .audit_fd = -1
#endif
        };
#if 0 /// UNNEEDED by elogind
        int r;

        if (getppid() != 1) {
                log_error("This program should be invoked by init only.");
                return EXIT_FAILURE;
        }

        if (argc != 2) {
                log_error("This program requires one argument.");
                return EXIT_FAILURE;
        }

        log_set_target(LOG_TARGET_AUTO);
        log_parse_environment();
        log_open();

        umask(0022);
#else
        assert(2 == argc);
        assert(argv[1]);
#endif // 0

#if HAVE_AUDIT
        /* If the kernel lacks netlink or audit support,
         * don't worry about it. */
        c.audit_fd = audit_open();
        if (c.audit_fd < 0 && !IN_SET(errno, EAFNOSUPPORT, EPROTONOSUPPORT))
                log_error_errno(errno, "Failed to connect to audit log: %m");
#endif
#if 0 /// UNNEEDED by elogind
        r = bus_connect_system_systemd(&c.bus);
        if (r < 0) {
                log_error_errno(r, "Failed to get D-Bus connection: %m");
                r = -EIO;
                goto finish;
        }

        log_debug("systemd-update-utmp running as pid "PID_FMT, getpid_cached());

        if (streq(argv[1], "reboot"))
                r = on_reboot(&c);
        else if (streq(argv[1], "shutdown"))
                r = on_shutdown(&c);
        else if (streq(argv[1], "runlevel"))
                r = on_runlevel(&c);
        else {
                log_error("Unknown command %s", argv[1]);
                r = -EINVAL;
        }

        log_debug("systemd-update-utmp stopped as pid "PID_FMT, getpid_cached());

finish:
#else
        if (streq(argv[1], "reboot"))
                (void)on_reboot(&c);
        else if (streq(argv[1], "shutdown"))
                (void)on_shutdown(&c);
#endif // 0
#if HAVE_AUDIT
        if (c.audit_fd >= 0)
                audit_close(c.audit_fd);
#endif

#if 0 /// UNNEEDED by elogind
        sd_bus_flush_close_unref(c.bus);
        return r < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
#endif // 0
}
