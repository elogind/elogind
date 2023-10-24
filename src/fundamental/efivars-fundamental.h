/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#ifdef SD_BOOT
#  define EINVAL 22
#else
#  include <errno.h>
#endif
#include "string-util-fundamental.h"

/* Features of the loader, i.e. elogind-boot */
#define EFI_LOADER_FEATURE_CONFIG_TIMEOUT          (UINT64_C(1) << 0)
#define EFI_LOADER_FEATURE_CONFIG_TIMEOUT_ONE_SHOT (UINT64_C(1) << 1)
#define EFI_LOADER_FEATURE_ENTRY_DEFAULT           (UINT64_C(1) << 2)
#define EFI_LOADER_FEATURE_ENTRY_ONESHOT           (UINT64_C(1) << 3)
#define EFI_LOADER_FEATURE_BOOT_COUNTING           (UINT64_C(1) << 4)
#define EFI_LOADER_FEATURE_XBOOTLDR                (UINT64_C(1) << 5)
#define EFI_LOADER_FEATURE_RANDOM_SEED             (UINT64_C(1) << 6)
#define EFI_LOADER_FEATURE_LOAD_DRIVER             (UINT64_C(1) << 7)
#define EFI_LOADER_FEATURE_SORT_KEY                (UINT64_C(1) << 8)
#define EFI_LOADER_FEATURE_SAVED_ENTRY             (UINT64_C(1) << 9)
#define EFI_LOADER_FEATURE_DEVICETREE              (UINT64_C(1) << 10)

/* Features of the stub, i.e. elogind-stub */
#define EFI_STUB_FEATURE_REPORT_BOOT_PARTITION     (UINT64_C(1) << 0)
#define EFI_STUB_FEATURE_PICK_UP_CREDENTIALS       (UINT64_C(1) << 1)
#define EFI_STUB_FEATURE_PICK_UP_SYSEXTS           (UINT64_C(1) << 2)
#define EFI_STUB_FEATURE_THREE_PCRS                (UINT64_C(1) << 3)
#define EFI_STUB_FEATURE_RANDOM_SEED               (UINT64_C(1) << 4)

typedef enum SecureBootMode {
        SECURE_BOOT_UNSUPPORTED,
        SECURE_BOOT_DISABLED,
        SECURE_BOOT_UNKNOWN,
        SECURE_BOOT_AUDIT,
        SECURE_BOOT_DEPLOYED,
        SECURE_BOOT_SETUP,
        SECURE_BOOT_USER,
        _SECURE_BOOT_MAX,
        _SECURE_BOOT_INVALID = -EINVAL,
} SecureBootMode;

#if 0 /// UNNEEDED by elogind
const sd_char *secure_boot_mode_to_string(SecureBootMode m);
#endif // 0
SecureBootMode decode_secure_boot_mode(bool secure, bool audit, bool deployed, bool setup);
