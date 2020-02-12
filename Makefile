#if 0 /// The original Makefile follows, which isn't enough for elogind.
# all:
# 	ninja -C build
#
# install:
# 	DESTDIR=$(DESTDIR) ninja -C build install
#else // 0
.PHONY: all clean install loginctl rebuild test test-login

VERSION ?= 999
VARIANT ?= debug

HERE   := $(shell pwd -P)

BUILD  := $(HERE)/build
COMPDB := compile_commands.json
LN     := $(shell which ln) -s
RM     := $(shell which rm) -f


all:
	ninja -C build

clean:
	$(RM) -f $(COMPDB)
	$(HERE)/pwx/rebuild_all.sh $(VERSION) $(VARIANT)

install:
	DESTDIR=$(DESTDIR) ninja -C $(BUILD) install

loginctl:
	ninja -C $(BUILD) $@

rebuild: clean
	$(LN) $(BUILD)/$(COMPDB) $(COMPDB)

test:
	ninja -C $(BUILD) $@

test-login:
	ninja -C $(BUILD) $@

#endif // 0
