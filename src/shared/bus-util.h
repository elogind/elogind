/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include "sd-bus.h"
#include "sd-event.h"

#include "errno-util.h"
#include "macro.h"
#include "runtime-scope.h"
#include "set.h"
#include "string-util.h"
#include "time-util.h"

typedef enum BusTransport {
        BUS_TRANSPORT_LOCAL,
        BUS_TRANSPORT_REMOTE,
        BUS_TRANSPORT_MACHINE,
        _BUS_TRANSPORT_MAX,
        _BUS_TRANSPORT_INVALID = -EINVAL,
} BusTransport;


#if 0 /// UNNEEDED by elogind
int bus_async_unregister_and_exit(sd_event *e, sd_bus *bus, const char *name);
#endif // 0

typedef bool (*check_idle_t)(void *userdata);

#if 0 /// UNNEEDED by elogind
int bus_event_loop_with_idle(sd_event *e, sd_bus *bus, const char *name, usec_t timeout, check_idle_t check_idle, void *userdata);
#endif // 0

int bus_name_has_owner(sd_bus *c, const char *name, sd_bus_error *error);
bool bus_error_is_unknown_service(const sd_bus_error *error);

#if 0 /// UNNEEDED by elogind
int bus_check_peercred(sd_bus *c);

int bus_connect_system_systemd(sd_bus **ret_bus);
int bus_connect_user_systemd(sd_bus **ret_bus);
#endif // 0

#if 0 /// UNNEEDED by elogind
#endif // 0
int bus_connect_transport(BusTransport transport, const char *host, RuntimeScope runtime_scope, sd_bus **bus);
int bus_connect_transport_elogind(BusTransport transport, const char *host, RuntimeScope runtime_scope, sd_bus **bus);

int bus_log_address_error(int r, BusTransport transport);
int bus_log_connect_error(int r, BusTransport transport);

#define bus_log_parse_error(r)                                  \
        log_error_errno(r, "Failed to parse bus message: %m")

#define bus_log_parse_error_debug(r)                            \
        log_debug_errno(r, "Failed to parse bus message: %m")

#define bus_log_create_error(r)                                 \
        log_error_errno(r, "Failed to create bus message: %m")

#if 0 /// UNNEEDED by elogind
int bus_path_encode_unique(sd_bus *b, const char *prefix, const char *sender_id, const char *external_id, char **ret_path);
int bus_path_decode_unique(const char *path, const char *prefix, char **ret_sender, char **ret_external);

int bus_track_add_name_many(sd_bus_track *t, char **l);
#endif // 0

int bus_open_system_watch_bind_with_description(sd_bus **ret, const char *description);
#if 0 /// UNNEEDED by elogind
static inline int bus_open_system_watch_bind(sd_bus **ret) {
        return bus_open_system_watch_bind_with_description(ret, NULL);
}

int bus_reply_pair_array(sd_bus_message *m, char **l);
#endif // 0

/* Listen to GetMallocInfo() calls to 'destination' and return malloc_info() via FD */
int bus_register_malloc_status(sd_bus *bus, const char *destination);

extern const struct hash_ops bus_message_hash_ops;

int bus_message_append_string_set(sd_bus_message *m, Set *s);

int bus_property_get_string_set(sd_bus *bus, const char *path, const char *interface, const char *property, sd_bus_message *reply, void *userdata, sd_bus_error *error);
