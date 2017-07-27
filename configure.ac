#
#  This file is part of elogind.
#
#  Copyright 2010-2012 Lennart Poettering
#  Copyright 2010-2012 Kay Sievers
#
#  elogind is free software; you can redistribute it and/or modify it
#  under the terms of the GNU Lesser General Public License as published by
#  the Free Software Foundation; either version 2.1 of the License, or
#  (at your option) any later version.
#
#  elogind is distributed in the hope that it will be useful, but
#  WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
#  Lesser General Public License for more details.
#
#  You should have received a copy of the GNU Lesser General Public License
#  along with systemd; If not, see <http://www.gnu.org/licenses/>.

AC_PREREQ([2.64])

AC_INIT([elogind],
        [232.4],
        [https://github.com/elogind/elogind/issues],
        [elogind],
        [https://github.com/elogind/elogind])

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

AC_PROG_CC_C99

AX_COMPILER_VENDOR
AS_IF([test "x$ax_cv_c_compiler_vendor" = "xgnu"], [
      AC_CHECK_TOOLS([AR], [gcc-ar ar], [:])
      AC_CHECK_TOOLS([NM], [gcc-nm nm], [:])
      AC_CHECK_TOOLS([RANLIB], [gcc-ranlib ranlib], [:])
])

LT_PREREQ(2.2)
LT_INIT([disable-static])

AS_IF([test "x$enable_static" = "xyes"], [AC_MSG_ERROR([--enable-static is not supported by elogind])])
AS_IF([test "x$enable_largefile" = "xno"], [AC_MSG_ERROR([--disable-largefile is not supported by elogind])])

SET_ARCH(X86_64, x86_64*)
SET_ARCH(IA32, i*86*)
SET_ARCH(MIPS, mips*)
SET_ARCH(AARCH64, aarch64*)

# i18n stuff for the PolicyKit policy files, check whether intltool can be found, disable NLS otherwise
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

AC_PATH_PROG([M4], [m4])
AC_PATH_PROG([XSLTPROC], [xsltproc])

AC_PATH_PROG([HALT], [halt], [halt])
AC_PATH_PROG([REBOOT], [reboot], [reboot])
AC_PATH_PROG([KEXEC], [kexec], [/usr/sbin/kexec], [$PATH:/usr/sbin:/sbin])

AS_IF([! ln --relative --help > /dev/null 2>&1], [AC_MSG_ERROR([*** ln doesn't support --relative ***])])

M4_DEFINES=

AC_CHECK_TOOL(OBJCOPY, objcopy)
AC_CHECK_TOOL(GPERF, gperf)
if test -z "$GPERF" ; then
        AC_MSG_ERROR([*** gperf not found])
fi


# ------------------------------------------------------------------------------
# Let users set the cgroup controller to use, in case the target controller
# isn't currently running the show.
# Example: Gentoo Linux, user wants to switch from systemd to openrc+elogind,
#          and emerges elogind before having booted the machine with openrc.
#          See: https://github.com/elogind/elogind/issues/18
with_cgroupctrl=
AC_ARG_WITH([cgroup-controller],
            AS_HELP_STRING([--with-cgroup-controller=name],
            [Set the name of the cgroup controller to use.
             Use this when the autodetection fails, or you plan to use your system with a different controller than the one in place now.
             The value 'auto' (default) detects the running controller.
             The values 'none' and 'elogind' will cause elogind to be its own (very limited) controller.
             When elogind shall be its own controller, there *MUST NOT* be any other controller running!
             Another popular controller would be 'openrc'.]),
            [with_cgroupctrl=$withval],
            [with_cgroupctrl=auto])

# ------------------------------------------------------------------------------
# Find running cgroup controller, if none was set
AS_IF(  [test "x$with_cgroupctrl" = "xauto"], [
        AS_IF([test -f /proc/self/cgroup], [
                # If the init system is a cgroup controler, it will be position 1.
                # Secondary controllers, like cgmanager, do not work.
                with_cgroupctrl=`grep "^1:name=" /proc/self/cgroup | \
                        sed -n 's/.*=//p' | sed -e 's/:.*$//'`
                AS_IF(  [test -z "$with_cgroupctrl"], [
                        # Try to be our own cgroup controller
                        with_cgroupctrl="elogind"
                        ])
        ], [
                # 'auto' but no cgroup fs is a problem.
                with_cgroupctrl=""
        ])
])

# If the user specified 'none', switch to 'elogind'.
# 'none' is allowed, as this means "there is no controller now"
AS_IF(  [test "x$with_cgroupctrl" = "xnone"], [with_cgroupctrl=elogind])

# If this was not possible, /proc/self/cgroup not mounted yet, and 'auto'
# chosen, error out.
AS_IF(  [test -z "$with_cgroupctrl"],
        AC_MSG_ERROR([No running cgroup controller found]), [
        # Otherwise add the 'name=' prefix
        with_cgroupctrl="name=$with_cgroupctrl"
])

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
        -Wundef \
        "-Wformat=2 -Wformat-security -Wformat-nonliteral" \
        -Wlogical-op \
        -Wmissing-include-dirs \
        -Wold-style-definition \
        -Wpointer-arith \
        -Winit-self \
        -Wdeclaration-after-statement \
        -Wfloat-equal \
        -Wsuggest-attribute=noreturn \
        -Werror=missing-prototypes \
        -Werror=implicit-function-declaration \
        -Werror=missing-declarations \
        -Werror=return-type \
        -Wstrict-prototypes \
        -Wredundant-decls \
        -Wmissing-noreturn \
        -Wshadow \
        -Wendif-labels \
        -Wstrict-aliasing=2 \
        -Wwrite-strings \
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
        -fstack-protector \
        -fstack-protector-strong \
        -fPIE \
        --param=ssp-buffer-size=4])

CC_CHECK_FLAG_APPEND([with_cflags], [CFLAGS], [-Werror=shadow], [
#include <time.h>
#include <inttypes.h>
typedef uint64_t usec_t;
usec_t now(clockid_t clock);
int main(void) {
        struct timespec now;
        return 0;
}
])

AS_CASE([$CC], [*clang*],
        [CC_CHECK_FLAGS_APPEND([with_cppflags], [CPPFLAGS], [\
               -Wno-typedef-redefinition \
               -Wno-gnu-variable-sized-type-not-at-end \
        ])])

# ------------------------------------------------------------------------------
have_lto=no
AC_ARG_ENABLE([lto], [AS_HELP_STRING([--disable-lto], [disable -flto])],
                     [], [enable_lto=yes])
AS_CASE([$CFLAGS], [*-O[[12345sz\ ]]*],
        [AS_IF([test "x$enable_lto" = "xyes"], [have_lto=yes],
               [AC_MSG_RESULT([disabling -flto as requested])])],
        [AC_MSG_RESULT([skipping -flto, optimization not enabled])])

AS_IF([test "x$have_lto" = "xyes"],
        [CC_CHECK_FLAGS_APPEND([with_cflags], [CFLAGS],
        [-flto -fuse-linker-plugin])])

AS_CASE([$with_cflags], [*-flto*], [], [have_lto=no])
AC_SUBST([OUR_CFLAGS], "$with_cflags $sanitizer_cflags")

# ------------------------------------------------------------------------------
AS_CASE([$CFLAGS], [*-O[[12345sz\ ]]*],
        [CC_CHECK_FLAGS_APPEND([with_cppflags], [CPPFLAGS], [\
               -Wp,-D_FORTIFY_SOURCE=2])],
        [AC_MSG_RESULT([skipping -D_FORTIFY_SOURCE, optimization not enabled])])
AC_SUBST([OUR_CPPFLAGS], "$with_cppflags $sanitizer_cppflags")

AS_CASE([$CFLAGS], [*-O[[12345sz\ ]]*],
        [CC_CHECK_FLAGS_APPEND([with_ldflags], [LDFLAGS], [\
               -Wl,--gc-sections])],
        [AC_MSG_RESULT([skipping --gc-sections, optimization not enabled])])
AC_SUBST([OUR_CFLAGS], "$with_ldflags $sanitizer_cflags")

AS_CASE([$CFLAGS], [*-O[[12345sz\ ]]*],
        [CC_CHECK_FLAGS_APPEND([with_cflags], [CFLAGS], [\
               -ffunction-sections -fdata-sections])],
        [AC_MSG_RESULT([skipping -ffunction/data-section, optimization not enabled])])
AC_SUBST([OUR_CFLAGS], "$with_cflags $sanitizer_cflags")

CC_CHECK_FLAGS_APPEND([with_ldflags], [LDFLAGS], [\
        -Wl,--as-needed \
        -Wl,--no-undefined \
        -Wl,-z,relro \
        -Wl,-z,now \
        -pie])
AS_IF([test "x$have_lto" = "xyes"],
        [CC_CHECK_FLAGS_APPEND([with_ldflags], [LDFLAGS], [-Wl,-fuse-ld=gold])])

# ------------------------------------------------------------------------------
AC_SUBST([OUR_CPPFLAGS], "$with_cppflags $sanitizer_cppflags")
AC_SUBST([OUR_CFLAGS], "-D__SANE_USERSPACE_TYPES__ $with_cflags $sanitizer_cflags")
AC_SUBST([OUR_LDFLAGS], "$with_ldflags $sanitizer_ldflags")

AC_CHECK_SIZEOF(pid_t)
AC_CHECK_SIZEOF(uid_t)
AC_CHECK_SIZEOF(gid_t)
AC_CHECK_SIZEOF(time_t)
AC_CHECK_SIZEOF(dev_t)
AC_CHECK_SIZEOF(ino_t)
AC_CHECK_SIZEOF(rlim_t,,[
       #include <sys/time.h>
       #include <sys/resource.h>
])

GPERF_TEST="$(echo foo,bar | ${GPERF} -L ANSI-C)"
save_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS -Wno-error"
AC_COMPILE_IFELSE(
        [AC_LANG_PROGRAM([
                #include <string.h>
                const char * in_word_set(const char *, size_t);
                $GPERF_TEST]
        )],
        [GPERF_LEN_TYPE=size_t],
        [AC_COMPILE_IFELSE(
                [AC_LANG_PROGRAM([
                        #include <string.h>
                        const char * in_word_set(const char *, unsigned);
                        $GPERF_TEST]
                )],
                [GPERF_LEN_TYPE=unsigned],
                [AC_MSG_ERROR([** unable to determine gperf len type])]
        )]
)
CFLAGS="$save_CFLAGS"

AC_DEFINE_UNQUOTED([GPERF_LEN_TYPE], [$GPERF_LEN_TYPE], [gperf len type])

# ------------------------------------------------------------------------------
# we use python to build the man page index
have_python=no
AC_ARG_WITH([python],
        [AS_HELP_STRING([--without-python], [Disable building the man page index and systemd-python (default: test)])])

have_lxml=no
AS_IF([test "x$with_python" != "xno"], [
        AM_PATH_PYTHON(,, [:])
        AS_IF([test "x$PYTHON" != "x:"], [
                AC_MSG_CHECKING([for python lxml module])
                AS_IF(["$PYTHON" -c 'import lxml' 2>/dev/null], [have_lxml=yes])
                AC_MSG_RESULT([$have_lxml])
                AS_IF([test "x$have_lxml" = "xyes"], [have_python=yes],
                      [AC_MSG_WARN([*** python support requires python-lxml module installed])])
        ])
])
AS_IF([test "$have_python" != "yes"], [
      AS_IF([test "$with_python" = "yes"],
            [AC_MSG_ERROR([*** python support requested but python support not found])])
      AS_IF([test "$with_python" != "no"],
            [AC_MSG_WARN([*** python support not found, some documentation cannot be built])])
])
AM_CONDITIONAL([HAVE_PYTHON], [test "x$have_python" = "xyes"])

# ------------------------------------------------------------------------------

AC_CHECK_HEADERS([sys/capability.h], [], [AC_MSG_ERROR([*** POSIX caps headers not found])])
AC_CHECK_HEADERS([linux/memfd.h], [], [])

AC_CHECK_HEADERS([printf.h], [have_printf_h=yes], [have_printf_h=no])
AS_IF([test x$have_printf_h = xyes], [
        AC_DEFINE(HAVE_PRINTF_H, 1, [Define if printf.h was found])
])



# unconditionally pull-in librt with old glibc versions
dnl AC_SEARCH_LIBS([clock_gettime], [rt], [], [])
dnl AC_SEARCH_LIBS([mq_unlink], [rt], [], [])

save_LIBS="$LIBS"
LIBS=
AC_SEARCH_LIBS([cap_init], [cap], [], [AC_MSG_ERROR([*** POSIX caps library not found])])
CAP_LIBS="$LIBS"
AC_SUBST(CAP_LIBS)

AC_CHECK_FUNCS([__secure_getenv secure_getenv])
AC_CHECK_DECLS([
        memfd_create,
        gettid,
        pivot_root,
        name_to_handle_at,
        setns,
        getrandom,
        renameat2,
        kcmp,
        keyctl,
        LO_FLAGS_PARTSCAN,
        copy_file_range],
                [], [], [[
#include <sys/types.h>
#include <unistd.h>
#include <sys/mount.h>
#include <fcntl.h>
#include <sched.h>
#include <linux/loop.h>
#include <linux/random.h>
]])

AC_CHECK_TYPES([char16_t, char32_t, key_serial_t],
               [], [], [[
#include <uchar.h>
]])

AC_CHECK_DECLS([IFLA_INET6_ADDR_GEN_MODE,
                IN6_ADDR_GEN_MODE_STABLE_PRIVACY,
                IFLA_VRF_TABLE,
                IFLA_MACVLAN_FLAGS,
                IFLA_IPVLAN_MODE,
                IFLA_VTI_REMOTE,
                IFLA_PHYS_PORT_ID,
                IFLA_BOND_AD_INFO,
                IFLA_VLAN_PROTOCOL,
                IFLA_VXLAN_REMCSUM_NOPARTIAL,
                IFLA_IPTUN_ENCAP_DPORT,
                IFLA_GRE_ENCAP_DPORT,
                IFLA_BRIDGE_VLAN_INFO,
                IFLA_BRPORT_PROXYARP,
                IFLA_BRPORT_LEARNING_SYNC,
                IFLA_BR_VLAN_DEFAULT_PVID,
                NDA_IFINDEX,
                IFA_FLAGS],
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
#include <linux/if_addr.h>
#include <linux/neighbour.h>
]])

# This makes sure pkg.m4 is available.
m4_pattern_forbid([^_?PKG_[A-Z_]+$],[*** pkg.m4 missing, please install pkg-config])


# ------------------------------------------------------------------------------
PKG_CHECK_MODULES(UDEV, [libudev])
dnl
AC_ARG_WITH([udevrulesdir],
            AS_HELP_STRING([--with-udevrulesdir=DIR], [Directory for udev rules files]),
            [],
            [with_udevrulesdir=$($PKG_CONFIG --variable=udevdir udev)/rules.d])
AC_SUBST([udevrulesdir], [$with_udevrulesdir])

AC_ARG_WITH([udevbindir],
            AS_HELP_STRING([--with-udevbindir=DIR], [Directory for udev binary files]),
            [],
            [with_udevbindir=$($PKG_CONFIG --variable=udevdir udev)])
AC_SUBST([udevbindir], [$with_udevbindir])

# ------------------------------------------------------------------------------
have_utmp=yes
AC_ARG_ENABLE([utmp], AS_HELP_STRING([--disable-utmp], [disable utmp/wtmp log handling]),
        AS_CASE("x${enableval}",
                [xyes], [have_utmp=yes],
                [xno],  [have_utmp=no],
                AC_MSG_ERROR(bad value ${enableval} for --enable-utmp)))
AS_IF([test "x$have_utmp" = "xyes"], [AC_DEFINE(HAVE_UTMP, 1, [Define if utmp/wtmp support is enabled])])
AM_CONDITIONAL([HAVE_UTMP], [test "x$have_utmp" = "xyes"])

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

# ------------------------------------------------------------------------------
AC_ARG_WITH([kill-user-processes],
        [AS_HELP_STRING([--without-kill-user-processes], [Set elogind's KillUserProcesses=no by default])])
AS_IF([test "$with_kill_user_processes" != "no"],
       [kill_user_processes=true
        KILL_USER_PROCESSES=yes],
       [kill_user_processes=false
        KILL_USER_PROCESSES=no])
AC_DEFINE_UNQUOTED(KILL_USER_PROCESSES, [$kill_user_processes], [Default KillUserProcesses setting])
AC_SUBST(KILL_USER_PROCESSES)

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

if test "x${have_smack}" != xno; then
        AC_DEFINE(HAVE_SMACK, 1, [Define if SMACK is available])
        M4_DEFINES="$M4_DEFINES -DHAVE_SMACK"
        have_smack=yes
fi

AM_CONDITIONAL([HAVE_SMACK], [test "x$have_smack" = "xyes"])

have_smack_run_label=no
AC_ARG_WITH(smack-run-label,
AS_HELP_STRING([--with-smack-run-label=STRING],
        [run systemd --system itself with a specific SMACK label]),
        [AC_DEFINE_UNQUOTED(SMACK_RUN_LABEL, ["$withval"], [Run systemd itself with SMACK label]) have_smack_run_label=yes],
        [])

if test "x${have_smack_run_label}" = xyes; then
        M4_DEFINES="$M4_DEFINES -DHAVE_SMACK_RUN_LABEL"
fi

AC_ARG_WITH(smack-default-process-label,
AS_HELP_STRING([--with-smack-default-process-label=STRING],
        [default SMACK label for executed processes]),
        [AC_DEFINE_UNQUOTED(SMACK_DEFAULT_PROCESS_LABEL, ["$withval"], [Default SMACK label for executed processes])],
        [])

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
AC_CHECK_HEADERS_ONCE([valgrind/memcheck.h valgrind/valgrind.h])

# ------------------------------------------------------------------------------
have_manpages=no
AC_ARG_ENABLE(manpages, AS_HELP_STRING([--disable-manpages], [disable manpages]))
AC_PATH_PROG([XSLTPROC], [xsltproc])
AS_IF([test "x$enable_manpages" != xno], [have_manpages=yes])
AM_CONDITIONAL(ENABLE_MANPAGES, [test "x$have_manpages" = "xyes"])

# ------------------------------------------------------------------------------
AC_SUBST(M4_DEFINES)

AC_ARG_WITH([dbuspolicydir],
        AS_HELP_STRING([--with-dbuspolicydir=DIR], [D-Bus policy directory]),
        [],
        [with_dbuspolicydir=${sysconfdir}/dbus-1/system.d])
AX_NORMALIZE_PATH([with_dbuspolicydir])

AC_ARG_WITH([dbussystemservicedir],
        AS_HELP_STRING([--with-dbussystemservicedir=DIR], [D-Bus system service directory]),
        [],
        [with_dbussystemservicedir=${datadir}/dbus-1/system-services])
AX_NORMALIZE_PATH([with_dbussystemservicedir])

AC_ARG_WITH([bashcompletiondir],
        AS_HELP_STRING([--with-bashcompletiondir=DIR], [Bash completions directory]),
        [],
        [AS_IF([$($PKG_CONFIG --exists bash-completion)], [
                with_bashcompletiondir=$($PKG_CONFIG --variable=completionsdir bash-completion)
        ] , [
                with_bashcompletiondir=${datadir}/bash-completion/completions
        ])])
AM_CONDITIONAL(ENABLE_BASH_COMPLETION, [test "$with_bashcompletiondir" != "no"])
AX_NORMALIZE_PATH([with_bashcompletiondir])

AC_ARG_WITH([zshcompletiondir],
        AS_HELP_STRING([--with-zshcompletiondir=DIR], [Zsh completions directory]),
        [], [with_zshcompletiondir=${datadir}/zsh/site-functions])
AM_CONDITIONAL(ENABLE_ZSH_COMPLETION, [test "$with_zshcompletiondir" != "no"])
AX_NORMALIZE_PATH([with_zshcompletiondir])

AC_ARG_WITH([rootprefix],
        AS_HELP_STRING([--with-rootprefix=DIR], [rootfs directory prefix for config files and kernel modules]),
        [], [with_rootprefix=${ac_default_prefix}])
# --with-rootprefix= (empty) should default to "/" but AX_NORMALIZE_PATH
# defaults those to ".", solve that here for now until we can find a suitable
# fix for AX_NORMALIZE_PATH upstream at autoconf-archive.
# See: https://github.com/systemd/systemd/issues/54
if test "x${with_rootprefix}" = "x"; then
        with_rootprefix="/"
fi
AX_NORMALIZE_PATH([with_rootprefix])

AC_ARG_WITH([rootlibdir],
        AS_HELP_STRING([--with-rootlibdir=DIR], [Root directory for libraries necessary for boot]),
        [],
        [with_rootlibdir=${libdir}])
AX_NORMALIZE_PATH([with_rootlibdir])

AC_ARG_WITH([rootlibexecdir],
        AS_HELP_STRING([--with-rootlibexecdir=DIR], [Root directory for executables necessary for boot]),
        [],
        [with_rootlibexecdir=${libdir}/libexec])
AX_NORMALIZE_PATH([with_rootlibexecdir])

AC_ARG_WITH([pamlibdir],
        AS_HELP_STRING([--with-pamlibdir=DIR], [Directory for PAM modules]),
        [],
        [with_pamlibdir=${with_rootlibdir}/security])
AX_NORMALIZE_PATH([with_pamlibdir])

AC_ARG_WITH([pamconfdir],
        AS_HELP_STRING([--with-pamconfdir=DIR], [Directory for PAM configuration (pass no to disable installing)]),
        [],
        [with_pamconfdir=${sysconfdir}/pam.d])
AM_CONDITIONAL(ENABLE_PAM_CONFIG, [test "$with_pamconfdir" != "no"])
AX_NORMALIZE_PATH([with_pamconfdir])

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
AM_CONDITIONAL(ENABLE_SPLIT_USR, [test "x${enable_split_usr}" = "xyes"])

# work around intltool-update issues during 'make distcheck'
AS_IF([test "x$0" != "x./configure"], [
        AC_SUBST([INTLTOOL_UPDATE], [/usr/bin/env true])
])

AC_ARG_ENABLE(tests,
        [AC_HELP_STRING([--disable-tests], [disable tests])],
        enable_tests=$enableval, enable_tests=yes)
AM_CONDITIONAL(ENABLE_TESTS, [test x$enable_tests = xyes])

AC_ARG_ENABLE(debug,
        [AC_HELP_STRING([--enable-debug@<:@=LIST@:>@], [enable extra debugging (elogind,hashmap,mmap-cache)])],
        [if test "x$enableval" = "xyes"; then
                enableval="elogind,hashmap,mmap-cache"
        fi
        saved_ifs="$IFS"
        IFS="$IFS$PATH_SEPARATOR,"
        for name in $enableval; do
                case $name in
                elogind)
                        enable_debug_elogind=yes
                        ;;
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
AS_IF([test x$enable_debug_elogind = xyes], [
        AC_DEFINE(ENABLE_DEBUG_ELOGIND, 1, [Define if elogind debugging is to be enabled])
        enable_debug="elogind $enable_debug"
])
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
AC_SUBST([dbussystemservicedir], [$with_dbussystemservicedir])
AC_SUBST([bashcompletiondir], [$with_bashcompletiondir])
AC_SUBST([zshcompletiondir], [$with_zshcompletiondir])
AC_SUBST([pamlibdir], [$with_pamlibdir])
AC_SUBST([pamconfdir], [$with_pamconfdir])
AC_SUBST([rootprefix], [$with_rootprefix])
AC_SUBST([rootlibdir], [$with_rootlibdir])
AC_SUBST([rootlibexecdir], [$with_rootlibexecdir])
AC_SUBST([cgroup_controller], [$with_cgroupctrl])

AC_CONFIG_FILES([
        Makefile
        po/Makefile.in
])

AC_OUTPUT
AC_MSG_RESULT([
        $PACKAGE_NAME $PACKAGE_VERSION

        PAM: . . . . . . . . . . . . . . . ${have_pam}
        SELinux: . . . . . . . . . . . . . ${have_selinux}
        SMACK: . . . . . . . . . . . . . . ${have_smack}
        ACL: . . . . . . . . . . . . . . . ${have_acl}
        polkit: . . . . . . . . . . . . .  ${have_polkit}
        Python: . . . . . . . . . . . . .  ${have_python}
        man pages: . . . . . . . . . . . . ${have_manpages}
        test coverage: . . . . . . . . . . ${have_coverage}
        Split /usr: . . . . . . . . . . .  ${enable_split_usr}
        utmp/wtmp support: . . . . . . . . ${have_utmp}
        Link time optimization: . . . . .  ${have_lto}
        extra debugging: . . . . . . . . . ${enable_debug}
        cgroup controller: . . . . . . . . ${with_cgroupctrl}

        prefix: . . . . . . . . . . . . .  ${prefix}
        rootprefix: . . . . . . . . . . .  ${with_rootprefix}
        sysconf dir: . . . . . . . . . . . ${sysconfdir}
        datarootdir: . . . . . . . . . . . ${datarootdir}
        includedir: . . . . . . . . . . .  ${includedir}
        lib dir: . . . . . . . . . . . . . ${libdir}
        rootlib dir: . . . . . . . . . . . ${with_rootlibdir}
        rootlibexec dir: . . . . . . . . . ${with_rootlibexecdir}
        PAM modules dir: . . . . . . . . . ${with_pamlibdir}
        PAM configuration dir: . . . . . . ${with_pamconfdir}
        D-Bus policy dir: . . . . . . . .  ${with_dbuspolicydir}
        D-Bus system dir: . . . . . . . .  ${with_dbussystemservicedir}
        bash completions dir: . . . . . .  ${with_bashcompletiondir}
        zsh completions dir: . . . . . . . ${with_zshcompletiondir}
        maximum system UID: . . . . . . .  ${SYSTEM_UID_MAX}
        maximum system GID: . . . . . . .  ${SYSTEM_GID_MAX}

        CFLAGS: . . . . . . . . . . . . .  ${OUR_CFLAGS} ${CFLAGS}
        CPPFLAGS: . . . . . . . . . . . .  ${OUR_CPPFLAGS} ${CPPFLAGS}
        LDFLAGS: . . . . . . . . . . . . . ${OUR_LDFLAGS} ${LDFLAGS}
])
