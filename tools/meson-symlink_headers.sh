#!/bin/sh -eu

INCDIR="$1"
shift 1

while [ $# -gt 0 ] ; do
    ln -vfs "systemd/$1" "${DESTDIR:-}/${INCDIR}/elogind/$1"
	shift 1
done
