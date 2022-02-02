/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <stddef.h>

int acquire_data_fd(const void *data, size_t size, unsigned flags);
#if 0 /// UNNEEDED by elogind
int copy_data_fd(int fd);
#endif // 0
