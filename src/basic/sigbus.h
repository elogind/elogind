/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

void sigbus_install(void);
#if 0 /// UNNEEDED by elogind
void sigbus_reset(void);

int sigbus_pop(void **ret);
#endif // 0
