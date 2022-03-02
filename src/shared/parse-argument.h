/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include "json.h"

int parse_boolean_argument(const char *optname, const char *s, bool *ret);
int parse_json_argument(const char *s, JsonFormatFlags *ret);
#if 0 /// UNNEEDED by elogind
int parse_path_argument(const char *path, bool suppress_root, char **arg);
int parse_signal_argument(const char *s, int *ret);
#endif // 0
