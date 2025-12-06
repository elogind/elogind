/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <stdbool.h>

int _nss_elogind_block(bool b);
bool _nss_elogind_is_blocked(void);

/* For use with the _cleanup_() macro */
static inline void _nss_elogind_unblockp(bool *b) {
        if (*b)
                assert_se(_nss_elogind_block(false) >= 0);
}

/* aliases for using nss-elogind as a drop-in replacement for nss-systemd */
static inline int  _nss_systemd_block(bool b)     { return _nss_elogind_block(b); }
static inline bool _nss_systemd_is_blocked(void)  { return _nss_elogind_is_blocked(); }
