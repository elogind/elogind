/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <stdio.h>

#include "sd-bus.h"

#include "memstream-util.h"
#include "ordered-set.h"

struct introspect {
        MemStream m;
        char *interface_name;
        bool trusted;
};

int introspect_begin(struct introspect *i, bool trusted);
int introspect_write_default_interfaces(struct introspect *i, bool object_manager);
int introspect_write_child_nodes(struct introspect *i, OrderedSet *s, const char *prefix);
int introspect_write_interface(
                struct introspect *i,
                const char *interface_name,
                const sd_bus_vtable *v);
int introspect_finish(struct introspect *i, char **ret);
void introspect_done(struct introspect *i);
