# SPDX-License-Identifier: LGPL-2.1-or-later

#if 0 /// The list is much shorter for elogind (We only add some parts to loginctl)
# systemctl_sources = files(
#         'systemctl-add-dependency.c',
#         'systemctl-cancel-job.c',
#         'systemctl-clean-or-freeze.c',
#         'systemctl-compat-halt.c',
#         'systemctl-compat-runlevel.c',
#         'systemctl-compat-shutdown.c',
#         'systemctl-compat-telinit.c',
#         'systemctl-daemon-reload.c',
#         'systemctl-edit.c',
#         'systemctl-enable.c',
#         'systemctl-is-active.c',
#         'systemctl-is-enabled.c',
#         'systemctl-is-system-running.c',
#         'systemctl-kill.c',
#         'systemctl-list-dependencies.c',
#         'systemctl-list-jobs.c',
#         'systemctl-list-machines.c',
#         'systemctl-list-unit-files.c',
#         'systemctl-list-units.c',
#         'systemctl-log-setting.c',
#         'systemctl-logind.c',
#         'systemctl-mount.c',
#         'systemctl-preset-all.c',
#         'systemctl-reset-failed.c',
#         'systemctl-service-watchdogs.c',
#         'systemctl-set-default.c',
#         'systemctl-set-environment.c',
#         'systemctl-set-property.c',
#         'systemctl-show.c',
#         'systemctl-start-special.c',
#         'systemctl-start-unit.c',
#         'systemctl-switch-root.c',
#         'systemctl-sysv-compat.c',
#         'systemctl-trivial-method.c',
#         'systemctl-util.c',
#         'systemctl-whoami.c',
#         'systemctl.c',
# )
#else // 0
systemctl_sources = files(
        'systemctl-daemon-reload.c',
        'systemctl-logind.c',
        'systemctl-start-special.c',
        'systemctl-start-unit.c',
        'systemctl-sysv-compat.c',
        'systemctl-trivial-method.c',
        'systemctl-util.c',
        'systemctl.c',
)
systemctl_includes = [include_directories('.')]
#endif // 0

#if 0 /// elogind does not install systemctl
# if get_option('link-systemctl-shared')
#         systemctl_link_with = [libshared]
# else
#         systemctl_link_with = [libsystemd_static,
#                                libshared_static]
# endif
#
# executables += [
#         executable_template + {
#                 'name' : 'systemctl',
#                 'public' : true,
#                 'sources' : systemctl_sources,
#                 'link_with' : systemctl_link_with,
#                 'dependencies' : [
#                         libcap,
#                         liblz4_cflags,
#                         libselinux,
#                         libxz_cflags,
#                         libzstd_cflags,
#                         threads,
#                 ],
#                 'install_tag' : 'systemctl',
#         },
#         fuzz_template + {
#                 'sources' : [
#                         files('fuzz-systemctl-parse-argv.c'),
#                         systemctl_sources,
#                 ],
#                 'link_with' : systemctl_link_with,
#                 'c_args' : ['-DFUZZ_SYSTEMCTL_PARSE_ARGV'],
#         },
# ]
#
# foreach alias : (['halt', 'poweroff', 'reboot', 'shutdown'] +
#                  (conf.get('HAVE_SYSV_COMPAT') == 1 ? ['runlevel', 'telinit'] : []))
#         install_emptydir(sbindir)
#         meson.add_install_script(sh, '-c',
#                                  ln_s.format(bindir / 'systemctl',
#                                              sbindir / alias))
# endforeach
#endif // 0