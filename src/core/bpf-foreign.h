/* SPDX-License-Identifier: LGPL-2.1+ */

#pragma once

#include "unit.h"

int bpf_foreign_supported(void);
/*
 * Attach cgroup-bpf programs foreign to elogind, i.e. loaded to the kernel by an entity
 * external to elogind.
 */
int bpf_foreign_install(Unit *u);
