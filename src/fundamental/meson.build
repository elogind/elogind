# SPDX-License-Identifier: LGPL-2.1-or-later

fundamental_path = meson.current_source_dir()

fundamental_headers = files(
        'bootspec-fundamental.h',
        'efivars-fundamental.h',
        'macro-fundamental.h',
        'memory-util-fundamental.h',
        'sha256.h',
        'string-util-fundamental.h',
#if 0 /// Nowhere used in elogind
#         'tpm-pcr.h',
#endif // 0
)

# for sd-boot
fundamental_source_paths = files(
        'bootspec-fundamental.c',
        'efivars-fundamental.c',
        'sha256.c',
        'string-util-fundamental.c',
#if 0 /// Nowhere used in elogind
#         'tpm-pcr.c',
#endif // 0
)

# for libbasic
fundamental_sources = fundamental_source_paths + fundamental_headers
