/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <stdint.h>
#include <sys/mount.h>

#include "sd-bus.h"

#include "bus-error.h"
#include "bus-locator.h"
#include "dev-setup.h"
#include "format-util.h"
#include "fs-util.h"
#include "label.h"
#include "limits-util.h"
//#include "main-func.h"
#include "mkdir-label.h"
#include "mount-util.h"
#include "mountpoint-util.h"
#include "path-util.h"
#include "rm-rf.h"
//#include "selinux-util.h"
#include "smack-util.h"
#include "stdio-util.h"
#include "string-util.h"
//#include "strv.h"
#include "user-util.h"
/// Additional includes needed by elogind
#include "log.h"
#include "user-runtime-dir.h"

#if 0 /// UNNEEDED by elogind
static int acquire_runtime_dir_properties(uint64_t *size, uint64_t *inodes) {
        _cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
        _cleanup_(sd_bus_flush_close_unrefp) sd_bus *bus = NULL;
        int r;

        r = sd_bus_default_system(&bus);
        if (r < 0)
                return log_error_errno(r, "Failed to connect to system bus: %m");

        r = bus_get_property_trivial(bus, bus_login_mgr, "RuntimeDirectorySize", &error, 't', size);
        if (r < 0) {
                log_warning_errno(r, "Failed to acquire runtime directory size, ignoring: %s", bus_error_message(&error, r));
                *size = physical_memory_scale(10U, 100U); /* 10% */
        }

        r = bus_get_property_trivial(bus, bus_login_mgr, "RuntimeDirectoryInodesMax", &error, 't', inodes);
        if (r < 0) {
                log_warning_errno(r, "Failed to acquire number of inodes for runtime directory, ignoring: %s", bus_error_message(&error, r));
                *inodes = DIV_ROUND_UP(*size, 4096);
        }

        return 0;
}
#endif // 0

static int user_mkdir_runtime_path(
                const char *runtime_path,
                uid_t uid,
                gid_t gid,
                uint64_t runtime_dir_size,
                uint64_t runtime_dir_inodes) {

        int r;

        assert(runtime_path);
        assert(path_is_absolute(runtime_path));
        assert(uid_is_valid(uid));
        assert(gid_is_valid(gid));

        r = mkdir_safe_label("/run/user", 0755, 0, 0, MKDIR_WARN_MODE);
        if (r < 0)
                return log_error_errno(r, "Failed to create /run/user: %m");

        if (path_is_mount_point(runtime_path, NULL, 0) >= 0)
                log_debug("%s is already a mount point", runtime_path);
        else {
                char options[sizeof("mode=0700,uid=,gid=,size=,nr_inodes=,smackfsroot=*")
                             + DECIMAL_STR_MAX(uid_t)
                             + DECIMAL_STR_MAX(gid_t)
                             + DECIMAL_STR_MAX(uint64_t)
                             + DECIMAL_STR_MAX(uint64_t)];

                xsprintf(options,
                         "mode=0700,uid=" UID_FMT ",gid=" GID_FMT ",size=%" PRIu64 ",nr_inodes=%" PRIu64 "%s",
                         uid, gid, runtime_dir_size, runtime_dir_inodes,
                         mac_smack_use() ? ",smackfsroot=*" : "");

                r = mkdir_label(runtime_path, 0700);
                if (r < 0 && r != -EEXIST)
                        return log_error_errno(r, "Failed to create %s: %m", runtime_path);

                r = mount_nofollow_verbose(LOG_DEBUG, "tmpfs", runtime_path, "tmpfs", MS_NODEV|MS_NOSUID, options);
                if (r < 0) {
                        if (!ERRNO_IS_PRIVILEGE(r)) {
                                log_error_errno(r, "Failed to mount per-user tmpfs directory %s: %m", runtime_path);
                                goto fail;
                        }

                        log_debug_errno(r,
                                        "Failed to mount per-user tmpfs directory %s.\n"
                                        "Assuming containerized execution, ignoring: %m", runtime_path);

                        r = chmod_and_chown(runtime_path, 0700, uid, gid);
                        if (r < 0) {
                                log_error_errno(r, "Failed to change ownership and mode of \"%s\": %m", runtime_path);
                                goto fail;
                        }
                }

                r = label_fix(runtime_path, 0);
                if (r < 0)
                        log_warning_errno(r, "Failed to fix label of \"%s\", ignoring: %m", runtime_path);
        }

        return 0;

fail:
        /* Try to clean up, but ignore errors */
        (void) rmdir(runtime_path);
        return r;
}

static int user_remove_runtime_path(const char *runtime_path) {
        int r;

        assert(runtime_path);
        assert(path_is_absolute(runtime_path));

        r = rm_rf(runtime_path, 0);
        if (r < 0)
                log_debug_errno(r, "Failed to remove runtime directory %s (before unmounting), ignoring: %m", runtime_path);

        /* Ignore cases where the directory isn't mounted, as that's quite possible, if we lacked the permissions to
         * mount something */
        r = umount2(runtime_path, MNT_DETACH);
        if (r < 0 && !IN_SET(errno, EINVAL, ENOENT))
                log_debug_errno(errno, "Failed to unmount user runtime directory %s, ignoring: %m", runtime_path);

        r = rm_rf(runtime_path, REMOVE_ROOT);
        if (r < 0 && r != -ENOENT)
                return log_error_errno(r, "Failed to remove runtime directory %s (after unmounting): %m", runtime_path);

        return 0;
}

#if 0 /// having a User instance, elogind can ask its manager directly.
static int do_mount(const char *user) {
        char runtime_path[sizeof("/run/user") + DECIMAL_STR_MAX(uid_t)];
        uint64_t runtime_dir_size, runtime_dir_inodes;
        uid_t uid;
        gid_t gid;
        int r;

        r = get_user_creds(&user, &uid, &gid, NULL, NULL, 0);
        if (r < 0)
                return log_error_errno(r,
                                       r == -ESRCH ? "No such user \"%s\"" :
                                       r == -ENOMSG ? "UID \"%s\" is invalid or has an invalid main group"
                                                    : "Failed to look up user \"%s\": %m",
                                       user);

        r = acquire_runtime_dir_properties(&runtime_dir_size, &runtime_dir_inodes);
        if (r < 0)
                return r;

        xsprintf(runtime_path, "/run/user/" UID_FMT, uid);
#else // 0
static int do_mount(const char *runtime_path, size_t runtime_dir_size, size_t runtime_dir_inodes, uid_t uid, gid_t gid) {
#endif // 0

        log_debug("Will mount %s owned by "UID_FMT":"GID_FMT, runtime_path, uid, gid);
        return user_mkdir_runtime_path(runtime_path, uid, gid, runtime_dir_size, runtime_dir_inodes);
}

#if 0 /// elogind already has the runtime path
static int do_umount(const char *user) {
        char runtime_path[sizeof("/run/user") + DECIMAL_STR_MAX(uid_t)];
        uid_t uid;
        int r;

        /* The user may be already removed. So, first try to parse the string by parse_uid(),
         * and if it fails, fall back to get_user_creds(). */
        if (parse_uid(user, &uid) < 0) {
                r = get_user_creds(&user, &uid, NULL, NULL, NULL, 0);
                if (r < 0)
                        return log_error_errno(r,
                                               r == -ESRCH ? "No such user \"%s\"" :
                                               r == -ENOMSG ? "UID \"%s\" is invalid or has an invalid main group"
                                                            : "Failed to look up user \"%s\": %m",
                                               user);
        }

        xsprintf(runtime_path, "/run/user/" UID_FMT, uid);
#else // 0
static int do_umount(const char *runtime_path) {
#endif // 0

        log_debug("Will remove %s", runtime_path);
        return user_remove_runtime_path(runtime_path);
}

#if 0 /// elogind does this internally as we have no unit chain being init.
static int run(int argc, char *argv[]) {
        int r;

        log_parse_environment();
        log_open();

        if (argc != 3)
                return log_error_errno(SYNTHETIC_ERRNO(EINVAL),
                                       "This program takes two arguments.");
        if (!STR_IN_SET(argv[1], "start", "stop"))
                return log_error_errno(SYNTHETIC_ERRNO(EINVAL),
                                       "First argument must be either \"start\" or \"stop\".");

        umask(0022);

        r = mac_selinux_init();
        if (r < 0)
                return r;

#else // 0
int user_runtime_dir(const char *verb, User *u) {
        int r = 0;

        assert_se(verb);
        assert_se(u);
        assert_se(u->manager);

#endif // 0

#if 0 /// elogind has more information and can do this more conveniently
        if (streq(argv[1], "start"))
                return do_mount(argv[2]);
        if (streq(argv[1], "stop"))
                return do_umount(argv[2]);
        assert_not_reached();
#else // 0
        if (streq(verb, "start"))
                r = do_mount(u->runtime_path, u->manager->runtime_dir_size, u->manager->runtime_dir_inodes,
                             u->user_record->uid, u->user_record->gid);
        else if (streq(verb, "stop"))
                r = do_umount(u->runtime_path);
        else
                assert_not_reached();

        return r;
#endif // 0
}

#if 0 /// No main function needed in elogind
DEFINE_MAIN_FUNCTION(run);
#endif // 0
