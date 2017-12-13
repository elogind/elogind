.PHONY: all install test test-login

all:
	ninja -C build

install:
	DESTDIR=$(DESTDIR) ninja -C build install

test:
	ninja -C build test

test-login:
	ninja -C build test-login
