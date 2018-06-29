/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

/***
  This file is part of systemd.

  Copyright 2013 Lennart Poettering
***/

#include <byteswap.h>
#include <stdbool.h>
#include <sys/socket.h>

#include "sd-bus.h"

#include "bus-creds.h"
#include "bus-protocol.h"
#include "macro.h"
#include "time-util.h"

struct bus_container {
        char enclosing;
        bool need_offsets:1;

        /* Indexes into the signature  string */
        unsigned index, saved_index;
        char *signature;

        size_t before, begin, end;

        /* dbus1: pointer to the array size value, if this is a value */
        uint32_t *array_size;

        /* gvariant: list of offsets to end of children if this is struct/dict entry/array */
        size_t *offsets, n_offsets, offsets_allocated, offset_index;
        size_t item_size;

        char *peeked_signature;
};

struct bus_body_part {
        struct bus_body_part *next;
        void *data;
        void *mmap_begin;
        size_t size;
        size_t mapped;
        size_t allocated;
        uint64_t memfd_offset;
        int memfd;
        bool free_this:1;
        bool munmap_this:1;
        bool sealed:1;
        bool is_zero:1;
};

struct sd_bus_message {
        unsigned n_ref;

        sd_bus *bus;

        uint64_t reply_cookie;

        const char *path;
        const char *interface;
        const char *member;
        const char *destination;
        const char *sender;

        sd_bus_error error;

        sd_bus_creds creds;

        usec_t monotonic;
        usec_t realtime;
        uint64_t seqnum;
        int64_t priority;
        uint64_t verify_destination_id;

        bool sealed:1;
        bool dont_send:1;
        bool allow_fds:1;
        bool free_header:1;
        bool free_fds:1;
        bool poisoned:1;

        /* The first and last bytes of the message */
        struct bus_header *header;
        void *footer;

        /* How many bytes are accessible in the above pointers */
        size_t header_accessible;
        size_t footer_accessible;

        size_t fields_size;
        size_t body_size;
        size_t user_body_size;

        struct bus_body_part body;
        struct bus_body_part *body_end;
        unsigned n_body_parts;

        size_t rindex;
        struct bus_body_part *cached_rindex_part;
        size_t cached_rindex_part_begin;

        uint32_t n_fds;
        int *fds;

        struct bus_container root_container, *containers;
        size_t n_containers;
        size_t containers_allocated;

        struct iovec *iovec;
        struct iovec iovec_fixed[2];
        unsigned n_iovec;

        char *peeked_signature;

        /* If set replies to this message must carry the signature
         * specified here to successfully seal. This is initialized
         * from the vtable data */
        const char *enforced_reply_signature;

        usec_t timeout;

        size_t header_offsets[_BUS_MESSAGE_HEADER_MAX];
        unsigned n_header_offsets;
};

static inline bool BUS_MESSAGE_NEED_BSWAP(sd_bus_message *m) {
        return m->header->endian != BUS_NATIVE_ENDIAN;
}

static inline uint16_t BUS_MESSAGE_BSWAP16(sd_bus_message *m, uint16_t u) {
        return BUS_MESSAGE_NEED_BSWAP(m) ? bswap_16(u) : u;
}

static inline uint32_t BUS_MESSAGE_BSWAP32(sd_bus_message *m, uint32_t u) {
        return BUS_MESSAGE_NEED_BSWAP(m) ? bswap_32(u) : u;
}

static inline uint64_t BUS_MESSAGE_BSWAP64(sd_bus_message *m, uint64_t u) {
        return BUS_MESSAGE_NEED_BSWAP(m) ? bswap_64(u) : u;
}

static inline uint64_t BUS_MESSAGE_COOKIE(sd_bus_message *m) {
        if (m->header->version == 2)
                return BUS_MESSAGE_BSWAP64(m, m->header->dbus2.cookie);

        return BUS_MESSAGE_BSWAP32(m, m->header->dbus1.serial);
}

static inline size_t BUS_MESSAGE_SIZE(sd_bus_message *m) {
        return
                sizeof(struct bus_header) +
                ALIGN8(m->fields_size) +
                m->body_size;
}

static inline size_t BUS_MESSAGE_BODY_BEGIN(sd_bus_message *m) {
        return
                sizeof(struct bus_header) +
                ALIGN8(m->fields_size);
}

static inline void* BUS_MESSAGE_FIELDS(sd_bus_message *m) {
        return (uint8_t*) m->header + sizeof(struct bus_header);
}

static inline bool BUS_MESSAGE_IS_GVARIANT(sd_bus_message *m) {
        return m->header->version == 2;
}

#if 0 /// UNNEEDED by elogind
int bus_message_get_blob(sd_bus_message *m, void **buffer, size_t *sz);
#endif // 0
int bus_message_read_strv_extend(sd_bus_message *m, char ***l);

int bus_message_from_header(
                sd_bus *bus,
                void *header,
                size_t header_accessible,
                void *footer,
                size_t footer_accessible,
                size_t message_size,
                int *fds,
                size_t n_fds,
                const char *label,
                size_t extra,
                sd_bus_message **ret);

int bus_message_from_malloc(
                sd_bus *bus,
                void *buffer,
                size_t length,
                int *fds,
                size_t n_fds,
                const char *label,
                sd_bus_message **ret);

int bus_message_get_arg(sd_bus_message *m, unsigned i, const char **str);
int bus_message_get_arg_strv(sd_bus_message *m, unsigned i, char ***strv);

int bus_message_parse_fields(sd_bus_message *m);

struct bus_body_part *message_append_part(sd_bus_message *m);

#define MESSAGE_FOREACH_PART(part, i, m) \
        for ((i) = 0, (part) = &(m)->body; (i) < (m)->n_body_parts; (i)++, (part) = (part)->next)

int bus_body_part_map(struct bus_body_part *part);
void bus_body_part_unmap(struct bus_body_part *part);

int bus_message_to_errno(sd_bus_message *m);

int bus_message_new_synthetic_error(sd_bus *bus, uint64_t serial, const sd_bus_error *e, sd_bus_message **m);

int bus_message_remarshal(sd_bus *bus, sd_bus_message **m);

#if 0 /// UNNEEDED by elogind
int bus_message_append_sender(sd_bus_message *m, const char *sender);
#endif // 0

void bus_message_set_sender_driver(sd_bus *bus, sd_bus_message *m);
void bus_message_set_sender_local(sd_bus *bus, sd_bus_message *m);
