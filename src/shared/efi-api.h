/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include "efivars-fundamental.h"
#include "efivars.h"
#include "string-util.h"

/* Various calls for interfacing with EFI variables from the official UEFI specs. */

#if ENABLE_EFI

int efi_reboot_to_firmware_supported(void);
int efi_get_reboot_to_firmware(void);
int efi_set_reboot_to_firmware(bool value);

#if 0 /// UNNEEDED by elogind
int efi_get_boot_option(uint16_t nr, char **ret_title, sd_id128_t *ret_part_uuid, char **ret_path, bool *ret_active);
int efi_add_boot_option(uint16_t id, const char *title, uint32_t part, uint64_t pstart, uint64_t psize, sd_id128_t part_uuid, const char *path);
int efi_remove_boot_option(uint16_t id);
int efi_get_boot_order(uint16_t **ret_order);
int efi_set_boot_order(const uint16_t *order, size_t n);
int efi_get_boot_options(uint16_t **ret_options);

bool efi_has_tpm2(void);
#endif // 0

#else

static inline int efi_reboot_to_firmware_supported(void) {
        return -EOPNOTSUPP;
}

static inline int efi_get_reboot_to_firmware(void) {
        return -EOPNOTSUPP;
}

static inline int efi_set_reboot_to_firmware(bool value) {
        return -EOPNOTSUPP;
}

#if 0 /// UNNEEDED by elogind
static inline int efi_get_boot_option(uint16_t nr, char **ret_title, sd_id128_t *ret_part_uuid, char **ret_path, bool *ret_active) {
        return -EOPNOTSUPP;
}

static inline int efi_add_boot_option(uint16_t id, const char *title, uint32_t part, uint64_t pstart, uint64_t psize, sd_id128_t part_uuid, const char *path) {
        return -EOPNOTSUPP;
}

static inline int efi_remove_boot_option(uint16_t id) {
        return -EOPNOTSUPP;
}

static inline int efi_get_boot_order(uint16_t **ret_order) {
        return -EOPNOTSUPP;
}

static inline int efi_set_boot_order(const uint16_t *order, size_t n) {
        return -EOPNOTSUPP;
}

static inline int efi_get_boot_options(uint16_t **ret_options) {
        return -EOPNOTSUPP;
}

static inline bool efi_has_tpm2(void) {
        return false;
}
#endif // 0

#endif

#if 0 /// UNNEEDED by elogind
static inline char *efi_tilt_backslashes(char *s) {
        return string_replace_char(s, '\\', '/');
}
#endif // 0

sd_id128_t efi_guid_to_id128(const void *guid);
void efi_id128_to_guid(sd_id128_t id, void *ret_guid);
