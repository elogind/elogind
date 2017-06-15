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

LIBELOGIND_CURRENT=14
LIBELOGIND_REVISION=6
LIBELOGIND_AGE=14

# Dirs of external packages
dbuspolicydir=@dbuspolicydir@
dbussystemservicedir=@dbussystemservicedir@
pamlibdir=@pamlibdir@
pamconfdir=@pamconfdir@
pkgconfigdatadir=$(datadir)/pkgconfig
pkgconfiglibdir=$(libdir)/pkgconfig
polkitpolicydir=$(datadir)/polkit-1/actions
bashcompletiondir=@bashcompletiondir@
zshcompletiondir=@zshcompletiondir@

CGROUP_CONTROLLER=@cgroup_controller@
PKTTYAGENT=$(bindir)/pkttyagent

# Our own, non-special dirs
pkgsysconfdir=$(sysconfdir)/elogind
pkgincludedir=$(includedir)/elogind/systemd
udevrulesdir=@udevrulesdir@
udevbindir=@udevbindir@
udevlibexecdir=$(udevbindir)
udevhomedir=$(udevlibexecdir)
systemshutdowndir=$(rootlibexecdir)/system-shutdown
systemsleepdir=$(rootlibexecdir)/system-sleep
factory_pamdir = $(datadir)/factory/etc/pam.d

# And these are the special ones for /
rootprefix=@rootprefix@
rootbindir=$(rootprefix)/bin
rootlibexecdir=$(rootprefix)/lib/elogind

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
noinst_DATA =
pkgconfiglib_DATA =
polkitpolicy_in_files =
polkitpolicy_files =
dist_udevrules_DATA =
nodist_udevrules_DATA =
dist_pkgsysconf_DATA =
dist_dbuspolicy_DATA =
dist_dbussystemservice_DATA =
check_PROGRAMS =
check_DATA =
tests=
manual_tests =
if ENABLE_TESTS
noinst_PROGRAMS = $(manual_tests) $(tests)
TESTS = $(tests)
else
noinst_PROGRAMS =
TESTS =
endif

if ENABLE_BASH_COMPLETION
dist_bashcompletion_DATA = $(dist_bashcompletion_data)
endif
if ENABLE_ZSH_COMPLETION
dist_zshcompletion_DATA = $(dist_zshcompletion_data)
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
	-DSYSTEMD_CGROUP_CONTROLLER=\"$(CGROUP_CONTROLLER)\" \
	-DSYSTEMD_CGROUP_AGENT_PATH=\"$(rootlibexecdir)/elogind-cgroups-agent\" \
	-DUDEVLIBEXECDIR=\"$(udevlibexecdir)\" \
	-DPOLKIT_AGENT_BINARY_PATH=\"$(PKTTYAGENT)\" \
	-DSYSTEM_SLEEP_PATH=\"$(systemsleepdir)\" \
	-DSYSTEM_SHUTDOWN_PATH=\"$(systemshutdowndir)\" \
	-DHALT=\"$(HALT)\" \
	-DREBOOT=\"$(REBOOT)\" \
	-DKEXEC=\"$(KEXEC)\" \
	-DLIBDIR=\"$(libdir)\" \
	-DROOTLIBDIR=\"$(rootlibdir)\" \
	-DROOTLIBEXECDIR=\"$(rootlibexecdir)\" \
	-DTEST_DIR=\"$(abs_top_srcdir)/test\" \
	-I $(top_srcdir)/src \
	-I $(top_builddir)/src/basic \
	-I $(top_srcdir)/src/basic \
	-I $(top_srcdir)/src/core \
	-I $(top_srcdir)/src/shared \
	-I $(top_builddir)/src/shared \
	-I $(top_srcdir)/src/login \
	-I $(top_srcdir)/src/systemd \
	-I $(top_srcdir)/src/libelogind/sd-bus \
	-I $(top_srcdir)/src/libelogind/sd-event \
	-I $(top_srcdir)/src/libelogind/sd-login \
	-I $(top_srcdir)/src/update-utmp \
	$(OUR_CPPFLAGS)

AM_CFLAGS = $(OUR_CFLAGS)
AM_LDFLAGS = $(OUR_LDFLAGS)

# ------------------------------------------------------------------------------
define move-to-rootlibdir
	if test "$(libdir)" != "$(rootlibdir)"; then \
		$(MKDIR_P) $(DESTDIR)$(rootlibdir) && \
		so_img_name=$$(readlink $(DESTDIR)$(libdir)/$$libname) && \
		rm -f $(DESTDIR)$(libdir)/$$libname && \
		$(LN_S) --relative -f $(DESTDIR)$(rootlibdir)/$$so_img_name $(DESTDIR)$(libdir)/$$libname && \
		mv $(DESTDIR)$(libdir)/$$libname.* $(DESTDIR)$(rootlibdir); \
	fi
endef

install-touch-usr-hook:
	touch -c $(DESTDIR)/$(prefix)

INSTALL_EXEC_HOOKS += \
	install-touch-usr-hook


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
rootbin_PROGRAMS =
pkglibexec_PROGRAMS =

dist_doc_DATA = \
	README \
	NEWS \
	LICENSE.LGPL2.1 \
	LICENSE.GPL2

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
	$(HTML_ALIAS) \
	docs/html/man
endif

CLEANFILES += \
	$(man_MANS) \
	$(HTML_FILES) \
	$(HTML_ALIAS) \
	docs/html/man

docs/html/man:
	$(AM_V_at)$(MKDIR_P) $(dir $@)
	$(AM_V_LN)$(LN_S) -f ../../man $@

man/index.html: man/elogind.index.html
	$(AM_V_LN)$(LN_S) -f elogind.index.html $@

if HAVE_PYTHON
noinst_DATA += \
       man/index.html
endif

CLEANFILES += \
       man/index.html

XML_GLOB            := $(wildcard $(top_srcdir)/man/*.xml)
DIRECTIVE_FILE      := man/elogind.directives.xml
INDEX_XML_FILE      := man/elogind.index.xml
NON_INDEX_XML_FILES := $(DIRECTIVE_FILE) $(filter-out $(INDEX_XML_FILE) $(DIRECTIVE_FILE),$(XML_FILES))
ALL_XML_FILES       := $(INDEX_XML_FILE) $(DIRECTIVE_FILE) $(filter-out $(INDEX_XML_FILE) $(DIRECTIVE_FILE),$(XML_GLOB))
SOURCE_XML_FILES    := ${patsubst %,$(top_srcdir)/%,$(filter-out $(DIRECTIVE_FILE),$(NON_INDEX_XML_FILES))}

# This target should only be run manually. It recreates Makefile-man.am
# file in the source directory based on all man/*.xml files. Run it after
# adding, removing, or changing the conditional in a man page.
update-man-list: $(top_srcdir)/tools/make-man-rules.py $(ALL_XML_FILES) man/custom-entities.ent
	$(AM_V_GEN)$(PYTHON) $< $(ALL_XML_FILES) > $(top_srcdir)/Makefile-man.tmp
	$(AM_V_at)mv $(top_srcdir)/Makefile-man.tmp $(top_srcdir)/Makefile-man.am
	@echo "Makefile-man.am has been regenerated"

$(INDEX_XML_FILE): $(top_srcdir)/tools/make-man-index.py $(NON_INDEX_XML_FILES)
	$(AM_V_at)$(MKDIR_P) $(dir $@)
	$(AM_V_GEN)$(PYTHON) $< $@ $(filter-out $<,$^)

$(DIRECTIVE_FILE): $(top_srcdir)/tools/make-directive-index.py man/custom-entities.ent $(SOURCE_XML_FILES)
	$(AM_V_at)$(MKDIR_P) $(dir $@)
	$(AM_V_GEN)$(PYTHON) $< $@ $(SOURCE_XML_FILES)

CLEANFILES += \
       $(INDEX_XML_FILE) \
       $(DIRECTIVE_FILE)

EXTRA_DIST += \
	$(filter-out $(DIRECTIVE_FILE) $(INDEX_XML_FILE),$(XML_FILES)) \
	tools/make-man-index.py \
	tools/make-man-rules.py \
	tools/make-directive-index.py \
	tools/xml_helper.py \
	man/glib-event-glue.c

# ------------------------------------------------------------------------------
noinst_LTLIBRARIES += \
	libbasic.la

libbasic_la_SOURCES = \
	src/basic/missing.h \
	src/basic/capability-util.c \
	src/basic/capability-util.h \
	src/basic/conf-files.c \
	src/basic/conf-files.h \
	src/basic/stdio-util.h \
	src/basic/hostname-util.h \
	src/basic/hostname-util.c \
	src/basic/unit-name.c \
	src/basic/unit-name.h \
	src/basic/unaligned.h \
	src/basic/util.c \
	src/basic/util.h \
	src/basic/io-util.c \
	src/basic/io-util.h \
	src/basic/string-util.c \
	src/basic/string-util.h \
	src/basic/parse-util.c \
	src/basic/parse-util.h \
	src/basic/fd-util.c \
	src/basic/fd-util.h \
	src/basic/user-util.c \
	src/basic/user-util.h \
	src/basic/dirent-util.c \
	src/basic/dirent-util.h \
	src/basic/xattr-util.c \
	src/basic/xattr-util.h \
	src/basic/proc-cmdline.c \
	src/basic/proc-cmdline.h \
	src/basic/fs-util.c \
	src/basic/fs-util.h \
	src/basic/syslog-util.c \
	src/basic/syslog-util.h \
	src/basic/stat-util.c \
	src/basic/stat-util.h \
	src/basic/mount-util.c \
	src/basic/mount-util.h \
	src/basic/hexdecoct.c \
	src/basic/hexdecoct.h \
	src/basic/extract-word.c \
	src/basic/extract-word.h \
	src/basic/escape.c \
	src/basic/escape.h \
	src/basic/path-util.c \
	src/basic/path-util.h \
	src/basic/parse-printf-format.c \
	src/basic/parse-printf-format.h \
	src/basic/time-util.c \
	src/basic/time-util.h \
	src/basic/locale-util.c \
	src/basic/locale-util.h \
	src/basic/umask-util.h \
	src/basic/signal-util.c \
	src/basic/signal-util.h \
	src/basic/string-table.c \
	src/basic/string-table.h \
	src/basic/mempool.c \
	src/basic/mempool.h \
	src/basic/hashmap.c \
	src/basic/hashmap.h \
	src/basic/hash-funcs.c \
	src/basic/hash-funcs.h \
	src/basic/siphash24.c \
	src/basic/siphash24.h \
	src/basic/prioq.c \
	src/basic/prioq.h \
	src/basic/strv.c \
	src/basic/strv.h \
	src/basic/log.c \
	src/basic/log.h \
	src/basic/bus-label.c \
	src/basic/bus-label.h \
	src/basic/virt.c \
	src/basic/virt.h \
	src/basic/smack-util.c \
	src/basic/smack-util.h \
	src/basic/utf8.c \
	src/basic/utf8.h \
	src/basic/gunicode.c \
	src/basic/gunicode.h \
	src/basic/socket-util.c \
	src/basic/socket-util.h \
	src/basic/fileio.c \
	src/basic/fileio.h \
	src/basic/mkdir.c \
	src/basic/mkdir.h \
	src/basic/cgroup-util.c \
	src/basic/cgroup-util.h \
	src/basic/errno-list.c \
	src/basic/errno-list.h \
	src/basic/terminal-util.c \
	src/basic/terminal-util.h \
	src/basic/login-util.h \
	src/basic/login-util.c \
	src/basic/audit-util.c \
	src/basic/audit-util.h \
	src/basic/memfd-util.c \
	src/basic/memfd-util.h \
	src/basic/process-util.c \
	src/basic/process-util.h \
	src/basic/random-util.c \
	src/basic/random-util.h \
	src/basic/verbs.c \
	src/basic/verbs.h \
	src/basic/label.c \
	src/basic/label.h \
	src/basic/selinux-util.c \
	src/basic/selinux-util.h \
	src/basic/mkdir-label.c \
	src/basic/fileio-label.c \
	src/basic/fileio-label.h \
	src/basic/rm-rf.c \
	src/basic/rm-rf.h \
	src/basic/copy.c \
	src/basic/copy.h \
	src/basic/alloc-util.h \
	src/basic/alloc-util.c

nodist_libbasic_la_SOURCES = \
	src/basic/errno-from-name.h \
	src/basic/errno-to-name.h

libbasic_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(SELINUX_CFLAGS) \
	$(CAP_CFLAGS) \
	-pthread

libbasic_la_LDFLAGS = \
	$(CAP_LDFLAGS)

libbasic_la_LIBADD = \
	$(SELINUX_LIBS) \
	$(CAP_LIBS) \
	-ldl \
	-lm

# -----------------------------------------------------------------------------
noinst_LTLIBRARIES += \
	libshared.la

libshared_la_SOURCES = \
	src/shared/bus-util.c \
	src/shared/bus-util.h \
	src/shared/clean-ipc.c \
	src/shared/clean-ipc.h \
	src/shared/conf-parser.c \
	src/shared/conf-parser.h \
	src/shared/musl_missing.h \
	src/shared/musl_missing.c \
	src/shared/pager.c \
	src/shared/pager.h \
	src/shared/spawn-polkit-agent.c \
	src/shared/spawn-polkit-agent.h

if HAVE_UTMP
libshared_la_SOURCES += \
	src/shared/utmp-wtmp.c \
	src/update-utmp/update-utmp.c
endif

if HAVE_ACL
libshared_la_SOURCES += \
	src/shared/acl-util.c \
	src/shared/acl-util.h
endif

libshared_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(ACL_CFLAGS)

libshared_la_LIBADD = \
	libelogind-internal.la \
	$(UDEV_LIBS) \
	$(ACL_LIBS)

gperf_txt_sources = \
	src/basic/errno-list.txt

BUILT_SOURCES += \
	$(gperf_gperf_m4_sources:-gperf.gperf.m4=-gperf.c) \
	$(gperf_gperf_sources:-gperf.gperf=-gperf.c) \
	$(gperf_txt_sources:-list.txt=-from-name.h) \
	$(gperf_txt_sources:-list.txt=-to-name.h)

CLEANFILES += \
	$(gperf_txt_sources:-list.txt=-from-name.gperf)
DISTCLEANFILES = \
	$(gperf_txt_sources)

EXTRA_DIST += \
	$(gperf_gperf_m4_sources) \
	$(gperf_gperf_sources)

CLEANFILES += \
	$(gperf_txt_sources)

%-from-name.gperf: %-list.txt
	$(AM_V_at)$(MKDIR_P) $(dir $@)
	$(AM_V_GEN)$(AWK) 'BEGIN{ print "struct $(notdir $*)_name { const char* name; int id; };"; print "%null-strings"; print "%%";} { printf "%s, %s\n", $$1, $$1 }' <$< >$@

%-from-name.h: %-from-name.gperf
	$(AM_V_at)$(MKDIR_P) $(dir $@)
	$(AM_V_GPERF)$(GPERF) -L ANSI-C -t --ignore-case -N lookup_$(notdir $*) -H hash_$(notdir $*)_name -p -C <$< >$@


src/basic/errno-list.txt:
	$(AM_V_at)$(MKDIR_P) $(dir $@)
	$(AM_V_GEN)$(CPP) $(CFLAGS) $(AM_CPPFLAGS) $(CPPFLAGS) -dM -include errno.h - </dev/null | $(AWK) '/^#define[ \t]+E[^ _]+[ \t]+/ { print $$2; }' >$@

src/basic/errno-to-name.h: src/basic/errno-list.txt
	$(AM_V_at)$(MKDIR_P) $(dir $@)
	$(AM_V_GEN)$(AWK) 'BEGIN{ print "static const char* const errno_names[] = { "} !/EDEADLOCK/ && !/EWOULDBLOCK/ && !/ENOTSUP/ { printf "[%s] = \"%s\",\n", $$1, $$1 } END{print "};"}' <$< >$@


# ------------------------------------------------------------------------------

if HAVE_PAM
dist_factory_pam_DATA = \
	factory/etc/pam.d/system-auth \
	factory/etc/pam.d/other
endif

# ------------------------------------------------------------------------------
pkglibexec_PROGRAMS += \
       elogind-cgroups-agent

elogind_cgroups_agent_SOURCES = \
       src/cgroups-agent/cgroups-agent.c

elogind_cgroups_agent_LDADD = \
       libshared.la

# ------------------------------------------------------------------------------
libelogind_internal_la_SOURCES = \
	src/systemd/sd-bus.h \
	src/systemd/sd-bus-protocol.h \
	src/systemd/sd-bus-vtable.h \
	src/systemd/sd-event.h \
	src/systemd/sd-login.h \
	src/systemd/sd-id128.h \
	src/systemd/sd-daemon.h \
	src/libelogind/libelogind.sym \
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
	src/libelogind/sd-bus/bus-slot.c \
	src/libelogind/sd-bus/bus-slot.h \
	src/libelogind/sd-bus/bus-protocol.h \
	src/libelogind/sd-event/sd-event.c \
	src/libelogind/sd-id128/sd-id128.c \
	src/libelogind/sd-daemon/sd-daemon.c \
	src/libelogind/sd-login/sd-login.c

libelogind_internal_la_LIBADD = \
	libbasic.la \
	-lresolv

noinst_LTLIBRARIES += \
	libelogind-internal.la

EXTRA_DIST += \
	src/libelogind/libelogind.pc.in \
	src/libelogind/sd-bus/DIFFERENCES \
	src/libelogind/sd-bus/GVARIANT-SERIALIZATION

libelogind_la_SOURCES = \
	$(libelogind_internal_la_SOURCES)

nodist_libelogind_la_SOURCES = \
	$(nodist_libelogind_internal_la_SOURCES)

libelogind_la_CFLAGS = \
	$(libelogind_internal_la_CFLAGS)

libelogind_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-version-info $(LIBELOGIND_CURRENT):$(LIBELOGIND_REVISION):$(LIBELOGIND_AGE) \
	-Wl,--version-script=$(top_srcdir)/src/libelogind/libelogind.sym

libelogind_la_LIBADD = \
	$(libelogind_internal_la_LIBADD)

libelogind-install-hook:
	libname=libelogind.so && $(move-to-rootlibdir)

libelogind-uninstall-hook:
	rm -f $(DESTDIR)$(rootlibdir)/libelogind.so*

INSTALL_EXEC_HOOKS += libelogind-install-hook
UNINSTALL_EXEC_HOOKS += libelogind-uninstall-hook

pkgconfiglib_DATA += \
	src/libelogind/libelogind.pc

pkginclude_HEADERS += \
	src/systemd/sd-login.h \
	src/systemd/sd-messages.h \
	src/systemd/sd-id128.h \
	src/systemd/_sd-common.h

header-install-hook:
	$(MKDIR_P) $(DESTDIR)/$(pkgincludedir)
	cd $(DESTDIR)/$(includedir)/elogind && \
		for hdr in $(notdir $(pkginclude_HEADERS)) ; do \
		        rm -f $$hdr ; \
			$(LN_S) systemd/$$hdr $$hdr ; \
		done

header-uninstall-hook:
	m -f $(DESTDIR)/$(includedir)/elogind/*.h

INSTALL_EXEC_HOOKS += header-install-hook
UNINSTALL_EXEC_HOOKS += header-uninstall-hook

lib_LTLIBRARIES += \
	libelogind.la

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
	src/login/logind-sleep.c \
	src/login/logind-sleep.h \
	src/login/logind-user.c \
	src/login/logind-user.h \
	src/login/logind-inhibit.c \
	src/login/logind-inhibit.h \
	src/login/logind-dbus.c \
	src/login/logind-session-dbus.c \
	src/login/logind-seat-dbus.c \
	src/login/logind-user-dbus.c \
	src/login/logind-utmp.c \
	src/login/logind-acl.h \
	src/core/cgroup.h \
	src/core/cgroup.c \
	src/core/mount-setup.h \
	src/core/mount-setup.c

libelogind_core_la_LIBADD = \
	libshared.la

if HAVE_ACL
libelogind_core_la_SOURCES += \
	src/login/logind-acl.c
endif

noinst_LTLIBRARIES += \
	libelogind-core.la

pkglibexec_PROGRAMS += \
	elogind

loginctl_SOURCES = \
	src/login/loginctl.c \
	src/login/sysfs-show.h \
	src/login/sysfs-show.c

loginctl_LDADD = \
	libshared.la

rootbin_PROGRAMS += \
	loginctl

dist_bashcompletion_data = \
	shell-completion/bash/loginctl

dist_zshcompletion_data = \
	shell-completion/zsh/_loginctl \
	shell-completion/zsh/_elogind-inhibit

elogind_inhibit_SOURCES = \
	src/login/inhibit.c

elogind_inhibit_LDADD = \
	libshared.la

rootbin_PROGRAMS += \
	elogind-inhibit

test_login_SOURCES = \
	src/libelogind/sd-login/test-login.c

test_login_LDADD = \
	libshared.la

test_login_shared_SOURCES = \
	src/login/test-login-shared.c

test_login_shared_LDADD = \
	libshared.la

test_inhibit_SOURCES = \
	src/login/test-inhibit.c

test_inhibit_LDADD = \
	libshared.la

test_login_tables_SOURCES = \
	src/login/test-login-tables.c

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
	$(PAM_CFLAGS)

pam_elogind_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-module \
	-export-dynamic \
	-avoid-version \
	-shared \
	-Wl,--version-script=$(top_srcdir)/src/login/pam_elogind.sym

pam_elogind_la_LIBADD = \
	libshared.la \
	$(PAM_LIBS)

pamlib_LTLIBRARIES = \
	pam_elogind.la

dist_pamconf_DATA = \
	src/login/elogind-user

EXTRA_DIST += \
	src/login/elogind-user.m4
endif

dist_dbussystemservice_DATA += \
	src/login/org.freedesktop.login1.service

dist_dbuspolicy_DATA += \
	src/login/org.freedesktop.login1.conf

dist_pkgsysconf_DATA += \
	src/login/logind.conf

polkitpolicy_files += \
	src/login/org.freedesktop.login1.policy

dist_udevrules_DATA += \
	src/login/70-uaccess.rules \
	src/login/70-power-switch.rules

nodist_udevrules_DATA += \
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
       '|rootlibexecdir=$(rootlibexecdir)|' \
       '|rootbindir=$(rootbindir)|' \
       '|bindir=$(bindir)|' \
       '|pkgsysconfdir=$(pkgsysconfdir)|' \
       '|pkgdatadir=$(pkgdatadir)|' \
       '|udevrulesdir=$(udevrulesdir)|' \
       '|PACKAGE_VERSION=$(PACKAGE_VERSION)|' \
       '|PACKAGE_NAME=$(PACKAGE_NAME)|' \
       '|PACKAGE_URL=$(PACKAGE_URL)|' \
       '|prefix=$(prefix)|' \
       '|exec_prefix=$(exec_prefix)|' \
       '|libdir=$(libdir)|' \
       '|includedir=$(includedir)|' \
       '|VERSION=$(VERSION)|' \
       '|rootprefix=$(rootprefix)|' \
       '|udevlibexecdir=$(udevlibexecdir)|' \
       '|KILL=$(KILL)|' \
       '|MKDIR_P=$(MKDIR_P)|' \
       '|systemuidmax=$(SYSTEM_UID_MAX)|' \
       '|systemgidmax=$(SYSTEM_GID_MAX)|' \
       '|TTY_GID=$(TTY_GID)|'

SED_PROCESS = \
	$(AM_V_GEN)$(MKDIR_P) $(dir $@) && \
	$(SED) $(subst '|,-e 's|@,$(subst =,\@|,$(subst |',|g',$(substitutions)))) \
		< $< > $@

man/%: man/%.in
	$(SED_PROCESS)

sysctl.d/%: sysctl.d/%.in
	$(SED_PROCESS)

%.pc: %.pc.in
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
	$(polkitpolicy_files)
endif

EXTRA_DIST += \
	$(polkitpolicy_in_files)

# ------------------------------------------------------------------------------
if ENABLE_MANPAGES
man/custom-entities.ent: configure.ac
	$(AM_V_GEN)$(MKDIR_P) $(dir $@)
	$(AM_V_GEN)(echo '<?xml version="1.0" encoding="utf-8" ?>' && \
	 printf '$(subst '|,<!ENTITY ,$(subst =, ",$(subst |',">\n,$(substitutions))))') \
	 > $@ # '

CLEANFILES += \
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
	rm -f $(abs_srcdir)/hwdb/usb.ids $(abs_srcdir)/hwdb/pci.ids $(abs_srcdir)/hwdb/oui.txt \
	      $(abs_srcdir)/hwdb/iab.txt

DISTCHECK_CONFIGURE_FLAGS = \
	--with-dbuspolicydir=$$dc_install_base/$(dbuspolicydir) \
	--with-dbussystemservicedir=$$dc_install_base/$(dbussystemservicedir) \
	--with-bashcompletiondir=$$dc_install_base/$(bashcompletiondir) \
	--with-zshcompletiondir=$$dc_install_base/$(zshcompletiondir) \
	--with-pamlibdir=$$dc_install_base/$(pamlibdir) \
	--with-pamconfdir=$$dc_install_base/$(pamconfdir) \
	--with-rootprefix=$$dc_install_base

if ENABLE_SPLIT_USR
DISTCHECK_CONFIGURE_FLAGS += \
	--enable-split-usr
else
DISTCHECK_CONFIGURE_FLAGS += \
	--disable-split-usr
endif

.PHONY: dist-check-help

dist-check-help: $(rootbin_PROGRAMS)
	for i in $(abspath $^); do                                             \
            if $$i  --help | grep -v 'default:' | grep -E -q '.{80}.' ; then   \
		echo "$(basename $$i) --help output is too wide:";             \
	        $$i  --help | awk 'length > 80' | grep -E --color=yes '.{80}'; \
	        exit 1;                                                        \
            fi; done

dist:

.PHONY: git-tag
git-tag:
	git tag -s "v$(VERSION)" -m "elogind $(VERSION)"

.PHONY: git-tar
git-tar:
	$(AM_V_at)git archive --format=tar --prefix=elogind-$(VERSION)-raw/ HEAD | \
		(cd /var/tmp/ && tar xf -)
	$(AM_V_GEN)(cd /var/tmp && rm -rf elogind-$(VERSION) && $(MKDIR_P) elogind-$(VERSION) && \
	 for f in `find elogind-$(VERSION)-raw/ -type f | cut -d '/' -f 2-` ; do \
		$(MKDIR_P) elogind-$(VERSION)/`dirname $$f` ;            \
		$(AWK) 'BEGIN                    { i=0;e=0      }        \
		        /^#if\s+0.*elogind.*$$/  { i=1;next     }        \
		        /^#else\s*$$/            {                       \
		            { if ( i==1 ) e=1; else print }{next}        \
			}                                                \
		        /^#endif\s*\/\/\s*0\s*$$/ {                      \
		            { if ( i==1 ) { i=0;e=0 } else print }{next} \
			}                                                \
		        /^\s*\/\/\/.*elogind.*$$/{ next         }        \
		        /^\s*\/\/\s*#include.+$$/{ next         }        \
		        { if ( (i==0) || (e==1) ) print         }'       \
		          elogind-$(VERSION)-raw/$$f                     \
		        > elogind-$(VERSION)/$$f ;                       \
		        chmod `stat -c '%a' elogind-$(VERSION)-raw/$$f`  \
		                            elogind-$(VERSION)/$$f ;     \
	 done ; \
	 for l in `find elogind-$(VERSION)-raw/ -type l | cut -d '/' -f 2-` ; do \
		$(MKDIR_P) elogind-$(VERSION)/`dirname $$l` ; \
		cp -P elogind-$(VERSION)-raw/$$l elogind-$(VERSION)/$$l ; \
	 done ; \
	 tar czf elogind-$(VERSION).tar.gz elogind-$(VERSION) ; \
	 rm -rf elogind-$(VERSION) elogind-$(VERSION)-raw) ; \
	$(AM_V_at)mv /var/tmp/elogind-$(VERSION).tar.gz ./

.PHONY: install-tree
install-tree: all
	rm -rf $(abs_srcdir)/install-tree
	$(MAKE) install DESTDIR=$(abs_srcdir)/install-tree
	tree $(abs_srcdir)/install-tree

# Let's run all tests of the test suite, but under valgrind. Let's
# exclude the one perl script we have in there
.PHONY: valgrind-tests
valgrind-tests: $(TESTS)
	$(AM_V_GEN)for f in $(filter-out %.pl, $^); do \
		if file $$f | grep -q shell; then \
		echo -e "$${x}Skipping non-binary $$f"; else \
		echo -e "$${x}Running $$f"; \
		libtool --mode=execute valgrind -q --leak-check=full --max-stackframe=5242880 --error-exitcode=55 $(builddir)/$$f ; fi; \
		x="\n\n"; \
	done

exported-%: %
	$(AM_V_GEN)$(NM) -g --defined-only $(builddir)/.libs/$(<:.la=.so) 2>&1 /dev/null | grep " T " | cut -d" " -f3 > $@

exported: $(addprefix exported-, $(lib_LTLIBRARIES))
	$(AM_V_GEN)cat $^ > $@

.PHONY: check-api-docs
check-api-docs: exported man
	$(AM_V_GEN)for symbol in `cat exported` ; do \
		if test -f $(builddir)/man/$$symbol.html ; then \
			echo "  Symbol $$symbol() is documented." ; \
		else \
			echo "‣ Symbol $$symbol() lacks documentation." ; \
		fi ; \
	done

OBJECT_VARIABLES:=$(filter %_OBJECTS,$(.VARIABLES))
ALL_OBJECTS:=$(foreach v,$(OBJECT_VARIABLES),$($(v)))

undefined defined: $(ALL_OBJECTS)
	$(AM_V_GEN)for f in $(ALL_OBJECTS) ; do \
		$(NM) -g --$@-only `echo $(builddir)/"$$f" | sed -e 's,\([^/]*\).lo$$,.libs/\1.o,'` ; \
	done | cut -c 20- | cut -d @ -f 1 | sort -u > $@

CLEANFILES += \
	defined \
	undefined

.PHONY: check-api-unused
check-api-unused: defined undefined exported
	( cat exported undefined ) | sort -u  | diff -u - defined | grep ^+ | grep -v ^+++ | cut -c2-

.PHONY: check-includes
check-includes: $(top_srcdir)/tools/check-includes.pl
	$(AM_V_GEN) find * -name '*.[hcS]' -type f -print | sort -u \
		| xargs $(top_srcdir)/tools/check-includes.pl

EXTRA_DIST += \
	$(top_srcdir)/tools/check-includes.pl

# Stupid test that everything purported to be exported really is
define generate-sym-test
	$(AM_V_at)$(MKDIR_P) $(dir $@)
	$(AM_V_at)printf '#include <stdio.h>\n' > $@
	$(AM_V_at)printf '#include "%s"\n' $(notdir $(filter %.h, $^)) >> $@
	$(AM_V_at)printf 'void* functions[] = {\n' >> $@
	$(AM_V_GEN)sed -r -n 's/^ +([a-zA-Z0-9_]+);/\1,/p' $< >> $@
	$(AM_V_at)printf '};\nint main(void) {\n' >> $@
	$(AM_V_at)printf 'unsigned i; for (i=0;i<sizeof(functions)/sizeof(void*);i++) printf("%%p\\n", functions[i]);\n' >> $@
	$(AM_V_at)printf 'return 0; }\n' >> $@
endef

test-libelogind-sym.c: \
		$(top_builddir)/src/libelogind/libelogind.sym \
		src/systemd/sd-daemon.h \
		src/systemd/sd-login.h \
		src/systemd/sd-bus.h \
		src/systemd/sd-event.h
	$(generate-sym-test)

test_libelogind_sym_SOURCES = \
	test-libelogind-sym.c
test_libelogind_sym_LDADD = \
	libelogind.la

BUILT_SOURCES += \
	$(test_libelogind_sym_SOURCES)

tests += \
	test-libelogind-sym

.PHONY: cppcheck
cppcheck:
	cppcheck --enable=all -q $(top_srcdir)

# Used to extract compile flags for YCM.
print-%:
	@echo $($*)

git-contrib:
	@git shortlog -s `git describe --abbrev=0`.. | cut -c8- | awk '{ print $$0 "," }' | sort -u

EXTRA_DIST += \
        tools/gdb-sd_dump_hashmaps.py

list-keys:
	gpg --verbose --no-options --no-default-keyring --no-auto-key-locate --batch --trust-model=always --keyring=$(srcdir)/src/import/import-pubring.gpg --list-keys

add-key:
	gpg --verbose --no-options --no-default-keyring --no-auto-key-locate --batch --trust-model=always --keyring=$(srcdir)/src/import/import-pubring.gpg --import -
