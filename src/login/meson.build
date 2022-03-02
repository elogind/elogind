# SPDX-License-Identifier: LGPL-2.1-or-later

#if 0 /// rename to elogind
# systemd_logind_sources = files(
#         'logind.c',
#         'logind.h')
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


#if 1 /// elogind has some additional files:
liblogind_core_sources += [files('''
        elogind-dbus.c
        elogind-dbus.h
        user-runtime-dir.c
        user-runtime-dir.h
'''.split()),
        libcore_sources,
        sleep_files,
        update_utmp_files]
#endif // 1
liblogind_core_sources = files(
        'logind-action.c',
        'logind-action.h',
        'logind-brightness.c',
        'logind-brightness.h',
        'logind-button.c',
        'logind-button.h',
        'logind-core.c',
        'logind-dbus.c',
        'logind-dbus.h',
        'logind-device.c',
        'logind-device.h',
        'logind-inhibit.c',
        'logind-inhibit.h',
        'logind-polkit.c',
        'logind-polkit.h',
        'logind-seat-dbus.c',
        'logind-seat-dbus.h',
        'logind-seat.c',
        'logind-seat.h',
        'logind-session-dbus.c',
        'logind-session-dbus.h',
        'logind-session-device.c',
        'logind-session-device.h',
        'logind-session.c',
        'logind-session.h',
        'logind-user-dbus.c',
        'logind-user-dbus.h',
        'logind-user.c',
        'logind-user.h',
        'logind-utmp.c')

liblogind_core_sources += [logind_gperf_c]

liblogind_core = static_library(
        'logind-core',
        liblogind_core_sources,
        include_directories : includes,
#if 0 /// elogind needs audit support for utmp inclusion
#         dependencies : libacl,
#         build_by_default : false)
#else // 0
        dependencies : [libacl,
                        libaudit])
#endif // 0


#if 0 /// UNNEEDED by elogind
#endif // 0

#if 1 /// elogind has and needs some additional files:
loginctl_sources += files('''
        eloginctl.c
        eloginctl.h
'''.split())
#endif // 1

loginctl_sources = files(
        'loginctl.c',
        'sysfs-show.h',
        'sysfs-show.c')

#if 0 /// elogind builds itself unconditionally
# user_runtime_dir_sources = files('user-runtime-dir.c')
#else // 0
enable_logind = true
#endif // 0

pam_elogind_sym = 'src/login/pam_elogind.sym'
pam_elogind_c = files('pam_elogind.c')

enable_logind = conf.get('ENABLE_LOGIND') == 1
in_files = [
        ['logind.conf',        pkgsysconfdir, enable_logind and install_sysconfdir_samples],
        ['70-uaccess.rules',   udevrulesdir,  enable_logind and conf.get('HAVE_ACL') == 1],
        ['71-seat.rules',      udevrulesdir,  enable_logind],
        ['73-seat-late.rules', udevrulesdir,  enable_logind],
        ['elogind-user',       pamconfdir,    enable_logind]]

foreach tuple : in_files
        file = tuple[0]
        dir = tuple[1]
        install = (dir == pkgsysconfdir) ? install_sysconfdir_samples : (dir != 'no')
        custom_target(
                file,
                input : file + '.in',
                output: file,
                command : [meson_render_jinja2, config_h, '@INPUT@', '@OUTPUT@'],
                install : tuple[2] and install,
                install_dir : dir)
endforeach

if enable_logind
        install_data('org.freedesktop.login1.conf',
                     install_dir : dbuspolicydir)
#if 0 /// Works a bit differently here, as elogind allows dbus activation.
#         install_data('org.freedesktop.login1.service',
#                      install_dir : dbussystemservicedir)
#else // 0
        logind_service = configure_file(
                input : 'org.freedesktop.login1.service.in',
                output : 'org.freedesktop.login1.service',
                configuration : conf)
        install_data(logind_service,
                install_dir : dbussystemservicedir)
#endif // 0
        install_data('org.freedesktop.login1.policy',
                     install_dir : polkitpolicydir)
        install_data('70-power-switch.rules',
                     install_dir : udevrulesdir)
endif

############################################################

tests += [
        [files('test-login-shared.c')],

        [files('test-inhibit.c'),
         [], [], [], '', 'manual'],

        [files('test-login-tables.c'),
         [liblogind_core,
          libshared],
         [threads]],
]
