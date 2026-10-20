/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#if 0 /// elogind has its manager in logind.h
#include "manager.h"
#else // 0
#include "logind.h"
#endif // 0

int manager_setup_varlink_server(Manager *m);

int manager_varlink_init(Manager *m);
void manager_varlink_done(Manager *m);

#if 0 /// UNNEEDED by elogind - we do not handle OOM situations
/* The manager is expected to send an update to systemd-oomd if one of the following occurs:
 * - The value of ManagedOOM*= properties change
 * - A unit with ManagedOOM*= properties changes unit active state */
int manager_varlink_send_managed_oom_update(Unit *u);
#endif // 0
