/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <errno.h>
#include <stdbool.h>

#include "macro.h"

typedef enum SpecialGlyph {
        SPECIAL_GLYPH_TREE_VERTICAL,
        SPECIAL_GLYPH_TREE_BRANCH,
        SPECIAL_GLYPH_TREE_RIGHT,
        SPECIAL_GLYPH_TREE_SPACE,
        SPECIAL_GLYPH_TREE_TOP,
        SPECIAL_GLYPH_VERTICAL_DOTTED,
        SPECIAL_GLYPH_TRIANGULAR_BULLET,
        SPECIAL_GLYPH_BLACK_CIRCLE,
        SPECIAL_GLYPH_WHITE_CIRCLE,
        SPECIAL_GLYPH_MULTIPLICATION_SIGN,
        SPECIAL_GLYPH_CIRCLE_ARROW,
        SPECIAL_GLYPH_BULLET,
        SPECIAL_GLYPH_MU,
        SPECIAL_GLYPH_CHECK_MARK,
        SPECIAL_GLYPH_CROSS_MARK,
        SPECIAL_GLYPH_ARROW_LEFT,
        SPECIAL_GLYPH_ARROW_RIGHT,
        SPECIAL_GLYPH_ARROW_UP,
        SPECIAL_GLYPH_ARROW_DOWN,
        SPECIAL_GLYPH_ELLIPSIS,
        SPECIAL_GLYPH_LIGHT_SHADE,
        SPECIAL_GLYPH_DARK_SHADE,
        SPECIAL_GLYPH_SIGMA,
        SPECIAL_GLYPH_EXTERNAL_LINK,
        _SPECIAL_GLYPH_FIRST_EMOJI,
        SPECIAL_GLYPH_ECSTATIC_SMILEY = _SPECIAL_GLYPH_FIRST_EMOJI,
        SPECIAL_GLYPH_HAPPY_SMILEY,
        SPECIAL_GLYPH_SLIGHTLY_HAPPY_SMILEY,
        SPECIAL_GLYPH_NEUTRAL_SMILEY,
        SPECIAL_GLYPH_SLIGHTLY_UNHAPPY_SMILEY,
        SPECIAL_GLYPH_UNHAPPY_SMILEY,
        SPECIAL_GLYPH_DEPRESSED_SMILEY,
        SPECIAL_GLYPH_LOCK_AND_KEY,
        SPECIAL_GLYPH_TOUCH,
        SPECIAL_GLYPH_RECYCLING,
        SPECIAL_GLYPH_DOWNLOAD,
        SPECIAL_GLYPH_SPARKLES,
        SPECIAL_GLYPH_LOW_BATTERY,
        SPECIAL_GLYPH_WARNING_SIGN,
        _SPECIAL_GLYPH_MAX,
        _SPECIAL_GLYPH_INVALID = -EINVAL,
} SpecialGlyph;

bool emoji_enabled(void);

#if 0 /// UNNEEDED by elogind
const char *special_glyph_full(SpecialGlyph code, bool force_utf) _const_;

static inline const char *special_glyph(SpecialGlyph code) {
        return special_glyph_full(code, false);
}

static inline const char *special_glyph_check_mark(bool b) {
        return b ? special_glyph(SPECIAL_GLYPH_CHECK_MARK) : special_glyph(SPECIAL_GLYPH_CROSS_MARK);
}

static inline const char *special_glyph_check_mark_space(bool b) {
        return b ? special_glyph(SPECIAL_GLYPH_CHECK_MARK) : " ";
}
#endif // 0
