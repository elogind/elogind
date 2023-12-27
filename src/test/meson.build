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

#if 0 /// UNNEEDED in elogind
# if efi_addon != ''
#         test_env.set('EFI_ADDON', efi_addon)
# endif
#endif // 0

############################################################

generate_sym_test_py = find_program('generate-sym-test.py')

test_libelogind_sym_c = custom_target(
        'test-libelogind-sym.c',
        output : 'test-libelogind-sym.c',
        input : [libelogind_sym_path] + systemd_headers + libelogind_sources,
        command : [generate_sym_test_py, libelogind_sym_path, libelogind_dir_path] + systemd_headers,
        capture : true,
        build_by_default : want_tests != 'false')

#if 0 /// UNNEEDED in elogind
# test_libudev_sym_c = custom_target(
#         'test-libudev-sym.c',
#         input : [libudev_sym_path, libudev_h_path] + libudev_sources,
#         output : 'test-libudev-sym.c',
#         command : [generate_sym_test_py, libudev_sym_path, libudev_dir_path, libudev_h_path],
#         capture : true,
#         build_by_default : want_tests != 'false')
#endif // 0

test_dlopen_c = files('test-dlopen.c')

############################################################

simple_tests += files(
        'test-alloc-util.c',
#if 0 /// UNNEEDED by elogind
#         'test-architecture.c',
#endif // 0
        'test-argv-util.c',
#if 0 /// UNNEEDED by elogind
#         'test-barrier.c',
#         'test-bitfield.c',
#         'test-bitmap.c',
#         'test-blockdev-util.c',
#endif // 0
        'test-bootspec.c',
        'test-bus-util.c',
#if 0 /// UNNEEDED by elogind
#         'test-calendarspec.c',
#endif // 0
        'test-cgroup-setup.c',
        'test-cgroup-util.c',
        'test-cgroup.c',
        'test-chase.c',
#if 0 /// UNNEEDED by elogind
#         'test-clock.c',
#         'test-compare-operator.c',
#         'test-condition.c',
#endif // 0
        'test-conf-files.c',
        'test-conf-parser.c',
        'test-copy.c',
#if 0 /// UNNEEDED by elogind
#         'test-coredump-util.c',
#         'test-cpu-set-util.c',
#         'test-creds.c',
#endif // 0
        'test-daemon.c',
#if 0 /// UNNEEDED by elogind
#         'test-data-fd-util.c',
#         'test-date.c',
#         'test-dev-setup.c',
#         'test-device-nodes.c',
#endif // 0
        'test-devnum-util.c',
#if 0 /// UNNEEDED by elogind
#         'test-dns-domain.c',
#endif // 0
        'test-ellipsize.c',
#if 0 /// UNNEEDED by elogind
#         'test-env-file.c',
#         'test-env-util.c',
#         'test-errno-util.c',
#endif // 0
        'test-escape.c',
#if 0 /// UNNEEDED by elogind
#         'test-ether-addr-util.c',
#endif // 0
        'test-exec-util.c',
#if 0 /// UNNEEDED by elogind
#         'test-execve.c',
#         'test-exit-status.c',
#endif // 0
        'test-extract-word.c',
#if 0 /// UNNEEDED by elogind
#         'test-fdset.c',
#         'test-fileio.c',
#         'test-firewall-util.c',
#endif // 0
        'test-format-table.c',
        'test-format-util.c',
        'test-fs-util.c',
#if 0 /// UNNEEDED by elogind
#         'test-fstab-util.c',
#         'test-glob-util.c',
#         'test-gpt.c',
#endif // 0
        'test-gunicode.c',
#if 0 /// UNNEEDED by elogind
#         'test-hash-funcs.c',
#endif // 0
        'test-hexdecoct.c',
        'test-hmac.c',
#if 0 /// UNNEEDED by elogind
#         'test-hostname-setup.c',
#endif // 0
        'test-hostname-util.c',
        'test-id128.c',
#if 0 /// UNNEEDED by elogind
#         'test-image-policy.c',
#         'test-import-util.c',
#         'test-in-addr-prefix-util.c',
#         'test-in-addr-util.c',
#         'test-install-file.c',
#         'test-install-root.c',
#         'test-io-util.c',
#         'test-journal-importer.c',
#         'test-kbd-util.c',
#endif // 0
        'test-limits-util.c',
        'test-list.c',
#if 0 /// UNNEEDED by elogind
#         'test-local-addresses.c',
#endif // 0
        'test-locale-util.c',
#if 0 /// UNNEEDED by elogind
#         'test-lock-util.c',
#endif // 0
        'test-log.c',
        'test-logarithm.c',
        'test-macro.c',
#if 0 /// UNNEEDED by elogind
#         'test-memfd-util.c',
#endif // 0
        'test-memory-util.c',
        'test-mempool.c',
        'test-memstream-util.c',
#if 0 /// UNNEEDED by elogind
#         'test-mkdir.c',
#         'test-modhex.c',
#endif // 0
        'test-mountpoint-util.c',
#if 0 /// UNNEEDED by elogind
#         'test-net-naming-scheme.c',
#endif // 0
        'test-nulstr-util.c',
#if 0 /// UNNEEDED by elogind
#         'test-open-file.c',
#endif // 0
        'test-ordered-set.c',
#if 0 /// UNNEEDED by elogind
#         'test-os-util.c',
#endif // 0
        'test-parse-argument.c',
#if 0 /// UNNEEDED by elogind
#         'test-parse-helpers.c',
#         'test-path-lookup.c',
#endif // 0
        'test-path-util.c',
        'test-percent-util.c',
        'test-pretty-print.c',
        'test-prioq.c',
        'test-proc-cmdline.c',
        'test-procfs-util.c',
#if 0 /// UNNEEDED by elogind
#         'test-psi-util.c',
#endif // 0
        'test-ratelimit.c',
        'test-raw-clone.c',
        'test-recurse-dir.c',
#if 0 /// UNNEEDED by elogind
#         'test-replace-var.c',
#         'test-rlimit-util.c',
#endif // 0
        'test-rm-rf.c',
#if 0 /// UNNEEDED by elogind
#         'test-sd-hwdb.c',
#endif // 0
        'test-sd-path.c',
#if 0 /// UNNEEDED by elogind
#         'test-secure-bits.c',
#endif // 0
        'test-selinux.c',
#if 0 /// UNNEEDED by elogind
#         'test-serialize.c',
#endif // 0
        'test-set.c',
        'test-sha256.c',
#if 0 /// UNNEEDED by elogind
#         'test-sigbus.c',
#endif // 0
        'test-signal-util.c',
        'test-siphash24.c',
#if 0 /// UNNEEDED by elogind
#         'test-sleep.c',
#         'test-socket-netlink.c',
#         'test-socket-util.c',
#         'test-specifier.c',
#endif // 0
        'test-stat-util.c',
        'test-static-destruct.c',
#if 0 /// UNNEEDED by elogind
#         'test-strbuf.c',
#endif // 0
        'test-string-util.c',
#if 0 /// UNNEEDED by elogind
#         'test-strip-tab-ansi.c',
#endif // 0
        'test-strv.c',
        'test-strxcpyx.c',
#if 0 /// UNNEEDED by elogind
#         'test-sysctl-util.c',
#endif // 0
        'test-terminal-util.c',
        'test-tmpfile-util.c',
#if 0 /// UNNEEDED by elogind
#         'test-tpm2.c',
#         'test-udev-util.c',
#endif // 0
        'test-uid-alloc-range.c',
        'test-uid-range.c',
        'test-umask-util.c',
        'test-unaligned.c',
#if 0 /// UNNEEDED by elogind
#         'test-unit-file.c',
#endif // 0
        'test-user-util.c',
        'test-utf8.c',
        'test-verbs.c',
#if 0 /// UNNEEDED by elogind
#         'test-web-util.c',
#endif // 0
        'test-xattr-util.c',
        'test-xml.c',
)

############################################################

common_test_dependencies = [
        libblkid,
        libmount,
        librt,
        libseccomp,
        libselinux,
        threads,
]

tests += [
#if 0 /// the functions elogind uses are not covered in these tests
#         {
#                 'sources' : files('test-acl-util.c'),
#                 'condition' : 'HAVE_ACL',
#         },
#         {
#                 'sources' : files('test-af-list.c') +
#                             generated_gperf_headers,
#         },
#endif // 0
#if 0 /// UNNEEDED by elogind
#         {
#                 'sources' : files('test-arphrd-util.c') +
#                             generated_gperf_headers,
#         },
#         {
#                 'sources' : files('test-ask-password-api.c'),
#                 'type' : 'manual',
#         },
#endif // 0
        {
                'sources' : files('test-async.c'),
                'timeout' : 120,
        },
#if 0 /// UNNEEDED by elogind
#         {
#                 'sources' : files('test-boot-timestamps.c'),
#                 'condition' : 'ENABLE_EFI',
#         },
#endif // 0
        {
                'sources' : files('test-btrfs.c'),
                'type' : 'manual',
        },
        {
                'sources' : files('test-cap-list.c') +
                            generated_gperf_headers,
                'dependencies' : libcap,
        },
#if 0 /// UNNEEDED by elogind
#         {
#                 'sources' : files('test-capability.c'),
#                 'dependencies' : libcap,
#         },
#         {
#                 'sources' : files('test-chase-manual.c'),
#                 'type' : 'manual',
#         },
#         {
#                 'sources' : files('test-compress-benchmark.c'),
#                 'link_with' : [
#                         libbasic_compress,
#                         libshared,
#                 ],
#                 'timeout' : 90,
#         },
#         {
#                 'sources' : files('test-compress.c'),
#                 'link_with' : [
#                         libbasic_compress,
#                         libshared,
#                 ],
#         },
#         {
#                 'sources' : files('test-cryptolib.c'),
#                 'link_with' : libshared,
#                 'dependencies' : lib_openssl_or_gcrypt,
#                 'condition' : 'HAVE_OPENSSL_OR_GCRYPT',
#         },
#         {
#                 'sources' : files('test-dlopen-so.c'),
#                 'dependencies' : libp11kit_cflags
#         },
#endif // 0
        {
                'sources' : files('test-errno-list.c') +
                            generated_gperf_headers,
        },
        {
                'sources' : files('test-fd-util.c'),
                'dependencies' : libseccomp,
        },
        {
                'sources' : files(
                                  'test-hashmap.c',
                                  'test-hashmap-plain.c',
                            ) + [
                                   test_hashmap_ordered_c,
                            ],
                'timeout' : 180,
        },
#if 0 /// UNNEEDED by elogind
#         {
#                 'sources' : files('test-ip-protocol-list.c') +
#                             shared_generated_gperf_headers,
#         },
#endif // 0
        {
                'sources' : files('test-ipcrm.c'),
                'type' : 'unsafe',
        },
        {
                'sources' : files('test-json.c'),
                'dependencies' : libm,
        },
        {
                'sources' : files('test-libcrypt-util.c'),
                'dependencies' : libcrypt,
                'timeout' : 120,
        },
        {
                'sources' : files('test-libmount.c'),
                'dependencies' : [
                        libmount,
                        threads,
                ],
        },
#if 0 /// UNNEEDED by elogind
#         {
#                 'sources' : files('test-loopback.c'),
#                 'dependencies' : common_test_dependencies,
#         },
#endif // 0
        {
                'sources' : files('test-math-util.c'),
                'dependencies' : libm,
        },
        {
                'sources' : files('test-mempress.c'),
                'dependencies' : threads,
        },
        {
                'sources' : files('test-mount-util.c'),
                'dependencies' : libmount,
        },
#if 0 /// UNNEEDED by elogind
#         {
#                 'sources' : files('test-netlink-manual.c'),
#                 'dependencies' : libkmod,
#                 'condition' : 'HAVE_KMOD',
#                 'type' : 'manual',
#         },
#         {
#                 'sources' : files('test-nscd-flush.c'),
#                 'condition' : 'ENABLE_NSCD',
#                 'type' : 'manual',
#         },
#         {
#                 'sources' : files(
#                         'test-nss-hosts.c',
#                         'nss-test-util.c',
#                 ),
#                 'dependencies' : libdl,
#                 'condition' : 'ENABLE_NSS',
#                 'timeout' : 120,
#         },
#         {
#                 'sources' : files(
#                         'test-nss-users.c',
#                         'nss-test-util.c',
#                 ),
#                 'dependencies' : libdl,
#                 'condition' : 'ENABLE_NSS',
#         },
#endif // 0
        {
                'sources' : files('test-parse-util.c'),
                'dependencies' : libm,
        },
        {
                'sources' : files('test-process-util.c'),
                'dependencies' : threads,
        },
#if 0 /// UNNEEDED by elogind
#         {
#                 'sources' : files('test-qrcode-util.c'),
#                 'dependencies' : libdl,
#         },
#endif // 0
        {
                'sources' : files('test-random-util.c'),
                'dependencies' : libm,
                'timeout' : 120,
        },
#if 0 /// UNNEEDED by elogind
#         {
#                 'sources' : files('test-sbat.c'),
#                 'condition' : 'ENABLE_BOOTLOADER',
#                 'c_args' : '-I@0@'.format(efi_config_h_dir),
#         },
#         {
#                 'sources' : files('test-seccomp.c'),
#                 'dependencies' : libseccomp,
#                 'condition' : 'HAVE_SECCOMP',
#         },
#endif // 0
        {
                'sources' : files('test-set-disable-mempool.c'),
                'dependencies' : threads,
        },
        {
                'sources' : files('test-sizeof.c'),
                'link_with' : libbasic,
        },
#if 0 /// UNNEEDED by elogind
#         {
#                 'sources' : files('test-tables.c'),
#                 'link_with' : [
#                         libcore,
#                         libjournal_core,
#                         libshared,
#                         libudevd_core,
#                 ],
#                 'dependencies' : [
#                         libblkid,
#                         liblz4,
#                         libmount,
#                         libseccomp,
#                         libselinux,
#                         libxz,
#                         threads,
#                 ],
#                 'includes' : [
#                         core_includes,
#                         journal_includes,
#                         udev_includes,
#                 ],
#         },
#endif // 0
        {
                'sources' : files('test-time-util.c'),
                'timeout' : 120,
        },
#if 0 /// elogind does not provide libudev
#         {
#                 'sources' : files('udev-rule-runner.c'),
#                 'link_with' : [
#                         libshared,
#                         libudevd_core,
#                 ],
#                 'dependencies' : [
#                         libacl,
#                         libblkid,
#                         libkmod,
#                         librt,
#                         libselinux,
#                         threads,
#                 ],
#                 'includes' : udev_includes,
#                 'type' : 'manual',
#         },
#endif // 0
        {
                'sources' : files('test-utmp.c'),
                'condition' : 'ENABLE_UTMP',
        },
#if 0 /// elogind does not provide libudev
#         {
#                 'sources' : files('test-varlink.c'),
#                 'dependencies' : threads,
#         },
#         {
#                 'sources' : files('test-watchdog.c'),
#                 'type' : 'unsafe',
#         },
#endif // 0


#if 0 /// elogind is never PID 1
#         # Tests that link to libcore, i.e. tests for pid1 code.
#         {
#                 'sources' : files('test-bpf-devices.c'),
#                 'dependencies' : common_test_dependencies,
#                 'base' : test_core_base,
#         },
#         {
#                 'sources' : files('test-bpf-firewall.c'),
#                 'dependencies' : common_test_dependencies,
#                 'base' : test_core_base,
#         },
#         {
#                 'sources' : files('test-bpf-foreign-programs.c'),
#                 'base' : test_core_base,
#         },
#         {
#                 'sources' : files('test-bpf-lsm.c'),
#                 'dependencies' : common_test_dependencies,
#                 'base' : test_core_base,
#         },
#         {
#                 'sources' : files('test-cgroup-cpu.c'),
#                 'base' : test_core_base,
#         },
#         {
#                 'sources' : files('test-cgroup-mask.c'),
#                 'dependencies' : common_test_dependencies,
#                 'base' : test_core_base,
#         },
#         {
#                 'sources' : files('test-cgroup-unit-default.c'),
#                 'base' : test_core_base,
#         },
#         {
#                 'sources' : files('test-chown-rec.c'),
#                 'base' : test_core_base,
#         },
#         {
#                 'sources' : files('test-core-unit.c'),
#                 'dependencies' : common_test_dependencies,
#                 'base' : test_core_base,
#         },
#         {
#                 'sources' : files('test-emergency-action.c'),
#                 'base' : test_core_base,
#         },
#         {
#                 'sources' : files('test-engine.c'),
#                 'dependencies' : common_test_dependencies,
#                 'base' : test_core_base,
#         },
#         {
#                 'sources' : files('test-execute.c'),
#                 'dependencies' : common_test_dependencies,
#                 'base' : test_core_base,
#                 'timeout' : 360,
#         },
#         {
#                 'sources' : files('test-install.c'),
#                 'base' : test_core_base,
#                 'type' : 'manual',
#         },
#         {
#                 'sources' : files('test-job-type.c'),
#                 'dependencies' : common_test_dependencies,
#                 'base' : test_core_base,
#         },
#         {
#                 'sources' : files('test-load-fragment.c'),
#                 'dependencies' : common_test_dependencies,
#                 'base' : test_core_base,
#         },
#         {
#                 'sources' : files('test-loop-block.c'),
#                 'dependencies' : [threads, libblkid],
#                 'base' : test_core_base,
#                 'parallel' : false,
#         },
#         {
#                 'sources' : files('test-manager.c'),
#                 'base' : test_core_base,
#         },
#         {
#                 'sources' : files('test-namespace.c'),
#                 'dependencies' : [
#                         libblkid,
#                         threads,
#                 ],
#                 'base' : test_core_base,
#         },
#         {
#                 'sources' : files('test-ns.c'),
#                 'dependencies' : common_test_dependencies,
#                 'base' : test_core_base,
#                 'type' : 'manual',
#         },
#         {
#                 'sources' : files('test-path.c'),
#                 'dependencies' : common_test_dependencies,
#                 'base' : test_core_base,
#                 'timeout' : 120,
#         },
#         {
#                 'sources' : files('test-sched-prio.c'),
#                 'dependencies' : common_test_dependencies,
#                 'base' : test_core_base,
#         },
#         {
#                 'sources' : files('test-socket-bind.c'),
#                 'dependencies' : libdl,
#                 'condition' : 'BPF_FRAMEWORK',
#                 'base' : test_core_base,
#         },
#         {
#                 'sources' : files('test-unit-name.c'),
#                 'dependencies' : common_test_dependencies,
#                 'base' : test_core_base,
#         },
#         {
#                 'sources' : files('test-unit-serialize.c'),
#                 'dependencies' : common_test_dependencies,
#                 'base' : test_core_base,
#         },
#         {
#                 'sources' : files('test-watch-pid.c'),
#                 'dependencies' : common_test_dependencies,
#                 'base' : test_core_base,
#         },
#endif // 0

        # Tests from other directories that have link_with deps that were not defined earlier
        {
                'sources' : files('../libelogind/sd-bus/test-bus-error.c'),
                'link_with' : [
                        libshared_static,
                        libelogind_static,
                ],
        },
        {
                'sources' : files('../libelogind/sd-device/test-sd-device-thread.c'),
                'link_with' : libelogind,
                'dependencies' : threads,
        },
#if 0 /// UNNEEDED by elogind
#         {
#                 'sources' : files('../libudev/test-udev-device-thread.c'),
#                 'link_with' : libudev,
#                 'dependencies' : threads,
#         },
#endif // 0
]
