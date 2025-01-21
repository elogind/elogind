/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include "sd-bus.h"

#include "hashmap.h"
#include "user-util.h"
#include "varlink.h"

int bus_test_polkit(sd_bus_message *call, const char *action, const char **details, uid_t good_user, bool *_challenge, sd_bus_error *e);

int bus_verify_polkit_async_full(sd_bus_message *call, const char *action, const char **details, bool interactive, uid_t good_user, Hashmap **registry, sd_bus_error *error);
static inline int bus_verify_polkit_async(sd_bus_message *call, const char *action, const char **details, Hashmap **registry, sd_bus_error *ret_error) {
        return bus_verify_polkit_async_full(call, action, details, false, UID_INVALID, registry, ret_error);
}

int varlink_verify_polkit_async(Varlink *link, sd_bus *bus, const char *action, const char **details, uid_t good_user, Hashmap **registry);

/* A JsonDispatch initializer that makes sure the allowInteractiveAuthentication boolean field we want for
 * polkit support in Varlink calls is ignored while regular dispatching (and does not result in errors
 * regarding unexpected fields) */
#define VARLINK_DISPATCH_POLKIT_FIELD {                          \
                .name = "allowInteractiveAuthentication",        \
                .type = JSON_VARIANT_BOOLEAN,                    \
        }
