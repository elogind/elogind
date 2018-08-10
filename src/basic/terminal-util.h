/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

/***
  This file is part of systemd.

  Copyright 2010 Lennart Poettering
***/

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>

#include "macro.h"
#include "time-util.h"

/* Regular colors */
#define ANSI_BLACK   "\x1B[0;30m"
#define ANSI_RED     "\x1B[0;31m"
#define ANSI_GREEN   "\x1B[0;32m"
#define ANSI_YELLOW  "\x1B[0;33m"
#define ANSI_BLUE    "\x1B[0;34m"
#define ANSI_MAGENTA "\x1B[0;35m"
#define ANSI_CYAN    "\x1B[0;36m"
#define ANSI_WHITE   "\x1B[0;37m"

/* Bold/highlighted */
#define ANSI_HIGHLIGHT_BLACK   "\x1B[0;1;30m"
#define ANSI_HIGHLIGHT_RED     "\x1B[0;1;31m"
#define ANSI_HIGHLIGHT_GREEN   "\x1B[0;1;32m"
#define ANSI_HIGHLIGHT_YELLOW  "\x1B[0;1;33m"
#define ANSI_HIGHLIGHT_BLUE    "\x1B[0;1;34m"
#define ANSI_HIGHLIGHT_MAGENTA "\x1B[0;1;35m"
#define ANSI_HIGHLIGHT_CYAN    "\x1B[0;1;36m"
#define ANSI_HIGHLIGHT_WHITE   "\x1B[0;1;37m"

/* Underlined */
#define ANSI_HIGHLIGHT_BLACK_UNDERLINE   "\x1B[0;1;4;30m"
#define ANSI_HIGHLIGHT_RED_UNDERLINE     "\x1B[0;1;4;31m"
#define ANSI_HIGHLIGHT_GREEN_UNDERLINE   "\x1B[0;1;4;32m"
#define ANSI_HIGHLIGHT_YELLOW_UNDERLINE  "\x1B[0;1;4;33m"
#define ANSI_HIGHLIGHT_BLUE_UNDERLINE    "\x1B[0;1;4;34m"
#define ANSI_HIGHLIGHT_MAGENTA_UNDERLINE "\x1B[0;1;4;35m"
#define ANSI_HIGHLIGHT_CYAN_UNDERLINE    "\x1B[0;1;4;36m"
#define ANSI_HIGHLIGHT_WHITE_UNDERLINE   "\x1B[0;1;4;37m"

/* Other ANSI codes */
#define ANSI_UNDERLINE "\x1B[0;4m"
#define ANSI_HIGHLIGHT "\x1B[0;1;39m"
#define ANSI_HIGHLIGHT_UNDERLINE "\x1B[0;1;4m"

/* Reset/clear ANSI styles */
#define ANSI_NORMAL "\x1B[0m"

/* Erase characters until the end of the line */
#define ANSI_ERASE_TO_END_OF_LINE "\x1B[K"

/* Set cursor to top left corner and clear screen */
#define ANSI_HOME_CLEAR "\x1B[H\x1B[2J"

#if 0 /// UNNEEDED by elogind
int reset_terminal_fd(int fd, bool switch_to_text);
int reset_terminal(const char *name);
#endif // 0

int open_terminal(const char *name, int mode);
#if 0 /// UNNEEDED by elogind

/* Flags for tweaking the way we become the controlling process of a terminal. */
typedef enum AcquireTerminalFlags {
        /* Try to become the controlling process of the TTY. If we can't return -EPERM. */
        ACQUIRE_TERMINAL_TRY        = 0,

        /* Tell the kernel to forcibly make us the controlling process of the TTY. Returns -EPERM if the kernel doesn't allow that. */
        ACQUIRE_TERMINAL_FORCE      = 1,

        /* If we can't become the controlling process of the TTY right-away, then wait until we can. */
        ACQUIRE_TERMINAL_WAIT       = 2,

        /* Pick one of the above, and then OR this flag in, in order to request permissive behaviour, if we can't become controlling process then don't mind */
        ACQUIRE_TERMINAL_PERMISSIVE = 1 << 2,
} AcquireTerminalFlags;

int acquire_terminal(const char *name, AcquireTerminalFlags flags, usec_t timeout);
int release_terminal(void);

int terminal_vhangup_fd(int fd);
int terminal_vhangup(const char *name);
#endif // 0

int chvt(int vt);

#if 0 /// UNNEEDED by elogind
int read_one_char(FILE *f, char *ret, usec_t timeout, bool *need_nl);
int ask_char(char *ret, const char *replies, const char *text, ...) _printf_(3, 4);
int ask_string(char **ret, const char *text, ...) _printf_(2, 3);

int vt_disallocate(const char *name);

#endif // 0
int resolve_dev_console(char **ret);
int get_kernel_consoles(char ***ret);
bool tty_is_vc(const char *tty);
#if 0 /// UNNEEDED by elogind
bool tty_is_vc_resolve(const char *tty);
#endif // 0
bool tty_is_console(const char *tty) _pure_;
int vtnr_from_tty(const char *tty);
#if 0 /// UNNEEDED by elogind
const char *default_term_for_tty(const char *tty);
#endif // 0

#if 0 /// UNNEEDED by elogind
int make_console_stdio(void);
#endif // 0

int fd_columns(int fd);
unsigned columns(void);
int fd_lines(int fd);
unsigned lines(void);

#if 0 /// UNNEEDED by elogind
void columns_lines_cache_reset(int _unused_ signum);
#endif // 0
void reset_terminal_feature_caches(void);

bool on_tty(void);
bool terminal_is_dumb(void);
bool colors_enabled(void);
bool underline_enabled(void);
#if 0 /// UNNEEDED by elogind
bool dev_console_colors_enabled(void);
#endif // 0

#define DEFINE_ANSI_FUNC(name, NAME)                            \
        static inline const char *ansi_##name(void) {           \
                return colors_enabled() ? ANSI_##NAME : "";     \
        }

#define DEFINE_ANSI_FUNC_UNDERLINE(name, NAME, REPLACEMENT)             \
        static inline const char *ansi_##name(void) {                   \
                return underline_enabled() ? ANSI_##NAME :              \
                        colors_enabled() ? ANSI_##REPLACEMENT : "";     \
        }

DEFINE_ANSI_FUNC(highlight,                  HIGHLIGHT);
DEFINE_ANSI_FUNC(highlight_red,              HIGHLIGHT_RED);
DEFINE_ANSI_FUNC(highlight_green,            HIGHLIGHT_GREEN);
DEFINE_ANSI_FUNC(highlight_yellow,           HIGHLIGHT_YELLOW);
DEFINE_ANSI_FUNC(highlight_blue,             HIGHLIGHT_BLUE);
DEFINE_ANSI_FUNC(highlight_magenta,          HIGHLIGHT_MAGENTA);
DEFINE_ANSI_FUNC(normal,                     NORMAL);

DEFINE_ANSI_FUNC_UNDERLINE(underline,                  UNDERLINE, NORMAL);
DEFINE_ANSI_FUNC_UNDERLINE(highlight_underline,        HIGHLIGHT_UNDERLINE, HIGHLIGHT);
DEFINE_ANSI_FUNC_UNDERLINE(highlight_red_underline,    HIGHLIGHT_RED_UNDERLINE, HIGHLIGHT_RED);
DEFINE_ANSI_FUNC_UNDERLINE(highlight_green_underline,  HIGHLIGHT_GREEN_UNDERLINE, HIGHLIGHT_GREEN);
DEFINE_ANSI_FUNC_UNDERLINE(highlight_yellow_underline, HIGHLIGHT_YELLOW_UNDERLINE, HIGHLIGHT_YELLOW);
DEFINE_ANSI_FUNC_UNDERLINE(highlight_blue_underline,   HIGHLIGHT_BLUE_UNDERLINE, HIGHLIGHT_BLUE);

int get_ctty_devnr(pid_t pid, dev_t *d);
int get_ctty(pid_t, dev_t *_devnr, char **r);

int getttyname_malloc(int fd, char **r);
int getttyname_harder(int fd, char **r);

#if 0 /// UNNEEDED by elogind
int ptsname_malloc(int fd, char **ret);
int ptsname_namespace(int pty, char **ret);

int openpt_in_namespace(pid_t pid, int flags);
int open_terminal_in_namespace(pid_t pid, const char *name, int mode);
#endif // 0

int vt_default_utf8(void);
int vt_reset_keyboard(int fd);

int terminal_urlify(const char *url, const char *text, char **ret);
int terminal_urlify_path(const char *path, const char *text, char **ret);

typedef enum CatFlags {
        CAT_FLAGS_MAIN_FILE_OPTIONAL = 1 << 0,
} CatFlags;

int cat_files(const char *file, char **dropins, CatFlags flags);

void print_separator(void);
