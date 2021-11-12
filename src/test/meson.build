# SPDX-License-Identifier: LGPL-2.1-or-later

awkscript = 'test-hashmap-ordered.awk'
test_hashmap_ordered_c = custom_target(
        'test-hashmap-ordered.c',
        input : [awkscript, 'test-hashmap-plain.c'],
        output : 'test-hashmap-ordered.c',
        command : [awk, '-f', '@INPUT0@', '@INPUT1@'],
        capture : true,
        build_by_default : want_tests != 'false')

test_include_dir = include_directories('.')

path = run_command(sh, '-c', 'echo "$PATH"').stdout().strip()
test_env = environment()
#if 0 /// UNNEEDED in elogind
# test_env.set('SYSTEMD_LANGUAGE_FALLBACK_MAP', language_fallback_map)
#endif // 0
test_env.set('PATH', project_build_root + ':' + path)

############################################################

generate_sym_test_py = find_program('generate-sym-test.py')

test_libelogind_sym_c = custom_target(
        'test-libelogind-sym.c',
        input : [libelogind_sym_path] + systemd_headers,
        output : 'test-libelogind-sym.c',
        command : [generate_sym_test_py, libelogind_sym_path] + systemd_headers,
        capture : true,
        build_by_default : want_tests != 'false')

#if 0 /// UNNEEDED in elogind
# test_libudev_sym_c = custom_target(
#         'test-libudev-sym.c',
#         input : [libudev_sym_path, libudev_h_path],
#         output : 'test-libudev-sym.c',
#         command : [generate_sym_test_py, '@INPUT0@', '@INPUT1@'],
#         capture : true,
#         build_by_default : want_tests != 'false')
#endif // 0

test_dlopen_c = files('test-dlopen.c')

############################################################

tests += [
#if 0 /// UNNEEDED in elogind
#         [['src/test/test-device-nodes.c']],
#
#         [['src/test/test-ether-addr-util.c']],
#
#         [['src/test/test-engine.c'],
#          [libcore,
#           libshared],
#          [threads,
#           librt,
#           libseccomp,
#           libselinux,
#           libmount,
#           libblkid],
#          core_includes],
#
#         [['src/test/test-emergency-action.c'],
#          [libcore,
#           libshared],
#          [],
#          core_includes],
#
#         [['src/test/test-chown-rec.c'],
#          [libcore,
#           libshared],
#          [],
#          core_includes],
#
#         [['src/test/test-dlopen-so.c']],
#
#         [['src/test/test-job-type.c'],
#          [libcore,
#           libshared],
#          [threads,
#           librt,
#           libseccomp,
#           libselinux,
#           libmount,
#           libblkid],
#          core_includes],
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
#          core_includes, '', 'manual'],
#
#         [['src/test/test-loopback.c'],
#          [libcore,
#           libshared],
#          [threads,
#           librt,
#           libseccomp,
#           libselinux,
#           libmount,
#           libblkid],
#          core_includes],
#
#         [['src/test/test-dns-domain.c']],
#
#         [['src/test/test-boot-timestamps.c'],
#          [], [], [], 'ENABLE_EFI'],
#
#         [['src/test/test-unit-file.c']],
#
#         [['src/test/test-unit-name.c'],
#          [libcore,
#           libshared],
#          [threads,
#           librt,
#           libseccomp,
#           libselinux,
#           libmount,
#           libblkid],
#          core_includes],
#
#         [['src/test/test-load-fragment.c'],
#          [libcore,
#           libshared],
#          [threads,
#           librt,
#           libseccomp,
#           libselinux,
#           libmount,
#           libblkid],
#          core_includes],
#
#         [['src/test/test-serialize.c']],
#
#         [['src/test/test-unit-serialize.c'],
#          [libcore,
#           libshared],
#          [threads,
#           librt,
#           libseccomp,
#           libselinux,
#           libmount,
#           libblkid],
#          core_includes],
#endif // 0

        [['src/test/test-utf8.c']],

/// elogind empty mask removed (UNNEEDED in elogind)
        [['src/test/test-kbd-util.c']],

        [['src/test/test-blockdev-util.c']],

        [['src/test/test-dev-setup.c']],

#if 0 /// UNNEEDED in elogind
#         [['src/test/test-capability.c'],
#          [],
#          [libcap]],
#
#         [['src/test/test-async.c'],
#          [], [], [], '', 'timeout=120'],
#endif // 0

        [['src/test/test-locale-util.c']],

        [['src/test/test-copy.c']],

/// elogind empty mask removed (UNNEEDED in elogind)
        [['src/test/test-recurse-dir.c']],

        [['src/test/test-data-fd-util.c']],

        [['src/test/test-static-destruct.c']],

#if 0 /// UNNEEDED in elogind
#         [['src/test/test-sigbus.c']],
#
#         [['src/test/test-condition.c']],
#
#         [['src/test/test-fdset.c']],
#
#         [['src/test/test-fstab-util.c']],
#endif // 0

        [['src/test/test-random-util.c'],
         [],
         [libm],
         [], '', 'timeout=120'],

        [['src/test/test-format-table.c']],

        [['src/test/test-format-util.c']],

#if 0 /// UNNEEDED in elogind
#         [['src/test/test-ratelimit.c']],
#endif // 0

        [['src/test/test-util.c']],

        [['src/test/test-macro.c']],

        [['src/test/test-json.c']],

#if 0 /// UNNEEDED in elogind
#         [['src/test/test-modhex.c']],
#
#         [['src/test/test-libmount.c'],
#          [],
#          [threads,
#           libmount]],
#
#endif // 0
        [['src/test/test-mount-util.c']],

        [['src/test/test-mountpoint-util.c']],

        [['src/test/test-exec-util.c']],

        [['src/test/test-hexdecoct.c']],

        [['src/test/test-alloc-util.c']],

#if 0 /// UNNEEDED in elogind
#         [['src/test/test-xattr-util.c']],
#endif // 0

        [['src/test/test-io-util.c']],

#if 0 /// UNNEEDED in elogind
#         [['src/test/test-glob-util.c']],
#endif // 0

        [['src/test/test-fs-util.c']],

        [['src/test/test-install-file.c']],

        [['src/test/test-umask-util.c']],

        [['src/test/test-proc-cmdline.c']],

        [['src/test/test-fd-util.c'],
         [],
         [libseccomp]],

#if 0 /// UNNEEDED in elogind
#         [['src/test/test-web-util.c']],
#
#         [['src/test/test-cpu-set-util.c']],
#endif // 0

        [['src/test/test-stat-util.c']],

#if 0 /// UNNEEDED in elogind
#         [['src/test/test-os-util.c']],
#
#         [['src/test/test-libcrypt-util.c'],
#          [], [libcrypt], [], '', 'timeout=120'],
#
#endif // 0
        [['src/test/test-escape.c']],


#if 0 /// UNNEEDED in elogind
#         [['src/test/test-exit-status.c']],
#
#         [['src/test/test-specifier.c']],
#endif // 0

        [['src/test/test-string-util.c']],

        [['src/test/test-extract-word.c']],

        [['src/test/test-parse-argument.c']],

/// elogind empty mask removed (UNNEEDED in elogind)
        [['src/test/test-parse-socket-bind-item.c']],

        [['src/test/test-parse-util.c']],

#if 0 /// UNNEEDED in elogind
#         [['src/test/test-sysctl-util.c']],
#endif // 0

        [['src/test/test-import-util.c']],

        [['src/test/test-uid-alloc-range.c']],

        [['src/test/test-user-util.c']],

#if 0 /// UNNEEDED in elogind
#         [['src/test/test-hostname-setup.c']],
#endif // 0

        [['src/test/test-hostname-util.c']],

        [['src/test/test-process-util.c']],

#if 0 /// UNNEEDED in elogind
#         [['src/test/test-terminal-util.c']],
#
#         [['src/test/test-path-lookup.c']],
#endif // 0

        [['src/test/test-pretty-print.c']],

#if 0 /// UNNEEDED in elogind
#         [['src/test/test-uid-range.c']],
#
#         [['src/test/test-cap-list.c',
#           generated_gperf_headers],
#          [],
#          [libcap]],
#
#         [['src/test/test-socket-util.c']],
#
#         [['src/test/test-socket-netlink.c']],
#
#         [['src/test/test-in-addr-util.c']],
#
#         [['src/test/test-in-addr-prefix-util.c']],
#
#         [['src/test/test-barrier.c']],
#
#         [['src/test/test-tmpfiles.c']],
#
#         [['src/test/test-namespace.c'],
#          [libcore,
#           libshared],
#          [threads,
#           libblkid],
#          core_includes],
#endif // 0

        [['src/test/test-verbs.c']],

#if 0 /// UNNEEDED in elogind
#         [['src/test/test-install-root.c']],
#
#         [['src/test/test-acl-util.c'],
#          [], [], [], 'HAVE_ACL'],
#
#         [['src/test/test-seccomp.c'],
#          [],
#          [libseccomp],
#          [], 'HAVE_SECCOMP'],
#
#         [['src/test/test-rlimit-util.c']],
#
#         [['src/test/test-ask-password-api.c'],
#          [], [], [], '', 'manual'],
#endif // 0

        [['src/test/test-signal-util.c']],

#if 0 /// UNNEEDED in elogind
#         [['src/test/test-loop-block.c'],
#          [libcore,
#           libshared],
#          [threads,
#           libblkid],
#          core_includes, '', '', [], false],
#endif // 0

        [['src/test/test-selinux.c']],

        [['src/test/test-sizeof.c'],
         [libbasic]],

#if 0 /// UNNEEDED in elogind
#         [['src/test/test-bpf-devices.c'],
#          [libcore,
#           libshared],
#          [libmount,
#           threads,
#           librt,
#           libseccomp,
#           libselinux,
#           libblkid],
#          core_includes],
#
#         [['src/test/test-bpf-firewall.c'],
#          [libcore,
#           libshared],
#          [libmount,
#           threads,
#           librt,
#           libseccomp,
#           libselinux,
#           libblkid],
#          core_includes],
#
#         [['src/test/test-bpf-foreign-programs.c'],
#          [libcore,
#           libshared],
#          [],
#          core_includes],
#
#         [['src/test/test-bpf-lsm.c'],
#          [libcore,
#           libshared],
#          [libmount,
#           threads,
#           librt,
#           libseccomp,
#           libselinux,
#           libblkid],
#          core_includes],
#
#         [['src/test/test-watch-pid.c'],
#          [libcore,
#           libshared],
#          [libmount,
#           threads,
#           librt,
#           libseccomp,
#           libselinux,
#           libblkid],
#          core_includes],
#endif // 0

        [['src/test/test-hashmap.c',
          'src/test/test-hashmap-plain.c',
          test_hashmap_ordered_c],
         [], [], [], '', 'timeout=180'],

        [['src/test/test-set.c'],
         [libbasic]],

#if 0 /// UNNEEDED in elogind
#         [['src/test/test-ordered-set.c']],
#endif // 0

        [['src/test/test-set-disable-mempool.c'],
         [],
         [threads]],

#if 0 /// UNNEEDED in elogind
#         [['src/test/test-hash-funcs.c']],
#
#         [['src/test/test-bitmap.c']],
#
#         [['src/test/test-xml.c']],
#endif // 0

        [['src/test/test-list.c']],

        [['src/test/test-procfs-util.c']],

        [['src/test/test-unaligned.c']],

#if 0 /// UNNEEDED in elogind
#         [['src/test/test-tables.c'],
#          [libcore,
#           libjournal_core,
#           libudevd_core,
#           libshared],
#          [threads,
#           libseccomp,
#           libmount,
#           libxz,
#           liblz4,
#           libblkid],
#          [core_includes, journal_includes, udev_includes]],
#endif // 0

        [['src/test/test-prioq.c']],

#if 0 /// UNNEEDED in elogind
#         [['src/test/test-fileio.c']],
#
#         [['src/test/test-time-util.c']],
#
#         [['src/test/test-clock.c']],
#
#endif // 0
        [['src/test/test-tmpfile-util.c']],

#if 0 /// UNNEEDED in elogind
#         [['src/test/test-architecture.c']],
#endif // 0

        [['src/test/test-log.c']],

        [['src/test/test-ipcrm.c'],
         [], [], [], '', 'unsafe'],

#if 0 /// UNNEEDED in elogind
#         [['src/test/test-btrfs.c'],
#          [], [], [], '', 'manual'],
#
#         [['src/test/test-firewall-util.c']],
#
#         [['src/test/test-net-naming-scheme.c']],
#
#         [['src/test/test-netlink-manual.c'],
#          [],
#          [libkmod],
#          [], 'HAVE_KMOD', 'manual'],
#endif // 0

        [['src/test/test-ellipsize.c']],

#if 0 /// UNNEEDED in elogind
#         [['src/test/test-date.c']],
#
#         [['src/test/test-sleep.c']],
#
#         [['src/test/test-tpm2.c']],
#
#         [['src/test/test-replace-var.c']],
#
#         [['src/test/test-calendarspec.c']],
#endif // 0

        [['src/test/test-strip-tab-ansi.c']],

#if 0 /// UNNEEDED in elogind
#         [['src/test/test-coredump-util.c']],
#
#endif // 0
        [['src/test/test-daemon.c']],

        [['src/test/test-cgroup.c']],

#if 0 /// UNNEEDED in elogind
#         [['src/test/test-cgroup-cpu.c'],
#          [libcore,
#           libshared],
#          [],
#          core_includes],
#
#         [['src/test/test-cgroup-unit-default.c'],
#          [libcore,
#           libshared],
#          [],
#          core_includes],
#
#         [['src/test/test-cgroup-mask.c'],
#          [libcore,
#           libshared],
#          [threads,
#           librt,
#           libseccomp,
#           libselinux,
#           libmount,
#           libblkid],
#          core_includes],
#
#         [['src/test/test-varlink.c'],
#          [],
#          [threads]],
#
#         [['src/test/test-cgroup-util.c']],
#
#         [['src/test/test-cgroup-setup.c']],
#
#         [['src/test/test-env-file.c']],
#
#         [['src/test/test-env-util.c']],
#
#         [['src/test/test-strbuf.c']],
#
#endif // 0
        [['src/test/test-strv.c']],

        [['src/test/test-path-util.c']],

        [['src/test/test-rm-rf.c']],

        [['src/test/test-chase-symlinks.c'],
         [], [], [], '', 'manual'],

#if 0 /// UNNEEDED in elogind
#         [['src/test/test-path.c'],
#          [libcore,
#           libshared],
#          [threads,
#           librt,
#           libseccomp,
#           libselinux,
#           libmount,
#           libblkid],
#          core_includes, '', 'timeout=120'],
#
#         [['src/test/test-execute.c'],
#          [libcore,
#           libshared],
#          [threads,
#           librt,
#           libseccomp,
#           libselinux,
#           libmount,
#           libblkid],
#          core_includes, '', 'timeout=360'],
#endif // 0

        [['src/test/test-siphash24.c']],

        [['src/test/test-strxcpyx.c']],

#if 0 /// UNNEEDED in elogind
#         [['src/test/test-install.c'],
#          [libcore,
#           libshared],
#          [],
#          core_includes, '', 'manual'],
#
#         [['src/test/test-watchdog.c']],
#
#         [['src/test/test-sched-prio.c'],
#          [libcore,
#           libshared],
#          [threads,
#           librt,
#           libseccomp,
#           libselinux,
#           libmount,
#           libblkid],
#          core_includes],
#endif // 0

        [['src/test/test-conf-files.c']],

        [['src/test/test-conf-parser.c']],

        [['src/test/test-af-list.c',
          generated_gperf_headers]],

#if 0 /// UNNEEDED in elogind
#         [['src/test/test-arphrd-util.c',
#           generated_gperf_headers]],
#
#         [['src/test/test-ip-protocol-list.c',
#           shared_generated_gperf_headers]],
#
#         [['src/test/test-journal-importer.c']],
#
#         [['src/test/test-udev.c'],
#          [libudevd_core,
#           libshared],
#          [threads,
#           librt,
#           libblkid,
#           libkmod,
#           libacl,
#           libselinux],
#          udev_includes, '', 'manual'],
#
#         [['src/test/test-udev-util.c']],
#endif // 0

        [['src/test/test-id128.c']],


#if 0 /// UNNEEDED in elogind
#         [['src/test/test-gcrypt-util.c'],
#          [], [], [], 'HAVE_GCRYPT'],
#
#         [['src/test/test-nss-hosts.c',
#           'src/test/nss-test-util.c',
#           'src/test/nss-test-util.h'],
#          [],
#          [libdl],
#          [], 'ENABLE_NSS', 'manual'],
#
#         [['src/test/test-nss-users.c',
#           'src/test/nss-test-util.c',
#           'src/test/nss-test-util.h'],
#          [],
#          [libdl],
#          [], 'ENABLE_NSS', 'manual'],
#
#endif // 0
        [['src/test/test-bus-util.c']],

        [['src/test/test-percent-util.c']],

#if 0 /// UNNEEDED by elogind
#         [['src/test/test-sd-hwdb.c']],
#endif // 0

        [['src/test/test-sd-path.c']],


#if 0 /// UNNEEDED by elogind
#         [['src/test/test-local-addresses.c']],
#
#         [['src/test/test-psi-util.c']],
#
#         [['src/test/test-qrcode-util.c'],
#          [],
#          [libdl]],
#
#         [['src/test/test-nscd-flush.c'],
#          [], [], [], 'ENABLE_NSCD', 'manual'],
#endif // 0

        [['src/test/test-hmac.c']],
]

############################################################

# define some tests here, because the link_with deps were not defined earlier

tests += [
        [['src/libelogind/sd-bus/test-bus-error.c'],
         [libshared_static,
          libelogind_static]],

        [['src/libelogind/sd-device/test-sd-device-thread.c'],
         [libelogind],
         [threads]],

#if 0 /// elogind does not ship libudev itself
#         [['src/libudev/test-udev-device-thread.c'],
#          [libudev],
#          [threads]],
#endif // 0
]

#if 0 /// UNNEEDED by elogind
# tests += [
#          [['src/test/test-socket-bind.c'],
#           [libcore,
#           libshared],
#          [libdl],
#          core_includes,
#          'BPF_FRAMEWORK'],
# ]
#endif // 0
