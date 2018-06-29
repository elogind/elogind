/* SPDX-License-Identifier: LGPL-2.1+ */
/***
  This file is part of systemd.

  Copyright 2012 Lennart Poettering
***/

#include <stdio.h>

//#include "alloc-util.h"
#include "string-util.h"
#include "terminal-util.h"
#include "util.h"

int main(int argc, char *argv[]) {
        _cleanup_free_ char *urlified = NULL, *q = NULL, *qq = NULL;
        char *p, *z;

        assert_se(p = strdup("\tFoobar\tbar\twaldo\t"));
        assert_se(strip_tab_ansi(&p, NULL, NULL));
        fprintf(stdout, "<%s>\n", p);
        assert_se(streq(p, "        Foobar        bar        waldo        "));
        free(p);

        assert_se(p = strdup(ANSI_HIGHLIGHT "Hello" ANSI_NORMAL ANSI_HIGHLIGHT_RED " world!" ANSI_NORMAL));
        assert_se(strip_tab_ansi(&p, NULL, NULL));
        fprintf(stdout, "<%s>\n", p);
        assert_se(streq(p, "Hello world!"));
        free(p);

        assert_se(p = strdup("\x1B[\x1B[\t\x1B[" ANSI_HIGHLIGHT "\x1B[" "Hello" ANSI_NORMAL ANSI_HIGHLIGHT_RED " world!" ANSI_NORMAL));
        assert_se(strip_tab_ansi(&p, NULL, NULL));
        assert_se(streq(p, "\x1B[\x1B[        \x1B[\x1B[Hello world!"));
        free(p);

        assert_se(p = strdup("\x1B[waldo"));
        assert_se(strip_tab_ansi(&p, NULL, NULL));
        assert_se(streq(p, "\x1B[waldo"));
        free(p);

        assert_se(terminal_urlify_path("/etc/fstab", "i am a fabulous link", &urlified) >= 0);
        assert_se(p = strjoin("something ", urlified, " something-else"));
        assert_se(q = strdup(p));
        printf("<%s>\n", p);
        assert_se(strip_tab_ansi(&p, NULL, NULL));
        printf("<%s>\n", p);
        assert_se(streq(p, "something i am a fabulous link something-else"));
        p = mfree(p);

        /* Truncate the formatted string in the middle of an ANSI sequence (in which case we shouldn't touch the
         * incomplete sequence) */
        z = strstr(q, "fstab");
        if (z) {
                *z = 0;
                assert_se(qq = strdup(q));
                assert_se(strip_tab_ansi(&q, NULL, NULL));
                assert_se(streq(q, qq));
        }

        return 0;
}
