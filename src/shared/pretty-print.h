/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

#if 0 /// UNNEEDED by elogind
void print_separator(void);

int file_url_from_path(const char *path, char **ret);
#endif // 0

int terminal_urlify(const char *url, const char *text, char **ret);
#if 0 /// UNNEEDED by elogind
int terminal_urlify_path(const char *path, const char *text, char **ret);
#endif // 0
int terminal_urlify_man(const char *page, const char *section, char **ret);

#if 0 /// UNNEEDED by elogind
typedef enum CatFlags {
        CAT_FLAGS_MAIN_FILE_OPTIONAL = 1 << 0,
} CatFlags;

int cat_files(const char *file, char **dropins, CatFlags flags);
int conf_files_cat(const char *root, const char *name);
#endif // 0
