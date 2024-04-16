#if 0 /// The original Makefile follows, which isn't enough for elogind.
# # SPDX-License-Identifier: LGPL-2.1-or-later
#
# all:
# 	ninja -C build
#
# install:
# 	DESTDIR=$(DESTDIR) ninja -C build install
#else // 0
.PHONY: all build clean full install justprint loginctl test test-login
export

# Set this to YES on the command line for a debug build
DEBUG      ?= NO

# Set this to yes to not build, but to show all build commands ninja would issue
JUST_PRINT ?= NO

HERE := $(shell pwd -P)

BASIC_OPT  := --buildtype release
BUILDDIR   ?= $(HERE)/build
BUILDMODE  ?= auto
CGCONTROL  ?= $(shell $(HERE)/tools/meson-get-cg-controller.sh)
CGDEFAULT  ?= $(shell grep "^rc_cgroup_mode" /etc/rc.conf | cut -d '"' -f 2)
DESTDIR    ?=
MESON_LST  := $(shell find $(HERE)/ -type f -name 'meson.build') $(HERE)/meson_options.txt
ROOTPREFIX ?= /tmp/elogind_test
PREFIX     ?= $(ROOTPREFIX)/usr
SYSCONFDIR ?= $(ROOTPREFIX)/etc
VERSION    ?= 9999

# Detailed config with sane defaults
USE_ACL     ?= enabled
USE_AUDIT   ?= enabled
USE_EFI     ?= false
USE_HTML    ?= auto
USE_MAN     ?= auto
USE_SELINUX ?= disabled
USE_SMACK   ?= true
USE_UTMP    ?= true
USE_XENCTRL ?= auto

# Tools needed by the wrapper
CC    ?= $(shell which cc)
LD    ?= $(shell which ld)
LN    := $(shell which ln) -s
MAKE  := $(shell which make)
MESON ?= $(shell which meson)
MKDIR := $(shell which mkdir) -p
NINJA ?= $(shell which ninja)
RM    := $(shell which rm) -f

# Save users/systems choice
envCFLAGS   := ${CFLAGS}
envLDFLAGS  := ${LDFLAGS}

BASIC_OPT := --buildtype release
NINJA_OPT := --verbose

# Make sure "--just-print" gets translated over to ninja
ifneq (,$(findstring n,$(MAKEFLAGS)))
    FILTER_ME = n
    override MAKEFLAGS    := $(filter-out $(FILTER_ME),$(MAKEFLAGS))
    override MAKEOVERRIDE := $(MAKEFLAGS)
    # Explicitly set JUST_PRINT to "YES"
    JUST_PRINT := YES
endif

# Simulate --just-print?
ifeq (YES,$(JUST_PRINT))
    NINJA_OPT := ${NINJA_OPT} -t commands
endif

# Combine with "sane defaults"
ifeq (YES,$(DEBUG))
    BASIC_OPT := --werror -Dlog-trace=true -Dslow-tests=true -Ddebug-extra=elogind --buildtype debug
    BUILDDIR  := ${BUILDDIR}_debug
    CFLAGS    := -O0 -g3 -ggdb -ftrapv ${envCFLAGS} -fPIE
    LDFLAGS   := -fPIE
    ifeq (NO,$(JUST_PRINT))
        NINJA_OPT := ${NINJA_OPT} -j 1 -k 1
    endif
    ifneq (release,$(BUILDMODE))
        BUILDMODE := developer
    endif
else
    BUILDDIR  := ${BUILDDIR}_release
    CFLAGS    := -fwrapv ${envCFLAGS}
    LDFLAGS   :=
    ifeq (YES,$(JUST_PRINT))
        NINJA_OPT := ${NINJA_OPT} -s
    endif
    ifneq (developer,$(BUILDMODE))
        BUILDMODE := release
    endif
endif

# Set search paths including the actual build directory
VPATH  := $(BUILDDIR):$(HERE):$(HERE)/src

# Set the build configuration we use to check whether a reconfiguration is needed
CONFIG := $(BUILDDIR)/compile_commands.json

# Finalize CFLAGS
CFLAGS := -march=native -pipe ${CFLAGS} -Wunused -ftree-vectorize

# Finalize LDFLAGS
LDFLAGS := ${envLDFLAGS} ${LDFLAGS} -lpthread

# Ensure a sane default cgroup controller mode is set.
# if /etc/rc.conf has not set one, "unified" is probably the default.
ifeq (,$(CGDEFAULT))
    CGDEFAULT := unified
endif

all: build

build: $(CONFIG)
	+@(echo "make[2]: Entering directory '$(BUILDDIR)'")
	+(cd $(BUILDDIR) && $(NINJA) $(NINJA_OPT))
	+@(echo "make[2]: Leaving directory '$(BUILDDIR)'")

clean: $(CONFIG)
	+@(echo "make[2]: Entering directory '$(BUILDDIR)'")
	(cd $(BUILDDIR) && $(NINJA) $(NINJA_OPT) -t cleandead)
	(cd $(BUILDDIR) && $(NINJA) $(NINJA_OPT) -t clean)
	+@(echo "make[2]: Leaving directory '$(BUILDDIR)'")

cleanall:
	+(BUILDDIR=$(HERE)/build $(MAKE) clean DEBUG=YES)
	+(BUILDDIR=$(HERE)/build $(MAKE) clean DEBUG=NO )

full: build

install: build
	+@(echo "make[2]: Entering directory '$(BUILDDIR)'")
	(cd $(BUILDDIR) && DESTDIR=$(DESTDIR) $(NINJA) $(NINJA_OPT) install)
	+@(echo "make[2]: Leaving directory '$(BUILDDIR)'")

justprint: $(CONFIG)
	+(BUILDDIR=$(HERE)/build $(MAKE) all JUST_PRINT=YES)

loginctl: $(CONFIG)
	+@(echo "make[2]: Entering directory '$(BUILDDIR)'")
	(cd $(BUILDDIR) && $(NINJA) $(NINJA_OPT) $@)
	+@(echo "make[2]: Leaving directory '$(BUILDDIR)'")

test: $(CONFIG)
	+@(echo "make[2]: Entering directory '$(BUILDDIR)'")
	(cd $(BUILDDIR) && $(NINJA) $(NINJA_OPT) $@)
	+@(echo "make[2]: Leaving directory '$(BUILDDIR)'")

test-login: $(CONFIG)
	+@(echo "make[2]: Entering directory '$(BUILDDIR)'")
	(cd $(BUILDDIR) && $(NINJA) $(NINJA_OPT) $@)
	+@(echo "make[2]: Leaving directory '$(BUILDDIR)'")

$(BUILDDIR):
	+$(MKDIR) $@

$(CONFIG): $(BUILDDIR) $(MESON_LST)
	@echo " Generating $@"
	+test -f $@ && ( \
		$(MESON) configure $(BUILDDIR) $(BASIC_OPT) \
	) || ( \
		$(MESON) setup $(BUILDDIR) $(BASIC_OPT) \
			--prefix="$(PREFIX)" \
			--libdir="$(PREFIX)"/lib64 \
			--libexecdir="$(ROOTPREFIX)"/lib64/elogind \
			--localstatedir="$(ROOTPREFIX)"/var \
			--sysconfdir="$(SYSCONFDIR)" \
			--wrap-mode nodownload  \
			-Ddbuspolicydir="$(PREFIX)"/share/dbus-1/system-services \
			-Ddbussystemservicedir="$(PREFIX)"/share/dbus-1/system-services \
			-Dbashcompletiondir="$(PREFIX)"/share/bash-completion/completions \
			-Dzshcompletiondir="$(PREFIX)"/share/zsh/site-functions \
			-Dacl=$(USE_ACL) \
			-Daudit=$(USE_AUDIT) \
			-Dcgroup-controller=$(CGCONTROL) \
			-Ddbus=enabled \
			-Ddefault-hierarchy=$(CGDEFAULT) \
			-Defi=$(USE_EFI) \
			-Dhtml=$(USE_HTML) \
			-Dman=$(USE_MAN) \
			-Dpam=enabled \
			-Dselinux=$(USE_SELINUX) \
			-Dsmack=$(USE_SMACK) \
			-Dutmp=$(USE_UTMP) \
			-Dxenctrl=$(USE_XENCTRL) \
			-Dmode=$(BUILDMODE) \
	)

.DEFAULT: all
#endif // 0
