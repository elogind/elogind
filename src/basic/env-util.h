/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>

#include "macro.h"
#include "string.h"

static inline size_t sc_arg_max(void) {
        long l = sysconf(_SC_ARG_MAX);
        assert(l > 0);
        return (size_t) l;
}

bool env_name_is_valid(const char *e);
bool env_value_is_valid(const char *e);
bool env_assignment_is_valid(const char *e);

typedef enum ReplaceEnvFlags {
        REPLACE_ENV_USE_ENVIRONMENT = 1 << 0,
        REPLACE_ENV_ALLOW_BRACELESS = 1 << 1,
        REPLACE_ENV_ALLOW_EXTENDED  = 1 << 2,
} ReplaceEnvFlags;

#if 0 /// UNNEEDED by elogind
int replace_env_full(const char *format, size_t n, char **env, ReplaceEnvFlags flags, char **ret, char ***ret_unset_variables, char ***ret_bad_variables);
static inline int replace_env(const char *format, char **env, ReplaceEnvFlags flags, char **ret) {
        return replace_env_full(format, SIZE_MAX, env, flags, ret, NULL, NULL);
}

int replace_env_argv(char **argv, char **env, char ***ret, char ***ret_unset_variables, char ***ret_bad_variables);

bool strv_env_is_valid(char **e);
#define strv_env_clean(l) strv_env_clean_with_callback(l, NULL, NULL)
char **strv_env_clean_with_callback(char **l, void (*invalid_callback)(const char *p, void *userdata), void *userdata);

bool strv_env_name_is_valid(char **l);
bool strv_env_name_or_assignment_is_valid(char **l);

char** _strv_env_merge(char **first, ...);
#define strv_env_merge(first, ...) _strv_env_merge(first, __VA_ARGS__, POINTER_MAX)
char **strv_env_delete(char **x, size_t n_lists, ...); /* New copy */
#endif // 0

char **strv_env_unset(char **l, const char *p); /* In place ... */

#if 0 /// UNNEEDED by elogind
char **strv_env_unset_many(char **l, ...) _sentinel_;
#endif // 0
int strv_env_replace_consume(char ***l, char *p); /* In place ... */
int strv_env_replace_strdup(char ***l, const char *assignment);
#if 0 /// UNNEEDED by elogind
int strv_env_replace_strdup_passthrough(char ***l, const char *assignment);
int strv_env_assign(char ***l, const char *key, const char *value);
int _strv_env_assign_many(char ***l, ...) _sentinel_;
#define strv_env_assign_many(l, ...) _strv_env_assign_many(l, __VA_ARGS__, NULL)

char *strv_env_get_n(char **l, const char *name, size_t k, ReplaceEnvFlags flags) _pure_;
static inline char *strv_env_get(char **x, const char *n) {
        return strv_env_get_n(x, n, SIZE_MAX, 0);
}

char *strv_env_pairs_get(char **l, const char *name) _pure_;
#endif // 0

int getenv_bool(const char *p);
int getenv_bool_secure(const char *p);

#if 0 /// UNNEEDED by elogind
int getenv_uint64_secure(const char *p, uint64_t *ret);
#endif // 0

/* Like setenv, but calls unsetenv if value == NULL. */
int set_unset_env(const char *name, const char *value, bool overwrite);

#if 0 /// UNNEEDED by elogind
/* Like putenv, but duplicates the memory like setenv. */
int putenv_dup(const char *assignment, bool override);
#endif // 0

int setenv_elogind_exec_pid(bool update_only);

#if 0 /// UNNEEDED by elogind
/* Parses and does sanity checks on an environment variable containing
 * PATH-like colon-separated absolute paths */
int getenv_path_list(const char *name, char ***ret_paths);

int getenv_steal_erase(const char *name, char **ret);
#endif // 0
