/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

#include <stdbool.h>

#include "macro.h"

int pager_open(bool no_pager, bool jump_to_end);
void pager_close(void);
bool pager_have(void) _pure_;

#if 0 /// UNNEEDED by elogind
int show_man_page(const char *page, bool null_stdio);
#endif // 0
