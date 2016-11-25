#  -*- Mode: makefile; indent-tabs-mode: t -*-
#
#  This file is part of elogind.
#
#  Copyright 2010-2012 Lennart Poettering
#  Copyright 2010-2012 Kay Sievers
#  Copyright 2013 Zbigniew Jędrzejewski-Szmek
#  Copyright 2013 David Strauss
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
#  along with elogind; If not, see <http://www.gnu.org/licenses/>.

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}
AM_MAKEFLAGS = --no-print-directory
AUTOMAKE_OPTIONS = color-tests parallel-tests

GCC_COLORS ?= 'ooh, shiny!'
export GCC_COLORS

SUBDIRS = . po

# remove targets if the command fails
.DELETE_ON_ERROR:

# keep intermediate files
.SECONDARY:

# Keep the test-suite.log
.PRECIOUS: $(TEST_SUITE_LOG) Makefile

LIBELOGIND_CURRENT=7
LIBELOGIND_REVISION=0
LIBELOGIND_AGE=7

# Dirs of external packages
dbuspolicydir=@dbuspolicydir@
dbussessionservicedir=@dbussessionservicedir@
dbussystemservicedir=@dbussystemservicedir@
pamlibdir=@pamlibdir@
pamconfdir=@pamconfdir@
pkgconfiglibdir=$(libdir)/pkgconfig
polkitpolicydir=$(datadir)/polkit-1/actions
bashcompletiondir=@bashcompletiondir@
zshcompletiondir=@zshcompletiondir@
varlogdir=$(localstatedir)/log
elogindstatedir=$(localstatedir)/lib/elogind
systemsleepdir=$(pkglibexecdir)/sleep.d
systemshutdowndir=$(pkglibexecdir)/shutdown.d

udevrulesdir=@udevrulesdir@
udevbindir=@udevbindir@

PKTTYAGENT=$(bindir)/pkttyagent

# Our own, non-special dirs
pkgsysconfdir=$(sysconfdir)/elogind
pkgincludedir=$(includedir)/elogind
udevlibexecdir=$(udevbindir)
udevhomedir=$(udevlibexecdir)
udevhwdbdir=$(udevlibexecdir)/hwdb.d
factory_etcdir = $(datadir)/factory/etc
factory_pamdir = $(datadir)/factory/etc/pam.d

EXTRA_DIST =
BUILT_SOURCES =
INSTALL_EXEC_HOOKS =
UNINSTALL_EXEC_HOOKS =
INSTALL_DATA_HOOKS =
UNINSTALL_DATA_HOOKS =
DISTCLEAN_LOCAL_HOOKS =
CLEAN_LOCAL_HOOKS =
pkginclude_HEADERS =
noinst_LTLIBRARIES =
lib_LTLIBRARIES =
include_HEADERS =
noinst_DATA =
pkgconfiglib_DATA =
polkitpolicy_in_in_files =
polkitpolicy_in_files =
polkitpolicy_files =
dist_pkgsysconf_DATA =
nodist_pkgsysconf_DATA =
dist_pkgdata_DATA =
dist_dbuspolicy_DATA =
dist_dbussystemservice_DATA =
check_PROGRAMS =
check_DATA =
tests=
manual_tests =
TEST_EXTENSIONS = .py
PY_LOG_COMPILER = $(PYTHON)
if ENABLE_TESTS
noinst_PROGRAMS = $(manual_tests) $(tests)
TESTS = $(tests)
else
noinst_PROGRAMS =
TESTS =
endif

in_files = $(filter %.in,$(EXTRA_DIST))
in_in_files = $(filter %.in.in, $(in_files))
m4_files = $(filter %.m4,$(EXTRA_DIST) $(in_files:.m4.in=.m4))

CLEANFILES = $(BUILT_SOURCES) \
	$(pkgconfiglib_DATA) \
	$(in_files:.in=) $(in_in_files:.in.in=) \
	$(m4_files:.m4=)

.PHONY: $(INSTALL_EXEC_HOOKS) $(UNINSTALL_EXEC_HOOKS) \
	$(INSTALL_DATA_HOOKS) $(UNINSTALL_DATA_HOOKS) \
	$(DISTCLEAN_LOCAL_HOOKS) $(CLEAN_LOCAL_HOOKS)

AM_CPPFLAGS = \
	-include $(top_builddir)/config.h \
	-DPKGSYSCONFDIR=\"$(pkgsysconfdir)\" \
	-DPOLKIT_AGENT_BINARY_PATH=\"$(PKTTYAGENT)\" \
	-DSYSTEM_CONFIG_UNIT_PATH=\"$(pkgsysconfdir)/system\" \
	-DSYSTEM_DATA_UNIT_PATH=\"$(systemunitdir)\" \
	-DSYSTEMD_CGROUP_AGENT_PATH=\"$(pkglibexecdir)/logind-cgroups-agent\" \
	-DUSER_CONFIG_UNIT_PATH=\"$(pkgsysconfdir)/user\" \
	-DUSER_DATA_UNIT_PATH=\"$(userunitdir)\" \
	-DROOTPREFIX=\"$(rootprefix)\" \
	-DSYSTEM_SLEEP_PATH=\"$(systemsleepdir)\" \
	-DSYSTEM_SHUTDOWN_PATH=\"$(systemshutdowndir)\" \
	-DHALT=\"$(HALT)\" \
	-DREBOOT=\"$(REBOOT)\" \
	-DUDEVLIBEXECDIR=\"$(udevlibexecdir)\" \
	-DKEXEC=\"$(KEXEC)\" \
	-DLIBDIR=\"$(libdir)\" \
	-DROOTLIBDIR=\"$(rootlibdir)\" \
	-DTEST_DIR=\"$(abs_top_srcdir)/test\" \
	-I $(top_srcdir)/src \
	-I $(top_builddir)/src/shared \
	-I $(top_srcdir)/src/shared \
	-I $(top_srcdir)/src/login \
	-I $(top_srcdir)/src/systemd \
	-I $(top_srcdir)/src/libelogind/sd-bus \
	-I $(top_srcdir)/src/libelogind/sd-event \
	-I $(top_srcdir)/src/libelogind/sd-device \
	$(UDEV_CFLAGS) \
	$(OUR_CPPFLAGS)

AM_CFLAGS = $(OUR_CFLAGS)
AM_LDFLAGS = $(OUR_LDFLAGS)

# ------------------------------------------------------------------------------
SHUTDOWN_TARGET_WANTS =
LOCAL_FS_TARGET_WANTS =
MULTI_USER_TARGET_WANTS =
GRAPHICAL_TARGET_WANTS =
RESCUE_TARGET_WANTS =
SYSINIT_TARGET_WANTS =
SOCKETS_TARGET_WANTS =
BUSNAMES_TARGET_WANTS =
TIMERS_TARGET_WANTS =
USER_SOCKETS_TARGET_WANTS =
USER_DEFAULT_TARGET_WANTS =
USER_BUSNAMES_TARGET_WANTS =

SYSTEM_UNIT_ALIASES =
USER_UNIT_ALIASES =
GENERAL_ALIASES =

install-target-wants-hook:
	what="$(SHUTDOWN_TARGET_WANTS)" && wants=shutdown.target && dir=$(systemunitdir) && $(add-wants)
	what="$(LOCAL_FS_TARGET_WANTS)" && wants=local-fs.target && dir=$(systemunitdir) && $(add-wants)
	what="$(MULTI_USER_TARGET_WANTS)" && wants=multi-user.target && dir=$(systemunitdir) && $(add-wants)
	what="$(GRAPHICAL_TARGET_WANTS)" && wants=graphical.target && dir=$(systemunitdir) && $(add-wants)
	what="$(RESCUE_TARGET_WANTS)" && wants=rescue.target && dir=$(systemunitdir) && $(add-wants)
	what="$(SYSINIT_TARGET_WANTS)" && wants=sysinit.target && dir=$(systemunitdir) && $(add-wants)
	what="$(SOCKETS_TARGET_WANTS)" && wants=sockets.target && dir=$(systemunitdir) && $(add-wants)
	what="$(TIMERS_TARGET_WANTS)" && wants=timers.target && dir=$(systemunitdir) && $(add-wants)
	what="$(SLICES_TARGET_WANTS)" && wants=slices.target && dir=$(systemunitdir) && $(add-wants)
	what="$(USER_SOCKETS_TARGET_WANTS)" && wants=sockets.target && dir=$(userunitdir) && $(add-wants)
	what="$(USER_DEFAULT_TARGET_WANTS)" && wants=default.target && dir=$(userunitdir) && $(add-wants)

install-busnames-target-wants-hook:
	what="$(BUSNAMES_TARGET_WANTS)" && wants=busnames.target && dir=$(systemunitdir) && $(add-wants)
	what="$(USER_BUSNAMES_TARGET_WANTS)" && wants=busnames.target && dir=$(userunitdir) && $(add-wants)

define add-wants
	[ -z "$$what" ] || ( \
	  dir=$(DESTDIR)$$dir/$$wants.wants && \
	  $(MKDIR_P) -m 0755 $$dir && \
	  cd $$dir && \
	  rm -f $$what && \
	  for i in $$what; do $(LN_S) ../$$i . || exit $$? ; done )
endef

install-aliases-hook:
	set -- $(GENERAL_ALIASES) && \
		dir= && $(install-relative-aliases)

define install-aliases
	while [ -n "$$1" ]; do \
		$(MKDIR_P) `dirname $(DESTDIR)$$dir/$$2` && \
		rm -f $(DESTDIR)$$dir/$$2 && \
		$(LN_S) $$1 $(DESTDIR)$$dir/$$2 && \
		shift 2 || exit $$?; \
	done
endef

define install-relative-aliases
	while [ -n "$$1" ]; do \
		$(MKDIR_P) `dirname $(DESTDIR)$$dir/$$2` && \
		rm -f $(DESTDIR)$$dir/$$2 && \
		$(LN_S) --relative $(DESTDIR)$$1 $(DESTDIR)$$dir/$$2 && \
		shift 2 || exit $$?; \
	done
endef

install-touch-usr-hook:
	touch -c $(DESTDIR)/$(prefix)

INSTALL_EXEC_HOOKS += \
	install-target-wants-hook \
	install-aliases-hook \
	install-touch-usr-hook

if ENABLE_KDBUS
INSTALL_EXEC_HOOKS += \
	install-busnames-target-wants-hook
endif

# ------------------------------------------------------------------------------
AM_V_M4 = $(AM_V_M4_$(V))
AM_V_M4_ = $(AM_V_M4_$(AM_DEFAULT_VERBOSITY))
AM_V_M4_0 = @echo "  M4      " $@;

AM_V_XSLT = $(AM_V_XSLT_$(V))
AM_V_XSLT_ = $(AM_V_XSLT_$(AM_DEFAULT_VERBOSITY))
AM_V_XSLT_0 = @echo "  XSLT    " $@;

AM_V_GPERF = $(AM_V_GPERF_$(V))
AM_V_GPERF_ = $(AM_V_GPERF_$(AM_DEFAULT_VERBOSITY))
AM_V_GPERF_0 = @echo "  GPERF   " $@;

AM_V_LN = $(AM_V_LN_$(V))
AM_V_LN_ = $(AM_V_LN_$(AM_DEFAULT_VERBOSITY))
AM_V_LN_0 = @echo "  LN      " $@;

AM_V_RM = $(AM_V_RM_$(V))
AM_V_RM_ = $(AM_V_RM_$(AM_DEFAULT_VERBOSITY))
AM_V_RM_0 = @echo "  RM      " $@;

# ------------------------------------------------------------------------------
bin_PROGRAMS =
pkglibexec_PROGRAMS =
dist_bashcompletion_DATA =
dist_zshcompletion_DATA =

dist_doc_DATA = \
	README \
	NEWS \
	LICENSE.LGPL2.1 \
	LICENSE.GPL2 \
	LICENSE.MIT \
	src/libelogind/sd-bus/PORTING-DBUS1 \
	src/libelogind/sd-bus/DIFFERENCES \
	src/libelogind/sd-bus/GVARIANT-SERIALIZATION

@INTLTOOL_POLICY_RULE@

# ------------------------------------------------------------------------------

MANPAGES =
MANPAGES_ALIAS =

include Makefile-man.am

.PHONY: man update-man-list
man: $(MANPAGES) $(MANPAGES_ALIAS) $(HTML_FILES) $(HTML_ALIAS)

XML_FILES = \
	${patsubst %.1,%.xml,${patsubst %.3,%.xml,${patsubst %.5,%.xml,${patsubst %.7,%.xml,${patsubst %.8,%.xml,$(MANPAGES)}}}}}
HTML_FILES = \
	${XML_FILES:.xml=.html}
HTML_ALIAS = \
	${patsubst %.1,%.html,${patsubst %.3,%.html,${patsubst %.5,%.html,${patsubst %.7,%.html,${patsubst %.8,%.html,$(MANPAGES_ALIAS)}}}}}

if ENABLE_MANPAGES
man_MANS = \
	$(MANPAGES) \
	$(MANPAGES_ALIAS)

noinst_DATA += \
	$(HTML_FILES) \
	$(HTML_ALIAS)

CLEANFILES += \
	$(man_MANS) \
	$(HTML_FILES) \
	$(HTML_ALIAS)

docs/html/man:
	$(AM_V_at)$(MKDIR_P) $(dir $@)
	$(AM_V_LN)$(LN_S) -f ../../man $@

noinst_DATA += \
	docs/html/man

CLEANFILES += \
	docs/html/man

endif

EXTRA_DIST += \
	$(XML_FILES) \
	$(HTML_FILES) \
	$(HTML_ALIAS) \
	$(man_MANS) \
	tools/make-man-index.py \
	tools/make-directive-index.py \
	tools/xml_helper.py

# ------------------------------------------------------------------------------
noinst_LTLIBRARIES += \
	libelogind-shared.la

libelogind_shared_la_SOURCES = \
	src/shared/capability.c \
	src/shared/capability.h \
	src/shared/linux/auto_dev-ioctl.h \
	src/shared/ioprio.h \
	src/shared/missing.h \
	src/shared/initreq.h \
	src/shared/securebits.h \
	src/shared/special.h \
	src/shared/list.h \
	src/shared/unaligned.h \
	src/shared/macro.h \
	src/shared/def.h \
	src/shared/sparse-endian.h \
	src/shared/refcnt.h \
	src/shared/udev-util.h \
	src/shared/device-nodes.c \
	src/shared/device-nodes.h \
	src/shared/util.c \
	src/shared/util.h \
	src/shared/virt.c \
	src/shared/virt.h \
	src/shared/architecture.c \
	src/shared/architecture.h \
	src/shared/fstab-util.c \
	src/shared/fstab-util.h \
	src/shared/path-util.c \
	src/shared/path-util.h \
	src/shared/time-util.c \
	src/shared/time-util.h \
	src/shared/locale-util.c \
	src/shared/locale-util.h \
	src/shared/mempool.c \
	src/shared/mempool.h \
	src/shared/hashmap.c \
	src/shared/hashmap.h \
	src/shared/siphash24.c \
	src/shared/siphash24.h \
	src/shared/set.h \
	src/shared/fdset.c \
	src/shared/fdset.h \
	src/shared/prioq.c \
	src/shared/prioq.h \
	src/shared/sleep-config.c \
	src/shared/sleep-config.h \
	src/shared/strv.c \
	src/shared/strv.h \
	src/shared/env-util.c \
	src/shared/env-util.h \
	src/shared/strbuf.c \
	src/shared/strbuf.h \
	src/shared/strxcpyx.c \
	src/shared/strxcpyx.h \
	src/shared/conf-parser.c \
	src/shared/conf-parser.h \
	src/shared/log.c \
	src/shared/log.h \
	src/shared/ratelimit.h \
	src/shared/ratelimit.c \
	src/shared/exit-status.c \
	src/shared/exit-status.h \
	src/shared/utf8.c \
	src/shared/utf8.h \
	src/shared/gunicode.c \
	src/shared/gunicode.h \
	src/shared/pager.c \
	src/shared/pager.h \
	src/shared/socket-util.c \
	src/shared/socket-util.h \
	src/shared/in-addr-util.c \
	src/shared/in-addr-util.h \
	src/shared/ether-addr-util.h \
	src/shared/conf-files.c \
	src/shared/conf-files.h \
	src/shared/cgroup-util.c \
	src/shared/cgroup-util.h \
	src/shared/cgroup-show.c \
	src/shared/cgroup-show.h \
	src/shared/logs-show.h \
	src/shared/unit-name.c \
	src/shared/unit-name.h \
	src/shared/watchdog.c \
	src/shared/watchdog.h \
	src/shared/replace-var.c \
	src/shared/replace-var.h \
	src/shared/spawn-polkit-agent.c \
	src/shared/spawn-polkit-agent.h \
	src/shared/clock-util.c \
	src/shared/clock-util.h \
	src/shared/calendarspec.c \
	src/shared/calendarspec.h \
	src/shared/fileio.c \
	src/shared/fileio.h \
	src/shared/output-mode.h \
	src/shared/MurmurHash2.c \
	src/shared/MurmurHash2.h \
	src/shared/acpi-fpdt.h \
	src/shared/acpi-fpdt.c \
	src/shared/mkdir.c \
	src/shared/mkdir.h \
	src/shared/smack-util.c \
	src/shared/smack-util.h \
	src/shared/apparmor-util.c \
	src/shared/apparmor-util.h \
	src/shared/ima-util.c \
	src/shared/ima-util.h \
	src/shared/ptyfwd.c \
	src/shared/ptyfwd.h \
	src/shared/errno-list.c \
	src/shared/errno-list.h \
	src/shared/af-list.c \
	src/shared/af-list.h \
	src/shared/arphrd-list.c \
	src/shared/arphrd-list.h \
	src/shared/cap-list.c \
	src/shared/cap-list.h \
	src/shared/audit.c \
	src/shared/audit.h \
	src/shared/xml.c \
	src/shared/xml.h \
	src/shared/json.c \
	src/shared/json.h \
	src/shared/selinux-util.c \
	src/shared/selinux-util.h \
	src/shared/label.c \
	src/shared/label.h \
	src/shared/socket-label.c \
	src/shared/mkdir-label.c \
	src/shared/fileio-label.h \
	src/shared/fileio-label.c \
	src/shared/bus-label.c \
	src/shared/bus-label.h \
	src/shared/gpt.h \
	src/shared/clean-ipc.h \
	src/shared/clean-ipc.c \
	src/shared/login-shared.c \
	src/shared/login-shared.h \
	src/shared/ring.c \
	src/shared/ring.h \
	src/shared/barrier.c \
	src/shared/barrier.h \
	src/shared/pty.c \
	src/shared/pty.h \
	src/shared/async.c \
	src/shared/async.h \
	src/shared/base-filesystem.c \
	src/shared/base-filesystem.h \
	src/shared/memfd-util.c \
	src/shared/memfd-util.h \
	src/shared/process-util.c \
	src/shared/process-util.h \
	src/shared/random-util.c \
	src/shared/random-util.h \
	src/shared/terminal-util.c \
	src/shared/terminal-util.h \
	src/shared/uid-range.c \
	src/shared/uid-range.h \
	src/shared/nss-util.h \
	src/shared/verbs.c \
	src/shared/verbs.h \
	src/shared/sigbus.c \
	src/shared/sigbus.h \
	src/shared/build.h \
	src/shared/import-util.c \
	src/shared/import-util.h \
	src/shared/sysctl-util.c \
	src/shared/sysctl-util.h \
	src/shared/hostname-util.h \
	src/shared/hostname-util.c

nodist_libelogind_shared_la_SOURCES = \
	src/shared/errno-from-name.h \
	src/shared/errno-to-name.h \
	src/shared/af-from-name.h \
	src/shared/af-to-name.h \
	src/shared/arphrd-from-name.h \
	src/shared/arphrd-to-name.h \
	src/shared/cap-from-name.h \
	src/shared/cap-to-name.h

libelogind_shared_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(CAP_CFLAGS) \
	$(SECCOMP_CFLAGS) \
	-pthread

libelogind_shared_la_LIBADD = \
	$(CAP_LIBS) \
	-lm

libelogind_shared_la_LDFLAGS = \
	$(CAP_LDFLAGS)

# ------------------------------------------------------------------------------
if HAVE_ACL
noinst_LTLIBRARIES += \
	libelogind-acl.la

libelogind_acl_la_SOURCES = \
	src/shared/acl-util.c \
	src/shared/acl-util.h

libelogind_acl_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(ACL_CFLAGS)

libelogind_acl_la_LIBADD = \
	$(ACL_LIBS)
endif

gperf_txt_sources = \
	src/shared/errno-list.txt \
	src/shared/af-list.txt \
	src/shared/arphrd-list.txt \
	src/shared/cap-list.txt

BUILT_SOURCES += \
	$(gperf_gperf_sources:-gperf.gperf=-gperf.c) \
	$(gperf_txt_sources:-list.txt=-from-name.h) \
	$(gperf_txt_sources:-list.txt=-to-name.h)

CLEANFILES += \
	$(gperf_txt_sources:-list.txt=-from-name.gperf)
DISTCLEANFILES = \
	$(gperf_txt_sources)

EXTRA_DIST += \
	$(gperf_gperf_m4_sources) \
	$(gperf_gperf_sources) \
	$(gperf_txt_sources:-list.txt=-from-name.gperf)

CLEANFILES += \
	$(gperf_txt_sources)

%-from-name.gperf: %-list.txt
	$(AM_V_GEN)$(AWK) 'BEGIN{ print "struct $(notdir $*)_name { const char* name; int id; };"; print "%null-strings"; print "%%";} { printf "%s, %s\n", $$1, $$1 }' <$< >$@

%-from-name.h: %-from-name.gperf
	$(AM_V_GPERF)$(GPERF) -L ANSI-C -t --ignore-case -N lookup_$(notdir $*) -H hash_$(notdir $*)_name -p -C <$< >$@


src/shared/errno-list.txt:
	$(AM_V_at)$(MKDIR_P) $(dir $@)
	$(AM_V_GEN)$(CPP) $(CFLAGS) $(AM_CPPFLAGS) $(CPPFLAGS) -dM -include errno.h - </dev/null | $(AWK) '/^#define[ \t]+E[^ _]+[ \t]+/ { print $$2; }' >$@

src/shared/errno-to-name.h: src/shared/errno-list.txt
	$(AM_V_GEN)$(AWK) 'BEGIN{ print "static const char* const errno_names[] = { "} !/EDEADLOCK/ && !/EWOULDBLOCK/ && !/ENOTSUP/ { printf "[%s] = \"%s\",\n", $$1, $$1 } END{print "};"}' <$< >$@


src/shared/af-list.txt:
	$(AM_V_at)$(MKDIR_P) $(dir $@)
	$(AM_V_GEN)$(CPP) $(CFLAGS) $(AM_CPPFLAGS) $(CPPFLAGS) -dM -include sys/socket.h - </dev/null | grep -v AF_UNSPEC | grep -v AF_MAX | $(AWK) '/^#define[ \t]+AF_[^ \t]+[ \t]+PF_[^ \t]/ { print $$2; }' >$@

src/shared/af-to-name.h: src/shared/af-list.txt
	$(AM_V_GEN)$(AWK) 'BEGIN{ print "static const char* const af_names[] = { "} !/AF_FILE/ && !/AF_ROUTE/ && !/AF_LOCAL/ { printf "[%s] = \"%s\",\n", $$1, $$1 } END{print "};"}' <$< >$@


src/shared/arphrd-list.txt:
	$(AM_V_at)$(MKDIR_P) $(dir $@)
	$(AM_V_GEN)$(CPP) $(CFLAGS) $(AM_CPPFLAGS) $(CPPFLAGS) -dM -include net/if_arp.h - </dev/null | $(AWK) '/^#define[ \t]+ARPHRD_[^ \t]+[ \t]+[^ \t]/ { print $$2; }' | sed -e 's/ARPHRD_//' >$@

src/shared/arphrd-to-name.h: src/shared/arphrd-list.txt
	$(AM_V_GEN)$(AWK) 'BEGIN{ print "static const char* const arphrd_names[] = { "} !/CISCO/ { printf "[ARPHRD_%s] = \"%s\",\n", $$1, $$1 } END{print "};"}' <$< >$@

src/shared/arphrd-from-name.gperf: src/shared/arphrd-list.txt
	$(AM_V_GEN)$(AWK) 'BEGIN{ print "struct arphrd_name { const char* name; int id; };"; print "%null-strings"; print "%%";} { printf "%s, ARPHRD_%s\n", $$1, $$1 }' <$< >$@


src/shared/cap-list.txt:
	$(AM_V_at)$(MKDIR_P) $(dir $@)
	$(AM_V_GEN)$(CPP) $(CFLAGS) $(AM_CPPFLAGS) $(CPPFLAGS) -dM -include linux/capability.h -include missing.h - </dev/null | $(AWK) '/^#define[ \t]+CAP_[A-Z_]+[ \t]+/ { print $$2; }' | grep -v CAP_LAST_CAP >$@

src/shared/cap-to-name.h: src/shared/cap-list.txt
	$(AM_V_GEN)$(AWK) 'BEGIN{ print "static const char* const capability_names[] = { "} { printf "[%s] = \"%s\",\n", $$1, tolower($$1) } END{print "};"}' <$< >$@

src/shared/cap-from-name.gperf: src/shared/cap-list.txt
	$(AM_V_GEN)$(AWK) 'BEGIN{ print "struct capability_name { const char* name; int id; };"; print "%null-strings"; print "%%";} { printf "%s, %s\n", $$1, $$1 }' <$< >$@

src/shared/cap-from-name.h: src/shared/cap-from-name.gperf
	$(AM_V_GPERF)$(GPERF) -L ANSI-C -t --ignore-case -N lookup_capability -H hash_capability_name -p -C <$< >$@

# ------------------------------------------------------------------------------
dist_factory_etc_DATA =

if HAVE_PAM
dist_factory_pam_DATA = \
	factory/etc/pam.d/system-auth \
	factory/etc/pam.d/other
endif

# ------------------------------------------------------------------------------
noinst_LTLIBRARIES += \
	libelogind-internal.la

libelogind_internal_la_SOURCES = \
	src/systemd/sd-bus.h \
	src/systemd/sd-bus-protocol.h \
	src/systemd/sd-bus-vtable.h \
	src/systemd/sd-utf8.h \
	src/systemd/sd-event.h \
	src/systemd/sd-rtnl.h \
	src/systemd/sd-resolve.h \
	src/systemd/sd-login.h \
	src/systemd/sd-id128.h \
	src/systemd/sd-daemon.h \
	src/systemd/sd-path.h \
	src/systemd/sd-network.h \
	src/systemd/sd-hwdb.h \
	src/systemd/sd-device.h \
	src/libelogind/sd-bus/sd-bus.c \
	src/libelogind/sd-bus/bus-control.c \
	src/libelogind/sd-bus/bus-control.h \
	src/libelogind/sd-bus/bus-error.c \
	src/libelogind/sd-bus/bus-error.h \
	src/libelogind/sd-bus/bus-common-errors.h \
	src/libelogind/sd-bus/bus-common-errors.c \
	src/libelogind/sd-bus/bus-internal.c \
	src/libelogind/sd-bus/bus-internal.h \
	src/libelogind/sd-bus/bus-socket.c \
	src/libelogind/sd-bus/bus-socket.h \
	src/libelogind/sd-bus/bus-kernel.c \
	src/libelogind/sd-bus/bus-kernel.h \
	src/libelogind/sd-bus/bus-container.c \
	src/libelogind/sd-bus/bus-container.h \
	src/libelogind/sd-bus/bus-message.c \
	src/libelogind/sd-bus/bus-message.h \
	src/libelogind/sd-bus/bus-creds.c \
	src/libelogind/sd-bus/bus-creds.h \
	src/libelogind/sd-bus/bus-signature.c \
	src/libelogind/sd-bus/bus-signature.h \
	src/libelogind/sd-bus/bus-type.c \
	src/libelogind/sd-bus/bus-type.h \
	src/libelogind/sd-bus/bus-match.c \
	src/libelogind/sd-bus/bus-match.h \
	src/libelogind/sd-bus/bus-bloom.c \
	src/libelogind/sd-bus/bus-bloom.h \
	src/libelogind/sd-bus/bus-introspect.c \
	src/libelogind/sd-bus/bus-introspect.h \
	src/libelogind/sd-bus/bus-objects.c \
	src/libelogind/sd-bus/bus-objects.h \
	src/libelogind/sd-bus/bus-gvariant.c \
	src/libelogind/sd-bus/bus-gvariant.h \
	src/libelogind/sd-bus/bus-convenience.c \
	src/libelogind/sd-bus/bus-track.c \
	src/libelogind/sd-bus/bus-track.h \
	src/libelogind/sd-bus/bus-util.c \
	src/libelogind/sd-bus/bus-util.h \
	src/libelogind/sd-bus/bus-slot.c \
	src/libelogind/sd-bus/bus-slot.h \
	src/libelogind/sd-bus/bus-protocol.h \
	src/libelogind/sd-bus/kdbus.h \
	src/libelogind/sd-utf8/sd-utf8.c \
	src/libelogind/sd-event/sd-event.c \
	src/libelogind/sd-event/event-util.h \
	src/libelogind/sd-rtnl/sd-rtnl.c \
	src/libelogind/sd-rtnl/rtnl-internal.h \
	src/libelogind/sd-rtnl/rtnl-message.c \
	src/libelogind/sd-rtnl/rtnl-types.h \
	src/libelogind/sd-rtnl/rtnl-types.c \
	src/libelogind/sd-rtnl/rtnl-util.h \
	src/libelogind/sd-rtnl/rtnl-util.c \
	src/libelogind/sd-rtnl/local-addresses.h \
	src/libelogind/sd-rtnl/local-addresses.c \
	src/libelogind/sd-id128/sd-id128.c \
	src/libelogind/sd-daemon/sd-daemon.c \
	src/libelogind/sd-login/sd-login.c \
	src/libelogind/sd-path/sd-path.c \
	src/libelogind/sd-network/sd-network.c \
	src/libelogind/sd-network/network-util.h \
	src/libelogind/sd-network/network-util.c \
	src/libelogind/sd-hwdb/sd-hwdb.c \
	src/libelogind/sd-hwdb/hwdb-util.h \
	src/libelogind/sd-hwdb/hwdb-internal.h \
	src/libelogind/sd-device/device-internal.h \
	src/libelogind/sd-device/device-util.h \
	src/libelogind/sd-device/device-enumerator.c \
	src/libelogind/sd-device/device-enumerator-private.h \
	src/libelogind/sd-device/sd-device.c \
	src/libelogind/sd-device/device-private.c \
	src/libelogind/sd-device/device-private.h

nodist_libelogind_internal_la_SOURCES = \
	src/libelogind/libelogind.sym

libelogind_internal_la_CFLAGS = \
	$(AM_CFLAGS) \
	-pthread

libelogind_internal_la_LIBADD = \
	libelogind-shared.la

libelogind_internal_la_LDFLAGS = \
	$(AM_LDFLAGS)

lib_LTLIBRARIES += \
	libelogind.la

libelogind_la_SOURCES =

libelogind_la_CFLAGS = \
	$(AM_CFLAGS)

libelogind_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-version-info $(LIBELOGIND_CURRENT):$(LIBELOGIND_REVISION):$(LIBELOGIND_AGE) \
	-Wl,--version-script=src/libelogind/libelogind.sym

libelogind_la_LIBADD = \
	libelogind-internal.la

pkgconfiglib_DATA += \
	src/libelogind/libelogind.pc

pkginclude_HEADERS += \
	src/systemd/sd-login.h \
	src/systemd/sd-messages.h \
	src/systemd/sd-journal.h \
	src/systemd/_sd-common.h

EXTRA_DIST += \
	src/libelogind/libelogind.sym.m4 \
	src/libelogind/libelogind.pc.in \
	src/libelogind/sd-bus/DIFFERENCES \
	src/libelogind/sd-bus/GVARIANT-SERIALIZATION

BUILT_SOURCES += \
       src/libelogind/libelogind.sym

# ------------------------------------------------------------------------------
elogind_SOURCES = \
	src/login/logind.c \
	src/login/logind.h

nodist_elogind_SOURCES = \
	src/login/logind-gperf.c

elogind_LDADD = \
	libelogind-core.la

libelogind_core_la_SOURCES = \
	src/login/logind-core.c \
	src/login/logind-device.c \
	src/login/logind-device.h \
	src/login/logind-button.c \
	src/login/logind-button.h \
	src/login/logind-action.c \
	src/login/logind-action.h \
	src/login/logind-seat.c \
	src/login/logind-seat.h \
	src/login/logind-session.c \
	src/login/logind-session.h \
	src/login/logind-session-device.c \
	src/login/logind-session-device.h \
	src/login/logind-user.c \
	src/login/logind-user.h \
	src/login/logind-inhibit.c \
	src/login/logind-inhibit.h \
	src/login/logind-dbus.c \
	src/login/logind-session-dbus.c \
	src/login/logind-seat-dbus.c \
	src/login/logind-user-dbus.c \
	src/login/logind-acl.h

libelogind_core_la_LIBADD = \
	$(UDEV_LIBS) \
	libelogind-internal.la

if HAVE_ACL
libelogind_core_la_SOURCES += \
	src/login/logind-acl.c

libelogind_core_la_LIBADD += \
	libelogind-acl.la
endif

noinst_LTLIBRARIES += \
	libelogind-core.la

pkglibexec_PROGRAMS += elogind

loginctl_SOURCES = \
	src/login/loginctl.c \
	src/login/sysfs-show.h \
	src/login/sysfs-show.c

loginctl_LDADD = \
	$(UDEV_LIBS) \
	libelogind-internal.la \
	libelogind-shared.la

bin_PROGRAMS += \
	loginctl

logind_cgroups_agent_SOURCES = \
	src/cgroups-agent/cgroups-agent.c

logind_cgroups_agent_LDADD = \
	libelogind-internal.la \
	libelogind-shared.la

pkglibexec_PROGRAMS += \
	logind-cgroups-agent

dist_bashcompletion_DATA += \
	shell-completion/bash/loginctl

dist_zshcompletion_DATA += \
	shell-completion/zsh/_loginctl

systemd_inhibit_SOURCES = \
	src/login/inhibit.c

systemd_inhibit_LDADD = \
	libelogind-internal.la \
	libelogind-shared.la

bin_PROGRAMS += \
	systemd-inhibit

test_login_SOURCES = \
	src/libelogind/sd-login/test-login.c

test_login_LDADD = \
	libelogind-internal.la \
	libelogind-shared.la

test_login_shared_SOURCES = \
	src/login/test-login-shared.c

test_login_shared_LDADD = \
	libelogind-internal.la \
	libelogind-shared.la

test_inhibit_SOURCES = \
	src/login/test-inhibit.c

test_inhibit_LDADD = \
	libelogind-internal.la \
	libelogind-shared.la

test_login_tables_SOURCES = \
	src/login/test-login-tables.c \
	src/shared/test-tables.h

test_login_tables_LDADD = \
	libelogind-core.la

manual_tests += \
	test-login \
	test-inhibit

tests += \
	test-login-tables \
	test-login-shared

if HAVE_PAM
pam_elogind_la_SOURCES = \
	src/login/pam_elogind.sym \
	src/login/pam_elogind.c

pam_elogind_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(PAM_CFLAGS) \
	-fvisibility=hidden

pam_elogind_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-module \
	-export-dynamic \
	-avoid-version \
	-shared \
	-Wl,--version-script=$(top_srcdir)/src/login/pam_elogind.sym

pam_elogind_la_LIBADD = \
	libelogind-internal.la \
	$(PAM_LIBS)

pamlib_LTLIBRARIES = \
	pam_elogind.la

dist_pamconf_DATA = \
	src/login/elogind-user

endif

dist_dbussystemservice_DATA += \
	src/login/org.freedesktop.login1.service

dist_dbuspolicy_DATA += \
	src/login/org.freedesktop.login1.conf

dist_pkgsysconf_DATA += \
	src/login/logind.conf

polkitpolicy_files += \
	src/login/org.freedesktop.login1.policy

SYSTEM_UNIT_ALIASES += \
	elogind.service dbus-org.freedesktop.login1.service

dist_udevrules_DATA = \
	src/login/70-uaccess.rules \
	src/login/70-power-switch.rules

nodist_udevrules_DATA = \
	src/login/71-seat.rules \
	src/login/73-seat-late.rules

polkitpolicy_in_files += \
	src/login/org.freedesktop.login1.policy.in

gperf_gperf_sources = \
	src/login/logind-gperf.gperf

EXTRA_DIST += \
	src/login/71-seat.rules.in \
	src/login/73-seat-late.rules.in

# ------------------------------------------------------------------------------
substitutions = \
       '|bindir=$(bindir)|' \
       '|udevbindir=$(udevbindir)|' \
       '|pkgsysconfdir=$(pkgsysconfdir)|' \
       '|pkgdatadir=$(pkgdatadir)|' \
       '|PACKAGE_VERSION=$(PACKAGE_VERSION)|' \
       '|PACKAGE_NAME=$(PACKAGE_NAME)|' \
       '|PACKAGE_URL=$(PACKAGE_URL)|' \
       '|prefix=$(prefix)|' \
       '|exec_prefix=$(exec_prefix)|' \
       '|libdir=$(libdir)|' \
       '|includedir=$(includedir)|' \
       '|VERSION=$(VERSION)|' \
       '|rootprefix=$(rootprefix)|' \
       '|MKDIR_P=$(MKDIR_P)|' \
       '|systemuidmax=$(SYSTEM_UID_MAX)|' \
       '|systemgidmax=$(SYSTEM_GID_MAX)|'

SED_PROCESS = \
	$(AM_V_GEN)$(MKDIR_P) $(dir $@) && \
	$(SED) $(subst '|,-e 's|@,$(subst =,\@|,$(subst |',|g',$(substitutions)))) \
		< $< > $@

units/%: units/%.in
	$(SED_PROCESS)

man/%: man/%.in
	$(SED_PROCESS)

sysctl.d/%: sysctl.d/%.in
	$(SED_PROCESS)

%.pc: %.pc.in
	$(SED_PROCESS)

%.conf: %.conf.in
	$(SED_PROCESS)

src/%.policy.in: src/%.policy.in.in
	$(SED_PROCESS)

shell-completion/%: shell-completion/%.in
	$(SED_PROCESS)

%.rules: %.rules.in
	$(SED_PROCESS)

%.conf: %.conf.in
	$(SED_PROCESS)

%.sh: %.sh.in
	$(SED_PROCESS)
	$(AM_V_GEN)chmod +x $@

src/%.c: src/%.gperf
	$(AM_V_at)$(MKDIR_P) $(dir $@)
	$(AM_V_GPERF)$(GPERF) < $< > $@

src/%: src/%.m4
	$(AM_V_at)$(MKDIR_P) $(dir $@)
	$(AM_V_M4)$(M4) -P $(M4_DEFINES) < $< > $@

sysusers.d/%: sysusers.d/%.m4
	$(AM_V_at)$(MKDIR_P) $(dir $@)
	$(AM_V_M4)$(M4) -P $(M4_DEFINES) < $< > $@

tmpfiles.d/%: tmpfiles.d/%.m4
	$(AM_V_at)$(MKDIR_P) $(dir $@)
	$(AM_V_M4)$(M4) -P $(M4_DEFINES) < $< > $@


units/%: units/%.m4
	$(AM_V_at)$(MKDIR_P) $(dir $@)
	$(AM_V_M4)$(M4) -P $(M4_DEFINES) -DFOR_SYSTEM=1 < $< > $@

units/user/%: units/user/%.m4
	$(AM_V_at)$(MKDIR_P) $(dir $@)
	$(AM_V_M4)$(M4) -P $(M4_DEFINES) -DFOR_USER=1 < $< > $@

if ENABLE_POLKIT
nodist_polkitpolicy_DATA = \
	$(polkitpolicy_files) \
	$(polkitpolicy_in_in_files:.policy.in.in=.policy)
endif

EXTRA_DIST += \
	$(polkitpolicy_in_files) \
	$(polkitpolicy_in_in_files)

# ------------------------------------------------------------------------------
if ENABLE_MANPAGES
man/custom-entities.ent: configure.ac
	$(AM_V_GEN)$(MKDIR_P) $(dir $@)
	$(AM_V_GEN)(echo '<?xml version="1.0" encoding="utf-8" ?>' && \
	 printf '$(subst '|,<!ENTITY ,$(subst =, ",$(subst |',">\n,$(substitutions))))') \
	 > $@ # '

DISTCLEANFILES += \
	man/custom-entities.ent

XSLTPROC_FLAGS = \
	--nonet \
	--xinclude \
	--stringparam man.output.quietly 1 \
	--stringparam funcsynopsis.style ansi \
	--stringparam man.authors.section.enabled 0 \
	--stringparam man.copyright.section.enabled 0 \
	--stringparam elogind.version $(VERSION) \
	--path '$(builddir)/man:$(srcdir)/man'

XSLTPROC_PROCESS_MAN = \
	$(AM_V_XSLT)$(XSLTPROC) -o $@ $(XSLTPROC_FLAGS) $(srcdir)/man/custom-man.xsl $<

XSLTPROC_PROCESS_HTML = \
	$(AM_V_XSLT)$(XSLTPROC) -o $@ $(XSLTPROC_FLAGS) $(srcdir)/man/custom-html.xsl $<

man/%.1: man/%.xml man/custom-man.xsl man/custom-entities.ent
	$(XSLTPROC_PROCESS_MAN)

man/%.3: man/%.xml man/custom-man.xsl man/custom-entities.ent
	$(XSLTPROC_PROCESS_MAN)

man/%.5: man/%.xml man/custom-man.xsl man/custom-entities.ent
	$(XSLTPROC_PROCESS_MAN)

man/%.7: man/%.xml man/custom-man.xsl man/custom-entities.ent
	$(XSLTPROC_PROCESS_MAN)

man/%.8: man/%.xml man/custom-man.xsl man/custom-entities.ent
	$(XSLTPROC_PROCESS_MAN)

man/%.html: man/%.xml man/custom-html.xsl man/custom-entities.ent
	$(XSLTPROC_PROCESS_HTML)

define html-alias
	$(AM_V_LN)$(LN_S) -f $(notdir $<) $@
endef

endif

EXTRA_DIST += \
	man/custom-html.xsl \
	man/custom-man.xsl

install-exec-hook: $(INSTALL_EXEC_HOOKS)

uninstall-hook: $(UNINSTALL_DATA_HOOKS) $(UNINSTALL_EXEC_HOOKS)

install-data-hook: $(INSTALL_DATA_HOOKS)

distclean-local: $(DISTCLEAN_LOCAL_HOOKS)

clean-local: $(CLEAN_LOCAL_HOOKS)
	rm -rf $(abs_srcdir)/install-tree

DISTCHECK_CONFIGURE_FLAGS = \
	--with-dbuspolicydir=$$dc_install_base/$(dbuspolicydir) \
	--with-dbussessionservicedir=$$dc_install_base/$(dbussessionservicedir) \
	--with-dbussystemservicedir=$$dc_install_base/$(dbussystemservicedir) \
	--with-bashcompletiondir=$$dc_install_base/$(bashcompletiondir) \
	--with-zshcompletiondir=$$dc_install_base/$(zshcompletiondir) \
	--with-pamlibdir=$$dc_install_base/$(pamlibdir) \
	--with-pamconfdir=$$dc_install_base/$(pamconfdir) \
	--enable-kdbus \
	--with-udevrulesdir=$$dc_install_base/$(udevrulesdir)

if ENABLE_GTK_DOC
DISTCHECK_CONFIGURE_FLAGS += \
	--enable-gtk-doc
endif
