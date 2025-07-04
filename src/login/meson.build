# SPDX-License-Identifier: LGPL-2.1-or-later

#if 0 /// rename to elogind
# systemd_logind_sources = files(
#         'logind.c',
# )
#else // 0
elogind_sources = files(
        'elogind.c',
        'logind.c')
#endif // 0

logind_gperf_c = custom_target(
        'logind_gperf.c',
        input : 'logind-gperf.gperf',
        output : 'logind-gperf.c',
        command : [gperf, '@INPUT@', '--output-file', '@OUTPUT@'])

liblogind_core_sources = files(
        'logind-action.c',
        'logind-brightness.c',
        'logind-button.c',
        'logind-core.c',
        'logind-dbus.c',
        'logind-device.c',
        'logind-inhibit.c',
        'logind-polkit.c',
        'logind-seat-dbus.c',
        'logind-seat.c',
        'logind-session-dbus.c',
        'logind-session-device.c',
        'logind-session.c',
        'logind-user-dbus.c',
        'logind-user.c',
        'logind-wall.c',
)

#if 1 /// elogind has some additional files:
liblogind_core_sources += files(
        'user-runtime-dir.c'
) + [
        libcore_sources,
        sleep_files,
        update_utmp_files
]
#endif // 1

liblogind_core_sources += [logind_gperf_c]

liblogind_core = static_library(
        'logind-core',
        liblogind_core_sources,
        include_directories : includes,
#if 0 /// elogind needs audit support for utmp inclusion
#         dependencies : [libacl,
#                         userspace],
#else // 0
        dependencies : [libacl,
                        libaudit,
                        userspace],
#endif // 0
        build_by_default : false)

loginctl_sources = files(
        'loginctl.c',
        'sysfs-show.c',
)

#if 1 /// elogind has and needs some additional files:
loginctl_sources += files(
        'eloginctl.c',
) + systemctl_sources
#endif // 1

executables += [
        libexec_template + {
                'name' : 'elogind',
                'dbus' : true,
                'conditions' : ['ENABLE_LOGIND'],
                'sources' : elogind_sources,
                'link_with' : [
                        liblogind_core,
                        libshared,
                ],
                'dependencies' : [
                        libacl,
                        threads,
                ],
        },
        executable_template + {
                'name' : 'loginctl',
                'public' : true,
                'conditions' : ['ENABLE_LOGIND'],
                'sources' : loginctl_sources,
#if 1 /// elogind adds some parts from systemctl to loginctl
                'include_directories' : includes + systemctl_includes,
#endif // 1
                'dependencies' : [
                        liblz4_cflags,
                        libxz_cflags,
                        libzstd_cflags,
                        threads,
                ],
        },
        executable_template + {
                'name' : 'elogind-inhibit',
                'public' : true,
                'conditions' : ['ENABLE_LOGIND'],
                'sources' : files('inhibit.c'),
        },
#if 0 /// elogind does not ship systemd-user-runtime-dir, but uses its functionality directly
#         libexec_template + {
#                 'name' : 'systemd-user-runtime-dir',
#                 'conditions' : ['ENABLE_LOGIND'],
#                 'sources' : files('user-runtime-dir.c'),
#         },
#endif // 0
#if 1 /// elogind has an extra executable to manage uaccess
        libexec_template + {
                'name' : 'elogind-uaccess-command',
                'public' : true,
                'conditions' : ['ENABLE_LOGIND'],
                'sources' : uaccess_command_files,
                'link_with' : [
                        liblogind_core,
                        libshared
                ],
                'dependencies': [
                        libacl,
                        libudev,
                        userspace
                ],
        },
#endif // 1
        test_template + {
                'sources' : files('test-inhibit.c'),
                'type' : 'manual',
        },
        test_template + {
                'sources' : files('test-login-tables.c'),
                'link_with' : [
                        liblogind_core,
                        libshared,
                ],
                'dependencies' : threads,
        },
        test_template + {
                'sources' : files('test-session-properties.c'),
                'type' : 'manual',
        },
]

simple_tests += files(
        'test-login-shared.c'
)

modules += [
        pam_template + {
                'name' : 'pam_elogind',
                'conditions' : [
                        'ENABLE_LOGIND',
                        'HAVE_PAM',
                ],
                'sources' : files('pam_elogind.c'),
                'version-script' : meson.current_source_dir() / 'pam_elogind.sym',
        },
#if 0 /// elogind can currently not be used to autounlock LUKS containers/drives
#         pam_template + {
#                 'name' : 'pam_elogind_loadkey',
#                 'conditions' : [
#                         'HAVE_PAM',
#                 ],
#                 'sources' : files('pam_elogind_loadkey.c'),
#                 'version-script' : meson.current_source_dir() / 'pam_elogind_loadkey.sym',
#         },
#endif // 0
]

enable_logind = conf.get('ENABLE_LOGIND') == 1

custom_target(
        'logind.conf',
        input : 'logind.conf.in',
        output : 'logind.conf',
        command : [jinja2_cmdline, '@INPUT@', '@OUTPUT@'],
        install : enable_logind and install_sysconfdir_samples and pkgsysconfdir != 'no',
        install_dir : pkgconfigfiledir)

#if 0 /// elogind has no user instances like systemd-user
# custom_target(
#         'systemd-user',
#         input : 'systemd-user.in',
#         output : 'systemd-user',
#         command : [jinja2_cmdline, '@INPUT@', '@OUTPUT@'],
#         install : enable_logind and pamconfdir != 'no',
#         install_dir : pamconfdir)
#endif // 0

if enable_logind
        install_data('org.freedesktop.login1.conf',
                     install_dir : dbuspolicydir)
#if 0 /// Works a bit differently here, as elogind allows dbus activation.
#         install_data('org.freedesktop.login1.service',
#                      install_dir : dbussystemservicedir)
#else // 0
        custom_target(
                'org.freedesktop.login1.service',
                input: 'org.freedesktop.login1.service.in',
                output: 'org.freedesktop.login1.service',
                command : [jinja2_cmdline, '@INPUT@', '@OUTPUT@'],
                install: enable_logind,
                install_dir : dbussystemservicedir)
#endif // 0
        install_data('org.freedesktop.login1.policy',
                     install_dir : polkitpolicydir)
endif

#if 1 /// elogind comes with a first drop-in representing its defaults
if install_sysconfdir_samples
        install_data('10-elogind.conf',
                     install_dir : pkgconfigfiledir / 'logind.conf.d' )
endif
#endif // 1
