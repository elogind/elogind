# SPDX-License-Identifier: LGPL-2.1-or-later

executables += [
#if 0 /// sleep is integrated in elogind and not an extra utility
#         libexec_template + {
#                 'name' : 'elogind-sleep',
#                 'sources' : files(
#                         'sleep.c',
#                         'battery-capacity.c',
#                 ),
#         },
#endif // 0
        test_template + {
                'sources' : files(
                        'test-battery-capacity.c',
                        'battery-capacity.c',
                ),
        },
]

#if 1 /// elogind needs the files list for sleep integration
sleep_files = files(
        'sleep.c',
        'battery-capacity.c'
)
#endif // 1

if install_sysconfdir_samples
        install_data('sleep.conf',
                     install_dir : pkgconfigfiledir)
endif
