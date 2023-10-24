/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <errno.h>
//#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "chase-symlinks.h"
#include "conf-files.h"
#include "constants.h"
#include "dirent-util.h"
#include "fd-util.h"
#include "hashmap.h"
#include "log.h"
#include "macro.h"
#include "nulstr-util.h"
#include "path-util.h"
#include "set.h"
#include "sort-util.h"
#include "stat-util.h"
#include "string-util.h"
#include "strv.h"
//#include "terminal-util.h"

static int files_add(
                Hashmap **h,
                Set **masked,
                const char *suffix,
                const char *root,
                unsigned flags,
                const char *path) {

        _cleanup_free_ char *dirpath = NULL;
        _cleanup_closedir_ DIR *dir = NULL;
        int r;

        assert(h);
        assert(masked);
        assert(path);

        r = chase_symlinks_and_opendir(path, root, CHASE_PREFIX_ROOT, &dirpath, &dir);
        if (r == -ENOENT)
                return 0;
        if (r < 0)
                return log_debug_errno(r, "Failed to open directory '%s/%s': %m", empty_or_root(root) ? "" : root, dirpath);

        FOREACH_DIRENT(de, dir, return -errno) {
                _cleanup_free_ char *n = NULL, *p = NULL;
                struct stat st;

                /* Does this match the suffix? */
                if (suffix && !endswith(de->d_name, suffix))
                        continue;

                /* Has this file already been found in an earlier directory? */
                if (hashmap_contains(*h, de->d_name)) {
                        log_debug("Skipping overridden file '%s/%s'.", dirpath, de->d_name);
                        continue;
                }

                /* Has this been masked in an earlier directory? */
                if ((flags & CONF_FILES_FILTER_MASKED) && set_contains(*masked, de->d_name)) {
                        log_debug("File '%s/%s' is masked by previous entry.", dirpath, de->d_name);
                        continue;
                }

                /* Read file metadata if we shall validate the check for file masks, for node types or whether the node is marked executable. */
                if (flags & (CONF_FILES_FILTER_MASKED|CONF_FILES_REGULAR|CONF_FILES_DIRECTORY|CONF_FILES_EXECUTABLE))
                        if (fstatat(dirfd(dir), de->d_name, &st, 0) < 0) {
                                log_debug_errno(errno, "Failed to stat '%s/%s', ignoring: %m", dirpath, de->d_name);
                                continue;
                        }

                /* Is this a masking entry? */
                if ((flags & CONF_FILES_FILTER_MASKED))
                        if (null_or_empty(&st)) {
                                /* Mark this one as masked */
                                r = set_put_strdup(masked, de->d_name);
                                if (r < 0)
                                        return r;

                                log_debug("File '%s/%s' is a mask.", dirpath, de->d_name);
                                continue;
                        }

                /* Does this node have the right type? */
                if (flags & (CONF_FILES_REGULAR|CONF_FILES_DIRECTORY))
                        if (!((flags & CONF_FILES_DIRECTORY) && S_ISDIR(st.st_mode)) &&
                            !((flags & CONF_FILES_REGULAR) && S_ISREG(st.st_mode))) {
                                log_debug("Ignoring '%s/%s', as it does not have the right type.", dirpath, de->d_name);
                                continue;
                        }

                /* Does this node have the executable bit set? */
                if (flags & CONF_FILES_EXECUTABLE)
                        /* As requested: check if the file is marked executable. Note that we don't check access(X_OK)
                         * here, as we care about whether the file is marked executable at all, and not whether it is
                         * executable for us, because if so, such errors are stuff we should log about. */

                        if ((st.st_mode & 0111) == 0) { /* not executable */
                                log_debug("Ignoring '%s/%s', as it is not marked executable.", dirpath, de->d_name);
                                continue;
                        }

                n = strdup(de->d_name);
                if (!n)
                        return -ENOMEM;

                if ((flags & CONF_FILES_BASENAME))
                        r = hashmap_ensure_put(h, &string_hash_ops_free, n, n);
                else {
                        p = path_join(dirpath, de->d_name);
                        if (!p)
                                return -ENOMEM;

                        r = hashmap_ensure_put(h, &string_hash_ops_free_free, n, p);
                }
                if (r < 0)
                        return r;
                assert(r > 0);

                TAKE_PTR(n);
                TAKE_PTR(p);
        }

        return 0;
}

static int base_cmp(char * const *a, char * const *b) {
        return strcmp(basename(*a), basename(*b));
}

static int conf_files_list_strv_internal(
                char ***ret,
                const char *suffix,
                const char *root,
                unsigned flags,
                char **dirs) {

        _cleanup_hashmap_free_ Hashmap *fh = NULL;
        _cleanup_set_free_ Set *masked = NULL;
        _cleanup_strv_free_ char **files = NULL;
        _cleanup_free_ char **sv = NULL;
        int r;

        assert(ret);

        /* This alters the dirs string array */
        if (!path_strv_resolve_uniq(dirs, root))
                return -ENOMEM;

        STRV_FOREACH(p, dirs) {
                r = files_add(&fh, &masked, suffix, root, flags, *p);
                if (r == -ENOMEM)
                        return r;
                if (r < 0)
                        log_debug_errno(r, "Failed to search for files in %s, ignoring: %m", *p);
        }

        sv = hashmap_get_strv(fh);
        if (!sv)
                return -ENOMEM;

        /* The entries in the array given by hashmap_get_strv() are still owned by the hashmap. */
        files = strv_copy(sv);
        if (!files)
                return -ENOMEM;

        typesafe_qsort(files, strv_length(files), base_cmp);
        *ret = TAKE_PTR(files);

        return 0;
}

#if 0 /// UNNEEDED by elogind
int conf_files_insert(char ***strv, const char *root, char **dirs, const char *path) {
        /* Insert a path into strv, at the place honouring the usual sorting rules:
         * - we first compare by the basename
         * - and then we compare by dirname, allowing just one file with the given
         *   basename.
         * This means that we will
         * - add a new entry if basename(path) was not on the list,
         * - do nothing if an entry with higher priority was already present,
         * - do nothing if our new entry matches the existing entry,
         * - replace the existing entry if our new entry has higher priority.
         */
        size_t i, n;
        char *t;
        int r;

        n = strv_length(*strv);
        for (i = 0; i < n; i++) {
                int c;

                c = base_cmp((char* const*) *strv + i, (char* const*) &path);
                if (c == 0)
                        /* Oh, there already is an entry with a matching name (the last component). */
                        STRV_FOREACH(dir, dirs) {
                                _cleanup_free_ char *rdir = NULL;
                                char *p1, *p2;

                                rdir = path_join(root, *dir);
                                if (!rdir)
                                        return -ENOMEM;

                                p1 = path_startswith((*strv)[i], rdir);
                                if (p1)
                                        /* Existing entry with higher priority
                                         * or same priority, no need to do anything. */
                                        return 0;

                                p2 = path_startswith(path, *dir);
                                if (p2) {
                                        /* Our new entry has higher priority */

                                        t = path_join(root, path);
                                        if (!t)
                                                return log_oom();

                                        return free_and_replace((*strv)[i], t);
                                }
                        }

                else if (c > 0)
                        /* Following files have lower priority, let's go insert our
                         * new entry. */
                        break;

                /* … we are not there yet, let's continue */
        }

        /* The new file has lower priority than all the existing entries */
        t = path_join(root, path);
        if (!t)
                return -ENOMEM;

        r = strv_insert(strv, i, t);
        if (r < 0)
                free(t);

        return r;
}
#endif // 0

int conf_files_list_strv(char ***ret, const char *suffix, const char *root, unsigned flags, const char* const* dirs) {
        _cleanup_strv_free_ char **copy = NULL;

        assert(ret);

        copy = strv_copy((char**) dirs);
        if (!copy)
                return -ENOMEM;

        return conf_files_list_strv_internal(ret, suffix, root, flags, copy);
}

int conf_files_list(char ***ret, const char *suffix, const char *root, unsigned flags, const char *dir) {
        _cleanup_strv_free_ char **dirs = NULL;

        assert(ret);

        dirs = strv_new(dir);
        if (!dirs)
                return -ENOMEM;

        return conf_files_list_strv_internal(ret, suffix, root, flags, dirs);
}

int conf_files_list_nulstr(char ***ret, const char *suffix, const char *root, unsigned flags, const char *dirs) {
        _cleanup_strv_free_ char **d = NULL;

        assert(ret);

        d = strv_split_nulstr(dirs);
        if (!d)
                return -ENOMEM;

        return conf_files_list_strv_internal(ret, suffix, root, flags, d);
}

#if 0 /// UNNEEDED by elogind
int conf_files_list_with_replacement(
                const char *root,
                char **config_dirs,
                const char *replacement,
                char ***ret_files,
                char **ret_replace_file) {

        _cleanup_strv_free_ char **f = NULL;
        _cleanup_free_ char *p = NULL;
        int r;

        assert(config_dirs);
        assert(ret_files);
        assert(ret_replace_file || !replacement);

        r = conf_files_list_strv(&f, ".conf", root, 0, (const char* const*) config_dirs);
        if (r < 0)
                return log_error_errno(r, "Failed to enumerate config files: %m");

        if (replacement) {
                r = conf_files_insert(&f, root, config_dirs, replacement);
                if (r < 0)
                        return log_error_errno(r, "Failed to extend config file list: %m");

                p = path_join(root, replacement);
                if (!p)
                        return log_oom();
        }

        *ret_files = TAKE_PTR(f);
        if (ret_replace_file)
                *ret_replace_file = TAKE_PTR(p);

        return 0;
}
#endif // 0
