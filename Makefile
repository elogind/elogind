#if 0 /// The original Makefile follows, which isn't enough for elogind.
# all:
# 	ninja -C build
#
# install:
# 	DESTDIR=$(DESTDIR) ninja -C build install
#else // 0
.PHONY: all install loginctl test test-login

all:
	ninja -C build

install:
	DESTDIR=$(DESTDIR) ninja -C build install

loginctl:
	ninja -C build loginctl

test:
	ninja -C build test

test-login:
	ninja -C build test-login
#endif // 0
