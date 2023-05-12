/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

#include <dirent.h>
#include <inttypes.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "sd-device.h"

#include "macro.h"

int device_new_from_mode_and_devnum(sd_device **ret, mode_t mode, dev_t devnum);
int device_new_from_nulstr(sd_device **ret, char *nulstr, size_t len);
#if 0 /// UNNEEDED by elogind
int device_new_from_strv(sd_device **ret, char **strv);
#endif // 0

int device_opendir(sd_device *device, const char *subdir, DIR **ret);

int device_get_property_bool(sd_device *device, const char *key);
int device_get_sysattr_int(sd_device *device, const char *sysattr, int *ret_value);
int device_get_sysattr_unsigned(sd_device *device, const char *sysattr, unsigned *ret_value);
int device_get_sysattr_bool(sd_device *device, const char *sysattr);
int device_get_device_id(sd_device *device, const char **ret);

#if 0 /// UNNEEDED by elogind
int device_get_devlink_priority(sd_device *device, int *ret);
int device_get_devnode_mode(sd_device *device, mode_t *ret);
int device_get_devnode_uid(sd_device *device, uid_t *ret);
int device_get_devnode_gid(sd_device *device, gid_t *ret);

void device_clear_sysattr_cache(sd_device *device);
#endif // 0
int device_cache_sysattr_value(sd_device *device, const char *key, char *value);
int device_get_cached_sysattr_value(sd_device *device, const char *key, const char **ret_value);

#if 0 /// UNNEEDED by elogind
void device_seal(sd_device *device);
#endif // 0
void device_set_is_initialized(sd_device *device);
#if 0 /// UNNEEDED by elogind
void device_set_db_persist(sd_device *device);
void device_set_devlink_priority(sd_device *device, int priority);
int device_ensure_usec_initialized(sd_device *device, sd_device *device_old);
#endif // 0
int device_add_devlink(sd_device *device, const char *devlink);
#if 0 /// UNNEEDED by elogind
bool device_has_devlink(sd_device *device, const char *devlink);
#endif // 0
int device_add_property(sd_device *device, const char *property, const char *value);
#if 0 /// UNNEEDED by elogind
int device_add_propertyf(sd_device *device, const char *key, const char *format, ...) _printf_(3, 4);
#endif // 0
int device_add_tag(sd_device *device, const char *tag, bool both);
#if 0 /// UNNEEDED by elogind
void device_remove_tag(sd_device *device, const char *tag);
void device_cleanup_tags(sd_device *device);
void device_cleanup_devlinks(sd_device *device);

uint64_t device_get_properties_generation(sd_device *device);
uint64_t device_get_tags_generation(sd_device *device);
uint64_t device_get_devlinks_generation(sd_device *device);
#endif // 0

int device_properties_prepare(sd_device *device);
int device_get_properties_nulstr(sd_device *device, const char **ret_nulstr, size_t *ret_len);
#if 0 /// UNNEEDED by elogind
int device_get_properties_strv(sd_device *device, char ***ret);

int device_rename(sd_device *device, const char *name);
int device_clone_with_db(sd_device *device, sd_device **ret);

int device_tag_index(sd_device *dev, sd_device *dev_old, bool add);
int device_update_db(sd_device *device);
int device_delete_db(sd_device *device);
#endif // 0
int device_read_db_internal_filename(sd_device *device, const char *filename); /* For fuzzer */
int device_read_db_internal(sd_device *device, bool force);
static inline int device_read_db(sd_device *device) {
        return device_read_db_internal(device, false);
}

int device_read_uevent_file(sd_device *device);

int device_set_action(sd_device *device, sd_device_action_t a);
sd_device_action_t device_action_from_string(const char *s) _pure_;
const char *device_action_to_string(sd_device_action_t a) _const_;
#if 0 /// UNNEEDED by elogind
void dump_device_action_table(void);
#endif // 0
