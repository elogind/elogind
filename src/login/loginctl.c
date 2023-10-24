/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <errno.h>
#include <getopt.h>
#include <locale.h>
#include <unistd.h>

#include "sd-bus.h"

#include "alloc-util.h"
#include "build.h"
#include "bus-error.h"
#include "bus-locator.h"
#include "bus-map-properties.h"
#include "bus-print-properties.h"
//#include "bus-unit-procs.h"
//#include "cgroup-show.h"
#include "cgroup-util.h"
#include "format-table.h"
#include "log.h"
//#include "logs-show.h"
#include "macro.h"
#include "main-func.h"
#include "memory-util.h"
#include "pager.h"
#include "parse-argument.h"
//#include "parse-util.h"
#include "pretty-print.h"
#include "process-util.h"
#include "rlimit-util.h"
#include "sigbus.h"
//#include "signal-util.h"
#include "spawn-polkit-agent.h"
#include "string-table.h"
#include "strv.h"
#include "sysfs-show.h"
#include "terminal-util.h"
//#include "unit-name.h"
#include "user-util.h"
#include "verbs.h"

/// Additional includes needed by elogind
#include "eloginctl.h"
#include "musl_missing.h"

static char **arg_property = NULL;
static BusPrintPropertyFlags arg_print_flags = 0;
static bool arg_full = false;
static PagerFlags arg_pager_flags = 0;
static bool arg_legend = true;
static const char *arg_kill_whom = NULL;
static int arg_signal = SIGTERM;
#if 0 /// UNNEEDED by elogind
static BusTransport arg_transport = BUS_TRANSPORT_LOCAL;
static char *arg_host = NULL;
static bool arg_ask_password = true;
static unsigned arg_lines = 10;
#else // 0
/// Instead we need this:
extern BusTransport arg_transport;
static char *arg_host;
extern bool arg_ask_password;
extern bool arg_dry_run;
extern bool arg_no_wall;
extern usec_t arg_when;
extern bool arg_ignore_inhibitors;
extern elogind_action arg_action;
#if ENABLE_EFI
extern bool arg_firmware_setup;
#endif
#ifdef ENABLE_EFI_TODO /// @todo EFI - needs change to support UEFI boot.
extern usec_t arg_boot_loader_menu;
extern const char* arg_boot_loader_entry;
#endif
#endif // 0
static OutputMode arg_output = OUTPUT_SHORT;

STATIC_DESTRUCTOR_REGISTER(arg_property, strv_freep);

static OutputFlags get_output_flags(void) {

        return
                FLAGS_SET(arg_print_flags, BUS_PRINT_PROPERTY_SHOW_EMPTY) * OUTPUT_SHOW_ALL |
                (arg_full || !on_tty() || pager_have()) * OUTPUT_FULL_WIDTH |
                colors_enabled() * OUTPUT_COLOR;
}

static int get_session_path(sd_bus *bus, const char *session_id, sd_bus_error *error, char **path) {
        _cleanup_(sd_bus_message_unrefp) sd_bus_message *reply = NULL;
        int r;
        char *ans;

        r = bus_call_method(bus, bus_login_mgr, "GetSession", error, &reply, "s", session_id);
        if (r < 0)
                return r;

        r = sd_bus_message_read(reply, "o", &ans);
        if (r < 0)
                return r;

        ans = strdup(ans);
        if (!ans)
                return -ENOMEM;

        *path = ans;
        return 0;
}

static int show_table(Table *table, const char *word) {
        int r;

        assert(table);
        assert(word);

        if (table_get_rows(table) > 1 || OUTPUT_MODE_IS_JSON(arg_output)) {
                r = table_set_sort(table, (size_t) 0);
                if (r < 0)
                        return table_log_sort_error(r);

                table_set_header(table, arg_legend);

                if (OUTPUT_MODE_IS_JSON(arg_output))
                        r = table_print_json(table, NULL, output_mode_to_json_format_flags(arg_output) | JSON_FORMAT_COLOR_AUTO);
                else
                        r = table_print(table, NULL);
                if (r < 0)
                        return table_log_print_error(r);
        }

        if (arg_legend) {
                if (table_get_rows(table) > 1)
                        printf("\n%zu %s listed.\n", table_get_rows(table) - 1, word);
                else
                        printf("No %s.\n", word);
        }

        return 0;
}

static int list_sessions(int argc, char *argv[], void *userdata) {
        _cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
        _cleanup_(sd_bus_message_unrefp) sd_bus_message *reply = NULL;
        _cleanup_(table_unrefp) Table *table = NULL;
        sd_bus *bus = ASSERT_PTR(userdata);
        int r;

        assert(argv);

        pager_open(arg_pager_flags);

        r = bus_call_method(bus, bus_login_mgr, "ListSessions", &error, &reply, NULL);
        if (r < 0)
                return log_error_errno(r, "Failed to list sessions: %s", bus_error_message(&error, r));

        r = sd_bus_message_enter_container(reply, 'a', "(susso)");
        if (r < 0)
                return bus_log_parse_error(r);

        table = table_new("session", "uid", "user", "seat", "tty");
        if (!table)
                return log_oom();

        /* Right-align the first two fields (since they are numeric) */
        (void) table_set_align_percent(table, TABLE_HEADER_CELL(0), 100);
        (void) table_set_align_percent(table, TABLE_HEADER_CELL(1), 100);

        for (;;) {
                _cleanup_(sd_bus_error_free) sd_bus_error error_property = SD_BUS_ERROR_NULL;
                _cleanup_free_ char *tty = NULL;
                const char *id, *user, *seat, *object;
                uint32_t uid;

                r = sd_bus_message_read(reply, "(susso)", &id, &uid, &user, &seat, &object);
                if (r < 0)
                        return bus_log_parse_error(r);
                if (r == 0)
                        break;

                r = sd_bus_get_property_string(bus,
                                               "org.freedesktop.login1",
                                               object,
                                               "org.freedesktop.login1.Session",
                                               "TTY",
                                               &error_property,
                                               &tty);
                if (r < 0) {
                        if (sd_bus_error_has_name(&error_property, SD_BUS_ERROR_UNKNOWN_OBJECT))
                                /* The session is already closed when we're querying the property */
                                continue;

                        log_warning_errno(r, "Failed to get TTY for session %s, ignoring: %s",
                                          id, bus_error_message(&error_property, r));

                        sd_bus_error_free(&error_property);
                }

                r = table_add_many(table,
                                   TABLE_STRING, id,
                                   TABLE_UID, (uid_t) uid,
                                   TABLE_STRING, user,
                                   TABLE_STRING, seat,
                                   TABLE_STRING, strna(tty));
                if (r < 0)
                        return table_log_add_error(r);
        }

        r = sd_bus_message_exit_container(reply);
        if (r < 0)
                return bus_log_parse_error(r);

        return show_table(table, "sessions");
}

static int list_users(int argc, char *argv[], void *userdata) {
        _cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
        _cleanup_(sd_bus_message_unrefp) sd_bus_message *reply = NULL;
        _cleanup_(table_unrefp) Table *table = NULL;
        sd_bus *bus = ASSERT_PTR(userdata);
        int r;

        assert(argv);

        pager_open(arg_pager_flags);

        r = bus_call_method(bus, bus_login_mgr, "ListUsers", &error, &reply, NULL);
        if (r < 0)
                return log_error_errno(r, "Failed to list users: %s", bus_error_message(&error, r));

        r = sd_bus_message_enter_container(reply, 'a', "(uso)");
        if (r < 0)
                return bus_log_parse_error(r);

        table = table_new("uid", "user", "linger", "state");
        if (!table)
                return log_oom();

        (void) table_set_align_percent(table, TABLE_HEADER_CELL(0), 100);

        for (;;) {
                _cleanup_(sd_bus_error_free) sd_bus_error error_property = SD_BUS_ERROR_NULL;
                _cleanup_free_ char *state = NULL;
                const char *user, *object;
                uint32_t uid;
                int linger;

                r = sd_bus_message_read(reply, "(uso)", &uid, &user, &object);
                if (r < 0)
                        return bus_log_parse_error(r);
                if (r == 0)
                        break;

                r = sd_bus_get_property_trivial(bus,
                                                "org.freedesktop.login1",
                                                object,
                                                "org.freedesktop.login1.User",
                                                "Linger",
                                                &error_property,
                                                'b',
                                                &linger);
                if (r < 0) {
                        if (sd_bus_error_has_name(&error_property, SD_BUS_ERROR_UNKNOWN_OBJECT))
                                /* The user logged out when we're querying the property */
                                continue;

                        return log_error_errno(r, "Failed to get linger status for user %s: %s",
                                               user, bus_error_message(&error_property, r));
                }

                r = sd_bus_get_property_string(bus,
                                               "org.freedesktop.login1",
                                               object,
                                               "org.freedesktop.login1.User",
                                               "State",
                                               &error_property,
                                               &state);
                if (r < 0) {
                        if (sd_bus_error_has_name(&error_property, SD_BUS_ERROR_UNKNOWN_OBJECT))
                                /* The user logged out when we're querying the property */
                                continue;

                        return log_error_errno(r, "Failed to get state for user %s: %s",
                                               user, bus_error_message(&error_property, r));
                }

                r = table_add_many(table,
                                   TABLE_UID, (uid_t) uid,
                                   TABLE_STRING, user,
                                   TABLE_BOOLEAN, linger,
                                   TABLE_STRING, state);
                if (r < 0)
                        return table_log_add_error(r);
        }

        r = sd_bus_message_exit_container(reply);
        if (r < 0)
                return bus_log_parse_error(r);

        return show_table(table, "users");
}

static int list_seats(int argc, char *argv[], void *userdata) {
        _cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
        _cleanup_(sd_bus_message_unrefp) sd_bus_message *reply = NULL;
        _cleanup_(table_unrefp) Table *table = NULL;
        sd_bus *bus = ASSERT_PTR(userdata);
        int r;

        assert(argv);

        pager_open(arg_pager_flags);

        r = bus_call_method(bus, bus_login_mgr, "ListSeats", &error, &reply, NULL);
        if (r < 0)
                return log_error_errno(r, "Failed to list seats: %s", bus_error_message(&error, r));

        r = sd_bus_message_enter_container(reply, 'a', "(so)");
        if (r < 0)
                return bus_log_parse_error(r);

        table = table_new("seat");
        if (!table)
                return log_oom();

        for (;;) {
                const char *seat;

                r = sd_bus_message_read(reply, "(so)", &seat, NULL);
                if (r < 0)
                        return bus_log_parse_error(r);
                if (r == 0)
                        break;

                r = table_add_cell(table, NULL, TABLE_STRING, seat);
                if (r < 0)
                        return table_log_add_error(r);
        }

        r = sd_bus_message_exit_container(reply);
        if (r < 0)
                return bus_log_parse_error(r);

        return show_table(table, "seats");
}

#if 0 /// UNNEEDED by elogind
static int show_unit_cgroup(sd_bus *bus, const char *interface, const char *unit, pid_t leader) {
        _cleanup_free_ char *cgroup = NULL;
        _cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
        unsigned c;
        int r;

        assert(bus);
        assert(unit);

        r = show_cgroup_get_unit_path_and_warn(bus, unit, &cgroup);
        if (r < 0)
                return r;

        if (isempty(cgroup))
                return 0;

        c = columns();
        if (c > 18)
                c -= 18;
        else
                c = 0;

        r = unit_show_processes(bus, unit, cgroup, "\t\t  ", c, get_output_flags(), &error);
        if (r == -EBADR) {

                if (arg_transport == BUS_TRANSPORT_REMOTE)
                        return 0;

                /* Fallback for older systemd versions where the GetUnitProcesses() call is not yet available */

                if (cg_is_empty_recursive(SYSTEMD_CGROUP_CONTROLLER, cgroup) != 0 && leader <= 0)
                        return 0;

                show_cgroup_and_extra(SYSTEMD_CGROUP_CONTROLLER, cgroup, "\t\t  ", c, &leader, leader > 0, get_output_flags());
        } else if (r < 0)
                return log_error_errno(r, "Failed to dump process list: %s", bus_error_message(&error, r));

        return 0;
}
#endif // 0

typedef struct SessionStatusInfo {
        const char *id;
        uid_t uid;
        const char *name;
        struct dual_timestamp timestamp;
        unsigned vtnr;
        const char *seat;
        const char *tty;
        const char *display;
        bool remote;
        const char *remote_host;
        const char *remote_user;
        const char *service;
        pid_t leader;
        const char *type;
        const char *class;
        const char *state;
        const char *scope;
        const char *desktop;
} SessionStatusInfo;

typedef struct UserStatusInfo {
        uid_t uid;
        bool linger;
        const char *name;
        struct dual_timestamp timestamp;
        const char *state;
        char **sessions;
        const char *display;
        const char *slice;
} UserStatusInfo;

typedef struct SeatStatusInfo {
        const char *id;
        const char *active_session;
        char **sessions;
} SeatStatusInfo;

static void user_status_info_clear(UserStatusInfo *info) {
        if (info) {
                strv_free(info->sessions);
                zero(*info);
        }
}

static void seat_status_info_clear(SeatStatusInfo *info) {
        if (info) {
                strv_free(info->sessions);
                zero(*info);
        }
}

static int prop_map_first_of_struct(sd_bus *bus, const char *member, sd_bus_message *m, sd_bus_error *error, void *userdata) {
        const char *contents;
        int r;

        r = sd_bus_message_peek_type(m, NULL, &contents);
        if (r < 0)
                return r;

        r = sd_bus_message_enter_container(m, SD_BUS_TYPE_STRUCT, contents);
        if (r < 0)
                return r;

        r = sd_bus_message_read_basic(m, contents[0], userdata);
        if (r < 0)
                return r;

        r = sd_bus_message_skip(m, contents+1);
        if (r < 0)
                return r;

        r = sd_bus_message_exit_container(m);
        if (r < 0)
                return r;

        return 0;
}

static int prop_map_sessions_strv(sd_bus *bus, const char *member, sd_bus_message *m, sd_bus_error *error, void *userdata) {
        const char *name;
        int r;

        assert(bus);
        assert(m);

        r = sd_bus_message_enter_container(m, 'a', "(so)");
        if (r < 0)
                return r;

        while ((r = sd_bus_message_read(m, "(so)", &name, NULL)) > 0) {
                r = strv_extend(userdata, name);
                if (r < 0)
                        return r;
        }
        if (r < 0)
                return r;

        return sd_bus_message_exit_container(m);
}

static int print_session_status_info(sd_bus *bus, const char *path, bool *new_line) {

        static const struct bus_properties_map map[]  = {
                { "Id",                  "s",    NULL,                     offsetof(SessionStatusInfo, id)                  },
                { "Name",                "s",    NULL,                     offsetof(SessionStatusInfo, name)                },
                { "TTY",                 "s",    NULL,                     offsetof(SessionStatusInfo, tty)                 },
                { "Display",             "s",    NULL,                     offsetof(SessionStatusInfo, display)             },
                { "RemoteHost",          "s",    NULL,                     offsetof(SessionStatusInfo, remote_host)         },
                { "RemoteUser",          "s",    NULL,                     offsetof(SessionStatusInfo, remote_user)         },
                { "Service",             "s",    NULL,                     offsetof(SessionStatusInfo, service)             },
                { "Desktop",             "s",    NULL,                     offsetof(SessionStatusInfo, desktop)             },
                { "Type",                "s",    NULL,                     offsetof(SessionStatusInfo, type)                },
                { "Class",               "s",    NULL,                     offsetof(SessionStatusInfo, class)               },
                { "Scope",               "s",    NULL,                     offsetof(SessionStatusInfo, scope)               },
                { "State",               "s",    NULL,                     offsetof(SessionStatusInfo, state)               },
                { "VTNr",                "u",    NULL,                     offsetof(SessionStatusInfo, vtnr)                },
                { "Leader",              "u",    NULL,                     offsetof(SessionStatusInfo, leader)              },
                { "Remote",              "b",    NULL,                     offsetof(SessionStatusInfo, remote)              },
                { "Timestamp",           "t",    NULL,                     offsetof(SessionStatusInfo, timestamp.realtime)  },
                { "TimestampMonotonic",  "t",    NULL,                     offsetof(SessionStatusInfo, timestamp.monotonic) },
                { "User",                "(uo)", prop_map_first_of_struct, offsetof(SessionStatusInfo, uid)                 },
                { "Seat",                "(so)", prop_map_first_of_struct, offsetof(SessionStatusInfo, seat)                },
                {}
        };

        _cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
        _cleanup_(sd_bus_message_unrefp) sd_bus_message *m = NULL;
        SessionStatusInfo i = {};
        int r;

        r = bus_map_all_properties(bus, "org.freedesktop.login1", path, map, BUS_MAP_BOOLEAN_AS_BOOL, &error, &m, &i);
        if (r < 0)
                return log_error_errno(r, "Could not get properties: %s", bus_error_message(&error, r));

        if (*new_line)
                printf("\n");

        *new_line = true;

        printf("%s - ", strna(i.id));

        if (i.name)
                printf("%s (" UID_FMT ")\n", i.name, i.uid);
        else
                printf(UID_FMT "\n", i.uid);

        if (timestamp_is_set(i.timestamp.realtime))
                printf("\t   Since: %s; %s\n",
                       FORMAT_TIMESTAMP(i.timestamp.realtime),
                       FORMAT_TIMESTAMP_RELATIVE(i.timestamp.realtime));

        if (i.leader > 0) {
                _cleanup_free_ char *t = NULL;

                printf("\t  Leader: " PID_FMT, i.leader);

                (void) get_process_comm(i.leader, &t);
                if (t)
                        printf(" (%s)", t);

                printf("\n");
        }

        if (!isempty(i.seat)) {
                printf("\t    Seat: %s", i.seat);

                if (i.vtnr > 0)
                        printf("; vc%u", i.vtnr);

                printf("\n");
        }

        if (i.tty)
                printf("\t     TTY: %s\n", i.tty);
        else if (i.display)
                printf("\t Display: %s\n", i.display);

        if (i.remote_host && i.remote_user)
                printf("\t  Remote: %s@%s\n", i.remote_user, i.remote_host);
        else if (i.remote_host)
                printf("\t  Remote: %s\n", i.remote_host);
        else if (i.remote_user)
                printf("\t  Remote: user %s\n", i.remote_user);
        else if (i.remote)
                printf("\t  Remote: Yes\n");

        if (i.service) {
                printf("\t Service: %s", i.service);

                if (i.type)
                        printf("; type %s", i.type);

                if (i.class)
                        printf("; class %s", i.class);

                printf("\n");
        } else if (i.type) {
                printf("\t    Type: %s", i.type);

                if (i.class)
                        printf("; class %s", i.class);

                printf("\n");
        } else if (i.class)
                printf("\t   Class: %s\n", i.class);

        if (!isempty(i.desktop))
                printf("\t Desktop: %s\n", i.desktop);

        if (i.state)
                printf("\t   State: %s\n", i.state);

        if (i.scope) {
                printf("\t    Unit: %s\n", i.scope);
#if 0 /// UNNEEDED by elogind
                show_unit_cgroup(bus, "org.freedesktop.systemd1.Scope", i.scope, i.leader);

                if (arg_transport == BUS_TRANSPORT_LOCAL)
                        show_journal_by_unit(
                                        stdout,
                                        i.scope,
                                        NULL,
                                        arg_output,
                                        0,
                                        i.timestamp.monotonic,
                                        arg_lines,
                                        0,
                                        get_output_flags() | OUTPUT_BEGIN_NEWLINE,
                                        SD_JOURNAL_LOCAL_ONLY,
                                        true,
                                        NULL);
#endif // 0
        }

        return 0;
}

static int print_user_status_info(sd_bus *bus, const char *path, bool *new_line) {

        static const struct bus_properties_map map[]  = {
                { "Name",               "s",     NULL,                     offsetof(UserStatusInfo, name)                },
                { "Linger",             "b",     NULL,                     offsetof(UserStatusInfo, linger)              },
                { "Slice",              "s",     NULL,                     offsetof(UserStatusInfo, slice)               },
                { "State",              "s",     NULL,                     offsetof(UserStatusInfo, state)               },
                { "UID",                "u",     NULL,                     offsetof(UserStatusInfo, uid)                 },
                { "Timestamp",          "t",     NULL,                     offsetof(UserStatusInfo, timestamp.realtime)  },
                { "TimestampMonotonic", "t",     NULL,                     offsetof(UserStatusInfo, timestamp.monotonic) },
                { "Display",            "(so)",  prop_map_first_of_struct, offsetof(UserStatusInfo, display)             },
                { "Sessions",           "a(so)", prop_map_sessions_strv,   offsetof(UserStatusInfo, sessions)            },
                {}
        };

        _cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
        _cleanup_(sd_bus_message_unrefp) sd_bus_message *m = NULL;
        _cleanup_(user_status_info_clear) UserStatusInfo i = {};
        int r;

        r = bus_map_all_properties(bus, "org.freedesktop.login1", path, map, BUS_MAP_BOOLEAN_AS_BOOL, &error, &m, &i);
        if (r < 0)
                return log_error_errno(r, "Could not get properties: %s", bus_error_message(&error, r));

        if (*new_line)
                printf("\n");

        *new_line = true;

        if (i.name)
                printf("%s (%"PRIu32")\n", i.name, i.uid);
        else
                printf("%"PRIu32"\n", i.uid);

        if (timestamp_is_set(i.timestamp.realtime))
                printf("\t   Since: %s; %s\n",
                       FORMAT_TIMESTAMP(i.timestamp.realtime),
                       FORMAT_TIMESTAMP_RELATIVE(i.timestamp.realtime));

        if (!isempty(i.state))
                printf("\t   State: %s\n", i.state);

        if (!strv_isempty(i.sessions)) {
                printf("\tSessions:");

                STRV_FOREACH(l, i.sessions)
                        printf(" %s%s",
                               streq_ptr(*l, i.display) ? "*" : "",
                               *l);

                printf("\n");
        }

        printf("\t  Linger: %s\n", yes_no(i.linger));

        if (i.slice) {
                printf("\t    Unit: %s\n", i.slice);
#if 0 /// UNNEEDED by elogind
                show_unit_cgroup(bus, "org.freedesktop.systemd1.Slice", i.slice, 0);

                show_journal_by_unit(
                                stdout,
                                i.slice,
                                NULL,
                                arg_output,
                                0,
                                i.timestamp.monotonic,
                                arg_lines,
                                0,
                                get_output_flags() | OUTPUT_BEGIN_NEWLINE,
                                SD_JOURNAL_LOCAL_ONLY,
                                true,
                                NULL);
#endif // 0
        }

        return 0;
}

static int print_seat_status_info(sd_bus *bus, const char *path, bool *new_line) {

        static const struct bus_properties_map map[]  = {
                { "Id",            "s",     NULL, offsetof(SeatStatusInfo, id) },
                { "ActiveSession", "(so)",  prop_map_first_of_struct, offsetof(SeatStatusInfo, active_session) },
                { "Sessions",      "a(so)", prop_map_sessions_strv, offsetof(SeatStatusInfo, sessions) },
                {}
        };

        _cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
        _cleanup_(sd_bus_message_unrefp) sd_bus_message *m = NULL;
        _cleanup_(seat_status_info_clear) SeatStatusInfo i = {};
        int r;

        r = bus_map_all_properties(bus, "org.freedesktop.login1", path, map, 0, &error, &m, &i);
        if (r < 0)
                return log_error_errno(r, "Could not get properties: %s", bus_error_message(&error, r));

        if (*new_line)
                printf("\n");

        *new_line = true;

        printf("%s\n", strna(i.id));

        if (!strv_isempty(i.sessions)) {
                printf("\tSessions:");

                STRV_FOREACH(l, i.sessions) {
                        if (streq_ptr(*l, i.active_session))
                                printf(" *%s", *l);
                        else
                                printf(" %s", *l);
                }

                printf("\n");
        }

        if (arg_transport == BUS_TRANSPORT_LOCAL) {
                unsigned c;

                c = columns();
                if (c > 21)
                        c -= 21;
                else
                        c = 0;

                printf("\t Devices:\n");

                show_sysfs(i.id, "\t\t  ", c, get_output_flags());
        }

        return 0;
}

static int print_property(const char *name, const char *expected_value, sd_bus_message *m, BusPrintPropertyFlags flags) {
        char type;
        const char *contents;
        int r;

        assert(name);
        assert(m);

        r = sd_bus_message_peek_type(m, &type, &contents);
        if (r < 0)
                return r;

        switch (type) {

        case SD_BUS_TYPE_STRUCT:

                if (contents[0] == SD_BUS_TYPE_STRING && STR_IN_SET(name, "Display", "Seat", "ActiveSession")) {
                        const char *s;

                        r = sd_bus_message_read(m, "(so)", &s, NULL);
                        if (r < 0)
                                return bus_log_parse_error(r);

                        bus_print_property_value(name, expected_value, flags, s);

                        return 1;

                } else if (contents[0] == SD_BUS_TYPE_UINT32 && streq(name, "User")) {
                        uint32_t uid;

                        r = sd_bus_message_read(m, "(uo)", &uid, NULL);
                        if (r < 0)
                                return bus_log_parse_error(r);

                        if (!uid_is_valid(uid))
                                return log_error_errno(SYNTHETIC_ERRNO(EINVAL),
                                                       "Invalid user ID: " UID_FMT,
                                                       uid);

                        bus_print_property_valuef(name, expected_value, flags, UID_FMT, uid);
                        return 1;
                }
                break;

        case SD_BUS_TYPE_ARRAY:

                if (contents[0] == SD_BUS_TYPE_STRUCT_BEGIN && streq(name, "Sessions")) {
                        const char *s;
                        bool space = false;

                        r = sd_bus_message_enter_container(m, SD_BUS_TYPE_ARRAY, "(so)");
                        if (r < 0)
                                return bus_log_parse_error(r);

                        if (!FLAGS_SET(flags, BUS_PRINT_PROPERTY_ONLY_VALUE))
                                printf("%s=", name);

                        while ((r = sd_bus_message_read(m, "(so)", &s, NULL)) > 0) {
                                printf("%s%s", space ? " " : "", s);
                                space = true;
                        }

                        if (space || !FLAGS_SET(flags, BUS_PRINT_PROPERTY_ONLY_VALUE))
                                printf("\n");

                        if (r < 0)
                                return bus_log_parse_error(r);

                        r = sd_bus_message_exit_container(m);
                        if (r < 0)
                                return bus_log_parse_error(r);

                        return 1;
                }
                break;
        }

        return 0;
}

static int show_properties(sd_bus *bus, const char *path, bool *new_line) {
        int r;

        assert(bus);
        assert(path);
        assert(new_line);

        if (*new_line)
                printf("\n");

        *new_line = true;

        r = bus_print_all_properties(
                        bus,
                        "org.freedesktop.login1",
                        path,
                        print_property,
                        arg_property,
                        arg_print_flags,
                        NULL);
        if (r < 0)
                return bus_log_parse_error(r);

        return 0;
}

static int show_session(int argc, char *argv[], void *userdata) {
        bool properties, new_line = false;
        sd_bus *bus = ASSERT_PTR(userdata);
        int r;
        _cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
        _cleanup_free_ char *path = NULL;

        assert(argv);

        properties = !strstr(argv[0], "status");

        pager_open(arg_pager_flags);

        if (argc <= 1) {
                /* If no argument is specified inspect the manager itself */
                if (properties)
                        return show_properties(bus, "/org/freedesktop/login1", &new_line);

                return print_session_status_info(bus, "/org/freedesktop/login1/session/auto", &new_line);
        }

        for (int i = 1; i < argc; i++) {
                r = get_session_path(bus, argv[i], &error, &path);
                if (r < 0)
                        return log_error_errno(r, "Failed to get session path: %s", bus_error_message(&error, r));

                if (properties)
                        r = show_properties(bus, path, &new_line);
                else
                        r = print_session_status_info(bus, path, &new_line);

                if (r < 0)
                        return r;
        }

        return 0;
}

static int show_user(int argc, char *argv[], void *userdata) {
        bool properties, new_line = false;
        sd_bus *bus = ASSERT_PTR(userdata);
        int r;

        assert(argv);

        properties = !strstr(argv[0], "status");

        pager_open(arg_pager_flags);

        if (argc <= 1) {
                /* If no argument is specified inspect the manager itself */
                if (properties)
                        return show_properties(bus, "/org/freedesktop/login1", &new_line);

                return print_user_status_info(bus, "/org/freedesktop/login1/user/self", &new_line);
        }

        for (int i = 1; i < argc; i++) {
                _cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
                _cleanup_(sd_bus_message_unrefp) sd_bus_message * reply = NULL;
                const char *path = NULL;
                uid_t uid;

                r = get_user_creds((const char**) (argv+i), &uid, NULL, NULL, NULL, 0);
                if (r < 0)
                        return log_error_errno(r, "Failed to look up user %s: %m", argv[i]);

                r = sd_bus_call_method(
                                bus,
                                "org.freedesktop.login1",
                                "/org/freedesktop/login1",
                                "org.freedesktop.login1.Manager",
                                "GetUser",
                                &error, &reply,
                                "u", (uint32_t) uid);
                if (r < 0)
                        return log_error_errno(r, "Failed to get user: %s", bus_error_message(&error, r));

                r = sd_bus_message_read(reply, "o", &path);
                if (r < 0)
                        return bus_log_parse_error(r);

                if (properties)
                        r = show_properties(bus, path, &new_line);
                else
                        r = print_user_status_info(bus, path, &new_line);

                if (r < 0)
                        return r;
        }

        return 0;
}

static int show_seat(int argc, char *argv[], void *userdata) {
        bool properties, new_line = false;
        sd_bus *bus = ASSERT_PTR(userdata);
        int r;

        assert(argv);

        properties = !strstr(argv[0], "status");

        pager_open(arg_pager_flags);

        if (argc <= 1) {
                /* If no argument is specified inspect the manager itself */
                if (properties)
                        return show_properties(bus, "/org/freedesktop/login1", &new_line);

                return print_seat_status_info(bus, "/org/freedesktop/login1/seat/auto", &new_line);
        }

        for (int i = 1; i < argc; i++) {
                _cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
                _cleanup_(sd_bus_message_unrefp) sd_bus_message * reply = NULL;
                const char *path = NULL;

                r = bus_call_method(bus, bus_login_mgr, "GetSeat", &error, &reply, "s", argv[i]);
                if (r < 0)
                        return log_error_errno(r, "Failed to get seat: %s", bus_error_message(&error, r));

                r = sd_bus_message_read(reply, "o", &path);
                if (r < 0)
                        return bus_log_parse_error(r);

                if (properties)
                        r = show_properties(bus, path, &new_line);
                else
                        r = print_seat_status_info(bus, path, &new_line);

                if (r < 0)
                        return r;
        }

        return 0;
}

static int activate(int argc, char *argv[], void *userdata) {
        _cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
        sd_bus *bus = ASSERT_PTR(userdata);
        int r;

        assert(argv);

        polkit_agent_open_if_enabled(arg_transport, arg_ask_password);

        if (argc < 2) {
                r = sd_bus_call_method(
                                bus,
                                "org.freedesktop.login1",
                                "/org/freedesktop/login1/session/auto",
                                "org.freedesktop.login1.Session",
                                streq(argv[0], "lock-session")      ? "Lock" :
                                streq(argv[0], "unlock-session")    ? "Unlock" :
                                streq(argv[0], "terminate-session") ? "Terminate" :
                                                                      "Activate",
                                &error, NULL, NULL);
                if (r < 0)
                        return log_error_errno(r, "Failed to issue method call: %s", bus_error_message(&error, r));

                return 0;
        }

        for (int i = 1; i < argc; i++) {

                r = bus_call_method(
                                bus,
                                bus_login_mgr,
                                streq(argv[0], "lock-session")      ? "LockSession" :
                                streq(argv[0], "unlock-session")    ? "UnlockSession" :
                                streq(argv[0], "terminate-session") ? "TerminateSession" :
                                                                      "ActivateSession",
                                &error, NULL,
                                "s", argv[i]);
                if (r < 0)
                        return log_error_errno(r, "Failed to issue method call: %s", bus_error_message(&error, r));
        }

        return 0;
}

static int kill_session(int argc, char *argv[], void *userdata) {
        _cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
        sd_bus *bus = ASSERT_PTR(userdata);
        int r;

        assert(argv);

        polkit_agent_open_if_enabled(arg_transport, arg_ask_password);

        if (!arg_kill_whom)
                arg_kill_whom = "all";

        for (int i = 1; i < argc; i++) {

                r = bus_call_method(
                                bus,
                                bus_login_mgr,
                                "KillSession",
                                &error, NULL,
                                "ssi", argv[i], arg_kill_whom, arg_signal);
                if (r < 0)
                        return log_error_errno(r, "Could not kill session: %s", bus_error_message(&error, r));
        }

        return 0;
}

static int enable_linger(int argc, char *argv[], void *userdata) {
        _cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
        sd_bus *bus = ASSERT_PTR(userdata);
        char* short_argv[3];
        bool b;
        int r;

        assert(argv);

        polkit_agent_open_if_enabled(arg_transport, arg_ask_password);

        b = streq(argv[0], "enable-linger");

        if (argc < 2) {
                /* No argument? Let's use an empty user name,
                 * then logind will use our user. */

                short_argv[0] = argv[0];
                short_argv[1] = (char*) "";
                short_argv[2] = NULL;
                argv = short_argv;
                argc = 2;
        }

        for (int i = 1; i < argc; i++) {
                uid_t uid;

                if (isempty(argv[i]))
                        uid = UID_INVALID;
                else {
                        r = get_user_creds((const char**) (argv+i), &uid, NULL, NULL, NULL, 0);
                        if (r < 0)
                                return log_error_errno(r, "Failed to look up user %s: %m", argv[i]);
                }

                r = bus_call_method(
                                bus,
                                bus_login_mgr,
                                "SetUserLinger",
                                &error, NULL,
                                "ubb", (uint32_t) uid, b, true);
                if (r < 0)
                        return log_error_errno(r, "Could not enable linger: %s", bus_error_message(&error, r));
        }

        return 0;
}

static int terminate_user(int argc, char *argv[], void *userdata) {
        _cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
        sd_bus *bus = ASSERT_PTR(userdata);
        int r;

        assert(argv);

        polkit_agent_open_if_enabled(arg_transport, arg_ask_password);

        for (int i = 1; i < argc; i++) {
                uid_t uid;

                if (isempty(argv[i]))
                        uid = getuid();
                else {
                        const char *u = argv[i];

                        r = get_user_creds(&u, &uid, NULL, NULL, NULL, 0);
                        if (r < 0)
                                return log_error_errno(r, "Failed to look up user %s: %m", argv[i]);
                }

                r = bus_call_method(bus, bus_login_mgr, "TerminateUser", &error, NULL, "u", (uint32_t) uid);
                if (r < 0)
                        return log_error_errno(r, "Could not terminate user: %s", bus_error_message(&error, r));
        }

        return 0;
}

static int kill_user(int argc, char *argv[], void *userdata) {
        _cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
        sd_bus *bus = ASSERT_PTR(userdata);
        int r;

        assert(argv);

        polkit_agent_open_if_enabled(arg_transport, arg_ask_password);

        if (!arg_kill_whom)
                arg_kill_whom = "all";

        for (int i = 1; i < argc; i++) {
                uid_t uid;

                if (isempty(argv[i]))
                        uid = getuid();
                else {
                        const char *u = argv[i];

                        r = get_user_creds(&u, &uid, NULL, NULL, NULL, 0);
                        if (r < 0)
                                return log_error_errno(r, "Failed to look up user %s: %m", argv[i]);
                }

                r = bus_call_method(
                        bus,
                        bus_login_mgr,
                        "KillUser",
                        &error, NULL,
                        "ui", (uint32_t) uid, arg_signal);
                if (r < 0)
                        return log_error_errno(r, "Could not kill user: %s", bus_error_message(&error, r));
        }

        return 0;
}

static int attach(int argc, char *argv[], void *userdata) {
        _cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
        sd_bus *bus = ASSERT_PTR(userdata);
        int r;

        assert(argv);

        polkit_agent_open_if_enabled(arg_transport, arg_ask_password);

        for (int i = 2; i < argc; i++) {

                r = bus_call_method(
                        bus,
                        bus_login_mgr,
                        "AttachDevice",
                        &error, NULL,
                        "ssb", argv[1], argv[i], true);
                if (r < 0)
                        return log_error_errno(r, "Could not attach device: %s", bus_error_message(&error, r));
        }

        return 0;
}

static int flush_devices(int argc, char *argv[], void *userdata) {
        _cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
        sd_bus *bus = ASSERT_PTR(userdata);
        int r;

        assert(argv);

        polkit_agent_open_if_enabled(arg_transport, arg_ask_password);

        r = bus_call_method(bus, bus_login_mgr, "FlushDevices", &error, NULL, "b", true);
        if (r < 0)
                return log_error_errno(r, "Could not flush devices: %s", bus_error_message(&error, r));

        return 0;
}

static int lock_sessions(int argc, char *argv[], void *userdata) {
        _cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
        sd_bus *bus = ASSERT_PTR(userdata);
        int r;

        assert(argv);

        polkit_agent_open_if_enabled(arg_transport, arg_ask_password);

        r = bus_call_method(
                        bus,
                        bus_login_mgr,
                        streq(argv[0], "lock-sessions") ? "LockSessions" : "UnlockSessions",
                        &error, NULL,
                        NULL);
        if (r < 0)
                return log_error_errno(r, "Could not lock sessions: %s", bus_error_message(&error, r));

        return 0;
}

static int terminate_seat(int argc, char *argv[], void *userdata) {
        _cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
        sd_bus *bus = ASSERT_PTR(userdata);
        int r;

        assert(argv);

        polkit_agent_open_if_enabled(arg_transport, arg_ask_password);

        for (int i = 1; i < argc; i++) {

                r = bus_call_method(bus, bus_login_mgr, "TerminateSeat", &error, NULL, "s", argv[i]);
                if (r < 0)
                        return log_error_errno(r, "Could not terminate seat: %s", bus_error_message(&error, r));
        }

        return 0;
}

#if 1 /// Add a reload command for reloading the elogind configuration, like systemctl has it.
static int reload_config(int argc, char *argv[], void *userdata) {
        _cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
        sd_bus *bus = userdata;
        int r;

        assert(bus);
        assert(argv);

        polkit_agent_open_if_enabled(arg_transport, arg_ask_password);

        r = sd_bus_call_method(
                        bus,
                        "org.freedesktop.login1",
                        "/org/freedesktop/login1",
                        "org.freedesktop.login1.Manager",
                        "ReloadConfig",
                        &error, NULL,
                        NULL);
        if (r < 0)
                return log_error_errno(r, "Could not reload configuration: %s", bus_error_message(&error, -r));

        return 0;
}
#endif // 1

static int help(int argc, char *argv[], void *userdata) {
        _cleanup_free_ char *link = NULL;
        int r;

        pager_open(arg_pager_flags);

        r = terminal_urlify_man("loginctl", "1", &link);
        if (r < 0)
                return log_oom();

        printf("%s [OPTIONS...] COMMAND ...\n\n"
               "%sSend control commands to or query the login manager.%s\n"
               "\nSession Commands:\n"
#if 0 /// elogind has "list" as a shorthand for "list-sessions"
               "  list-sessions            List sessions\n"
#else // 0
               "  list[-sessions]          List sessions (default command)\n"
#endif // 0
               "  session-status [ID...]   Show session status\n"
               "  show-session [ID...]     Show properties of sessions or the manager\n"
               "  activate [ID]            Activate a session\n"
               "  lock-session [ID...]     Screen lock one or more sessions\n"
               "  unlock-session [ID...]   Screen unlock one or more sessions\n"
               "  lock-sessions            Screen lock all current sessions\n"
               "  unlock-sessions          Screen unlock all current sessions\n"
               "  terminate-session ID...  Terminate one or more sessions\n"
               "  kill-session ID...       Send signal to processes of a session\n"
               "\nUser Commands:\n"
               "  list-users               List users\n"
               "  user-status [USER...]    Show user status\n"
               "  show-user [USER...]      Show properties of users or the manager\n"
               "  enable-linger [USER...]  Enable linger state of one or more users\n"
               "  disable-linger [USER...] Disable linger state of one or more users\n"
               "  terminate-user USER...   Terminate all sessions of one or more users\n"
               "  kill-user USER...        Send signal to processes of a user\n"
               "\nSeat Commands:\n"
               "  list-seats               List seats\n"
               "  seat-status [NAME...]    Show seat status\n"
               "  show-seat [NAME...]      Show properties of seats or the manager\n"
               "  attach NAME DEVICE...    Attach one or more devices to a seat\n"
               "  flush-devices            Flush all device associations\n"
               "  terminate-seat NAME...   Terminate all sessions on one or more seats\n"
#if 1 /// elogind adds some system commands to loginctl
               "\nSystem Commands:\n"
               "  reload                    Reload the elogind config file\n"
               "  poweroff [TIME] [WALL...] Turn off the machine\n"
               "  reboot   [TIME] [WALL...] Reboot the machine\n"
               "  suspend                   Suspend the machine to memory\n"
               "  hibernate                 Suspend the machine to disk\n"
               "  hybrid-sleep              Suspend the machine to memory and disk\n"
               "  suspend-then-hibernate    Suspend the system, wake after a period of\n"
               "                            time and put it into hibernate\n"
#endif // 1
               "\nOptions:\n"
               "  -h --help                Show this help\n"
               "     --version             Show package version\n"
               "     --no-pager            Do not pipe output into a pager\n"
#if 1 /// elogind supports --no-wall and --dry-run
               "     --no-wall             Do not send wall messages\n"
               "     --dry-run             Only print what would be done\n"
#endif // 1
               "     --no-legend           Do not show the headers and footers\n"
               "     --no-ask-password     Don't prompt for password\n"
               "  -H --host=[USER@]HOST    Operate on remote host\n"
               "  -M --machine=CONTAINER   Operate on local container\n"
               "  -p --property=NAME       Show only properties by this name\n"
               "  -P NAME                  Equivalent to --value --property=NAME\n"
               "  -a --all                 Show all properties, including empty ones\n"
               "     --value               When showing properties, only print the value\n"
               "  -l --full                Do not ellipsize output\n"
               "     --kill-whom=WHOM      Whom to send signal to\n"
               "  -s --signal=SIGNAL       Which signal to send\n"
#if 0 /// UNNEEDED by elogind
               "  -n --lines=INTEGER       Number of journal entries to show\n"
#else // 0
                /// elogind can cancel shutdowns and allows to ignore inhibitors
               "  -c                       Cancel a pending shutdown or reboot\n"
               "  -i --ignore-inhibitors   When shutting down or sleeping, ignore inhibitors\n\n"
#endif // 0
               "  -o --output=STRING       Change journal output mode (short, short-precise,\n"
               "                             short-iso, short-iso-precise, short-full,\n"
               "                             short-monotonic, short-unix, short-delta,\n"
               "                             json, json-pretty, json-sse, json-seq, cat,\n"
#if 1 /// As elogind can reboot, it allows to control the reboot process
#if ENABLE_EFI
               "     --firmware-setup      Tell the firmware to show the setup menu on next boot\n"
#endif
#ifdef ENABLE_EFI_TODO /// @todo EFI - needs change to support UEFI boot.
               "     --boot-loader-menu=TIME\n"
               "                           Boot into boot loader menu on next boot\n"
               "     --boot-loader-entry=NAME\n"
               "                           Boot into a specific boot loader entry on next boot\n"
#endif
#endif // 1
               "                             verbose, export, with-unit)\n"
               "\nSee the %s for details.\n",
               program_invocation_short_name,
               ansi_highlight(),
               ansi_normal(),
               link);

        return 0;
}

static int parse_argv(int argc, char *argv[]) {
        enum {
                ARG_VERSION = 0x100,
                ARG_VALUE,
                ARG_NO_PAGER,
#if 1 /// elogind supports --no-wall and --dry-run
                ARG_NO_WALL,
                ARG_DRY_RUN,
#endif // 1
                ARG_NO_LEGEND,
                ARG_KILL_WHOM,
                ARG_NO_ASK_PASSWORD,
#if 1 /// elogind supports controlling the reboot process
#if ENABLE_EFI
                ARG_FIRMWARE_SETUP,
#endif
#ifdef ENABLE_EFI_TODO /// @todo EFI - needs change to support UEFI boot.
                ARG_BOOT_LOADER_MENU,
                ARG_BOOT_LOADER_ENTRY,
#endif
#endif // 1
        };

        static const struct option options[] = {
                { "help",            no_argument,       NULL, 'h'                 },
                { "version",         no_argument,       NULL, ARG_VERSION         },
                { "property",        required_argument, NULL, 'p'                 },
                { "all",             no_argument,       NULL, 'a'                 },
                { "value",           no_argument,       NULL, ARG_VALUE           },
                { "full",            no_argument,       NULL, 'l'                 },
                { "no-pager",        no_argument,       NULL, ARG_NO_PAGER        },
#if 1 /// elogind supports --no-wall and --dry-run
                { "no-wall",         no_argument,       NULL, ARG_NO_WALL         },
                { "dry-run",         no_argument,       NULL, ARG_DRY_RUN         },
#endif // 1
                { "no-legend",       no_argument,       NULL, ARG_NO_LEGEND       },
                { "kill-whom",       required_argument, NULL, ARG_KILL_WHOM       },
                { "signal",          required_argument, NULL, 's'                 },
                { "host",            required_argument, NULL, 'H'                 },
                { "machine",         required_argument, NULL, 'M'                 },
                { "no-ask-password", no_argument,       NULL, ARG_NO_ASK_PASSWORD },
#if 0 /// UNNEEDED by elogind
                { "lines",           required_argument, NULL, 'n'                 },
                { "output",          required_argument, NULL, 'o'                 },
#else // 0
                /// elogind allows to ignore inhibitors for system commands.
                { "ignore-inhibitors", no_argument,     NULL, 'i'                 },
#endif // 0
#if 1 /// elogind supports controlling the reboot process
#if ENABLE_EFI
                { "firmware-setup",    no_argument,       NULL, ARG_FIRMWARE_SETUP      },
#endif
#ifdef ENABLE_EFI_TODO /// @todo EFI - needs change to support UEFI boot.
                { "boot-loader-menu",  required_argument, NULL, ARG_BOOT_LOADER_MENU    },
                { "boot-loader-entry", required_argument, NULL, ARG_BOOT_LOADER_ENTRY   },
#endif
#endif // 1
                {}
        };

        int c, r;

        assert(argc >= 0);
        assert(argv);

#if 0 /// elogind adds some system commands to loginctl
        while ((c = getopt_long(argc, argv, "hp:P:als:H:M:n:o:", options, NULL)) >= 0)
#else // 0
        while ((c = getopt_long(argc, argv, "hp:P:als:H:M:n:o:ci", options, NULL)) >= 0)
#endif // 0

                switch (c) {

                case 'h':
                        return help(0, NULL, NULL);

                case ARG_VERSION:
                        return version();

                case 'P':
                        SET_FLAG(arg_print_flags, BUS_PRINT_PROPERTY_ONLY_VALUE, true);
                        _fallthrough_;

                case 'p': {
                        r = strv_extend(&arg_property, optarg);
                        if (r < 0)
                                return log_oom();

                        /* If the user asked for a particular
                         * property, show it to them, even if it is
                         * empty. */
                        SET_FLAG(arg_print_flags, BUS_PRINT_PROPERTY_SHOW_EMPTY, true);
                        break;
                }

                case 'a':
                        SET_FLAG(arg_print_flags, BUS_PRINT_PROPERTY_SHOW_EMPTY, true);
                        break;

                case ARG_VALUE:
                        SET_FLAG(arg_print_flags, BUS_PRINT_PROPERTY_ONLY_VALUE, true);
                        break;

                case 'l':
                        arg_full = true;
                        break;

#if 0 /// UNNEEDED by elogind
                case 'n':
                        if (safe_atou(optarg, &arg_lines) < 0)
                                return log_error_errno(SYNTHETIC_ERRNO(EINVAL),
                                                       "Failed to parse lines '%s'", optarg);
                        break;
#endif // 0

                case 'o':
                        if (streq(optarg, "help")) {
                                DUMP_STRING_TABLE(output_mode, OutputMode, _OUTPUT_MODE_MAX);
                                return 0;
                        }

                        arg_output = output_mode_from_string(optarg);
                        if (arg_output < 0)
                                return log_error_errno(arg_output, "Unknown output '%s'.", optarg);

                        if (OUTPUT_MODE_IS_JSON(arg_output))
                                arg_legend = false;

                        break;
#if 1 /// elogind supports --no-wall and --dry-run
                case ARG_NO_WALL:
                        arg_no_wall = true;
                        break;

                case ARG_DRY_RUN:
                        arg_dry_run = true;
                        arg_pager_flags |= PAGER_DISABLE;
                        break;
#endif // 1
                case ARG_NO_PAGER:
                        arg_pager_flags |= PAGER_DISABLE;
                        break;

                case ARG_NO_LEGEND:
                        arg_legend = false;
                        break;

                case ARG_NO_ASK_PASSWORD:
                        arg_ask_password = false;
                        break;

                case ARG_KILL_WHOM:
                        arg_kill_whom = optarg;
                        break;

                case 's':
                        r = parse_signal_argument(optarg, &arg_signal);
                        if (r <= 0)
                                return r;
                        break;

                case 'H':
                        arg_transport = BUS_TRANSPORT_REMOTE;
                        arg_host = optarg;
                        break;

                case 'M':
                        arg_transport = BUS_TRANSPORT_MACHINE;
                        arg_host = optarg;
                        break;
#if 1 /// elogind can cancel shutdowns, allows to ignore inhibitors and can controll the reboot process
                case 'c':
                        arg_action = ACTION_CANCEL_SHUTDOWN;
                        break;

                case 'i':
                        arg_ignore_inhibitors = true;
                        break;

#if ENABLE_EFI
                case ARG_FIRMWARE_SETUP:
                        arg_firmware_setup = true;
                        break;

#endif
#ifdef ENABLE_EFI_TODO /// @todo EFI - needs change to support UEFI boot.
                case ARG_BOOT_LOADER_MENU:

                        r = parse_sec(optarg, &arg_boot_loader_menu);
                        if (r < 0)
                                return log_error_errno(r, "Failed to parse --boot-loader-menu= argument '%s': %m", optarg);

                        break;

                case ARG_BOOT_LOADER_ENTRY:

                        arg_boot_loader_entry = empty_to_null(optarg);
                        break;
#endif
#endif // 1
                case '?':
                        return -EINVAL;

                default:
                        assert_not_reached();
                }

        return 1;
}

static int loginctl_main(int argc, char *argv[], sd_bus *bus) {
        static const Verb verbs[] = {
                { "help",              VERB_ANY, VERB_ANY, 0,            help              },
#if 0 /// elogind has "list" as a shorthand for "list-sessions"
                { "list-sessions",     VERB_ANY, 1,        VERB_DEFAULT, list_sessions     },
#else // 0
                { "list",              VERB_ANY, 1,        VERB_DEFAULT, list_sessions     },
                { "list-sessions",     VERB_ANY, 1,        0,            list_sessions     },
#endif // 0
                { "session-status",    VERB_ANY, VERB_ANY, 0,            show_session      },
                { "show-session",      VERB_ANY, VERB_ANY, 0,            show_session      },
                { "activate",          VERB_ANY, 2,        0,            activate          },
                { "lock-session",      VERB_ANY, VERB_ANY, 0,            activate          },
                { "unlock-session",    VERB_ANY, VERB_ANY, 0,            activate          },
                { "lock-sessions",     VERB_ANY, 1,        0,            lock_sessions     },
                { "unlock-sessions",   VERB_ANY, 1,        0,            lock_sessions     },
                { "terminate-session", 2,        VERB_ANY, 0,            activate          },
                { "kill-session",      2,        VERB_ANY, 0,            kill_session      },
                { "list-users",        VERB_ANY, 1,        0,            list_users        },
                { "user-status",       VERB_ANY, VERB_ANY, 0,            show_user         },
                { "show-user",         VERB_ANY, VERB_ANY, 0,            show_user         },
                { "enable-linger",     VERB_ANY, VERB_ANY, 0,            enable_linger     },
                { "disable-linger",    VERB_ANY, VERB_ANY, 0,            enable_linger     },
                { "terminate-user",    2,        VERB_ANY, 0,            terminate_user    },
                { "kill-user",         2,        VERB_ANY, 0,            kill_user         },
                { "list-seats",        VERB_ANY, 1,        0,            list_seats        },
                { "seat-status",       VERB_ANY, VERB_ANY, 0,            show_seat         },
                { "show-seat",         VERB_ANY, VERB_ANY, 0,            show_seat         },
                { "attach",            3,        VERB_ANY, 0,            attach            },
                { "flush-devices",     VERB_ANY, 1,        0,            flush_devices     },
                { "terminate-seat",    2,        VERB_ANY, 0,            terminate_seat    },
#if 1 /// elogind adds some system commands to loginctl
                { "reload",            VERB_ANY, 1,        0,            reload_config     },
                { "poweroff",          VERB_ANY, VERB_ANY, 0,            start_special     },
                { "reboot",            VERB_ANY, VERB_ANY, 0,            start_special     },
                { "suspend",           VERB_ANY, 1,        0,            start_special     },
                { "hibernate",         VERB_ANY, 1,        0,            start_special     },
                { "hybrid-sleep",      VERB_ANY, 1,        0,            start_special     },
                { "suspend-then-hibernate", VERB_ANY, 1,   0,            start_special     },
                { "cancel-shutdown",   VERB_ANY, 1,        0,            start_special     },
#endif // 1
                {}
        };

#if 1 /// elogind can do shutdown and allows its cancellation
        if ((argc == optind) && (ACTION_CANCEL_SHUTDOWN == arg_action))
                return elogind_cancel_shutdown(bus);
#endif // 1
        return dispatch_verb(argc, argv, verbs, bus);
}

static int run(int argc, char *argv[]) {
        _cleanup_(sd_bus_flush_close_unrefp) sd_bus *bus = NULL;
        int r;

        setlocale(LC_ALL, "");
        elogind_set_program_name(argv[0]);
        log_setup();

        /* The journal merging logic potentially needs a lot of fds. */
        (void) rlimit_nofile_bump(HIGH_RLIMIT_NOFILE);

        sigbus_install();

        r = parse_argv(argc, argv);
        if (r <= 0)
                return r;

        r = bus_connect_transport(arg_transport, arg_host, RUNTIME_SCOPE_SYSTEM, &bus);
        if (r < 0)
                return bus_log_connect_error(r, arg_transport);

        (void) sd_bus_set_allow_interactive_authorization(bus, arg_ask_password);

#if 0 /// elogind has some own cleanups to do
        return loginctl_main(argc, argv, bus);
#else // 0
#if ENABLE_DEBUG_ELOGIND
        log_set_max_level(LOG_DEBUG);
        log_set_target(LOG_TARGET_SYSLOG_OR_KMSG);
#endif // ENABLE_DEBUG_ELOGIND
        r = loginctl_main(argc, argv, bus);

        elogind_cleanup();

        return r;
#endif // 0
}

DEFINE_MAIN_FUNCTION(run);
