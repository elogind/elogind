#!/bin/sh

#  This file is part of elogind
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

set -e

oldpwd=$(pwd)
topdir=$(dirname $0)
cd $topdir

intltoolize --force --automake
autoreconf --force --install --symlink

libdir() {
        echo $(cd "$1/$(gcc -print-multi-os-directory)"; pwd)
}

args="\
--sysconfdir=/etc \
--localstatedir=/var \
--libdir=$(libdir /usr/lib) \
"

if [ -f "$topdir/.config.args" ]; then
        args="$args $(cat $topdir/.config.args)"
fi

if [ ! -L /bin ]; then
args="$args \
--with-rootprefix=/ \
--with-rootlibdir=$(libdir /lib) \
"
fi

cd $oldpwd

echo
echo "----------------------------------------------------------------"
echo "Initialized build system. For a common configuration please run:"
echo "----------------------------------------------------------------"
echo
echo "$topdir/configure CFLAGS='-g -O0 -ftrapv' --enable-kdbus $args"
echo
