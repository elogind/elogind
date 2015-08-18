#
#  This file is part of systemd.
#
#  Copyright 2010-2012 Lennart Poettering
#  Copyright 2010-2012 Kay Sievers
#
#  systemd is free software; you can redistribute it and/or modify it
#  under the terms of the GNU Lesser General Public License as published by
#  the Free Software Foundation; either version 2.1 of the License, or
#  (at your option) any later version.
#
#  systemd is distributed in the hope that it will be useful, but
#  WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
#  Lesser General Public License for more details.
#
#  You should have received a copy of the GNU Lesser General Public License
#  along with systemd; If not, see <http://www.gnu.org/licenses/>.

AC_PREREQ([2.64])

# FIXME: Update to proper web page
AC_INIT([elogind],
        [219.1],
        [http://bugs.freedesktop.org/enter_bug.cgi?product=elogind],
        [elogind],
        [http://www.freedesktop.org/wiki/Software/elogind])

AC_CONFIG_SRCDIR([src/login/logind.c])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR([build-aux])

AC_USE_SYSTEM_EXTENSIONS
AC_SYS_LARGEFILE
AC_PREFIX_DEFAULT([/usr])
AM_MAINTAINER_MODE([enable])
AM_INIT_AUTOMAKE([foreign 1.11 -Wall -Wno-portability silent-rules tar-pax no-dist-gzip dist-xz subdir-objects parallel-tests])
AM_SILENT_RULES([yes])
AC_CANONICAL_HOST
AC_DEFINE_UNQUOTED([CANONICAL_HOST], "$host", [Canonical host string.])
LT_PREREQ(2.2)
LT_INIT([disable-static])

AS_IF([test "x$enable_static" = "xyes"], [AC_MSG_ERROR([--enable-static is not supported by elogind])])
AS_IF([test "x$enable_largefile" = "xno"], [AC_MSG_ERROR([--disable-largefile is not supported by elogind])])

SET_ARCH(X86_64, x86_64*)
SET_ARCH(IA32, i*86*)
SET_ARCH(MIPS, mips*)

# i18n stuff for the PolicyKit policy files, heck whether intltool can be found, disable NLS otherwise
AC_CHECK_PROG(intltool_found, [intltool-merge], [yes], [no])
AS_IF([test x"$intltool_found" != xyes],
      [AS_IF([test x"$enable_nls" = xyes],
             [AC_MSG_ERROR([--enable-nls requested but intltool not found])],
             [AS_IF([test x"$enable_nls" != xno],
                    [AC_MSG_WARN([*** Disabling NLS support because intltool was not found])
                     enable_nls=no])
             ])
      ])

AM_NLS
AS_IF([test x"$enable_nls" != xno -o "x$enable_polkit" != xno], [
    # intltoolize greps for '^(AC|IT)_PROG_INTLTOOL', so it needs to be on its own line
IT_PROG_INTLTOOL([0.40.0])
])

AS_IF([test -z "$INTLTOOL_POLICY_RULE"], [
    # If intltool is not available, provide a dummy rule to fail generation of %.policy files with a meaningful error message
    INTLTOOL_POLICY_RULE='%.policy: %.policy.in ; @echo "  ITMRG   " $@ && echo "*** intltool support required to build target $@" && false'
    AC_SUBST(INTLTOOL_POLICY_RULE)
])

GETTEXT_PACKAGE=elogind
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [elogind])

AC_PROG_MKDIR_P
AC_PROG_LN_S
AC_PROG_SED
AC_PROG_GREP
AC_PROG_AWK

AC_PROG_CC_C99

AC_PATH_PROG([M4], [m4])
AC_PATH_PROG([XSLTPROC], [xsltproc])

AC_PATH_PROG([KEXEC], [kexec], [/usr/sbin/kexec], [$PATH:/usr/sbin:/sbin])

AS_IF([! ln --relative --help > /dev/null 2>&1], [AC_MSG_ERROR([*** ln doesn't support --relative ***])])

M4_DEFINES=

# gtkdocize greps for '^GTK_DOC_CHECK', so it needs to be on its own line
m4_ifdef([GTK_DOC_CHECK], [
GTK_DOC_CHECK([1.18],[--flavour no-tmpl])],
         [AM_CONDITIONAL([ENABLE_GTK_DOC], [false])
          enable_gtk_doc=no])

AS_IF([test "x$enable_gtk_doc" = "xyes" -a "x$XSLTPROC" = x], [
        AC_MSG_ERROR([*** GTK doc requested but xsltproc not found])
])

AC_CHECK_TOOL(GPERF, gperf)
if test -z "$GPERF" ; then
        AC_MSG_ERROR([*** gperf not found])
fi

# ------------------------------------------------------------------------------
address_sanitizer_cflags=
address_sanitizer_cppflags=
address_sanitizer_ldflags=
AC_ARG_ENABLE(address-sanitizer, AS_HELP_STRING([--enable-address-sanitizer], [enable -fsanitize=address]))
AS_IF([test "x$enable_address_sanitizer" = "xyes"], [
            CC_CHECK_FLAG_APPEND([with_as_cflags], [CFLAGS], [-fsanitize=address])
            AS_IF([test -z "$with_as_cflags"],
                  [AC_MSG_ERROR([*** -fsanitize=address is not supported])])
            address_sanitizer_cflags="$with_as_cflags -fno-omit-frame-pointer -DVALGRIND=1"
            address_sanitizer_cppflags="-DVALGRIND=1"
            address_sanitizer_ldflags="-Wc,-fsanitize=address"
      ])

undefined_sanitizer_cflags=
undefined_sanitizer_cppflags=
undefined_sanitizer_ldflags=
AC_ARG_ENABLE(undefined-sanitizer, AS_HELP_STRING([--enable-undefined-sanitizer], [enable -fsanitize=undefined]))
AS_IF([test "x$enable_undefined_sanitizer" = "xyes"], [
            CC_CHECK_FLAG_APPEND([with_us_cflags], [CFLAGS], [-fsanitize=undefined])
            AS_IF([test -z "$with_us_cflags"],
                  [AC_MSG_ERROR([*** -fsanitize=undefined is not supported])])
            undefined_sanitizer_cflags="$with_us_cflags -fno-omit-frame-pointer -DVALGRIND=1"
            undefined_sanitizer_cppflags="-DVALGRIND=1"
            undefined_sanitizer_ldflags="-Wc,-fsanitize=undefined"
      ])

sanitizer_cflags="$address_sanitizer_cflags $undefined_sanitizer_cflags"
sanitizer_cppflags="$address_sanitizer_cppflags $undefined_sanitizer_cppflags"
sanitizer_ldflags="$address_sanitizer_ldflags $undefined_sanitizer_ldflags"

CC_CHECK_FLAGS_APPEND([with_cflags], [CFLAGS], [\
        -pipe \
        -Wall \
        -Wextra \
        -Wno-inline \
        -Wundef \
        "-Wformat=2 -Wformat-security -Wformat-nonliteral" \
        -Wlogical-op \
        -Wsign-compare \
        -Wmissing-include-dirs \
        -Wold-style-definition \
        -Wpointer-arith \
        -Winit-self \
        -Wdeclaration-after-statement \
        -Wfloat-equal \
        -Wsuggest-attribute=noreturn \
        -Wmissing-prototypes \
        -Wstrict-prototypes \
        -Wredundant-decls \
        -Wmissing-declarations \
        -Wmissing-noreturn \
        -Wshadow \
        -Wendif-labels \
        -Wstrict-aliasing=2 \
        -Wwrite-strings \
        -Wno-long-long \
        -Wno-overlength-strings \
        -Wno-unused-parameter \
        -Wno-missing-field-initializers \
        -Wno-unused-result \
        -Wno-format-signedness \
        -Werror=overflow \
        -Wdate-time \
        -Wnested-externs \
        -ffast-math \
        -fno-common \
        -fdiagnostics-show-option \
        -fno-strict-aliasing \
        -fvisibility=hidden \
        -ffunction-sections \
        -fdata-sections \
        -fstack-protector \
        -fstack-protector-strong \
        -fPIE \
        --param=ssp-buffer-size=4])

AS_CASE([$CC], [*clang*],
        [CC_CHECK_FLAGS_APPEND([with_cppflags], [CPPFLAGS], [\
               -Wno-typedef-redefinition \
               -Wno-gnu-variable-sized-type-not-at-end \
        ])])

dnl AS_CASE([$CFLAGS], [*-O[[12345sz\ ]]*],
dnl         [CC_CHECK_FLAGS_APPEND([with_cflags], [CFLAGS], [\
dnl                -flto -ffat-lto-objects])],
dnl         [AC_MSG_RESULT([skipping -flto, optimization not enabled])])
AC_SUBST([OUR_CFLAGS], "$with_cflags $sanitizer_cflags")

AS_CASE([$CFLAGS], [*-O[[12345sz\ ]]*],
        [CC_CHECK_FLAGS_APPEND([with_cppflags], [CPPFLAGS], [\
               -Wp,-D_FORTIFY_SOURCE=2])],
        [AC_MSG_RESULT([skipping -D_FORTIFY_SOURCE, optimization not enabled])])
AC_SUBST([OUR_CPPFLAGS], "$with_cppflags $sanitizer_cppflags")

CC_CHECK_FLAGS_APPEND([with_ldflags], [LDFLAGS], [\
        -Wl,--as-needed \
        -Wl,--no-undefined \
        -Wl,--gc-sections \
        -Wl,-z,relro \
        -Wl,-z,now \
        -pie \
        -Wl,-fuse-ld=gold])
AC_SUBST([OUR_LDFLAGS], "$with_ldflags $sanitizer_ldflags")

AC_CHECK_SIZEOF(pid_t)
AC_CHECK_SIZEOF(uid_t)
AC_CHECK_SIZEOF(gid_t)
AC_CHECK_SIZEOF(time_t)
AC_CHECK_SIZEOF(dev_t)
AC_CHECK_SIZEOF(rlim_t,,[
       #include <sys/time.h>
       #include <sys/resource.h>
])

# ------------------------------------------------------------------------------

AC_CHECK_HEADERS([sys/capability.h], [], [AC_MSG_ERROR([*** POSIX caps headers not found])])
AC_CHECK_HEADERS([linux/btrfs.h], [], [])
AC_CHECK_HEADERS([linux/memfd.h], [], [])

# unconditionally pull-in librt with old glibc versions
AC_SEARCH_LIBS([clock_gettime], [rt], [], [])

save_LIBS="$LIBS"
LIBS=
AC_SEARCH_LIBS([cap_init], [cap], [], [AC_MSG_ERROR([*** POSIX caps library not found])])
CAP_LIBS="$LIBS"
AC_SUBST(CAP_LIBS)

AC_CHECK_FUNCS([memfd_create])
AC_CHECK_FUNCS([__secure_getenv secure_getenv])
AC_CHECK_DECLS([gettid, pivot_root, name_to_handle_at, setns, getrandom, renameat2, kcmp, LO_FLAGS_PARTSCAN],
               [], [], [[
#include <sys/types.h>
#include <unistd.h>
#include <sys/mount.h>
#include <fcntl.h>
#include <sched.h>
#include <linux/loop.h>
#include <linux/random.h>
]])

AC_CHECK_DECLS([IFLA_INET6_ADDR_GEN_MODE,
                IFLA_MACVLAN_FLAGS,
                IFLA_IPVLAN_MODE,
                IFLA_VTI_REMOTE,
                IFLA_PHYS_PORT_ID,
                IFLA_BOND_AD_INFO,
                IFLA_VLAN_PROTOCOL,
                IFLA_VXLAN_LOCAL6,
                IFLA_IPTUN_6RD_RELAY_PREFIXLEN,
                IFLA_BRIDGE_VLAN_INFO,
                IFLA_BRPORT_UNICAST_FLOOD,
                NDA_IFINDEX],
[], [], [[
#include <inttypes.h>
#include <netinet/in.h>
#include <netinet/ether.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <linux/ip.h>
#include <linux/if_tunnel.h>
#include <linux/if_link.h>
#include <linux/if_bridge.h>
#include <linux/neighbour.h>
]])

# This makes sure pkg.m4 is available.
m4_pattern_forbid([^_?PKG_[A-Z_]+$],[*** pkg.m4 missing, please install pkg-config])

# ------------------------------------------------------------------------------
have_dbus=no
AC_ARG_ENABLE(dbus, AS_HELP_STRING([--disable-dbus], [disable usage of dbus-1 in tests]))
AS_IF([test "x$enable_dbus" != "xno"], [
        PKG_CHECK_MODULES(DBUS, [dbus-1 >= 1.3.2],
                [AC_DEFINE(HAVE_DBUS, 1, [Define if dbus-1 library is available]) have_dbus=yes],
                [have_dbus=no])
        AS_IF([test "x$have_dbus" = "xno" -a "x$enable_dbus" = "xyes"],
                [AC_MSG_ERROR([*** dbus-1 support requested but libraries not found])])])
AM_CONDITIONAL(HAVE_DBUS, [test "$have_dbus" = "yes"])

# ------------------------------------------------------------------------------
PKG_CHECK_MODULES(UDEV, [libudev])

# ------------------------------------------------------------------------------
have_coverage=no
AC_ARG_ENABLE(coverage, AS_HELP_STRING([--enable-coverage], [enable test coverage]))
if test "x$enable_coverage" = "xyes" ; then
        AC_CHECK_PROG(lcov_found, [lcov], [yes], [no])
        if test "x$lcov_found" = xno ; then
                AC_MSG_ERROR([*** lcov support requested but the program was not found])
        else
                lcov_version_major="`lcov --version | cut -d ' ' -f 4 | cut -d '.' -f 1`"
                lcov_version_minor="`lcov --version | cut -d ' ' -f 4 | cut -d '.' -f 2`"
                if test "$lcov_version_major" -eq 1 -a "$lcov_version_minor" -lt 10; then
                        AC_MSG_ERROR([*** lcov version is too old. 1.10 required])
                else
                        have_coverage=yes
                        CC_CHECK_FLAGS_APPEND([with_coverage_cflags], [CFLAGS], [\
                        -fprofile-arcs \
                        -ftest-coverage])
                        AC_SUBST([OUR_CFLAGS], "$with_cflags $with_coverage_cflags")
                fi
        fi
fi
AM_CONDITIONAL(ENABLE_COVERAGE, [test "$have_coverage" = "yes"])

# ------------------------------------------------------------------------------
have_blkid=no
AC_ARG_ENABLE(blkid, AS_HELP_STRING([--disable-blkid], [disable blkid support]))
if test "x$enable_blkid" != "xno"; then
        PKG_CHECK_MODULES(BLKID, [ blkid >= 2.24 ],
                [AC_DEFINE(HAVE_BLKID, 1, [Define if blkid is available]) have_blkid=yes], have_blkid=no)
        if test "x$have_blkid" = xno -a "x$enable_blkid" = xyes; then
                AC_MSG_ERROR([*** blkid support requested but libraries not found])
        fi
fi
AM_CONDITIONAL(HAVE_BLKID, [test "$have_blkid" = "yes"])

# ------------------------------------------------------------------------------
have_seccomp=no
AC_ARG_ENABLE(seccomp, AS_HELP_STRING([--disable-seccomp], [Disable optional SECCOMP support]))
if test "x$enable_seccomp" != "xno"; then
        PKG_CHECK_MODULES(SECCOMP, [libseccomp >= 1.0.0],
               [AC_DEFINE(HAVE_SECCOMP, 1, [Define if seccomp is available])
                have_seccomp=yes
                M4_DEFINES="$M4_DEFINES -DHAVE_SECCOMP"],
               [have_seccomp=no])
        if test "x$have_seccomp" = "xno" -a "x$enable_seccomp" = "xyes"; then
                AC_MSG_ERROR([*** seccomp support requested but libraries not found])
        fi
fi
AM_CONDITIONAL(HAVE_SECCOMP, [test "$have_seccomp" = "yes"])

# ------------------------------------------------------------------------------
have_selinux=no
AC_ARG_ENABLE(selinux, AS_HELP_STRING([--disable-selinux], [Disable optional SELINUX support]))
if test "x$enable_selinux" != "xno"; then
        PKG_CHECK_MODULES([SELINUX], [libselinux >= 2.1.9],
                [AC_DEFINE(HAVE_SELINUX, 1, [Define if SELinux is available])
                 have_selinux=yes
                 M4_DEFINES="$M4_DEFINES -DHAVE_SELINUX"],
                [have_selinux=no])
        if test "x$have_selinux" = xno -a "x$enable_selinux" = xyes; then
                AC_MSG_ERROR([*** SELinux support requested but libraries not found])
        fi
fi
AM_CONDITIONAL(HAVE_SELINUX, [test "$have_selinux" = "yes"])

have_apparmor=no
AC_ARG_ENABLE(apparmor, AS_HELP_STRING([--disable-apparmor], [Disable optional AppArmor support]))
if test "x$enable_apparmor" != "xno"; then
        PKG_CHECK_MODULES([APPARMOR], [libapparmor],
                [AC_DEFINE(HAVE_APPARMOR, 1, [Define if AppArmor is available])
                 have_apparmor=yes
                 M4_DEFINES="$M4_DEFINES -DHAVE_APPARMOR"],
                [have_apparmor=no])
        if test "x$have_apparmor" = xno -a "x$enable_apparmor" = xyes; then
                AC_MSG_ERROR([*** AppArmor support requested but libraries not found])
        fi
fi
AM_CONDITIONAL(HAVE_APPARMOR, [test "$have_apparmor" = "yes"])

# ------------------------------------------------------------------------------
AC_ARG_ENABLE([pam],
        AS_HELP_STRING([--disable-pam],[Disable optional PAM support]),
                [case "${enableval}" in
                        yes) have_pam=yes ;;
                        no) have_pam=no ;;
                        *) AC_MSG_ERROR(bad value ${enableval} for --disable-pam) ;;
                esac],
                [have_pam=auto])

if test "x${have_pam}" != xno ; then
        AC_CHECK_HEADERS(
                [security/pam_modules.h security/pam_modutil.h security/pam_ext.h],
                [have_pam=yes],
                [if test "x$have_pam" = xyes ; then
                        AC_MSG_ERROR([*** PAM headers not found.])
                fi])

        AC_CHECK_LIB(
                [pam],
                [pam_syslog],
                [have_pam=yes],
                [if test "x$have_pam" = xyes ; then
                        AC_MSG_ERROR([*** libpam not found.])
                fi])

        if test "x$have_pam" = xyes ; then
                PAM_LIBS="-lpam -lpam_misc"
                AC_DEFINE(HAVE_PAM, 1, [PAM available])
                M4_DEFINES="$M4_DEFINES -DHAVE_PAM"
        else
                have_pam=no
        fi
else
        PAM_LIBS=
fi
AC_SUBST(PAM_LIBS)
AM_CONDITIONAL([HAVE_PAM], [test "x$have_pam" != xno])

# ------------------------------------------------------------------------------
AC_ARG_ENABLE([acl],
        AS_HELP_STRING([--disable-acl],[Disable optional ACL support]),
                [case "${enableval}" in
                        yes) have_acl=yes ;;
                        no) have_acl=no ;;
                        *) AC_MSG_ERROR(bad value ${enableval} for --disable-acl) ;;
                esac],
                [have_acl=auto])

if test "x${have_acl}" != xno ; then
        AC_CHECK_HEADERS(
                [sys/acl.h acl/libacl.h],
                [have_acl=yes],
                [if test "x$have_acl" = xyes ; then
                        AC_MSG_ERROR([*** ACL headers not found.])
                fi])

        AC_CHECK_LIB(
                [acl],
                [acl_get_file],
                [have_acl=yes],
                [if test "x$have_acl" = xyes ; then
                        AC_MSG_ERROR([*** libacl not found.])
                fi])

        if test "x$have_acl" = xyes ; then
                ACL_LIBS="-lacl"
                AC_DEFINE(HAVE_ACL, 1, [ACL available])
                M4_DEFINES="$M4_DEFINES -DHAVE_ACL"
        else
                have_acl=no
        fi
else
        ACL_LIBS=
fi
AC_SUBST(ACL_LIBS)
AM_CONDITIONAL([HAVE_ACL], [test "x$have_acl" != xno])

# ------------------------------------------------------------------------------
AC_ARG_ENABLE([smack], AS_HELP_STRING([--disable-smack],[Disable optional SMACK support]),
                [case "${enableval}" in
                        yes) have_smack=yes ;;
                        no) have_smack=no ;;
                        *) AC_MSG_ERROR(bad value ${enableval} for --disable-smack) ;;
                esac],
                [have_smack=auto])

if test "x${have_smack}" = xauto; then
        M4_DEFINES="$M4_DEFINES -DHAVE_SMACK"
        have_smack=yes
fi

AC_ARG_WITH(smack-run-label,
AS_HELP_STRING([--with-smack-run-label=STRING],
        [run systemd --system with a specific SMACK label]),
        [AC_DEFINE_UNQUOTED(SMACK_RUN_LABEL, ["$withval"], [Run with a smack label])],
        [])

if test "x${have_smack}" = xyes ; then
        AC_DEFINE(HAVE_SMACK, 1, [Define if SMACK is available])
fi

AM_CONDITIONAL([HAVE_SMACK], [test "x$have_smack" = "xyes"])

# ------------------------------------------------------------------------------
AC_ARG_WITH(system-uid-max,
        AS_HELP_STRING([--with-system-uid-max=UID]
                [Maximum UID for system users]),
        [SYSTEM_UID_MAX="$withval"],
        [SYSTEM_UID_MAX="`awk 'BEGIN { uid=999 } /^\s*SYS_UID_MAX\s+/ { uid=$2 } END { print uid }' /etc/login.defs 2>/dev/null || echo 999`"])

AC_DEFINE_UNQUOTED(SYSTEM_UID_MAX, [$SYSTEM_UID_MAX], [Maximum System UID])
AC_SUBST(SYSTEM_UID_MAX)

# ------------------------------------------------------------------------------
AC_ARG_WITH(system-gid-max,
        AS_HELP_STRING([--with-system-gid-max=GID]
                [Maximum GID for system groups]),
        [SYSTEM_GID_MAX="$withval"],
        [SYSTEM_GID_MAX="`awk 'BEGIN { gid=999 } /^\s*SYS_GID_MAX\s+/ { gid=$2 } END { print gid }' /etc/login.defs 2>/dev/null || echo 999`"])

AC_DEFINE_UNQUOTED(SYSTEM_GID_MAX, [$SYSTEM_GID_MAX], [Maximum System GID])
AC_SUBST(SYSTEM_GID_MAX)

# ------------------------------------------------------------------------------
have_polkit=no
AC_ARG_ENABLE(polkit, AS_HELP_STRING([--disable-polkit], [disable PolicyKit support]))
if test "x$enable_polkit" != "xno"; then
        AC_DEFINE(ENABLE_POLKIT, 1, [Define if PolicyKit support is to be enabled])
        have_polkit=yes
fi
AM_CONDITIONAL(ENABLE_POLKIT, [test "x$have_polkit" = "xyes"])

# ------------------------------------------------------------------------------
have_kdbus=no
AC_ARG_ENABLE(kdbus, AS_HELP_STRING([--enable-kdbus], [do connect to kdbus by default]))
if test "x$enable_kdbus" = "xyes"; then
        AC_DEFINE(ENABLE_KDBUS, 1, [Define if kdbus support is to be enabled])
        have_kdbus=yes
        M4_DEFINES="$M4_DEFINES -DENABLE_KDBUS"
fi
AM_CONDITIONAL(ENABLE_KDBUS, [test "$have_kdbus" = "yes"])

# ------------------------------------------------------------------------------
AC_CHECK_HEADERS_ONCE([valgrind/memcheck.h valgrind/valgrind.h])

# ------------------------------------------------------------------------------
PKG_CHECK_MODULES([GLIB], [glib-2.0 >= 2.22.0 gobject-2.0 >= 2.22.0 gio-2.0],
                  [have_glib=yes], [have_glib=no])
AS_IF([test "x$have_glib" = "xyes"], [ AC_DEFINE(HAVE_GLIB, 1, [Define if glib is available]) ])

# ------------------------------------------------------------------------------
have_manpages=no
AC_ARG_ENABLE(manpages, AS_HELP_STRING([--disable-manpages], [disable manpages]))
AS_IF([test "x$enable_manpages" != xno], [have_manpages=yes])
AM_CONDITIONAL(ENABLE_MANPAGES, [test "x$have_manpages" = "xyes"])

# ------------------------------------------------------------------------------
AC_SUBST(M4_DEFINES)

AC_ARG_WITH([dbuspolicydir],
        AS_HELP_STRING([--with-dbuspolicydir=DIR], [D-Bus policy directory]),
        [],
        [with_dbuspolicydir=${sysconfdir}/dbus-1/system.d])

AC_ARG_WITH([dbussessionservicedir],
        AS_HELP_STRING([--with-dbussessionservicedir=DIR], [D-Bus session service directory]),
        [],
        [with_dbussessionservicedir=${datadir}/dbus-1/services])

AC_ARG_WITH([dbussystemservicedir],
        AS_HELP_STRING([--with-dbussystemservicedir=DIR], [D-Bus system service directory]),
        [],
        [with_dbussystemservicedir=${datadir}/dbus-1/system-services])

AC_ARG_WITH([bashcompletiondir],
        AS_HELP_STRING([--with-bashcompletiondir=DIR], [Bash completions directory]),
        [],
        [AS_IF([$($PKG_CONFIG --exists bash-completion)], [
                with_bashcompletiondir=$($PKG_CONFIG --variable=completionsdir bash-completion)
        ] , [
                with_bashcompletiondir=${datadir}/bash-completion/completions
        ])])

AC_ARG_WITH([zshcompletiondir],
        AS_HELP_STRING([--with-zshcompletiondir=DIR], [Zsh completions directory]),
        [], [with_zshcompletiondir=${datadir}/zsh/site-functions])

AC_ARG_WITH([rootprefix],
        AS_HELP_STRING([--with-rootprefix=DIR], [rootfs directory prefix for config files and kernel modules]),
        [], [with_rootprefix=${ac_default_prefix}])

AC_ARG_WITH([rootlibdir],
        AS_HELP_STRING([--with-rootlibdir=DIR], [Root directory for libraries necessary for boot]),
        [],
        [with_rootlibdir=${libdir}])

AC_ARG_WITH([pamlibdir],
        AS_HELP_STRING([--with-pamlibdir=DIR], [Directory for PAM modules]),
        [],
        [with_pamlibdir=${with_rootlibdir}/security])

AC_ARG_WITH([pamconfdir],
        AS_HELP_STRING([--with-pamconfdir=DIR], [Directory for PAM configuration]),
        [],
        [with_pamconfdir=${sysconfdir}/pam.d])

AC_ARG_ENABLE([split-usr],
        AS_HELP_STRING([--enable-split-usr], [Assume that /bin, /sbin aren\'t symlinks into /usr]),
        [],
        [AS_IF([test "x${ac_default_prefix}" != "x${with_rootprefix}"], [
                enable_split_usr=yes
        ], [
                enable_split_usr=no
        ])])

AS_IF([test "x${enable_split_usr}" = "xyes"], [
        AC_DEFINE(HAVE_SPLIT_USR, 1, [Define if /bin, /sbin aren't symlinks into /usr])
])

# Work around intltoolize and gtk-doc problems in VPATH builds
AM_CONDITIONAL([ENABLE_GTK_DOC_TESTS], [test "x$0" = "x./configure"],
                                       [Define to do gtk-doc tests])
AS_IF([test "x$0" != "x./configure"], [
        AC_SUBST([INTLTOOL_UPDATE], [/usr/bin/env true])
])

AC_ARG_ENABLE(tests,
        [AC_HELP_STRING([--disable-tests], [disable tests])],
        enable_tests=$enableval, enable_tests=yes)
AM_CONDITIONAL(ENABLE_TESTS, [test x$enable_tests = xyes])

AC_ARG_ENABLE(debug,
        [AC_HELP_STRING([--enable-debug@<:@=LIST@:>@], [enable extra debugging (hashmap,mmap-cache)])],
        [if test "x$enableval" = "xyes"; then
                enableval="hashmap,mmap-cache"
        fi
        saved_ifs="$IFS"
        IFS="$IFS$PATH_SEPARATOR,"
        for name in $enableval; do
                case $name in
                hashmap)
                        enable_debug_hashmap=yes
                        ;;
                mmap-cache)
                        enable_debug_mmap_cache=yes
                        ;;
                esac
        done
        IFS="$saved_ifs"],[])

enable_debug=""
AS_IF([test x$enable_debug_hashmap = xyes], [
        AC_DEFINE(ENABLE_DEBUG_HASHMAP, 1, [Define if hashmap debugging is to be enabled])
        enable_debug="hashmap $enable_debug"
])
AS_IF([test x$enable_debug_mmap_cache = xyes], [
        AC_DEFINE(ENABLE_DEBUG_MMAP_CACHE, 1, [Define if mmap cache debugging is to be enabled])
        enable_debug="mmap-cache $enable_debug"
])
test -z "$enable_debug" && enable_debug="none"

AC_SUBST([dbuspolicydir], [$with_dbuspolicydir])
AC_SUBST([dbussessionservicedir], [$with_dbussessionservicedir])
AC_SUBST([dbussystemservicedir], [$with_dbussystemservicedir])
AC_SUBST([bashcompletiondir], [$with_bashcompletiondir])
AC_SUBST([zshcompletiondir], [$with_zshcompletiondir])
AC_SUBST([pamlibdir], [$with_pamlibdir])
AC_SUBST([pamconfdir], [$with_pamconfdir])
AC_SUBST([rootprefix], [$with_rootprefix])
AC_SUBST([rootlibdir], [$with_rootlibdir])

AC_CONFIG_FILES([
        Makefile po/Makefile.in
])

AC_OUTPUT
AC_MSG_RESULT([
        $PACKAGE_NAME $VERSION

        PAM:                     ${have_pam}
        AppArmor:                ${have_apparmor}
        SELinux:                 ${have_selinux}
        SECCOMP:                 ${have_seccomp}
        SMACK:                   ${have_smack}
        ACL:                     ${have_acl}
        polkit:                  ${have_polkit}
        blkid:                   ${have_blkid}
        dbus:                    ${have_dbus}
        kdbus:                   ${have_kdbus}
        man pages:               ${have_manpages}
        gtk-doc:                 ${enable_gtk_doc}
        test coverage:           ${have_coverage}
        Split /usr:              ${enable_split_usr}
        extra debugging:         ${enable_debug}

        prefix:                  ${prefix}
        rootprefix:              ${with_rootprefix}
        sysconf dir:             ${sysconfdir}
        datarootdir:             ${datarootdir}
        includedir:              ${includedir}
        lib dir:                 ${libdir}
        rootlib dir:             ${with_rootlibdir}
        PAM modules dir:         ${with_pamlibdir}
        PAM configuration dir:   ${with_pamconfdir}
        D-Bus policy dir:        ${with_dbuspolicydir}
        D-Bus session dir:       ${with_dbussessionservicedir}
        D-Bus system dir:        ${with_dbussystemservicedir}
        Bash completions dir:    ${with_bashcompletiondir}
        Zsh completions dir:     ${with_zshcompletiondir}
        Maximum System UID:      ${SYSTEM_UID_MAX}
        Maximum System GID:      ${SYSTEM_GID_MAX}

        CFLAGS:                  ${OUR_CFLAGS} ${CFLAGS}
        CPPFLAGS:                ${OUR_CPPFLAGS} ${CPPFLAGS}
        LDFLAGS:                 ${OUR_LDFLAGS} ${LDFLAGS}
])
