/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

/***
  Copyright 2013 Lennart Poettering
***/

#include "sd-bus.h"

int bus_add_match_internal(sd_bus *bus, const char *match);
int bus_add_match_internal_async(sd_bus *bus, sd_bus_slot **ret, const char *match, sd_bus_message_handler_t callback, void *userdata);

int bus_remove_match_internal(sd_bus *bus, const char *match);
