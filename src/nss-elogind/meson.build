# SPDX-License-Identifier: LGPL-2.1-or-later

modules += [
        nss_template + {
                'name' : 'nss_elogind',
                'conditions' : ['ENABLE_NSS_ELOGIND'],
                'sources' : files(
                        'nss-elogind.c',
                        'userdb-glue.c',
                ),
                'version-script' : meson.current_source_dir() / 'nss-elogind.sym',
        },
]
