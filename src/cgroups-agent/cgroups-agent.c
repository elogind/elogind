/* SPDX-License-Identifier: LGPL-2.1+ */
/***
  This file is part of systemd.

  Copyright 2010 Lennart Poettering
***/

#include <stdlib.h>
#include <sys/socket.h>

#include "fd-util.h"
#include "log.h"
#include "socket-util.h"

/// Additional includes needed by elogind
#include "musl_missing.h"

int main(int argc, char *argv[]) {

        static const union sockaddr_union sa = {
                .un.sun_family = AF_UNIX,
                .un.sun_path = "/run/systemd/cgroups-agent",
        };

        _cleanup_close_ int fd = -1;
        ssize_t n;
        size_t l;

        if (argc != 2) {
                log_error("Incorrect number of arguments.");
                return EXIT_FAILURE;
        }

        elogind_set_program_name(argv[0]);
        log_set_target(LOG_TARGET_AUTO);
        log_parse_environment();
        log_open();

        fd = socket(AF_UNIX, SOCK_DGRAM|SOCK_CLOEXEC, 0);
        if (fd < 0) {
                log_debug_errno(errno, "Failed to allocate socket: %m");
                return EXIT_FAILURE;
        }

        l = strlen(argv[1]);

        n = sendto(fd, argv[1], l, 0, &sa.sa, SOCKADDR_UN_LEN(sa.un));
        if (n < 0) {
                log_debug_errno(errno, "Failed to send cgroups agent message: %m");
                return EXIT_FAILURE;
        }

        if ((size_t) n != l) {
                log_debug("Datagram size mismatch");
                return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
}
