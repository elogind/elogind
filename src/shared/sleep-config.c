/* SPDX-License-Identifier: LGPL-2.1+ */
/***
  This file is part of systemd.

  Copyright 2013 Zbigniew Jędrzejewski-Szmek
  Copyright 2018 Dell Inc.
***/

//#include <errno.h>
//#include <stdbool.h>
//#include <stddef.h>
//#include <stdio.h>
//#include <string.h>
//#include <syslog.h>
//#include <unistd.h>

#include "alloc-util.h"
//#include "conf-parser.h"
//#include "def.h"
//#include "env-util.h"
#include "fd-util.h"
#include "fileio.h"
//#include "log.h"
//#include "macro.h"
#include "parse-util.h"
#include "sleep-config.h"
#include "string-util.h"
#include "strv.h"

#if 0 /// UNNEEDED by elogind
int parse_sleep_config(const char *verb, char ***_modes, char ***_states, usec_t *_delay) {

        _cleanup_strv_free_ char
                **suspend_mode = NULL, **suspend_state = NULL,
                **hibernate_mode = NULL, **hibernate_state = NULL,
                **hybrid_mode = NULL, **hybrid_state = NULL;
        char **modes, **states;
        usec_t delay = 180 * USEC_PER_MINUTE;

        const ConfigTableItem items[] = {
                { "Sleep",   "SuspendMode",      config_parse_strv,  0, &suspend_mode  },
                { "Sleep",   "SuspendState",     config_parse_strv,  0, &suspend_state },
                { "Sleep",   "HibernateMode",    config_parse_strv,  0, &hibernate_mode  },
                { "Sleep",   "HibernateState",   config_parse_strv,  0, &hibernate_state },
                { "Sleep",   "HybridSleepMode",  config_parse_strv,  0, &hybrid_mode  },
                { "Sleep",   "HybridSleepState", config_parse_strv,  0, &hybrid_state },
                { "Sleep",   "HibernateDelaySec", config_parse_sec,  0, &delay},
                {}
        };

        (void) config_parse_many_nulstr(PKGSYSCONFDIR "/sleep.conf",
                                        CONF_PATHS_NULSTR("elogind/sleep.conf.d"),
                                        "Sleep\0", config_item_table_lookup, items,
                                        CONFIG_PARSE_WARN, NULL);

        if (streq(verb, "suspend")) {
                /* empty by default */
                modes = TAKE_PTR(suspend_mode);

                if (suspend_state)
                        states = TAKE_PTR(suspend_state);
                else
                        states = strv_new("mem", "standby", "freeze", NULL);

        } else if (streq(verb, "hibernate")) {
                if (hibernate_mode)
                        modes = TAKE_PTR(hibernate_mode);
                else
                        modes = strv_new("platform", "shutdown", NULL);

                if (hibernate_state)
                        states = TAKE_PTR(hibernate_state);
                else
                        states = strv_new("disk", NULL);

        } else if (streq(verb, "hybrid-sleep")) {
                if (hybrid_mode)
                        modes = TAKE_PTR(hybrid_mode);
                else
                        modes = strv_new("suspend", "platform", "shutdown", NULL);

                if (hybrid_state)
                        states = TAKE_PTR(hybrid_state);
                else
                        states = strv_new("disk", NULL);

        } else if (streq(verb, "suspend-then-hibernate"))
                modes = states = NULL;
        else
                assert_not_reached("what verb");

        if ((!modes && STR_IN_SET(verb, "hibernate", "hybrid-sleep")) ||
            (!states && !streq(verb, "suspend-then-hibernate"))) {
                strv_free(modes);
                strv_free(states);
                return log_oom();
        }

        if (_modes)
                *_modes = modes;
        if (_states)
                *_states = states;
        if (_delay)
                *_delay = delay;

        return 0;
}
#endif // 0

#if 1 /// Only available in this file for elogind
static
#endif // 1
int can_sleep_state(char **types) {
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

#if 1 /// Only available in this file for elogind
static
#endif // 1
int can_sleep_disk(char **types) {
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

#if 0 /// elogind does not allow any bypass, we are never init!
        if (getenv_bool("SYSTEMD_BYPASS_HIBERNATION_MEMORY_CHECK") > 0)
                return true;
#endif // 0

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


#if 0 /// elogind has to do, or better, *can* do it differently
static bool can_s2h(void) {
        int r;

        r = access("/sys/class/rtc/rtc0/wakealarm", W_OK);
        if (r < 0) {
                log_full(errno == ENOENT ? LOG_DEBUG : LOG_WARNING,
                         "/sys/class/rct/rct0/wakealarm is not writable %m");
                return false;
        }

        r = can_sleep("suspend");
        if (r < 0) {
                log_debug_errno(r, "Unable to suspend system.");
                return false;
        }

        r = can_sleep("hibernate");
        if (r < 0) {
                log_debug_errno(r, "Unable to hibernate system.");
                return false;
        }

        return true;
}
#else
int can_sleep(Manager *m, const char *verb) {

        assert(streq(verb, "suspend") ||
               streq(verb, "hibernate") ||
               streq(verb, "hybrid-sleep"));
int can_sleep(const char *verb) {
        _cleanup_strv_free_ char **modes = NULL, **states = NULL;
        int r;

        if ( streq(verb, "suspend")
          && ( !can_sleep_state(m->suspend_state)
            || !can_sleep_disk(m->suspend_mode) ) )
                return false;
        assert(STR_IN_SET(verb, "suspend", "hibernate", "hybrid-sleep", "suspend-to-hibernate"));
        assert(STR_IN_SET(verb, "suspend", "hibernate", "hybrid-sleep", "suspend-then-hibernate"));

        if ( streq(verb, "hibernate")
          && ( !can_sleep_state(m->hibernate_state)
            || !can_sleep_disk(m->hibernate_mode) ) )
                return false;
        if (streq(verb, "suspend-to-hibernate"))
        if (streq(verb, "suspend-then-hibernate"))
                return can_s2h();

        if ( streq(verb, "hybrid-sleep")
          && ( !can_sleep_state(m->hybrid_sleep_state)
            || !can_sleep_disk(m->hybrid_sleep_mode) ) )
        r = parse_sleep_config(verb, &modes, &states, NULL);
        if (r < 0)
                return false;

        if (!can_sleep_state(states) || !can_sleep_disk(modes))
                return false;

        return streq(verb, "suspend") || enough_memory_for_hibernation();
}
#endif // 0
