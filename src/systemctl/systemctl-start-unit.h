/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include "systemctl.h"

#if 0 /// UNNEEDED by elogind
int verb_start(int argc, char *argv[], void *userdata);
#endif /// 0

struct action_metadata {
        const char *target;
        const char *verb;
        const char *mode;
};

extern const struct action_metadata action_table[_ACTION_MAX];

enum action verb_to_action(const char *verb);
