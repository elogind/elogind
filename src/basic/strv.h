/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

/***
  Copyright 2010 Lennart Poettering
***/

#include <fnmatch.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>

#include "alloc-util.h"
#include "extract-word.h"
#include "macro.h"
#include "util.h"

char *strv_find(char **l, const char *name) _pure_;
char *strv_find_prefix(char **l, const char *name) _pure_;
char *strv_find_startswith(char **l, const char *name) _pure_;

char **strv_free(char **l);
DEFINE_TRIVIAL_CLEANUP_FUNC(char**, strv_free);
#define _cleanup_strv_free_ _cleanup_(strv_freep)

char **strv_free_erase(char **l);
DEFINE_TRIVIAL_CLEANUP_FUNC(char**, strv_free_erase);
#define _cleanup_strv_free_erase_ _cleanup_(strv_free_erasep)

void strv_clear(char **l);

char **strv_copy(char * const *l);
size_t strv_length(char * const *l) _pure_;

#if 0 /// UNNEEDED by elogind
int strv_extend_strv(char ***a, char **b, bool filter_duplicates);
int strv_extend_strv_concat(char ***a, char **b, const char *suffix);
#endif // 0
int strv_extend(char ***l, const char *value);
#if 0 /// UNNEEDED by elogind
int strv_extendf(char ***l, const char *format, ...) _printf_(2,0);
#endif // 0
int strv_extend_front(char ***l, const char *value);
int strv_push(char ***l, char *value);
int strv_push_pair(char ***l, char *a, char *b);
int strv_insert(char ***l, size_t position, char *value);

static inline int strv_push_prepend(char ***l, char *value) {
        return strv_insert(l, 0, value);
}

int strv_consume(char ***l, char *value);
#if 0 /// UNNEEDED by elogind
int strv_consume_pair(char ***l, char *a, char *b);
#endif // 0
int strv_consume_prepend(char ***l, char *value);

char **strv_remove(char **l, const char *s);
char **strv_uniq(char **l);
#if 0 /// UNNEEDED by elogind
bool strv_is_uniq(char **l);
#endif // 0

bool strv_equal(char **a, char **b);

#define strv_contains(l, s) (!!strv_find((l), (s)))

char **strv_new(const char *x, ...) _sentinel_;
char **strv_new_ap(const char *x, va_list ap);

#define STRV_IGNORE ((const char *) -1)

static inline const char* STRV_IFNOTNULL(const char *x) {
        return x ? x : STRV_IGNORE;
}

static inline bool strv_isempty(char * const *l) {
        return !l || !*l;
}

char **strv_split(const char *s, const char *separator);
#if 0 /// UNNEEDED by elogind
char **strv_split_newlines(const char *s);
#endif // 0

int strv_split_extract(char ***t, const char *s, const char *separators, ExtractFlags flags);

char *strv_join(char **l, const char *separator);
#if 0 /// UNNEEDED by elogind
#endif // 0

char **strv_parse_nulstr(const char *s, size_t l);
char **strv_split_nulstr(const char *s);
#if 0 /// UNNEEDED by elogind
int strv_make_nulstr(char **l, char **p, size_t *n);

bool strv_overlap(char **a, char **b) _pure_;
#endif // 0

#define STRV_FOREACH(s, l)                      \
        for ((s) = (l); (s) && *(s); (s)++)

#define STRV_FOREACH_BACKWARDS(s, l)                                \
        for (s = ({                                                 \
                        char **_l = l;                              \
                        _l ? _l + strv_length(_l) - 1U : NULL;      \
                        });                                         \
             (l) && ((s) >= (l));                                   \
             (s)--)

#define STRV_FOREACH_PAIR(x, y, l)               \
        for ((x) = (l), (y) = (x+1); (x) && *(x) && *(y); (x) += 2, (y) = (x + 1))

char **strv_sort(char **l);
void strv_print(char **l);

#define STRV_MAKE(...) ((char**) ((const char*[]) { __VA_ARGS__, NULL }))

#define STRV_MAKE_EMPTY ((char*[1]) { NULL })

#define strv_from_stdarg_alloca(first)                          \
        ({                                                      \
                char **_l;                                      \
                                                                \
                if (!first)                                     \
                        _l = (char**) &first;                   \
                else {                                          \
                        size_t _n;                              \
                        va_list _ap;                            \
                                                                \
                        _n = 1;                                 \
                        va_start(_ap, first);                   \
                        while (va_arg(_ap, char*))              \
                                _n++;                           \
                        va_end(_ap);                            \
                                                                \
                        _l = newa(char*, _n+1);                 \
                        _l[_n = 0] = (char*) first;             \
                        va_start(_ap, first);                   \
                        for (;;) {                              \
                                _l[++_n] = va_arg(_ap, char*);  \
                                if (!_l[_n])                    \
                                        break;                  \
                        }                                       \
                        va_end(_ap);                            \
                }                                               \
                _l;                                             \
        })

#define STR_IN_SET(x, ...) strv_contains(STRV_MAKE(__VA_ARGS__), x)
#define STRPTR_IN_SET(x, ...)                                    \
        ({                                                       \
                const char* _x = (x);                            \
                _x && strv_contains(STRV_MAKE(__VA_ARGS__), _x); \
        })

#define FOREACH_STRING(x, ...)                               \
        for (char **_l = ({                                  \
                char **_ll = STRV_MAKE(__VA_ARGS__);         \
                x = _ll ? _ll[0] : NULL;                     \
                _ll;                                         \
        });                                                  \
        _l && *_l;                                           \
        x = ({                                               \
                _l ++;                                       \
                _l[0];                                       \
        }))

#if 0 /// UNNEEDED by elogind
char **strv_reverse(char **l);
char **strv_shell_escape(char **l, const char *bad);

bool strv_fnmatch(char* const* patterns, const char *s, int flags);

static inline bool strv_fnmatch_or_empty(char* const* patterns, const char *s, int flags) {
        assert(s);
        return strv_isempty(patterns) ||
               strv_fnmatch(patterns, s, flags);
}

char ***strv_free_free(char ***l);
DEFINE_TRIVIAL_CLEANUP_FUNC(char***, strv_free_free);

char **strv_skip(char **l, size_t n);
#endif // 0

int strv_extend_n(char ***l, const char *value, size_t n);

#if 0 /// UNNEEDED by elogind
int fputstrv(FILE *f, char **l, const char *separator, bool *space);
#endif // 0

#define strv_free_and_replace(a, b)             \
        ({                                      \
                strv_free(a);                   \
                (a) = (b);                      \
                (b) = NULL;                     \
                0;                              \
        })
