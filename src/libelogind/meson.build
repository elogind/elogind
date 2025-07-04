# SPDX-License-Identifier: LGPL-2.1-or-later

sd_journal_sources = files(
#if 0 /// UNNEEDED by elogind
#         'sd-journal/audit-type.c',
#         'sd-journal/catalog.c',
#         'sd-journal/journal-file.c',
#endif // 0
        'sd-journal/journal-send.c',
#if 0 /// UNNEEDED by elogind
#         'sd-journal/journal-vacuum.c',
#         'sd-journal/journal-verify.c',
#         'sd-journal/lookup3.c',
#         'sd-journal/mmap-cache.c',
#endif // 0
        'sd-journal/sd-journal.c',
)

#if 0 /// UNNEEDED by elogind
# if conf.get('HAVE_GCRYPT') == 1
#         sd_journal_sources += files(
#                 'sd-journal/fsprg.c',
#                 'sd-journal/journal-authenticate.c',
#         )
# endif
#
# audit_type_includes = [config_h,
#                        missing_audit_h,
#                        'linux/audit.h']
# if conf.get('HAVE_AUDIT') == 1
#         audit_type_includes += 'libaudit.h'
# endif
#
# generate_audit_type_list = find_program('sd-journal/generate-audit_type-list.sh')
# audit_type_list_txt = custom_target(
#         'audit_type-list.txt',
#         output : 'audit_type-list.txt',
#         command : [generate_audit_type_list, cpp] + audit_type_includes,
#         capture : true)
#
# audit_type_to_name = custom_target(
#         'audit_type-to-name.h',
#         input : ['sd-journal/audit_type-to-name.awk', audit_type_list_txt],
#         output : 'audit_type-to-name.h',
#         command : [awk, '-f', '@INPUT0@', '@INPUT1@'],
#         capture : true)
#
# sd_journal_sources += [audit_type_to_name]
#endif // 0

############################################################

sd_id128_sources = files(
        'sd-id128/id128-util.c',
        'sd-id128/sd-id128.c',
)

############################################################

sd_daemon_sources = files('sd-daemon/sd-daemon.c')

############################################################

sd_event_sources = files(
        'sd-event/event-util.c',
        'sd-event/sd-event.c',
)

############################################################

sd_bus_sources = files(
        'sd-bus/bus-common-errors.c',
        'sd-bus/bus-container.c',
        'sd-bus/bus-control.c',
        'sd-bus/bus-convenience.c',
        'sd-bus/bus-creds.c',
        'sd-bus/bus-dump.c',
        'sd-bus/bus-error.c',
        'sd-bus/bus-internal.c',
        'sd-bus/bus-introspect.c',
        'sd-bus/bus-kernel.c',
        'sd-bus/bus-match.c',
        'sd-bus/bus-message.c',
        'sd-bus/bus-objects.c',
        'sd-bus/bus-signature.c',
        'sd-bus/bus-slot.c',
        'sd-bus/bus-socket.c',
        'sd-bus/bus-track.c',
        'sd-bus/bus-type.c',
        'sd-bus/sd-bus.c',
)

############################################################

sd_device_sources = files(
        'sd-device/device-enumerator.c',
        'sd-device/device-filter.c',
        'sd-device/device-monitor.c',
        'sd-device/device-private.c',
        'sd-device/device-util.c',
        'sd-device/sd-device.c',
)

############################################################

sd_login_sources = files('sd-login/sd-login.c')

############################################################

sd_json_sources = files(
        'sd-json/json-util.c',
        'sd-json/sd-json.c',
)

############################################################

sd_varlink_sources = files(
        'sd-varlink/sd-varlink.c',
        'sd-varlink/sd-varlink-idl.c',
        'sd-varlink/varlink-io.systemd.c',
        'sd-varlink/varlink-org.varlink.service.c',
        'sd-varlink/varlink-util.c',
)

############################################################

sd_path_sources = files(
#if 0 /// Not used by elogind
#         'sd-path/path-lookup.c',
#endif // 0
        'sd-path/sd-path.c',
)

############################################################

sd_netlink_sources = files(
        'sd-netlink/netlink-genl.c',
#if 0 /// Not used by elogind
#         'sd-netlink/netlink-message-nfnl.c',
#endif // 0
        'sd-netlink/netlink-message-rtnl.c',
        'sd-netlink/netlink-message.c',
        'sd-netlink/netlink-slot.c',
        'sd-netlink/netlink-socket.c',
        'sd-netlink/netlink-types-genl.c',
        'sd-netlink/netlink-types-nfnl.c',
        'sd-netlink/netlink-types-rtnl.c',
        'sd-netlink/netlink-types.c',
        'sd-netlink/netlink-util.c',
        'sd-netlink/sd-netlink.c',
)

############################################################

sd_network_sources = files(
#if 0 /// Not used by elogind
#         'sd-network/network-util.c',
#         'sd-network/sd-network.c',
#endif // 0
)

############################################################

libelogind_sources = files(
        'sd-hwdb/sd-hwdb.c',
#if 0 /// elogind does not need sd-resolve.c
#         'sd-resolve/sd-resolve.c',
#endif // 0
) + sd_journal_sources + sd_id128_sources + sd_daemon_sources \
  + sd_event_sources + sd_bus_sources + sd_device_sources \
  + sd_login_sources + sd_json_sources + sd_varlink_sources \
  + sd_path_sources + sd_netlink_sources + sd_network_sources

libelogind_c_args = ['-fvisibility=default']

#if 0 /// rename to libelogind
# libsystemd_static = static_library(
#         'systemd_static',
#         libsystemd_sources,
#         include_directories : libsystemd_includes,
#         c_args : libsystemd_c_args,
#         link_with : [libbasic_static],
#         dependencies : [threads,
#                         libm,
#                         librt,
#                         userspace],
#         build_by_default : false)
#
# libsystemd_dir_path = meson.current_source_dir()
#
# libsystemd_sym = files('libsystemd.sym')
# libsystemd_sym_path = libsystemd_dir_path / 'libsystemd.sym'
#
# static_libsystemd = get_option('static-libsystemd')
# static_libsystemd_pic = static_libsystemd == 'true' or static_libsystemd == 'pic'
#
# libsystemd_pc = custom_target(
#         'libsystemd.pc',
#         input : 'libsystemd.pc.in',
#         output : 'libsystemd.pc',
#         command : [jinja2_cmdline, '@INPUT@', '@OUTPUT@'],
#         install : pkgconfiglibdir != 'no',
#         install_tag : 'devel',
#         install_dir : pkgconfiglibdir)
#else // 0
libelogind_static = static_library(
        'elogind_static',
        libelogind_sources,
        include_directories : libelogind_includes,
        c_args : libelogind_c_args,
        link_with : [libbasic_static],
        dependencies : [threads,
                        userspace],
        build_by_default : false)

libelogind_dir_path = meson.current_source_dir()

libelogind_sym = files('libelogind.sym')
libelogind_sym_path = libelogind_dir_path / 'libelogind.sym'

static_libelogind = get_option('static-libelogind')
static_libelogind_pic = static_libelogind == 'true' or static_libelogind == 'pic'

libelogind_pc = custom_target(
        'libelogind.pc',
        input : 'libelogind.pc.in',
        output : 'libelogind.pc',
        command : [jinja2_cmdline, '@INPUT@', '@OUTPUT@'],
        install : pkgconfiglibdir != 'no',
        install_tag : 'devel',
        install_dir : pkgconfiglibdir)
#endif // 0

############################################################

#if 0 /// elogind has only a tiny journald stub, none of these tests could do anything useful
# simple_tests += files(
#         'sd-journal/test-audit-type.c',
#         'sd-journal/test-catalog.c',
#         'sd-journal/test-journal-file.c',
#         'sd-journal/test-journal-init.c',
#         'sd-journal/test-journal-match.c',
#         'sd-journal/test-journal-send.c',
#         'sd-journal/test-mmap-cache.c',
# )
#
# libelogind_tests += [
#         {
#                 'sources' : files('sd-journal/test-journal-enum.c'),
#                 'timeout' : 360,
#         },
#         {
#                 'sources' : files('sd-event/test-event.c'),
#                 'timeout' : 120,
#         }
# ]
#endif // 0

############################################################

simple_tests += files(
        'sd-bus/test-bus-creds.c',
        'sd-bus/test-bus-introspect.c',
        'sd-bus/test-bus-match.c',
        'sd-bus/test-bus-vtable.c',
        'sd-device/test-device-util.c',
        'sd-device/test-sd-device-monitor.c',
        'sd-device/test-sd-device.c',
#if 0 /// elogind only stubs tiny parts of journald, none of these would work
#         'sd-journal/test-journal-flush.c',
#         'sd-journal/test-journal-interleaving.c',
#         'sd-journal/test-journal-stream.c',
#         'sd-journal/test-journal.c',
#endif // 0
        'sd-login/test-login.c',
#if 0 /// elogind does not provide the networking stuff
#         'sd-netlink/test-netlink.c',
#endif // 0
)

libelogind_tests += [
        {
                'sources' : files('sd-bus/test-bus-address.c'),
                'dependencies' : threads
        },
        {
                'sources' : files('sd-bus/test-bus-benchmark.c'),
                'dependencies' : threads,
                'type' : 'manual',
        },
        {
                'sources' : files('sd-bus/test-bus-chat.c'),
                'dependencies' : threads,
        },
#if 0 /// UNNEEDED by elogind
#         {
#                 'sources' : files('sd-bus/test-bus-cleanup.c'),
#                 'dependencies' : [threads, libseccomp],
#         },
#         {
#                 'sources' : files('sd-bus/test-bus-marshal.c'),
#                 'dependencies' : [
#                         libdbus,
#                         libgio,
#                         libglib,
#                         libgobject,
#                         libm,
#                         threads,
#                 ],
#         },
#endif // 0
        {
                'sources' : files('sd-bus/test-bus-objects.c'),
                'dependencies' : threads,
        },
        {
                'sources' : files('sd-bus/test-bus-peersockaddr.c'),
                'dependencies' : threads,
        },
#if 0 /// UNNEEDED by elogind
#         {
#                 'sources' : files('sd-bus/test-bus-queue-ref-cycle.c'),
#                 'dependencies' : threads,
#         },
#endif // 0
        {
                'sources' : files('sd-bus/test-bus-server.c'),
                'dependencies' : threads,
        },
        {
                'sources' : files('sd-bus/test-bus-signature.c'),
                'dependencies' : threads,
        },
        {
                'sources' : files('sd-bus/test-bus-track.c'),
                'dependencies' : libseccomp,
        },
        {
                'sources' : files('sd-bus/test-bus-watch-bind.c'),
                'dependencies' : threads,
                'timeout' : 120,
        },
#if 0 /// UNNEEDED by elogind
#         {
#                 'sources' : files('sd-journal/test-journal-append.c'),
#                 'type' : 'manual',
#         },
#         {
#                 'sources' : files('sd-journal/test-journal-verify.c'),
#                 'timeout' : 90,
#         },
#         {
#                 'sources' : files('sd-resolve/test-resolve.c'),
#                 'dependencies' : threads,
#                 'timeout' : 120,
#         },
#endif // 0
]

if cxx_cmd != ''
        simple_tests += files('sd-bus/test-bus-vtable-cc.cc')
endif

############################################################

#if 0 /// UNNEEDED by elogind
# simple_fuzzers += files(
#         'sd-bus/fuzz-bus-match.c',
#         'sd-bus/fuzz-bus-message.c',
# )
#endif // 0
