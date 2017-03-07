/*-*- Mode: C; c-basic-offset: 8; indent-tabs-mode: nil -*-*/

/***
  This file is part of systemd.

  Copyright 2010 Lennart Poettering

  systemd is free software; you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation; either version 2.1 of the License, or
  (at your option) any later version.

  systemd is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with systemd; If not, see <http://www.gnu.org/licenses/>.
***/

#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <ftw.h>

#include "basic/musl_missing.h"
#include "cgroup-util.h"
#include "set.h"
#include "macro.h"
#include "util.h"
#include "path-util.h"
#include "fileio.h"
#include "special.h"
#include "mkdir.h"

int cg_enumerate_processes(const char *controller, const char *path, FILE **_f) {
        _cleanup_free_ char *fs = NULL;
        FILE *f;
        int r;

        assert(_f);

        r = cg_get_path(controller, path, "cgroup.procs", &fs);
        if (r < 0)
                return r;

        f = fopen(fs, "re");
        if (!f)
                return -errno;

        *_f = f;
        return 0;
}

int cg_read_pid(FILE *f, pid_t *_pid) {
        unsigned long ul;

        /* Note that the cgroup.procs might contain duplicates! See
         * cgroups.txt for details. */

        assert(f);
        assert(_pid);

        errno = 0;
        if (fscanf(f, "%lu", &ul) != 1) {

                if (feof(f))
                        return 0;

                return errno ? -errno : -EIO;
        }

        if (ul <= 0)
                return -EIO;

        *_pid = (pid_t) ul;
        return 1;
}

int cg_enumerate_subgroups(const char *controller, const char *path, DIR **_d) {
        _cleanup_free_ char *fs = NULL;
        int r;
        DIR *d;

        assert(_d);

        /* This is not recursive! */

        r = cg_get_path(controller, path, NULL, &fs);
        if (r < 0)
                return r;

        d = opendir(fs);
        if (!d)
                return -errno;

        *_d = d;
        return 0;
}

int cg_read_subgroup(DIR *d, char **fn) {
        struct dirent *de;

        assert(d);
        assert(fn);

        FOREACH_DIRENT(de, d, return -errno) {
                char *b;

                if (de->d_type != DT_DIR)
                        continue;

                if (streq(de->d_name, ".") ||
                    streq(de->d_name, ".."))
                        continue;

                b = strdup(de->d_name);
                if (!b)
                        return -ENOMEM;

                *fn = b;
                return 1;
        }

        return 0;
}

int cg_rmdir(const char *controller, const char *path) {
        _cleanup_free_ char *p = NULL;
        int r;

        r = cg_get_path(controller, path, NULL, &p);
        if (r < 0)
                return r;

        r = rmdir(p);
        if (r < 0 && errno != ENOENT)
                return -errno;

        return 0;
}

int cg_kill(const char *controller, const char *path, int sig, bool sigcont, bool ignore_self, Set *s) {
        _cleanup_set_free_ Set *allocated_set = NULL;
        bool done = false;
        int r, ret = 0;
        pid_t my_pid;

        assert(sig >= 0);

        /* This goes through the tasks list and kills them all. This
         * is repeated until no further processes are added to the
         * tasks list, to properly handle forking processes */

        if (!s) {
                s = allocated_set = set_new(NULL);
                if (!s)
                        return -ENOMEM;
        }

        my_pid = getpid();

        do {
                _cleanup_fclose_ FILE *f = NULL;
                pid_t pid = 0;
                done = true;

                r = cg_enumerate_processes(controller, path, &f);
                if (r < 0) {
                        if (ret >= 0 && r != -ENOENT)
                                return r;

                        return ret;
                }

                while ((r = cg_read_pid(f, &pid)) > 0) {

                        if (ignore_self && pid == my_pid)
                                continue;

                        if (set_get(s, LONG_TO_PTR(pid)) == LONG_TO_PTR(pid))
                                continue;

                        /* If we haven't killed this process yet, kill
                         * it */
                        if (kill(pid, sig) < 0) {
                                if (ret >= 0 && errno != ESRCH)
                                        ret = -errno;
                        } else {
                                if (sigcont && sig != SIGKILL)
                                        kill(pid, SIGCONT);

                                if (ret == 0)
                                        ret = 1;
                        }

                        done = false;

                        r = set_put(s, LONG_TO_PTR(pid));
                        if (r < 0) {
                                if (ret >= 0)
                                        return r;

                                return ret;
                        }
                }

                if (r < 0) {
                        if (ret >= 0)
                                return r;

                        return ret;
                }

                /* To avoid racing against processes which fork
                 * quicker than we can kill them we repeat this until
                 * no new pids need to be killed. */

        } while (!done);

        return ret;
}

int cg_kill_recursive(const char *controller, const char *path, int sig, bool sigcont, bool ignore_self, bool rem, Set *s) {
        _cleanup_set_free_ Set *allocated_set = NULL;
        _cleanup_closedir_ DIR *d = NULL;
        int r, ret = 0;
        char *fn;

        assert(path);
        assert(sig >= 0);

        if (!s) {
                s = allocated_set = set_new(NULL);
                if (!s)
                        return -ENOMEM;
        }

        ret = cg_kill(controller, path, sig, sigcont, ignore_self, s);

        r = cg_enumerate_subgroups(controller, path, &d);
        if (r < 0) {
                if (ret >= 0 && r != -ENOENT)
                        return r;

                return ret;
        }

        while ((r = cg_read_subgroup(d, &fn)) > 0) {
                _cleanup_free_ char *p = NULL;

                p = strjoin(path, "/", fn, NULL);
                free(fn);
                if (!p)
                        return -ENOMEM;

                r = cg_kill_recursive(controller, p, sig, sigcont, ignore_self, rem, s);
                if (ret >= 0 && r != 0)
                        ret = r;
        }

        if (ret >= 0 && r < 0)
                ret = r;

        if (rem) {
                r = cg_rmdir(controller, path);
                if (r < 0 && ret >= 0 && r != -ENOENT && r != -EBUSY)
                        return r;
        }

        return ret;
}

int cg_migrate(const char *cfrom, const char *pfrom, const char *cto, const char *pto, bool ignore_self) {
        bool done = false;
        _cleanup_set_free_ Set *s = NULL;
        int r, ret = 0;
        pid_t my_pid;

        assert(cfrom);
        assert(pfrom);
        assert(cto);
        assert(pto);

        s = set_new(NULL);
        if (!s)
                return -ENOMEM;

        my_pid = getpid();

        do {
                _cleanup_fclose_ FILE *f = NULL;
                pid_t pid = 0;
                done = true;

                r = cg_enumerate_processes(cfrom, pfrom, &f);
                if (r < 0) {
                        if (ret >= 0 && r != -ENOENT)
                                return r;

                        return ret;
                }

                while ((r = cg_read_pid(f, &pid)) > 0) {

                        /* This might do weird stuff if we aren't a
                         * single-threaded program. However, we
                         * luckily know we are not */
                        if (ignore_self && pid == my_pid)
                                continue;

                        if (set_get(s, LONG_TO_PTR(pid)) == LONG_TO_PTR(pid))
                                continue;

                        r = cg_attach(cto, pto, pid);
                        if (r < 0) {
                                if (ret >= 0 && r != -ESRCH)
                                        ret = r;
                        } else if (ret == 0)
                                ret = 1;

                        done = false;

                        r = set_put(s, LONG_TO_PTR(pid));
                        if (r < 0) {
                                if (ret >= 0)
                                        return r;

                                return ret;
                        }
                }

                if (r < 0) {
                        if (ret >= 0)
                                return r;

                        return ret;
                }
        } while (!done);

        return ret;
}

int cg_migrate_recursive(
                const char *cfrom,
                const char *pfrom,
                const char *cto,
                const char *pto,
                bool ignore_self,
                bool rem) {

        _cleanup_closedir_ DIR *d = NULL;
        int r, ret = 0;
        char *fn;

        assert(cfrom);
        assert(pfrom);
        assert(cto);
        assert(pto);

        ret = cg_migrate(cfrom, pfrom, cto, pto, ignore_self);

        r = cg_enumerate_subgroups(cfrom, pfrom, &d);
        if (r < 0) {
                if (ret >= 0 && r != -ENOENT)
                        return r;

                return ret;
        }

        while ((r = cg_read_subgroup(d, &fn)) > 0) {
                _cleanup_free_ char *p = NULL;

                p = strjoin(pfrom, "/", fn, NULL);
                free(fn);
                if (!p) {
                        if (ret >= 0)
                                return -ENOMEM;

                        return ret;
                }

                r = cg_migrate_recursive(cfrom, p, cto, pto, ignore_self, rem);
                if (r != 0 && ret >= 0)
                        ret = r;
        }

        if (r < 0 && ret >= 0)
                ret = r;

        if (rem) {
                r = cg_rmdir(cfrom, pfrom);
                if (r < 0 && ret >= 0 && r != -ENOENT && r != -EBUSY)
                        return r;
        }

        return ret;
}

int cg_migrate_recursive_fallback(
                const char *cfrom,
                const char *pfrom,
                const char *cto,
                const char *pto,
                bool ignore_self,
                bool rem) {

        int r;

        assert(cfrom);
        assert(pfrom);
        assert(cto);
        assert(pto);

        r = cg_migrate_recursive(cfrom, pfrom, cto, pto, ignore_self, rem);
        if (r < 0) {
                char prefix[strlen(pto) + 1];

                /* This didn't work? Then let's try all prefixes of the destination */

                PATH_FOREACH_PREFIX(prefix, pto) {
                        r = cg_migrate_recursive(cfrom, pfrom, cto, prefix, ignore_self, rem);
                        if (r >= 0)
                                break;
                }
        }

        return 0;
}

static const char *normalize_controller(const char *controller) {

        assert(controller);

        if (streq(controller, SYSTEMD_CGROUP_CONTROLLER))
                return "elogind";
        else if (startswith(controller, "name="))
                return controller + 5;
        else
                return controller;
}

static int join_path(const char *controller, const char *path, const char *suffix, char **fs) {
        char *t = NULL;

        if (!isempty(controller)) {
                if (!isempty(path) && !isempty(suffix))
                        t = strjoin("/sys/fs/cgroup/", controller, "/", path, "/", suffix, NULL);
                else if (!isempty(path))
                        t = strjoin("/sys/fs/cgroup/", controller, "/", path, NULL);
                else if (!isempty(suffix))
                        t = strjoin("/sys/fs/cgroup/", controller, "/", suffix, NULL);
                else
                        t = strappend("/sys/fs/cgroup/", controller);
        } else {
                if (!isempty(path) && !isempty(suffix))
                        t = strjoin(path, "/", suffix, NULL);
                else if (!isempty(path))
                        t = strdup(path);
                else
                        return -EINVAL;
        }

        if (!t)
                return -ENOMEM;

        *fs = path_kill_slashes(t);
        return 0;
}

int cg_get_path(const char *controller, const char *path, const char *suffix, char **fs) {
        const char *p;
        static thread_local bool good = false;

        assert(fs);

        if (controller && !cg_controller_is_valid(controller, true))
                return -EINVAL;

        if (_unlikely_(!good)) {
                int r;

                r = path_is_mount_point("/sys/fs/cgroup", false);
                if (r <= 0)
                        return r < 0 ? r : -ENOENT;

                /* Cache this to save a few stat()s */
                good = true;
        }

        p = controller ? normalize_controller(controller) : NULL;

        return join_path(p, path, suffix, fs);
}

static int check_hierarchy(const char *p) {
        const char *cc;

        assert(p);

        if (!filename_is_valid(p))
                return 0;

        /* Check if this controller actually really exists */
        cc = strjoina("/sys/fs/cgroup/", p);
        if (laccess(cc, F_OK) < 0)
                return -errno;

        return 0;
}

int cg_get_path_and_check(const char *controller, const char *path, const char *suffix, char **fs) {
        const char *p;
        int r;

        assert(fs);

        if (!cg_controller_is_valid(controller, true))
                return -EINVAL;

        /* Normalize the controller syntax */
        p = normalize_controller(controller);

        /* Check if this controller actually really exists */
        r = check_hierarchy(p);
        if (r < 0)
                return r;

        return join_path(p, path, suffix, fs);
}

static int trim_cb(const char *path, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
        assert(path);
        assert(sb);
        assert(ftwbuf);

        if (typeflag != FTW_DP)
                return 0;

        if (ftwbuf->level < 1)
                return 0;

        rmdir(path);
        return 0;
}

int cg_trim(const char *controller, const char *path, bool delete_root) {
        _cleanup_free_ char *fs = NULL;
        int r = 0;

        assert(path);

        r = cg_get_path(controller, path, NULL, &fs);
        if (r < 0)
                return r;

        errno = 0;
        if (nftw(fs, trim_cb, 64, FTW_DEPTH|FTW_MOUNT|FTW_PHYS) != 0)
                r = errno ? -errno : -EIO;

        if (delete_root) {
                if (rmdir(fs) < 0 && errno != ENOENT)
                        return -errno;
        }

        return r;
}

int cg_delete(const char *controller, const char *path) {
        _cleanup_free_ char *parent = NULL;
        int r;

        assert(path);

        r = path_get_parent(path, &parent);
        if (r < 0)
                return r;

        r = cg_migrate_recursive(controller, path, controller, parent, false, true);
        return r == -ENOENT ? 0 : r;
}

int cg_create(const char *controller, const char *path) {
        _cleanup_free_ char *fs = NULL;
        int r;

        r = cg_get_path_and_check(controller, path, NULL, &fs);
        if (r < 0)
                return r;

        r = mkdir_parents(fs, 0755);
        if (r < 0)
                return r;

        if (mkdir(fs, 0755) < 0) {

                if (errno == EEXIST)
                        return 0;

                return -errno;
        }

        return 1;
}

int cg_create_and_attach(const char *controller, const char *path, pid_t pid) {
        int r, q;

        assert(pid >= 0);

        r = cg_create(controller, path);
        if (r < 0)
                return r;

        q = cg_attach(controller, path, pid);
        if (q < 0)
                return q;

        /* This does not remove the cgroup on failure */
        return r;
}

int cg_attach(const char *controller, const char *path, pid_t pid) {
        _cleanup_free_ char *fs = NULL;
        char c[DECIMAL_STR_MAX(pid_t) + 2];
        int r;

        assert(path);
        assert(pid >= 0);

        r = cg_get_path_and_check(controller, path, "cgroup.procs", &fs);
        if (r < 0)
                return r;

        if (pid == 0)
                pid = getpid();

        snprintf(c, sizeof(c), PID_FMT"\n", pid);

        return write_string_file_no_create(fs, c);
}

int cg_attach_fallback(const char *controller, const char *path, pid_t pid) {
        int r;

        assert(controller);
        assert(path);
        assert(pid >= 0);

        r = cg_attach(controller, path, pid);
        if (r < 0) {
                char prefix[strlen(path) + 1];

                /* This didn't work? Then let's try all prefixes of
                 * the destination */

                PATH_FOREACH_PREFIX(prefix, path) {
                        r = cg_attach(controller, prefix, pid);
                        if (r >= 0)
                                break;
                }
        }

        return 0;
}

int cg_set_group_access(
                const char *controller,
                const char *path,
                mode_t mode,
                uid_t uid,
                gid_t gid) {

        _cleanup_free_ char *fs = NULL;
        int r;

        assert(path);

        if (mode != MODE_INVALID)
                mode &= 0777;

        r = cg_get_path(controller, path, NULL, &fs);
        if (r < 0)
                return r;

        return chmod_and_chown(fs, mode, uid, gid);
}

int cg_set_task_access(
                const char *controller,
                const char *path,
                mode_t mode,
                uid_t uid,
                gid_t gid) {

        _cleanup_free_ char *fs = NULL, *procs = NULL;
        int r;

        assert(path);

        if (mode == MODE_INVALID && uid == UID_INVALID && gid == GID_INVALID)
                return 0;

        if (mode != MODE_INVALID)
                mode &= 0666;

        r = cg_get_path(controller, path, "cgroup.procs", &fs);
        if (r < 0)
                return r;

        r = chmod_and_chown(fs, mode, uid, gid);
        if (r < 0)
                return r;

        /* Compatibility, Always keep values for "tasks" in sync with
         * "cgroup.procs" */
        r = cg_get_path(controller, path, "tasks", &procs);
        if (r < 0)
                return r;

        return chmod_and_chown(procs, mode, uid, gid);
}

int cg_pid_get_path(const char *controller, pid_t pid, char **path) {
        _cleanup_fclose_ FILE *f = NULL;
        char line[LINE_MAX];
        const char *fs;
        size_t cs;

        assert(path);
        assert(pid >= 0);

        if (controller) {
                if (!cg_controller_is_valid(controller, true))
                        return -EINVAL;

                controller = normalize_controller(controller);
        } else
                controller = SYSTEMD_CGROUP_CONTROLLER;

        fs = procfs_file_alloca(pid, "cgroup");

        f = fopen(fs, "re");
        if (!f)
                return errno == ENOENT ? -ESRCH : -errno;

        cs = strlen(controller);

        FOREACH_LINE(line, f, return -errno) {
                char *l, *p, *e;
                size_t k;
                const char *word, *state;
                bool found = false;

                truncate_nl(line);

                l = strchr(line, ':');
                if (!l)
                        continue;

                l++;
                e = strchr(l, ':');
                if (!e)
                        continue;

                *e = 0;

                FOREACH_WORD_SEPARATOR(word, k, l, ",", state) {

                        if (k == cs && memcmp(word, controller, cs) == 0) {
                                found = true;
                                break;
                        }

                        if (k == 5 + cs &&
                            memcmp(word, "name=", 5) == 0 &&
                            memcmp(word+5, controller, cs) == 0) {
                                found = true;
                                break;
                        }
                }

                if (!found)
                        continue;

                p = strdup(e + 1);
                if (!p)
                        return -ENOMEM;

                *path = p;
                return 0;
        }

        return -ENOENT;
}

int cg_install_release_agent(const char *controller, const char *agent) {
        _cleanup_free_ char *fs = NULL, *contents = NULL;
        char *sc;
        int r;

        assert(agent);

        r = cg_get_path(controller, NULL, "release_agent", &fs);
        if (r < 0)
                return r;

        r = read_one_line_file(fs, &contents);
        if (r < 0)
                return r;

        sc = strstrip(contents);
        if (sc[0] == 0) {
                r = write_string_file_no_create(fs, agent);
                if (r < 0)
                        return r;
        } else if (!streq(sc, agent))
                return -EEXIST;

        free(fs);
        fs = NULL;
        r = cg_get_path(controller, NULL, "notify_on_release", &fs);
        if (r < 0)
                return r;

        free(contents);
        contents = NULL;
        r = read_one_line_file(fs, &contents);
        if (r < 0)
                return r;

        sc = strstrip(contents);
        if (streq(sc, "0")) {
                r = write_string_file_no_create(fs, "1");
                if (r < 0)
                        return r;

                return 1;
        }

        if (!streq(sc, "1"))
                return -EIO;

        return 0;
}

int cg_uninstall_release_agent(const char *controller) {
        _cleanup_free_ char *fs = NULL;
        int r;

        r = cg_get_path(controller, NULL, "notify_on_release", &fs);
        if (r < 0)
                return r;

        r = write_string_file_no_create(fs, "0");
        if (r < 0)
                return r;

        free(fs);
        fs = NULL;

        r = cg_get_path(controller, NULL, "release_agent", &fs);
        if (r < 0)
                return r;

        r = write_string_file_no_create(fs, "");
        if (r < 0)
                return r;

        return 0;
}

int cg_is_empty(const char *controller, const char *path, bool ignore_self) {
        _cleanup_fclose_ FILE *f = NULL;
        pid_t pid = 0, self_pid;
        bool found = false;
        int r;

        assert(path);

        r = cg_enumerate_processes(controller, path, &f);
        if (r < 0)
                return r == -ENOENT ? 1 : r;

        self_pid = getpid();

        while ((r = cg_read_pid(f, &pid)) > 0) {

                if (ignore_self && pid == self_pid)
                        continue;

                found = true;
                break;
        }

        if (r < 0)
                return r;

        return !found;
}

int cg_is_empty_recursive(const char *controller, const char *path, bool ignore_self) {
        _cleanup_closedir_ DIR *d = NULL;
        char *fn;
        int r;

        assert(path);

        r = cg_is_empty(controller, path, ignore_self);
        if (r <= 0)
                return r;

        r = cg_enumerate_subgroups(controller, path, &d);
        if (r < 0)
                return r == -ENOENT ? 1 : r;

        while ((r = cg_read_subgroup(d, &fn)) > 0) {
                _cleanup_free_ char *p = NULL;

                p = strjoin(path, "/", fn, NULL);
                free(fn);
                if (!p)
                        return -ENOMEM;

                r = cg_is_empty_recursive(controller, p, ignore_self);
                if (r <= 0)
                        return r;
        }

        if (r < 0)
                return r;

        return 1;
}

int cg_split_spec(const char *spec, char **controller, char **path) {
        const char *e;
        char *t = NULL, *u = NULL;
        _cleanup_free_ char *v = NULL;

        assert(spec);

        if (*spec == '/') {
                if (!path_is_safe(spec))
                        return -EINVAL;

                if (path) {
                        t = strdup(spec);
                        if (!t)
                                return -ENOMEM;

                        *path = path_kill_slashes(t);
                }

                if (controller)
                        *controller = NULL;

                return 0;
        }

        e = strchr(spec, ':');
        if (!e) {
                if (!cg_controller_is_valid(spec, true))
                        return -EINVAL;

                if (controller) {
                        t = strdup(normalize_controller(spec));
                        if (!t)
                                return -ENOMEM;

                        *controller = t;
                }

                if (path)
                        *path = NULL;

                return 0;
        }

        v = strndup(spec, e-spec);
        if (!v)
                return -ENOMEM;
        t = strdup(normalize_controller(v));
        if (!t)
                return -ENOMEM;
        if (!cg_controller_is_valid(t, true)) {
                free(t);
                return -EINVAL;
        }

        if (streq(e+1, "")) {
                u = strdup("/");
                if (!u) {
                        free(t);
                        return -ENOMEM;
                }
        } else {
                u = strdup(e+1);
                if (!u) {
                        free(t);
                        return -ENOMEM;
                }

                if (!path_is_safe(u) ||
                    !path_is_absolute(u)) {
                        free(t);
                        free(u);
                        return -EINVAL;
                }

                path_kill_slashes(u);
        }

        if (controller)
                *controller = t;
        else
                free(t);

        if (path)
                *path = u;
        else
                free(u);

        return 0;
}

int cg_mangle_path(const char *path, char **result) {
        _cleanup_free_ char *c = NULL, *p = NULL;
        char *t;
        int r;

        assert(path);
        assert(result);

        /* First, check if it already is a filesystem path */
        if (path_startswith(path, "/sys/fs/cgroup")) {

                t = strdup(path);
                if (!t)
                        return -ENOMEM;

                *result = path_kill_slashes(t);
                return 0;
        }

        /* Otherwise, treat it as cg spec */
        r = cg_split_spec(path, &c, &p);
        if (r < 0)
                return r;

        return cg_get_path(c ? c : SYSTEMD_CGROUP_CONTROLLER, p ? p : "/", NULL, result);
}

int cg_get_root_path(char **path) {
        assert(path);

        return cg_pid_get_path(SYSTEMD_CGROUP_CONTROLLER, 1, path);
}

int cg_shift_path(const char *cgroup, const char *root, const char **shifted) {
        _cleanup_free_ char *rt = NULL;
        char *p;
        int r;

        assert(cgroup);
        assert(shifted);

        if (!root) {
                /* If the root was specified let's use that, otherwise
                 * let's determine it from PID 1 */

                r = cg_get_root_path(&rt);
                if (r < 0)
                        return r;

                root = rt;
        }

        p = path_startswith(cgroup, root);
        if (p)
                *shifted = p - 1;
        else
                *shifted = cgroup;

        return 0;
}

int cg_pid_get_path_shifted(pid_t pid, const char *root, char **cgroup) {
        _cleanup_free_ char *raw = NULL;
        const char *c;
        int r;

        assert(pid >= 0);
        assert(cgroup);

        r = cg_pid_get_path(SYSTEMD_CGROUP_CONTROLLER, pid, &raw);
        if (r < 0)
                return r;

        r = cg_shift_path(raw, root, &c);
        if (r < 0)
                return r;

        if (c == raw) {
                *cgroup = raw;
                raw = NULL;
        } else {
                char *n;

                n = strdup(c);
                if (!n)
                        return -ENOMEM;

                *cgroup = n;
        }

        return 0;
}

int cg_path_get_session(const char *path, char **session) {
        const char *e, *n, *s;

        /* Elogind uses a flat hierarchy, just "/SESSION".  The only
           wrinkle is that SESSION might be escaped.  */

        assert(path);
        assert(path[0] == '/');

        e = path + 1;
        n = strchrnul(e, '/');
        if (e == n)
                return -ENOENT;

        s = strndupa(e, n - e);
        s = cg_unescape(s);

        if (!s[0])
                return -ENOENT;

        if (session) {
                char *r;

                r = strdup(s);
                if (!r)
                        return -ENOMEM;

                *session = r;
        }

        return 0;
}

int cg_pid_get_session(pid_t pid, char **session) {
        _cleanup_free_ char *cgroup = NULL;
        int r;

        r = cg_pid_get_path_shifted(pid, NULL, &cgroup);
        if (r < 0)
                return r;

        return cg_path_get_session(cgroup, session);
}

char *cg_escape(const char *p) {
        bool need_prefix = false;

        /* This implements very minimal escaping for names to be used
         * as file names in the cgroup tree: any name which might
         * conflict with a kernel name or is prefixed with '_' is
         * prefixed with a '_'. That way, when reading cgroup names it
         * is sufficient to remove a single prefixing underscore if
         * there is one. */

        /* The return value of this function (unlike cg_unescape())
         * needs free()! */

        if (p[0] == 0 ||
            p[0] == '_' ||
            p[0] == '.' ||
            streq(p, "notify_on_release") ||
            streq(p, "release_agent") ||
            streq(p, "tasks"))
                need_prefix = true;
        else {
                const char *dot;

                dot = strrchr(p, '.');
                if (dot) {

                        if (dot - p == 6 && memcmp(p, "cgroup", 6) == 0)
                                need_prefix = true;
                        else {
                                char *n;

                                n = strndupa(p, dot - p);

                                if (check_hierarchy(n) >= 0)
                                        need_prefix = true;
                        }
                }
        }

        if (need_prefix)
                return strappend("_", p);
        else
                return strdup(p);
}

char *cg_unescape(const char *p) {
        assert(p);

        /* The return value of this function (unlike cg_escape())
         * doesn't need free()! */

        if (p[0] == '_')
                return (char*) p+1;

        return (char*) p;
}

#define CONTROLLER_VALID                        \
        DIGITS LETTERS                          \
        "_"

bool cg_controller_is_valid(const char *p, bool allow_named) {
        const char *t, *s;

        if (!p)
                return false;

        if (allow_named) {
                s = startswith(p, "name=");
                if (s)
                        p = s;
        }

        if (*p == 0 || *p == '_')
                return false;

        for (t = p; *t; t++)
                if (!strchr(CONTROLLER_VALID, *t))
                        return false;

        if (t - p > FILENAME_MAX)
                return false;

        return true;
}

int cg_set_attribute(const char *controller, const char *path, const char *attribute, const char *value) {
        _cleanup_free_ char *p = NULL;
        int r;

        r = cg_get_path(controller, path, attribute, &p);
        if (r < 0)
                return r;

        return write_string_file_no_create(p, value);
}

int cg_get_attribute(const char *controller, const char *path, const char *attribute, char **ret) {
        _cleanup_free_ char *p = NULL;
        int r;

        r = cg_get_path(controller, path, attribute, &p);
        if (r < 0)
                return r;

        return read_one_line_file(p, ret);
}

static const char mask_names[] =
        "cpu\0"
        "cpuacct\0"
        "blkio\0"
        "memory\0"
        "devices\0";

int cg_create_everywhere(CGroupControllerMask supported, CGroupControllerMask mask, const char *path) {
        CGroupControllerMask bit = 1;
        const char *n;
        int r;

        /* This one will create a cgroup in our private tree, but also
         * duplicate it in the trees specified in mask, and remove it
         * in all others */

        /* First create the cgroup in our own hierarchy. */
        r = cg_create(SYSTEMD_CGROUP_CONTROLLER, path);
        if (r < 0)
                return r;

        /* Then, do the same in the other hierarchies */
        NULSTR_FOREACH(n, mask_names) {
                if (mask & bit)
                        cg_create(n, path);
                else if (supported & bit)
                        cg_trim(n, path, true);

                bit <<= 1;
        }

        return 0;
}

int cg_attach_everywhere(CGroupControllerMask supported, const char *path, pid_t pid, cg_migrate_callback_t path_callback, void *userdata) {
        CGroupControllerMask bit = 1;
        const char *n;
        int r;

        r = cg_attach(SYSTEMD_CGROUP_CONTROLLER, path, pid);
        if (r < 0)
                return r;

        NULSTR_FOREACH(n, mask_names) {

                if (supported & bit) {
                        const char *p = NULL;

                        if (path_callback)
                                p = path_callback(bit, userdata);

                        if (!p)
                                p = path;

                        cg_attach_fallback(n, path, pid);
                }

                bit <<= 1;
        }

        return 0;
}

int cg_attach_many_everywhere(CGroupControllerMask supported, const char *path, Set* pids, cg_migrate_callback_t path_callback, void *userdata) {
        Iterator i;
        void *pidp;
        int r = 0;

        SET_FOREACH(pidp, pids, i) {
                pid_t pid = PTR_TO_LONG(pidp);
                int q;

                q = cg_attach_everywhere(supported, path, pid, path_callback, userdata);
                if (q < 0)
                        r = q;
        }

        return r;
}

int cg_migrate_everywhere(CGroupControllerMask supported, const char *from, const char *to, cg_migrate_callback_t to_callback, void *userdata) {
        CGroupControllerMask bit = 1;
        const char *n;
        int r;

        if (!path_equal(from, to))  {
                r = cg_migrate_recursive(SYSTEMD_CGROUP_CONTROLLER, from, SYSTEMD_CGROUP_CONTROLLER, to, false, true);
                if (r < 0)
                        return r;
        }

        NULSTR_FOREACH(n, mask_names) {
                if (supported & bit) {
                        const char *p = NULL;

                        if (to_callback)
                                p = to_callback(bit, userdata);

                        if (!p)
                                p = to;

                        cg_migrate_recursive_fallback(SYSTEMD_CGROUP_CONTROLLER, to, n, p, false, false);
                }

                bit <<= 1;
        }

        return 0;
}

int cg_trim_everywhere(CGroupControllerMask supported, const char *path, bool delete_root) {
        CGroupControllerMask bit = 1;
        const char *n;
        int r;

        r = cg_trim(SYSTEMD_CGROUP_CONTROLLER, path, delete_root);
        if (r < 0)
                return r;

        NULSTR_FOREACH(n, mask_names) {
                if (supported & bit)
                        cg_trim(n, path, delete_root);

                bit <<= 1;
        }

        return 0;
}

CGroupControllerMask cg_mask_supported(void) {
        CGroupControllerMask bit = 1, mask = 0;
        const char *n;

        NULSTR_FOREACH(n, mask_names) {
                if (check_hierarchy(n) >= 0)
                        mask |= bit;

                bit <<= 1;
        }

        return mask;
}

int cg_kernel_controllers(Set *controllers) {
        _cleanup_fclose_ FILE *f = NULL;
        char buf[LINE_MAX];
        int r;

        assert(controllers);

        f = fopen("/proc/cgroups", "re");
        if (!f) {
                if (errno == ENOENT)
                        return 0;
                return -errno;
        }

        /* Ignore the header line */
        (void) fgets(buf, sizeof(buf), f);

        for (;;) {
                char *controller;
                int enabled = 0;

                errno = 0;
                if (fscanf(f, "%ms %*i %*i %i", &controller, &enabled) != 2) {

                        if (feof(f))
                                break;

                        if (ferror(f) && errno)
                                return -errno;

                        return -EBADMSG;
                }

                if (!enabled) {
                        free(controller);
                        continue;
                }

                if (!filename_is_valid(controller)) {
                        free(controller);
                        return -EBADMSG;
                }

                r = set_consume(controllers, controller);
                if (r < 0)
                        return r;
        }

        return 0;
}
