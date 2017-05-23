/***
  This file is part of systemd.

  Copyright 2013 Zbigniew Jędrzejewski-Szmek

  systemd is free software; you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation; either version 2.1 of the License, or
  (at your option) any later version.

  systemd is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with systemd; If not, see <http://www.gnu.org/licenses/>.
***/

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
//#include <syslog.h>
//#include <unistd.h>

#include "sd-messages.h"

//#include "alloc-util.h"
//#include "conf-parser.h"
//#include "def.h"
#include "fd-util.h"
#include "fileio.h"
#include "log.h"
#include "logind-sleep.h"
//#include "macro.h"
#include "parse-util.h"
#include "string-util.h"
#include "strv.h"

static int can_sleep_state(char **types) {
        char **type;
        int r;
        _cleanup_free_ char *p = NULL;

        if (strv_isempty(types))
                return true;

        /* If /sys is read-only we cannot sleep */
        if (access("/sys/power/state", W_OK) < 0)
                return false;

        r = read_one_line_file("/sys/power/state", &p);
        if (r < 0)
                return false;

        STRV_FOREACH(type, types) {
                const char *word, *state;
                size_t l, k;

                k = strlen(*type);
                FOREACH_WORD_SEPARATOR(word, l, p, WHITESPACE, state)
                        if (l == k && memcmp(word, *type, l) == 0)
                                return true;
        }

        return false;
}

static int can_sleep_disk(char **types) {
        char **type;
        int r;
        _cleanup_free_ char *p = NULL;

        if (strv_isempty(types))
                return true;

        /* If /sys is read-only we cannot sleep */
        if (access("/sys/power/disk", W_OK) < 0)
                return false;

        r = read_one_line_file("/sys/power/disk", &p);
        if (r < 0)
                return false;

        STRV_FOREACH(type, types) {
                const char *word, *state;
                size_t l, k;

                k = strlen(*type);
                FOREACH_WORD_SEPARATOR(word, l, p, WHITESPACE, state) {
                        if (l == k && memcmp(word, *type, l) == 0)
                                return true;

                        if (l == k + 2 &&
                            word[0] == '[' &&
                            memcmp(word + 1, *type, l - 2) == 0 &&
                            word[l-1] == ']')
                                return true;
                }
        }

        return false;
}

#define HIBERNATION_SWAP_THRESHOLD 0.98

static int hibernation_partition_size(size_t *size, size_t *used) {
        _cleanup_fclose_ FILE *f;
        unsigned i;

        assert(size);
        assert(used);

        f = fopen("/proc/swaps", "re");
        if (!f) {
                log_full(errno == ENOENT ? LOG_DEBUG : LOG_WARNING,
                         "Failed to retrieve open /proc/swaps: %m");
                assert(errno > 0);
                return -errno;
        }

        (void) fscanf(f, "%*s %*s %*s %*s %*s\n");

        for (i = 1;; i++) {
                _cleanup_free_ char *dev = NULL, *type = NULL;
                size_t size_field, used_field;
                int k;

                k = fscanf(f,
                           "%ms "   /* device/file */
                           "%ms "   /* type of swap */
                           "%zu "   /* swap size */
                           "%zu "   /* used */
                           "%*i\n", /* priority */
                           &dev, &type, &size_field, &used_field);
                if (k != 4) {
                        if (k == EOF)
                                break;

                        log_warning("Failed to parse /proc/swaps:%u", i);
                        continue;
                }

                if (streq(type, "partition") && endswith(dev, "\\040(deleted)")) {
                        log_warning("Ignoring deleted swapfile '%s'.", dev);
                        continue;
                }

                *size = size_field;
                *used = used_field;
                return 0;
        }

        log_debug("No swap partitions were found.");
        return -ENOSYS;
}

static bool enough_memory_for_hibernation(void) {
        _cleanup_free_ char *active = NULL;
        unsigned long long act = 0;
        size_t size = 0, used = 0;
        int r;

        r = hibernation_partition_size(&size, &used);
        if (r < 0)
                return false;

        r = get_proc_field("/proc/meminfo", "Active(anon)", WHITESPACE, &active);
        if (r < 0) {
                log_error_errno(r, "Failed to retrieve Active(anon) from /proc/meminfo: %m");
                return false;
        }

        r = safe_atollu(active, &act);
        if (r < 0) {
                log_error_errno(r, "Failed to parse Active(anon) from /proc/meminfo: %s: %m",
                                active);
                return false;
        }

        r = act <= (size - used) * HIBERNATION_SWAP_THRESHOLD;
        log_debug("Hibernation is %spossible, Active(anon)=%llu kB, size=%zu kB, used=%zu kB, threshold=%.2g%%",
                  r ? "" : "im", act, size, used, 100*HIBERNATION_SWAP_THRESHOLD);

        return r;
}

int can_sleep(Manager *m, const char *verb) {

        assert(streq(verb, "suspend") ||
               streq(verb, "hibernate") ||
               streq(verb, "hybrid-sleep"));

        if ( streq(verb, "suspend")
          && ( !can_sleep_state(m->suspend_state)
            || !can_sleep_disk(m->suspend_mode) ) )
                return false;

        if ( streq(verb, "hibernate")
          && ( !can_sleep_state(m->hibernate_state)
            || !can_sleep_disk(m->hibernate_mode) ) )
                return false;

        if ( streq(verb, "hybrid-sleep")
          && ( !can_sleep_state(m->hybrid_sleep_state)
            || !can_sleep_disk(m->hybrid_sleep_mode) ) )
                return false;


        return streq(verb, "suspend") || enough_memory_for_hibernation();
}

static int write_mode(char **modes) {
        int r = 0;
        char **mode;

        STRV_FOREACH(mode, modes) {
                int k;

                k = write_string_file("/sys/power/disk", *mode, 0);
                if (k == 0)
                        return 0;

                log_debug_errno(k, "Failed to write '%s' to /sys/power/disk: %m",
                                *mode);
                if (r == 0)
                        r = k;
        }

        if (r < 0)
                log_error_errno(r, "Failed to write mode to /sys/power/disk: %m");

        return r;
}

static int write_state(FILE **f, char **states) {
        char **state;
        int r = 0;

        STRV_FOREACH(state, states) {
                int k;

                k = write_string_stream(*f, *state, true);
                if (k == 0)
                        return 0;
                log_debug_errno(k, "Failed to write '%s' to /sys/power/state: %m",
                                *state);
                if (r == 0)
                        r = k;

                fclose(*f);
                *f = fopen("/sys/power/state", "we");
                if (!*f)
                        return log_error_errno(errno, "Failed to open /sys/power/state: %m");
        }

        return r;
}

int do_sleep(const char *arg_verb, char **modes, char **states) {

        char *arguments[] = {
                NULL,
                (char*) "pre",
                (char*) arg_verb,
                NULL
        };
        static const char* const dirs[] = {SYSTEM_SLEEP_PATH, NULL};

        int r;
        _cleanup_fclose_ FILE *f = NULL;

        /* This file is opened first, so that if we hit an error,
         * we can abort before modifying any state. */
        f = fopen("/sys/power/state", "we");
        if (!f)
                return log_error_errno(errno, "Failed to open /sys/power/state: %m");

        /* Configure the hibernation mode */
        r = write_mode(modes);
        if (r < 0)
                return r;

        execute_directories(dirs, DEFAULT_TIMEOUT_USEC, arguments);

        log_struct(LOG_INFO,
                   LOG_MESSAGE_ID(SD_MESSAGE_SLEEP_START),
                   LOG_MESSAGE("Suspending system..."),
                   "SLEEP=%s", arg_verb,
                   NULL);

        r = write_state(&f, states);
        if (r < 0)
                return r;

        log_struct(LOG_INFO,
                   LOG_MESSAGE_ID(SD_MESSAGE_SLEEP_STOP),
                   LOG_MESSAGE("System resumed."),
                   "SLEEP=%s", arg_verb,
                   NULL);

        arguments[1] = (char*) "post";
        execute_directories(dirs, DEFAULT_TIMEOUT_USEC, arguments);

        return r;
}

