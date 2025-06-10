/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <stdbool.h>

#include "hashmap.h"
#include "macro.h"
#include "set.h"

typedef struct FDSet FDSet;

FDSet* fdset_new(void);
FDSet* fdset_free(FDSet *s);
#if 0 /// UNNEEDED by elogind
FDSet* fdset_free_async(FDSet *s);
#endif // 0

int fdset_put(FDSet *s, int fd);
#if 0 /// UNNEEDED by elogind
int fdset_consume(FDSet *s, int fd);
int fdset_put_dup(FDSet *s, int fd);
#endif // 0

bool fdset_contains(FDSet *s, int fd);
#if 0 /// UNNEEDED by elogind
int fdset_remove(FDSet *s, int fd);
#endif // 0

#if 0 /// UNNEEDED by elogind
int fdset_new_array(FDSet **ret, const int *fds, size_t n_fds);
#endif // 0
int fdset_new_fill(int filter_cloexec, FDSet **ret);
#if 0 /// UNNEEDED by elogind
int fdset_new_listen_fds(FDSet **ret, bool unset);

int fdset_cloexec(FDSet *fds, bool b);

int fdset_to_array(FDSet *fds, int **ret);

int fdset_close_others(FDSet *fds);

unsigned fdset_size(FDSet *fds);
bool fdset_isempty(FDSet *fds);

int fdset_iterate(FDSet *s, Iterator *i);
#endif // 0

int fdset_steal_first(FDSet *fds);

void fdset_close(FDSet *fds, bool async);

#if 0 /// UNNEEDED by elogind
#define _FDSET_FOREACH(fd, fds, i) \
        for (Iterator i = ITERATOR_FIRST; ((fd) = fdset_iterate((fds), &i)) >= 0; )
#define FDSET_FOREACH(fd, fds) \
        _FDSET_FOREACH(fd, fds, UNIQ_T(i, UNIQ))
#endif // 0

DEFINE_TRIVIAL_CLEANUP_FUNC(FDSet*, fdset_free);
#define _cleanup_fdset_free_ _cleanup_(fdset_freep)

#if 0 /// UNNEEDED by elogind
DEFINE_TRIVIAL_CLEANUP_FUNC(FDSet*, fdset_free_async);
#endif // 0
