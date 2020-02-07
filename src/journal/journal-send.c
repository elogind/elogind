/* SPDX-License-Identifier: LGPL-2.1+ */

#include <errno.h>
#if 0 /// elogind is musl-libc compatible and does not directly include printf.h
#include <fcntl.h>
#include <printf.h>
#else // 0
#include "parse-printf-format.h"
#endif // 0
#include <stddef.h>
//#include <sys/socket.h>
//#include <sys/un.h>
#include <unistd.h>

#define SD_JOURNAL_SUPPRESS_LOCATION

#include "sd-journal.h"

#include "alloc-util.h"
#include "errno-util.h"
#include "fd-util.h"
#include "io-util.h"
#include "memfd-util.h"
#include "socket-util.h"
#include "stdio-util.h"
#include "string-util.h"
#include "tmpfile-util.h"
/// Additional includes needed by elogind
#include <syslog.h>

#define SNDBUF_SIZE (8*1024*1024)

#define ALLOCA_CODE_FUNC(f, func)                 \
        do {                                      \
                size_t _fl;                       \
                const char *_func = (func);       \
                char **_f = &(f);                 \
                _fl = strlen(_func) + 1;          \
                *_f = newa(char, _fl + 10);       \
                memcpy(*_f, "CODE_FUNC=", 10);    \
                memcpy(*_f + 10, _func, _fl);     \
        } while (false)

#if 0 /// UNNEEDED by elogind
/* We open a single fd, and we'll share it with the current process,
 * all its threads, and all its subprocesses. This means we need to
 * initialize it atomically, and need to operate on it atomically
 * never assuming we are the only user */

static int journal_fd(void) {
        int fd;
        static int fd_plus_one = 0;

retry:
        if (fd_plus_one > 0)
                return fd_plus_one - 1;

        fd = socket(AF_UNIX, SOCK_DGRAM|SOCK_CLOEXEC, 0);
        if (fd < 0)
                return -errno;

        fd_inc_sndbuf(fd, SNDBUF_SIZE);

        if (!__sync_bool_compare_and_swap(&fd_plus_one, 0, fd+1)) {
                safe_close(fd);
                goto retry;
        }

        return fd;
}
#endif // 0

_public_ int sd_journal_print(int priority, const char *format, ...) {
        int r;
        va_list ap;

        va_start(ap, format);
        r = sd_journal_printv(priority, format, ap);
        va_end(ap);

        return r;
}

_public_ int sd_journal_printv(int priority, const char *format, va_list ap) {

        /* FIXME: Instead of limiting things to LINE_MAX we could do a
           C99 variable-length array on the stack here in a loop. */

#if 0 /// elogind only needs a LINE_MAX buffer
        char buffer[8 + LINE_MAX], p[STRLEN("PRIORITY=") + DECIMAL_STR_MAX(int) + 1];
        struct iovec iov[2];
#else // 0
        char buffer[8 + LINE_MAX]; // We keep the +8 to not make a too big mess below.
#endif // 0

        assert_return(priority >= 0, -EINVAL);
        assert_return(priority <= 7, -EINVAL);
        assert_return(format, -EINVAL);

#if 0 /// No bells and whistles needed in elogind
        xsprintf(p, "PRIORITY=%i", priority & LOG_PRIMASK);

        memcpy(buffer, "MESSAGE=", 8);
#endif // 0
        vsnprintf(buffer+8, sizeof(buffer) - 8, format, ap);

        /* Strip trailing whitespace, keep prefix whitespace. */
        (void) strstrip(buffer);

        /* Suppress empty lines */
        if (isempty(buffer+8))
                return 0;

#if 0 /// As elogind does not talk to systemd-journal, use syslog.
        iov[0] = IOVEC_MAKE_STRING(buffer);
        iov[1] = IOVEC_MAKE_STRING(p);

        return sd_journal_sendv(iov, 2);
#else // 0
        syslog(LOG_DAEMON | priority, "%s", buffer + 8);
        return 0;
#endif // 0
}

_printf_(1, 0) static int fill_iovec_sprintf(const char *format, va_list ap, int extra, struct iovec **_iov) {
        PROTECT_ERRNO;
        int r, n = 0, i = 0, j;
        struct iovec *iov = NULL;

        assert(_iov);

        if (extra > 0) {
                n = MAX(extra * 2, extra + 4);
                iov = malloc0(n * sizeof(struct iovec));
                if (!iov) {
                        r = -ENOMEM;
                        goto fail;
                }

                i = extra;
        }

        while (format) {
                struct iovec *c;
                char *buffer;
                va_list aq;

                if (i >= n) {
                        n = MAX(i*2, 4);
                        c = realloc(iov, n * sizeof(struct iovec));
                        if (!c) {
                                r = -ENOMEM;
                                goto fail;
                        }

                        iov = c;
                }

                va_copy(aq, ap);
                if (vasprintf(&buffer, format, aq) < 0) {
                        va_end(aq);
                        r = -ENOMEM;
                        goto fail;
                }
                va_end(aq);

                VA_FORMAT_ADVANCE(format, ap);

                (void) strstrip(buffer); /* strip trailing whitespace, keep prefixing whitespace */

                iov[i++] = IOVEC_MAKE_STRING(buffer);

                format = va_arg(ap, char *);
        }

        *_iov = iov;

        return i;

fail:
        for (j = 0; j < i; j++)
                free(iov[j].iov_base);

        free(iov);

        return r;
}

_public_ int sd_journal_send(const char *format, ...) {
        int r, i, j;
        va_list ap;
        struct iovec *iov = NULL;

        va_start(ap, format);
        i = fill_iovec_sprintf(format, ap, 0, &iov);
        va_end(ap);

        if (_unlikely_(i < 0)) {
                r = i;
                goto finish;
        }

        r = sd_journal_sendv(iov, i);

finish:
        for (j = 0; j < i; j++)
                free(iov[j].iov_base);

        free(iov);

        return r;
}

_public_ int sd_journal_sendv(const struct iovec *iov, int n) {
        PROTECT_ERRNO;
#if 0 /// UNNEEDED by elogind
        int fd, r;
        _cleanup_close_ int buffer_fd = -1;
        struct iovec *w;
        uint64_t *l;
        int i, j = 0;
        static const union sockaddr_union sa = {
                .un.sun_family = AF_UNIX,
                .un.sun_path = "/run/systemd/journal/socket",
        };
        struct msghdr mh = {
                .msg_name = (struct sockaddr*) &sa.sa,
                .msg_namelen = SOCKADDR_UN_LEN(sa.un),
        };
        ssize_t k;
        bool have_syslog_identifier = false;
        bool seal = true;
#else // 0
        _cleanup_free_ char *file    = NULL;
        _cleanup_free_ char *func    = NULL;
        _cleanup_free_ char *line    = NULL;
        _cleanup_free_ char *message = NULL;
        int priority = LOG_INFO, i;
#endif // 0

        assert_return(iov, -EINVAL);
        assert_return(n > 0, -EINVAL);

#if 0 /// UNNEEDED by elogind
        w = newa(struct iovec, n * 5 + 3);
        l = newa(uint64_t, n);
#endif // 0

        for (i = 0; i < n; i++) {
#if 0 /// nl is not needed by elogind
                char *c, *nl;

#else // 0
                char *c;
#endif // 0
                if (_unlikely_(!iov[i].iov_base || iov[i].iov_len <= 1))
                        return -EINVAL;

                c = memchr(iov[i].iov_base, '=', iov[i].iov_len);
                if (_unlikely_(!c || c == iov[i].iov_base))
                        return -EINVAL;

#if 0 /// elogind only needs priority and the message.
                have_syslog_identifier = have_syslog_identifier ||
                        (c == (char *) iov[i].iov_base + 17 &&
                         startswith(iov[i].iov_base, "SYSLOG_IDENTIFIER"));

                nl = memchr(iov[i].iov_base, '\n', iov[i].iov_len);
                if (nl) {
                        if (_unlikely_(nl < c))
                                return -EINVAL;

                        /* Already includes a newline? Bummer, then
                         * let's write the variable name, then a
                         * newline, then the size (64bit LE), followed
                         * by the data and a final newline */

                        w[j++] = IOVEC_MAKE(iov[i].iov_base, c - (char*) iov[i].iov_base);
                        w[j++] = IOVEC_MAKE_STRING("\n");

                        l[i] = htole64(iov[i].iov_len - (c - (char*) iov[i].iov_base) - 1);
                        w[j++] = IOVEC_MAKE(&l[i], sizeof(uint64_t));

                        w[j++] = IOVEC_MAKE(c + 1, iov[i].iov_len - (c - (char*) iov[i].iov_base) - 1);
                } else
                        /* Nothing special? Then just add the line and
                         * append a newline */
                        w[j++] = iov[i];

                w[j++] = IOVEC_MAKE_STRING("\n");
#else // 0
                if ( startswith(iov[i].iov_base, "PRIORITY=") ) {
                        if (1 != sscanf(iov[i].iov_base, "PRIORITY=%i", &priority))
                                priority = LOG_NOTICE;
                } else if ( (c = startswith(iov[i].iov_base, "CODE_FILE=")) )
                        file = strdup(c);
                else if ( (c = startswith(iov[i].iov_base, "CODE_FUNC=")) )
                        func = strdup(c);
                else if ( (c = startswith(iov[i].iov_base, "CODE_LINE=")) )
                        line = strdup(c);
                else if ( (c = startswith(iov[i].iov_base, "MESSAGE=")) )
                        message = strdup(c);
#endif // 0
        }

#if 0 /// UNNEEDED by elogind
        if (!have_syslog_identifier &&
            string_is_safe(program_invocation_short_name)) {

                /* Implicitly add program_invocation_short_name, if it
                 * is not set explicitly. We only do this for
                 * program_invocation_short_name, and nothing else
                 * since everything else is much nicer to retrieve
                 * from the outside. */

                w[j++] = IOVEC_MAKE_STRING("SYSLOG_IDENTIFIER=");
                w[j++] = IOVEC_MAKE_STRING(program_invocation_short_name);
                w[j++] = IOVEC_MAKE_STRING("\n");
        }
#endif // 0

#if 0 /// elogind does not send to systemd-journal, but to syslog instead
        fd = journal_fd();
        if (_unlikely_(fd < 0))
                return fd;

        mh.msg_iov = w;
        mh.msg_iovlen = j;

        k = sendmsg(fd, &mh, MSG_NOSIGNAL);
        if (k >= 0)
                return 0;

        /* Fail silently if the journal is not available */
        if (errno == ENOENT)
                return 0;

        if (!IN_SET(errno, EMSGSIZE, ENOBUFS))
                return -errno;

        /* Message doesn't fit... Let's dump the data in a memfd or
         * temporary file and just pass a file descriptor of it to the
         * other side.
         *
         * For the temporary files we use /dev/shm instead of /tmp
         * here, since we want this to be a tmpfs, and one that is
         * available from early boot on and where unprivileged users
         * can create files. */
        buffer_fd = memfd_new(NULL);
        if (buffer_fd < 0) {
                if (buffer_fd == -ENOSYS) {
                        buffer_fd = open_tmpfile_unlinkable("/dev/shm", O_RDWR | O_CLOEXEC);
                        if (buffer_fd < 0)
                                return buffer_fd;

                        seal = false;
                } else
                        return buffer_fd;
        }

        n = writev(buffer_fd, w, j);
        if (n < 0)
                return -errno;

        if (seal) {
                r = memfd_set_sealed(buffer_fd);
                if (r < 0)
                        return r;
        }

        r = send_one_fd_sa(fd, buffer_fd, mh.msg_name, mh.msg_namelen, 0);
        if (r == -ENOENT)
                /* Fail silently if the journal is not available */
                return 0;
        return r;
#else // 0
        if (message) {
                if (file || func || line)
                        return sd_journal_print_with_location(priority, file,
                                                              line, func, "%s",
                                                              message);
                return sd_journal_print(priority, "%s", message);
        }

        return 0;
#endif // 0
}

#if 0 /// UNNEEDED by elogind
static int fill_iovec_perror_and_send(const char *message, int skip, struct iovec iov[]) {
        PROTECT_ERRNO;
        size_t n, k;

        k = isempty(message) ? 0 : strlen(message) + 2;
        n = 8 + k + 256 + 1;

        for (;;) {
                char buffer[n];
                char* j;

                errno = 0;
                j = strerror_r(_saved_errno_, buffer + 8 + k, n - 8 - k);
                if (errno == 0) {
                        char error[STRLEN("ERRNO=") + DECIMAL_STR_MAX(int) + 1];

                        if (j != buffer + 8 + k)
                                memmove(buffer + 8 + k, j, strlen(j)+1);

                        memcpy(buffer, "MESSAGE=", 8);

                        if (k > 0) {
                                memcpy(buffer + 8, message, k - 2);
                                memcpy(buffer + 8 + k - 2, ": ", 2);
                        }

                        xsprintf(error, "ERRNO=%i", _saved_errno_);

                        assert_cc(3 == LOG_ERR);
                        iov[skip+0] = IOVEC_MAKE_STRING("PRIORITY=3");
                        iov[skip+1] = IOVEC_MAKE_STRING(buffer);
                        iov[skip+2] = IOVEC_MAKE_STRING(error);

                        return sd_journal_sendv(iov, skip + 3);
                }

                if (errno != ERANGE)
                        return -errno;

                n *= 2;
        }
}
#endif // 0

_public_ int sd_journal_perror(const char *message) {
#if 0 /// No real journald in elogind, so use the standard print instead
        struct iovec iovec[3];

        return fill_iovec_perror_and_send(message, 0, iovec);
#else // 0
        if(message && *message)
                return sd_journal_print(LOG_ERR, "%s: %s", message, strerror(errno));
        else
                return sd_journal_print(LOG_ERR, "%s", strerror(errno));

        return 0;
#endif // 0
}

_public_ int sd_journal_stream_fd(const char *identifier, int priority, int level_prefix) {
#if 0 /// elogind is not journald and can not stream fds in the latter.
        static const union sockaddr_union sa = {
                .un.sun_family = AF_UNIX,
                .un.sun_path = "/run/systemd/journal/stdout",
        };
        _cleanup_close_ int fd = -1;
        char *header;
        size_t l;
        int r;

        assert_return(priority >= 0, -EINVAL);
        assert_return(priority <= 7, -EINVAL);

        fd = socket(AF_UNIX, SOCK_STREAM|SOCK_CLOEXEC, 0);
        if (fd < 0)
                return -errno;

        r = connect(fd, &sa.sa, SOCKADDR_UN_LEN(sa.un));
        if (r < 0)
                return -errno;

        if (shutdown(fd, SHUT_RD) < 0)
                return -errno;

        (void) fd_inc_sndbuf(fd, SNDBUF_SIZE);

        identifier = strempty(identifier);

        l = strlen(identifier);
        header = newa(char, l + 1 + 1 + 2 + 2 + 2 + 2 + 2);

        memcpy(header, identifier, l);
        header[l++] = '\n';
        header[l++] = '\n'; /* unit id */
        header[l++] = '0' + priority;
        header[l++] = '\n';
        header[l++] = '0' + !!level_prefix;
        header[l++] = '\n';
        header[l++] = '0';
        header[l++] = '\n';
        header[l++] = '0';
        header[l++] = '\n';
        header[l++] = '0';
        header[l++] = '\n';

        r = loop_write(fd, header, l, false);
        if (r < 0)
                return r;

        return TAKE_FD(fd);
#else // 0
        return -ENOSYS;
#endif // 0
}

_public_ int sd_journal_print_with_location(int priority, const char *file, const char *line, const char *func, const char *format, ...) {
        int r;
        va_list ap;

        va_start(ap, format);
        r = sd_journal_printv_with_location(priority, file, line, func, format, ap);
        va_end(ap);

        return r;
}

_public_ int sd_journal_printv_with_location(int priority, const char *file, const char *line, const char *func, const char *format, va_list ap) {
#if 0 /// UNNEEDED by elogind
        char buffer[8 + LINE_MAX], p[STRLEN("PRIORITY=") + DECIMAL_STR_MAX(int) + 1];
        struct iovec iov[5];
        char *f;
#endif // 0

        assert_return(priority >= 0, -EINVAL);
        assert_return(priority <= 7, -EINVAL);
        assert_return(format, -EINVAL);

#if 0 /// As elogind sends to syslog anyway, take a shortcut here
        xsprintf(p, "PRIORITY=%i", priority & LOG_PRIMASK);

        memcpy(buffer, "MESSAGE=", 8);
        vsnprintf(buffer+8, sizeof(buffer) - 8, format, ap);

        /* Strip trailing whitespace, keep prefixing whitespace */
        (void) strstrip(buffer);

        /* Suppress empty lines */
        if (isempty(buffer+8))
                return 0;

        /* func is initialized from __func__ which is not a macro, but
         * a static const char[], hence cannot easily be prefixed with
         * CODE_FUNC=, hence let's do it manually here. */
        ALLOCA_CODE_FUNC(f, func);

        iov[0] = IOVEC_MAKE_STRING(buffer);
        iov[1] = IOVEC_MAKE_STRING(p);
        iov[2] = IOVEC_MAKE_STRING(file);
        iov[3] = IOVEC_MAKE_STRING(line);
        iov[4] = IOVEC_MAKE_STRING(f);

        return sd_journal_sendv(iov, ELEMENTSOF(iov));
#else // 0
        char buffer[8 + LINE_MAX];
        vsnprintf(buffer, sizeof(buffer), format, ap);

        /* Strip trailing whitespace, keep prefixing whitespace */
        (void) strstrip(buffer);

        /* Suppress empty lines */
        if (isempty(buffer))
                return 0;

        /* just prefix with the data we have */
        return sd_journal_print(priority, "%s:%s:%s:%s", strna(file), strna(line), strna(func), buffer);
#endif // 0
}

_public_ int sd_journal_send_with_location(const char *file, const char *line, const char *func, const char *format, ...) {
        _cleanup_free_ struct iovec *iov = NULL;
        int r, i, j;
        va_list ap;
#if 0 /// UNNEEDED by elogind
        char *f;
#endif // 0

        va_start(ap, format);
        i = fill_iovec_sprintf(format, ap, 3, &iov);
        va_end(ap);

        if (_unlikely_(i < 0)) {
                r = i;
                goto finish;
        }

#if 0 /// If elogind splits this again in sd_journal_sendv, file/line/func will be lost.
        ALLOCA_CODE_FUNC(f, func);

        iov[0] = IOVEC_MAKE_STRING(file);
        iov[1] = IOVEC_MAKE_STRING(line);
        iov[2] = IOVEC_MAKE_STRING(f);

        r = sd_journal_sendv(iov, i);

#else // 0
        r = sd_journal_sendv_with_location(file, line, func, &iov[3], i - 3);
#endif // 0
finish:
        for (j = 3; j < i; j++)
                free(iov[j].iov_base);

        return r;
}

_public_ int sd_journal_sendv_with_location(
                const char *file, const char *line,
                const char *func,
                const struct iovec *iov, int n) {

#if 0 /// UNNEEDED by elogind
        struct iovec *niov;
        char *f;
#endif // 0

        assert_return(iov, -EINVAL);
        assert_return(n > 0, -EINVAL);

#if 0 /// If elogind splits this again in sd_journal_sendv, file/line/func will be lost.
        niov = newa(struct iovec, n + 3);
        memcpy(niov, iov, sizeof(struct iovec) * n);

        ALLOCA_CODE_FUNC(f, func);

        niov[n++] = IOVEC_MAKE_STRING(file);
        niov[n++] = IOVEC_MAKE_STRING(line);
        niov[n++] = IOVEC_MAKE_STRING(f);

        return sd_journal_sendv(niov, n);
#else // 0
        int   i;
        int   priority = LOG_INFO;
        char *message = NULL;

        for (i = 0; i < n ; ++i) {
                if (startswith(iov[i].iov_base, "PRIORITY=")
                                && (1 == sscanf(iov[i].iov_base, "PRIORITY=%i", &priority)))
                        continue;
                if ((message = startswith(iov[i].iov_base, "MESSAGE=")))
                        break;
        }

        if (message)
                return sd_journal_print_with_location(priority, file, line, func, "%s", message);

        return 0;
#endif // 0
}

_public_ int sd_journal_perror_with_location(
                const char *file, const char *line,
                const char *func,
                const char *message) {

#if 0 /// just send away in elogind
        struct iovec iov[6];
        char *f;

        ALLOCA_CODE_FUNC(f, func);

        iov[0] = IOVEC_MAKE_STRING(file);
        iov[1] = IOVEC_MAKE_STRING(line);
        iov[2] = IOVEC_MAKE_STRING(f);

        return fill_iovec_perror_and_send(message, 3, iov);
#else // 0
        if(message && *message)
                return sd_journal_print(LOG_ERR, "%s:%s:%s:%s: %s",
                                        strna(file), strna(line), strna(func),
                                        message, strerror(errno));
        else
                return sd_journal_print(LOG_ERR, "%s:%s:%s:%s",
                                        strna(file), strna(line), strna(func),
                                        strerror(errno));

        return 0;
#endif // 0
}
