/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

#include <stddef.h>

#include "macro.h"

size_t strpcpy(char **dest, size_t size, const char *src);
#if 0 /// UNNEEDED by elogind
size_t strpcpyf(char **dest, size_t size, const char *src, ...) _printf_(3, 4);
size_t strpcpyl(char **dest, size_t size, const char *src, ...) _sentinel_;
#endif // 0
size_t strscpy(char *dest, size_t size, const char *src);
size_t strscpyl(char *dest, size_t size, const char *src, ...) _sentinel_;
