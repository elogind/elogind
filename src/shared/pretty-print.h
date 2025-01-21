/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include "glyph-util.h"
#include "terminal-util.h"

#if 0 /// UNNEEDED by elogind
#define CYLON_BUFFER_EXTRA (2*STRLEN(ANSI_RED) + STRLEN(ANSI_HIGHLIGHT_RED) + 2*STRLEN(ANSI_NORMAL))

void draw_cylon(char buffer[], size_t buflen, unsigned width, unsigned pos);

void print_separator(void);

int file_url_from_path(const char *path, char **ret);
#endif // 0

bool urlify_enabled(void);

int terminal_urlify(const char *url, const char *text, char **ret);
#if 0 /// UNNEEDED by elogind
int terminal_urlify_path(const char *path, const char *text, char **ret);
#endif // 0
int terminal_urlify_man(const char *page, const char *section, char **ret);

#if 0 /// UNNEEDED by elogind
typedef enum CatFlags {
        CAT_CONFIG_OFF          = 0,
        CAT_CONFIG_ON           = 1 << 0,
        CAT_FORMAT_HAS_SECTIONS = 1 << 1,  /* Sections are meaningful for this file format */
        CAT_TLDR                = 1 << 2,  /* Only print comments and relevant section headers */
} CatFlags;

int cat_files(const char *file, char **dropins, CatFlags flags);
int conf_files_cat(const char *root, const char *name, CatFlags flags);
#endif // 0

#define RED_CROSS_MARK_MAX (STRLEN(ANSI_HIGHLIGHT_RED) + STRLEN("✗") + STRLEN(ANSI_NORMAL) + 1)
#define GREEN_CHECK_MARK_MAX (STRLEN(ANSI_HIGHLIGHT_GREEN) + STRLEN("✓") + STRLEN(ANSI_NORMAL) + 1)

static inline const char *red_cross_mark_internal(char buffer[static RED_CROSS_MARK_MAX]) {
        assert(buffer);
        assert_se(stpcpy(stpcpy(stpcpy(buffer, ansi_highlight_red()), special_glyph(SPECIAL_GLYPH_CROSS_MARK)), ansi_normal()) < buffer + RED_CROSS_MARK_MAX);
        return buffer;
}

static inline const char *green_check_mark_internal(char buffer[static GREEN_CHECK_MARK_MAX]) {
        assert(buffer);
        assert_se(stpcpy(stpcpy(stpcpy(buffer, ansi_highlight_green()), special_glyph(SPECIAL_GLYPH_CHECK_MARK)), ansi_normal()) < buffer + GREEN_CHECK_MARK_MAX);
        return buffer;
}

#define RED_CROSS_MARK() red_cross_mark_internal((char[RED_CROSS_MARK_MAX]) {})
#define GREEN_CHECK_MARK() green_check_mark_internal((char[GREEN_CHECK_MARK_MAX]) {})

#define COLOR_MARK_BOOL(b) ((b) ? GREEN_CHECK_MARK() : RED_CROSS_MARK())

int terminal_tint_color(double hue, char **ret);
