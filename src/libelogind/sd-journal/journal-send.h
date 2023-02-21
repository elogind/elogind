/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <stdbool.h>

#if 0 /// UNNEEDED by elogind
int journal_fd_nonblock(bool nonblock);
#endif // 0

void close_journal_fd(void);
