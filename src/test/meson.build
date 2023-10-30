# SPDX-License-Identifier: LGPL-2.1-or-later

awkscript = 'test-hashmap-ordered.awk'
test_hashmap_ordered_c = custom_target(
        'test-hashmap-ordered.c',
        input : [awkscript, 'test-hashmap-plain.c'],
        output : 'test-hashmap-ordered.c',
        command : [awk, '-f', '@INPUT0@', '@INPUT1@'],
        capture : true,
        build_by_default : want_tests != 'false')

path = run_command(sh, '-c', 'echo "$PATH"', check: true).stdout().strip()
test_env = environment()
#if 0 /// UNNEEDED in elogind
# test_env.set('SYSTEMD_LANGUAGE_FALLBACK_MAP', language_fallback_map)
#endif // 0
test_env.set('PATH', project_build_root + ':' + path)
test_env.set('PROJECT_BUILD_ROOT', project_build_root)

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
        [files('test-argv-util.c')],

#if 0 /// UNNEEDED in elogind
#         [files('test-device-nodes.c')],
#
#         [files('test-ether-addr-util.c')],
#
#         [files('test-engine.c'),
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
#         [files('test-manager.c'),
#          [libcore,
#           libshared],
#          [],
#          core_includes],
#
#         [files('test-emergency-action.c'),
#          [libcore,
#           libshared],
#          [],
#          core_includes],
#
#         [files('test-chown-rec.c'),
#          [libcore,
#           libshared],
#          [],
#          core_includes],
#
#         [files('test-dlopen-so.c'),
#          [],
#          libp11kit_cflags],
#
#         [files('test-job-type.c'),
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
#         [files('test-ns.c'),
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
#         [files('test-loopback.c'),
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
#         [files('test-dns-domain.c')],
#
#         [files('test-boot-timestamps.c'),
#          [], [], [], 'ENABLE_EFI'],
#
#         [files('test-unit-file.c')],
#
#         [files('test-unit-name.c'),
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
#         [files('test-load-fragment.c'),
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
#         [files('test-serialize.c')],
#
#         [files('test-unit-serialize.c'),
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

        [files('test-utf8.c')],

#if 0 /// UNNEEDED by elogind
#         [files('test-kbd-util.c')],
#
#         [files('test-blockdev-util.c')],
#endif // 0

        [files('test-dev-setup.c')],

#if 0 /// UNNEEDED in elogind
#         [files('test-capability.c'),
#          [],
#          [libcap]],
#
#         [files('test-async.c'),
#          [], [], [], '', 'timeout=120'],
#endif // 0

        [files('test-locale-util.c')],

        [files('test-copy.c')],

        [files('test-recurse-dir.c')],

#if 0 /// UNNEEDED in elogind
#         [files('test-compress.c'),
#          [libshared,
#           libbasic_compress]],
#
#         [files('test-compress-benchmark.c'),
#          [libshared,
#           libbasic_compress],
#          [],
#          [], '', 'timeout=90'],
#
#         [files('test-data-fd-util.c')],
#endif // 0

        [files('test-static-destruct.c')],

#if 0 /// UNNEEDED in elogind
#         [files('test-sigbus.c')],
#
#         [files('test-condition.c')],
#
#         [files('test-fdset.c')],
#
#         [files('test-fstab-util.c')],
#endif // 0

        [files('test-random-util.c'),
         [],
         [libm],
         [], '', 'timeout=120'],

        [files('test-format-table.c')],

        [files('test-format-util.c')],

        [files('test-ratelimit.c')],

        [files('test-raw-clone.c')],

        [files('test-limits-util.c')],

        [files('test-logarithm.c')],

        [files('test-macro.c')],

        [files('test-math-util.c'),
         [],
         [libm]],

#if 0 /// UNNEEDED in elogind
#         [files('test-memory-util.c')],
#
#         [files('test-mkdir.c')],
#endif // 0

        [files('test-json.c'),
         [],
         [libm]],

#if 0 /// UNNEEDED in elogind
#         [files('test-modhex.c')],
#
#         [files('test-libmount.c'),
#          [],
#          [threads,
#           libmount]],
#
#endif // 0
        [files('test-mount-util.c')],

        [files('test-mountpoint-util.c')],

        [files('test-exec-util.c')],

#if 0 /// UNNEEDED in elogind
#         [files('test-execve.c')],
#endif // 0

        [files('test-hexdecoct.c')],

        [files('test-alloc-util.c')],

#if 0 /// UNNEEDED in elogind
#         [files('test-xattr-util.c')],
#
#         [files('test-io-util.c')],
#
#         [files('test-glob-util.c')],
#endif // 0

        [files('test-fs-util.c')],

#if 0 /// UNNEEDED in elogind
#         [files('test-install-file.c')],
#endif // 0

        [files('test-umask-util.c')],

        [files('test-creds.c')],

        [files('test-proc-cmdline.c')],

        [files('test-fd-util.c'),
         [],
         [libseccomp]],

#if 0 /// UNNEEDED in elogind
#         [files('test-web-util.c')],
#
#         [files('test-cpu-set-util.c')],
#endif // 0

        [files('test-stat-util.c')],

#if 0 /// UNNEEDED in elogind
#         [files('test-devnum-util.c')],
#
#         [files('test-os-util.c')],
#
#         [files('test-libcrypt-util.c'),
#          [], [libcrypt], [], '', 'timeout=120'],
#
#endif // 0
        [files('test-escape.c')],

#if 0 /// UNNEEDED in elogind
#         [files('test-exit-status.c')],
#
#         [files('test-specifier.c')],
#
#endif // 0
        [files('test-string-util.c')],

        [files('test-extract-word.c')],

        [files('test-parse-argument.c')],

#if 0 /// UNNEEDED in elogind
#         [files('test-parse-helpers.c')],
#endif // 0

        [files('test-parse-util.c'),
         [],
         [libm]],

#if 0 /// UNNEEDED in elogind
#         [files('test-sysctl-util.c')],
#
#         [files('test-import-util.c')],
#
#         [files('test-uid-alloc-range.c')],
#
#endif // 0
        [files('test-user-util.c')],

#if 0 /// UNNEEDED in elogind
#         [files('test-hostname-setup.c')],
#endif // 0

        [files('test-hostname-util.c')],

        [files('test-process-util.c')],

        [files('test-terminal-util.c')],

#if 0 /// UNNEEDED in elogind
#         [files('test-path-lookup.c')],
#
#endif // 0
        [files('test-pretty-print.c')],

#if 0 /// UNNEEDED in elogind
#         [files('test-uid-range.c')],
#
#         [files('test-cap-list.c') +
#          generated_gperf_headers,
#          [],
#          [libcap]],
#
#         [files('test-socket-util.c')],
#
#         [files('test-socket-netlink.c')],
#
#         [files('test-in-addr-util.c')],
#
#         [files('test-in-addr-prefix-util.c')],
#
#         [files('test-barrier.c')],
#
#         [files('test-tmpfiles.c')],
#
#         [files('test-namespace.c'),
#          [libcore,
#           libshared],
#          [threads,
#           libblkid],
#          core_includes],
#endif // 0

        [files('test-verbs.c')],

#if 0 /// UNNEEDED in elogind
#         [files('test-install-root.c')],
#
#         [files('test-acl-util.c'),
#          [], [], [], 'HAVE_ACL'],
#
#         [files('test-seccomp.c'),
#          [],
#          [libseccomp],
#          [], 'HAVE_SECCOMP'],
#
#         [files('test-rlimit-util.c')],
#
#         [files('test-ask-password-api.c'),
#          [], [], [], '', 'manual'],
#endif // 0

        [files('test-signal-util.c')],

#if 0 /// UNNEEDED in elogind
#         [files('test-loop-block.c'),
#          [libcore,
#           libshared],
#          [threads,
#           libblkid],
#          core_includes, '', '', [], false],
#endif // 0

        [files('test-selinux.c')],

        [files('test-sizeof.c'),
         [libbasic]],

#if 0 /// UNNEEDED in elogind
#         [files('test-bpf-devices.c'),
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
#         [files('test-bpf-firewall.c'),
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
#         [files('test-bpf-foreign-programs.c'),
#          [libcore,
#           libshared],
#          [],
#          core_includes],
#
#         [files('test-bpf-lsm.c'),
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
#         [files('test-watch-pid.c'),
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

        [files('test-hashmap.c',
               'test-hashmap-plain.c') +
         [test_hashmap_ordered_c],
         [], [], [], '', 'timeout=180'],

        [files('test-set.c')],

        [files('test-ordered-set.c')],

        [files('test-set-disable-mempool.c'),
         [],
         [threads]],

#if 0 /// UNNEEDED in elogind
#         [files('test-hash-funcs.c')],
#
#         [files('test-bitmap.c')],
#endif // 0

        [files('test-xml.c')],

        [files('test-list.c')],

        [files('test-procfs-util.c')],

        [files('test-unaligned.c')],

#if 0 /// UNNEEDED in elogind
#         [files('test-tables.c'),
#          [libcore,
#           libjournal_core,
#           libudevd_core,
#           libshared],
#          [threads,
#           libseccomp,
#           libmount,
#           libxz,
#           liblz4,
#           libblkid,
#           libselinux],
#          [core_includes, journal_includes, udev_includes]],
#endif // 0

        [files('test-prioq.c')],

#if 0 /// UNNEEDED in elogind
#         [files('test-fileio.c')],
#
#         [files('test-time-util.c')],
#
#         [files('test-clock.c')],
#
#endif // 0
        [files('test-tmpfile-util.c')],

#if 0 /// UNNEEDED in elogind
#         [files('test-architecture.c')],
#
#         [files('test-gpt.c')],
#endif // 0

        [files('test-log.c')],

        [files('test-ipcrm.c'),
         [], [], [], '', 'unsafe'],

#if 0 /// UNNEEDED in elogind
#         [files('test-btrfs.c'),
#          [], [], [], '', 'manual'],
#
#         [files('test-firewall-util.c')],
#
#         [files('test-net-naming-scheme.c')],
#
#         [files('test-netlink-manual.c'),
#          [],
#          [libkmod],
#          [], 'HAVE_KMOD', 'manual'],
#endif // 0

        [files('test-ellipsize.c')],

#if 0 /// UNNEEDED in elogind
#         [files('test-date.c')],
#
#         [files('test-sbat.c'),
#          [], [], [], 'HAVE_GNU_EFI', '',
#          ['-I@0@'.format(efi_config_h_dir)]],
#
#         [files('test-sleep.c')],
#
#         [files('test-tpm2.c')],
#
#         [files('test-replace-var.c')],
#
#         [files('test-calendarspec.c')],
#
#         [files('test-strip-tab-ansi.c')],
#
#         [files('test-coredump-util.c')],
#
#endif // 0
        [files('test-daemon.c')],

        [files('test-cgroup.c')],

#if 0 /// UNNEEDED in elogind
#         [files('test-cgroup-cpu.c'),
#          [libcore,
#           libshared],
#          [],
#          core_includes],
#
#         [files('test-cgroup-unit-default.c'),
#          [libcore,
#           libshared],
#          [],
#          core_includes],
#
#         [files('test-cgroup-mask.c'),
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
#         [files('test-varlink.c'),
#          [],
#          [threads]],
#
#         [files('test-cgroup-util.c')],
#
#         [files('test-cgroup-setup.c')],
#
#         [files('test-env-file.c')],
#
#         [files('test-env-util.c')],
#
#         [files('test-strbuf.c')],
#
#endif // 0
        [files('test-bootspec.c')],

        [files('test-strv.c')],

        [files('test-nulstr-util.c')],

        [files('test-path-util.c')],

        [files('test-rm-rf.c')],

        [files('test-chase-symlinks.c'),
         [], [], [], '', 'manual'],

#if 0 /// UNNEEDED in elogind
#         [files('test-path.c'),
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
#         [files('test-execute.c'),
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

        [files('test-siphash24.c')],

        [files('test-strxcpyx.c')],

#if 0 /// UNNEEDED in elogind
#         [files('test-install.c'),
#          [libcore,
#           libshared],
#          [],
#          core_includes, '', 'manual'],
#
#         [files('test-watchdog.c'),
#          [], [], [], '', 'unsafe'],
#
#         [files('test-sched-prio.c'),
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

        [files('test-conf-files.c')],

        [files('test-conf-parser.c')],

#if 0 /// UNNEEDED in elogind
#         [files('test-af-list.c') +
#          generated_gperf_headers],
#
#         [files('test-arphrd-util.c') +
#          generated_gperf_headers],
#
#         [files('test-errno-list.c') +
#          generated_gperf_headers],
#
#         [files('test-errno-util.c')],
#
#         [files('test-ip-protocol-list.c') +
#          shared_generated_gperf_headers],
#
#         [files('test-journal-importer.c')],
#
#         [files('test-utmp.c'),
#          [], [], [], 'ENABLE_UTMP'],
#
#         [files('test-udev.c'),
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
#         [files('test-udev-util.c')],
#endif // 0

        [files('test-id128.c')],

#if 0 /// UNNEEDED in elogind
#         [files('test-cryptolib.c'),
#          [libshared],
#          [lib_openssl_or_gcrypt],
#          [], 'HAVE_OPENSSL_OR_GCRYPT'],
#
#         [files('test-nss-hosts.c',
#                'nss-test-util.c'),
#          [],
#          [libdl],
#          [], 'ENABLE_NSS', 'timeout=120'],
#
#         [files('test-nss-users.c',
#                'nss-test-util.c'),
#          [],
#          [libdl],
#          [], 'ENABLE_NSS'],
#
#endif // 0
        [files('test-bus-util.c')],

        [files('test-percent-util.c')],

#if 0 /// UNNEEDED by elogind
#         [files('test-sd-hwdb.c')],
#endif // 0

        [files('test-sd-path.c')],

#if 0 /// UNNEEDED by elogind
#         [files('test-local-addresses.c')],
#
#         [files('test-psi-util.c')],
#
#         [files('test-qrcode-util.c'),
#          [],
#          [libdl]],
#
#         [files('test-nscd-flush.c'),
#          [], [], [], 'ENABLE_NSCD', 'manual'],
#endif // 0

        [files('test-hmac.c')],

        [files('test-sha256.c')],

        [files('test-open-file.c')],
]

############################################################

# define some tests here, because the link_with deps were not defined earlier

tests += [
        [files('../libelogind/sd-bus/test-bus-error.c'),
         [libshared_static,
          libelogind_static]],

        [files('../libelogind/sd-device/test-sd-device-thread.c'),
         [libelogind],
         [threads]],

#if 0 /// elogind does not ship libudev itself
#         [files('../libudev/test-udev-device-thread.c'),
#          [libudev],
#          [threads]],
#endif // 0
]

#if 0 /// UNNEEDED by elogind
# tests += [
#         [files('test-socket-bind.c'),
#          [libcore,
#          libshared],
#          [libdl],
#          core_includes,
#          'BPF_FRAMEWORK'],
# ]
#endif // 0
