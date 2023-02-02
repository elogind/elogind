/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <stdbool.h>

#include "sd-id128.h"

#include "hash-funcs.h"
#include "macro.h"


#if 0 /// UNNEEDED by elogind
bool id128_is_valid(const char *s) _pure_;
#endif // 0

typedef enum Id128FormatFlag {
        ID128_FORMAT_PLAIN = 1 << 0,  /* formatted as 32 hex chars as-is */
        ID128_FORMAT_UUID  = 1 << 1,  /* formatted as 36 character uuid string */
        ID128_FORMAT_ANY   = ID128_FORMAT_PLAIN | ID128_FORMAT_UUID,
} Id128FormatFlag;

int id128_read_fd(int fd, Id128FormatFlag f, sd_id128_t *ret);
int id128_read(const char *p, Id128FormatFlag f, sd_id128_t *ret);

int id128_write_fd(int fd, Id128FormatFlag f, sd_id128_t id, bool do_sync);
#if 0 /// UNNEEDED by elogind
int id128_write(const char *p, Id128FormatFlag f, sd_id128_t id, bool do_sync);
#endif // 0

void id128_hash_func(const sd_id128_t *p, struct siphash *state);
int id128_compare_func(const sd_id128_t *a, const sd_id128_t *b) _pure_;
extern const struct hash_ops id128_hash_ops;

sd_id128_t id128_make_v4_uuid(sd_id128_t id);

int id128_get_product(sd_id128_t *ret);
