/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

#include "bus-object.h"

#if 1 /// NEEDED by elogogind
extern bool daemonize;
#endif // 1

int service_parse_argv(
                const char *service,
                const char *description,
                const BusObjectImplementation* const* bus_objects,
                int argc, char *argv[]);
