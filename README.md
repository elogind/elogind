Elogind User, Seat and Session Manager

Introduction
============

Elogind is the systemd project's "logind", extracted out to be a
standalone daemon.  It integrates with PAM to know the set of users
that are logged in to a system and whether they are logged in
graphically, on the console, or remotely.  Elogind exposes this
information via the standard org.freedesktop.login1 D-Bus interface,
as well as through the file system using systemd's standard
/run/systemd layout.  Elogind also provides "libelogind", which is a
subset of the facilities offered by "libsystemd".  There is a
"libelogind.pc" pkg-config file as well.

All of the credit for elogind should go to the systemd developers.  
For more on systemd, see  
  http://www.freedesktop.org/wiki/Software/systemd  
All of the blame should go to Andy Wingo, who extracted elogind
from systemd.  
All complaints should go to Sven Eden, who is maintaining elogind.
But you could also [Buy Him A Coffee](https://www.buymeacoffee.com/EdenWorX)
instead if you like elogind and want to say thanks.

Build Status
------------
* CI Status main       : [![elogind CI Status main](https://github.com/elogind/elogind/actions/workflows/build.yml/badge.svg?branch=main)](https://github.com/elogind/elogind/actions)
* CI Status v252-stable: [![elogind CI Status v252-stable](https://github.com/elogind/elogind/actions/workflows/build.yml/badge.svg?branch=v252-stable)](https://github.com/elogind/elogind/actions)
* CI Status v255-stable: [![elogind CI Status v255-stable](https://github.com/elogind/elogind/actions/workflows/build.yml/badge.svg?branch=v255-stable)](https://github.com/elogind/elogind/actions)

Contributing
============

Elogind was branched from systemd version 219, and preserves the git
history of the systemd project.  The version of elogind is the
upstream systemd version, followed by the patchlevel of elogind.  For
example version 219.12 is the twelfth elogind release, which aims to
provide a subset of the interfaces of systemd 219.

To contribute to elogind, fork the current source code from github:  
  https://github.com/elogind/elogind  
Send a pull request for the changes you like.

If you do not have a github account, the elogind wiki page at  
  https://github.com/elogind/elogind/wiki
lists further possibilities to contact the maintainers.

To chat about elogind:
  #elogind on freenode

Bug reports should go to:
  https://github.com/elogind/elogind/issues

Why bother?
===========

Elogind has been developed for use in Guix System, the OS distribution of
GNU Guix.  See http://gnu.org/s/guix for more on Guix.  Guix System uses a
specific init manager (GNU Shepherd), for reasons that are not relevant
here, but still aims to eventually be a full-featured distribution that
can run GNOME and other desktop environments.  However, to run GNOME
these days means that you need to have support for the login1 D-Bus
interface, which is currently only provided by systemd.  That is the
origin of this project: to take the excellent logind functionality
from systemd and provide it as a standalone package.

You're welcome to use elogind for whatever purpose you like --
as-is, or as a jumping-off point for other things -- but please don't
use it as part of some anti-systemd vendetta. We are appreciative of
the systemd developers logind effort and think that everyone deserves
to run it if they like. No matter what kind of PID1 they use.

Differences relative to systemd
===============================

The pkg-config file is called libelogind, not libsystemd or
libsystemd-logind.

The headers are in `<elogind/...>`, like `<elogind/sd-login.h>`
instead of `<systemd/sd-login.h>`.  
To make it easier for projects to add support for elogind, there is a
subfolder "systemd" in the elogind include directory. So if
`pkg-config` is used to get the cflags, including
`<systemd/sd-login.h>` will still work.

Libelogind just implements login-related functionality.  It also
provides the sd-bus API.

Unlike systemd, whose logind arranges to manage resources for user
sessions via RPC calls to systemd, in elogind there is no systemd so
there is no global cgroup-based resource management.  This has a few
implications:

  * Elogind does not create "slices" for users.  Elogind will not
    record that users are associated with slices.
  * The /run/systemd/slices directory will always be empty.
  * Elogind does not have the concept of a "scope", internally, as
    it's the same as a session. Any API that refers to scopes will
    always return an error code.

On the other hand, elogind does use a similar strategy to systemd in
that it places processes in a private cgroup for organizational
purposes, without installing any controllers (see
http://0pointer.de/blog/projects/cgroups-vs-cgroups.html).  This
allows elogind to map arbitrary processes to sessions, even if the
process does the usual double-fork to be reparented to PID 1.

Elogind does not manage virtual terminals.

Elogind does monitor power button and the lid switch, like systemd,
but instead of doing RPC to systemd to suspend, poweroff, or restart
the machine, elogind just does this directly.  For suspend, hibernate,
and hybrid-sleep, elogind uses the same code as systemd-sleep.
Instead of using a separate sleep.conf file to configure the sleep
behavior, this is included in the [Sleep] section of
`/etc/elogind/login.conf`. See the example `login.conf` for more.  
For shutdown, reboot, and kexec, elogind shells out to `halt`,
`reboot` and `kexec` binaries.

The loginctl command has the `poweroff`, `reboot`, `suspend`,
`hibernate`, `hybrid-sleep` and `suspend-then-hibernate` commands
from systemd, as well as the `--ignore-inhibitors` flag.

The PAM module is called `pam_elogind.so`, not `pam_systemd.so`.

Elogind and the running cgroup controller
=========================================
While `meson` runs, it will detect which controller is in place.
If no controller is in place, configure will determine, that elogind
should be its own controller, which will be a very limited one.

This approach should generally work, but if you just have no cgroup
controller in place, yet, or if you are currently switching to
another one, this approach will fail.

In this case you can do one of the two following things:

 1) Boot your system with the target init system and cgroup
    controller, before configuring and building elogind, or
 2) Use the `--with-cgroup-controller=name` option.

Example: If you plan to use openrc, but openrc has not yet booted
         the machine, you can use  
         `--with-cgroup-controller=openrc`  
         to let elogind know that openrc will be the controller
         in charge.

However, if you set the controller at configure time to something
different than what is in place, elogind will not start until that
controller is actively used as the primary controller.

ABI compatibility with libsystemd
=================================

Basically all symbols are included. But any API calls that require to
call systemd, or need internal knowledge of systemd, are simple stubs.
They are there to provide ABI compatibility, but will not work.

One exception is `sd_is_mq()` that is found in sd-daemon.h. This is the
only place using POSIX message queues, which would add further
dependencies. As those would be completely unused in the rest of
elogind, this function is also a stub, and always returns -ENOSYS.

License
=======

LGPLv2.1+ for all code
  - except `src/basic/MurmurHash2.c` which is Public Domain
  - except `src/basic/siphash24.c` which is CC0 Public Domain

Dependencies
============

  * glibc >= 2.16 (*or* musl-libc >= 1.1.20)
  * libcap
  * libudev
  * PAM >= 1.1.2 (optional)
  * libacl (optional)
  * libselinux (optional)
  * libaudit (optional)
  * pkg-config
  * gperf >= 3.1
  * docbook-xsl (optional, required for documentation)
  * xsltproc    (optional, required for documentation)
  * python-lxml (optional, required to build the indices)
  * python, meson, ninja
  * gcc, awk, sed, grep, m4, and similar tools

During runtime, you need the following additional dependencies:
---------------------------------------------------------------
  * util-linux >= v2.27.1 required
  * dbus >= 1.9.14 (strictly speaking optional, but recommended)  
    NOTE: If using dbus < 1.9.18, you should override the default  
          policy directory (--with-dbuspolicydir=/etc/dbus-1/system.d).
  * PolicyKit (optional)

To build in directory build/:  
    `meson build/ && ninja -C build`

If you plan to use a build directory outside the source tree, make sure that
it is not too '_far away_'. To detect broken setups, some compiler magic is
included to check whether the relative path to the sources is shorter than the
absolute path to each source file.
So if you get an error like `error: size of array 'x' is negative` when the
macro `assert_cc(STRLEN(FILE) > STRLEN(RELATIVE_SOURCE_PATH) + 1);` is
expanded, put your build directory nearer to the source tree.

Any configuration options can be specified as -Darg=value... arguments
to meson. After the build directory is initially configured, the configuration
can be changed with:  
    `meson configure -Darg=value... build/`  
`meson configure` without any arguments will print out available options and
their current values.

Useful commands:  
----------------
  * `ninja -v some/target`
  * `ninja test`
  * `sudo ninja install`
  * `DESTDIR=... ninja install`
  * `make DEBUG=YES`  
    The Makefile is a full convenience wrapper, that allows to use meson/ninja in  
    Makefile compatible IDEs like CLion.  
    Note: For maximum control you should use meson/ninja directly instead.  

A tarball can be created with:  
  `git archive --format=tar --prefix=elogind-241/ v241 | xz > elogind-241.tar.xz`

--------

Many thanks to JetBrains and their
"[Licenses for Open Source Development](https://jb.gg/OpenSourceSupport)"
program, providing free licenses for

![CLion by JetBrains](https://resources.jetbrains.com/storage/products/company/brand/logos/CLion.svg)
