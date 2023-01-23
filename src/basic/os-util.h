/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <stdio.h>

/* The *_extension_release flavours will look for /usr/lib/extension-release/extension-release.NAME
 * in accordance with the OS extension specification, rather than for /usr/lib/ or /etc/os-release. */

bool image_name_is_valid(const char *s) _pure_;

#if 0 /// UNNEEDED by elogind
int path_is_extension_tree(const char *path, const char *extension, bool relax_extension_release_check);
static inline int path_is_os_tree(const char *path) {
        return path_is_extension_tree(path, NULL, false);
}
#endif // 0

int open_extension_release(const char *root, const char *extension, bool relax_extension_release_check, char **ret_path, int *ret_fd);
#if 0 /// UNNEEDED by elogind
static inline int open_os_release(const char *root, char **ret_path, int *ret_fd) {
        return open_extension_release(root, NULL, false, ret_path, ret_fd);
}
#endif // 0

int fopen_extension_release(const char *root, const char *extension, bool relax_extension_release_check, char **ret_path, FILE **ret_file);
#if 0 /// UNNEEDED by elogind
static inline int fopen_os_release(const char *root, char **ret_path, FILE **ret_file) {
        return fopen_extension_release(root, NULL, false, ret_path, ret_file);
}

int _parse_extension_release(const char *root, bool relax_extension_release_check, const char *extension, ...) _sentinel_;
#endif // 0
int _parse_os_release(const char *root, ...) _sentinel_;
#if 0 /// UNNEEDED by elogind
#define parse_extension_release(root, relax_extension_release_check, extension, ...) _parse_extension_release(root, relax_extension_release_check, extension, __VA_ARGS__, NULL)
#endif // 0
#define parse_os_release(root, ...) _parse_os_release(root, __VA_ARGS__, NULL)

#if 0 /// UNNEEDED by elogind
int load_extension_release_pairs(const char *root, const char *extension, bool relax_extension_release_check, char ***ret);
int load_os_release_pairs(const char *root, char ***ret);
int load_os_release_pairs_with_prefix(const char *root, const char *prefix, char ***ret);

int os_release_support_ended(const char *support_end, bool quiet);
#endif // 0

const char *os_release_pretty_name(const char *pretty_name, const char *name);
