/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "syslog-util.h"

#if 0 /// UNNEEDED by elogind
int journal_fd_nonblock(bool nonblock);
void close_journal_fd(void);
#endif // 0

/* We declare sd_journal_stream_fd() as async-signal-safe. So instead of strjoin(), which calls malloc()
 * internally, use a macro + alloca(). */
#define journal_stream_path(log_namespace)                                              \
        ({                                                                              \
                const char *_ns = (log_namespace), *_ret;                               \
                if (!_ns)                                                               \
                        _ret = "/run/systemd/journal/stdout";                           \
                else if (log_namespace_name_valid(_ns))                                 \
                        _ret = strjoina("/run/systemd/journal.", _ns, "/stdout");       \
                else                                                                    \
                        _ret = NULL;                                                    \
                _ret;                                                                   \
        })
