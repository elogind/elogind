/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

/***
  This file is part of systemd.

  Copyright 2010 Lennart Poettering
***/

#include <stdbool.h>

#include "log.h"

enum {
        PROC_CMDLINE_STRIP_RD_PREFIX = 1,
        PROC_CMDLINE_VALUE_OPTIONAL = 2,
};

typedef int (*proc_cmdline_parse_t)(const char *key, const char *value, void *data);

int proc_cmdline(char **ret);

int proc_cmdline_parse(const proc_cmdline_parse_t parse, void *userdata, unsigned flags);

#if 0 /// UNNEEDED by elogind
int proc_cmdline_get_key(const char *parameter, unsigned flags, char **value);
int proc_cmdline_get_bool(const char *key, bool *ret);

char *proc_cmdline_key_startswith(const char *s, const char *prefix);
#endif // 0
bool proc_cmdline_key_streq(const char *x, const char *y);

#if 0 /// UNNEEDED by elogind
int shall_restore_state(void);
const char* runlevel_to_target(const char *rl);
#endif // 0

/* A little helper call, to be used in proc_cmdline_parse_t callbacks */
static inline bool proc_cmdline_value_missing(const char *key, const char *value) {
        if (!value) {
                log_warning("Missing argument for %s= kernel command line switch, ignoring.", key);
                return true;
        }

        return false;
}
