# SPDX-License-Identifier: LGPL-2.1-or-later

#if 0 /// sleep is integrated in elogind and not an extra utility
# executables += [
#         libexec_template + {
#                 'name' : 'elogind-sleep',
#                 'sources' : files(
#                         'sleep.c',
#                         'battery-capacity.c',
#                 ),
#         },
#         test_template + {
#                 'sources' : files(
#                         'test-battery-capacity.c',
#                         'battery-capacity.c',
#                 ),
#         },
# ]
#else // 0
sleep_files = files('''
        sleep.c
        sleep.h
'''.split())
#endif // 0

if install_sysconfdir_samples
        install_data('sleep.conf',
                     install_dir : pkgconfigfiledir)
endif
