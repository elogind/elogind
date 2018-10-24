/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

/***
***/

#include <pthread.h>
#include <sys/socket.h>

#include "sd-bus.h"

#include "bus-error.h"
#include "bus-kernel.h"
#include "bus-match.h"
#include "def.h"
#include "hashmap.h"
#include "list.h"
#include "prioq.h"
#include "refcnt.h"
#include "socket-util.h"
#include "util.h"

struct reply_callback {
        sd_bus_message_handler_t callback;
        usec_t timeout_usec; /* this is a relative timeout until we reach the BUS_HELLO state, and an absolute one right after */
        uint64_t cookie;
        unsigned prioq_idx;
};

struct filter_callback {
        sd_bus_message_handler_t callback;

        unsigned last_iteration;

        LIST_FIELDS(struct filter_callback, callbacks);
};

struct match_callback {
        sd_bus_message_handler_t callback;
        sd_bus_message_handler_t install_callback;

        sd_bus_slot *install_slot; /* The AddMatch() call */

        unsigned last_iteration;

        char *match_string;

        struct bus_match_node *match_node;
};

struct node {
        char *path;
        struct node *parent;
        LIST_HEAD(struct node, child);
        LIST_FIELDS(struct node, siblings);

        LIST_HEAD(struct node_callback, callbacks);
        LIST_HEAD(struct node_vtable, vtables);
        LIST_HEAD(struct node_enumerator, enumerators);
        LIST_HEAD(struct node_object_manager, object_managers);
};

struct node_callback {
        struct node *node;

        bool is_fallback;
        sd_bus_message_handler_t callback;

        unsigned last_iteration;

        LIST_FIELDS(struct node_callback, callbacks);
};

struct node_enumerator {
        struct node *node;

        sd_bus_node_enumerator_t callback;

        unsigned last_iteration;

        LIST_FIELDS(struct node_enumerator, enumerators);
};

struct node_object_manager {
        struct node *node;

        LIST_FIELDS(struct node_object_manager, object_managers);
};

struct node_vtable {
        struct node *node;

        char *interface;
        bool is_fallback;
        const sd_bus_vtable *vtable;
        sd_bus_object_find_t find;

        unsigned last_iteration;

        LIST_FIELDS(struct node_vtable, vtables);
};

struct vtable_member {
        const char *path;
        const char *interface;
        const char *member;
        struct node_vtable *parent;
        unsigned last_iteration;
        const sd_bus_vtable *vtable;
};

typedef enum BusSlotType {
        BUS_REPLY_CALLBACK,
        BUS_FILTER_CALLBACK,
        BUS_MATCH_CALLBACK,
        BUS_NODE_CALLBACK,
        BUS_NODE_ENUMERATOR,
        BUS_NODE_VTABLE,
        BUS_NODE_OBJECT_MANAGER,
        _BUS_SLOT_INVALID = -1,
} BusSlotType;

struct sd_bus_slot {
        unsigned n_ref;
        sd_bus *bus;
        void *userdata;
        sd_bus_destroy_t destroy_callback;
        BusSlotType type:5;

        /* Slots can be "floating" or not. If they are not floating (the usual case) then they reference the bus object
         * they are associated with. This means the bus object stays allocated at least as long as there is a slot
         * around associated with it. If it is floating, then the slot's lifecycle is bound to the lifecycle of the
         * bus: it will be disconnected from the bus when the bus is destroyed, and it keeping the slot reffed hence
         * won't mean the bus stays reffed too. Internally this means the reference direction is reversed: floating
         * slots objects are referenced by the bus object, and not vice versa. */
        bool floating:1;

        bool match_added:1;
        char *description;

        LIST_FIELDS(sd_bus_slot, slots);

        union {
                struct reply_callback reply_callback;
                struct filter_callback filter_callback;
                struct match_callback match_callback;
                struct node_callback node_callback;
                struct node_enumerator node_enumerator;
                struct node_object_manager node_object_manager;
                struct node_vtable node_vtable;
        };
};

enum bus_state {
        BUS_UNSET,
        BUS_WATCH_BIND,      /* waiting for the socket to appear via inotify */
        BUS_OPENING,         /* the kernel's connect() is still not ready */
        BUS_AUTHENTICATING,  /* we are currently in the "SASL" authorization phase of dbus */
        BUS_HELLO,           /* we are waiting for the Hello() response */
        BUS_RUNNING,
        BUS_CLOSING,
        BUS_CLOSED,
        _BUS_STATE_MAX,
};

static inline bool BUS_IS_OPEN(enum bus_state state) {
        return state > BUS_UNSET && state < BUS_CLOSING;
}

enum bus_auth {
        _BUS_AUTH_INVALID,
        BUS_AUTH_EXTERNAL,
        BUS_AUTH_ANONYMOUS
};

struct sd_bus {
        /* We use atomic ref counting here since sd_bus_message
           objects retain references to their originating sd_bus but
           we want to allow them to be processed in a different
           thread. We won't provide full thread safety, but only the
           bare minimum that makes it possible to use sd_bus and
           sd_bus_message objects independently and on different
           threads as long as each object is used only once at the
           same time. */
        RefCount n_ref;

        enum bus_state state;
        int input_fd, output_fd;
        int inotify_fd;
        int message_version;
        int message_endian;

        bool can_fds:1;
        bool bus_client:1;
        bool ucred_valid:1;
        bool is_server:1;
        bool anonymous_auth:1;
        bool prefer_readv:1;
        bool prefer_writev:1;
        bool match_callbacks_modified:1;
        bool filter_callbacks_modified:1;
        bool nodes_modified:1;
        bool trusted:1;
        bool manual_peer_interface:1;
        bool is_system:1;
        bool is_user:1;
        bool allow_interactive_authorization:1;
        bool exit_on_disconnect:1;
        bool exited:1;
        bool exit_triggered:1;
        bool is_local:1;
        bool watch_bind:1;
        bool is_monitor:1;
        bool accept_fd:1;
        bool attach_timestamp:1;
        bool connected_signal:1;

        int use_memfd;

        void *rbuffer;
        size_t rbuffer_size;

        sd_bus_message **rqueue;
        unsigned rqueue_size;
        size_t rqueue_allocated;

        sd_bus_message **wqueue;
        unsigned wqueue_size;
        size_t windex;
        size_t wqueue_allocated;

        uint64_t cookie;

        char *unique_name;
        uint64_t unique_id;

        struct bus_match_node match_callbacks;
        Prioq *reply_callbacks_prioq;
        OrderedHashmap *reply_callbacks;
        LIST_HEAD(struct filter_callback, filter_callbacks);

        Hashmap *nodes;
        Hashmap *vtable_methods;
        Hashmap *vtable_properties;

        union sockaddr_union sockaddr;
        socklen_t sockaddr_size;

        char *machine;
        pid_t nspid;

        sd_id128_t server_id;

        char *address;
        unsigned address_index;

        int last_connect_error;

        enum bus_auth auth;
        size_t auth_rbegin;
        struct iovec auth_iovec[3];
        unsigned auth_index;
        char *auth_buffer;
        usec_t auth_timeout;

        struct ucred ucred;
        char *label;
        gid_t *groups;
        size_t n_groups;

        uint64_t creds_mask;

        int *fds;
        size_t n_fds;

        char *exec_path;
        char **exec_argv;

        unsigned iteration_counter;

        /* We do locking around the memfd cache, since we want to
         * allow people to process a sd_bus_message in a different
         * thread then it was generated on and free it there. Since
         * adding something to the memfd cache might happen when a
         * message is released, we hence need to protect this bit with
         * a mutex. */
        pthread_mutex_t memfd_cache_mutex;
        struct memfd_cache memfd_cache[MEMFD_CACHE_MAX];
        unsigned n_memfd_cache;

        pid_t original_pid;
        pid_t busexec_pid;

        sd_event_source *input_io_event_source;
        sd_event_source *output_io_event_source;
        sd_event_source *time_event_source;
        sd_event_source *quit_event_source;
        sd_event_source *inotify_event_source;
        sd_event *event;
        int event_priority;

        sd_bus_message *current_message;
        sd_bus_slot *current_slot;
        sd_bus_message_handler_t current_handler;
        void *current_userdata;

        sd_bus **default_bus_ptr;
        pid_t tid;

        char *cgroup_root;

        char *description;
        char *patch_sender;

        sd_bus_track *track_queue;

        LIST_HEAD(sd_bus_slot, slots);
        LIST_HEAD(sd_bus_track, tracks);

        int *inotify_watches;
        size_t n_inotify_watches;
};

/* For method calls we time-out at 25s, like in the D-Bus reference implementation */
#define BUS_DEFAULT_TIMEOUT ((usec_t) (25 * USEC_PER_SEC))

/* For the authentication phase we grant 90s, to provide extra room during boot, when RNGs and such are not filled up
 * with enough entropy yet and might delay the boot */
#define BUS_AUTH_TIMEOUT ((usec_t) DEFAULT_TIMEOUT_USEC)

#define BUS_WQUEUE_MAX (192*1024)
#define BUS_RQUEUE_MAX (192*1024)

#define BUS_MESSAGE_SIZE_MAX (128*1024*1024)
#define BUS_AUTH_SIZE_MAX (64*1024)

#define BUS_CONTAINER_DEPTH 128

/* Defined by the specification as maximum size of an array in
 * bytes */
#define BUS_ARRAY_MAX_SIZE 67108864

#define BUS_FDS_MAX 1024

#define BUS_EXEC_ARGV_MAX 256

bool interface_name_is_valid(const char *p) _pure_;
bool service_name_is_valid(const char *p) _pure_;
#if 0 /// UNNEEDED by elogind
char* service_name_startswith(const char *a, const char *b);
#endif // 0
bool member_name_is_valid(const char *p) _pure_;
bool object_path_is_valid(const char *p) _pure_;
char *object_path_startswith(const char *a, const char *b) _pure_;

bool namespace_complex_pattern(const char *pattern, const char *value) _pure_;
bool path_complex_pattern(const char *pattern, const char *value) _pure_;

bool namespace_simple_pattern(const char *pattern, const char *value) _pure_;
bool path_simple_pattern(const char *pattern, const char *value) _pure_;

int bus_message_type_from_string(const char *s, uint8_t *u) _pure_;
const char *bus_message_type_to_string(uint8_t u) _pure_;

#define error_name_is_valid interface_name_is_valid

sd_bus *bus_resolve(sd_bus *bus);

int bus_ensure_running(sd_bus *bus);
int bus_start_running(sd_bus *bus);
int bus_next_address(sd_bus *bus);

int bus_seal_synthetic_message(sd_bus *b, sd_bus_message *m);

int bus_rqueue_make_room(sd_bus *bus);

bool bus_pid_changed(sd_bus *bus);

char *bus_address_escape(const char *v);

int bus_attach_io_events(sd_bus *b);
int bus_attach_inotify_event(sd_bus *b);

void bus_close_inotify_fd(sd_bus *b);
void bus_close_io_fds(sd_bus *b);

#define OBJECT_PATH_FOREACH_PREFIX(prefix, path)                        \
        for (char *_slash = ({ strcpy((prefix), (path)); streq((prefix), "/") ? NULL : strrchr((prefix), '/'); }) ; \
             _slash && !(_slash[(_slash) == (prefix)] = 0);             \
             _slash = streq((prefix), "/") ? NULL : strrchr((prefix), '/'))

/* If we are invoking callbacks of a bus object, ensure unreffing the
 * bus from the callback doesn't destroy the object we are working
 * on */
#define BUS_DONT_DESTROY(bus) \
        _cleanup_(sd_bus_unrefp) _unused_ sd_bus *_dont_destroy_##bus = sd_bus_ref(bus)

int bus_set_address_system(sd_bus *bus);
int bus_set_address_user(sd_bus *bus);
int bus_set_address_system_remote(sd_bus *b, const char *host);
int bus_set_address_system_machine(sd_bus *b, const char *machine);

int bus_get_root_path(sd_bus *bus);

int bus_maybe_reply_error(sd_bus_message *m, int r, sd_bus_error *error);

#define bus_assert_return(expr, r, error)                               \
        do {                                                            \
                if (!assert_log(expr, #expr))                           \
                        return sd_bus_error_set_errno(error, r);        \
        } while (false)

void bus_enter_closing(sd_bus *bus);

void bus_set_state(sd_bus *bus, enum bus_state state);
