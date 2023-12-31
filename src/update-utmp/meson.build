# SPDX-License-Identifier: LGPL-2.1-or-later

executables += [
        libexec_template + {
                'name' : 'elogind-update-utmp',
                'conditions' : ['ENABLE_UTMP'],
                'sources' : files('update-utmp.c'),
                'dependencies' : libaudit,
        },
]

#if 1 /// needed by elogind for liblogind_core
update_utmp_files = files(
        'update-utmp.c'
)
#endif // 1
