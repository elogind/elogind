/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <errno.h>
#include <getopt.h>
#include <stdbool.h>
#include <stddef.h>

#include "env-util.h"
#include "log.h"
#include "macro.h"
#include "process-util.h"
#include "string-util.h"
#include "verbs.h"
#include "virt.h"

/* Wraps running_in_chroot() which is used in various places, but also adds an environment variable check
 * so external processes can reliably force this on. */
bool running_in_chroot_or_offline(void) {
        int r;

        /* Added to support use cases like rpm-ostree, where from %post scripts we only want to execute "preset",
         * but not "start"/"restart" for example.
         *
         * See docs/ENVIRONMENT.md for docs.
         */
        r = getenv_bool("SYSTEMD_OFFLINE");
        if (r >= 0)
                return r > 0;
        if (r != -ENXIO)
                log_debug_errno(r, "Failed to parse $SYSTEMD_OFFLINE, ignoring: %m");

        /* We've had this condition check for a long time which basically checks for legacy chroot case like Fedora's
         * "mock", which is used for package builds.  We don't want to try to start systemd services there, since
         * without --new-chroot we don't even have systemd running, and even if we did, adding a concept of background
         * daemons to builds would be an enormous change, requiring considering things like how the journal output is
         * handled, etc.  And there's really not a use case today for a build talking to a service.
         *
         * Note this call itself also looks for a different variable SYSTEMD_IGNORE_CHROOT=1.
         */
        r = running_in_chroot();
        if (r < 0)
                log_debug_errno(r, "Failed to check if we're running in chroot, assuming not: %m");
        return r > 0;
}

const Verb* verbs_find_verb(const char *name, const Verb verbs[]) {
        for (size_t i = 0; verbs[i].dispatch; i++)
                if (streq_ptr(name, verbs[i].verb) ||
                    (!name && FLAGS_SET(verbs[i].flags, VERB_DEFAULT)))
                        return &verbs[i];

        /* At the end of the list? */
        return NULL;
}

int dispatch_verb(int argc, char *argv[], const Verb verbs[], void *userdata) {
        const Verb *verb;
        const char *name;
        int left;

        assert(verbs);
        assert(verbs[0].dispatch);
        assert(argc >= 0);
        assert(argv);
        assert(argc >= optind);

        left = argc - optind;
        argv += optind;
        optind = 0;
        name = argv[0];

        verb = verbs_find_verb(name, verbs);
        if (!verb) {
                if (name)
                        return log_error_errno(SYNTHETIC_ERRNO(EINVAL),
                                               "Unknown command verb %s.", name);
                else
                        return log_error_errno(SYNTHETIC_ERRNO(EINVAL),
                                               "Command verb required.");
        }

        if (!name)
                left = 1;

        if (verb->min_args != VERB_ANY &&
            (unsigned) left < verb->min_args)
                return log_error_errno(SYNTHETIC_ERRNO(EINVAL),
                                       "Too few arguments.");

        if (verb->max_args != VERB_ANY &&
            (unsigned) left > verb->max_args)
                return log_error_errno(SYNTHETIC_ERRNO(EINVAL),
                                       "Too many arguments.");

        if ((verb->flags & VERB_ONLINE_ONLY) && running_in_chroot_or_offline()) {
                log_info("Running in chroot, ignoring command '%s'", name ?: verb->verb);
                return 0;
        }

        if (name)
                return verb->dispatch(left, argv, userdata);
        else {
                char* fake[2] = {
                        (char*) verb->verb,
                        NULL
                };

                return verb->dispatch(1, fake, userdata);
        }
}
