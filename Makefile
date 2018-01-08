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
