/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include "alloc-util.h"
#include "efi-loader.h"
#include "parse-util.h"
#include "path-util.h"
#include "stat-util.h"
#include "strv.h"
#include "utf8.h"

#if ENABLE_EFI


#if 0 /// UNNEEDED by elogind
static int read_usec(const char *variable, usec_t *ret) {
        _cleanup_free_ char *j = NULL;
        uint64_t x = 0;
        int r;

        assert(variable);
        assert(ret);

        r = efi_get_variable_string(variable, &j);
        if (r < 0)
                return r;

        r = safe_atou64(j, &x);
        if (r < 0)
                return r;

        *ret = x;
        return 0;
}

int efi_loader_get_boot_usec(usec_t *ret_firmware, usec_t *ret_loader) {
        uint64_t x, y;
        int r;

        assert(ret_firmware);
        assert(ret_loader);

        if (!is_efi_boot())
                return -EOPNOTSUPP;

        r = read_usec(EFI_LOADER_VARIABLE(LoaderTimeInitUSec), &x);
        if (r < 0)
                return log_debug_errno(r, "Failed to read LoaderTimeInitUSec: %m");

        r = read_usec(EFI_LOADER_VARIABLE(LoaderTimeExecUSec), &y);
        if (r < 0)
                return log_debug_errno(r, "Failed to read LoaderTimeExecUSec: %m");

        if (y == 0 || y < x || y - x > USEC_PER_HOUR)
                return log_debug_errno(SYNTHETIC_ERRNO(EIO),
                                       "Bad LoaderTimeInitUSec=%"PRIu64", LoaderTimeExecUSec=%" PRIu64"; refusing.",
                                       x, y);

        *ret_firmware = x;
        *ret_loader = y;
        return 0;
}

int efi_loader_get_device_part_uuid(sd_id128_t *ret) {
        _cleanup_free_ char *p = NULL;
        int r;
        unsigned parsed[16];

        if (!is_efi_boot())
                return -EOPNOTSUPP;

        r = efi_get_variable_string(EFI_LOADER_VARIABLE(LoaderDevicePartUUID), &p);
        if (r < 0)
                return r;

        if (sscanf(p, SD_ID128_UUID_FORMAT_STR,
                   &parsed[0], &parsed[1], &parsed[2], &parsed[3],
                   &parsed[4], &parsed[5], &parsed[6], &parsed[7],
                   &parsed[8], &parsed[9], &parsed[10], &parsed[11],
                   &parsed[12], &parsed[13], &parsed[14], &parsed[15]) != 16)
                return -EIO;

        if (ret)
                for (unsigned i = 0; i < ELEMENTSOF(parsed); i++)
                        ret->bytes[i] = parsed[i];

        return 0;
}
#endif // 0

int efi_loader_get_entries(char ***ret) {
        _cleanup_free_ char16_t *entries = NULL;
        _cleanup_strv_free_ char **l = NULL;
        size_t size;
        int r;

        assert(ret);

        if (!is_efi_boot())
                return -EOPNOTSUPP;

        r = efi_get_variable(EFI_LOADER_VARIABLE(LoaderEntries), NULL, (void**) &entries, &size);
        if (r < 0)
                return r;

        /* The variable contains a series of individually NUL terminated UTF-16 strings. */

        for (size_t i = 0, start = 0;; i++) {
                _cleanup_free_ char *decoded = NULL;
                bool end;

                /* Is this the end of the variable's data? */
                end = i * sizeof(char16_t) >= size;

                /* Are we in the middle of a string? (i.e. not at the end of the variable, nor at a NUL terminator?) If
                 * so, let's go to the next entry. */
                if (!end && entries[i] != 0)
                        continue;

                /* We reached the end of a string, let's decode it into UTF-8 */
                decoded = utf16_to_utf8(entries + start, (i - start) * sizeof(char16_t));
                if (!decoded)
                        return -ENOMEM;

                if (efi_loader_entry_name_valid(decoded)) {
                        r = strv_consume(&l, TAKE_PTR(decoded));
                        if (r < 0)
                                return r;
                } else
                        log_debug("Ignoring invalid loader entry '%s'.", decoded);

                /* We reached the end of the variable */
                if (end)
                        break;

                /* Continue after the NUL byte */
                start = i + 1;
        }

        *ret = TAKE_PTR(l);
        return 0;
}

int efi_loader_get_features(uint64_t *ret) {
        _cleanup_free_ void *v = NULL;
        size_t s;
        int r;

        assert(ret);

        if (!is_efi_boot()) {
                *ret = 0;
                return 0;
        }

        r = efi_get_variable(EFI_LOADER_VARIABLE(LoaderFeatures), NULL, &v, &s);
        if (r == -ENOENT) {
                _cleanup_free_ char *info = NULL;

                /* The new (v240+) LoaderFeatures variable is not supported, let's see if it's systemd-boot at all */
                r = efi_get_variable_string(EFI_LOADER_VARIABLE(LoaderInfo), &info);
                if (r < 0) {
                        if (r != -ENOENT)
                                return r;

                        /* Variable not set, definitely means not systemd-boot */

                } else if (first_word(info, "systemd-boot")) {

                        /* An older systemd-boot version. Let's hardcode the feature set, since it was pretty
                         * static in all its versions. */

                        *ret = EFI_LOADER_FEATURE_CONFIG_TIMEOUT |
                                EFI_LOADER_FEATURE_ENTRY_DEFAULT |
                                EFI_LOADER_FEATURE_ENTRY_ONESHOT;

                        return 0;
                }

                /* No features supported */
                *ret = 0;
                return 0;
        }
        if (r < 0)
                return r;

        if (s != sizeof(uint64_t))
                return log_debug_errno(SYNTHETIC_ERRNO(EINVAL),
                                       "LoaderFeatures EFI variable doesn't have the right size.");

        memcpy(ret, v, sizeof(uint64_t));
        return 0;
}

#if 0 /// UNNEEDED by elogind
int efi_stub_get_features(uint64_t *ret) {
        _cleanup_free_ void *v = NULL;
        size_t s;
        int r;

        assert(ret);

        if (!is_efi_boot()) {
                *ret = 0;
                return 0;
        }

        r = efi_get_variable(EFI_LOADER_VARIABLE(StubFeatures), NULL, &v, &s);
        if (r == -ENOENT) {
                _cleanup_free_ char *info = NULL;

                /* The new (v252+) StubFeatures variable is not supported, let's see if it's elogind-stub at all */
                /* The new (v252+) StubFeatures variable is not supported, let's see if it's systemd-stub at all */
                r = efi_get_variable_string(EFI_LOADER_VARIABLE(StubInfo), &info);
                if (r < 0) {
                        if (r != -ENOENT)
                                return r;

                        /* Variable not set, definitely means not elogind-stub */
                        /* Variable not set, definitely means not systemd-stub */

                } else if (first_word(info, "systemd-stub")) {

                        /* An older systemd-stub version. Let's hardcode the feature set, since it was pretty
                         * static in all its versions. */

                        *ret = EFI_STUB_FEATURE_REPORT_BOOT_PARTITION;
                        return 0;
                }

                /* No features supported */
                *ret = 0;
                return 0;
        }
        if (r < 0)
                return r;

        if (s != sizeof(uint64_t))
                return log_debug_errno(SYNTHETIC_ERRNO(EINVAL),
                                       "StubFeatures EFI variable doesn't have the right size.");

        memcpy(ret, v, sizeof(uint64_t));
        return 0;
}
#endif // 0

int efi_loader_get_config_timeout_one_shot(usec_t *ret) {
        _cleanup_free_ char *v = NULL;
        static struct stat cache_stat = {};
        struct stat new_stat;
        static usec_t cache;
        uint64_t sec;
        int r;

        assert(ret);

        /* stat() the EFI variable, to see if the mtime changed. If it did, we need to cache again. */
        if (stat(EFIVAR_PATH(EFI_LOADER_VARIABLE(LoaderConfigTimeoutOneShot)), &new_stat) < 0)
                return -errno;

        if (stat_inode_unmodified(&new_stat, &cache_stat)) {
                *ret = cache;
                return 0;
        }

        r = efi_get_variable_string(EFI_LOADER_VARIABLE(LoaderConfigTimeoutOneShot), &v);
        if (r < 0)
                return r;

        r = safe_atou64(v, &sec);
        if (r < 0)
                return r;
        if (sec > USEC_INFINITY / USEC_PER_SEC)
                return -ERANGE;

        cache_stat = new_stat;
        *ret = cache = sec * USEC_PER_SEC; /* return in µs */
        return 0;
}

int efi_loader_update_entry_one_shot_cache(char **cache, struct stat *cache_stat) {
        _cleanup_free_ char *v = NULL;
        struct stat new_stat;
        int r;

        assert(cache);
        assert(cache_stat);

        /* stat() the EFI variable, to see if the mtime changed. If it did we need to cache again. */
        if (stat(EFIVAR_PATH(EFI_LOADER_VARIABLE(LoaderEntryOneShot)), &new_stat) < 0)
                return -errno;

        if (stat_inode_unmodified(&new_stat, cache_stat))
                return 0;

        r = efi_get_variable_string(EFI_LOADER_VARIABLE(LoaderEntryOneShot), &v);
        if (r < 0)
                return r;

        if (!efi_loader_entry_name_valid(v))
                return -EINVAL;

        *cache_stat = new_stat;
        free_and_replace(*cache, v);

        return 0;
}

#endif

bool efi_loader_entry_name_valid(const char *s) {
        if (!filename_is_valid(s)) /* Make sure entry names fit in filenames */
                return false;

        return in_charset(s, ALPHANUMERICAL "+-_.");
}

