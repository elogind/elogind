#ifndef __GLIBC__
#include <string.h>
#include "util.h"

char *program_invocation_name       = NULL;
char *program_invocation_short_name = NULL;
#endif // __GLIBC__

#include "musl_missing.h"

static void elogind_free_program_name(void) {
#ifndef __GLIBC__
        if (program_invocation_name)
                program_invocation_name       = mfree(program_invocation_name);
        if (program_invocation_short_name)
                program_invocation_short_name = mfree(program_invocation_short_name);
#endif // __GLIBC__
}

void elogind_set_program_name(const char* pcall) {
        assert(pcall && pcall[0]);
        elogind_free_program_name();
#ifndef __GLIBC__
        program_invocation_name       = strdup(pcall);
        program_invocation_short_name = strdup(basename(pcall));
        atexit(elogind_free_program_name);
#endif // __GLIBC__
}

