#pragma once
#ifndef ELOGIND_BASIC_MUSL_MISSING_H_INCLUDED
#define ELOGIND_BASIC_MUSL_MISSING_H_INCLUDED


/****************************************************************
 * musl_missing.h - work around glibc extensions for musl libc.
 *
 * Implements glibc functions missing in musl libc as macros.
 * Is to be included where these functions are used.
 * Also defines some glibc only constants as either 0 or
 * as found in the corresponding glibc header file.
 *
 * Juergen Buchmueller <pullmoll@t-online.de> for Void Linux
 * Public Domain; no warranties whatsoever. Thank you Mr. P.
 *
 ****************************************************************/


void elogind_set_program_name(const char* pcall);

#include "qsort_r_missing.h"

#if !defined(__GLIBC__)
#include <string.h>
#include <unistd.h>
#include <pthread.h> /* for pthread_atfork */

#define strerror_r(e, m, k) (strerror_r(e, m, k) < 0 ? strdup("strerror_r() failed") : m);

/*
 * Possibly TODO according to http://man7.org/linux/man-pages/man3/getenv.3.html
 * + test if the process's effective user ID does not match its real user ID or
 *   the process's effective group ID does not match its real group ID;
 *   typically this is the result of executing a set-user-ID or set-
 *   group-ID program. Is calling issetugid() sufficient here?
 * + test if the effective capability bit was set on the executable file
 * + test if the process has a nonempty permitted capability set
 */
#if ! HAVE_SECURE_GETENV && ! HAVE___SECURE_GETENV
#  define secure_getenv(name) \
        (issetugid() ? NULL : getenv(name))
#  undef HAVE_SECURE_GETENV
#  define HAVE_SECURE_GETENV 1
#endif // HAVE_[__]SECURE_GETENV

/* Poor man's basename */
#define basename(path) \
        (strrchr(path, '/') ? strrchr(path, '/')+1 : path)

/* strndupa may already be defined in another compatibility header */
#if !defined(strndupa)
#define strndupa(x_src, x_n) \
        (__extension__ ( {   \
                const char* x_in  = (x_src);                  \
                size_t      x_len = strnlen(x_in, (x_n)) + 1; \
                char*       x_out = (char *) alloca(x_len);   \
                x_out[x_len-1] = '\0';                        \
                (char *) memcpy(x_out, x_in, x_len-1);        \
        } ) )
#endif

/* getnameinfo(3) glibc extensions are undefined in musl libc */
#define NI_IDN 0
#define NI_IDN_USE_STD3_ASCII_RULES 0

/* Taken from glibc's net/if_arp.h */
#if !defined(ARPHRD_IEEE802154_PHY)
#define ARPHRD_IEEE802154_PHY 805        /* IEEE 802.15.4 PHY header.  */
#endif

/* Shorthand for type of comparison functions. */
#ifndef __COMPAR_FN_T
# define __COMPAR_FN_T
typedef int (*__compar_fn_t) (const void *, const void *);
typedef __compar_fn_t comparison_fn_t;
typedef int (*__compar_d_fn_t) (const void *, const void *, void *);
#endif

/* Make musl utmp/wtmp stubs visible if needed. */
#if ENABLE_UTMP
# include <paths.h>
# include <utmp.h>
# include <utmpx.h>
# if defined(_PATH_UTMP) && !defined(_PATH_UTMPX)
#   define _PATH_UTMPX _PATH_UTMP
# endif
# if defined(_PATH_WTMP) && !defined(_PATH_WTMPX)
#   define _PATH_WTMPX _PATH_WTMP
# endif
#endif // ENABLE_UTMP

/*
 * Systemd makes use of undeclared glibc-specific __register_atfork to avoid
 * a depednency on libpthread, __register_atfork is roughly equivalent to
 * pthread_atfork so define __register_atfork to pthread_atfork.
 */
#define __register_atfork(prepare,parent,child,dso) pthread_atfork(prepare,parent,child)

/* 
 * Missing FTW macros in musl, define them if not defined
 * taken from
 * https://git.yoctoproject.org/cgit.cgi/poky/plain/meta/recipes-core/systemd/systemd/0028-add-missing-FTW_-macros-for-musl.patch
 */
#ifndef FTW_ACTIONRETVAL
#define FTW_ACTIONRETVAL 16
#endif

#ifndef FTW_CONTINUE
#define FTW_CONTINUE 0
#endif

#ifndef FTW_STOP
#define FTW_STOP 1
#endif

#ifndef FTW_SKIP_SUBTREE
#define FTW_SKIP_SUBTREE 2
#endif

#endif // !defined(__GLIBC__)

#endif // ELOGIND_BASIC_MUSL_MISSING_H_INCLUDED

