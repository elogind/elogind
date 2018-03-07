#!/bin/bash

xVersion="$1"
xType="$2"

if [[ "x$xType" != "xdebug" ]] && [[ "x$xType" != "xrelease" ]]; then
	echo "Usage: $0 <version> <debug|release> [additional configure options]"
	exit 1
fi

shift 2

minVers=${xVersion:0:3}

if [[ ! $minVers =~ [0-9]{3} ]]; then
	echo "The version must be like \"nnn[.n[n]]\""
	exit 2
fi

PREFIX=/tmp/elogind_test
if [[ $UID -eq 0 ]]; then
	PREFIX=""
else
	rm -rf $PREFIX
	mkdir -p $PREFIX
fi

my_CFLAGS="$CFLAGS"
my_LDFLAGS="$LDFLAGS"

if [[ "x$xType" = "xdebug" ]]; then
	my_CFLAGS="-Og -g3 -ggdb -ftrapv ${my_CFLAGS} -fPIE"
	LDFLAGS="${my_LDFLAGS} -fPIE"
else
	my_CFLAGS="-O2 -fwrapv ${my_CFLAGS}"
fi

if [[ $minVers -gt 234 ]]; then
	# After 234 the new meson+ninja build system is used:

	debug_opt="-Ddebug=\"\" --buildtype release"
	if [[ "x$xType" = "xdebug" ]]; then
		debug_opt="-Ddebug=elogind -Dtest=unsafe --buildtype debug"
	fi

	set -x
	rm -rf build
	mkdir build

	cgdefault="$(grep "^rc_cgroup_mode" /etc/rc.conf | cut -d '"' -f 2)"

	extra_opts="$@"

	set +x
	if [[ "x$extra_opts" != "x" ]]; then
		echo -n "Configure ? [y/N] (cg $cgdefault) [$extra_opts]"
	else
		echo -n "Configure ? [y/N] (cg $cgdefault)  "
	fi
	read answer

	if [[ "x$answer" != "xy" ]]; then
		exit 0
	fi

	set -x

	CFLAGS="-march=native -pipe ${my_CFLAGS} -Wall -Wextra -Wunused -Wno-unused-parameter -Wno-unused-result -ftree-vectorize" \
	LDFLAGS="${my_LDFLAGS}" \
		meson $debug_opt --prefix $PREFIX/usr -Drootprefix=$PREFIX \
			--wrap-mode nodownload --libdir lib64 \
			--localstatedir $PREFIX/var/lib  --sysconfdir $PREFIX/etc \
			 -Ddocdir=$PREFIX/usr/share/doc/elogind-9999 \
			 -Dhtmldir=$PREFIX/usr/share/doc/elogind-9999/html \
			 -Dpamlibdir=$PREFIX/lib64/security \
			 -Dudevrulesdir=$PREFIX/lib/udev/rules.d \
			 --libdir=$PREFIX/usr/lib64 -Drootlibdir=$PREFIX/lib64 \
			 -Drootlibexecdir=$PREFIX/lib64/elogind \
			 -Dsmack=true -Dman=auto -Dhtml=auto \
			 -Dcgroup-controller=openrc -Ddefault-hierarchy=$cgdefault \
			 -Dacl=true -Dpam=true -Dselinux=false \
			 -Dbashcompletiondir=$PREFIX/usr/share/bash-completion/completions \
			 -Dzsh-completion=$PREFIX/usr/share/zsh/site-functions \
			 $extra_opts $(pwd -P) $(pwd -P)/build

	set +x
	echo -n "Build and install ? [y/N] "
	read answer

	if [[ "x$answer" = "xy" ]]; then
		set -x
		ninja -C build && ninja -C build install
		set +x
	fi
else
	# Up to 233 the old autotools build system is used

	debug_opt="--disable-debug"
	if [[ "x$xType" = "xdebug" ]]; then
#		debug_opt="--enable-debug=elogind --enable-address-sanitizer --enable-undefined-sanitizer --enable-coverage"
		debug_opt="--enable-debug=elogind --enable-address-sanitizer --enable-undefined-sanitizer"
#		debug_opt="--enable-debug=elogind --enable-undefined-sanitizer --enable-coverage"
#		debug_opt="--enable-debug=elogind --enable-undefined-sanitizer"
	fi

	set -x
	make clean
	make distclean
	intltoolize --automake --copy --force
	libtoolize --install --copy --force --automake
	aclocal -I m4
	autoconf --force
	autoheader
	automake --add-missing --copy --foreign --force-missing
	set +x

	extra_opts="$@"
	if [[ "x$extra_opts" != "x" ]]; then
		echo -n "Configure ? [y/N] [$extra_opts]"
	else
		echo -n "Configure ? [y/N] "
	fi
	read answer

	if [[ "x$answer" != "xy" ]]; then
		exit 0
	fi

	set --x
	CFLAGS="-march=native -pipe ${my_CFLAGS} -Wall -Wextra -Wunused -Wno-unused-parameter -Wno-unused-result -ftree-vectorize" \
	LDFLAGS="${my_LDFLAGS}" \
		./configure --prefix=$PREFIX/usr --with-rootprefix=$PREFIX/ \
			--with-bashcompletiondir=$PREFIX/usr/share/bash-completion/completions \
			--enable-dependency-tracking --disable-silent-rules \
			--build=x86_64-pc-linux-gnu --host=x86_64-pc-linux-gnu \
			--mandir=$PREFIX/usr/share/man --infodir=$PREFIX/usr/share/info \
			--datadir=$PREFIX/usr/share --sysconfdir=$PREFIX/etc \
			--localstatedir=$PREFIX/var/lib \
			--docdir=$PREFIX/usr/share/doc/elogind-${xVersion} \
			--htmldir=$PREFIX/usr/share/doc/elogind-${xVersion}/html \
			--with-pamlibdir=$PREFIX/lib64/security \
			--with-udevrulesdir=$PREFIX/lib/udev/rules.d \
			--enable-smack --with-cgroup-controller=openrc \
			--enable-acl --enable-pam --disable-selinux \
			$debug_opt $extra_opts
	set +x

	echo -n "Build and install ? [y/N] "
	read answer

	if [[ "x$answer" = "xy" ]]; then
		set -x
		make update-man-list && \
		make -j 17 && \
		make update-man-list && \
		make && make install
		set +x
	fi
fi
