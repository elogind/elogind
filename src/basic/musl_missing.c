#include <string.h>
#include "util.h"

#ifndef __GLIBC__
char *program_invocation_name       = NULL;
char *program_invocation_short_name = NULL;
#endif // __GLIBC__

#include "musl_missing.h"

static void elogind_free_program_name(void) {
        if (program_invocation_name)
                program_invocation_name       = mfree(program_invocation_name);
        if (program_invocation_short_name)
                program_invocation_short_name = mfree(program_invocation_short_name);
}

void elogind_set_program_name(const char* pcall) {
        assert(pcall && pcall[0]);

        if ( ( program_invocation_name
            && strcmp(program_invocation_name, pcall))
          || ( program_invocation_short_name
            && strcmp(program_invocation_short_name, basename(pcall)) ) )
                elogind_free_program_name();

        if (NULL == program_invocation_name)
                program_invocation_name       = strdup(pcall);
        if (NULL == program_invocation_short_name)
                program_invocation_short_name = strdup(basename(pcall));

#ifndef __GLIBC__
        atexit(elogind_free_program_name);
#endif // __GLIBC__
}

