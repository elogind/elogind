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

#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include <libintl.h>
#include <locale.h>
#include <stdio.h>
#include <syslog.h>
#include <sched.h>
#include <sys/resource.h>
#include <linux/sched.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <linux/vt.h>
#include <linux/tiocl.h>
#include <termios.h>
#include <stdarg.h>
#include <poll.h>
#include <ctype.h>
#include <sys/prctl.h>
#include <sys/utsname.h>
#include <pwd.h>
#include <netinet/ip.h>
#include <linux/kd.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <glob.h>
#include <grp.h>
#include <sys/mman.h>
#include <sys/vfs.h>
#include <sys/mount.h>
#include <linux/magic.h>
#include <limits.h>
#include <langinfo.h>
#include <locale.h>
#include <sys/personality.h>
#include <sys/xattr.h>
#include <sys/statvfs.h>
#include <sys/file.h>
#include <linux/fs.h>

/* When we include libgen.h because we need dirname() we immediately
 * undefine basename() since libgen.h defines it as a macro to the XDG
 * version which is really broken. */
#include <libgen.h>
#undef basename

#ifdef HAVE_SYS_AUXV_H
#include <sys/auxv.h>
#endif

#include "basic/musl_missing.h"
#include "config.h"
#include "macro.h"
#include "util.h"
#include "ioprio.h"
#include "missing.h"
#include "log.h"
#include "strv.h"
#include "mkdir.h"
#include "path-util.h"
#include "exit-status.h"
#include "hashmap.h"
#include "env-util.h"
#include "fileio.h"
#include "device-nodes.h"
#include "utf8.h"
#include "gunicode.h"
#include "virt.h"
#include "def.h"
#include "sparse-endian.h"

/* Put this test here for a lack of better place */
assert_cc(EAGAIN == EWOULDBLOCK);

int saved_argc = 0;
char **saved_argv = NULL;

static volatile unsigned cached_columns = 0;
static volatile unsigned cached_lines = 0;

size_t page_size(void) {
        static thread_local size_t pgsz = 0;
        long r;

        if (_likely_(pgsz > 0))
                return pgsz;

        r = sysconf(_SC_PAGESIZE);
        assert(r > 0);

        pgsz = (size_t) r;
        return pgsz;
}

bool streq_ptr(const char *a, const char *b) {

        /* Like streq(), but tries to make sense of NULL pointers */

        if (a && b)
                return streq(a, b);

        if (!a && !b)
                return true;

        return false;
}

char* endswith(const char *s, const char *postfix) {
        size_t sl, pl;

        assert(s);
        assert(postfix);

        sl = strlen(s);
        pl = strlen(postfix);

        if (pl == 0)
                return (char*) s + sl;

        if (sl < pl)
                return NULL;

        if (memcmp(s + sl - pl, postfix, pl) != 0)
                return NULL;

        return (char*) s + sl - pl;
}

char* first_word(const char *s, const char *word) {
        size_t sl, wl;
        const char *p;

        assert(s);
        assert(word);

        /* Checks if the string starts with the specified word, either
         * followed by NUL or by whitespace. Returns a pointer to the
         * NUL or the first character after the whitespace. */

        sl = strlen(s);
        wl = strlen(word);

        if (sl < wl)
                return NULL;

        if (wl == 0)
                return (char*) s;

        if (memcmp(s, word, wl) != 0)
                return NULL;

        p = s + wl;
        if (*p == 0)
                return (char*) p;

        if (!strchr(WHITESPACE, *p))
                return NULL;

        p += strspn(p, WHITESPACE);
        return (char*) p;
}

static size_t cescape_char(char c, char *buf) {
        char * buf_old = buf;

        switch (c) {

                case '\a':
                        *(buf++) = '\\';
                        *(buf++) = 'a';
                        break;
                case '\b':
                        *(buf++) = '\\';
                        *(buf++) = 'b';
                        break;
                case '\f':
                        *(buf++) = '\\';
                        *(buf++) = 'f';
                        break;
                case '\n':
                        *(buf++) = '\\';
                        *(buf++) = 'n';
                        break;
                case '\r':
                        *(buf++) = '\\';
                        *(buf++) = 'r';
                        break;
                case '\t':
                        *(buf++) = '\\';
                        *(buf++) = 't';
                        break;
                case '\v':
                        *(buf++) = '\\';
                        *(buf++) = 'v';
                        break;
                case '\\':
                        *(buf++) = '\\';
                        *(buf++) = '\\';
                        break;
                case '"':
                        *(buf++) = '\\';
                        *(buf++) = '"';
                        break;
                case '\'':
                        *(buf++) = '\\';
                        *(buf++) = '\'';
                        break;

                default:
                        /* For special chars we prefer octal over
                         * hexadecimal encoding, simply because glib's
                         * g_strescape() does the same */
                        if ((c < ' ') || (c >= 127)) {
                                *(buf++) = '\\';
                                *(buf++) = octchar((unsigned char) c >> 6);
                                *(buf++) = octchar((unsigned char) c >> 3);
                                *(buf++) = octchar((unsigned char) c);
                        } else
                                *(buf++) = c;
                        break;
        }

        return buf - buf_old;
}

int close_nointr(int fd) {
        assert(fd >= 0);

        if (close(fd) >= 0)
                return 0;

        /*
         * Just ignore EINTR; a retry loop is the wrong thing to do on
         * Linux.
         *
         * http://lkml.indiana.edu/hypermail/linux/kernel/0509.1/0877.html
         * https://bugzilla.gnome.org/show_bug.cgi?id=682819
         * http://utcc.utoronto.ca/~cks/space/blog/unix/CloseEINTR
         * https://sites.google.com/site/michaelsafyan/software-engineering/checkforeintrwheninvokingclosethinkagain
         */
        if (errno == EINTR)
                return 0;

        return -errno;
}

int safe_close(int fd) {

        /*
         * Like close_nointr() but cannot fail. Guarantees errno is
         * unchanged. Is a NOP with negative fds passed, and returns
         * -1, so that it can be used in this syntax:
         *
         * fd = safe_close(fd);
         */

        if (fd >= 0) {
                PROTECT_ERRNO;

                /* The kernel might return pretty much any error code
                 * via close(), but the fd will be closed anyway. The
                 * only condition we want to check for here is whether
                 * the fd was invalid at all... */

                assert_se(close_nointr(fd) != -EBADF);
        }

        return -1;
}

void close_many(const int fds[], unsigned n_fd) {
        unsigned i;

        assert(fds || n_fd <= 0);

        for (i = 0; i < n_fd; i++)
                safe_close(fds[i]);
}

int unlink_noerrno(const char *path) {
        PROTECT_ERRNO;
        int r;

        r = unlink(path);
        if (r < 0)
                return -errno;

        return 0;
}

int parse_boolean(const char *v) {
        assert(v);

        if (streq(v, "1") || strcaseeq(v, "yes") || strcaseeq(v, "y") || strcaseeq(v, "true") || strcaseeq(v, "t") || strcaseeq(v, "on"))
                return 1;
        else if (streq(v, "0") || strcaseeq(v, "no") || strcaseeq(v, "n") || strcaseeq(v, "false") || strcaseeq(v, "f") || strcaseeq(v, "off"))
                return 0;

        return -EINVAL;
}

int parse_pid(const char *s, pid_t* ret_pid) {
        unsigned long ul = 0;
        pid_t pid;
        int r;

        assert(s);
        assert(ret_pid);

        r = safe_atolu(s, &ul);
        if (r < 0)
                return r;

        pid = (pid_t) ul;

        if ((unsigned long) pid != ul)
                return -ERANGE;

        if (pid <= 0)
                return -ERANGE;

        *ret_pid = pid;
        return 0;
}

int parse_uid(const char *s, uid_t* ret_uid) {
        unsigned long ul = 0;
        uid_t uid;
        int r;

        assert(s);
        assert(ret_uid);

        r = safe_atolu(s, &ul);
        if (r < 0)
                return r;

        uid = (uid_t) ul;

        if ((unsigned long) uid != ul)
                return -ERANGE;

        /* Some libc APIs use UID_INVALID as special placeholder */
        if (uid == (uid_t) 0xFFFFFFFF)
                return -ENXIO;

        /* A long time ago UIDs where 16bit, hence explicitly avoid the 16bit -1 too */
        if (uid == (uid_t) 0xFFFF)
                return -ENXIO;

        *ret_uid = uid;
        return 0;
}

int safe_atou(const char *s, unsigned *ret_u) {
        char *x = NULL;
        unsigned long l;

        assert(s);
        assert(ret_u);

        errno = 0;
        l = strtoul(s, &x, 0);

        if (!x || x == s || *x || errno)
                return errno > 0 ? -errno : -EINVAL;

        if ((unsigned long) (unsigned) l != l)
                return -ERANGE;

        *ret_u = (unsigned) l;
        return 0;
}

int safe_atoi(const char *s, int *ret_i) {
        char *x = NULL;
        long l;

        assert(s);
        assert(ret_i);

        errno = 0;
        l = strtol(s, &x, 0);

        if (!x || x == s || *x || errno)
                return errno > 0 ? -errno : -EINVAL;

        if ((long) (int) l != l)
                return -ERANGE;

        *ret_i = (int) l;
        return 0;
}

int safe_atou8(const char *s, uint8_t *ret) {
        char *x = NULL;
        unsigned long l;

        assert(s);
        assert(ret);

        errno = 0;
        l = strtoul(s, &x, 0);

        if (!x || x == s || *x || errno)
                return errno > 0 ? -errno : -EINVAL;

        if ((unsigned long) (uint8_t) l != l)
                return -ERANGE;

        *ret = (uint8_t) l;
        return 0;
}

int safe_atou16(const char *s, uint16_t *ret) {
        char *x = NULL;
        unsigned long l;

        assert(s);
        assert(ret);

        errno = 0;
        l = strtoul(s, &x, 0);

        if (!x || x == s || *x || errno)
                return errno > 0 ? -errno : -EINVAL;

        if ((unsigned long) (uint16_t) l != l)
                return -ERANGE;

        *ret = (uint16_t) l;
        return 0;
}

int safe_atoi16(const char *s, int16_t *ret) {
        char *x = NULL;
        long l;

        assert(s);
        assert(ret);

        errno = 0;
        l = strtol(s, &x, 0);

        if (!x || x == s || *x || errno)
                return errno > 0 ? -errno : -EINVAL;

        if ((long) (int16_t) l != l)
                return -ERANGE;

        *ret = (int16_t) l;
        return 0;
}

int safe_atollu(const char *s, long long unsigned *ret_llu) {
        char *x = NULL;
        unsigned long long l;

        assert(s);
        assert(ret_llu);

        errno = 0;
        l = strtoull(s, &x, 0);

        if (!x || x == s || *x || errno)
                return errno ? -errno : -EINVAL;

        *ret_llu = l;
        return 0;
}

int safe_atolli(const char *s, long long int *ret_lli) {
        char *x = NULL;
        long long l;

        assert(s);
        assert(ret_lli);

        errno = 0;
        l = strtoll(s, &x, 0);

        if (!x || x == s || *x || errno)
                return errno ? -errno : -EINVAL;

        *ret_lli = l;
        return 0;
}

int safe_atod(const char *s, double *ret_d) {
        char *x = NULL;
        double d = 0;
        locale_t loc;

        assert(s);
        assert(ret_d);

        loc = newlocale(LC_NUMERIC_MASK, "C", (locale_t) 0);
        if (loc == (locale_t) 0)
                return -errno;

        errno = 0;
        d = strtod_l(s, &x, loc);

        if (!x || x == s || *x || errno) {
                freelocale(loc);
                return errno ? -errno : -EINVAL;
        }

        freelocale(loc);
        *ret_d = (double) d;
        return 0;
}

static size_t strcspn_escaped(const char *s, const char *reject) {
        bool escaped = false;
        int n;

        for (n=0; s[n]; n++) {
                if (escaped)
                        escaped = false;
                else if (s[n] == '\\')
                        escaped = true;
                else if (strchr(reject, s[n]))
                        break;
        }

        /* if s ends in \, return index of previous char */
        return n - escaped;
}

/* Split a string into words. */
const char* split(const char **state, size_t *l, const char *separator, bool quoted) {
        const char *current;

        current = *state;

        if (!*current) {
                assert(**state == '\0');
                return NULL;
        }

        current += strspn(current, separator);
        if (!*current) {
                *state = current;
                return NULL;
        }

        if (quoted && strchr("\'\"", *current)) {
                char quotechars[2] = {*current, '\0'};

                *l = strcspn_escaped(current + 1, quotechars);
                if (current[*l + 1] == '\0' ||
                    (current[*l + 2] && !strchr(separator, current[*l + 2]))) {
                        /* right quote missing or garbage at the end */
                        *state = current;
                        return NULL;
                }
                assert(current[*l + 1] == quotechars[0]);
                *state = current++ + *l + 2;
        } else if (quoted) {
                *l = strcspn_escaped(current, separator);
                if (current[*l] && !strchr(separator, current[*l])) {
                        /* unfinished escape */
                        *state = current;
                        return NULL;
                }
                *state = current + *l;
        } else {
                *l = strcspn(current, separator);
                *state = current + *l;
        }

        return current;
}

int get_parent_of_pid(pid_t pid, pid_t *_ppid) {
        int r;
        _cleanup_free_ char *line = NULL;
        long unsigned ppid;
        const char *p;

        assert(pid >= 0);
        assert(_ppid);

        if (pid == 0) {
                *_ppid = getppid();
                return 0;
        }

        p = procfs_file_alloca(pid, "stat");
        r = read_one_line_file(p, &line);
        if (r < 0)
                return r;

        /* Let's skip the pid and comm fields. The latter is enclosed
         * in () but does not escape any () in its value, so let's
         * skip over it manually */

        p = strrchr(line, ')');
        if (!p)
                return -EIO;

        p++;

        if (sscanf(p, " "
                   "%*c "  /* state */
                   "%lu ", /* ppid */
                   &ppid) != 1)
                return -EIO;

        if ((long unsigned) (pid_t) ppid != ppid)
                return -ERANGE;

        *_ppid = (pid_t) ppid;

        return 0;
}

int fchmod_umask(int fd, mode_t m) {
        mode_t u;
        int r;

        u = umask(0777);
        r = fchmod(fd, m & (~u)) < 0 ? -errno : 0;
        umask(u);

        return r;
}

char *truncate_nl(char *s) {
        assert(s);

        s[strcspn(s, NEWLINE)] = 0;
        return s;
}

int get_process_state(pid_t pid) {
        const char *p;
        char state;
        int r;
        _cleanup_free_ char *line = NULL;

        assert(pid >= 0);

        p = procfs_file_alloca(pid, "stat");
        r = read_one_line_file(p, &line);
        if (r < 0)
                return r;

        p = strrchr(line, ')');
        if (!p)
                return -EIO;

        p++;

        if (sscanf(p, " %c", &state) != 1)
                return -EIO;

        return (unsigned char) state;
}

int get_process_comm(pid_t pid, char **name) {
        const char *p;
        int r;

        assert(name);
        assert(pid >= 0);

        p = procfs_file_alloca(pid, "comm");

        r = read_one_line_file(p, name);
        if (r == -ENOENT)
                return -ESRCH;

        return r;
}

int get_process_cmdline(pid_t pid, size_t max_length, bool comm_fallback, char **line) {
        _cleanup_fclose_ FILE *f = NULL;
        char *r = NULL, *k;
        const char *p;
        int c;

        assert(line);
        assert(pid >= 0);

        p = procfs_file_alloca(pid, "cmdline");

        f = fopen(p, "re");
        if (!f)
                return -errno;

        if (max_length == 0) {
                size_t len = 0, allocated = 0;

                while ((c = getc(f)) != EOF) {

                        if (!GREEDY_REALLOC(r, allocated, len+2)) {
                                free(r);
                                return -ENOMEM;
                        }

                        r[len++] = isprint(c) ? c : ' ';
                }

                if (len > 0)
                        r[len-1] = 0;

        } else {
                bool space = false;
                size_t left;

                r = new(char, max_length);
                if (!r)
                        return -ENOMEM;

                k = r;
                left = max_length;
                while ((c = getc(f)) != EOF) {

                        if (isprint(c)) {
                                if (space) {
                                        if (left <= 4)
                                                break;

                                        *(k++) = ' ';
                                        left--;
                                        space = false;
                                }

                                if (left <= 4)
                                        break;

                                *(k++) = (char) c;
                                left--;
                        }  else
                                space = true;
                }

                if (left <= 4) {
                        size_t n = MIN(left-1, 3U);
                        memcpy(k, "...", n);
                        k[n] = 0;
                } else
                        *k = 0;
        }

        /* Kernel threads have no argv[] */
        if (isempty(r)) {
                _cleanup_free_ char *t = NULL;
                int h;

                free(r);

                if (!comm_fallback)
                        return -ENOENT;

                h = get_process_comm(pid, &t);
                if (h < 0)
                        return h;

                r = strjoin("[", t, "]", NULL);
                if (!r)
                        return -ENOMEM;
        }

        *line = r;
        return 0;
}

int is_kernel_thread(pid_t pid) {
        const char *p;
        size_t count;
        char c;
        bool eof;
        FILE *f;

        if (pid == 0)
                return 0;

        assert(pid > 0);

        p = procfs_file_alloca(pid, "cmdline");
        f = fopen(p, "re");
        if (!f)
                return -errno;

        count = fread(&c, 1, 1, f);
        eof = feof(f);
        fclose(f);

        /* Kernel threads have an empty cmdline */

        if (count <= 0)
                return eof ? 1 : -errno;

        return 0;
}

int get_process_capeff(pid_t pid, char **capeff) {
        const char *p;

        assert(capeff);
        assert(pid >= 0);

        p = procfs_file_alloca(pid, "status");

        return get_status_field(p, "\nCapEff:", capeff);
}

static int get_process_link_contents(const char *proc_file, char **name) {
        int r;

        assert(proc_file);
        assert(name);

        r = readlink_malloc(proc_file, name);
        if (r < 0)
                return r == -ENOENT ? -ESRCH : r;

        return 0;
}

int get_process_exe(pid_t pid, char **name) {
        const char *p;
        char *d;
        int r;

        assert(pid >= 0);

        p = procfs_file_alloca(pid, "exe");
        r = get_process_link_contents(p, name);
        if (r < 0)
                return r;

        d = endswith(*name, " (deleted)");
        if (d)
                *d = '\0';

        return 0;
}

static int get_process_id(pid_t pid, const char *field, uid_t *uid) {
        _cleanup_fclose_ FILE *f = NULL;
        char line[LINE_MAX];
        const char *p;

        assert(field);
        assert(uid);

        if (pid == 0)
                return getuid();

        p = procfs_file_alloca(pid, "status");
        f = fopen(p, "re");
        if (!f)
                return -errno;

        FOREACH_LINE(line, f, return -errno) {
                char *l;

                l = strstrip(line);

                if (startswith(l, field)) {
                        l += strlen(field);
                        l += strspn(l, WHITESPACE);

                        l[strcspn(l, WHITESPACE)] = 0;

                        return parse_uid(l, uid);
                }
        }

        return -EIO;
}

int get_process_uid(pid_t pid, uid_t *uid) {
        return get_process_id(pid, "Uid:", uid);
}

int get_process_gid(pid_t pid, gid_t *gid) {
        assert_cc(sizeof(uid_t) == sizeof(gid_t));
        return get_process_id(pid, "Gid:", gid);
}

int get_process_cwd(pid_t pid, char **cwd) {
        const char *p;

        assert(pid >= 0);

        p = procfs_file_alloca(pid, "cwd");

        return get_process_link_contents(p, cwd);
}

int get_process_root(pid_t pid, char **root) {
        const char *p;

        assert(pid >= 0);

        p = procfs_file_alloca(pid, "root");

        return get_process_link_contents(p, root);
}

int get_process_environ(pid_t pid, char **env) {
        _cleanup_fclose_ FILE *f = NULL;
        _cleanup_free_ char *outcome = NULL;
        int c;
        const char *p;
        size_t allocated = 0, sz = 0;

        assert(pid >= 0);
        assert(env);

        p = procfs_file_alloca(pid, "environ");

        f = fopen(p, "re");
        if (!f)
                return -errno;

        while ((c = fgetc(f)) != EOF) {
                if (!GREEDY_REALLOC(outcome, allocated, sz + 5))
                        return -ENOMEM;

                if (c == '\0')
                        outcome[sz++] = '\n';
                else
                        sz += cescape_char(c, outcome + sz);
        }

        outcome[sz] = '\0';
        *env = outcome;
        outcome = NULL;

        return 0;
}

char *strnappend(const char *s, const char *suffix, size_t b) {
        size_t a;
        char *r;

        if (!s && !suffix)
                return strdup("");

        if (!s)
                return strndup(suffix, b);

        if (!suffix)
                return strdup(s);

        assert(s);
        assert(suffix);

        a = strlen(s);
        if (b > ((size_t) -1) - a)
                return NULL;

        r = new(char, a+b+1);
        if (!r)
                return NULL;

        memcpy(r, s, a);
        memcpy(r+a, suffix, b);
        r[a+b] = 0;

        return r;
}

char *strappend(const char *s, const char *suffix) {
        return strnappend(s, suffix, suffix ? strlen(suffix) : 0);
}

int readlinkat_malloc(int fd, const char *p, char **ret) {
        size_t l = 100;
        int r;

        assert(p);
        assert(ret);

        for (;;) {
                char *c;
                ssize_t n;

                c = new(char, l);
                if (!c)
                        return -ENOMEM;

                n = readlinkat(fd, p, c, l-1);
                if (n < 0) {
                        r = -errno;
                        free(c);
                        return r;
                }

                if ((size_t) n < l-1) {
                        c[n] = 0;
                        *ret = c;
                        return 0;
                }

                free(c);
                l *= 2;
        }
}

int readlink_malloc(const char *p, char **ret) {
        return readlinkat_malloc(AT_FDCWD, p, ret);
}

int readlink_value(const char *p, char **ret) {
        _cleanup_free_ char *link = NULL;
        char *value;
        int r;

        r = readlink_malloc(p, &link);
        if (r < 0)
                return r;

        value = basename(link);
        if (!value)
                return -ENOENT;

        value = strdup(value);
        if (!value)
                return -ENOMEM;

        *ret = value;

        return 0;
}

int readlink_and_make_absolute(const char *p, char **r) {
        _cleanup_free_ char *target = NULL;
        char *k;
        int j;

        assert(p);
        assert(r);

        j = readlink_malloc(p, &target);
        if (j < 0)
                return j;

        k = file_in_same_dir(p, target);
        if (!k)
                return -ENOMEM;

        *r = k;
        return 0;
}

int readlink_and_canonicalize(const char *p, char **r) {
        char *t, *s;
        int j;

        assert(p);
        assert(r);

        j = readlink_and_make_absolute(p, &t);
        if (j < 0)
                return j;

        s = canonicalize_file_name(t);
        if (s) {
                free(t);
                *r = s;
        } else
                *r = t;

        path_kill_slashes(*r);

        return 0;
}

int reset_all_signal_handlers(void) {
        int sig, r = 0;

        for (sig = 1; sig < _NSIG; sig++) {
                struct sigaction sa = {
                        .sa_handler = SIG_DFL,
                        .sa_flags = SA_RESTART,
                };

                /* These two cannot be caught... */
                if (sig == SIGKILL || sig == SIGSTOP)
                        continue;

                /* On Linux the first two RT signals are reserved by
                 * glibc, and sigaction() will return EINVAL for them. */
                if ((sigaction(sig, &sa, NULL) < 0))
                        if (errno != EINVAL && r == 0)
                                r = -errno;
        }

        return r;
}

int reset_signal_mask(void) {
        sigset_t ss;

        if (sigemptyset(&ss) < 0)
                return -errno;

        if (sigprocmask(SIG_SETMASK, &ss, NULL) < 0)
                return -errno;

        return 0;
}

char *strstrip(char *s) {
        char *e;

        /* Drops trailing whitespace. Modifies the string in
         * place. Returns pointer to first non-space character */

        s += strspn(s, WHITESPACE);

        for (e = strchr(s, 0); e > s; e --)
                if (!strchr(WHITESPACE, e[-1]))
                        break;

        *e = 0;

        return s;
}

char *delete_chars(char *s, const char *bad) {
        char *f, *t;

        /* Drops all whitespace, regardless where in the string */

        for (f = s, t = s; *f; f++) {
                if (strchr(bad, *f))
                        continue;

                *(t++) = *f;
        }

        *t = 0;

        return s;
}

char *file_in_same_dir(const char *path, const char *filename) {
        char *e, *ret;
        size_t k;

        assert(path);
        assert(filename);

        /* This removes the last component of path and appends
         * filename, unless the latter is absolute anyway or the
         * former isn't */

        if (path_is_absolute(filename))
                return strdup(filename);

        e = strrchr(path, '/');
        if (!e)
                return strdup(filename);

        k = strlen(filename);
        ret = new(char, (e + 1 - path) + k + 1);
        if (!ret)
                return NULL;

        memcpy(mempcpy(ret, path, e + 1 - path), filename, k + 1);
        return ret;
}

int rmdir_parents(const char *path, const char *stop) {
        size_t l;
        int r = 0;

        assert(path);
        assert(stop);

        l = strlen(path);

        /* Skip trailing slashes */
        while (l > 0 && path[l-1] == '/')
                l--;

        while (l > 0) {
                char *t;

                /* Skip last component */
                while (l > 0 && path[l-1] != '/')
                        l--;

                /* Skip trailing slashes */
                while (l > 0 && path[l-1] == '/')
                        l--;

                if (l <= 0)
                        break;

                if (!(t = strndup(path, l)))
                        return -ENOMEM;

                if (path_startswith(stop, t)) {
                        free(t);
                        return 0;
                }

                r = rmdir(t);
                free(t);

                if (r < 0)
                        if (errno != ENOENT)
                                return -errno;
        }

        return 0;
}

char hexchar(int x) {
        static const char table[16] = "0123456789abcdef";

        return table[x & 15];
}

int unhexchar(char c) {

        if (c >= '0' && c <= '9')
                return c - '0';

        if (c >= 'a' && c <= 'f')
                return c - 'a' + 10;

        if (c >= 'A' && c <= 'F')
                return c - 'A' + 10;

        return -EINVAL;
}

char *hexmem(const void *p, size_t l) {
        char *r, *z;
        const uint8_t *x;

        z = r = malloc(l * 2 + 1);
        if (!r)
                return NULL;

        for (x = p; x < (const uint8_t*) p + l; x++) {
                *(z++) = hexchar(*x >> 4);
                *(z++) = hexchar(*x & 15);
        }

        *z = 0;
        return r;
}

void *unhexmem(const char *p, size_t l) {
        uint8_t *r, *z;
        const char *x;

        assert(p);

        z = r = malloc((l + 1) / 2 + 1);
        if (!r)
                return NULL;

        for (x = p; x < p + l; x += 2) {
                int a, b;

                a = unhexchar(x[0]);
                if (x+1 < p + l)
                        b = unhexchar(x[1]);
                else
                        b = 0;

                *(z++) = (uint8_t) a << 4 | (uint8_t) b;
        }

        *z = 0;
        return r;
}

char octchar(int x) {
        return '0' + (x & 7);
}

int unoctchar(char c) {

        if (c >= '0' && c <= '7')
                return c - '0';

        return -EINVAL;
}

char decchar(int x) {
        return '0' + (x % 10);
}

int undecchar(char c) {

        if (c >= '0' && c <= '9')
                return c - '0';

        return -EINVAL;
}

char *cescape(const char *s) {
        char *r, *t;
        const char *f;

        assert(s);

        /* Does C style string escaping. */

        r = new(char, strlen(s)*4 + 1);
        if (!r)
                return NULL;

        for (f = s, t = r; *f; f++)
                t += cescape_char(*f, t);

        *t = 0;

        return r;
}

static int cunescape_one(const char *p, size_t length, char *ret) {
        int r = 1;

        assert(p);
        assert(*p);
        assert(ret);

        if (length != (size_t) -1 && length < 1)
                return -EINVAL;

        switch (p[0]) {

        case 'a':
                *ret = '\a';
                break;
        case 'b':
                *ret = '\b';
                break;
        case 'f':
                *ret = '\f';
                break;
        case 'n':
                *ret = '\n';
                break;
        case 'r':
                *ret = '\r';
                break;
        case 't':
                *ret = '\t';
                break;
        case 'v':
                *ret = '\v';
                break;
        case '\\':
                *ret = '\\';
                break;
        case '"':
                *ret = '"';
                break;
        case '\'':
                *ret = '\'';
                break;

        case 's':
                /* This is an extension of the XDG syntax files */
                *ret = ' ';
                break;

        case 'x': {
                /* hexadecimal encoding */
                int a, b;

                if (length != (size_t) -1 && length < 3)
                        return -EINVAL;

                a = unhexchar(p[1]);
                if (a < 0)
                        return -EINVAL;

                b = unhexchar(p[2]);
                if (b < 0)
                        return -EINVAL;

                /* don't allow NUL bytes */
                if (a == 0 && b == 0)
                        return -EINVAL;

                *ret = (char) ((a << 4) | b);
                r = 3;
                break;
        }

        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7': {
                /* octal encoding */
                int a, b, c, m;

                if (length != (size_t) -1 && length < 4)
                        return -EINVAL;

                a = unoctchar(p[0]);
                if (a < 0)
                        return -EINVAL;

                b = unoctchar(p[1]);
                if (b < 0)
                        return -EINVAL;

                c = unoctchar(p[2]);
                if (c < 0)
                        return -EINVAL;

                /* don't allow NUL bytes */
                if (a == 0 && b == 0 && c == 0)
                        return -EINVAL;

                /* Don't allow bytes above 255 */
                m = (a << 6) | (b << 3) | c;
                if (m > 255)
                        return -EINVAL;

                *ret = (char) m;
                r = 3;
                break;
        }

        default:
                return -EINVAL;
        }

        return r;
}

char *cunescape_length_with_prefix(const char *s, size_t length, const char *prefix) {
        char *r, *t;
        const char *f;
        size_t pl;

        assert(s);

        /* Undoes C style string escaping, and optionally prefixes it. */

        pl = prefix ? strlen(prefix) : 0;

        r = new(char, pl+length+1);
        if (!r)
                return NULL;

        if (prefix)
                memcpy(r, prefix, pl);

        for (f = s, t = r + pl; f < s + length; f++) {
                size_t remaining;
                int k;

                remaining = s + length - f;
                assert(remaining > 0);

                if (*f != '\\' || remaining == 1) {
                        /* a literal literal, or a trailing backslash, copy verbatim */
                        *(t++) = *f;
                        continue;
                }

                k = cunescape_one(f + 1, remaining - 1, t);
                if (k < 0) {
                        /* Invalid escape code, let's take it literal then */
                        *(t++) = '\\';
                        continue;
                }

                f += k;
                t++;
        }

        *t = 0;
        return r;
}

char *cunescape_length(const char *s, size_t length) {
        return cunescape_length_with_prefix(s, length, NULL);
}

char *cunescape(const char *s) {
        assert(s);

        return cunescape_length(s, strlen(s));
}

char *xescape(const char *s, const char *bad) {
        char *r, *t;
        const char *f;

        /* Escapes all chars in bad, in addition to \ and all special
         * chars, in \xFF style escaping. May be reversed with
         * cunescape. */

        r = new(char, strlen(s) * 4 + 1);
        if (!r)
                return NULL;

        for (f = s, t = r; *f; f++) {

                if ((*f < ' ') || (*f >= 127) ||
                    (*f == '\\') || strchr(bad, *f)) {
                        *(t++) = '\\';
                        *(t++) = 'x';
                        *(t++) = hexchar(*f >> 4);
                        *(t++) = hexchar(*f);
                } else
                        *(t++) = *f;
        }

        *t = 0;

        return r;
}

char *ascii_strlower(char *t) {
        char *p;

        assert(t);

        for (p = t; *p; p++)
                if (*p >= 'A' && *p <= 'Z')
                        *p = *p - 'A' + 'a';

        return t;
}

_pure_ static bool hidden_file_allow_backup(const char *filename) {
        assert(filename);

        return
                filename[0] == '.' ||
                streq(filename, "lost+found") ||
                streq(filename, "aquota.user") ||
                streq(filename, "aquota.group") ||
                endswith(filename, ".rpmnew") ||
                endswith(filename, ".rpmsave") ||
                endswith(filename, ".rpmorig") ||
                endswith(filename, ".dpkg-old") ||
                endswith(filename, ".dpkg-new") ||
                endswith(filename, ".dpkg-tmp") ||
                endswith(filename, ".dpkg-dist") ||
                endswith(filename, ".dpkg-bak") ||
                endswith(filename, ".dpkg-backup") ||
                endswith(filename, ".dpkg-remove") ||
                endswith(filename, ".swp");
}

bool hidden_file(const char *filename) {
        assert(filename);

        if (endswith(filename, "~"))
                return true;

        return hidden_file_allow_backup(filename);
}

int fd_nonblock(int fd, bool nonblock) {
        int flags, nflags;

        assert(fd >= 0);

        flags = fcntl(fd, F_GETFL, 0);
        if (flags < 0)
                return -errno;

        if (nonblock)
                nflags = flags | O_NONBLOCK;
        else
                nflags = flags & ~O_NONBLOCK;

        if (nflags == flags)
                return 0;

        if (fcntl(fd, F_SETFL, nflags) < 0)
                return -errno;

        return 0;
}

int fd_cloexec(int fd, bool cloexec) {
        int flags, nflags;

        assert(fd >= 0);

        flags = fcntl(fd, F_GETFD, 0);
        if (flags < 0)
                return -errno;

        if (cloexec)
                nflags = flags | FD_CLOEXEC;
        else
                nflags = flags & ~FD_CLOEXEC;

        if (nflags == flags)
                return 0;

        if (fcntl(fd, F_SETFD, nflags) < 0)
                return -errno;

        return 0;
}

_pure_ static bool fd_in_set(int fd, const int fdset[], unsigned n_fdset) {
        unsigned i;

        assert(n_fdset == 0 || fdset);

        for (i = 0; i < n_fdset; i++)
                if (fdset[i] == fd)
                        return true;

        return false;
}

int close_all_fds(const int except[], unsigned n_except) {
        _cleanup_closedir_ DIR *d = NULL;
        struct dirent *de;
        int r = 0;

        assert(n_except == 0 || except);

        d = opendir("/proc/self/fd");
        if (!d) {
                int fd;
                struct rlimit rl;

                /* When /proc isn't available (for example in chroots)
                 * the fallback is brute forcing through the fd
                 * table */

                assert_se(getrlimit(RLIMIT_NOFILE, &rl) >= 0);
                for (fd = 3; fd < (int) rl.rlim_max; fd ++) {

                        if (fd_in_set(fd, except, n_except))
                                continue;

                        if (close_nointr(fd) < 0)
                                if (errno != EBADF && r == 0)
                                        r = -errno;
                }

                return r;
        }

        while ((de = readdir(d))) {
                int fd = -1;

                if (hidden_file(de->d_name))
                        continue;

                if (safe_atoi(de->d_name, &fd) < 0)
                        /* Let's better ignore this, just in case */
                        continue;

                if (fd < 3)
                        continue;

                if (fd == dirfd(d))
                        continue;

                if (fd_in_set(fd, except, n_except))
                        continue;

                if (close_nointr(fd) < 0) {
                        /* Valgrind has its own FD and doesn't want to have it closed */
                        if (errno != EBADF && r == 0)
                                r = -errno;
                }
        }

        return r;
}

bool chars_intersect(const char *a, const char *b) {
        const char *p;

        /* Returns true if any of the chars in a are in b. */
        for (p = a; *p; p++)
                if (strchr(b, *p))
                        return true;

        return false;
}

bool fstype_is_network(const char *fstype) {
        static const char table[] =
                "afs\0"
                "cifs\0"
                "smbfs\0"
                "sshfs\0"
                "ncpfs\0"
                "ncp\0"
                "nfs\0"
                "nfs4\0"
                "gfs\0"
                "gfs2\0"
                "glusterfs\0";

        const char *x;

        x = startswith(fstype, "fuse.");
        if (x)
                fstype = x;

        return nulstr_contains(table, fstype);
}

int chvt(int vt) {
        _cleanup_close_ int fd;

        fd = open_terminal("/dev/tty0", O_RDWR|O_NOCTTY|O_CLOEXEC);
        if (fd < 0)
                return -errno;

        if (vt < 0) {
                int tiocl[2] = {
                        TIOCL_GETKMSGREDIRECT,
                        0
                };

                if (ioctl(fd, TIOCLINUX, tiocl) < 0)
                        return -errno;

                vt = tiocl[0] <= 0 ? 1 : tiocl[0];
        }

        if (ioctl(fd, VT_ACTIVATE, vt) < 0)
                return -errno;

        return 0;
}

int read_one_char(FILE *f, char *ret, usec_t t, bool *need_nl) {
        struct termios old_termios, new_termios;
        char c, line[LINE_MAX];

        assert(f);
        assert(ret);

        if (tcgetattr(fileno(f), &old_termios) >= 0) {
                new_termios = old_termios;

                new_termios.c_lflag &= ~ICANON;
                new_termios.c_cc[VMIN] = 1;
                new_termios.c_cc[VTIME] = 0;

                if (tcsetattr(fileno(f), TCSADRAIN, &new_termios) >= 0) {
                        size_t k;

                        if (t != USEC_INFINITY) {
                                if (fd_wait_for_event(fileno(f), POLLIN, t) <= 0) {
                                        tcsetattr(fileno(f), TCSADRAIN, &old_termios);
                                        return -ETIMEDOUT;
                                }
                        }

                        k = fread(&c, 1, 1, f);

                        tcsetattr(fileno(f), TCSADRAIN, &old_termios);

                        if (k <= 0)
                                return -EIO;

                        if (need_nl)
                                *need_nl = c != '\n';

                        *ret = c;
                        return 0;
                }
        }

        if (t != USEC_INFINITY) {
                if (fd_wait_for_event(fileno(f), POLLIN, t) <= 0)
                        return -ETIMEDOUT;
        }

        errno = 0;
        if (!fgets(line, sizeof(line), f))
                return errno ? -errno : -EIO;

        truncate_nl(line);

        if (strlen(line) != 1)
                return -EBADMSG;

        if (need_nl)
                *need_nl = false;

        *ret = line[0];
        return 0;
}

int ask_char(char *ret, const char *replies, const char *text, ...) {
        int r;

        assert(ret);
        assert(replies);
        assert(text);

        for (;;) {
                va_list ap;
                char c;
                bool need_nl = true;

                if (on_tty())
                        fputs(ANSI_HIGHLIGHT_ON, stdout);

                va_start(ap, text);
                vprintf(text, ap);
                va_end(ap);

                if (on_tty())
                        fputs(ANSI_HIGHLIGHT_OFF, stdout);

                fflush(stdout);

                r = read_one_char(stdin, &c, USEC_INFINITY, &need_nl);
                if (r < 0) {

                        if (r == -EBADMSG) {
                                puts("Bad input, please try again.");
                                continue;
                        }

                        putchar('\n');
                        return r;
                }

                if (need_nl)
                        putchar('\n');

                if (strchr(replies, c)) {
                        *ret = c;
                        return 0;
                }

                puts("Read unexpected character, please try again.");
        }
}

int ask_string(char **ret, const char *text, ...) {
        assert(ret);
        assert(text);

        for (;;) {
                char line[LINE_MAX];
                va_list ap;

                if (on_tty())
                        fputs(ANSI_HIGHLIGHT_ON, stdout);

                va_start(ap, text);
                vprintf(text, ap);
                va_end(ap);

                if (on_tty())
                        fputs(ANSI_HIGHLIGHT_OFF, stdout);

                fflush(stdout);

                errno = 0;
                if (!fgets(line, sizeof(line), stdin))
                        return errno ? -errno : -EIO;

                if (!endswith(line, "\n"))
                        putchar('\n');
                else {
                        char *s;

                        if (isempty(line))
                                continue;

                        truncate_nl(line);
                        s = strdup(line);
                        if (!s)
                                return -ENOMEM;

                        *ret = s;
                        return 0;
                }
        }
}

int reset_terminal_fd(int fd, bool switch_to_text) {
        struct termios termios;
        int r = 0;

        /* Set terminal to some sane defaults */

        assert(fd >= 0);

        /* We leave locked terminal attributes untouched, so that
         * Plymouth may set whatever it wants to set, and we don't
         * interfere with that. */

        /* Disable exclusive mode, just in case */
        ioctl(fd, TIOCNXCL);

        /* Switch to text mode */
        if (switch_to_text)
                ioctl(fd, KDSETMODE, KD_TEXT);

        /* Enable console unicode mode */
        ioctl(fd, KDSKBMODE, K_UNICODE);

        if (tcgetattr(fd, &termios) < 0) {
                r = -errno;
                goto finish;
        }

        /* We only reset the stuff that matters to the software. How
         * hardware is set up we don't touch assuming that somebody
         * else will do that for us */

        termios.c_iflag &= ~(IGNBRK | BRKINT | ISTRIP | INLCR | IGNCR | IUCLC);
        termios.c_iflag |= ICRNL | IMAXBEL | IUTF8;
        termios.c_oflag |= ONLCR;
        termios.c_cflag |= CREAD;
        termios.c_lflag = ISIG | ICANON | IEXTEN | ECHO | ECHOE | ECHOK | ECHOCTL | ECHOPRT | ECHOKE;

        termios.c_cc[VINTR]    =   03;  /* ^C */
        termios.c_cc[VQUIT]    =  034;  /* ^\ */
        termios.c_cc[VERASE]   = 0177;
        termios.c_cc[VKILL]    =  025;  /* ^X */
        termios.c_cc[VEOF]     =   04;  /* ^D */
        termios.c_cc[VSTART]   =  021;  /* ^Q */
        termios.c_cc[VSTOP]    =  023;  /* ^S */
        termios.c_cc[VSUSP]    =  032;  /* ^Z */
        termios.c_cc[VLNEXT]   =  026;  /* ^V */
        termios.c_cc[VWERASE]  =  027;  /* ^W */
        termios.c_cc[VREPRINT] =  022;  /* ^R */
        termios.c_cc[VEOL]     =    0;
        termios.c_cc[VEOL2]    =    0;

        termios.c_cc[VTIME]  = 0;
        termios.c_cc[VMIN]   = 1;

        if (tcsetattr(fd, TCSANOW, &termios) < 0)
                r = -errno;

finish:
        /* Just in case, flush all crap out */
        tcflush(fd, TCIOFLUSH);

        return r;
}

int reset_terminal(const char *name) {
        _cleanup_close_ int fd = -1;

        fd = open_terminal(name, O_RDWR|O_NOCTTY|O_CLOEXEC);
        if (fd < 0)
                return fd;

        return reset_terminal_fd(fd, true);
}

int open_terminal(const char *name, int mode) {
        int fd, r;
        unsigned c = 0;

        /*
         * If a TTY is in the process of being closed opening it might
         * cause EIO. This is horribly awful, but unlikely to be
         * changed in the kernel. Hence we work around this problem by
         * retrying a couple of times.
         *
         * https://bugs.launchpad.net/ubuntu/+source/linux/+bug/554172/comments/245
         */

        assert(!(mode & O_CREAT));

        for (;;) {
                fd = open(name, mode, 0);
                if (fd >= 0)
                        break;

                if (errno != EIO)
                        return -errno;

                /* Max 1s in total */
                if (c >= 20)
                        return -errno;

                usleep(50 * USEC_PER_MSEC);
                c++;
        }

        r = isatty(fd);
        if (r < 0) {
                safe_close(fd);
                return -errno;
        }

        if (!r) {
                safe_close(fd);
                return -ENOTTY;
        }

        return fd;
}

int flush_fd(int fd) {
        struct pollfd pollfd = {
                .fd = fd,
                .events = POLLIN,
        };

        for (;;) {
                char buf[LINE_MAX];
                ssize_t l;
                int r;

                r = poll(&pollfd, 1, 0);
                if (r < 0) {
                        if (errno == EINTR)
                                continue;

                        return -errno;

                } else if (r == 0)
                        return 0;

                l = read(fd, buf, sizeof(buf));
                if (l < 0) {

                        if (errno == EINTR)
                                continue;

                        if (errno == EAGAIN)
                                return 0;

                        return -errno;
                } else if (l == 0)
                        return 0;
        }
}

int acquire_terminal(
                const char *name,
                bool fail,
                bool force,
                bool ignore_tiocstty_eperm,
                usec_t timeout) {

        int fd = -1, notify = -1, r = 0, wd = -1;
        usec_t ts = 0;

        assert(name);

        /* We use inotify to be notified when the tty is closed. We
         * create the watch before checking if we can actually acquire
         * it, so that we don't lose any event.
         *
         * Note: strictly speaking this actually watches for the
         * device being closed, it does *not* really watch whether a
         * tty loses its controlling process. However, unless some
         * rogue process uses TIOCNOTTY on /dev/tty *after* closing
         * its tty otherwise this will not become a problem. As long
         * as the administrator makes sure not configure any service
         * on the same tty as an untrusted user this should not be a
         * problem. (Which he probably should not do anyway.) */

        if (timeout != USEC_INFINITY)
                ts = now(CLOCK_MONOTONIC);

        if (!fail && !force) {
                notify = inotify_init1(IN_CLOEXEC | (timeout != USEC_INFINITY ? IN_NONBLOCK : 0));
                if (notify < 0) {
                        r = -errno;
                        goto fail;
                }

                wd = inotify_add_watch(notify, name, IN_CLOSE);
                if (wd < 0) {
                        r = -errno;
                        goto fail;
                }
        }

        for (;;) {
                struct sigaction sa_old, sa_new = {
                        .sa_handler = SIG_IGN,
                        .sa_flags = SA_RESTART,
                };

                if (notify >= 0) {
                        r = flush_fd(notify);
                        if (r < 0)
                                goto fail;
                }

                /* We pass here O_NOCTTY only so that we can check the return
                 * value TIOCSCTTY and have a reliable way to figure out if we
                 * successfully became the controlling process of the tty */
                fd = open_terminal(name, O_RDWR|O_NOCTTY|O_CLOEXEC);
                if (fd < 0)
                        return fd;

                /* Temporarily ignore SIGHUP, so that we don't get SIGHUP'ed
                 * if we already own the tty. */
                assert_se(sigaction(SIGHUP, &sa_new, &sa_old) == 0);

                /* First, try to get the tty */
                if (ioctl(fd, TIOCSCTTY, force) < 0)
                        r = -errno;

                assert_se(sigaction(SIGHUP, &sa_old, NULL) == 0);

                /* Sometimes it makes sense to ignore TIOCSCTTY
                 * returning EPERM, i.e. when very likely we already
                 * are have this controlling terminal. */
                if (r < 0 && r == -EPERM && ignore_tiocstty_eperm)
                        r = 0;

                if (r < 0 && (force || fail || r != -EPERM)) {
                        goto fail;
                }

                if (r >= 0)
                        break;

                assert(!fail);
                assert(!force);
                assert(notify >= 0);

                for (;;) {
                        union inotify_event_buffer buffer;
                        struct inotify_event *e;
                        ssize_t l;

                        if (timeout != USEC_INFINITY) {
                                usec_t n;

                                n = now(CLOCK_MONOTONIC);
                                if (ts + timeout < n) {
                                        r = -ETIMEDOUT;
                                        goto fail;
                                }

                                r = fd_wait_for_event(fd, POLLIN, ts + timeout - n);
                                if (r < 0)
                                        goto fail;

                                if (r == 0) {
                                        r = -ETIMEDOUT;
                                        goto fail;
                                }
                        }

                        l = read(notify, &buffer, sizeof(buffer));
                        if (l < 0) {
                                if (errno == EINTR || errno == EAGAIN)
                                        continue;

                                r = -errno;
                                goto fail;
                        }

                        FOREACH_INOTIFY_EVENT(e, buffer, l) {
                                if (e->wd != wd || !(e->mask & IN_CLOSE)) {
                                        r = -EIO;
                                        goto fail;
                                }
                        }

                        break;
                }

                /* We close the tty fd here since if the old session
                 * ended our handle will be dead. It's important that
                 * we do this after sleeping, so that we don't enter
                 * an endless loop. */
                fd = safe_close(fd);
        }

        safe_close(notify);

        r = reset_terminal_fd(fd, true);
        if (r < 0)
                log_warning_errno(r, "Failed to reset terminal: %m");

        return fd;

fail:
        safe_close(fd);
        safe_close(notify);

        return r;
}

int release_terminal(void) {
        static const struct sigaction sa_new = {
                .sa_handler = SIG_IGN,
                .sa_flags = SA_RESTART,
        };

        _cleanup_close_ int fd = -1;
        struct sigaction sa_old;
        int r = 0;

        fd = open("/dev/tty", O_RDWR|O_NOCTTY|O_NDELAY|O_CLOEXEC);
        if (fd < 0)
                return -errno;

        /* Temporarily ignore SIGHUP, so that we don't get SIGHUP'ed
         * by our own TIOCNOTTY */
        assert_se(sigaction(SIGHUP, &sa_new, &sa_old) == 0);

        if (ioctl(fd, TIOCNOTTY) < 0)
                r = -errno;

        assert_se(sigaction(SIGHUP, &sa_old, NULL) == 0);

        return r;
}

int sigaction_many(const struct sigaction *sa, ...) {
        va_list ap;
        int r = 0, sig;

        va_start(ap, sa);
        while ((sig = va_arg(ap, int)) > 0)
                if (sigaction(sig, sa, NULL) < 0)
                        r = -errno;
        va_end(ap);

        return r;
}

int ignore_signals(int sig, ...) {
        struct sigaction sa = {
                .sa_handler = SIG_IGN,
                .sa_flags = SA_RESTART,
        };
        va_list ap;
        int r = 0;

        if (sigaction(sig, &sa, NULL) < 0)
                r = -errno;

        va_start(ap, sig);
        while ((sig = va_arg(ap, int)) > 0)
                if (sigaction(sig, &sa, NULL) < 0)
                        r = -errno;
        va_end(ap);

        return r;
}

int default_signals(int sig, ...) {
        struct sigaction sa = {
                .sa_handler = SIG_DFL,
                .sa_flags = SA_RESTART,
        };
        va_list ap;
        int r = 0;

        if (sigaction(sig, &sa, NULL) < 0)
                r = -errno;

        va_start(ap, sig);
        while ((sig = va_arg(ap, int)) > 0)
                if (sigaction(sig, &sa, NULL) < 0)
                        r = -errno;
        va_end(ap);

        return r;
}

void safe_close_pair(int p[]) {
        assert(p);

        if (p[0] == p[1]) {
                /* Special case pairs which use the same fd in both
                 * directions... */
                p[0] = p[1] = safe_close(p[0]);
                return;
        }

        p[0] = safe_close(p[0]);
        p[1] = safe_close(p[1]);
}

ssize_t loop_read(int fd, void *buf, size_t nbytes, bool do_poll) {
        uint8_t *p = buf;
        ssize_t n = 0;

        assert(fd >= 0);
        assert(buf);

        while (nbytes > 0) {
                ssize_t k;

                k = read(fd, p, nbytes);
                if (k < 0) {
                        if (errno == EINTR)
                                continue;

                        if (errno == EAGAIN && do_poll) {

                                /* We knowingly ignore any return value here,
                                 * and expect that any error/EOF is reported
                                 * via read() */

                                fd_wait_for_event(fd, POLLIN, USEC_INFINITY);
                                continue;
                        }

                        return n > 0 ? n : -errno;
                }

                if (k == 0)
                        return n;

                p += k;
                nbytes -= k;
                n += k;
        }

        return n;
}

int loop_read_exact(int fd, void *buf, size_t nbytes, bool do_poll) {
        ssize_t n;

        n = loop_read(fd, buf, nbytes, do_poll);
        if (n < 0)
                return n;
        if ((size_t) n != nbytes)
                return -EIO;
        return 0;
}

int loop_write(int fd, const void *buf, size_t nbytes, bool do_poll) {
        const uint8_t *p = buf;

        assert(fd >= 0);
        assert(buf);

        errno = 0;

        while (nbytes > 0) {
                ssize_t k;

                k = write(fd, p, nbytes);
                if (k < 0) {
                        if (errno == EINTR)
                                continue;

                        if (errno == EAGAIN && do_poll) {
                                /* We knowingly ignore any return value here,
                                 * and expect that any error/EOF is reported
                                 * via write() */

                                fd_wait_for_event(fd, POLLOUT, USEC_INFINITY);
                                continue;
                        }

                        return -errno;
                }

                if (k == 0) /* Can't really happen */
                        return -EIO;

                p += k;
                nbytes -= k;
        }

        return 0;
}

int parse_size(const char *t, off_t base, off_t *size) {

        /* Soo, sometimes we want to parse IEC binary suffxies, and
         * sometimes SI decimal suffixes. This function can parse
         * both. Which one is the right way depends on the
         * context. Wikipedia suggests that SI is customary for
         * hardrware metrics and network speeds, while IEC is
         * customary for most data sizes used by software and volatile
         * (RAM) memory. Hence be careful which one you pick!
         *
         * In either case we use just K, M, G as suffix, and not Ki,
         * Mi, Gi or so (as IEC would suggest). That's because that's
         * frickin' ugly. But this means you really need to make sure
         * to document which base you are parsing when you use this
         * call. */

        struct table {
                const char *suffix;
                unsigned long long factor;
        };

        static const struct table iec[] = {
                { "E", 1024ULL*1024ULL*1024ULL*1024ULL*1024ULL*1024ULL },
                { "P", 1024ULL*1024ULL*1024ULL*1024ULL*1024ULL },
                { "T", 1024ULL*1024ULL*1024ULL*1024ULL },
                { "G", 1024ULL*1024ULL*1024ULL },
                { "M", 1024ULL*1024ULL },
                { "K", 1024ULL },
                { "B", 1 },
                { "", 1 },
        };

        static const struct table si[] = {
                { "E", 1000ULL*1000ULL*1000ULL*1000ULL*1000ULL*1000ULL },
                { "P", 1000ULL*1000ULL*1000ULL*1000ULL*1000ULL },
                { "T", 1000ULL*1000ULL*1000ULL*1000ULL },
                { "G", 1000ULL*1000ULL*1000ULL },
                { "M", 1000ULL*1000ULL },
                { "K", 1000ULL },
                { "B", 1 },
                { "", 1 },
        };

        const struct table *table;
        const char *p;
        unsigned long long r = 0;
        unsigned n_entries, start_pos = 0;

        assert(t);
        assert(base == 1000 || base == 1024);
        assert(size);

        if (base == 1000) {
                table = si;
                n_entries = ELEMENTSOF(si);
        } else {
                table = iec;
                n_entries = ELEMENTSOF(iec);
        }

        p = t;
        do {
                long long l;
                unsigned long long l2;
                double frac = 0;
                char *e;
                unsigned i;

                errno = 0;
                l = strtoll(p, &e, 10);

                if (errno > 0)
                        return -errno;

                if (l < 0)
                        return -ERANGE;

                if (e == p)
                        return -EINVAL;

                if (*e == '.') {
                        e++;
                        if (*e >= '0' && *e <= '9') {
                                char *e2;

                                /* strotoull itself would accept space/+/- */
                                l2 = strtoull(e, &e2, 10);

                                if (errno == ERANGE)
                                        return -errno;

                                /* Ignore failure. E.g. 10.M is valid */
                                frac = l2;
                                for (; e < e2; e++)
                                        frac /= 10;
                        }
                }

                e += strspn(e, WHITESPACE);

                for (i = start_pos; i < n_entries; i++)
                        if (startswith(e, table[i].suffix)) {
                                unsigned long long tmp;
                                if ((unsigned long long) l + (frac > 0) > ULLONG_MAX / table[i].factor)
                                        return -ERANGE;
                                tmp = l * table[i].factor + (unsigned long long) (frac * table[i].factor);
                                if (tmp > ULLONG_MAX - r)
                                        return -ERANGE;

                                r += tmp;
                                if ((unsigned long long) (off_t) r != r)
                                        return -ERANGE;

                                p = e + strlen(table[i].suffix);

                                start_pos = i + 1;
                                break;
                        }

                if (i >= n_entries)
                        return -EINVAL;

        } while (*p);

        *size = r;

        return 0;
}

int make_stdio(int fd) {
        int r, s, t;

        assert(fd >= 0);

        r = dup2(fd, STDIN_FILENO);
        s = dup2(fd, STDOUT_FILENO);
        t = dup2(fd, STDERR_FILENO);

        if (fd >= 3)
                safe_close(fd);

        if (r < 0 || s < 0 || t < 0)
                return -errno;

        /* Explicitly unset O_CLOEXEC, since if fd was < 3, then
         * dup2() was a NOP and the bit hence possibly set. */
        fd_cloexec(STDIN_FILENO, false);
        fd_cloexec(STDOUT_FILENO, false);
        fd_cloexec(STDERR_FILENO, false);

        return 0;
}

int make_null_stdio(void) {
        int null_fd;

        null_fd = open("/dev/null", O_RDWR|O_NOCTTY);
        if (null_fd < 0)
                return -errno;

        return make_stdio(null_fd);
}

bool is_device_path(const char *path) {

        /* Returns true on paths that refer to a device, either in
         * sysfs or in /dev */

        return
                path_startswith(path, "/dev/") ||
                path_startswith(path, "/sys/");
}

int dir_is_empty(const char *path) {
        _cleanup_closedir_ DIR *d;

        d = opendir(path);
        if (!d)
                return -errno;

        for (;;) {
                struct dirent *de;

                errno = 0;
                de = readdir(d);
                if (!de && errno != 0)
                        return -errno;

                if (!de)
                        return 1;

                if (!hidden_file(de->d_name))
                        return 0;
        }
}

char* dirname_malloc(const char *path) {
        char *d, *dir, *dir2;

        d = strdup(path);
        if (!d)
                return NULL;
        dir = dirname(d);
        assert(dir);

        if (dir != d) {
                dir2 = strdup(dir);
                free(d);
                return dir2;
        }

        return dir;
}

int dev_urandom(void *p, size_t n) {
        static int have_syscall = -1;

        _cleanup_close_ int fd = -1;
        int r;

        /* Gathers some randomness from the kernel. This call will
         * never block, and will always return some data from the
         * kernel, regardless if the random pool is fully initialized
         * or not. It thus makes no guarantee for the quality of the
         * returned entropy, but is good enough for or usual usecases
         * of seeding the hash functions for hashtable */

        /* Use the getrandom() syscall unless we know we don't have
         * it, or when the requested size is too large for it. */
        if (have_syscall != 0 || (size_t) (int) n != n) {
                r = getrandom(p, n, GRND_NONBLOCK);
                if (r == (int) n) {
                        have_syscall = true;
                        return 0;
                }

                if (r < 0) {
                        if (errno == ENOSYS)
                                /* we lack the syscall, continue with
                                 * reading from /dev/urandom */
                                have_syscall = false;
                        else if (errno == EAGAIN)
                                /* not enough entropy for now. Let's
                                 * remember to use the syscall the
                                 * next time, again, but also read
                                 * from /dev/urandom for now, which
                                 * doesn't care about the current
                                 * amount of entropy.  */
                                have_syscall = true;
                        else
                                return -errno;
                } else
                        /* too short read? */
                        return -ENODATA;
        }

        fd = open("/dev/urandom", O_RDONLY|O_CLOEXEC|O_NOCTTY);
        if (fd < 0)
                return errno == ENOENT ? -ENOSYS : -errno;

        return loop_read_exact(fd, p, n, true);
}

void initialize_srand(void) {
        static bool srand_called = false;
        unsigned x;
#ifdef HAVE_SYS_AUXV_H
        void *auxv;
#endif

        if (srand_called)
                return;

        x = 0;

#ifdef HAVE_SYS_AUXV_H
        /* The kernel provides us with a bit of entropy in auxv, so
         * let's try to make use of that to seed the pseudo-random
         * generator. It's better than nothing... */

        auxv = (void*) getauxval(AT_RANDOM);
        if (auxv)
                x ^= *(unsigned*) auxv;
#endif

        x ^= (unsigned) now(CLOCK_REALTIME);
        x ^= (unsigned) gettid();

        srand(x);
        srand_called = true;
}

void random_bytes(void *p, size_t n) {
        uint8_t *q;
        int r;

        r = dev_urandom(p, n);
        if (r >= 0)
                return;

        /* If some idiot made /dev/urandom unavailable to us, he'll
         * get a PRNG instead. */

        initialize_srand();

        for (q = p; q < (uint8_t*) p + n; q ++)
                *q = rand();
}

void rename_process(const char name[8]) {
        assert(name);

        /* This is a like a poor man's setproctitle(). It changes the
         * comm field, argv[0], and also the glibc's internally used
         * name of the process. For the first one a limit of 16 chars
         * applies, to the second one usually one of 10 (i.e. length
         * of "/sbin/init"), to the third one one of 7 (i.e. length of
         * "systemd"). If you pass a longer string it will be
         * truncated */

        prctl(PR_SET_NAME, name);

        if (program_invocation_name)
                strncpy(program_invocation_name, name, strlen(program_invocation_name));

        if (saved_argc > 0) {
                int i;

                if (saved_argv[0])
                        strncpy(saved_argv[0], name, strlen(saved_argv[0]));

                for (i = 1; i < saved_argc; i++) {
                        if (!saved_argv[i])
                                break;

                        memzero(saved_argv[i], strlen(saved_argv[i]));
                }
        }
}

void sigset_add_many(sigset_t *ss, ...) {
        va_list ap;
        int sig;

        assert(ss);

        va_start(ap, ss);
        while ((sig = va_arg(ap, int)) > 0)
                assert_se(sigaddset(ss, sig) == 0);
        va_end(ap);
}

int sigprocmask_many(int how, ...) {
        va_list ap;
        sigset_t ss;
        int sig;

        assert_se(sigemptyset(&ss) == 0);

        va_start(ap, how);
        while ((sig = va_arg(ap, int)) > 0)
                assert_se(sigaddset(&ss, sig) == 0);
        va_end(ap);

        if (sigprocmask(how, &ss, NULL) < 0)
                return -errno;

        return 0;
}

char* gethostname_malloc(void) {
        struct utsname u;

        assert_se(uname(&u) >= 0);

        if (!isempty(u.nodename) && !streq(u.nodename, "(none)"))
                return strdup(u.nodename);

        return strdup(u.sysname);
}

bool hostname_is_set(void) {
        struct utsname u;

        assert_se(uname(&u) >= 0);

        return !isempty(u.nodename) && !streq(u.nodename, "(none)");
}

char *lookup_uid(uid_t uid) {
        long bufsize;
        char *name;
        _cleanup_free_ char *buf = NULL;
        struct passwd pwbuf, *pw = NULL;

        /* Shortcut things to avoid NSS lookups */
        if (uid == 0)
                return strdup("root");

        bufsize = sysconf(_SC_GETPW_R_SIZE_MAX);
        if (bufsize <= 0)
                bufsize = 4096;

        buf = malloc(bufsize);
        if (!buf)
                return NULL;

        if (getpwuid_r(uid, &pwbuf, buf, bufsize, &pw) == 0 && pw)
                return strdup(pw->pw_name);

        if (asprintf(&name, UID_FMT, uid) < 0)
                return NULL;

        return name;
}

char* getlogname_malloc(void) {
        uid_t uid;
        struct stat st;

        if (isatty(STDIN_FILENO) && fstat(STDIN_FILENO, &st) >= 0)
                uid = st.st_uid;
        else
                uid = getuid();

        return lookup_uid(uid);
}

char *getusername_malloc(void) {
        const char *e;

        e = getenv("USER");
        if (e)
                return strdup(e);

        return lookup_uid(getuid());
}

int getttyname_malloc(int fd, char **ret) {
        size_t l = 100;
        int r;

        assert(fd >= 0);
        assert(ret);

        for (;;) {
                char path[l];

                r = ttyname_r(fd, path, sizeof(path));
                if (r == 0) {
                        const char *p;
                        char *c;

                        p = startswith(path, "/dev/");
                        c = strdup(p ?: path);
                        if (!c)
                                return -ENOMEM;

                        *ret = c;
                        return 0;
                }

                if (r != ERANGE)
                        return -r;

                l *= 2;
        }

        return 0;
}

int getttyname_harder(int fd, char **r) {
        int k;
        char *s = NULL;

        k = getttyname_malloc(fd, &s);
        if (k < 0)
                return k;

        if (streq(s, "tty")) {
                free(s);
                return get_ctty(0, NULL, r);
        }

        *r = s;
        return 0;
}

int get_ctty_devnr(pid_t pid, dev_t *d) {
        int r;
        _cleanup_free_ char *line = NULL;
        const char *p;
        unsigned long ttynr;

        assert(pid >= 0);

        p = procfs_file_alloca(pid, "stat");
        r = read_one_line_file(p, &line);
        if (r < 0)
                return r;

        p = strrchr(line, ')');
        if (!p)
                return -EIO;

        p++;

        if (sscanf(p, " "
                   "%*c "  /* state */
                   "%*d "  /* ppid */
                   "%*d "  /* pgrp */
                   "%*d "  /* session */
                   "%lu ", /* ttynr */
                   &ttynr) != 1)
                return -EIO;

        if (major(ttynr) == 0 && minor(ttynr) == 0)
                return -ENOENT;

        if (d)
                *d = (dev_t) ttynr;

        return 0;
}

int get_ctty(pid_t pid, dev_t *_devnr, char **r) {
        char fn[sizeof("/dev/char/")-1 + 2*DECIMAL_STR_MAX(unsigned) + 1 + 1], *b = NULL;
        _cleanup_free_ char *s = NULL;
        const char *p;
        dev_t devnr;
        int k;

        assert(r);

        k = get_ctty_devnr(pid, &devnr);
        if (k < 0)
                return k;

        sprintf(fn, "/dev/char/%u:%u", major(devnr), minor(devnr));

        k = readlink_malloc(fn, &s);
        if (k < 0) {

                if (k != -ENOENT)
                        return k;

                /* This is an ugly hack */
                if (major(devnr) == 136) {
                        if (asprintf(&b, "pts/%u", minor(devnr)) < 0)
                                return -ENOMEM;
                } else {
                        /* Probably something like the ptys which have no
                         * symlink in /dev/char. Let's return something
                         * vaguely useful. */

                        b = strdup(fn + 5);
                        if (!b)
                                return -ENOMEM;
                }
        } else {
                if (startswith(s, "/dev/"))
                        p = s + 5;
                else if (startswith(s, "../"))
                        p = s + 3;
                else
                        p = s;

                b = strdup(p);
                if (!b)
                        return -ENOMEM;
        }

        *r = b;
        if (_devnr)
                *_devnr = devnr;

        return 0;
}

int rm_rf_children_dangerous(int fd, bool only_dirs, bool honour_sticky, struct stat *root_dev) {
        _cleanup_closedir_ DIR *d = NULL;
        int ret = 0;

        assert(fd >= 0);

        /* This returns the first error we run into, but nevertheless
         * tries to go on. This closes the passed fd. */

        d = fdopendir(fd);
        if (!d) {
                safe_close(fd);

                return errno == ENOENT ? 0 : -errno;
        }

        for (;;) {
                struct dirent *de;
                bool is_dir, keep_around;
                struct stat st;
                int r;

                errno = 0;
                de = readdir(d);
                if (!de) {
                        if (errno != 0 && ret == 0)
                                ret = -errno;
                        return ret;
                }

                if (streq(de->d_name, ".") || streq(de->d_name, ".."))
                        continue;

                if (de->d_type == DT_UNKNOWN ||
                    honour_sticky ||
                    (de->d_type == DT_DIR && root_dev)) {
                        if (fstatat(fd, de->d_name, &st, AT_SYMLINK_NOFOLLOW) < 0) {
                                if (ret == 0 && errno != ENOENT)
                                        ret = -errno;
                                continue;
                        }

                        is_dir = S_ISDIR(st.st_mode);
                        keep_around =
                                honour_sticky &&
                                (st.st_uid == 0 || st.st_uid == getuid()) &&
                                (st.st_mode & S_ISVTX);
                } else {
                        is_dir = de->d_type == DT_DIR;
                        keep_around = false;
                }

                if (is_dir) {
                        int subdir_fd;

                        /* if root_dev is set, remove subdirectories only, if device is same as dir */
                        if (root_dev && st.st_dev != root_dev->st_dev)
                                continue;

                        subdir_fd = openat(fd, de->d_name,
                                           O_RDONLY|O_NONBLOCK|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW|O_NOATIME);
                        if (subdir_fd < 0) {
                                if (ret == 0 && errno != ENOENT)
                                        ret = -errno;
                                continue;
                        }

                        r = rm_rf_children_dangerous(subdir_fd, only_dirs, honour_sticky, root_dev);
                        if (r < 0 && ret == 0)
                                ret = r;

                        if (!keep_around)
                                if (unlinkat(fd, de->d_name, AT_REMOVEDIR) < 0) {
                                        if (ret == 0 && errno != ENOENT)
                                                ret = -errno;
                                }

                } else if (!only_dirs && !keep_around) {

                        if (unlinkat(fd, de->d_name, 0) < 0) {
                                if (ret == 0 && errno != ENOENT)
                                        ret = -errno;
                        }
                }
        }
}

_pure_ static int is_temporary_fs(struct statfs *s) {
        assert(s);

        return F_TYPE_EQUAL(s->f_type, TMPFS_MAGIC) ||
               F_TYPE_EQUAL(s->f_type, RAMFS_MAGIC);
}

int is_fd_on_temporary_fs(int fd) {
        struct statfs s;

        if (fstatfs(fd, &s) < 0)
                return -errno;

        return is_temporary_fs(&s);
}

int rm_rf_children(int fd, bool only_dirs, bool honour_sticky, struct stat *root_dev) {
        struct statfs s;

        assert(fd >= 0);

        if (fstatfs(fd, &s) < 0) {
                safe_close(fd);
                return -errno;
        }

        /* We refuse to clean disk file systems with this call. This
         * is extra paranoia just to be sure we never ever remove
         * non-state data */
        if (!is_temporary_fs(&s)) {
                log_error("Attempted to remove disk file system, and we can't allow that.");
                safe_close(fd);
                return -EPERM;
        }

        return rm_rf_children_dangerous(fd, only_dirs, honour_sticky, root_dev);
}

static int file_is_priv_sticky(const char *p) {
        struct stat st;

        assert(p);

        if (lstat(p, &st) < 0)
                return -errno;

        return
                (st.st_uid == 0 || st.st_uid == getuid()) &&
                (st.st_mode & S_ISVTX);
}

static int rm_rf_internal(const char *path, bool only_dirs, bool delete_root, bool honour_sticky, bool dangerous) {
        int fd, r;
        struct statfs s;

        assert(path);

        /* We refuse to clean the root file system with this
         * call. This is extra paranoia to never cause a really
         * seriously broken system. */
        if (path_equal(path, "/")) {
                log_error("Attempted to remove entire root file system, and we can't allow that.");
                return -EPERM;
        }

        fd = open(path, O_RDONLY|O_NONBLOCK|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW|O_NOATIME);
        if (fd < 0) {

                if (errno != ENOTDIR && errno != ELOOP)
                        return -errno;

                if (!dangerous) {
                        if (statfs(path, &s) < 0)
                                return -errno;

                        if (!is_temporary_fs(&s)) {
                                log_error("Attempted to remove disk file system, and we can't allow that.");
                                return -EPERM;
                        }
                }

                if (delete_root && !only_dirs)
                        if (unlink(path) < 0 && errno != ENOENT)
                                return -errno;

                return 0;
        }

        if (!dangerous) {
                if (fstatfs(fd, &s) < 0) {
                        safe_close(fd);
                        return -errno;
                }

                if (!is_temporary_fs(&s)) {
                        log_error("Attempted to remove disk file system, and we can't allow that.");
                        safe_close(fd);
                        return -EPERM;
                }
        }

        r = rm_rf_children_dangerous(fd, only_dirs, honour_sticky, NULL);
        if (delete_root) {

                if (honour_sticky && file_is_priv_sticky(path) > 0)
                        return r;

                if (rmdir(path) < 0 && errno != ENOENT) {
                        if (r == 0)
                                r = -errno;
                }
        }

        return r;
}

int rm_rf(const char *path, bool only_dirs, bool delete_root, bool honour_sticky) {
        return rm_rf_internal(path, only_dirs, delete_root, honour_sticky, false);
}

int rm_rf_dangerous(const char *path, bool only_dirs, bool delete_root, bool honour_sticky) {
        return rm_rf_internal(path, only_dirs, delete_root, honour_sticky, true);
}

int chmod_and_chown(const char *path, mode_t mode, uid_t uid, gid_t gid) {
        assert(path);

        /* Under the assumption that we are running privileged we
         * first change the access mode and only then hand out
         * ownership to avoid a window where access is too open. */

        if (mode != MODE_INVALID)
                if (chmod(path, mode) < 0)
                        return -errno;

        if (uid != UID_INVALID || gid != GID_INVALID)
                if (chown(path, uid, gid) < 0)
                        return -errno;

        return 0;
}

int fchmod_and_fchown(int fd, mode_t mode, uid_t uid, gid_t gid) {
        assert(fd >= 0);

        /* Under the assumption that we are running privileged we
         * first change the access mode and only then hand out
         * ownership to avoid a window where access is too open. */

        if (mode != MODE_INVALID)
                if (fchmod(fd, mode) < 0)
                        return -errno;

        if (uid != UID_INVALID || gid != GID_INVALID)
                if (fchown(fd, uid, gid) < 0)
                        return -errno;

        return 0;
}

cpu_set_t* cpu_set_malloc(unsigned *ncpus) {
        cpu_set_t *r;
        unsigned n = 1024;

        /* Allocates the cpuset in the right size */

        for (;;) {
                if (!(r = CPU_ALLOC(n)))
                        return NULL;

                if (sched_getaffinity(0, CPU_ALLOC_SIZE(n), r) >= 0) {
                        CPU_ZERO_S(CPU_ALLOC_SIZE(n), r);

                        if (ncpus)
                                *ncpus = n;

                        return r;
                }

                CPU_FREE(r);

                if (errno != EINVAL)
                        return NULL;

                n *= 2;
        }
}

int status_vprintf(const char *status, bool ellipse, bool ephemeral, const char *format, va_list ap) {
        static const char status_indent[] = "         "; /* "[" STATUS "] " */
        _cleanup_free_ char *s = NULL;
        _cleanup_close_ int fd = -1;
        struct iovec iovec[6] = {};
        int n = 0;
        static bool prev_ephemeral;

        assert(format);

        /* This is independent of logging, as status messages are
         * optional and go exclusively to the console. */

        if (vasprintf(&s, format, ap) < 0)
                return log_oom();

        fd = open_terminal("/dev/console", O_WRONLY|O_NOCTTY|O_CLOEXEC);
        if (fd < 0)
                return fd;

        if (ellipse) {
                char *e;
                size_t emax, sl;
                int c;

                c = fd_columns(fd);
                if (c <= 0)
                        c = 80;

                sl = status ? sizeof(status_indent)-1 : 0;

                emax = c - sl - 1;
                if (emax < 3)
                        emax = 3;

                e = ellipsize(s, emax, 50);
                if (e) {
                        free(s);
                        s = e;
                }
        }

        if (prev_ephemeral)
                IOVEC_SET_STRING(iovec[n++], "\r" ANSI_ERASE_TO_END_OF_LINE);
        prev_ephemeral = ephemeral;

        if (status) {
                if (!isempty(status)) {
                        IOVEC_SET_STRING(iovec[n++], "[");
                        IOVEC_SET_STRING(iovec[n++], status);
                        IOVEC_SET_STRING(iovec[n++], "] ");
                } else
                        IOVEC_SET_STRING(iovec[n++], status_indent);
        }

        IOVEC_SET_STRING(iovec[n++], s);
        if (!ephemeral)
                IOVEC_SET_STRING(iovec[n++], "\n");

        if (writev(fd, iovec, n) < 0)
                return -errno;

        return 0;
}

int status_printf(const char *status, bool ellipse, bool ephemeral, const char *format, ...) {
        va_list ap;
        int r;

        assert(format);

        va_start(ap, format);
        r = status_vprintf(status, ellipse, ephemeral, format, ap);
        va_end(ap);

        return r;
}

char *replace_env(const char *format, char **env) {
        enum {
                WORD,
                CURLY,
                VARIABLE
        } state = WORD;

        const char *e, *word = format;
        char *r = NULL, *k;

        assert(format);

        for (e = format; *e; e ++) {

                switch (state) {

                case WORD:
                        if (*e == '$')
                                state = CURLY;
                        break;

                case CURLY:
                        if (*e == '{') {
                                k = strnappend(r, word, e-word-1);
                                if (!k)
                                        goto fail;

                                free(r);
                                r = k;

                                word = e-1;
                                state = VARIABLE;

                        } else if (*e == '$') {
                                k = strnappend(r, word, e-word);
                                if (!k)
                                        goto fail;

                                free(r);
                                r = k;

                                word = e+1;
                                state = WORD;
                        } else
                                state = WORD;
                        break;

                case VARIABLE:
                        if (*e == '}') {
                                const char *t;

                                t = strempty(strv_env_get_n(env, word+2, e-word-2));

                                k = strappend(r, t);
                                if (!k)
                                        goto fail;

                                free(r);
                                r = k;

                                word = e+1;
                                state = WORD;
                        }
                        break;
                }
        }

        k = strnappend(r, word, e-word);
        if (!k)
                goto fail;

        free(r);
        return k;

fail:
        free(r);
        return NULL;
}

char **replace_env_argv(char **argv, char **env) {
        char **ret, **i;
        unsigned k = 0, l = 0;

        l = strv_length(argv);

        ret = new(char*, l+1);
        if (!ret)
                return NULL;

        STRV_FOREACH(i, argv) {

                /* If $FOO appears as single word, replace it by the split up variable */
                if ((*i)[0] == '$' && (*i)[1] != '{') {
                        char *e;
                        char **w, **m = NULL;
                        unsigned q;

                        e = strv_env_get(env, *i+1);
                        if (e) {
                                int r;

                                r = strv_split_quoted(&m, e, UNQUOTE_RELAX);
                                if (r < 0) {
                                        ret[k] = NULL;
                                        strv_free(ret);
                                        return NULL;
                                }
                        } else
                                m = NULL;

                        q = strv_length(m);
                        l = l + q - 1;

                        w = realloc(ret, sizeof(char*) * (l+1));
                        if (!w) {
                                ret[k] = NULL;
                                strv_free(ret);
                                strv_free(m);
                                return NULL;
                        }

                        ret = w;
                        if (m) {
                                memcpy(ret + k, m, q * sizeof(char*));
                                free(m);
                        }

                        k += q;
                        continue;
                }

                /* If ${FOO} appears as part of a word, replace it by the variable as-is */
                ret[k] = replace_env(*i, env);
                if (!ret[k]) {
                        strv_free(ret);
                        return NULL;
                }
                k++;
        }

        ret[k] = NULL;
        return ret;
}

int fd_columns(int fd) {
        struct winsize ws = {};

        if (ioctl(fd, TIOCGWINSZ, &ws) < 0)
                return -errno;

        if (ws.ws_col <= 0)
                return -EIO;

        return ws.ws_col;
}

unsigned columns(void) {
        const char *e;
        int c;

        if (_likely_(cached_columns > 0))
                return cached_columns;

        c = 0;
        e = getenv("COLUMNS");
        if (e)
                (void) safe_atoi(e, &c);

        if (c <= 0)
                c = fd_columns(STDOUT_FILENO);

        if (c <= 0)
                c = 80;

        cached_columns = c;
        return cached_columns;
}

int fd_lines(int fd) {
        struct winsize ws = {};

        if (ioctl(fd, TIOCGWINSZ, &ws) < 0)
                return -errno;

        if (ws.ws_row <= 0)
                return -EIO;

        return ws.ws_row;
}

unsigned lines(void) {
        const char *e;
        int l;

        if (_likely_(cached_lines > 0))
                return cached_lines;

        l = 0;
        e = getenv("LINES");
        if (e)
                (void) safe_atoi(e, &l);

        if (l <= 0)
                l = fd_lines(STDOUT_FILENO);

        if (l <= 0)
                l = 24;

        cached_lines = l;
        return cached_lines;
}

/* intended to be used as a SIGWINCH sighandler */
void columns_lines_cache_reset(int signum) {
        cached_columns = 0;
        cached_lines = 0;
}

bool on_tty(void) {
        static int cached_on_tty = -1;

        if (_unlikely_(cached_on_tty < 0))
                cached_on_tty = isatty(STDOUT_FILENO) > 0;

        return cached_on_tty;
}

int files_same(const char *filea, const char *fileb) {
        struct stat a, b;

        if (stat(filea, &a) < 0)
                return -errno;

        if (stat(fileb, &b) < 0)
                return -errno;

        return a.st_dev == b.st_dev &&
               a.st_ino == b.st_ino;
}

int running_in_chroot(void) {
        int ret;

        ret = files_same("/proc/1/root", "/");
        if (ret < 0)
                return ret;

        return ret == 0;
}

static char *ascii_ellipsize_mem(const char *s, size_t old_length, size_t new_length, unsigned percent) {
        size_t x;
        char *r;

        assert(s);
        assert(percent <= 100);
        assert(new_length >= 3);

        if (old_length <= 3 || old_length <= new_length)
                return strndup(s, old_length);

        r = new0(char, new_length+1);
        if (!r)
                return NULL;

        x = (new_length * percent) / 100;

        if (x > new_length - 3)
                x = new_length - 3;

        memcpy(r, s, x);
        r[x] = '.';
        r[x+1] = '.';
        r[x+2] = '.';
        memcpy(r + x + 3,
               s + old_length - (new_length - x - 3),
               new_length - x - 3);

        return r;
}

char *ellipsize_mem(const char *s, size_t old_length, size_t new_length, unsigned percent) {
        size_t x;
        char *e;
        const char *i, *j;
        unsigned k, len, len2;

        assert(s);
        assert(percent <= 100);
        assert(new_length >= 3);

        /* if no multibyte characters use ascii_ellipsize_mem for speed */
        if (ascii_is_valid(s))
                return ascii_ellipsize_mem(s, old_length, new_length, percent);

        if (old_length <= 3 || old_length <= new_length)
                return strndup(s, old_length);

        x = (new_length * percent) / 100;

        if (x > new_length - 3)
                x = new_length - 3;

        k = 0;
        for (i = s; k < x && i < s + old_length; i = utf8_next_char(i)) {
                int c;

                c = utf8_encoded_to_unichar(i);
                if (c < 0)
                        return NULL;
                k += unichar_iswide(c) ? 2 : 1;
        }

        if (k > x) /* last character was wide and went over quota */
                x ++;

        for (j = s + old_length; k < new_length && j > i; ) {
                int c;

                j = utf8_prev_char(j);
                c = utf8_encoded_to_unichar(j);
                if (c < 0)
                        return NULL;
                k += unichar_iswide(c) ? 2 : 1;
        }
        assert(i <= j);

        /* we don't actually need to ellipsize */
        if (i == j)
                return memdup(s, old_length + 1);

        /* make space for ellipsis */
        j = utf8_next_char(j);

        len = i - s;
        len2 = s + old_length - j;
        e = new(char, len + 3 + len2 + 1);
        if (!e)
                return NULL;

        /*
        printf("old_length=%zu new_length=%zu x=%zu len=%u len2=%u k=%u\n",
               old_length, new_length, x, len, len2, k);
        */

        memcpy(e, s, len);
        e[len]   = 0xe2; /* tri-dot ellipsis: … */
        e[len + 1] = 0x80;
        e[len + 2] = 0xa6;

        memcpy(e + len + 3, j, len2 + 1);

        return e;
}

char *ellipsize(const char *s, size_t length, unsigned percent) {
        return ellipsize_mem(s, strlen(s), length, percent);
}

int touch_file(const char *path, bool parents, usec_t stamp, uid_t uid, gid_t gid, mode_t mode) {
        _cleanup_close_ int fd;
        int r;

        assert(path);

        if (parents)
                mkdir_parents(path, 0755);

        fd = open(path, O_WRONLY|O_CREAT|O_CLOEXEC|O_NOCTTY, mode > 0 ? mode : 0644);
        if (fd < 0)
                return -errno;

        if (mode > 0) {
                r = fchmod(fd, mode);
                if (r < 0)
                        return -errno;
        }

        if (uid != UID_INVALID || gid != GID_INVALID) {
                r = fchown(fd, uid, gid);
                if (r < 0)
                        return -errno;
        }

        if (stamp != USEC_INFINITY) {
                struct timespec ts[2];

                timespec_store(&ts[0], stamp);
                ts[1] = ts[0];
                r = futimens(fd, ts);
        } else
                r = futimens(fd, NULL);
        if (r < 0)
                return -errno;

        return 0;
}

int touch(const char *path) {
        return touch_file(path, false, USEC_INFINITY, UID_INVALID, GID_INVALID, 0);
}

char *unquote(const char *s, const char* quotes) {
        size_t l;
        assert(s);

        /* This is rather stupid, simply removes the heading and
         * trailing quotes if there is one. Doesn't care about
         * escaping or anything. We should make this smarter one
         * day... */

        l = strlen(s);
        if (l < 2)
                return strdup(s);

        if (strchr(quotes, s[0]) && s[l-1] == s[0])
                return strndup(s+1, l-2);

        return strdup(s);
}

char *normalize_env_assignment(const char *s) {
        _cleanup_free_ char *value = NULL;
        const char *eq;
        char *p, *name;

        eq = strchr(s, '=');
        if (!eq) {
                char *r, *t;

                r = strdup(s);
                if (!r)
                        return NULL;

                t = strstrip(r);
                if (t != r)
                        memmove(r, t, strlen(t) + 1);

                return r;
        }

        name = strndupa(s, eq - s);
        p = strdupa(eq + 1);

        value = unquote(strstrip(p), QUOTES);
        if (!value)
                return NULL;

        return strjoin(strstrip(name), "=", value, NULL);
}

int wait_for_terminate(pid_t pid, siginfo_t *status) {
        siginfo_t dummy;

        assert(pid >= 1);

        if (!status)
                status = &dummy;

        for (;;) {
                zero(*status);

                if (waitid(P_PID, pid, status, WEXITED) < 0) {

                        if (errno == EINTR)
                                continue;

                        return -errno;
                }

                return 0;
        }
}

/*
 * Return values:
 * < 0 : wait_for_terminate() failed to get the state of the
 *       process, the process was terminated by a signal, or
 *       failed for an unknown reason.
 * >=0 : The process terminated normally, and its exit code is
 *       returned.
 *
 * That is, success is indicated by a return value of zero, and an
 * error is indicated by a non-zero value.
 *
 * A warning is emitted if the process terminates abnormally,
 * and also if it returns non-zero unless check_exit_code is true.
 */
int wait_for_terminate_and_warn(const char *name, pid_t pid, bool check_exit_code) {
        int r;
        siginfo_t status;

        assert(name);
        assert(pid > 1);

        r = wait_for_terminate(pid, &status);
        if (r < 0)
                return log_warning_errno(r, "Failed to wait for %s: %m", name);

        if (status.si_code == CLD_EXITED) {
                if (status.si_status != 0)
                        log_full(check_exit_code ? LOG_WARNING : LOG_DEBUG,
                                 "%s failed with error code %i.", name, status.si_status);
                else
                        log_debug("%s succeeded.", name);

                return status.si_status;
        } else if (status.si_code == CLD_KILLED ||
                   status.si_code == CLD_DUMPED) {

                log_warning("%s terminated by signal %s.", name, signal_to_string(status.si_status));
                return -EPROTO;
        }

        log_warning("%s failed due to unknown reason.", name);
        return -EPROTO;
}

noreturn void freeze(void) {

        /* Make sure nobody waits for us on a socket anymore */
        close_all_fds(NULL, 0);

        sync();

        for (;;)
                pause();
}

bool null_or_empty(struct stat *st) {
        assert(st);

        if (S_ISREG(st->st_mode) && st->st_size <= 0)
                return true;

        if (S_ISCHR(st->st_mode) || S_ISBLK(st->st_mode))
                return true;

        return false;
}

int null_or_empty_path(const char *fn) {
        struct stat st;

        assert(fn);

        if (stat(fn, &st) < 0)
                return -errno;

        return null_or_empty(&st);
}

int null_or_empty_fd(int fd) {
        struct stat st;

        assert(fd >= 0);

        if (fstat(fd, &st) < 0)
                return -errno;

        return null_or_empty(&st);
}

DIR *xopendirat(int fd, const char *name, int flags) {
        int nfd;
        DIR *d;

        assert(!(flags & O_CREAT));

        nfd = openat(fd, name, O_RDONLY|O_NONBLOCK|O_DIRECTORY|O_CLOEXEC|flags, 0);
        if (nfd < 0)
                return NULL;

        d = fdopendir(nfd);
        if (!d) {
                safe_close(nfd);
                return NULL;
        }

        return d;
}

int signal_from_string_try_harder(const char *s) {
        int signo;
        assert(s);

        signo = signal_from_string(s);
        if (signo <= 0)
                if (startswith(s, "SIG"))
                        return signal_from_string(s+3);

        return signo;
}

static char *tag_to_udev_node(const char *tagvalue, const char *by) {
        _cleanup_free_ char *t = NULL, *u = NULL;
        size_t enc_len;

        u = unquote(tagvalue, "\"\'");
        if (!u)
                return NULL;

        enc_len = strlen(u) * 4 + 1;
        t = new(char, enc_len);
        if (!t)
                return NULL;

        if (encode_devnode_name(u, t, enc_len) < 0)
                return NULL;

        return strjoin("/dev/disk/by-", by, "/", t, NULL);
}

char *fstab_node_to_udev_node(const char *p) {
        assert(p);

        if (startswith(p, "LABEL="))
                return tag_to_udev_node(p+6, "label");

        if (startswith(p, "UUID="))
                return tag_to_udev_node(p+5, "uuid");

        if (startswith(p, "PARTUUID="))
                return tag_to_udev_node(p+9, "partuuid");

        if (startswith(p, "PARTLABEL="))
                return tag_to_udev_node(p+10, "partlabel");

        return strdup(p);
}

bool tty_is_vc(const char *tty) {
        assert(tty);

        return vtnr_from_tty(tty) >= 0;
}

bool tty_is_console(const char *tty) {
        assert(tty);

        if (startswith(tty, "/dev/"))
                tty += 5;

        return streq(tty, "console");
}

int vtnr_from_tty(const char *tty) {
        int i, r;

        assert(tty);

        if (startswith(tty, "/dev/"))
                tty += 5;

        if (!startswith(tty, "tty") )
                return -EINVAL;

        if (tty[3] < '0' || tty[3] > '9')
                return -EINVAL;

        r = safe_atoi(tty+3, &i);
        if (r < 0)
                return r;

        if (i < 0 || i > 63)
                return -EINVAL;

        return i;
}

char *resolve_dev_console(char **active) {
        char *tty;

        /* Resolve where /dev/console is pointing to, if /sys is actually ours
         * (i.e. not read-only-mounted which is a sign for container setups) */

        if (path_is_read_only_fs("/sys") > 0)
                return NULL;

        if (read_one_line_file("/sys/class/tty/console/active", active) < 0)
                return NULL;

        /* If multiple log outputs are configured the last one is what
         * /dev/console points to */
        tty = strrchr(*active, ' ');
        if (tty)
                tty++;
        else
                tty = *active;

        if (streq(tty, "tty0")) {
                char *tmp;

                /* Get the active VC (e.g. tty1) */
                if (read_one_line_file("/sys/class/tty/tty0/active", &tmp) >= 0) {
                        free(*active);
                        tty = *active = tmp;
                }
        }

        return tty;
}

bool tty_is_vc_resolve(const char *tty) {
        _cleanup_free_ char *active = NULL;

        assert(tty);

        if (startswith(tty, "/dev/"))
                tty += 5;

        if (streq(tty, "console")) {
                tty = resolve_dev_console(&active);
                if (!tty)
                        return false;
        }

        return tty_is_vc(tty);
}

const char *default_term_for_tty(const char *tty) {
        assert(tty);

        return tty_is_vc_resolve(tty) ? "TERM=linux" : "TERM=vt220";
}

bool dirent_is_file(const struct dirent *de) {
        assert(de);

        if (hidden_file(de->d_name))
                return false;

        if (de->d_type != DT_REG &&
            de->d_type != DT_LNK &&
            de->d_type != DT_UNKNOWN)
                return false;

        return true;
}

bool dirent_is_file_with_suffix(const struct dirent *de, const char *suffix) {
        assert(de);

        if (de->d_type != DT_REG &&
            de->d_type != DT_LNK &&
            de->d_type != DT_UNKNOWN)
                return false;

        if (hidden_file_allow_backup(de->d_name))
                return false;

        return endswith(de->d_name, suffix);
}

static int do_execute(char **directories, usec_t timeout, char *argv[]) {
        _cleanup_hashmap_free_free_ Hashmap *pids = NULL;
        _cleanup_set_free_free_ Set *seen = NULL;
        char **directory;

        /* We fork this all off from a child process so that we can
         * somewhat cleanly make use of SIGALRM to set a time limit */

        reset_all_signal_handlers();
        reset_signal_mask();

        assert_se(prctl(PR_SET_PDEATHSIG, SIGTERM) == 0);

        pids = hashmap_new(NULL);
        if (!pids)
                return log_oom();

        seen = set_new(&string_hash_ops);
        if (!seen)
                return log_oom();

        STRV_FOREACH(directory, directories) {
                _cleanup_closedir_ DIR *d;
                struct dirent *de;

                d = opendir(*directory);
                if (!d) {
                        if (errno == ENOENT)
                                continue;

                        return log_error_errno(errno, "Failed to open directory %s: %m", *directory);
                }

                FOREACH_DIRENT(de, d, break) {
                        _cleanup_free_ char *path = NULL;
                        pid_t pid;
                        int r;

                        if (!dirent_is_file(de))
                                continue;

                        if (set_contains(seen, de->d_name)) {
                                log_debug("%1$s/%2$s skipped (%2$s was already seen).", *directory, de->d_name);
                                continue;
                        }

                        r = set_put_strdup(seen, de->d_name);
                        if (r < 0)
                                return log_oom();

                        path = strjoin(*directory, "/", de->d_name, NULL);
                        if (!path)
                                return log_oom();

                        if (null_or_empty_path(path)) {
                                log_debug("%s is empty (a mask).", path);
                                continue;
                        }

                        pid = fork();
                        if (pid < 0) {
                                log_error_errno(errno, "Failed to fork: %m");
                                continue;
                        } else if (pid == 0) {
                                char *_argv[2];

                                assert_se(prctl(PR_SET_PDEATHSIG, SIGTERM) == 0);

                                if (!argv) {
                                        _argv[0] = path;
                                        _argv[1] = NULL;
                                        argv = _argv;
                                } else
                                        argv[0] = path;

                                execv(path, argv);
                                return log_error_errno(errno, "Failed to execute %s: %m", path);
                        }

                        log_debug("Spawned %s as " PID_FMT ".", path, pid);

                        r = hashmap_put(pids, UINT_TO_PTR(pid), path);
                        if (r < 0)
                                return log_oom();
                        path = NULL;
                }
        }

        /* Abort execution of this process after the timout. We simply
         * rely on SIGALRM as default action terminating the process,
         * and turn on alarm(). */

        if (timeout != USEC_INFINITY)
                alarm((timeout + USEC_PER_SEC - 1) / USEC_PER_SEC);

        while (!hashmap_isempty(pids)) {
                _cleanup_free_ char *path = NULL;
                pid_t pid;

                pid = PTR_TO_UINT(hashmap_first_key(pids));
                assert(pid > 0);

                path = hashmap_remove(pids, UINT_TO_PTR(pid));
                assert(path);

                wait_for_terminate_and_warn(path, pid, true);
        }

        return 0;
}

void execute_directories(const char* const* directories, usec_t timeout, char *argv[]) {
        pid_t executor_pid;
        int r;
        char *name;
        char **dirs = (char**) directories;

        assert(!strv_isempty(dirs));

        name = basename(dirs[0]);
        assert(!isempty(name));

        /* Executes all binaries in the directories in parallel and waits
         * for them to finish. Optionally a timeout is applied. If a file
         * with the same name exists in more than one directory, the
         * earliest one wins. */

        executor_pid = fork();
        if (executor_pid < 0) {
                log_error_errno(errno, "Failed to fork: %m");
                return;

        } else if (executor_pid == 0) {
                r = do_execute(dirs, timeout, argv);
                _exit(r < 0 ? EXIT_FAILURE : EXIT_SUCCESS);
        }

        wait_for_terminate_and_warn(name, executor_pid, true);
}

int kill_and_sigcont(pid_t pid, int sig) {
        int r;

        r = kill(pid, sig) < 0 ? -errno : 0;

        if (r >= 0)
                kill(pid, SIGCONT);

        return r;
}

bool nulstr_contains(const char*nulstr, const char *needle) {
        const char *i;

        if (!nulstr)
                return false;

        NULSTR_FOREACH(i, nulstr)
                if (streq(i, needle))
                        return true;

        return false;
}

bool plymouth_running(void) {
        return access("/run/plymouth/pid", F_OK) >= 0;
}

char* strshorten(char *s, size_t l) {
        assert(s);

        if (l < strlen(s))
                s[l] = 0;

        return s;
}

static bool hostname_valid_char(char c) {
        return
                (c >= 'a' && c <= 'z') ||
                (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') ||
                c == '-' ||
                c == '_' ||
                c == '.';
}

bool hostname_is_valid(const char *s) {
        const char *p;
        bool dot;

        if (isempty(s))
                return false;

        /* Doesn't accept empty hostnames, hostnames with trailing or
         * leading dots, and hostnames with multiple dots in a
         * sequence. Also ensures that the length stays below
         * HOST_NAME_MAX. */

        for (p = s, dot = true; *p; p++) {
                if (*p == '.') {
                        if (dot)
                                return false;

                        dot = true;
                } else {
                        if (!hostname_valid_char(*p))
                                return false;

                        dot = false;
                }
        }

        if (dot)
                return false;

        if (p-s > HOST_NAME_MAX)
                return false;

        return true;
}

char* hostname_cleanup(char *s, bool lowercase) {
        char *p, *d;
        bool dot;

        for (p = s, d = s, dot = true; *p; p++) {
                if (*p == '.') {
                        if (dot)
                                continue;

                        *(d++) = '.';
                        dot = true;
                } else if (hostname_valid_char(*p)) {
                        *(d++) = lowercase ? tolower(*p) : *p;
                        dot = false;
                }

        }

        if (dot && d > s)
                d[-1] = 0;
        else
                *d = 0;

        strshorten(s, HOST_NAME_MAX);

        return s;
}

bool machine_name_is_valid(const char *s) {

        if (!hostname_is_valid(s))
                return false;

        /* Machine names should be useful hostnames, but also be
         * useful in unit names, hence we enforce a stricter length
         * limitation. */

        if (strlen(s) > 64)
                return false;

        return true;
}

int pipe_eof(int fd) {
        struct pollfd pollfd = {
                .fd = fd,
                .events = POLLIN|POLLHUP,
        };

        int r;

        r = poll(&pollfd, 1, 0);
        if (r < 0)
                return -errno;

        if (r == 0)
                return 0;

        return pollfd.revents & POLLHUP;
}

int fd_wait_for_event(int fd, int event, usec_t t) {

        struct pollfd pollfd = {
                .fd = fd,
                .events = event,
        };

        struct timespec ts;
        int r;

        r = ppoll(&pollfd, 1, t == USEC_INFINITY ? NULL : timespec_store(&ts, t), NULL);
        if (r < 0)
                return -errno;

        if (r == 0)
                return 0;

        return pollfd.revents;
}

int fopen_temporary(const char *path, FILE **_f, char **_temp_path) {
        FILE *f;
        char *t;
        int r, fd;

        assert(path);
        assert(_f);
        assert(_temp_path);

        r = tempfn_xxxxxx(path, &t);
        if (r < 0)
                return r;

        fd = mkostemp_safe(t, O_WRONLY|O_CLOEXEC);
        if (fd < 0) {
                free(t);
                return -errno;
        }

        f = fdopen(fd, "we");
        if (!f) {
                unlink(t);
                free(t);
                return -errno;
        }

        *_f = f;
        *_temp_path = t;

        return 0;
}

int terminal_vhangup_fd(int fd) {
        assert(fd >= 0);

        if (ioctl(fd, TIOCVHANGUP) < 0)
                return -errno;

        return 0;
}

int terminal_vhangup(const char *name) {
        _cleanup_close_ int fd;

        fd = open_terminal(name, O_RDWR|O_NOCTTY|O_CLOEXEC);
        if (fd < 0)
                return fd;

        return terminal_vhangup_fd(fd);
}

int vt_disallocate(const char *name) {
        int fd, r;
        unsigned u;

        /* Deallocate the VT if possible. If not possible
         * (i.e. because it is the active one), at least clear it
         * entirely (including the scrollback buffer) */

        if (!startswith(name, "/dev/"))
                return -EINVAL;

        if (!tty_is_vc(name)) {
                /* So this is not a VT. I guess we cannot deallocate
                 * it then. But let's at least clear the screen */

                fd = open_terminal(name, O_RDWR|O_NOCTTY|O_CLOEXEC);
                if (fd < 0)
                        return fd;

                loop_write(fd,
                           "\033[r"    /* clear scrolling region */
                           "\033[H"    /* move home */
                           "\033[2J",  /* clear screen */
                           10, false);
                safe_close(fd);

                return 0;
        }

        if (!startswith(name, "/dev/tty"))
                return -EINVAL;

        r = safe_atou(name+8, &u);
        if (r < 0)
                return r;

        if (u <= 0)
                return -EINVAL;

        /* Try to deallocate */
        fd = open_terminal("/dev/tty0", O_RDWR|O_NOCTTY|O_CLOEXEC);
        if (fd < 0)
                return fd;

        r = ioctl(fd, VT_DISALLOCATE, u);
        safe_close(fd);

        if (r >= 0)
                return 0;

        if (errno != EBUSY)
                return -errno;

        /* Couldn't deallocate, so let's clear it fully with
         * scrollback */
        fd = open_terminal(name, O_RDWR|O_NOCTTY|O_CLOEXEC);
        if (fd < 0)
                return fd;

        loop_write(fd,
                   "\033[r"   /* clear scrolling region */
                   "\033[H"   /* move home */
                   "\033[3J", /* clear screen including scrollback, requires Linux 2.6.40 */
                   10, false);
        safe_close(fd);

        return 0;
}

int symlink_atomic(const char *from, const char *to) {
        _cleanup_free_ char *t = NULL;
        int r;

        assert(from);
        assert(to);

        r = tempfn_random(to, &t);
        if (r < 0)
                return r;

        if (symlink(from, t) < 0)
                return -errno;

        if (rename(t, to) < 0) {
                unlink_noerrno(t);
                return -errno;
        }

        return 0;
}

int mknod_atomic(const char *path, mode_t mode, dev_t dev) {
        _cleanup_free_ char *t = NULL;
        int r;

        assert(path);

        r = tempfn_random(path, &t);
        if (r < 0)
                return r;

        if (mknod(t, mode, dev) < 0)
                return -errno;

        if (rename(t, path) < 0) {
                unlink_noerrno(t);
                return -errno;
        }

        return 0;
}

int mkfifo_atomic(const char *path, mode_t mode) {
        _cleanup_free_ char *t = NULL;
        int r;

        assert(path);

        r = tempfn_random(path, &t);
        if (r < 0)
                return r;

        if (mkfifo(t, mode) < 0)
                return -errno;

        if (rename(t, path) < 0) {
                unlink_noerrno(t);
                return -errno;
        }

        return 0;
}

bool display_is_local(const char *display) {
        assert(display);

        return
                display[0] == ':' &&
                display[1] >= '0' &&
                display[1] <= '9';
}

int socket_from_display(const char *display, char **path) {
        size_t k;
        char *f, *c;

        assert(display);
        assert(path);

        if (!display_is_local(display))
                return -EINVAL;

        k = strspn(display+1, "0123456789");

        f = new(char, strlen("/tmp/.X11-unix/X") + k + 1);
        if (!f)
                return -ENOMEM;

        c = stpcpy(f, "/tmp/.X11-unix/X");
        memcpy(c, display+1, k);
        c[k] = 0;

        *path = f;

        return 0;
}

int get_user_creds(
                const char **username,
                uid_t *uid, gid_t *gid,
                const char **home,
                const char **shell) {

        struct passwd *p;
        uid_t u;

        assert(username);
        assert(*username);

        /* We enforce some special rules for uid=0: in order to avoid
         * NSS lookups for root we hardcode its data. */

        if (streq(*username, "root") || streq(*username, "0")) {
                *username = "root";

                if (uid)
                        *uid = 0;

                if (gid)
                        *gid = 0;

                if (home)
                        *home = "/root";

                if (shell)
                        *shell = "/bin/sh";

                return 0;
        }

        if (parse_uid(*username, &u) >= 0) {
                errno = 0;
                p = getpwuid(u);

                /* If there are multiple users with the same id, make
                 * sure to leave $USER to the configured value instead
                 * of the first occurrence in the database. However if
                 * the uid was configured by a numeric uid, then let's
                 * pick the real username from /etc/passwd. */
                if (p)
                        *username = p->pw_name;
        } else {
                errno = 0;
                p = getpwnam(*username);
        }

        if (!p)
                return errno > 0 ? -errno : -ESRCH;

        if (uid)
                *uid = p->pw_uid;

        if (gid)
                *gid = p->pw_gid;

        if (home)
                *home = p->pw_dir;

        if (shell)
                *shell = p->pw_shell;

        return 0;
}

char* uid_to_name(uid_t uid) {
        struct passwd *p;
        char *r;

        if (uid == 0)
                return strdup("root");

        p = getpwuid(uid);
        if (p)
                return strdup(p->pw_name);

        if (asprintf(&r, UID_FMT, uid) < 0)
                return NULL;

        return r;
}

char* gid_to_name(gid_t gid) {
        struct group *p;
        char *r;

        if (gid == 0)
                return strdup("root");

        p = getgrgid(gid);
        if (p)
                return strdup(p->gr_name);

        if (asprintf(&r, GID_FMT, gid) < 0)
                return NULL;

        return r;
}

int get_group_creds(const char **groupname, gid_t *gid) {
        struct group *g;
        gid_t id;

        assert(groupname);

        /* We enforce some special rules for gid=0: in order to avoid
         * NSS lookups for root we hardcode its data. */

        if (streq(*groupname, "root") || streq(*groupname, "0")) {
                *groupname = "root";

                if (gid)
                        *gid = 0;

                return 0;
        }

        if (parse_gid(*groupname, &id) >= 0) {
                errno = 0;
                g = getgrgid(id);

                if (g)
                        *groupname = g->gr_name;
        } else {
                errno = 0;
                g = getgrnam(*groupname);
        }

        if (!g)
                return errno > 0 ? -errno : -ESRCH;

        if (gid)
                *gid = g->gr_gid;

        return 0;
}

int in_gid(gid_t gid) {
        gid_t *gids;
        int ngroups_max, r, i;

        if (getgid() == gid)
                return 1;

        if (getegid() == gid)
                return 1;

        ngroups_max = sysconf(_SC_NGROUPS_MAX);
        assert(ngroups_max > 0);

        gids = alloca(sizeof(gid_t) * ngroups_max);

        r = getgroups(ngroups_max, gids);
        if (r < 0)
                return -errno;

        for (i = 0; i < r; i++)
                if (gids[i] == gid)
                        return 1;

        return 0;
}

int in_group(const char *name) {
        int r;
        gid_t gid;

        r = get_group_creds(&name, &gid);
        if (r < 0)
                return r;

        return in_gid(gid);
}

int glob_exists(const char *path) {
        _cleanup_globfree_ glob_t g = {};
        int k;

        assert(path);

        errno = 0;
        k = glob(path, GLOB_NOSORT|GLOB_BRACE, NULL, &g);

        if (k == GLOB_NOMATCH)
                return 0;
        else if (k == GLOB_NOSPACE)
                return -ENOMEM;
        else if (k == 0)
                return !strv_isempty(g.gl_pathv);
        else
                return errno ? -errno : -EIO;
}

int glob_extend(char ***strv, const char *path) {
        _cleanup_globfree_ glob_t g = {};
        int k;
        char **p;

        errno = 0;
        k = glob(path, GLOB_NOSORT|GLOB_BRACE, NULL, &g);

        if (k == GLOB_NOMATCH)
                return -ENOENT;
        else if (k == GLOB_NOSPACE)
                return -ENOMEM;
        else if (k != 0 || strv_isempty(g.gl_pathv))
                return errno ? -errno : -EIO;

        STRV_FOREACH(p, g.gl_pathv) {
                k = strv_extend(strv, *p);
                if (k < 0)
                        break;
        }

        return k;
}

int dirent_ensure_type(DIR *d, struct dirent *de) {
        struct stat st;

        assert(d);
        assert(de);

        if (de->d_type != DT_UNKNOWN)
                return 0;

        if (fstatat(dirfd(d), de->d_name, &st, AT_SYMLINK_NOFOLLOW) < 0)
                return -errno;

        de->d_type =
                S_ISREG(st.st_mode)  ? DT_REG  :
                S_ISDIR(st.st_mode)  ? DT_DIR  :
                S_ISLNK(st.st_mode)  ? DT_LNK  :
                S_ISFIFO(st.st_mode) ? DT_FIFO :
                S_ISSOCK(st.st_mode) ? DT_SOCK :
                S_ISCHR(st.st_mode)  ? DT_CHR  :
                S_ISBLK(st.st_mode)  ? DT_BLK  :
                                       DT_UNKNOWN;

        return 0;
}

int get_files_in_directory(const char *path, char ***list) {
        _cleanup_closedir_ DIR *d = NULL;
        size_t bufsize = 0, n = 0;
        _cleanup_strv_free_ char **l = NULL;

        assert(path);

        /* Returns all files in a directory in *list, and the number
         * of files as return value. If list is NULL returns only the
         * number. */

        d = opendir(path);
        if (!d)
                return -errno;

        for (;;) {
                struct dirent *de;

                errno = 0;
                de = readdir(d);
                if (!de && errno != 0)
                        return -errno;
                if (!de)
                        break;

                dirent_ensure_type(d, de);

                if (!dirent_is_file(de))
                        continue;

                if (list) {
                        /* one extra slot is needed for the terminating NULL */
                        if (!GREEDY_REALLOC(l, bufsize, n + 2))
                                return -ENOMEM;

                        l[n] = strdup(de->d_name);
                        if (!l[n])
                                return -ENOMEM;

                        l[++n] = NULL;
                } else
                        n++;
        }

        if (list) {
                *list = l;
                l = NULL; /* avoid freeing */
        }

        return n;
}

char *strjoin(const char *x, ...) {
        va_list ap;
        size_t l;
        char *r, *p;

        va_start(ap, x);

        if (x) {
                l = strlen(x);

                for (;;) {
                        const char *t;
                        size_t n;

                        t = va_arg(ap, const char *);
                        if (!t)
                                break;

                        n = strlen(t);
                        if (n > ((size_t) -1) - l) {
                                va_end(ap);
                                return NULL;
                        }

                        l += n;
                }
        } else
                l = 0;

        va_end(ap);

        r = new(char, l+1);
        if (!r)
                return NULL;

        if (x) {
                p = stpcpy(r, x);

                va_start(ap, x);

                for (;;) {
                        const char *t;

                        t = va_arg(ap, const char *);
                        if (!t)
                                break;

                        p = stpcpy(p, t);
                }

                va_end(ap);
        } else
                r[0] = 0;

        return r;
}

bool is_main_thread(void) {
        static thread_local int cached = 0;

        if (_unlikely_(cached == 0))
                cached = getpid() == gettid() ? 1 : -1;

        return cached > 0;
}

int block_get_whole_disk(dev_t d, dev_t *ret) {
        char *p, *s;
        int r;
        unsigned n, m;

        assert(ret);

        /* If it has a queue this is good enough for us */
        if (asprintf(&p, "/sys/dev/block/%u:%u/queue", major(d), minor(d)) < 0)
                return -ENOMEM;

        r = access(p, F_OK);
        free(p);

        if (r >= 0) {
                *ret = d;
                return 0;
        }

        /* If it is a partition find the originating device */
        if (asprintf(&p, "/sys/dev/block/%u:%u/partition", major(d), minor(d)) < 0)
                return -ENOMEM;

        r = access(p, F_OK);
        free(p);

        if (r < 0)
                return -ENOENT;

        /* Get parent dev_t */
        if (asprintf(&p, "/sys/dev/block/%u:%u/../dev", major(d), minor(d)) < 0)
                return -ENOMEM;

        r = read_one_line_file(p, &s);
        free(p);

        if (r < 0)
                return r;

        r = sscanf(s, "%u:%u", &m, &n);
        free(s);

        if (r != 2)
                return -EINVAL;

        /* Only return this if it is really good enough for us. */
        if (asprintf(&p, "/sys/dev/block/%u:%u/queue", m, n) < 0)
                return -ENOMEM;

        r = access(p, F_OK);
        free(p);

        if (r >= 0) {
                *ret = makedev(m, n);
                return 0;
        }

        return -ENOENT;
}

static const char *const ioprio_class_table[] = {
        [IOPRIO_CLASS_NONE] = "none",
        [IOPRIO_CLASS_RT] = "realtime",
        [IOPRIO_CLASS_BE] = "best-effort",
        [IOPRIO_CLASS_IDLE] = "idle"
};

DEFINE_STRING_TABLE_LOOKUP_WITH_FALLBACK(ioprio_class, int, INT_MAX);

static const char *const sigchld_code_table[] = {
        [CLD_EXITED] = "exited",
        [CLD_KILLED] = "killed",
        [CLD_DUMPED] = "dumped",
        [CLD_TRAPPED] = "trapped",
        [CLD_STOPPED] = "stopped",
        [CLD_CONTINUED] = "continued",
};

DEFINE_STRING_TABLE_LOOKUP(sigchld_code, int);

static const char *const log_facility_unshifted_table[LOG_NFACILITIES] = {
        [LOG_FAC(LOG_KERN)] = "kern",
        [LOG_FAC(LOG_USER)] = "user",
        [LOG_FAC(LOG_MAIL)] = "mail",
        [LOG_FAC(LOG_DAEMON)] = "daemon",
        [LOG_FAC(LOG_AUTH)] = "auth",
        [LOG_FAC(LOG_SYSLOG)] = "syslog",
        [LOG_FAC(LOG_LPR)] = "lpr",
        [LOG_FAC(LOG_NEWS)] = "news",
        [LOG_FAC(LOG_UUCP)] = "uucp",
        [LOG_FAC(LOG_CRON)] = "cron",
        [LOG_FAC(LOG_AUTHPRIV)] = "authpriv",
        [LOG_FAC(LOG_FTP)] = "ftp",
        [LOG_FAC(LOG_LOCAL0)] = "local0",
        [LOG_FAC(LOG_LOCAL1)] = "local1",
        [LOG_FAC(LOG_LOCAL2)] = "local2",
        [LOG_FAC(LOG_LOCAL3)] = "local3",
        [LOG_FAC(LOG_LOCAL4)] = "local4",
        [LOG_FAC(LOG_LOCAL5)] = "local5",
        [LOG_FAC(LOG_LOCAL6)] = "local6",
        [LOG_FAC(LOG_LOCAL7)] = "local7"
};

DEFINE_STRING_TABLE_LOOKUP_WITH_FALLBACK(log_facility_unshifted, int, LOG_FAC(~0));

static const char *const log_level_table[] = {
        [LOG_EMERG] = "emerg",
        [LOG_ALERT] = "alert",
        [LOG_CRIT] = "crit",
        [LOG_ERR] = "err",
        [LOG_WARNING] = "warning",
        [LOG_NOTICE] = "notice",
        [LOG_INFO] = "info",
        [LOG_DEBUG] = "debug"
};

DEFINE_STRING_TABLE_LOOKUP_WITH_FALLBACK(log_level, int, LOG_DEBUG);

static const char* const sched_policy_table[] = {
        [SCHED_OTHER] = "other",
        [SCHED_BATCH] = "batch",
        [SCHED_IDLE] = "idle",
        [SCHED_FIFO] = "fifo",
        [SCHED_RR] = "rr"
};

DEFINE_STRING_TABLE_LOOKUP_WITH_FALLBACK(sched_policy, int, INT_MAX);

static const char* const rlimit_table[_RLIMIT_MAX] = {
        [RLIMIT_CPU] = "LimitCPU",
        [RLIMIT_FSIZE] = "LimitFSIZE",
        [RLIMIT_DATA] = "LimitDATA",
        [RLIMIT_STACK] = "LimitSTACK",
        [RLIMIT_CORE] = "LimitCORE",
        [RLIMIT_RSS] = "LimitRSS",
        [RLIMIT_NOFILE] = "LimitNOFILE",
        [RLIMIT_AS] = "LimitAS",
        [RLIMIT_NPROC] = "LimitNPROC",
        [RLIMIT_MEMLOCK] = "LimitMEMLOCK",
        [RLIMIT_LOCKS] = "LimitLOCKS",
        [RLIMIT_SIGPENDING] = "LimitSIGPENDING",
        [RLIMIT_MSGQUEUE] = "LimitMSGQUEUE",
        [RLIMIT_NICE] = "LimitNICE",
        [RLIMIT_RTPRIO] = "LimitRTPRIO",
        [RLIMIT_RTTIME] = "LimitRTTIME"
};

DEFINE_STRING_TABLE_LOOKUP(rlimit, int);

static const char* const ip_tos_table[] = {
        [IPTOS_LOWDELAY] = "low-delay",
        [IPTOS_THROUGHPUT] = "throughput",
        [IPTOS_RELIABILITY] = "reliability",
        [IPTOS_LOWCOST] = "low-cost",
};

DEFINE_STRING_TABLE_LOOKUP_WITH_FALLBACK(ip_tos, int, 0xff);

static const char *const __signal_table[] = {
        [SIGHUP] = "HUP",
        [SIGINT] = "INT",
        [SIGQUIT] = "QUIT",
        [SIGILL] = "ILL",
        [SIGTRAP] = "TRAP",
        [SIGABRT] = "ABRT",
        [SIGBUS] = "BUS",
        [SIGFPE] = "FPE",
        [SIGKILL] = "KILL",
        [SIGUSR1] = "USR1",
        [SIGSEGV] = "SEGV",
        [SIGUSR2] = "USR2",
        [SIGPIPE] = "PIPE",
        [SIGALRM] = "ALRM",
        [SIGTERM] = "TERM",
#ifdef SIGSTKFLT
        [SIGSTKFLT] = "STKFLT",  /* Linux on SPARC doesn't know SIGSTKFLT */
#endif
        [SIGCHLD] = "CHLD",
        [SIGCONT] = "CONT",
        [SIGSTOP] = "STOP",
        [SIGTSTP] = "TSTP",
        [SIGTTIN] = "TTIN",
        [SIGTTOU] = "TTOU",
        [SIGURG] = "URG",
        [SIGXCPU] = "XCPU",
        [SIGXFSZ] = "XFSZ",
        [SIGVTALRM] = "VTALRM",
        [SIGPROF] = "PROF",
        [SIGWINCH] = "WINCH",
        [SIGIO] = "IO",
        [SIGPWR] = "PWR",
        [SIGSYS] = "SYS"
};

DEFINE_PRIVATE_STRING_TABLE_LOOKUP(__signal, int);

const char *signal_to_string(int signo) {
        static thread_local char buf[sizeof("RTMIN+")-1 + DECIMAL_STR_MAX(int) + 1];
        const char *name;

        name = __signal_to_string(signo);
        if (name)
                return name;

        if (signo >= SIGRTMIN && signo <= SIGRTMAX)
                snprintf(buf, sizeof(buf), "RTMIN+%d", signo - SIGRTMIN);
        else
                snprintf(buf, sizeof(buf), "%d", signo);

        return buf;
}

int signal_from_string(const char *s) {
        int signo;
        int offset = 0;
        unsigned u;

        signo = __signal_from_string(s);
        if (signo > 0)
                return signo;

        if (startswith(s, "RTMIN+")) {
                s += 6;
                offset = SIGRTMIN;
        }
        if (safe_atou(s, &u) >= 0) {
                signo = (int) u + offset;
                if (signo > 0 && signo < _NSIG)
                        return signo;
        }
        return -EINVAL;
}

bool kexec_loaded(void) {
       bool loaded = false;
       char *s;

       if (read_one_line_file("/sys/kernel/kexec_loaded", &s) >= 0) {
               if (s[0] == '1')
                       loaded = true;
               free(s);
       }
       return loaded;
}

int prot_from_flags(int flags) {

        switch (flags & O_ACCMODE) {

        case O_RDONLY:
                return PROT_READ;

        case O_WRONLY:
                return PROT_WRITE;

        case O_RDWR:
                return PROT_READ|PROT_WRITE;

        default:
                return -EINVAL;
        }
}

char *format_bytes(char *buf, size_t l, off_t t) {
        unsigned i;

        static const struct {
                const char *suffix;
                off_t factor;
        } table[] = {
                { "E", 1024ULL*1024ULL*1024ULL*1024ULL*1024ULL*1024ULL },
                { "P", 1024ULL*1024ULL*1024ULL*1024ULL*1024ULL },
                { "T", 1024ULL*1024ULL*1024ULL*1024ULL },
                { "G", 1024ULL*1024ULL*1024ULL },
                { "M", 1024ULL*1024ULL },
                { "K", 1024ULL },
        };

        if (t == (off_t) -1)
                return NULL;

        for (i = 0; i < ELEMENTSOF(table); i++) {

                if (t >= table[i].factor) {
                        snprintf(buf, l,
                                 "%llu.%llu%s",
                                 (unsigned long long) (t / table[i].factor),
                                 (unsigned long long) (((t*10ULL) / table[i].factor) % 10ULL),
                                 table[i].suffix);

                        goto finish;
                }
        }

        snprintf(buf, l, "%lluB", (unsigned long long) t);

finish:
        buf[l-1] = 0;
        return buf;

}

void* memdup(const void *p, size_t l) {
        void *r;

        assert(p);

        r = malloc(l);
        if (!r)
                return NULL;

        memcpy(r, p, l);
        return r;
}

int fd_inc_sndbuf(int fd, size_t n) {
        int r, value;
        socklen_t l = sizeof(value);

        r = getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &value, &l);
        if (r >= 0 && l == sizeof(value) && (size_t) value >= n*2)
                return 0;

        /* If we have the privileges we will ignore the kernel limit. */

        value = (int) n;
        if (setsockopt(fd, SOL_SOCKET, SO_SNDBUFFORCE, &value, sizeof(value)) < 0)
                if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &value, sizeof(value)) < 0)
                        return -errno;

        return 1;
}

int fd_inc_rcvbuf(int fd, size_t n) {
        int r, value;
        socklen_t l = sizeof(value);

        r = getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &value, &l);
        if (r >= 0 && l == sizeof(value) && (size_t) value >= n*2)
                return 0;

        /* If we have the privileges we will ignore the kernel limit. */

        value = (int) n;
        if (setsockopt(fd, SOL_SOCKET, SO_RCVBUFFORCE, &value, sizeof(value)) < 0)
                if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &value, sizeof(value)) < 0)
                        return -errno;
        return 1;
}

int fork_agent(pid_t *pid, const int except[], unsigned n_except, const char *path, ...) {
        bool stdout_is_tty, stderr_is_tty;
        pid_t parent_pid, agent_pid;
        sigset_t ss, saved_ss;
        unsigned n, i;
        va_list ap;
        char **l;

        assert(pid);
        assert(path);

        /* Spawns a temporary TTY agent, making sure it goes away when
         * we go away */

        parent_pid = getpid();

        /* First we temporarily block all signals, so that the new
         * child has them blocked initially. This way, we can be sure
         * that SIGTERMs are not lost we might send to the agent. */
        assert_se(sigfillset(&ss) >= 0);
        assert_se(sigprocmask(SIG_SETMASK, &ss, &saved_ss) >= 0);

        agent_pid = fork();
        if (agent_pid < 0) {
                assert_se(sigprocmask(SIG_SETMASK, &saved_ss, NULL) >= 0);
                return -errno;
        }

        if (agent_pid != 0) {
                assert_se(sigprocmask(SIG_SETMASK, &saved_ss, NULL) >= 0);
                *pid = agent_pid;
                return 0;
        }

        /* In the child:
         *
         * Make sure the agent goes away when the parent dies */
        if (prctl(PR_SET_PDEATHSIG, SIGTERM) < 0)
                _exit(EXIT_FAILURE);

        /* Make sure we actually can kill the agent, if we need to, in
         * case somebody invoked us from a shell script that trapped
         * SIGTERM or so... */
        reset_all_signal_handlers();
        reset_signal_mask();

        /* Check whether our parent died before we were able
         * to set the death signal and unblock the signals */
        if (getppid() != parent_pid)
                _exit(EXIT_SUCCESS);

        /* Don't leak fds to the agent */
        close_all_fds(except, n_except);

        stdout_is_tty = isatty(STDOUT_FILENO);
        stderr_is_tty = isatty(STDERR_FILENO);

        if (!stdout_is_tty || !stderr_is_tty) {
                int fd;

                /* Detach from stdout/stderr. and reopen
                 * /dev/tty for them. This is important to
                 * ensure that when systemctl is started via
                 * popen() or a similar call that expects to
                 * read EOF we actually do generate EOF and
                 * not delay this indefinitely by because we
                 * keep an unused copy of stdin around. */
                fd = open("/dev/tty", O_WRONLY);
                if (fd < 0) {
                        log_error_errno(errno, "Failed to open /dev/tty: %m");
                        _exit(EXIT_FAILURE);
                }

                if (!stdout_is_tty)
                        dup2(fd, STDOUT_FILENO);

                if (!stderr_is_tty)
                        dup2(fd, STDERR_FILENO);

                if (fd > 2)
                        close(fd);
        }

        /* Count arguments */
        va_start(ap, path);
        for (n = 0; va_arg(ap, char*); n++)
                ;
        va_end(ap);

        /* Allocate strv */
        l = alloca(sizeof(char *) * (n + 1));

        /* Fill in arguments */
        va_start(ap, path);
        for (i = 0; i <= n; i++)
                l[i] = va_arg(ap, char*);
        va_end(ap);

        execv(path, l);
        _exit(EXIT_FAILURE);
}

int setrlimit_closest(int resource, const struct rlimit *rlim) {
        struct rlimit highest, fixed;

        assert(rlim);

        if (setrlimit(resource, rlim) >= 0)
                return 0;

        if (errno != EPERM)
                return -errno;

        /* So we failed to set the desired setrlimit, then let's try
         * to get as close as we can */
        assert_se(getrlimit(resource, &highest) == 0);

        fixed.rlim_cur = MIN(rlim->rlim_cur, highest.rlim_max);
        fixed.rlim_max = MIN(rlim->rlim_max, highest.rlim_max);

        if (setrlimit(resource, &fixed) < 0)
                return -errno;

        return 0;
}

int getenv_for_pid(pid_t pid, const char *field, char **_value) {
        _cleanup_fclose_ FILE *f = NULL;
        char *value = NULL;
        int r;
        bool done = false;
        size_t l;
        const char *path;

        assert(pid >= 0);
        assert(field);
        assert(_value);

        path = procfs_file_alloca(pid, "environ");

        f = fopen(path, "re");
        if (!f)
                return -errno;

        l = strlen(field);
        r = 0;

        do {
                char line[LINE_MAX];
                unsigned i;

                for (i = 0; i < sizeof(line)-1; i++) {
                        int c;

                        c = getc(f);
                        if (_unlikely_(c == EOF)) {
                                done = true;
                                break;
                        } else if (c == 0)
                                break;

                        line[i] = c;
                }
                line[i] = 0;

                if (memcmp(line, field, l) == 0 && line[l] == '=') {
                        value = strdup(line + l + 1);
                        if (!value)
                                return -ENOMEM;

                        r = 1;
                        break;
                }

        } while (!done);

        *_value = value;
        return r;
}

bool http_etag_is_valid(const char *etag) {
        if (isempty(etag))
                return false;

        if (!endswith(etag, "\""))
                return false;

        if (!startswith(etag, "\"") && !startswith(etag, "W/\""))
                return false;

        return true;
}

bool http_url_is_valid(const char *url) {
        const char *p;

        if (isempty(url))
                return false;

        p = startswith(url, "http://");
        if (!p)
                p = startswith(url, "https://");
        if (!p)
                return false;

        if (isempty(p))
                return false;

        return ascii_is_valid(p);
}

bool documentation_url_is_valid(const char *url) {
        const char *p;

        if (isempty(url))
                return false;

        if (http_url_is_valid(url))
                return true;

        p = startswith(url, "file:/");
        if (!p)
                p = startswith(url, "info:");
        if (!p)
                p = startswith(url, "man:");

        if (isempty(p))
                return false;

        return ascii_is_valid(p);
}

bool in_initrd(void) {
        static int saved = -1;
        struct statfs s;

        if (saved >= 0)
                return saved;

        /* We make two checks here:
         *
         * 1. the flag file /etc/initrd-release must exist
         * 2. the root file system must be a memory file system
         *
         * The second check is extra paranoia, since misdetecting an
         * initrd can have bad bad consequences due the initrd
         * emptying when transititioning to the main systemd.
         */

        saved = access("/etc/initrd-release", F_OK) >= 0 &&
                statfs("/", &s) >= 0 &&
                is_temporary_fs(&s);

        return saved;
}

void warn_melody(void) {
        _cleanup_close_ int fd = -1;

        fd = open("/dev/console", O_WRONLY|O_CLOEXEC|O_NOCTTY);
        if (fd < 0)
                return;

        /* Yeah, this is synchronous. Kinda sucks. But well... */

        ioctl(fd, KIOCSOUND, (int)(1193180/440));
        usleep(125*USEC_PER_MSEC);

        ioctl(fd, KIOCSOUND, (int)(1193180/220));
        usleep(125*USEC_PER_MSEC);

        ioctl(fd, KIOCSOUND, (int)(1193180/220));
        usleep(125*USEC_PER_MSEC);

        ioctl(fd, KIOCSOUND, 0);
}

int make_console_stdio(void) {
        int fd, r;

        /* Make /dev/console the controlling terminal and stdin/stdout/stderr */

        fd = acquire_terminal("/dev/console", false, true, true, USEC_INFINITY);
        if (fd < 0)
                return log_error_errno(fd, "Failed to acquire terminal: %m");

        r = make_stdio(fd);
        if (r < 0)
                return log_error_errno(r, "Failed to duplicate terminal fd: %m");

        return 0;
}

int get_home_dir(char **_h) {
        struct passwd *p;
        const char *e;
        char *h;
        uid_t u;

        assert(_h);

        /* Take the user specified one */
        e = secure_getenv("HOME");
        if (e && path_is_absolute(e)) {
                h = strdup(e);
                if (!h)
                        return -ENOMEM;

                *_h = h;
                return 0;
        }

        /* Hardcode home directory for root to avoid NSS */
        u = getuid();
        if (u == 0) {
                h = strdup("/root");
                if (!h)
                        return -ENOMEM;

                *_h = h;
                return 0;
        }

        /* Check the database... */
        errno = 0;
        p = getpwuid(u);
        if (!p)
                return errno > 0 ? -errno : -ESRCH;

        if (!path_is_absolute(p->pw_dir))
                return -EINVAL;

        h = strdup(p->pw_dir);
        if (!h)
                return -ENOMEM;

        *_h = h;
        return 0;
}

int get_shell(char **_s) {
        struct passwd *p;
        const char *e;
        char *s;
        uid_t u;

        assert(_s);

        /* Take the user specified one */
        e = getenv("SHELL");
        if (e) {
                s = strdup(e);
                if (!s)
                        return -ENOMEM;

                *_s = s;
                return 0;
        }

        /* Hardcode home directory for root to avoid NSS */
        u = getuid();
        if (u == 0) {
                s = strdup("/bin/sh");
                if (!s)
                        return -ENOMEM;

                *_s = s;
                return 0;
        }

        /* Check the database... */
        errno = 0;
        p = getpwuid(u);
        if (!p)
                return errno > 0 ? -errno : -ESRCH;

        if (!path_is_absolute(p->pw_shell))
                return -EINVAL;

        s = strdup(p->pw_shell);
        if (!s)
                return -ENOMEM;

        *_s = s;
        return 0;
}

bool filename_is_valid(const char *p) {

        if (isempty(p))
                return false;

        if (strchr(p, '/'))
                return false;

        if (streq(p, "."))
                return false;

        if (streq(p, ".."))
                return false;

        if (strlen(p) > FILENAME_MAX)
                return false;

        return true;
}

bool string_is_safe(const char *p) {
        const char *t;

        if (!p)
                return false;

        for (t = p; *t; t++) {
                if (*t > 0 && *t < ' ')
                        return false;

                if (strchr("\\\"\'\0x7f", *t))
                        return false;
        }

        return true;
}

/**
 * Check if a string contains control characters. If 'ok' is non-NULL
 * it may be a string containing additional CCs to be considered OK.
 */
bool string_has_cc(const char *p, const char *ok) {
        const char *t;

        assert(p);

        for (t = p; *t; t++) {
                if (ok && strchr(ok, *t))
                        continue;

                if (*t > 0 && *t < ' ')
                        return true;

                if (*t == 127)
                        return true;
        }

        return false;
}

bool path_is_safe(const char *p) {

        if (isempty(p))
                return false;

        if (streq(p, "..") || startswith(p, "../") || endswith(p, "/..") || strstr(p, "/../"))
                return false;

        if (strlen(p) > PATH_MAX)
                return false;

        /* The following two checks are not really dangerous, but hey, they still are confusing */
        if (streq(p, ".") || startswith(p, "./") || endswith(p, "/.") || strstr(p, "/./"))
                return false;

        if (strstr(p, "//"))
                return false;

        return true;
}

/* hey glibc, APIs with callbacks without a user pointer are so useless */
void *xbsearch_r(const void *key, const void *base, size_t nmemb, size_t size,
                 int (*compar) (const void *, const void *, void *), void *arg) {
        size_t l, u, idx;
        const void *p;
        int comparison;

        l = 0;
        u = nmemb;
        while (l < u) {
                idx = (l + u) / 2;
                p = (void *)(((const char *) base) + (idx * size));
                comparison = compar(key, p, arg);
                if (comparison < 0)
                        u = idx;
                else if (comparison > 0)
                        l = idx + 1;
                else
                        return (void *)p;
        }
        return NULL;
}

void init_gettext(void) {
        setlocale(LC_ALL, "");
        textdomain(GETTEXT_PACKAGE);
}

bool is_locale_utf8(void) {
        const char *set;
        static int cached_answer = -1;

        if (cached_answer >= 0)
                goto out;

        if (!setlocale(LC_ALL, "")) {
                cached_answer = true;
                goto out;
        }

        set = nl_langinfo(CODESET);
        if (!set) {
                cached_answer = true;
                goto out;
        }

        if (streq(set, "UTF-8")) {
                cached_answer = true;
                goto out;
        }

        /* For LC_CTYPE=="C" return true, because CTYPE is effectly
         * unset and everything can do to UTF-8 nowadays. */
        set = setlocale(LC_CTYPE, NULL);
        if (!set) {
                cached_answer = true;
                goto out;
        }

        /* Check result, but ignore the result if C was set
         * explicitly. */
        cached_answer =
                streq(set, "C") &&
                !getenv("LC_ALL") &&
                !getenv("LC_CTYPE") &&
                !getenv("LANG");

out:
        return (bool) cached_answer;
}

const char *draw_special_char(DrawSpecialChar ch) {
        static const char *draw_table[2][_DRAW_SPECIAL_CHAR_MAX] = {

                /* UTF-8 */ {
                        [DRAW_TREE_VERTICAL]      = "\342\224\202 ",            /* │  */
                        [DRAW_TREE_BRANCH]        = "\342\224\234\342\224\200", /* ├─ */
                        [DRAW_TREE_RIGHT]         = "\342\224\224\342\224\200", /* └─ */
                        [DRAW_TREE_SPACE]         = "  ",                       /*    */
                        [DRAW_TRIANGULAR_BULLET]  = "\342\200\243",             /* ‣ */
                        [DRAW_BLACK_CIRCLE]       = "\342\227\217",             /* ● */
                        [DRAW_ARROW]              = "\342\206\222",             /* → */
                        [DRAW_DASH]               = "\342\200\223",             /* – */
                },

                /* ASCII fallback */ {
                        [DRAW_TREE_VERTICAL]      = "| ",
                        [DRAW_TREE_BRANCH]        = "|-",
                        [DRAW_TREE_RIGHT]         = "`-",
                        [DRAW_TREE_SPACE]         = "  ",
                        [DRAW_TRIANGULAR_BULLET]  = ">",
                        [DRAW_BLACK_CIRCLE]       = "*",
                        [DRAW_ARROW]              = "->",
                        [DRAW_DASH]               = "-",
                }
        };

        return draw_table[!is_locale_utf8()][ch];
}

char *strreplace(const char *text, const char *old_string, const char *new_string) {
        const char *f;
        char *t, *r;
        size_t l, old_len, new_len;

        assert(text);
        assert(old_string);
        assert(new_string);

        old_len = strlen(old_string);
        new_len = strlen(new_string);

        l = strlen(text);
        r = new(char, l+1);
        if (!r)
                return NULL;

        f = text;
        t = r;
        while (*f) {
                char *a;
                size_t d, nl;

                if (!startswith(f, old_string)) {
                        *(t++) = *(f++);
                        continue;
                }

                d = t - r;
                nl = l - old_len + new_len;
                a = realloc(r, nl + 1);
                if (!a)
                        goto oom;

                l = nl;
                r = a;
                t = r + d;

                t = stpcpy(t, new_string);
                f += old_len;
        }

        *t = 0;
        return r;

oom:
        free(r);
        return NULL;
}

char *strip_tab_ansi(char **ibuf, size_t *_isz) {
        const char *i, *begin = NULL;
        enum {
                STATE_OTHER,
                STATE_ESCAPE,
                STATE_BRACKET
        } state = STATE_OTHER;
        char *obuf = NULL;
        size_t osz = 0, isz;
        FILE *f;

        assert(ibuf);
        assert(*ibuf);

        /* Strips ANSI color and replaces TABs by 8 spaces */

        isz = _isz ? *_isz : strlen(*ibuf);

        f = open_memstream(&obuf, &osz);
        if (!f)
                return NULL;

        for (i = *ibuf; i < *ibuf + isz + 1; i++) {

                switch (state) {

                case STATE_OTHER:
                        if (i >= *ibuf + isz) /* EOT */
                                break;
                        else if (*i == '\x1B')
                                state = STATE_ESCAPE;
                        else if (*i == '\t')
                                fputs("        ", f);
                        else
                                fputc(*i, f);
                        break;

                case STATE_ESCAPE:
                        if (i >= *ibuf + isz) { /* EOT */
                                fputc('\x1B', f);
                                break;
                        } else if (*i == '[') {
                                state = STATE_BRACKET;
                                begin = i + 1;
                        } else {
                                fputc('\x1B', f);
                                fputc(*i, f);
                                state = STATE_OTHER;
                        }

                        break;

                case STATE_BRACKET:

                        if (i >= *ibuf + isz || /* EOT */
                            (!(*i >= '0' && *i <= '9') && *i != ';' && *i != 'm')) {
                                fputc('\x1B', f);
                                fputc('[', f);
                                state = STATE_OTHER;
                                i = begin-1;
                        } else if (*i == 'm')
                                state = STATE_OTHER;
                        break;
                }
        }

        if (ferror(f)) {
                fclose(f);
                free(obuf);
                return NULL;
        }

        fclose(f);

        free(*ibuf);
        *ibuf = obuf;

        if (_isz)
                *_isz = osz;

        return obuf;
}

int on_ac_power(void) {
        bool found_offline = false, found_online = false;
        _cleanup_closedir_ DIR *d = NULL;

        d = opendir("/sys/class/power_supply");
        if (!d)
                return errno == ENOENT ? true : -errno;

        for (;;) {
                struct dirent *de;
                _cleanup_close_ int fd = -1, device = -1;
                char contents[6];
                ssize_t n;

                errno = 0;
                de = readdir(d);
                if (!de && errno != 0)
                        return -errno;

                if (!de)
                        break;

                if (hidden_file(de->d_name))
                        continue;

                device = openat(dirfd(d), de->d_name, O_DIRECTORY|O_RDONLY|O_CLOEXEC|O_NOCTTY);
                if (device < 0) {
                        if (errno == ENOENT || errno == ENOTDIR)
                                continue;

                        return -errno;
                }

                fd = openat(device, "type", O_RDONLY|O_CLOEXEC|O_NOCTTY);
                if (fd < 0) {
                        if (errno == ENOENT)
                                continue;

                        return -errno;
                }

                n = read(fd, contents, sizeof(contents));
                if (n < 0)
                        return -errno;

                if (n != 6 || memcmp(contents, "Mains\n", 6))
                        continue;

                safe_close(fd);
                fd = openat(device, "online", O_RDONLY|O_CLOEXEC|O_NOCTTY);
                if (fd < 0) {
                        if (errno == ENOENT)
                                continue;

                        return -errno;
                }

                n = read(fd, contents, sizeof(contents));
                if (n < 0)
                        return -errno;

                if (n != 2 || contents[1] != '\n')
                        return -EIO;

                if (contents[0] == '1') {
                        found_online = true;
                        break;
                } else if (contents[0] == '0')
                        found_offline = true;
                else
                        return -EIO;
        }

        return found_online || !found_offline;
}

static int search_and_fopen_internal(const char *path, const char *mode, const char *root, char **search, FILE **_f) {
        char **i;

        assert(path);
        assert(mode);
        assert(_f);

        if (!path_strv_resolve_uniq(search, root))
                return -ENOMEM;

        STRV_FOREACH(i, search) {
                _cleanup_free_ char *p = NULL;
                FILE *f;

                if (root)
                        p = strjoin(root, *i, "/", path, NULL);
                else
                        p = strjoin(*i, "/", path, NULL);
                if (!p)
                        return -ENOMEM;

                f = fopen(p, mode);
                if (f) {
                        *_f = f;
                        return 0;
                }

                if (errno != ENOENT)
                        return -errno;
        }

        return -ENOENT;
}

int search_and_fopen(const char *path, const char *mode, const char *root, const char **search, FILE **_f) {
        _cleanup_strv_free_ char **copy = NULL;

        assert(path);
        assert(mode);
        assert(_f);

        if (path_is_absolute(path)) {
                FILE *f;

                f = fopen(path, mode);
                if (f) {
                        *_f = f;
                        return 0;
                }

                return -errno;
        }

        copy = strv_copy((char**) search);
        if (!copy)
                return -ENOMEM;

        return search_and_fopen_internal(path, mode, root, copy, _f);
}

int search_and_fopen_nulstr(const char *path, const char *mode, const char *root, const char *search, FILE **_f) {
        _cleanup_strv_free_ char **s = NULL;

        if (path_is_absolute(path)) {
                FILE *f;

                f = fopen(path, mode);
                if (f) {
                        *_f = f;
                        return 0;
                }

                return -errno;
        }

        s = strv_split_nulstr(search);
        if (!s)
                return -ENOMEM;

        return search_and_fopen_internal(path, mode, root, s, _f);
}

char *strextend(char **x, ...) {
        va_list ap;
        size_t f, l;
        char *r, *p;

        assert(x);

        l = f = *x ? strlen(*x) : 0;

        va_start(ap, x);
        for (;;) {
                const char *t;
                size_t n;

                t = va_arg(ap, const char *);
                if (!t)
                        break;

                n = strlen(t);
                if (n > ((size_t) -1) - l) {
                        va_end(ap);
                        return NULL;
                }

                l += n;
        }
        va_end(ap);

        r = realloc(*x, l+1);
        if (!r)
                return NULL;

        p = r + f;

        va_start(ap, x);
        for (;;) {
                const char *t;

                t = va_arg(ap, const char *);
                if (!t)
                        break;

                p = stpcpy(p, t);
        }
        va_end(ap);

        *p = 0;
        *x = r;

        return r + l;
}

char *strrep(const char *s, unsigned n) {
        size_t l;
        char *r, *p;
        unsigned i;

        assert(s);

        l = strlen(s);
        p = r = malloc(l * n + 1);
        if (!r)
                return NULL;

        for (i = 0; i < n; i++)
                p = stpcpy(p, s);

        *p = 0;
        return r;
}

void* greedy_realloc(void **p, size_t *allocated, size_t need, size_t size) {
        size_t a, newalloc;
        void *q;

        assert(p);
        assert(allocated);

        if (*allocated >= need)
                return *p;

        newalloc = MAX(need * 2, 64u / size);
        a = newalloc * size;

        /* check for overflows */
        if (a < size * need)
                return NULL;

        q = realloc(*p, a);
        if (!q)
                return NULL;

        *p = q;
        *allocated = newalloc;
        return q;
}

void* greedy_realloc0(void **p, size_t *allocated, size_t need, size_t size) {
        size_t prev;
        uint8_t *q;

        assert(p);
        assert(allocated);

        prev = *allocated;

        q = greedy_realloc(p, allocated, need, size);
        if (!q)
                return NULL;

        if (*allocated > prev)
                memzero(q + prev * size, (*allocated - prev) * size);

        return q;
}

bool id128_is_valid(const char *s) {
        size_t i, l;

        l = strlen(s);
        if (l == 32) {

                /* Simple formatted 128bit hex string */

                for (i = 0; i < l; i++) {
                        char c = s[i];

                        if (!(c >= '0' && c <= '9') &&
                            !(c >= 'a' && c <= 'z') &&
                            !(c >= 'A' && c <= 'Z'))
                                return false;
                }

        } else if (l == 36) {

                /* Formatted UUID */

                for (i = 0; i < l; i++) {
                        char c = s[i];

                        if ((i == 8 || i == 13 || i == 18 || i == 23)) {
                                if (c != '-')
                                        return false;
                        } else {
                                if (!(c >= '0' && c <= '9') &&
                                    !(c >= 'a' && c <= 'z') &&
                                    !(c >= 'A' && c <= 'Z'))
                                        return false;
                        }
                }

        } else
                return false;

        return true;
}

int split_pair(const char *s, const char *sep, char **l, char **r) {
        char *x, *a, *b;

        assert(s);
        assert(sep);
        assert(l);
        assert(r);

        if (isempty(sep))
                return -EINVAL;

        x = strstr(s, sep);
        if (!x)
                return -EINVAL;

        a = strndup(s, x - s);
        if (!a)
                return -ENOMEM;

        b = strdup(x + strlen(sep));
        if (!b) {
                free(a);
                return -ENOMEM;
        }

        *l = a;
        *r = b;

        return 0;
}

int shall_restore_state(void) {
        _cleanup_free_ char *value = NULL;
        int r;

        r = get_proc_cmdline_key("systemd.restore_state=", &value);
        if (r < 0)
                return r;
        if (r == 0)
                return true;

        return parse_boolean(value) != 0;
}

int proc_cmdline(char **ret) {
        assert(ret);

        if (detect_container(NULL) > 0)
                return get_process_cmdline(1, 0, false, ret);
        else
                return read_one_line_file("/proc/cmdline", ret);
}

int parse_proc_cmdline(int (*parse_item)(const char *key, const char *value)) {
        _cleanup_free_ char *line = NULL;
        const char *p;
        int r;

        assert(parse_item);

        r = proc_cmdline(&line);
        if (r < 0)
                return r;

        p = line;
        for (;;) {
                _cleanup_free_ char *word = NULL;
                char *value = NULL;

                r = unquote_first_word(&p, &word, UNQUOTE_RELAX);
                if (r < 0)
                        return r;
                if (r == 0)
                        break;

                /* Filter out arguments that are intended only for the
                 * initrd */
                if (!in_initrd() && startswith(word, "rd."))
                        continue;

                value = strchr(word, '=');
                if (value)
                        *(value++) = 0;

                r = parse_item(word, value);
                if (r < 0)
                        return r;
        }

        return 0;
}

int get_proc_cmdline_key(const char *key, char **value) {
        _cleanup_free_ char *line = NULL, *ret = NULL;
        bool found = false;
        const char *p;
        int r;

        assert(key);

        r = proc_cmdline(&line);
        if (r < 0)
                return r;

        p = line;
        for (;;) {
                _cleanup_free_ char *word = NULL;
                const char *e;

                r = unquote_first_word(&p, &word, UNQUOTE_RELAX);
                if (r < 0)
                        return r;
                if (r == 0)
                        break;

                /* Filter out arguments that are intended only for the
                 * initrd */
                if (!in_initrd() && startswith(word, "rd."))
                        continue;

                if (value) {
                        e = startswith(word, key);
                        if (!e)
                                continue;

                        r = free_and_strdup(&ret, e);
                        if (r < 0)
                                return r;

                        found = true;
                } else {
                        if (streq(word, key))
                                found = true;
                }
        }

        if (value) {
                *value = ret;
                ret = NULL;
        }

        return found;

}

int container_get_leader(const char *machine, pid_t *pid) {
        _cleanup_free_ char *s = NULL, *class = NULL;
        const char *p;
        pid_t leader;
        int r;

        assert(machine);
        assert(pid);

        p = strjoina("/run/systemd/machines/", machine);
        r = parse_env_file(p, NEWLINE, "LEADER", &s, "CLASS", &class, NULL);
        if (r == -ENOENT)
                return -EHOSTDOWN;
        if (r < 0)
                return r;
        if (!s)
                return -EIO;

        if (!streq_ptr(class, "container"))
                return -EIO;

        r = parse_pid(s, &leader);
        if (r < 0)
                return r;
        if (leader <= 1)
                return -EIO;

        *pid = leader;
        return 0;
}

int namespace_open(pid_t pid, int *pidns_fd, int *mntns_fd, int *netns_fd, int *root_fd) {
        _cleanup_close_ int pidnsfd = -1, mntnsfd = -1, netnsfd = -1;
        int rfd = -1;

        assert(pid >= 0);

        if (mntns_fd) {
                const char *mntns;

                mntns = procfs_file_alloca(pid, "ns/mnt");
                mntnsfd = open(mntns, O_RDONLY|O_NOCTTY|O_CLOEXEC);
                if (mntnsfd < 0)
                        return -errno;
        }

        if (pidns_fd) {
                const char *pidns;

                pidns = procfs_file_alloca(pid, "ns/pid");
                pidnsfd = open(pidns, O_RDONLY|O_NOCTTY|O_CLOEXEC);
                if (pidnsfd < 0)
                        return -errno;
        }

        if (netns_fd) {
                const char *netns;

                netns = procfs_file_alloca(pid, "ns/net");
                netnsfd = open(netns, O_RDONLY|O_NOCTTY|O_CLOEXEC);
                if (netnsfd < 0)
                        return -errno;
        }

        if (root_fd) {
                const char *root;

                root = procfs_file_alloca(pid, "root");
                rfd = open(root, O_RDONLY|O_NOCTTY|O_CLOEXEC|O_DIRECTORY);
                if (rfd < 0)
                        return -errno;
        }

        if (pidns_fd)
                *pidns_fd = pidnsfd;

        if (mntns_fd)
                *mntns_fd = mntnsfd;

        if (netns_fd)
                *netns_fd = netnsfd;

        if (root_fd)
                *root_fd = rfd;

        pidnsfd = mntnsfd = netnsfd = -1;

        return 0;
}

int namespace_enter(int pidns_fd, int mntns_fd, int netns_fd, int root_fd) {

        if (pidns_fd >= 0)
                if (setns(pidns_fd, CLONE_NEWPID) < 0)
                        return -errno;

        if (mntns_fd >= 0)
                if (setns(mntns_fd, CLONE_NEWNS) < 0)
                        return -errno;

        if (netns_fd >= 0)
                if (setns(netns_fd, CLONE_NEWNET) < 0)
                        return -errno;

        if (root_fd >= 0) {
                if (fchdir(root_fd) < 0)
                        return -errno;

                if (chroot(".") < 0)
                        return -errno;
        }

        if (setresgid(0, 0, 0) < 0)
                return -errno;

        if (setgroups(0, NULL) < 0)
                return -errno;

        if (setresuid(0, 0, 0) < 0)
                return -errno;

        return 0;
}

bool pid_is_unwaited(pid_t pid) {
        /* Checks whether a PID is still valid at all, including a zombie */

        if (pid <= 0)
                return false;

        if (kill(pid, 0) >= 0)
                return true;

        return errno != ESRCH;
}

bool pid_is_alive(pid_t pid) {
        int r;

        /* Checks whether a PID is still valid and not a zombie */

        if (pid <= 0)
                return false;

        r = get_process_state(pid);
        if (r == -ENOENT || r == 'Z')
                return false;

        return true;
}

int getpeercred(int fd, struct ucred *ucred) {
        socklen_t n = sizeof(struct ucred);
        struct ucred u;
        int r;

        assert(fd >= 0);
        assert(ucred);

        r = getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &u, &n);
        if (r < 0)
                return -errno;

        if (n != sizeof(struct ucred))
                return -EIO;

        /* Check if the data is actually useful and not suppressed due
         * to namespacing issues */
        if (u.pid <= 0)
                return -ENODATA;
        if (u.uid == UID_INVALID)
                return -ENODATA;
        if (u.gid == GID_INVALID)
                return -ENODATA;

        *ucred = u;
        return 0;
}

int getpeersec(int fd, char **ret) {
        socklen_t n = 64;
        char *s;
        int r;

        assert(fd >= 0);
        assert(ret);

        s = new0(char, n);
        if (!s)
                return -ENOMEM;

        r = getsockopt(fd, SOL_SOCKET, SO_PEERSEC, s, &n);
        if (r < 0) {
                free(s);

                if (errno != ERANGE)
                        return -errno;

                s = new0(char, n);
                if (!s)
                        return -ENOMEM;

                r = getsockopt(fd, SOL_SOCKET, SO_PEERSEC, s, &n);
                if (r < 0) {
                        free(s);
                        return -errno;
                }
        }

        if (isempty(s)) {
                free(s);
                return -EOPNOTSUPP;
        }

        *ret = s;
        return 0;
}

/* This is much like like mkostemp() but is subject to umask(). */
int mkostemp_safe(char *pattern, int flags) {
        _cleanup_umask_ mode_t u;
        int fd;

        assert(pattern);

        u = umask(077);

        fd = mkostemp(pattern, flags);
        if (fd < 0)
                return -errno;

        return fd;
}

int open_tmpfile(const char *path, int flags) {
        char *p;
        int fd;

        assert(path);

#ifdef O_TMPFILE
        /* Try O_TMPFILE first, if it is supported */
        fd = open(path, flags|O_TMPFILE, S_IRUSR|S_IWUSR);
        if (fd >= 0)
                return fd;
#endif

        /* Fall back to unguessable name + unlinking */
        p = strjoina(path, "/systemd-tmp-XXXXXX");

        fd = mkostemp_safe(p, flags);
        if (fd < 0)
                return fd;

        unlink(p);
        return fd;
}

int fd_warn_permissions(const char *path, int fd) {
        struct stat st;

        if (fstat(fd, &st) < 0)
                return -errno;

        if (st.st_mode & 0111)
                log_warning("Configuration file %s is marked executable. Please remove executable permission bits. Proceeding anyway.", path);

        if (st.st_mode & 0002)
                log_warning("Configuration file %s is marked world-writable. Please remove world writability permission bits. Proceeding anyway.", path);

        if (getpid() == 1 && (st.st_mode & 0044) != 0044)
                log_warning("Configuration file %s is marked world-inaccessible. This has no effect as configuration data is accessible via APIs without restrictions. Proceeding anyway.", path);

        return 0;
}

unsigned long personality_from_string(const char *p) {

        /* Parse a personality specifier. We introduce our own
         * identifiers that indicate specific ABIs, rather than just
         * hints regarding the register size, since we want to keep
         * things open for multiple locally supported ABIs for the
         * same register size. We try to reuse the ABI identifiers
         * used by libseccomp. */

#if defined(__x86_64__)

        if (streq(p, "x86"))
                return PER_LINUX32;

        if (streq(p, "x86-64"))
                return PER_LINUX;

#elif defined(__i386__)

        if (streq(p, "x86"))
                return PER_LINUX;
#endif

        /* personality(7) documents that 0xffffffffUL is used for
         * querying the current personality, hence let's use that here
         * as error indicator. */
        return 0xffffffffUL;
}

const char* personality_to_string(unsigned long p) {

#if defined(__x86_64__)

        if (p == PER_LINUX32)
                return "x86";

        if (p == PER_LINUX)
                return "x86-64";

#elif defined(__i386__)

        if (p == PER_LINUX)
                return "x86";
#endif

        return NULL;
}

uint64_t physical_memory(void) {
        long mem;

        /* We return this as uint64_t in case we are running as 32bit
         * process on a 64bit kernel with huge amounts of memory */

        mem = sysconf(_SC_PHYS_PAGES);
        assert(mem > 0);

        return (uint64_t) mem * (uint64_t) page_size();
}

void hexdump(FILE *f, const void *p, size_t s) {
        const uint8_t *b = p;
        unsigned n = 0;

        assert(s == 0 || b);

        while (s > 0) {
                size_t i;

                fprintf(f, "%04x  ", n);

                for (i = 0; i < 16; i++) {

                        if (i >= s)
                                fputs("   ", f);
                        else
                                fprintf(f, "%02x ", b[i]);

                        if (i == 7)
                                fputc(' ', f);
                }

                fputc(' ', f);

                for (i = 0; i < 16; i++) {

                        if (i >= s)
                                fputc(' ', f);
                        else
                                fputc(isprint(b[i]) ? (char) b[i] : '.', f);
                }

                fputc('\n', f);

                if (s < 16)
                        break;

                n += 16;
                b += 16;
                s -= 16;
        }
}

int update_reboot_param_file(const char *param) {
        int r = 0;

        if (param) {

                r = write_string_file(REBOOT_PARAM_FILE, param);
                if (r < 0)
                        log_error("Failed to write reboot param to "
                                  REBOOT_PARAM_FILE": %s", strerror(-r));
        } else
                unlink(REBOOT_PARAM_FILE);

        return r;
}

int umount_recursive(const char *prefix, int flags) {
        bool again;
        int n = 0, r;

        /* Try to umount everything recursively below a
         * directory. Also, take care of stacked mounts, and keep
         * unmounting them until they are gone. */

        do {
                _cleanup_fclose_ FILE *proc_self_mountinfo = NULL;

                again = false;
                r = 0;

                proc_self_mountinfo = fopen("/proc/self/mountinfo", "re");
                if (!proc_self_mountinfo)
                        return -errno;

                for (;;) {
                        _cleanup_free_ char *path = NULL, *p = NULL;
                        int k;

                        k = fscanf(proc_self_mountinfo,
                                   "%*s "       /* (1) mount id */
                                   "%*s "       /* (2) parent id */
                                   "%*s "       /* (3) major:minor */
                                   "%*s "       /* (4) root */
                                   "%ms "       /* (5) mount point */
                                   "%*s"        /* (6) mount options */
                                   "%*[^-]"     /* (7) optional fields */
                                   "- "         /* (8) separator */
                                   "%*s "       /* (9) file system type */
                                   "%*s"        /* (10) mount source */
                                   "%*s"        /* (11) mount options 2 */
                                   "%*[^\n]",   /* some rubbish at the end */
                                   &path);
                        if (k != 1) {
                                if (k == EOF)
                                        break;

                                continue;
                        }

                        p = cunescape(path);
                        if (!p)
                                return -ENOMEM;

                        if (!path_startswith(p, prefix))
                                continue;

                        if (umount2(p, flags) < 0) {
                                r = -errno;
                                continue;
                        }

                        again = true;
                        n++;

                        break;
                }

        } while (again);

        return r ? r : n;
}

static int get_mount_flags(const char *path, unsigned long *flags) {
        struct statvfs buf;

        if (statvfs(path, &buf) < 0)
                return -errno;
        *flags = buf.f_flag;
        return 0;
}

int bind_remount_recursive(const char *prefix, bool ro) {
        _cleanup_set_free_free_ Set *done = NULL;
        _cleanup_free_ char *cleaned = NULL;
        int r;

        /* Recursively remount a directory (and all its submounts)
         * read-only or read-write. If the directory is already
         * mounted, we reuse the mount and simply mark it
         * MS_BIND|MS_RDONLY (or remove the MS_RDONLY for read-write
         * operation). If it isn't we first make it one. Afterwards we
         * apply MS_BIND|MS_RDONLY (or remove MS_RDONLY) to all
         * submounts we can access, too. When mounts are stacked on
         * the same mount point we only care for each individual
         * "top-level" mount on each point, as we cannot
         * influence/access the underlying mounts anyway. We do not
         * have any effect on future submounts that might get
         * propagated, they migt be writable. This includes future
         * submounts that have been triggered via autofs. */

        cleaned = strdup(prefix);
        if (!cleaned)
                return -ENOMEM;

        path_kill_slashes(cleaned);

        done = set_new(&string_hash_ops);
        if (!done)
                return -ENOMEM;

        for (;;) {
                _cleanup_fclose_ FILE *proc_self_mountinfo = NULL;
                _cleanup_set_free_free_ Set *todo = NULL;
                bool top_autofs = false;
                char *x;
                unsigned long orig_flags;

                todo = set_new(&string_hash_ops);
                if (!todo)
                        return -ENOMEM;

                proc_self_mountinfo = fopen("/proc/self/mountinfo", "re");
                if (!proc_self_mountinfo)
                        return -errno;

                for (;;) {
                        _cleanup_free_ char *path = NULL, *p = NULL, *type = NULL;
                        int k;

                        k = fscanf(proc_self_mountinfo,
                                   "%*s "       /* (1) mount id */
                                   "%*s "       /* (2) parent id */
                                   "%*s "       /* (3) major:minor */
                                   "%*s "       /* (4) root */
                                   "%ms "       /* (5) mount point */
                                   "%*s"        /* (6) mount options (superblock) */
                                   "%*[^-]"     /* (7) optional fields */
                                   "- "         /* (8) separator */
                                   "%ms "       /* (9) file system type */
                                   "%*s"        /* (10) mount source */
                                   "%*s"        /* (11) mount options (bind mount) */
                                   "%*[^\n]",   /* some rubbish at the end */
                                   &path,
                                   &type);
                        if (k != 2) {
                                if (k == EOF)
                                        break;

                                continue;
                        }

                        p = cunescape(path);
                        if (!p)
                                return -ENOMEM;

                        /* Let's ignore autofs mounts.  If they aren't
                         * triggered yet, we want to avoid triggering
                         * them, as we don't make any guarantees for
                         * future submounts anyway.  If they are
                         * already triggered, then we will find
                         * another entry for this. */
                        if (streq(type, "autofs")) {
                                top_autofs = top_autofs || path_equal(cleaned, p);
                                continue;
                        }

                        if (path_startswith(p, cleaned) &&
                            !set_contains(done, p)) {

                                r = set_consume(todo, p);
                                p = NULL;

                                if (r == -EEXIST)
                                        continue;
                                if (r < 0)
                                        return r;
                        }
                }

                /* If we have no submounts to process anymore and if
                 * the root is either already done, or an autofs, we
                 * are done */
                if (set_isempty(todo) &&
                    (top_autofs || set_contains(done, cleaned)))
                        return 0;

                if (!set_contains(done, cleaned) &&
                    !set_contains(todo, cleaned)) {
                        /* The prefix directory itself is not yet a
                         * mount, make it one. */
                        if (mount(cleaned, cleaned, NULL, MS_BIND|MS_REC, NULL) < 0)
                                return -errno;

                        orig_flags = 0;
                        (void) get_mount_flags(cleaned, &orig_flags);
                        orig_flags &= ~MS_RDONLY;

                        if (mount(NULL, prefix, NULL, orig_flags|MS_BIND|MS_REMOUNT|(ro ? MS_RDONLY : 0), NULL) < 0)
                                return -errno;

                        x = strdup(cleaned);
                        if (!x)
                                return -ENOMEM;

                        r = set_consume(done, x);
                        if (r < 0)
                                return r;
                }

                while ((x = set_steal_first(todo))) {

                        r = set_consume(done, x);
                        if (r == -EEXIST)
                                continue;
                        if (r < 0)
                                return r;

                        /* Try to reuse the original flag set, but
                         * don't care for errors, in case of
                         * obstructed mounts */
                        orig_flags = 0;
                        (void) get_mount_flags(x, &orig_flags);
                        orig_flags &= ~MS_RDONLY;

                        if (mount(NULL, x, NULL, orig_flags|MS_BIND|MS_REMOUNT|(ro ? MS_RDONLY : 0), NULL) < 0) {

                                /* Deal with mount points that are
                                 * obstructed by a later mount */

                                if (errno != ENOENT)
                                        return -errno;
                        }

                }
        }
}

int fflush_and_check(FILE *f) {
        assert(f);

        errno = 0;
        fflush(f);

        if (ferror(f))
                return errno ? -errno : -EIO;

        return 0;
}

int tempfn_xxxxxx(const char *p, char **ret) {
        const char *fn;
        char *t;

        assert(p);
        assert(ret);

        /*
         * Turns this:
         *         /foo/bar/waldo
         *
         * Into this:
         *         /foo/bar/.#waldoXXXXXX
         */

        fn = basename(p);
        if (!filename_is_valid(fn))
                return -EINVAL;

        t = new(char, strlen(p) + 2 + 6 + 1);
        if (!t)
                return -ENOMEM;

        strcpy(stpcpy(stpcpy(mempcpy(t, p, fn - p), ".#"), fn), "XXXXXX");

        *ret = path_kill_slashes(t);
        return 0;
}

int tempfn_random(const char *p, char **ret) {
        const char *fn;
        char *t, *x;
        uint64_t u;
        unsigned i;

        assert(p);
        assert(ret);

        /*
         * Turns this:
         *         /foo/bar/waldo
         *
         * Into this:
         *         /foo/bar/.#waldobaa2a261115984a9
         */

        fn = basename(p);
        if (!filename_is_valid(fn))
                return -EINVAL;

        t = new(char, strlen(p) + 2 + 16 + 1);
        if (!t)
                return -ENOMEM;

        x = stpcpy(stpcpy(mempcpy(t, p, fn - p), ".#"), fn);

        u = random_u64();
        for (i = 0; i < 16; i++) {
                *(x++) = hexchar(u & 0xF);
                u >>= 4;
        }

        *x = 0;

        *ret = path_kill_slashes(t);
        return 0;
}

int tempfn_random_child(const char *p, char **ret) {
        char *t, *x;
        uint64_t u;
        unsigned i;

        assert(p);
        assert(ret);

        /* Turns this:
         *         /foo/bar/waldo
         * Into this:
         *         /foo/bar/waldo/.#3c2b6219aa75d7d0
         */

        t = new(char, strlen(p) + 3 + 16 + 1);
        if (!t)
                return -ENOMEM;

        x = stpcpy(stpcpy(t, p), "/.#");

        u = random_u64();
        for (i = 0; i < 16; i++) {
                *(x++) = hexchar(u & 0xF);
                u >>= 4;
        }

        *x = 0;

        *ret = path_kill_slashes(t);
        return 0;
}

/* make sure the hostname is not "localhost" */
bool is_localhost(const char *hostname) {
        assert(hostname);

        /* This tries to identify local host and domain names
         * described in RFC6761 plus the redhatism of .localdomain */

        return streq(hostname, "localhost") ||
               streq(hostname, "localhost.") ||
               streq(hostname, "localdomain.") ||
               streq(hostname, "localdomain") ||
               endswith(hostname, ".localhost") ||
               endswith(hostname, ".localhost.") ||
               endswith(hostname, ".localdomain") ||
               endswith(hostname, ".localdomain.");
}

int take_password_lock(const char *root) {

        struct flock flock = {
                .l_type = F_WRLCK,
                .l_whence = SEEK_SET,
                .l_start = 0,
                .l_len = 0,
        };

        const char *path;
        int fd, r;

        /* This is roughly the same as lckpwdf(), but not as awful. We
         * don't want to use alarm() and signals, hence we implement
         * our own trivial version of this.
         *
         * Note that shadow-utils also takes per-database locks in
         * addition to lckpwdf(). However, we don't given that they
         * are redundant as they they invoke lckpwdf() first and keep
         * it during everything they do. The per-database locks are
         * awfully racy, and thus we just won't do them. */

        if (root)
                path = strjoina(root, "/etc/.pwd.lock");
        else
                path = "/etc/.pwd.lock";

        fd = open(path, O_WRONLY|O_CREAT|O_CLOEXEC|O_NOCTTY|O_NOFOLLOW, 0600);
        if (fd < 0)
                return -errno;

        r = fcntl(fd, F_SETLKW, &flock);
        if (r < 0) {
                safe_close(fd);
                return -errno;
        }

        return fd;
}

int is_symlink(const char *path) {
        struct stat info;

        if (lstat(path, &info) < 0)
                return -errno;

        return !!S_ISLNK(info.st_mode);
}

int is_dir(const char* path, bool follow) {
        struct stat st;
        int r;

        if (follow)
                r = stat(path, &st);
        else
                r = lstat(path, &st);
        if (r < 0)
                return -errno;

        return !!S_ISDIR(st.st_mode);
}

int unquote_first_word(const char **p, char **ret, UnquoteFlags flags) {
        _cleanup_free_ char *s = NULL;
        size_t allocated = 0, sz = 0;
        int r;

        enum {
                START,
                VALUE,
                VALUE_ESCAPE,
                SINGLE_QUOTE,
                SINGLE_QUOTE_ESCAPE,
                DOUBLE_QUOTE,
                DOUBLE_QUOTE_ESCAPE,
                SPACE,
        } state = START;

        assert(p);
        assert(*p);
        assert(ret);

        /* Parses the first word of a string, and returns it in
         * *ret. Removes all quotes in the process. When parsing fails
         * (because of an uneven number of quotes or similar), leaves
         * the pointer *p at the first invalid character. */

        for (;;) {
                char c = **p;

                switch (state) {

                case START:
                        if (c == 0)
                                goto finish;
                        else if (strchr(WHITESPACE, c))
                                break;

                        state = VALUE;
                        /* fallthrough */

                case VALUE:
                        if (c == 0)
                                goto finish;
                        else if (c == '\'')
                                state = SINGLE_QUOTE;
                        else if (c == '\\')
                                state = VALUE_ESCAPE;
                        else if (c == '\"')
                                state = DOUBLE_QUOTE;
                        else if (strchr(WHITESPACE, c))
                                state = SPACE;
                        else {
                                if (!GREEDY_REALLOC(s, allocated, sz+2))
                                        return -ENOMEM;

                                s[sz++] = c;
                        }

                        break;

                case VALUE_ESCAPE:
                        if (c == 0) {
                                if (flags & UNQUOTE_RELAX)
                                        goto finish;
                                return -EINVAL;
                        }

                        if (!GREEDY_REALLOC(s, allocated, sz+2))
                                return -ENOMEM;

                        if (flags & UNQUOTE_CUNESCAPE) {
                                r = cunescape_one(*p, (size_t) -1, &c);
                                if (r < 0)
                                        return -EINVAL;

                                (*p) += r - 1;
                        }

                        s[sz++] = c;
                        state = VALUE;

                        break;

                case SINGLE_QUOTE:
                        if (c == 0) {
                                if (flags & UNQUOTE_RELAX)
                                        goto finish;
                                return -EINVAL;
                        } else if (c == '\'')
                                state = VALUE;
                        else if (c == '\\')
                                state = SINGLE_QUOTE_ESCAPE;
                        else {
                                if (!GREEDY_REALLOC(s, allocated, sz+2))
                                        return -ENOMEM;

                                s[sz++] = c;
                        }

                        break;

                case SINGLE_QUOTE_ESCAPE:
                        if (c == 0) {
                                if (flags & UNQUOTE_RELAX)
                                        goto finish;
                                return -EINVAL;
                        }

                        if (!GREEDY_REALLOC(s, allocated, sz+2))
                                return -ENOMEM;

                        if (flags & UNQUOTE_CUNESCAPE) {
                                r = cunescape_one(*p, (size_t) -1, &c);
                                if (r < 0)
                                        return -EINVAL;

                                (*p) += r - 1;
                        }

                        s[sz++] = c;
                        state = SINGLE_QUOTE;
                        break;

                case DOUBLE_QUOTE:
                        if (c == 0)
                                return -EINVAL;
                        else if (c == '\"')
                                state = VALUE;
                        else if (c == '\\')
                                state = DOUBLE_QUOTE_ESCAPE;
                        else {
                                if (!GREEDY_REALLOC(s, allocated, sz+2))
                                        return -ENOMEM;

                                s[sz++] = c;
                        }

                        break;

                case DOUBLE_QUOTE_ESCAPE:
                        if (c == 0) {
                                if (flags & UNQUOTE_RELAX)
                                        goto finish;
                                return -EINVAL;
                        }

                        if (!GREEDY_REALLOC(s, allocated, sz+2))
                                return -ENOMEM;

                        if (flags & UNQUOTE_CUNESCAPE) {
                                r = cunescape_one(*p, (size_t) -1, &c);
                                if (r < 0)
                                        return -EINVAL;

                                (*p) += r - 1;
                        }

                        s[sz++] = c;
                        state = DOUBLE_QUOTE;
                        break;

                case SPACE:
                        if (c == 0)
                                goto finish;
                        if (!strchr(WHITESPACE, c))
                                goto finish;

                        break;
                }

                (*p) ++;
        }

finish:
        if (!s) {
                *ret = NULL;
                return 0;
        }

        s[sz] = 0;
        *ret = s;
        s = NULL;

        return 1;
}

int unquote_many_words(const char **p, UnquoteFlags flags, ...) {
        va_list ap;
        char **l;
        int n = 0, i, c, r;

        /* Parses a number of words from a string, stripping any
         * quotes if necessary. */

        assert(p);

        /* Count how many words are expected */
        va_start(ap, flags);
        for (;;) {
                if (!va_arg(ap, char **))
                        break;
                n++;
        }
        va_end(ap);

        if (n <= 0)
                return 0;

        /* Read all words into a temporary array */
        l = newa0(char*, n);
        for (c = 0; c < n; c++) {

                r = unquote_first_word(p, &l[c], flags);
                if (r < 0) {
                        int j;

                        for (j = 0; j < c; j++)
                                free(l[j]);

                        return r;
                }

                if (r == 0)
                        break;
        }

        /* If we managed to parse all words, return them in the passed
         * in parameters */
        va_start(ap, flags);
        for (i = 0; i < n; i++) {
                char **v;

                v = va_arg(ap, char **);
                assert(v);

                *v = l[i];
        }
        va_end(ap);

        return c;
}

int free_and_strdup(char **p, const char *s) {
        char *t;

        assert(p);

        /* Replaces a string pointer with an strdup()ed new string,
         * possibly freeing the old one. */

        if (s) {
                t = strdup(s);
                if (!t)
                        return -ENOMEM;
        } else
                t = NULL;

        free(*p);
        *p = t;

        return 0;
}

int sethostname_idempotent(const char *s) {
        int r;
        char buf[HOST_NAME_MAX + 1] = {};

        assert(s);

        r = gethostname(buf, sizeof(buf));
        if (r < 0)
                return -errno;

        if (streq(buf, s))
                return 0;

        r = sethostname(s, strlen(s));
        if (r < 0)
                return -errno;

        return 1;
}

int ptsname_malloc(int fd, char **ret) {
        size_t l = 100;

        assert(fd >= 0);
        assert(ret);

        for (;;) {
                char *c;

                c = new(char, l);
                if (!c)
                        return -ENOMEM;

                if (ptsname_r(fd, c, l) == 0) {
                        *ret = c;
                        return 0;
                }
                if (errno != ERANGE) {
                        free(c);
                        return -errno;
                }

                free(c);
                l *= 2;
        }
}

int openpt_in_namespace(pid_t pid, int flags) {
        _cleanup_close_ int pidnsfd = -1, mntnsfd = -1, rootfd = -1;
        _cleanup_close_pair_ int pair[2] = { -1, -1 };
        union {
                struct cmsghdr cmsghdr;
                uint8_t buf[CMSG_SPACE(sizeof(int))];
        } control = {};
        struct msghdr mh = {
                .msg_control = &control,
                .msg_controllen = sizeof(control),
        };
        struct cmsghdr *cmsg;
        siginfo_t si;
        pid_t child;
        int r;

        assert(pid > 0);

        r = namespace_open(pid, &pidnsfd, &mntnsfd, NULL, &rootfd);
        if (r < 0)
                return r;

        if (socketpair(AF_UNIX, SOCK_DGRAM, 0, pair) < 0)
                return -errno;

        child = fork();
        if (child < 0)
                return -errno;

        if (child == 0) {
                int master;

                pair[0] = safe_close(pair[0]);

                r = namespace_enter(pidnsfd, mntnsfd, -1, rootfd);
                if (r < 0)
                        _exit(EXIT_FAILURE);

                master = posix_openpt(flags);
                if (master < 0)
                        _exit(EXIT_FAILURE);

                cmsg = CMSG_FIRSTHDR(&mh);
                cmsg->cmsg_level = SOL_SOCKET;
                cmsg->cmsg_type = SCM_RIGHTS;
                cmsg->cmsg_len = CMSG_LEN(sizeof(int));
                memcpy(CMSG_DATA(cmsg), &master, sizeof(int));

                mh.msg_controllen = cmsg->cmsg_len;

                if (sendmsg(pair[1], &mh, MSG_NOSIGNAL) < 0)
                        _exit(EXIT_FAILURE);

                _exit(EXIT_SUCCESS);
        }

        pair[1] = safe_close(pair[1]);

        r = wait_for_terminate(child, &si);
        if (r < 0)
                return r;
        if (si.si_code != CLD_EXITED || si.si_status != EXIT_SUCCESS)
                return -EIO;

        if (recvmsg(pair[0], &mh, MSG_NOSIGNAL|MSG_CMSG_CLOEXEC) < 0)
                return -errno;

        for (cmsg = CMSG_FIRSTHDR(&mh); cmsg; cmsg = CMSG_NXTHDR(&mh, cmsg))
                if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
                        int *fds;
                        unsigned n_fds;

                        fds = (int*) CMSG_DATA(cmsg);
                        n_fds = (cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(int);

                        if (n_fds != 1) {
                                close_many(fds, n_fds);
                                return -EIO;
                        }

                        return fds[0];
                }

        return -EIO;
}

ssize_t fgetxattrat_fake(int dirfd, const char *filename, const char *attribute, void *value, size_t size, int flags) {
        _cleanup_close_ int fd = -1;
        ssize_t l;

        /* The kernel doesn't have a fgetxattrat() command, hence let's emulate one */

        fd = openat(dirfd, filename, O_RDONLY|O_CLOEXEC|O_NOCTTY|O_NOATIME|(flags & AT_SYMLINK_NOFOLLOW ? O_NOFOLLOW : 0));
        if (fd < 0)
                return -errno;

        l = fgetxattr(fd, attribute, value, size);
        if (l < 0)
                return -errno;

        return l;
}

static int parse_crtime(le64_t le, usec_t *usec) {
        uint64_t u;

        assert(usec);

        u = le64toh(le);
        if (u == 0 || u == (uint64_t) -1)
                return -EIO;

        *usec = (usec_t) u;
        return 0;
}

int fd_getcrtime(int fd, usec_t *usec) {
        le64_t le;
        ssize_t n;

        assert(fd >= 0);
        assert(usec);

        /* Until Linux gets a real concept of birthtime/creation time,
         * let's fake one with xattrs */

        n = fgetxattr(fd, "user.crtime_usec", &le, sizeof(le));
        if (n < 0)
                return -errno;
        if (n != sizeof(le))
                return -EIO;

        return parse_crtime(le, usec);
}

int fd_getcrtime_at(int dirfd, const char *name, usec_t *usec, int flags) {
        le64_t le;
        ssize_t n;

        n = fgetxattrat_fake(dirfd, name, "user.crtime_usec", &le, sizeof(le), flags);
        if (n < 0)
                return -errno;
        if (n != sizeof(le))
                return -EIO;

        return parse_crtime(le, usec);
}

int path_getcrtime(const char *p, usec_t *usec) {
        le64_t le;
        ssize_t n;

        assert(p);
        assert(usec);

        n = getxattr(p, "user.crtime_usec", &le, sizeof(le));
        if (n < 0)
                return -errno;
        if (n != sizeof(le))
                return -EIO;

        return parse_crtime(le, usec);
}

int fd_setcrtime(int fd, usec_t usec) {
        le64_t le;

        assert(fd >= 0);

        if (usec <= 0)
                usec = now(CLOCK_REALTIME);

        le = htole64((uint64_t) usec);
        if (fsetxattr(fd, "user.crtime_usec", &le, sizeof(le), 0) < 0)
                return -errno;

        return 0;
}

int chattr_fd(int fd, bool b, unsigned mask) {
        unsigned old_attr, new_attr;

        assert(fd >= 0);

        if (mask == 0)
                return 0;

        if (ioctl(fd, FS_IOC_GETFLAGS, &old_attr) < 0)
                return -errno;

        if (b)
                new_attr = old_attr | mask;
        else
                new_attr = old_attr & ~mask;

        if (new_attr == old_attr)
                return 0;

        if (ioctl(fd, FS_IOC_SETFLAGS, &new_attr) < 0)
                return -errno;

        return 0;
}

int chattr_path(const char *p, bool b, unsigned mask) {
        _cleanup_close_ int fd = -1;

        assert(p);

        if (mask == 0)
                return 0;

        fd = open(p, O_RDONLY|O_CLOEXEC|O_NOCTTY|O_NOFOLLOW);
        if (fd < 0)
                return -errno;

        return chattr_fd(fd, b, mask);
}

int change_attr_fd(int fd, unsigned value, unsigned mask) {
        unsigned old_attr, new_attr;

        assert(fd >= 0);

        if (mask == 0)
                return 0;

        if (ioctl(fd, FS_IOC_GETFLAGS, &old_attr) < 0)
                return -errno;

        new_attr = (old_attr & ~mask) |(value & mask);

        if (new_attr == old_attr)
                return 0;

        if (ioctl(fd, FS_IOC_SETFLAGS, &new_attr) < 0)
                return -errno;

        return 0;
}

int read_attr_fd(int fd, unsigned *ret) {
        assert(fd >= 0);

        if (ioctl(fd, FS_IOC_GETFLAGS, ret) < 0)
                return -errno;

        return 0;
}

int read_attr_path(const char *p, unsigned *ret) {
        _cleanup_close_ int fd = -1;

        assert(p);
        assert(ret);

        fd = open(p, O_RDONLY|O_CLOEXEC|O_NOCTTY|O_NOFOLLOW);
        if (fd < 0)
                return -errno;

        return read_attr_fd(fd, ret);
}

int make_lock_file(const char *p, int operation, LockFile *ret) {
        _cleanup_close_ int fd = -1;
        _cleanup_free_ char *t = NULL;
        int r;

        /*
         * We use UNPOSIX locks if they are available. They have nice
         * semantics, and are mostly compatible with NFS. However,
         * they are only available on new kernels. When we detect we
         * are running on an older kernel, then we fall back to good
         * old BSD locks. They also have nice semantics, but are
         * slightly problematic on NFS, where they are upgraded to
         * POSIX locks, even though locally they are orthogonal to
         * POSIX locks.
         */

        t = strdup(p);
        if (!t)
                return -ENOMEM;

        for (;;) {
                struct flock fl = {
                        .l_type = (operation & ~LOCK_NB) == LOCK_EX ? F_WRLCK : F_RDLCK,
                        .l_whence = SEEK_SET,
                };
                struct stat st;

                fd = open(p, O_CREAT|O_RDWR|O_NOFOLLOW|O_CLOEXEC|O_NOCTTY, 0600);
                if (fd < 0)
                        return -errno;

                r = fcntl(fd, (operation & LOCK_NB) ? F_OFD_SETLK : F_OFD_SETLKW, &fl);
                if (r < 0) {

                        /* If the kernel is too old, use good old BSD locks */
                        if (errno == EINVAL)
                                r = flock(fd, operation);

                        if (r < 0)
                                return errno == EAGAIN ? -EBUSY : -errno;
                }

                /* If we acquired the lock, let's check if the file
                 * still exists in the file system. If not, then the
                 * previous exclusive owner removed it and then closed
                 * it. In such a case our acquired lock is worthless,
                 * hence try again. */

                r = fstat(fd, &st);
                if (r < 0)
                        return -errno;
                if (st.st_nlink > 0)
                        break;

                fd = safe_close(fd);
        }

        ret->path = t;
        ret->fd = fd;
        ret->operation = operation;

        fd = -1;
        t = NULL;

        return r;
}

int make_lock_file_for(const char *p, int operation, LockFile *ret) {
        const char *fn;
        char *t;

        assert(p);
        assert(ret);

        fn = basename(p);
        if (!filename_is_valid(fn))
                return -EINVAL;

        t = newa(char, strlen(p) + 2 + 4 + 1);
        stpcpy(stpcpy(stpcpy(mempcpy(t, p, fn - p), ".#"), fn), ".lck");

        return make_lock_file(t, operation, ret);
}

void release_lock_file(LockFile *f) {
        int r;

        if (!f)
                return;

        if (f->path) {

                /* If we are the exclusive owner we can safely delete
                 * the lock file itself. If we are not the exclusive
                 * owner, we can try becoming it. */

                if (f->fd >= 0 &&
                    (f->operation & ~LOCK_NB) == LOCK_SH) {
                        static const struct flock fl = {
                                .l_type = F_WRLCK,
                                .l_whence = SEEK_SET,
                        };

                        r = fcntl(f->fd, F_OFD_SETLK, &fl);
                        if (r < 0 && errno == EINVAL)
                                r = flock(f->fd, LOCK_EX|LOCK_NB);

                        if (r >= 0)
                                f->operation = LOCK_EX|LOCK_NB;
                }

                if ((f->operation & ~LOCK_NB) == LOCK_EX)
                        unlink_noerrno(f->path);

                free(f->path);
                f->path = NULL;
        }

        f->fd = safe_close(f->fd);
        f->operation = 0;
}

static size_t nul_length(const uint8_t *p, size_t sz) {
        size_t n = 0;

        while (sz > 0) {
                if (*p != 0)
                        break;

                n++;
                p++;
                sz--;
        }

        return n;
}

ssize_t sparse_write(int fd, const void *p, size_t sz, size_t run_length) {
        const uint8_t *q, *w, *e;
        ssize_t l;

        q = w = p;
        e = q + sz;
        while (q < e) {
                size_t n;

                n = nul_length(q, e - q);

                /* If there are more than the specified run length of
                 * NUL bytes, or if this is the beginning or the end
                 * of the buffer, then seek instead of write */
                if ((n > run_length) ||
                    (n > 0 && q == p) ||
                    (n > 0 && q + n >= e)) {
                        if (q > w) {
                                l = write(fd, w, q - w);
                                if (l < 0)
                                        return -errno;
                                if (l != q -w)
                                        return -EIO;
                        }

                        if (lseek(fd, n, SEEK_CUR) == (off_t) -1)
                                return -errno;

                        q += n;
                        w = q;
                } else if (n > 0)
                        q += n;
                else
                        q ++;
        }

        if (q > w) {
                l = write(fd, w, q - w);
                if (l < 0)
                        return -errno;
                if (l != q - w)
                        return -EIO;
        }

        return q - (const uint8_t*) p;
}

void sigkill_wait(pid_t *pid) {
        if (!pid)
                return;
        if (*pid <= 1)
                return;

        if (kill(*pid, SIGKILL) > 0)
                (void) wait_for_terminate(*pid, NULL);
}

int syslog_parse_priority(const char **p, int *priority, bool with_facility) {
        int a = 0, b = 0, c = 0;
        int k;

        assert(p);
        assert(*p);
        assert(priority);

        if ((*p)[0] != '<')
                return 0;

        if (!strchr(*p, '>'))
                return 0;

        if ((*p)[2] == '>') {
                c = undecchar((*p)[1]);
                k = 3;
        } else if ((*p)[3] == '>') {
                b = undecchar((*p)[1]);
                c = undecchar((*p)[2]);
                k = 4;
        } else if ((*p)[4] == '>') {
                a = undecchar((*p)[1]);
                b = undecchar((*p)[2]);
                c = undecchar((*p)[3]);
                k = 5;
        } else
                return 0;

        if (a < 0 || b < 0 || c < 0 ||
            (!with_facility && (a || b || c > 7)))
                return 0;

        if (with_facility)
                *priority = a*100 + b*10 + c;
        else
                *priority = (*priority & LOG_FACMASK) | c;

        *p += k;
        return 1;
}

ssize_t string_table_lookup(const char * const *table, size_t len, const char *key) {
        size_t i;

        if (!key)
                return -1;

        for (i = 0; i < len; ++i)
                if (streq_ptr(table[i], key))
                        return (ssize_t)i;

        return -1;
}

void cmsg_close_all(struct msghdr *mh) {
        struct cmsghdr *cmsg;

        assert(mh);

        for (cmsg = CMSG_FIRSTHDR(mh); cmsg; cmsg = CMSG_NXTHDR(mh, cmsg))
                if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS)
                        close_many((int*) CMSG_DATA(cmsg), (cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(int));
}

int rename_noreplace(int olddirfd, const char *oldpath, int newdirfd, const char *newpath) {
        struct stat buf;
        int ret;

        ret = renameat2(olddirfd, oldpath, newdirfd, newpath, RENAME_NOREPLACE);
        if (ret >= 0)
                return 0;

        /* Even though renameat2() exists since Linux 3.15, btrfs added
         * support for it later. If it is not implemented, fallback to another
         * method. */
        if (errno != EINVAL)
                return -errno;

        /* The link()/unlink() fallback does not work on directories. But
         * renameat() without RENAME_NOREPLACE gives the same semantics on
         * directories, except when newpath is an *empty* directory. This is
         * good enough. */
        ret = fstatat(olddirfd, oldpath, &buf, AT_SYMLINK_NOFOLLOW);
        if (ret >= 0 && S_ISDIR(buf.st_mode)) {
                ret = renameat(olddirfd, oldpath, newdirfd, newpath);
                return ret >= 0 ? 0 : -errno;
        }

        /* If it is not a directory, use the link()/unlink() fallback. */
        ret = linkat(olddirfd, oldpath, newdirfd, newpath, 0);
        if (ret < 0)
                return -errno;

        ret = unlinkat(olddirfd, oldpath, 0);
        if (ret < 0) {
                /* backup errno before the following unlinkat() alters it */
                ret = errno;
                (void) unlinkat(newdirfd, newpath, 0);
                errno = ret;
                return -errno;
        }

        return 0;
}
