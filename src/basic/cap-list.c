/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <errno.h>
#include <string.h>

#include "alloc-util.h"
#include "capability-util.h"
#include "cap-list.h"
#include "extract-word.h"
#include "macro.h"
#include "parse-util.h"
#include "stdio-util.h"

#if 0 /// UNNEEDED by elogind
static const struct capability_name* lookup_capability(register const char *str, register GPERF_LEN_TYPE len);

#include "cap-from-name.h"
#endif // 0
#include "cap-to-name.h"

const char *capability_to_name(int id) {
        if (id < 0)
                return NULL;

        if ((size_t) id >= ELEMENTSOF(capability_names))
                return NULL;

        return capability_names[id];
}

#if 0 /// UNNEEDED by elogind
int capability_from_name(const char *name) {
        const struct capability_name *sc;
        int r, i;

        assert(name);

        /* Try to parse numeric capability */
        r = safe_atoi(name, &i);
        if (r >= 0) {
                if (i < 0 || i >= 63)
                        return -EINVAL;

                return i;
        }

        /* Try to parse string capability */
        sc = lookup_capability(name, strlen(name));
        if (!sc)
                return -EINVAL;

        return sc->id;
}

/* This is the number of capability names we are *compiled* with.
 * For the max capability number of the currently-running kernel,
 * use cap_last_cap(). */
int capability_list_length(void) {
        return (int) ELEMENTSOF(capability_names);
}
#endif // 0

int capability_set_to_string(uint64_t set, char **ret) {
        _cleanup_free_ char *str = NULL;
        size_t n = 0;

        assert(ret);

        for (unsigned i = 0; i <= cap_last_cap(); i++)
                if (set & (UINT64_C(1) << i)) {
                        const char *p;
                        char buf[2 + 16 + 1];
                        size_t add;

                        p = capability_to_name(i);
                        if (!p) {
                                xsprintf(buf, "0x%x", i);
                                p = buf;
                        }

                        add = strlen(p);

                        if (!GREEDY_REALLOC(str, n + add + 2))
                                return -ENOMEM;

                        strcpy(mempcpy(str + n, p, add), " ");
                        n += add + 1;
                }

        if (!GREEDY_REALLOC(str, n + 1))
                return -ENOMEM;

        str[n > 0 ? n - 1 : 0] = '\0'; /* truncate the last space, if it's there */

        *ret = TAKE_PTR(str);
        return 0;
}

#if 0 /// UNNEEDED by elogind
int capability_set_from_string(const char *s, uint64_t *ret) {
        uint64_t val = 0;
        bool good = true;

        for (const char *p = s;;) {
                _cleanup_free_ char *word = NULL;
                int r;

                r = extract_first_word(&p, &word, NULL, EXTRACT_UNQUOTE|EXTRACT_RELAX);
                if (r < 0)
                        return r;
                if (r == 0)
                        break;

                r = capability_from_name(word);
                if (r < 0) {
                        log_debug_errno(r, "Failed to parse capability '%s', ignoring: %m", word);
                        good = false;
                } else
                        val |= UINT64_C(1) << r;
        }

        if (ret)
                *ret = val;

        return good;
}
#endif // 0
