/* SPDX-License-Identifier: LGPL-2.1+ */
/***
  This file is part of elogind.

  Copyright 2017 Sven Eden

  elogind is free software; you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation; either version 2.1 of the License, or
  (at your option) any later version.

  elogind is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with elogind; If not, see <http://www.gnu.org/licenses/>.
***/

#if HAVE_PROGRAM_INVOCATION_NAME == 0
#include <errno.h>
#include <string.h>
#endif // libc does not provide program_invocation_[short_]name

#include "alloc-util.h"

#if HAVE_PROGRAM_INVOCATION_NAME == 0
char *program_invocation_name       = NULL;
char *program_invocation_short_name = NULL;

const char *program_arg_name = NULL; /* Helper */
#endif // libc does not provide program_invocation_[short_]name

#include "musl_missing.h"

#if HAVE_PROGRAM_INVOCATION_NAME == 0
static void elogind_free_program_name(void) {
        if (program_invocation_name)
                program_invocation_name       = mfree(program_invocation_name);
        if (program_invocation_short_name)
                program_invocation_short_name = mfree(program_invocation_short_name);
}

void elogind_set_program_name(const char* pcall) {
        assert(pcall && pcall[0]);

        program_arg_name = pcall;

        if ( ( program_invocation_name
            && strcmp(program_invocation_name, program_arg_name))
          || ( program_invocation_short_name
            && strcmp(program_invocation_short_name, basename(program_arg_name)) ) )
                elogind_free_program_name();

        if (NULL == program_invocation_name)
                program_invocation_name       = strdup(program_arg_name);
        if (NULL == program_invocation_short_name)
                program_invocation_short_name = strdup(basename(program_arg_name));
        atexit(elogind_free_program_name);
}
#else // 0
void elogind_set_program_name(const char* pcall) {
        assert(pcall && pcall[0]);
}
#endif // libc does not provide program_invocation_[short_]name

