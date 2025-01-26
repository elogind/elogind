/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include "sd-netlink.h"

#if 0 /// UNNEEDED by elogind
int netlink_slot_allocate(
                sd_netlink *nl,
                bool floating,
                NetlinkSlotType type,
                size_t extra,
                void *userdata,
                const char *description,
                sd_netlink_slot **ret);
#endif // 0
void netlink_slot_disconnect(sd_netlink_slot *slot, bool unref);
