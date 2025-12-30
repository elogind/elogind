---
title: Known Environment Variables
category: Interfaces
layout: default
SPDX-License-Identifier: LGPL-2.1-or-later
---

# Known Environment Variables

A number of elogind components take additional runtime parameters via
environment variables. Many of these environment variables are not supported at
the same level as command line switches and other interfaces are: we don't
document them in the man pages and we make no stability guarantees for
them. While they generally are unlikely to be dropped any time soon again, we
do not want to guarantee that they stay around for good either.

Below is an (incomprehensive) list of the environment variables understood by
the various tools. Note that this list only covers environment variables not
documented in the proper man pages.

All tools:

* `$SYSTEMD_OFFLINE=[0|1]` — if set to `1`, then `systemctl` will refrain from
  talking to PID 1; this has the same effect as the historical detection of
  `chroot()`. Setting this variable to `0` instead has a similar effect as
  `$SYSTEMD_IN_CHROOT=0`; i.e. tools will try to communicate with PID 1
  even if a `chroot()` environment is detected. You almost certainly want to
  set this to `1` if you maintain a package build system or similar and are
  trying to use a modern container system and not plain `chroot()`.

* `$SYSTEMD_IN_CHROOT=0|1` — takes a boolean. If set, overrides chroot detection.
  This is particularly relevant for systemctl, as it will not alter its behaviour
  for `chroot()` environments if `SYSTEMD_IN_CHROOT=0`. Normally it refrains from
  talking to PID 1 in such a case; turning most operations such as `start` into
  no-ops. If that's what's explicitly desired, you might consider setting
  `$SYSTEMD_OFFLINE=1`.

* `$SD_EVENT_PROFILE_DELAYS=1` — if set, the sd-event event loop implementation
  will print latency information at runtime.

* `$SYSTEMD_PROC_CMDLINE` — if set, the contents are used as the kernel command
  line instead of the actual one in `/proc/cmdline`. This is useful for
  debugging, in order to test generators and other code against specific kernel
  command lines.

* `$SYSTEMD_OS_RELEASE` — if set, use this path instead of `/etc/os-release` or
  `/usr/lib/os-release`. When operating under some root, the path is prefixed
  with the root. Only useful for debugging.

* `$SYSTEMD_EFI_OPTIONS` — if set, used instead of the string in the
  `SystemdOptions` EFI variable. Analogous to `$SYSTEMD_PROC_CMDLINE`.

* `$SYSTEMD_DEFAULT_HOSTNAME` — override the compiled-in fallback hostname
  (relevant in particular for wall messages).
  Must be a valid hostname (either a single label or a FQDN).

* `$SYSTEMD_BUS_TIMEOUT=SECS` — specifies the maximum time to wait for method call
  completion. If no time unit is specified, assumes seconds. The usual other units
  are understood, too (us, ms, s, min, h, d, w, month, y). If it is not set or set
  to 0, then the built-in default is used.

* `$SYSTEMD_MEMPOOL=0` — if set, the internal memory caching logic employed by
  hash tables is turned off, and libc `malloc()` is used for all allocations.

* `$SYSTEMD_UTF8=` — takes a boolean value, and overrides whether to generate
  non-ASCII special glyphs at various places (i.e. "→" instead of
  "->"). Usually this is determined automatically, based on `$LC_CTYPE`, but in
  scenarios where locale definitions are not installed it might make sense to
  override this check explicitly.

* `$SYSTEMD_EMOJI=0` — if set, no tools will output graphical smiley emojis,
  but ASCII alternatives instead. Note that this only controls use of Unicode
  emoji glyphs, and has no effect on other Unicode glyphs.

* `$SYSTEMD_ENABLE_LOG_CONTEXT` — if set, extra fields will always be logged to
  the journal instead of only when logging in debug mode.

* `$SYSTEMD_NETLINK_DEFAULT_TIMEOUT` — specifies the default timeout of waiting
  replies for netlink messages from the kernel. Defaults to 25 seconds.

`loginctl`:

* `$SYSTEMCTL_FORCE_BUS=1` — if set, do not connect to PID 1's private D-Bus
  listener, and instead always connect through the dbus-daemon D-bus broker.

`elogind`:

* `$SYSTEMD_BYPASS_HIBERNATION_MEMORY_CHECK=1` — if set, report that
  hibernation is available even if the swap devices do not provide enough room
  for it.

* `$SYSTEMD_REBOOT_TO_FIRMWARE_SETUP` — if set, overrides `elogind`'s
  built-in EFI logic of requesting a reboot into the firmware. Takes a boolean.
  If set to false, the functionality is turned off entirely. If set to true,
  instead of requesting a reboot into the firmware setup UI through EFI a file,
  `/run/systemd/reboot-to-firmware-setup` is created whenever this is
  requested. This file may be checked for by services run during system
  shutdown in order to request the appropriate operation from the firmware in
  an alternative fashion.

* `$SYSTEMD_REBOOT_TO_BOOT_LOADER_MENU` — similar to the above, allows
  overriding of `elogind`'s built-in EFI logic of requesting a reboot
  into the boot loader menu. Takes a boolean. If set to false, the
  functionality is turned off entirely. If set to true, instead of requesting a
  reboot into the boot loader menu through EFI, the file
  `/run/systemd/reboot-to-boot-loader-menu` is created whenever this is
  requested. The file contains the requested boot loader menu timeout in µs,
  formatted in ASCII decimals, or zero in case no timeout is requested. This
  file may be checked for by services run during system shutdown in order to
  request the appropriate operation from the boot loader in an alternative
  fashion.

* `$SYSTEMD_REBOOT_TO_BOOT_LOADER_ENTRY` — similar to the above, allows
  overriding of `elogind`'s built-in EFI logic of requesting a reboot
  into a specific boot loader entry. Takes a boolean. If set to false, the
  functionality is turned off entirely. If set to true, instead of requesting a
  reboot into a specific boot loader entry through EFI, the file
  `/run/systemd/reboot-to-boot-loader-entry` is created whenever this is
  requested. The file contains the requested boot loader entry identifier. This
  file may be checked for by services run during system shutdown in order to
  request the appropriate operation from the boot loader in an alternative
  fashion. Note that by default only boot loader entries which follow the
  [Boot Loader Specification](https://uapi-group.org/specifications/specs/boot_loader_specification)
  and are placed in the ESP or the Extended Boot Loader partition may be
  selected this way. However, if a directory `/run/boot-loader-entries/`
  exists, the entries are loaded from there instead. The directory should
  contain the usual directory hierarchy mandated by the Boot Loader
  Specification, i.e. the entry drop-ins should be placed in
  `/run/boot-loader-entries/loader/entries/*.conf`, and the files referenced by
  the drop-ins (including the kernels and initrds) somewhere else below
  `/run/boot-loader-entries/`. Note that all these files may be (and are
  supposed to be) symlinks. `elogind` will load these files on-demand,
  these files can hence be updated (ideally atomically) whenever the boot
  loader configuration changes. A foreign boot loader installer script should
  hence synthesize drop-in snippets and symlinks for all boot entries at boot
  or whenever they change if it wants to integrate with `elogind`'s
  APIs.

sd-device library:

* `$SYSTEMD_DEVICE_VERIFY_SYSFS` — if set to "0", disables verification that
  devices sysfs path are actually backed by sysfs. Relaxing this verification
  is useful for testing purposes.

`nss-elogind`:

* `$SYSTEMD_NSS_BYPASS_SYNTHETIC=1` — if set, `nss-elogind` won't synthesize
  user/group records for the `root` and `nobody` users if they are missing from
  `/etc/passwd`.

* `$SYSTEMD_NSS_DYNAMIC_BYPASS=1` — if set, `nss-elogind` won't return
  user/group records for dynamically registered service users (i.e. users
  registered through `DynamicUser=1`).

Parts that access the EFI System Partition (ESP):

* `$SYSTEMD_RELAX_ESP_CHECKS=1` — if set, the ESP validation checks are
  relaxed. Specifically, validation checks that ensure the specified ESP path
  is a FAT file system are turned off, as are checks that the path is located
  on a GPT partition with the correct type UUID.

* `$SYSTEMD_ESP_PATH=…` — override the path to the EFI System Partition. This
  may be used to override ESP path auto detection, and redirect any accesses to
  the ESP to the specified directory. Note that unlike with `bootctl`'s
  `--path=` switch only very superficial validation of the specified path is
  done when this environment variable is used.

systemd tests:

* `$SYSTEMD_TEST_NSS_BUFSIZE` — size of scratch buffers for "reentrant"
  functions exported by the nss modules.

* `$TESTFUNCS` – takes a colon separated list of test functions to invoke,
  causes all non-matching test functions to be skipped. Only applies to tests
  using our regular test boilerplate.

Tools using the Varlink protocol (such as `varlinkctl`) or sd-bus (such as
`busctl`):

* `$SYSTEMD_SSH` – the ssh binary to invoke when the `ssh:` transport is
  used. May be a filename (which is searched for in `$PATH`) or absolute path.

* `$SYSTEMD_VARLINK_LISTEN` – interpreted by some tools that provide a Varlink
  service. Takes a file system path: if specified the tool will listen on an
  `AF_UNIX` stream socket on the specified path in addition to whatever else it
  would listen on. If set to "-" the tool will turn stdin/stdout into a Varlink
  connection.
