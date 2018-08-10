/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

/***
  Copyright 2013 Lennart Poettering
***/

#include "sd-bus.h"

#define MEMFD_CACHE_MAX 32

/* When we cache a memfd block for reuse, we will truncate blocks
 * longer than this in order not to keep too much data around. */
#define MEMFD_CACHE_ITEM_SIZE_MAX (128*1024)

/* This determines at which minimum size we prefer sending memfds over
 * sending vectors */
#define MEMFD_MIN_SIZE (512*1024)

struct memfd_cache {
        int fd;
        void *address;
        size_t mapped;
        size_t allocated;
};

#if 0 /// UNNEEDED by elogind
#endif // 0
void close_and_munmap(int fd, void *address, size_t size);
void bus_flush_memfd(sd_bus *bus);
#if 0 /// UNNEEDED by elogind
#endif // 0
