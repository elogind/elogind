/*-*- Mode: C; c-basic-offset: 8; indent-tabs-mode: nil -*-*/

#pragma once

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

#include <alloca.h>
#include <fcntl.h>
#include <inttypes.h>
#include <time.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sched.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stddef.h>
#include <unistd.h>
#include <locale.h>
#include <mntent.h>
#include <sys/inotify.h>

#if SIZEOF_PID_T == 4
#  define PID_PRI PRIi32
#elif SIZEOF_PID_T == 2
#  define PID_PRI PRIi16
#else
#  error Unknown pid_t size
#endif
#define PID_FMT "%" PID_PRI

#if SIZEOF_UID_T == 4
#  define UID_FMT "%" PRIu32
#elif SIZEOF_UID_T == 2
#  define UID_FMT "%" PRIu16
#else
#  error Unknown uid_t size
#endif

#if SIZEOF_GID_T == 4
#  define GID_FMT "%" PRIu32
#elif SIZEOF_GID_T == 2
#  define GID_FMT "%" PRIu16
#else
#  error Unknown gid_t size
#endif

#if SIZEOF_TIME_T == 8
#  define PRI_TIME PRIi64
#elif SIZEOF_TIME_T == 4
#  define PRI_TIME PRIu32
#else
#  error Unknown time_t size
#endif

#if SIZEOF_RLIM_T == 8
#  define RLIM_FMT "%" PRIu64
#elif SIZEOF_RLIM_T == 4
#  define RLIM_FMT "%" PRIu32
#else
#  error Unknown rlim_t size
#endif

#include "macro.h"
#include "missing.h"
#include "time-util.h"

/* What is interpreted as whitespace? */
#define WHITESPACE " \t\n\r"
#define NEWLINE    "\n\r"
#define QUOTES     "\"\'"
#define COMMENTS   "#;"
#define GLOB_CHARS "*?["

/* What characters are special in the shell? */
/* must be escaped outside and inside double-quotes */
#define SHELL_NEED_ESCAPE "\"\\`$"
/* can be escaped or double-quoted */
#define SHELL_NEED_QUOTES SHELL_NEED_ESCAPE GLOB_CHARS "'()<>|&;"

#define FORMAT_BYTES_MAX 8

#define ANSI_HIGHLIGHT_ON "\x1B[1;39m"
#define ANSI_RED_ON "\x1B[31m"
#define ANSI_HIGHLIGHT_RED_ON "\x1B[1;31m"
#define ANSI_GREEN_ON "\x1B[32m"
#define ANSI_HIGHLIGHT_GREEN_ON "\x1B[1;32m"
#define ANSI_HIGHLIGHT_YELLOW_ON "\x1B[1;33m"
#define ANSI_HIGHLIGHT_BLUE_ON "\x1B[1;34m"
#define ANSI_HIGHLIGHT_OFF "\x1B[0m"
#define ANSI_ERASE_TO_END_OF_LINE "\x1B[K"

size_t page_size(void) _pure_;
#define PAGE_ALIGN(l) ALIGN_TO((l), page_size())

#define streq(a,b) (strcmp((a),(b)) == 0)
#define strneq(a, b, n) (strncmp((a), (b), (n)) == 0)
#define strcaseeq(a,b) (strcasecmp((a),(b)) == 0)
#define strncaseeq(a, b, n) (strncasecmp((a), (b), (n)) == 0)

bool streq_ptr(const char *a, const char *b) _pure_;

#define new(t, n) ((t*) malloc_multiply(sizeof(t), (n)))

#define new0(t, n) ((t*) calloc((n), sizeof(t)))

#define newa(t, n) ((t*) alloca(sizeof(t)*(n)))

#define newa0(t, n) ((t*) alloca0(sizeof(t)*(n)))

#define newdup(t, p, n) ((t*) memdup_multiply(p, sizeof(t), (n)))

#define malloc0(n) (calloc((n), 1))

static inline const char* yes_no(bool b) {
        return b ? "yes" : "no";
}

static inline const char* true_false(bool b) {
        return b ? "true" : "false";
}

static inline const char* one_zero(bool b) {
        return b ? "1" : "0";
}

static inline const char* strempty(const char *s) {
        return s ? s : "";
}

static inline const char* strnull(const char *s) {
        return s ? s : "(null)";
}

static inline const char *strna(const char *s) {
        return s ? s : "n/a";
}

static inline bool isempty(const char *p) {
        return !p || !p[0];
}

static inline char *startswith(const char *s, const char *prefix) {
        size_t l;

        l = strlen(prefix);
        if (strncmp(s, prefix, l) == 0)
                return (char*) s + l;

        return NULL;
}

static inline char *startswith_no_case(const char *s, const char *prefix) {
        size_t l;

        l = strlen(prefix);
        if (strncasecmp(s, prefix, l) == 0)
                return (char*) s + l;

        return NULL;
}

char *endswith(const char *s, const char *postfix) _pure_;

char *first_word(const char *s, const char *word) _pure_;

int close_nointr(int fd);
int safe_close(int fd);
void safe_close_pair(int p[]);

void close_many(const int fds[], unsigned n_fd);

int parse_size(const char *t, off_t base, off_t *size);

int parse_boolean(const char *v) _pure_;
int parse_pid(const char *s, pid_t* ret_pid);
int parse_uid(const char *s, uid_t* ret_uid);
#define parse_gid(s, ret_uid) parse_uid(s, ret_uid)

int safe_atou(const char *s, unsigned *ret_u);
int safe_atoi(const char *s, int *ret_i);

int safe_atollu(const char *s, unsigned long long *ret_u);
int safe_atolli(const char *s, long long int *ret_i);

int safe_atod(const char *s, double *ret_d);

int safe_atou8(const char *s, uint8_t *ret);

#if LONG_MAX == INT_MAX
static inline int safe_atolu(const char *s, unsigned long *ret_u) {
        assert_cc(sizeof(unsigned long) == sizeof(unsigned));
        return safe_atou(s, (unsigned*) ret_u);
}
static inline int safe_atoli(const char *s, long int *ret_u) {
        assert_cc(sizeof(long int) == sizeof(int));
        return safe_atoi(s, (int*) ret_u);
}
#else
static inline int safe_atolu(const char *s, unsigned long *ret_u) {
        assert_cc(sizeof(unsigned long) == sizeof(unsigned long long));
        return safe_atollu(s, (unsigned long long*) ret_u);
}
static inline int safe_atoli(const char *s, long int *ret_u) {
        assert_cc(sizeof(long int) == sizeof(long long int));
        return safe_atolli(s, (long long int*) ret_u);
}
#endif

static inline int safe_atou32(const char *s, uint32_t *ret_u) {
        assert_cc(sizeof(uint32_t) == sizeof(unsigned));
        return safe_atou(s, (unsigned*) ret_u);
}

static inline int safe_atoi32(const char *s, int32_t *ret_i) {
        assert_cc(sizeof(int32_t) == sizeof(int));
        return safe_atoi(s, (int*) ret_i);
}

static inline int safe_atou64(const char *s, uint64_t *ret_u) {
        assert_cc(sizeof(uint64_t) == sizeof(unsigned long long));
        return safe_atollu(s, (unsigned long long*) ret_u);
}

static inline int safe_atoi64(const char *s, int64_t *ret_i) {
        assert_cc(sizeof(int64_t) == sizeof(long long int));
        return safe_atolli(s, (long long int*) ret_i);
}

int safe_atou16(const char *s, uint16_t *ret);
int safe_atoi16(const char *s, int16_t *ret);

const char* split(const char **state, size_t *l, const char *separator, bool quoted);

#define FOREACH_WORD(word, length, s, state)                            \
        _FOREACH_WORD(word, length, s, WHITESPACE, false, state)

#define FOREACH_WORD_SEPARATOR(word, length, s, separator, state)       \
        _FOREACH_WORD(word, length, s, separator, false, state)

#define FOREACH_WORD_QUOTED(word, length, s, state)                     \
        _FOREACH_WORD(word, length, s, WHITESPACE, true, state)

#define _FOREACH_WORD(word, length, s, separator, quoted, state)        \
        for ((state) = (s), (word) = split(&(state), &(length), (separator), (quoted)); (word); (word) = split(&(state), &(length), (separator), (quoted)))

pid_t get_parent_of_pid(pid_t pid, pid_t *ppid);

char *strappend(const char *s, const char *suffix);
char *strnappend(const char *s, const char *suffix, size_t length);

char *replace_env(const char *format, char **env);
char **replace_env_argv(char **argv, char **env);

int readlinkat_malloc(int fd, const char *p, char **ret);
int readlink_malloc(const char *p, char **r);
int readlink_value(const char *p, char **ret);
int readlink_and_make_absolute(const char *p, char **r);
int readlink_and_canonicalize(const char *p, char **r);

int reset_all_signal_handlers(void);
int reset_signal_mask(void);

char *strstrip(char *s);
char *delete_chars(char *s, const char *bad);
char *truncate_nl(char *s);

char *file_in_same_dir(const char *path, const char *filename);

int rmdir_parents(const char *path, const char *stop);

int get_process_state(pid_t pid);
int get_process_comm(pid_t pid, char **name);
int get_process_cmdline(pid_t pid, size_t max_length, bool comm_fallback, char **line);
int get_process_exe(pid_t pid, char **name);
int get_process_uid(pid_t pid, uid_t *uid);
int get_process_gid(pid_t pid, gid_t *gid);
int get_process_capeff(pid_t pid, char **capeff);
int get_process_cwd(pid_t pid, char **cwd);
int get_process_root(pid_t pid, char **root);
int get_process_environ(pid_t pid, char **environ);

char hexchar(int x) _const_;
int unhexchar(char c) _const_;
char octchar(int x) _const_;
int unoctchar(char c) _const_;
char decchar(int x) _const_;
int undecchar(char c) _const_;

char *cescape(const char *s);
char *cunescape(const char *s);
char *cunescape_length(const char *s, size_t length);
char *cunescape_length_with_prefix(const char *s, size_t length, const char *prefix);

char *xescape(const char *s, const char *bad);

char *ascii_strlower(char *path);

bool dirent_is_file(const struct dirent *de) _pure_;
bool dirent_is_file_with_suffix(const struct dirent *de, const char *suffix) _pure_;

bool hidden_file(const char *filename) _pure_;

bool chars_intersect(const char *a, const char *b) _pure_;

int make_stdio(int fd);
int make_null_stdio(void);
int make_console_stdio(void);

int dev_urandom(void *p, size_t n);
void random_bytes(void *p, size_t n);
void initialize_srand(void);

static inline uint64_t random_u64(void) {
        uint64_t u;
        random_bytes(&u, sizeof(u));
        return u;
}

static inline uint32_t random_u32(void) {
        uint32_t u;
        random_bytes(&u, sizeof(u));
        return u;
}

/* For basic lookup tables with strictly enumerated entries */
#define _DEFINE_STRING_TABLE_LOOKUP_TO_STRING(name,type,scope)          \
        scope const char *name##_to_string(type i) {                    \
                if (i < 0 || i >= (type) ELEMENTSOF(name##_table))      \
                        return NULL;                                    \
                return name##_table[i];                                 \
        }

ssize_t string_table_lookup(const char * const *table, size_t len, const char *key);

#define _DEFINE_STRING_TABLE_LOOKUP_FROM_STRING(name,type,scope)                                \
        scope inline type name##_from_string(const char *s) {                                   \
                return (type)string_table_lookup(name##_table, ELEMENTSOF(name##_table), s);    \
        }

#define _DEFINE_STRING_TABLE_LOOKUP(name,type,scope)                    \
        _DEFINE_STRING_TABLE_LOOKUP_TO_STRING(name,type,scope)          \
        _DEFINE_STRING_TABLE_LOOKUP_FROM_STRING(name,type,scope)        \
        struct __useless_struct_to_allow_trailing_semicolon__

#define DEFINE_STRING_TABLE_LOOKUP(name,type) _DEFINE_STRING_TABLE_LOOKUP(name,type,)
#define DEFINE_PRIVATE_STRING_TABLE_LOOKUP(name,type) _DEFINE_STRING_TABLE_LOOKUP(name,type,static)
#define DEFINE_PRIVATE_STRING_TABLE_LOOKUP_TO_STRING(name,type) _DEFINE_STRING_TABLE_LOOKUP_TO_STRING(name,type,static)
#define DEFINE_PRIVATE_STRING_TABLE_LOOKUP_FROM_STRING(name,type) _DEFINE_STRING_TABLE_LOOKUP_FROM_STRING(name,type,static)

/* For string conversions where numbers are also acceptable */
#define DEFINE_STRING_TABLE_LOOKUP_WITH_FALLBACK(name,type,max)         \
        int name##_to_string_alloc(type i, char **str) {                \
                char *s;                                                \
                int r;                                                  \
                if (i < 0 || i > max)                                   \
                        return -ERANGE;                                 \
                if (i < (type) ELEMENTSOF(name##_table)) {              \
                        s = strdup(name##_table[i]);                    \
                        if (!s)                                         \
                                return log_oom();                       \
                } else {                                                \
                        r = asprintf(&s, "%i", i);                      \
                        if (r < 0)                                      \
                                return log_oom();                       \
                }                                                       \
                *str = s;                                               \
                return 0;                                               \
        }                                                               \
        type name##_from_string(const char *s) {                        \
                type i;                                                 \
                unsigned u = 0;                                         \
                assert(s);                                              \
                for (i = 0; i < (type)ELEMENTSOF(name##_table); i++)    \
                        if (name##_table[i] &&                          \
                            streq(name##_table[i], s))                  \
                                return i;                               \
                if (safe_atou(s, &u) >= 0 && u <= max)                  \
                        return (type) u;                                \
                return (type) -1;                                       \
        }                                                               \
        struct __useless_struct_to_allow_trailing_semicolon__

int fd_nonblock(int fd, bool nonblock);
int fd_cloexec(int fd, bool cloexec);

int close_all_fds(const int except[], unsigned n_except);

bool fstype_is_network(const char *fstype);

int chvt(int vt);

int read_one_char(FILE *f, char *ret, usec_t timeout, bool *need_nl);
int ask_char(char *ret, const char *replies, const char *text, ...) _printf_(3, 4);
int ask_string(char **ret, const char *text, ...) _printf_(2, 3);

int reset_terminal_fd(int fd, bool switch_to_text);
int reset_terminal(const char *name);

int open_terminal(const char *name, int mode);
int acquire_terminal(const char *name, bool fail, bool force, bool ignore_tiocstty_eperm, usec_t timeout);
int release_terminal(void);

int flush_fd(int fd);

int ignore_signals(int sig, ...);
int default_signals(int sig, ...);
int sigaction_many(const struct sigaction *sa, ...);

int fopen_temporary(const char *path, FILE **_f, char **_temp_path);

ssize_t loop_read(int fd, void *buf, size_t nbytes, bool do_poll);
int loop_read_exact(int fd, void *buf, size_t nbytes, bool do_poll);
int loop_write(int fd, const void *buf, size_t nbytes, bool do_poll);

bool is_device_path(const char *path);

int dir_is_empty(const char *path);
char* dirname_malloc(const char *path);

void rename_process(const char name[8]);

void sigset_add_many(sigset_t *ss, ...);
int sigprocmask_many(int how, ...);

bool hostname_is_set(void);

char* lookup_uid(uid_t uid);
char* gethostname_malloc(void);
char* getlogname_malloc(void);
char* getusername_malloc(void);

int getttyname_malloc(int fd, char **r);
int getttyname_harder(int fd, char **r);

int get_ctty_devnr(pid_t pid, dev_t *d);
int get_ctty(pid_t, dev_t *_devnr, char **r);

int chmod_and_chown(const char *path, mode_t mode, uid_t uid, gid_t gid);
int fchmod_and_fchown(int fd, mode_t mode, uid_t uid, gid_t gid);

int is_fd_on_temporary_fs(int fd);

int rm_rf_children(int fd, bool only_dirs, bool honour_sticky, struct stat *root_dev);
int rm_rf_children_dangerous(int fd, bool only_dirs, bool honour_sticky, struct stat *root_dev);
int rm_rf(const char *path, bool only_dirs, bool delete_root, bool honour_sticky);
int rm_rf_dangerous(const char *path, bool only_dirs, bool delete_root, bool honour_sticky);

int pipe_eof(int fd);

cpu_set_t* cpu_set_malloc(unsigned *ncpus);

int status_vprintf(const char *status, bool ellipse, bool ephemeral, const char *format, va_list ap) _printf_(4,0);
int status_printf(const char *status, bool ellipse, bool ephemeral, const char *format, ...) _printf_(4,5);

#define xsprintf(buf, fmt, ...) assert_se((size_t) snprintf(buf, ELEMENTSOF(buf), fmt, __VA_ARGS__) < ELEMENTSOF(buf))

int fd_columns(int fd);
unsigned columns(void);
int fd_lines(int fd);
unsigned lines(void);
void columns_lines_cache_reset(int _unused_ signum);

bool on_tty(void);

static inline const char *ansi_highlight(void) {
        return on_tty() ? ANSI_HIGHLIGHT_ON : "";
}

static inline const char *ansi_highlight_red(void) {
        return on_tty() ? ANSI_HIGHLIGHT_RED_ON : "";
}

static inline const char *ansi_highlight_green(void) {
        return on_tty() ? ANSI_HIGHLIGHT_GREEN_ON : "";
}

static inline const char *ansi_highlight_yellow(void) {
        return on_tty() ? ANSI_HIGHLIGHT_YELLOW_ON : "";
}

static inline const char *ansi_highlight_blue(void) {
        return on_tty() ? ANSI_HIGHLIGHT_BLUE_ON : "";
}

static inline const char *ansi_highlight_off(void) {
        return on_tty() ? ANSI_HIGHLIGHT_OFF : "";
}

int files_same(const char *filea, const char *fileb);

int running_in_chroot(void);

char *ellipsize(const char *s, size_t length, unsigned percent);
                                   /* bytes                 columns */
char *ellipsize_mem(const char *s, size_t old_length, size_t new_length, unsigned percent);

int touch_file(const char *path, bool parents, usec_t stamp, uid_t uid, gid_t gid, mode_t mode);
int touch(const char *path);

char *unquote(const char *s, const char *quotes);
char *normalize_env_assignment(const char *s);

int wait_for_terminate(pid_t pid, siginfo_t *status);
int wait_for_terminate_and_warn(const char *name, pid_t pid, bool check_exit_code);

noreturn void freeze(void);

bool null_or_empty(struct stat *st) _pure_;
int null_or_empty_path(const char *fn);
int null_or_empty_fd(int fd);

DIR *xopendirat(int dirfd, const char *name, int flags);

char *fstab_node_to_udev_node(const char *p);

char *resolve_dev_console(char **active);
bool tty_is_vc(const char *tty);
bool tty_is_vc_resolve(const char *tty);
bool tty_is_console(const char *tty) _pure_;
int vtnr_from_tty(const char *tty);
const char *default_term_for_tty(const char *tty);

void execute_directories(const char* const* directories, usec_t timeout, char *argv[]);

int kill_and_sigcont(pid_t pid, int sig);

bool nulstr_contains(const char*nulstr, const char *needle);

bool plymouth_running(void);

bool hostname_is_valid(const char *s) _pure_;
char* hostname_cleanup(char *s, bool lowercase);

bool machine_name_is_valid(const char *s) _pure_;

char* strshorten(char *s, size_t l);

int terminal_vhangup_fd(int fd);
int terminal_vhangup(const char *name);

int vt_disallocate(const char *name);

int symlink_atomic(const char *from, const char *to);
int mknod_atomic(const char *path, mode_t mode, dev_t dev);
int mkfifo_atomic(const char *path, mode_t mode);

int fchmod_umask(int fd, mode_t mode);

bool display_is_local(const char *display) _pure_;
int socket_from_display(const char *display, char **path);

int get_user_creds(const char **username, uid_t *uid, gid_t *gid, const char **home, const char **shell);
int get_group_creds(const char **groupname, gid_t *gid);

int in_gid(gid_t gid);
int in_group(const char *name);

char* uid_to_name(uid_t uid);
char* gid_to_name(gid_t gid);

int glob_exists(const char *path);
int glob_extend(char ***strv, const char *path);

int dirent_ensure_type(DIR *d, struct dirent *de);

int get_files_in_directory(const char *path, char ***list);

char *strjoin(const char *x, ...) _sentinel_;

bool is_main_thread(void);

static inline bool _pure_ in_charset(const char *s, const char* charset) {
        assert(s);
        assert(charset);
        return s[strspn(s, charset)] == '\0';
}

int block_get_whole_disk(dev_t d, dev_t *ret);

#define NULSTR_FOREACH(i, l)                                    \
        for ((i) = (l); (i) && *(i); (i) = strchr((i), 0)+1)

#define NULSTR_FOREACH_PAIR(i, j, l)                             \
        for ((i) = (l), (j) = strchr((i), 0)+1; (i) && *(i); (i) = strchr((j), 0)+1, (j) = *(i) ? strchr((i), 0)+1 : (i))

int ioprio_class_to_string_alloc(int i, char **s);
int ioprio_class_from_string(const char *s);

const char *sigchld_code_to_string(int i) _const_;
int sigchld_code_from_string(const char *s) _pure_;

int log_facility_unshifted_to_string_alloc(int i, char **s);
int log_facility_unshifted_from_string(const char *s);

int log_level_to_string_alloc(int i, char **s);
int log_level_from_string(const char *s);

int sched_policy_to_string_alloc(int i, char **s);
int sched_policy_from_string(const char *s);

const char *rlimit_to_string(int i) _const_;
int rlimit_from_string(const char *s) _pure_;

int ip_tos_to_string_alloc(int i, char **s);
int ip_tos_from_string(const char *s);

const char *signal_to_string(int i) _const_;
int signal_from_string(const char *s) _pure_;

int signal_from_string_try_harder(const char *s);

extern int saved_argc;
extern char **saved_argv;

bool kexec_loaded(void);

int prot_from_flags(int flags) _const_;

char *format_bytes(char *buf, size_t l, off_t t);

int fd_wait_for_event(int fd, int event, usec_t timeout);

void* memdup(const void *p, size_t l) _alloc_(2);

int is_kernel_thread(pid_t pid);

int fd_inc_sndbuf(int fd, size_t n);
int fd_inc_rcvbuf(int fd, size_t n);

int fork_agent(pid_t *pid, const int except[], unsigned n_except, const char *path, ...);

int setrlimit_closest(int resource, const struct rlimit *rlim);

int getenv_for_pid(pid_t pid, const char *field, char **_value);

bool http_url_is_valid(const char *url) _pure_;
bool documentation_url_is_valid(const char *url) _pure_;

bool http_etag_is_valid(const char *etag);

bool in_initrd(void);

void warn_melody(void);

int get_home_dir(char **ret);
int get_shell(char **_ret);

static inline void freep(void *p) {
        free(*(void**) p);
}

static inline void closep(int *fd) {
        safe_close(*fd);
}

static inline void umaskp(mode_t *u) {
        umask(*u);
}

static inline void close_pairp(int (*p)[2]) {
        safe_close_pair(*p);
}

DEFINE_TRIVIAL_CLEANUP_FUNC(FILE*, fclose);
DEFINE_TRIVIAL_CLEANUP_FUNC(FILE*, pclose);
DEFINE_TRIVIAL_CLEANUP_FUNC(DIR*, closedir);
DEFINE_TRIVIAL_CLEANUP_FUNC(FILE*, endmntent);

#define _cleanup_free_ _cleanup_(freep)
#define _cleanup_close_ _cleanup_(closep)
#define _cleanup_umask_ _cleanup_(umaskp)
#define _cleanup_globfree_ _cleanup_(globfree)
#define _cleanup_fclose_ _cleanup_(fclosep)
#define _cleanup_pclose_ _cleanup_(pclosep)
#define _cleanup_closedir_ _cleanup_(closedirp)
#define _cleanup_endmntent_ _cleanup_(endmntentp)
#define _cleanup_close_pair_ _cleanup_(close_pairp)

_malloc_  _alloc_(1, 2) static inline void *malloc_multiply(size_t a, size_t b) {
        if (_unlikely_(b != 0 && a > ((size_t) -1) / b))
                return NULL;

        return malloc(a * b);
}

_alloc_(2, 3) static inline void *realloc_multiply(void *p, size_t a, size_t b) {
        if (_unlikely_(b != 0 && a > ((size_t) -1) / b))
                return NULL;

        return realloc(p, a * b);
}

_alloc_(2, 3) static inline void *memdup_multiply(const void *p, size_t a, size_t b) {
        if (_unlikely_(b != 0 && a > ((size_t) -1) / b))
                return NULL;

        return memdup(p, a * b);
}

bool filename_is_valid(const char *p) _pure_;
bool path_is_safe(const char *p) _pure_;
bool string_is_safe(const char *p) _pure_;
bool string_has_cc(const char *p, const char *ok) _pure_;

/**
 * Check if a string contains any glob patterns.
 */
_pure_ static inline bool string_is_glob(const char *p) {
        return !!strpbrk(p, GLOB_CHARS);
}

void *xbsearch_r(const void *key, const void *base, size_t nmemb, size_t size,
                 int (*compar) (const void *, const void *, void *),
                 void *arg);

#define _(String) gettext (String)
void init_gettext(void);
bool is_locale_utf8(void);

typedef enum DrawSpecialChar {
        DRAW_TREE_VERTICAL,
        DRAW_TREE_BRANCH,
        DRAW_TREE_RIGHT,
        DRAW_TREE_SPACE,
        DRAW_TRIANGULAR_BULLET,
        DRAW_BLACK_CIRCLE,
        DRAW_ARROW,
        DRAW_DASH,
        _DRAW_SPECIAL_CHAR_MAX
} DrawSpecialChar;

const char *draw_special_char(DrawSpecialChar ch);

char *strreplace(const char *text, const char *old_string, const char *new_string);

char *strip_tab_ansi(char **p, size_t *l);

int on_ac_power(void);

int search_and_fopen(const char *path, const char *mode, const char *root, const char **search, FILE **_f);
int search_and_fopen_nulstr(const char *path, const char *mode, const char *root, const char *search, FILE **_f);

#define FOREACH_LINE(line, f, on_error)                         \
        for (;;)                                                \
                if (!fgets(line, sizeof(line), f)) {            \
                        if (ferror(f)) {                        \
                                on_error;                       \
                        }                                       \
                        break;                                  \
                } else

#define FOREACH_DIRENT(de, d, on_error)                                 \
        for (errno = 0, de = readdir(d);; errno = 0, de = readdir(d))   \
                if (!de) {                                              \
                        if (errno > 0) {                                \
                                on_error;                               \
                        }                                               \
                        break;                                          \
                } else if (hidden_file((de)->d_name))                   \
                        continue;                                       \
                else

#define FOREACH_DIRENT_ALL(de, d, on_error)                             \
        for (errno = 0, de = readdir(d);; errno = 0, de = readdir(d))   \
                if (!de) {                                              \
                        if (errno > 0) {                                \
                                on_error;                               \
                        }                                               \
                        break;                                          \
                } else

static inline void *mempset(void *s, int c, size_t n) {
        memset(s, c, n);
        return (uint8_t*)s + n;
}

char *hexmem(const void *p, size_t l);
void *unhexmem(const char *p, size_t l);

char *strextend(char **x, ...) _sentinel_;
char *strrep(const char *s, unsigned n);

void* greedy_realloc(void **p, size_t *allocated, size_t need, size_t size);
void* greedy_realloc0(void **p, size_t *allocated, size_t need, size_t size);
#define GREEDY_REALLOC(array, allocated, need)                          \
        greedy_realloc((void**) &(array), &(allocated), (need), sizeof((array)[0]))

#define GREEDY_REALLOC0(array, allocated, need)                         \
        greedy_realloc0((void**) &(array), &(allocated), (need), sizeof((array)[0]))

static inline void _reset_errno_(int *saved_errno) {
        errno = *saved_errno;
}

#define PROTECT_ERRNO _cleanup_(_reset_errno_) __attribute__((unused)) int _saved_errno_ = errno

static inline int negative_errno(void) {
        /* This helper should be used to shut up gcc if you know 'errno' is
         * negative. Instead of "return -errno;", use "return negative_errno();"
         * It will suppress bogus gcc warnings in case it assumes 'errno' might
         * be 0 and thus the caller's error-handling might not be triggered. */
        assert_return(errno > 0, -EINVAL);
        return -errno;
}

struct _umask_struct_ {
        mode_t mask;
        bool quit;
};

static inline void _reset_umask_(struct _umask_struct_ *s) {
        umask(s->mask);
};

#define RUN_WITH_UMASK(mask)                                            \
        for (_cleanup_(_reset_umask_) struct _umask_struct_ _saved_umask_ = { umask(mask), false }; \
             !_saved_umask_.quit ;                                      \
             _saved_umask_.quit = true)

static inline unsigned u64log2(uint64_t n) {
#if __SIZEOF_LONG_LONG__ == 8
        return (n > 1) ? (unsigned) __builtin_clzll(n) ^ 63U : 0;
#else
#error "Wut?"
#endif
}

static inline unsigned u32ctz(uint32_t n) {
#if __SIZEOF_INT__ == 4
        return __builtin_ctz(n);
#else
#error "Wut?"
#endif
}

static inline unsigned log2i(int x) {
        assert(x > 0);

        return __SIZEOF_INT__ * 8 - __builtin_clz(x) - 1;
}

static inline unsigned log2u(unsigned x) {
        assert(x > 0);

        return sizeof(unsigned) * 8 - __builtin_clz(x) - 1;
}

static inline unsigned log2u_round_up(unsigned x) {
        assert(x > 0);

        if (x == 1)
                return 0;

        return log2u(x - 1) + 1;
}

static inline bool logind_running(void) {
        return access("/run/systemd/seats/", F_OK) >= 0;
}

#define DECIMAL_STR_WIDTH(x)                            \
        ({                                              \
                typeof(x) _x_ = (x);                    \
                unsigned ans = 1;                       \
                while (_x_ /= 10)                       \
                        ans++;                          \
                ans;                                    \
        })

int unlink_noerrno(const char *path);

#define alloca0(n)                                      \
        ({                                              \
                char *_new_;                            \
                size_t _len_ = n;                       \
                _new_ = alloca(_len_);                  \
                (void *) memset(_new_, 0, _len_);       \
        })

/* It's not clear what alignment glibc/gcc alloca() guarantee, hence provide a guaranteed safe version */
#define alloca_align(size, align)                                       \
        ({                                                              \
                void *_ptr_;                                            \
                size_t _mask_ = (align) - 1;                            \
                _ptr_ = alloca((size) + _mask_);                        \
                (void*)(((uintptr_t)_ptr_ + _mask_) & ~_mask_);         \
        })

#define alloca0_align(size, align)                                      \
        ({                                                              \
                void *_new_;                                            \
                size_t _size_ = (size);                                 \
                _new_ = alloca_align(_size_, (align));                  \
                (void*)memset(_new_, 0, _size_);                        \
        })

#define strjoina(a, ...)                                                \
        ({                                                              \
                const char *_appendees_[] = { a, __VA_ARGS__ };         \
                char *_d_, *_p_;                                        \
                int _len_ = 0;                                          \
                unsigned _i_;                                           \
                for (_i_ = 0; _i_ < ELEMENTSOF(_appendees_) && _appendees_[_i_]; _i_++) \
                        _len_ += strlen(_appendees_[_i_]);              \
                _p_ = _d_ = alloca(_len_ + 1);                          \
                for (_i_ = 0; _i_ < ELEMENTSOF(_appendees_) && _appendees_[_i_]; _i_++) \
                        _p_ = stpcpy(_p_, _appendees_[_i_]);            \
                *_p_ = 0;                                               \
                _d_;                                                    \
        })

#define procfs_file_alloca(pid, field)                                  \
        ({                                                              \
                pid_t _pid_ = (pid);                                    \
                const char *_r_;                                        \
                if (_pid_ == 0) {                                       \
                        _r_ = ("/proc/self/" field);                    \
                } else {                                                \
                        _r_ = alloca(strlen("/proc/") + DECIMAL_STR_MAX(pid_t) + 1 + sizeof(field)); \
                        sprintf((char*) _r_, "/proc/"PID_FMT"/" field, _pid_);                       \
                }                                                       \
                _r_;                                                    \
        })

bool id128_is_valid(const char *s) _pure_;

int split_pair(const char *s, const char *sep, char **l, char **r);

int shall_restore_state(void);

/**
 * Normal qsort requires base to be nonnull. Here were require
 * that only if nmemb > 0.
 */
static inline void qsort_safe(void *base, size_t nmemb, size_t size,
                              int (*compar)(const void *, const void *)) {
        if (nmemb) {
                assert(base);
                qsort(base, nmemb, size, compar);
        }
}

int proc_cmdline(char **ret);
int parse_proc_cmdline(int (*parse_word)(const char *key, const char *value));
int get_proc_cmdline_key(const char *parameter, char **value);

int container_get_leader(const char *machine, pid_t *pid);

int namespace_open(pid_t pid, int *pidns_fd, int *mntns_fd, int *netns_fd, int *root_fd);
int namespace_enter(int pidns_fd, int mntns_fd, int netns_fd, int root_fd);

bool pid_is_alive(pid_t pid);
bool pid_is_unwaited(pid_t pid);

int getpeercred(int fd, struct ucred *ucred);
int getpeersec(int fd, char **ret);

int writev_safe(int fd, const struct iovec *w, int j);

int mkostemp_safe(char *pattern, int flags);
int open_tmpfile(const char *path, int flags);

int fd_warn_permissions(const char *path, int fd);

unsigned long personality_from_string(const char *p);
const char *personality_to_string(unsigned long);

uint64_t physical_memory(void);

void hexdump(FILE *f, const void *p, size_t s);

union file_handle_union {
        struct file_handle handle;
        char padding[sizeof(struct file_handle) + MAX_HANDLE_SZ];
};
#define FILE_HANDLE_INIT { .handle.handle_bytes = MAX_HANDLE_SZ }

int update_reboot_param_file(const char *param);

int umount_recursive(const char *target, int flags);

int bind_remount_recursive(const char *prefix, bool ro);

int fflush_and_check(FILE *f);

int tempfn_xxxxxx(const char *p, char **ret);
int tempfn_random(const char *p, char **ret);
int tempfn_random_child(const char *p, char **ret);

bool is_localhost(const char *hostname);

int take_password_lock(const char *root);

int is_symlink(const char *path);
int is_dir(const char *path, bool follow);

typedef enum UnquoteFlags{
        UNQUOTE_RELAX     = 1,
        UNQUOTE_CUNESCAPE = 2,
} UnquoteFlags;

int unquote_first_word(const char **p, char **ret, UnquoteFlags flags);
int unquote_many_words(const char **p, UnquoteFlags flags, ...) _sentinel_;

int free_and_strdup(char **p, const char *s);

int sethostname_idempotent(const char *s);

#define INOTIFY_EVENT_MAX (sizeof(struct inotify_event) + NAME_MAX + 1)

#define FOREACH_INOTIFY_EVENT(e, buffer, sz) \
        for ((e) = &buffer.ev;                                \
             (uint8_t*) (e) < (uint8_t*) (buffer.raw) + (sz); \
             (e) = (struct inotify_event*) ((uint8_t*) (e) + sizeof(struct inotify_event) + (e)->len))

union inotify_event_buffer {
        struct inotify_event ev;
        uint8_t raw[INOTIFY_EVENT_MAX];
};

#ifdef __GLIBC__
#define laccess(path, mode) faccessat(AT_FDCWD, (path), (mode), AT_SYMLINK_NOFOLLOW)
#else
#define laccess(path, mode) faccessat(AT_FDCWD, (path), (mode), 0)
#endif

int ptsname_malloc(int fd, char **ret);

int openpt_in_namespace(pid_t pid, int flags);

ssize_t fgetxattrat_fake(int dirfd, const char *filename, const char *attribute, void *value, size_t size, int flags);

int fd_setcrtime(int fd, usec_t usec);
int fd_getcrtime(int fd, usec_t *usec);
int path_getcrtime(const char *p, usec_t *usec);
int fd_getcrtime_at(int dirfd, const char *name, usec_t *usec, int flags);

int chattr_fd(int fd, bool b, unsigned mask);
int chattr_path(const char *p, bool b, unsigned mask);
int change_attr_fd(int fd, unsigned value, unsigned mask);

int read_attr_fd(int fd, unsigned *ret);
int read_attr_path(const char *p, unsigned *ret);

typedef struct LockFile {
        char *path;
        int fd;
        int operation;
} LockFile;

int make_lock_file(const char *p, int operation, LockFile *ret);
int make_lock_file_for(const char *p, int operation, LockFile *ret);
void release_lock_file(LockFile *f);

#define _cleanup_release_lock_file_ _cleanup_(release_lock_file)

#define LOCK_FILE_INIT { .fd = -1, .path = NULL }

#define RLIMIT_MAKE_CONST(lim) ((struct rlimit) { lim, lim })

ssize_t sparse_write(int fd, const void *p, size_t sz, size_t run_length);

void sigkill_wait(pid_t *pid);
#define _cleanup_sigkill_wait_ _cleanup_(sigkill_wait)

int syslog_parse_priority(const char **p, int *priority, bool with_facility);

void cmsg_close_all(struct msghdr *mh);

int rename_noreplace(int olddirfd, const char *oldpath, int newdirfd, const char *newpath);
