# SPDX-License-Identifier: LGPL-2.1+
#if 0 /// rename to elogind
#
# systemd_logind_sources = files('''
#         logind.c
#         logind.h
# '''.split())
#else // 0
elogind_sources = files('''
        elogind.c
        elogind.h
        logind.c
        logind.h
'''.split())
#endif // 0

logind_gperf_c = custom_target(
        'logind_gperf.c',
        input : 'logind-gperf.gperf',
        output : 'logind-gperf.c',
        command : [gperf, '@INPUT@', '--output-file', '@OUTPUT@'])

liblogind_core_sources = files('''
        logind-acl.h
        logind-action.c
        logind-action.h
        logind-brightness.c
        logind-brightness.h
        logind-button.c
        logind-button.h
        logind-core.c
        logind-dbus.c
        logind-dbus.h
        logind-device.c
        logind-device.h
        logind-inhibit.c
        logind-inhibit.h
        logind-seat-dbus.c
        logind-seat-dbus.h
        logind-seat.c
        logind-seat.h
        logind-session-dbus.c
        logind-session-dbus.h
        logind-session-device.c
        logind-session-device.h
        logind-session.c
        logind-session.h
        logind-user-dbus.c
        logind-user-dbus.h
        logind-user.c
        logind-user.h
        logind-utmp.c
'''.split())

#if 1 /// elogind has some additional files:
liblogind_core_sources += [files('''
        elogind-dbus.c
        elogind-dbus.h
        user-runtime-dir.c
        user-runtime-dir.h
'''.split()),
        libcore_la_sources,
        sleep_files,
        update_utmp_files]
#endif // 1
liblogind_core_sources += [logind_gperf_c]

logind_acl_c = files('logind-acl.c')
if conf.get('HAVE_ACL') == 1
        liblogind_core_sources += logind_acl_c
endif

liblogind_core = static_library(
        'logind-core',
        liblogind_core_sources,
        include_directories : includes,
#if 0 /// elogind needs audit support for utmp inclusion
#         dependencies : [libacl])
#else // 0
        dependencies : [libacl,
                        libaudit])
#endif // 0

loginctl_sources = files('''
        loginctl.c
        sysfs-show.h
        sysfs-show.c
'''.split())

#if 0 /// UNNEEDED by elogind
# user_runtime_dir_sources = files('''
#         user-runtime-dir.c
# '''.split())
#endif // 0

#if 1 /// elogind has and needs some additional files:
loginctl_sources += files('''
        eloginctl.c
        eloginctl.h
'''.split())
#endif // 1
pam_elogind_sym = 'src/login/pam_elogind.sym'
pam_elogind_c = files('pam_elogind.c')

#if 0 /// elogind builds itself unconditionally
# if conf.get('ENABLE_LOGIND') == 1
#endif // 0
        logind_conf = configure_file(
                input : 'logind.conf.in',
                output : 'logind.conf',
                configuration : substs)
        install_data(logind_conf,
                     install_dir : pkgsysconfdir)

        install_data('org.freedesktop.login1.conf',
                     install_dir : dbuspolicydir)
#if 0 /// Works a bit differently here, as elogind allows dbus activation.
#         install_data('org.freedesktop.login1.service',
#                      install_dir : dbussystemservicedir)
#else // 0
        logind_service = configure_file(
                input : 'org.freedesktop.login1.service.in',
                output : 'org.freedesktop.login1.service',
                configuration : substs)
        install_data(logind_service,
                install_dir : dbussystemservicedir)
#endif // 0
        install_data('org.freedesktop.login1.policy',
                     install_dir : polkitpolicydir)

        install_data('70-power-switch.rules', install_dir : udevrulesdir)

        seat_rules = configure_file(
                input : '71-seat.rules.in',
                output : '71-seat.rules',
                configuration : substs)
        install_data(seat_rules,
                     install_dir : udevrulesdir)

#if 1 /// elogind needs two passes of this file
        seat_late_rules = configure_file(
                input : '73-seat-late.rules.in',
                output : '73-seat-late.rules.m4',
                configuration : substs)
#endif // 1
        custom_target(
                '70-uaccess.rules',
                input : '70-uaccess.rules.m4',
                output: '70-uaccess.rules',
                command : [meson_apply_m4, config_h, '@INPUT@'],
                capture : true,
                install : conf.get('HAVE_ACL') == 1,
                install_dir : udevrulesdir)

        custom_target(
                '73-seat-late.rules',
#if 0 /// elogind must configure the source
#                 input : '73-seat-late.rules.m4',
#else // 0
                input : seat_late_rules,
#endif // 0
                output: '73-seat-late.rules',
                command : [meson_apply_m4, config_h, '@INPUT@'],
                capture : true,
                install : true,
                install_dir : udevrulesdir)

        custom_target(
                'elogind-user',
                input : 'elogind-user.m4',
                output: 'elogind-user',
                command : [meson_apply_m4, config_h, '@INPUT@'],
                capture : true,
                install : pamconfdir != 'no',
                install_dir : pamconfdir)
#if 0 /// elogind builds itself unconditionally
# endif
#endif //0
