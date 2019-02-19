#if 0 /// Here follows the original Makefile, which is too little for elogind
# all:
# 	ninja -C build
#
# install:
# 	DESTDIR=$(DESTDIR) ninja -C build install
#else
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
