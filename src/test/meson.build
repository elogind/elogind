# SPDX-License-Identifier: LGPL-2.1+
#
# Copyright 2017 Zbigniew Jędrzejewski-Szmek
#
# elogind is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation; either version 2.1 of the License, or
# (at your option) any later version.
#
# elogind is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with elogind; If not, see <http://www.gnu.org/licenses/>.

awkscript = 'test-hashmap-ordered.awk'
test_hashmap_ordered_c = custom_target(
        'test-hashmap-ordered.c',
        input : [awkscript, 'test-hashmap-plain.c'],
        output : 'test-hashmap-ordered.c',
        command : [awk, '-f', '@INPUT0@', '@INPUT1@'],
        capture : true)

test_include_dir = include_directories('.')

path = run_command('sh', ['-c', 'echo "$PATH"']).stdout()
test_env = environment()
#if 0 /// UNNEEDED in elogind
# test_env.set('SYSTEMD_KBD_MODEL_MAP', kbd_model_map)
# test_env.set('SYSTEMD_LANGUAGE_FALLBACK_MAP', language_fallback_map)
#endif // 0
test_env.set('PATH', path)
test_env.prepend('PATH', meson.build_root())

############################################################

generate_sym_test_py = find_program('generate-sym-test.py')

test_libelogind_sym_c = custom_target(
        'test-libelogind-sym.c',
        input : [libelogind_sym_path] + systemd_headers,
        output : 'test-libelogind-sym.c',
        command : [generate_sym_test_py, libelogind_sym_path] + systemd_headers,
        capture : true)

#if 0 /// UNNEEDED in elogind
# test_libudev_sym_c = custom_target(
#         'test-libudev-sym.c',
#         input : [libudev_sym_path, libudev_h_path],
#         output : 'test-libudev-sym.c',
#         command : [generate_sym_test_py, '@INPUT0@', '@INPUT1@'],
#         capture : true)
#endif // 0

test_dlopen_c = files('test-dlopen.c')

############################################################

#if 0 /// UNNEEDED by elogind
# test_systemd_tmpfiles_py = find_program('test-systemd-tmpfiles.py')
#endif // 0

############################################################

tests += [
#if 0 /// UNNEEDED in elogind
#         [['src/test/test-device-nodes.c'],
#          [],
#          []],
# 
#         [['src/test/test-engine.c',
#           'src/test/test-helper.c'],
#          [libcore,
#           libudev,
#           libshared],
#          [threads,
#           librt,
#           libseccomp,
#           libselinux,
#           libmount,
#           libblkid]],
# 
#         [['src/test/test-job-type.c'],
#          [libcore,
#           libshared],
#          [threads,
#           librt,
#           libseccomp,
#           libselinux,
#           libmount,
#           libblkid]],
# 
#         [['src/test/test-ns.c'],
#          [libcore,
#           libshared],
#          [threads,
#           librt,
#           libseccomp,
#           libselinux,
#           libmount,
#           libblkid],
#          '', 'manual'],
# 
#         [['src/test/test-loopback.c'],
#          [libcore,
#           libshared],
#          [threads,
#           librt,
#           libseccomp,
#           libselinux,
#           libmount,
#           libblkid]],
# 
#         [['src/test/test-hostname.c'],
#          [libcore,
#           libshared],
#          [threads,
#           librt,
#           libseccomp,
#           libselinux,
#           libmount,
#           libblkid],
#          '', 'unsafe'],
# 
#         [['src/test/test-dns-domain.c'],
#          [libcore,
#           libshared,
#           libsystemd_network],
#          []],
# 
#         [['src/test/test-boot-timestamps.c'],
#          [],
#          [],
#          'ENABLE_EFI'],
# 
#         [['src/test/test-unit-name.c',
#           'src/test/test-helper.c'],
#          [libcore,
#           libshared],
#          [threads,
#           librt,
#           libseccomp,
#           libselinux,
#           libmount,
#           libblkid]],
# 
#         [['src/test/test-unit-file.c',
#           'src/test/test-helper.c'],
#          [libcore,
#           libshared],
#          [threads,
#           librt,
#           libseccomp,
#           libselinux,
#           libmount,
#           libblkid]],
#endif // 0

        [['src/test/test-utf8.c'],
         [],
         []],

#if 0 /// UNNEEDED in elogind
#         [['src/test/test-capability.c'],
#          [],
#          [libcap]],
# 
#         [['src/test/test-async.c'],
#          [],
#          [],
#          '', 'timeout=120'],
#endif // 0

        [['src/test/test-locale-util.c'],
         [],
         []],

        [['src/test/test-copy.c'],
         [],
         []],

#if 0 /// UNNEEDED in elogind
#         [['src/test/test-sigbus.c'],
#          [],
#          []],
# 
#         [['src/test/test-condition.c'],
#          [],
#          []],
# 
#         [['src/test/test-fdset.c'],
#          [],
#          []],
# 
#         [['src/test/test-fstab-util.c'],
#          [],
#          []],
#endif // 0

        [['src/test/test-random-util.c'],
         [],
         []],

#if 0 /// UNNEEDED in elogind
#         [['src/test/test-ratelimit.c'],
#          [],
#          []],
#endif // 0

        [['src/test/test-util.c'],
         [],
         []],

#if 0 /// UNNEEDED in elogind
#         [['src/test/test-mount-util.c'],
#          [],
#          []],
#endif // 0

        [['src/test/test-exec-util.c'],
         [],
         []],

        [['src/test/test-hexdecoct.c'],
         [],
         []],

        [['src/test/test-alloc-util.c'],
         [],
         []],

#if 0 /// UNNEEDED in elogind
#         [['src/test/test-xattr-util.c'],
#          [],
#          []],
#endif // 0

        [['src/test/test-io-util.c'],
         [],
         []],

#if 0 /// UNNEEDED in elogind
#         [['src/test/test-glob-util.c'],
#          [],
#          []],
#endif // 0

        [['src/test/test-fs-util.c'],
         [],
         []],

        [['src/test/test-proc-cmdline.c'],
         [],
         []],

        [['src/test/test-fd-util.c'],
         [],
         []],

#if 0 /// UNNEEDED in elogind
#         [['src/test/test-web-util.c'],
#          [],
#          []],
# 
#         [['src/test/test-cpu-set-util.c'],
#          [],
#          []],
#endif // 0

        [['src/test/test-stat-util.c'],
         [],
         []],

        [['src/test/test-escape.c'],
         [],
         []],

#if 0 /// UNNEEDED in elogind
#         [['src/test/test-specifier.c'],
#          [],
#          []],
#endif // 0

        [['src/test/test-string-util.c'],
         [],
         []],

        [['src/test/test-extract-word.c'],
         [],
         []],

        [['src/test/test-parse-util.c'],
         [],
         []],

        [['src/test/test-user-util.c'],
         [],
         []],

#if 0 /// UNNEEDED in elogind
#         [['src/test/test-hostname-util.c'],
#          [],
#          []],
#endif // 0

        [['src/test/test-process-util.c'],
         [],
         []],

#if 0 /// UNNEEDED in elogind
#         [['src/test/test-terminal-util.c'],
#          [],
#          []],
# 
#         [['src/test/test-path-lookup.c'],
#          [],
#          []],
# 
#         [['src/test/test-uid-range.c'],
#          [],
#          []],
# 
#         [['src/test/test-cap-list.c',
#           generated_gperf_headers],
#          [],
#          [libcap]],
# 
#         [['src/test/test-socket-util.c'],
#          [],
#          []],
# 
#         [['src/test/test-in-addr-util.c'],
#          [],
#          []],
# 
#         [['src/test/test-barrier.c'],
#          [],
#          []],
# 
#         [['src/test/test-tmpfiles.c'],
#          [],
#          []],
# 
#         [['src/test/test-namespace.c'],
#          [libcore,
#           libshared],
#          [threads,
#           libblkid]],
#endif // 0

        [['src/test/test-verbs.c'],
         [],
         []],

#if 0 /// UNNEEDED in elogind
#         [['src/test/test-install-root.c'],
#          [],
#          []],
# 
#         [['src/test/test-acl-util.c'],
#          [],
#          [],
#          'HAVE_ACL'],
# 
#         [['src/test/test-seccomp.c'],
#          [],
#          [libseccomp],
#          'HAVE_SECCOMP'],
# 
#         [['src/test/test-rlimit-util.c'],
#          [],
#          []],
# 
#         [['src/test/test-ask-password-api.c'],
#          [],
#          [],
#          '', 'manual'],
# 
#         [['src/test/test-dissect-image.c'],
#          [],
#          [libblkid],
#          '', 'manual'],
#endif // 0

        [['src/test/test-signal-util.c'],
         [],
         []],

        [['src/test/test-selinux.c'],
         [],
         []],

        [['src/test/test-sizeof.c'],
         [libbasic],
         []],

#if 0 /// UNNEEDED in elogind
#         [['src/test/test-bpf.c',
#           'src/test/test-helper.c'],
#          [libcore,
#           libshared],
#          [libmount,
#           threads,
#           librt,
#           libseccomp,
#           libselinux,
#           libblkid]],
# 
#         [['src/test/test-watch-pid.c',
#           'src/test/test-helper.c'],
#          [libcore,
#           libshared],
#          [libmount,
#           threads,
#           librt,
#           libseccomp,
#           libselinux,
#           libblkid]],
#endif // 0

        [['src/test/test-hashmap.c',
          'src/test/test-hashmap-plain.c',
          test_hashmap_ordered_c],
         [],
         [],
         '', 'timeout=90'],

        [['src/test/test-set.c'],
         [],
         []],

#if 0 /// UNNEEDED in elogind
#         [['src/test/test-bitmap.c'],
#          [],
#          []],
# 
#         [['src/test/test-xml.c'],
#          [],
#          []],
#endif // 0

        [['src/test/test-list.c'],
         [],
         []],

        [['src/test/test-procfs-util.c'],
         [],
         []],

        [['src/test/test-unaligned.c'],
         [],
         []],

#if 0 /// UNNEEDED in elogind
#         [['src/test/test-tables.c',
#           'src/shared/test-tables.h',
#           'src/journal/journald-server.c',
#           'src/journal/journald-server.h'],
#          [libcore,
#           libjournal_core,
#           libudev_core,
#           libudev_static,
#           libsystemd_network,
#           libshared],
#          [threads,
#           libseccomp,
#           libmount,
#           libxz,
#           liblz4,
#           libblkid],
#          '', '', [], libudev_core_includes],
#endif // 0

        [['src/test/test-prioq.c'],
         [],
         []],

#if 0 /// UNNEEDED in elogind
#         [['src/test/test-fileio.c'],
#          [],
#          []],
# 
#         [['src/test/test-time-util.c'],
#          [],
#          []],
# 
#         [['src/test/test-clock.c'],
#          [],
#          []],
# 
#         [['src/test/test-architecture.c'],
#          [],
#          []],
#endif // 0

        [['src/test/test-log.c'],
         [],
         []],

        [['src/test/test-ipcrm.c'],
         [],
         [],
         '', 'unsafe'],

#if 0 /// UNNEEDED in elogind
#         [['src/test/test-btrfs.c'],
#          [],
#          [],
#          '', 'manual'],
# 
# 
#         [['src/test/test-firewall-util.c'],
#          [libshared],
#          [],
#          'HAVE_LIBIPTC'],
# 
#         [['src/test/test-netlink-manual.c'],
#          [],
#          [libkmod],
#          'HAVE_KMOD', 'manual'],
#endif // 0

        [['src/test/test-ellipsize.c'],
         [],
         []],

#if 0 /// UNNEEDED in elogind
#         [['src/test/test-date.c'],
#          [],
#          []],
# 
#         [['src/test/test-sleep.c'],
#          [],
#          []],
# 
#         [['src/test/test-replace-var.c'],
#          [],
#          []],
# 
#         [['src/test/test-calendarspec.c'],
#          [],
#          []],
#endif // 0

        [['src/test/test-strip-tab-ansi.c'],
         [],
         []],

#if 0 /// UNNEEDED in elogind
#         [['src/test/test-daemon.c'],
#          [],
#          []],
#endif // 0

        [['src/test/test-cgroup.c'],
         [],
         [],
         '', 'manual'],


#if 0 /// UNNEEDED in elogind
#         [['src/test/test-cgroup-mask.c',
#           'src/test/test-helper.c'],
#          [libcore,
#           libshared],
#          [threads,
#           librt,
#           libseccomp,
#           libselinux,
#           libmount,
#           libblkid]],
# 
#         [['src/test/test-cgroup-util.c'],
#          [],
#          []],
# 
#         [['src/test/test-env-util.c'],
#          [],
#          []],
# 
#         [['src/test/test-strbuf.c'],
#          [],
#          []],
# 
#         [['src/test/test-strv.c'],
#          [],
#          []],
#endif // 0

        [['src/test/test-path-util.c'],
         [],
         []],

#if 0 /// UNNEEDED in elogind
#         [['src/test/test-path.c',
#           'src/test/test-helper.c'],
#          [libcore,
#           libshared],
#          [threads,
#           librt,
#           libseccomp,
#           libselinux,
#           libmount,
#           libblkid]],
# 
#         [['src/test/test-execute.c',
#           'src/test/test-helper.c'],
#          [libcore,
#           libshared],
#          [threads,
#           librt,
#           libseccomp,
#           libselinux,
#           libmount,
#           libblkid]],
#endif // 0

        [['src/test/test-siphash24.c'],
         [],
         []],

#if 0 /// UNNEEDED in elogind
#         [['src/test/test-strxcpyx.c'],
#          [],
#          []],
# 
#         [['src/test/test-install.c'],
#          [libcore,
#           libshared],
#          [],
#          '', 'manual'],
# 
#         [['src/test/test-watchdog.c'],
#          [],
#          []],
# 
#         [['src/test/test-sched-prio.c',
#           'src/test/test-helper.c'],
#          [libcore,
#           libshared],
#          [threads,
#           librt,
#           libseccomp,
#           libselinux,
#           libmount,
#           libblkid]],
#endif // 0

        [['src/test/test-conf-files.c'],
         [],
         []],

        [['src/test/test-conf-parser.c'],
         [],
         []],

#if 0 /// UNNEEDED in elogind
#         [['src/test/test-af-list.c',
#           generated_gperf_headers],
#          [],
#          []],
# 
#         [['src/test/test-arphrd-list.c',
#           generated_gperf_headers],
#          [],
#          []],
# 
#         [['src/test/test-journal-importer.c'],
#          [],
#          []],
# 
#         [['src/test/test-libudev.c'],
#          [libshared],
#          []],
# 
#         [['src/test/test-udev.c'],
#          [libudev_core,
#           libudev_static,
#           libsystemd_network,
#           libshared],
#          [threads,
#           librt,
#           libblkid,
#           libkmod,
#           libacl],
#          '', 'manual'],
#endif // 0

        [['src/test/test-id128.c'],
         [],
         []],

        [['src/test/test-hash.c'],
         [],
         []],

#if 0 /// UNNEEDED in elogind
#         [['src/test/test-gcrypt-util.c'],
#          [],
#          [],
#          'HAVE_GCRYPT'],
# 
#         [['src/test/test-nss.c'],
#          [],
#          [libdl],
#          '', 'manual'],
#endif // 0
]

############################################################

# define some tests here, because the link_with deps were not defined earlier

#if 0 /// No systemd-journal in elogind, of course
# tests += [
#         [['src/journal/test-journal.c'],
#          [libjournal_core,
#           libshared],
#          [threads,
#           libxz,
#           liblz4]],
# 
#         [['src/journal/test-journal-send.c'],
#          [libjournal_core,
#           libshared],
#          [threads,
#           libxz,
#           liblz4]],
# 
#         [['src/journal/test-journal-syslog.c'],
#          [libjournal_core,
#           libshared],
#          [threads,
#           libxz,
#           liblz4,
#           libselinux]],
# 
#         [['src/journal/test-journal-match.c'],
#          [libjournal_core,
#           libshared],
#          [threads,
#           libxz,
#           liblz4]],
# 
#         [['src/journal/test-journal-enum.c'],
#          [libjournal_core,
#           libshared],
#          [threads,
#           libxz,
#           liblz4],
#          '', 'timeout=360'],
# 
#         [['src/journal/test-journal-stream.c'],
#          [libjournal_core,
#           libshared],
#          [threads,
#           libxz,
#           liblz4]],
# 
#         [['src/journal/test-journal-flush.c'],
#          [libjournal_core,
#           libshared],
#          [threads,
#           libxz,
#           liblz4]],
# 
#         [['src/journal/test-journal-init.c'],
#          [libjournal_core,
#           libshared],
#          [threads,
#           libxz,
#           liblz4]],
# 
#         [['src/journal/test-journal-verify.c'],
#          [libjournal_core,
#           libshared],
#          [threads,
#           libxz,
#           liblz4]],
# 
#         [['src/journal/test-journal-interleaving.c'],
#          [libjournal_core,
#           libshared],
#          [threads,
#           libxz,
#           liblz4]],
# 
#         [['src/journal/test-mmap-cache.c'],
#          [libjournal_core,
#           libshared],
#          [threads,
#           libxz,
#           liblz4]],
# 
#         [['src/journal/test-catalog.c'],
#          [libjournal_core,
#           libshared],
#          [threads,
#           libxz,
#           liblz4],
#          '', '', '-DCATALOG_DIR="@0@"'.format(build_catalog_dir)],
# 
#         [['src/journal/test-compress.c'],
#          [libjournal_core,
#           libshared],
#          [liblz4,
#           libxz]],
# 
#         [['src/journal/test-compress-benchmark.c'],
#          [libjournal_core,
#           libshared],
#          [liblz4,
#           libxz],
#          '', 'timeout=90'],
# 
#         [['src/journal/test-audit-type.c'],
#          [libjournal_core,
#           libshared],
#          [liblz4,
#           libxz]],
# ]
#endif // 0

############################################################

tests += [
#if 0 /// UNNEEDED in elogind
#         [['src/libelogind/sd-bus/test-bus-marshal.c'],
#          [],
#          [threads,
#           libglib,
#           libgobject,
#           libgio,
#           libdbus]],
#endif // 0

        [['src/libelogind/sd-bus/test-bus-signature.c'],
         [],
         [threads]],

#if 0 /// UNNEEDED in elogind
#         [['src/libsystemd/sd-bus/test-bus-watch-bind.c'],
#          [],
#          [threads], '', 'timeout=120'],
# 
#         [['src/libsystemd/sd-bus/test-bus-chat.c'],
#          [],
#          [threads]],
# 
#         [['src/libelogind/sd-bus/test-bus-cleanup.c'],
#          [],
#          [threads,
#           libseccomp]],
#endif // 0

        [['src/libelogind/sd-bus/test-bus-error.c'],
         [libshared_static,
          libelogind_static],
         []],

#if 0 /// UNNEEDED in elogind
#         [['src/libelogind/sd-bus/test-bus-track.c'],
#          [],
#          [libseccomp]],
#endif // 0

        [['src/libelogind/sd-bus/test-bus-server.c'],
         [],
         [threads]],

#if 0 /// UNNEEDED in elogind
#         [['src/libelogind/sd-bus/test-bus-objects.c'],
#          [],
#          [threads]],
#endif // 0

        [['src/libelogind/sd-bus/test-bus-vtable.c'],
         [],
         []],

#if 0 /// UNNEEDED in elogind
#         [['src/libelogind/sd-bus/test-bus-gvariant.c'],
#          [],
#          [libglib,
#           libgobject,
#           libgio]],
# 
#         [['src/libelogind/sd-bus/test-bus-creds.c'],
#          [],
#          []],
#endif // 0

        [['src/libelogind/sd-bus/test-bus-match.c'],
         [],
         []],

#if 0 /// UNNEEDED in elogind
#         [['src/libelogind/sd-bus/test-bus-benchmark.c'],
#          [],
#          [threads],
#          '', 'manual'],
#endif // 0

        [['src/libelogind/sd-bus/test-bus-introspect.c'],
         [],
         []],

        [['src/libelogind/sd-event/test-event.c'],
         [],
         []],

#if 0 /// UNNEEDED in elogind
#         [['src/libelogind/sd-netlink/test-netlink.c'],
#          [],
#          []],
# 
#         [['src/libelogind/sd-netlink/test-local-addresses.c'],
#          [],
#          []],
# 
#         [['src/libelogind/sd-resolve/test-resolve.c'],
#          [],
#          [threads]],
#endif // 0

        [['src/libelogind/sd-login/test-login.c'],
         [],
         []],
]

#if 0 /// UNNEEDED in elogind
# if cxx.found()
#         tests += [
#                 [['src/libelogind/sd-bus/test-bus-vtable-cc.cc'],
#                  [],
#                  []]
#         ]
# endif
#endif // 0

############################################################

#if 0 /// UNNEEDED in elogind
# tests += [
#         [['src/libsystemd-network/test-dhcp-option.c',
#           'src/libsystemd-network/dhcp-protocol.h',
#           'src/libsystemd-network/dhcp-internal.h'],
#          [libshared,
#           libelogind_network],
#          []],
# 
#         [['src/libsystemd-network/test-sd-dhcp-lease.c',
#           'src/libsystemd-network/dhcp-lease-internal.h'],
#          [libshared,
#           libelogind_network],
#          []],
# 
#         [['src/libsystemd-network/test-dhcp-client.c',
#           'src/libsystemd-network/dhcp-protocol.h',
#           'src/libsystemd-network/dhcp-internal.h',
#           'src/systemd/sd-dhcp-client.h'],
#          [libshared,
#           libelogind_network],
#          []],
# 
#         [['src/libelogind-network/test-dhcp-server.c'],
#          [libshared,
#           libelogind_network],
#          []],
# 
#         [['src/libsystemd-network/test-ipv4ll.c',
#           'src/libsystemd-network/arp-util.h',
#           'src/systemd/sd-ipv4ll.h'],
#          [libshared,
#           libelogind_network],
#          []],
# 
#         [['src/libelogind-network/test-ipv4ll-manual.c',
#           'src/systemd/sd-ipv4ll.h'],
#          [libshared,
#           libelogind_network],
#          [],
#          '', 'manual'],
# 
#         [['src/libelogind-network/test-acd.c',
#           'src/systemd/sd-ipv4acd.h'],
#          [libshared,
#           libelogind_network],
#          [],
#          '', 'manual'],
# 
#         [['src/libsystemd-network/test-ndisc-rs.c',
#           'src/libsystemd-network/dhcp-identifier.h',
#           'src/libsystemd-network/dhcp-identifier.c',
#           'src/libsystemd-network/icmp6-util.h',
#           'src/systemd/sd-dhcp6-client.h',
#           'src/systemd/sd-ndisc.h'],
#          [libshared,
#           libelogind_network],
#          []],
# 
#         [['src/libsystemd-network/test-ndisc-ra.c',
#           'src/libsystemd-network/icmp6-util.h',
#           'src/systemd/sd-ndisc.h'],
#          [libshared,
#           libelogind_network],
#          []],
# 
#         [['src/libsystemd-network/test-dhcp6-client.c',
#           'src/libsystemd-network/dhcp-identifier.h',
#           'src/libsystemd-network/dhcp-identifier.c',
#           'src/libsystemd-network/dhcp6-internal.h',
#           'src/systemd/sd-dhcp6-client.h'],
#          [libshared,
#           libelogind_network],
#          []],
# 
#         [['src/libelogind-network/test-lldp.c'],
#          [libshared,
#           libelogind_network],
#          []],
# ]
#endif // 0

############################################################

tests += [
        [['src/login/test-login-shared.c'],
         [],
         []],

        [['src/login/test-inhibit.c'],
         [],
         [],
         '', 'manual'],

        [['src/login/test-login-tables.c'],
         [liblogind_core,
          libshared],
#if 0 /// elogind does not include udev implicitly
#          [threads]],
#else
         [threads,libudev]],
#endif // 0
]
