#!/bin/sh
set -eux

src="$1"
dst="$2"
target="$3"
options="$4"

[ -d "$dst" ] || meson "$src" "$dst" $options

# Locate ninja binary, on CentOS 7 it is called ninja-build, so
# use that name if available.
ninja=ninja
if which ninja-build >/dev/null 2>&1 ; then
    ninja=ninja-build
fi

"$ninja" -C "$dst" "$target"
