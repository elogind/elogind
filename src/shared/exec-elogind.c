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


#include <errno.h>


#include "exec-elogind.h"
#include "fd-util.h"
#include "fileio.h"
#include "log.h"
#include "logind.h"
#include "sleep-config.h"
#include "string-util.h"



static int gather_output_generate(int const fd, void *arg) {
        _cleanup_fclose_ FILE *f = NULL;
        SleepConfig* sleep_config = arg;
        unsigned line = 0;
        int r;

        /* Read and log lines from fd. Check if any line begins with a keyword
         * representing failure. Set callback_failed to true if such a keyword
         * is found.
         *
         * fd is always consumed, even on error.
         */
        f = fdopen(fd, "r");
        if (!f) {
                safe_close(fd);
                log_error_errno(errno, "Failed to open serialization fd: %m");
                if (sleep_config->callback_must_succeed)
                        sleep_config->callback_failed = true;
                return -errno;
        }

        for (;;) {
                _cleanup_free_ char *buf = NULL;

                r = read_line(f, LONG_LINE_MAX, &buf);

                if (r == 0)
                        break;
                ++line;
                if (r == -ENOBUFS) {
                        log_error_errno(r, "ERROR: Line %u too long", line);
                        if (sleep_config->callback_must_succeed)
                                sleep_config->callback_failed = true;
                        return r;
                }
                if (r < 0) {
                        log_error_errno(r, "ERROR: Line %u could not be read: %m", line);
                        if (sleep_config->callback_must_succeed)
                                sleep_config->callback_failed = true;
                        return r;
                }

                log_debug_elogind(" =>[%s]", buf);

                if ( startswith_no_case(buf, "cancelled")
                  || startswith_no_case(buf, "critical" )
                  || startswith_no_case(buf, "error"    )
                  || startswith_no_case(buf, "failed"   ) ) {
                        log_error_errno(ECANCELED, "Script failed at line %u: %s", line, buf);
                        if (sleep_config->callback_must_succeed) {
                                sleep_config->callback_failed = true;
                                return -ECANCELED;
                        }
                        return ECANCELED;
                }
        }

        return r;
}

static int gather_output_collect(int fd, void *arg) {
        /* Nothing to do here. All we wanted has happened in gather_output_generate() */
        return 0;
}

static int gather_output_consume(int fd, void *arg) {
        /* Nothing to do here. All we wanted has happened in gather_output_generate() */
        return 0;
}

const gather_stdout_callback_t gather_output[] = {
        gather_output_generate,
        gather_output_collect,
        gather_output_consume,
};
