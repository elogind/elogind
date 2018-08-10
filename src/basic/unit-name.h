/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

/***
  Copyright 2010 Lennart Poettering
***/

#include <stdbool.h>

#include "macro.h"
#include "unit-def.h"

#define UNIT_NAME_MAX 256

#if 0 /// UNNEEDED by elogind
#endif // 0
typedef enum UnitNameFlags {
        UNIT_NAME_PLAIN = 1,      /* Allow foo.service */
        UNIT_NAME_INSTANCE = 2,   /* Allow foo@bar.service */
        UNIT_NAME_TEMPLATE = 4,   /* Allow foo@.service */
        UNIT_NAME_ANY = UNIT_NAME_PLAIN|UNIT_NAME_INSTANCE|UNIT_NAME_TEMPLATE,
} UnitNameFlags;

bool unit_name_is_valid(const char *n, UnitNameFlags flags) _pure_;
bool unit_prefix_is_valid(const char *p) _pure_;
bool unit_instance_is_valid(const char *i) _pure_;
bool unit_suffix_is_valid(const char *s) _pure_;

#if 0 /// UNNEEDED by elogind
static inline int unit_prefix_and_instance_is_valid(const char *p) {
        /* For prefix+instance and instance the same rules apply */
        return unit_instance_is_valid(p);
}

int unit_name_to_prefix(const char *n, char **prefix);
int unit_name_to_instance(const char *n, char **instance);
int unit_name_to_prefix_and_instance(const char *n, char **ret);
#endif // 0

UnitType unit_name_to_type(const char *n) _pure_;

#if 0 /// UNNEEDED by elogind
int unit_name_change_suffix(const char *n, const char *suffix, char **ret);
#endif // 0

int unit_name_build(const char *prefix, const char *instance, const char *suffix, char **ret);
int unit_name_build_from_type(const char *prefix, const char *instance, UnitType, char **ret);

#if 0 /// UNNEEDED by elogind
char *unit_name_escape(const char *f);
int unit_name_unescape(const char *f, char **ret);
int unit_name_path_escape(const char *f, char **ret);
int unit_name_path_unescape(const char *f, char **ret);

int unit_name_replace_instance(const char *f, const char *i, char **ret);

int unit_name_template(const char *f, char **ret);

int unit_name_from_path(const char *path, const char *suffix, char **ret);
int unit_name_from_path_instance(const char *prefix, const char *path, const char *suffix, char **ret);
int unit_name_to_path(const char *name, char **ret);

typedef enum UnitNameMangle {
        UNIT_NAME_MANGLE_GLOB = 1,
        UNIT_NAME_MANGLE_WARN = 2,
} UnitNameMangle;

int unit_name_mangle_with_suffix(const char *name, UnitNameMangle flags, const char *suffix, char **ret);

static inline int unit_name_mangle(const char *name, UnitNameMangle flags, char **ret) {
        return unit_name_mangle_with_suffix(name, flags, ".service", ret);
}

int slice_build_parent_slice(const char *slice, char **ret);
#endif // 0
int slice_build_subslice(const char *slice, const char*name, char **subslice);
bool slice_name_is_valid(const char *name);
#if 0 /// UNNEEDED by elogind
#endif // 0
