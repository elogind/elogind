/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

#include <stdbool.h>
//#include <stdio.h>

#include "macro.h"

#if 0 /// UNNEEDED by elogind
bool hostname_is_set(void);
#endif // 0

char* gethostname_malloc(void);
#if 0 /// UNNEEDED by elogind
int gethostname_strict(char **ret);
#endif // 0

bool valid_ldh_char(char c) _const_;
bool hostname_is_valid(const char *s, bool allow_trailing_dot) _pure_;
#if 0 /// UNNEEDED by elogind
char* hostname_cleanup(char *s);
#endif // 0

#define machine_name_is_valid(s) hostname_is_valid(s, false)

bool is_localhost(const char *hostname);
#if 0 /// UNNEEDED by elogind
bool is_gateway_hostname(const char *hostname);

int sethostname_idempotent(const char *s);

#endif // 0
int shorten_overlong(const char *s, char **ret);

int read_etc_hostname_stream(FILE *f, char **ret);
int read_etc_hostname(const char *path, char **ret);
