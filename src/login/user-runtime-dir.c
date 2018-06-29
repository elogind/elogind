/* SPDX-License-Identifier: LGPL-2.1+ */

//#include <stdint.h>
//#include <sys/mount.h>

//#include "fs-util.h"
//#include "label.h"
//#include "logind.h"
//#include "mkdir.h"
//#include "mount-util.h"
//#include "path-util.h"
//#include "rm-rf.h"
//#include "smack-util.h"
//#include "stdio-util.h"
//#include "string-util.h"
//#include "strv.h"
//#include "user-util.h"

static int gather_configuration(size_t *runtime_dir_size) {
        Manager m = {};
        int r;

        manager_reset_config(&m);

        r = manager_parse_config_file(&m);
        if (r < 0)
                log_warning_errno(r, "Failed to parse logind.conf: %m");

        *runtime_dir_size = m.runtime_dir_size;
        return 0;
}

static int user_mkdir_runtime_path(const char *runtime_path, uid_t uid, gid_t gid, size_t runtime_dir_size) {
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
                char options[sizeof("mode=0700,uid=,gid=,size=,smackfsroot=*")
                             + DECIMAL_STR_MAX(uid_t)
                             + DECIMAL_STR_MAX(gid_t)
                             + DECIMAL_STR_MAX(size_t)];

                xsprintf(options,
                         "mode=0700,uid=" UID_FMT ",gid=" GID_FMT ",size=%zu%s",
                         uid, gid, runtime_dir_size,
                         mac_smack_use() ? ",smackfsroot=*" : "");

                (void) mkdir_label(runtime_path, 0700);

                r = mount("tmpfs", runtime_path, "tmpfs", MS_NODEV|MS_NOSUID, options);
                if (r < 0) {
                        if (!IN_SET(errno, EPERM, EACCES)) {
                                r = log_error_errno(errno, "Failed to mount per-user tmpfs directory %s: %m", runtime_path);
                                goto fail;
                        }

                        log_debug_errno(errno, "Failed to mount per-user tmpfs directory %s.\n"
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
                log_error_errno(r, "Failed to remove runtime directory %s (before unmounting): %m", runtime_path);

        /* Ignore cases where the directory isn't mounted, as that's
         * quite possible, if we lacked the permissions to mount
         * something */
        r = umount2(runtime_path, MNT_DETACH);
        if (r < 0 && !IN_SET(errno, EINVAL, ENOENT))
                log_error_errno(errno, "Failed to unmount user runtime directory %s: %m", runtime_path);

        r = rm_rf(runtime_path, REMOVE_ROOT);
        if (r < 0)
                log_error_errno(r, "Failed to remove runtime directory %s (after unmounting): %m", runtime_path);

        return r;
}

static int do_mount(const char *runtime_path, uid_t uid, gid_t gid) {
        size_t runtime_dir_size;

        assert_se(gather_configuration(&runtime_dir_size) == 0);

        log_debug("Will mount %s owned by "UID_FMT":"GID_FMT, runtime_path, uid, gid);
        return user_mkdir_runtime_path(runtime_path, uid, gid, runtime_dir_size);
}

static int do_umount(const char *runtime_path) {
        log_debug("Will remove %s", runtime_path);
        return user_remove_runtime_path(runtime_path);
}

int main(int argc, char *argv[]) {
        const char *user;
        uid_t uid;
        gid_t gid;
        char runtime_path[sizeof("/run/user") + DECIMAL_STR_MAX(uid_t)];
        int r;

        log_parse_environment();
        log_open();

        if (argc != 3) {
                log_error("This program takes two arguments.");
                return EXIT_FAILURE;
        }
        if (!STR_IN_SET(argv[1], "start", "stop")) {
                log_error("First argument must be either \"start\" or \"stop\".");
                return EXIT_FAILURE;
        }

        umask(0022);

        user = argv[2];
        r = get_user_creds(&user, &uid, &gid, NULL, NULL);
        if (r < 0) {
                log_error_errno(r,
                                r == -ESRCH ? "No such user \"%s\"" :
                                r == -ENOMSG ? "UID \"%s\" is invalid or has an invalid main group"
                                             : "Failed to look up user \"%s\": %m",
                                user);
                return EXIT_FAILURE;
        }

        xsprintf(runtime_path, "/run/user/" UID_FMT, uid);

        if (streq(argv[1], "start"))
                r = do_mount(runtime_path, uid, gid);
        else if (streq(argv[1], "stop"))
                r = do_umount(runtime_path);
        else
                assert_not_reached("Unknown verb!");

        return r < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
