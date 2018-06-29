/* SPDX-License-Identifier: LGPL-2.1+ */
/***
  This file is part of systemd

  Copyright 2014 Ronny Chevalier
***/

#include "locale-util.h"
#include "macro.h"
#include "strv.h"

static void test_get_locales(void) {
        _cleanup_strv_free_ char **locales = NULL;
        char **p;
        int r;

        r = get_locales(&locales);
        assert_se(r >= 0);
        assert_se(locales);

        STRV_FOREACH(p, locales) {
                puts(*p);
                assert_se(locale_is_valid(*p));
        }
}

static void test_locale_is_valid(void) {
        log_info("/* %s */", __func__);

        assert_se(locale_is_valid("en_EN.utf8"));
        assert_se(locale_is_valid("fr_FR.utf8"));
        assert_se(locale_is_valid("fr_FR@euro"));
        assert_se(locale_is_valid("fi_FI"));
        assert_se(locale_is_valid("POSIX"));
        assert_se(locale_is_valid("C"));

        assert_se(!locale_is_valid(""));
        assert_se(!locale_is_valid("/usr/bin/foo"));
        assert_se(!locale_is_valid("\x01gar\x02 bage\x03"));
}

static void test_keymaps(void) {
        _cleanup_strv_free_ char **kmaps = NULL;
        char **p;
        int r;

        log_info("/* %s */", __func__);

        assert_se(!keymap_is_valid(""));
        assert_se(!keymap_is_valid("/usr/bin/foo"));
        assert_se(!keymap_is_valid("\x01gar\x02 bage\x03"));

        r = get_keymaps(&kmaps);
        if (r == -ENOENT)
                return; /* skip test if no keymaps are installed */

        assert_se(r >= 0);
        assert_se(kmaps);

        STRV_FOREACH(p, kmaps) {
                puts(*p);
                assert_se(keymap_is_valid(*p));
        }

        assert_se(keymap_is_valid("uk"));
        assert_se(keymap_is_valid("de-nodeadkeys"));
        assert_se(keymap_is_valid("ANSI-dvorak"));
        assert_se(keymap_is_valid("unicode"));
}

#define dump_glyph(x) log_info(STRINGIFY(x) ": %s", special_glyph(x))
static void dump_special_glyphs(void) {
        assert_cc(ELLIPSIS + 1 == _SPECIAL_GLYPH_MAX);

        log_info("/* %s */", __func__);

        log_info("is_locale_utf8: %s", yes_no(is_locale_utf8()));

        dump_glyph(TREE_VERTICAL);
        dump_glyph(TREE_BRANCH);
        dump_glyph(TREE_RIGHT);
        dump_glyph(TREE_SPACE);
        dump_glyph(TRIANGULAR_BULLET);
        dump_glyph(BLACK_CIRCLE);
        dump_glyph(ARROW);
        dump_glyph(MDASH);
        dump_glyph(ELLIPSIS);
}

int main(int argc, char *argv[]) {
        test_get_locales();
        test_locale_is_valid();
        test_keymaps();

        dump_special_glyphs();

        return 0;
}
