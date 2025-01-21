/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <sys/prctl.h>

#include "fd-util.h"
#include "format-util.h"
#include "missing_sched.h"
#include "namespace-util.h"
#include "nsresource.h"
#include "process-util.h"
#include "varlink.h"

static int make_pid_name(char **ret) {
        char comm[TASK_COMM_LEN];

        assert(ret);

        if (prctl(PR_GET_NAME, comm) < 0)
                return -errno;

        /* So the namespace name should be 16 chars at max (because we want that it is usable in usernames,
         * which have a limit of 31 chars effectively, and the nsresourced service wants to prefix/suffix
         * some bits). But it also should be unique if we are called multiple times in a row. Hence we take
         * the "comm" name (which is 15 chars), and suffix it with the UID, possibly overriding the end. */
        assert_cc(TASK_COMM_LEN == 15 + 1);

        char spid[DECIMAL_STR_MAX(pid_t)];
        xsprintf(spid, PID_FMT, getpid_cached());

        assert(strlen(spid) <= 16);
        strshorten(comm, 16 - strlen(spid));

        _cleanup_free_ char *s = strjoin(comm, spid);
        if (!s)
                return -ENOMEM;

        *ret = TAKE_PTR(s);
        return 0;
}

int nsresource_allocate_userns(const char *name, uint64_t size) {
        _cleanup_(varlink_unrefp) Varlink *vl = NULL;
        _cleanup_close_ int userns_fd = -EBADF;
        _cleanup_free_ char *_name = NULL;
        const char *error_id;
        int r, userns_fd_idx;

        /* Allocate a new dynamic user namespace via the userdb registry logic */

        if (!name) {
                r = make_pid_name(&_name);
                if (r < 0)
                        return r;

                name = _name;
        }

        if (size <= 0 || size > UINT64_C(0x100000000)) /* Note: the server actually only allows allocating 1 or 64K right now */
                return -EINVAL;

        r = varlink_connect_address(&vl, "/run/systemd/io.systemd.NamespaceResource");
        if (r < 0)
                return log_debug_errno(r, "Failed to connect to namespace resource manager: %m");

        r = varlink_set_allow_fd_passing_output(vl, true);
        if (r < 0)
                return log_debug_errno(r, "Failed to enable varlink fd passing for write: %m");

        userns_fd = userns_acquire_empty();
        if (userns_fd < 0)
                return log_debug_errno(userns_fd, "Failed to acquire empty user namespace: %m");

        userns_fd_idx = varlink_push_dup_fd(vl, userns_fd);
        if (userns_fd_idx < 0)
                return log_debug_errno(userns_fd_idx, "Failed to push userns fd into varlink connection: %m");

        JsonVariant *reply = NULL;
        r = varlink_callb(vl,
                          "io.elogind.NamespaceResource.AllocateUserRange",
                          &reply,
                          &error_id,
                          JSON_BUILD_OBJECT(
                                          JSON_BUILD_PAIR("name", JSON_BUILD_STRING(name)),
                                          JSON_BUILD_PAIR("size", JSON_BUILD_UNSIGNED(size)),
                                          JSON_BUILD_PAIR("userNamespaceFileDescriptor", JSON_BUILD_UNSIGNED(userns_fd_idx))));
        if (r < 0)
                return log_debug_errno(r, "Failed to call AllocateUserRange() varlink call: %m");
        if (error_id)
                return log_debug_errno(varlink_error_to_errno(error_id, reply), "Failed to allocate user namespace with %" PRIu64 " users: %s", size, error_id);

        return TAKE_FD(userns_fd);
}

int nsresource_register_userns(const char *name, int userns_fd) {
        _cleanup_(varlink_unrefp) Varlink *vl = NULL;
        _cleanup_close_ int _userns_fd = -EBADF;
        _cleanup_free_ char *_name = NULL;
        const char *error_id;
        int r, userns_fd_idx;

        /* Register the specified user namespace with userbd. */

        if (!name) {
                r = make_pid_name(&_name);
                if (r < 0)
                        return r;

                name = _name;
        }

        if (userns_fd < 0) {
                _userns_fd = namespace_open_by_type(NAMESPACE_USER);
                if (_userns_fd < 0)
                        return -errno;

                userns_fd = _userns_fd;
        }

        r = varlink_connect_address(&vl, "/run/systemd/io.systemd.NamespaceResource");
        if (r < 0)
                return log_debug_errno(r, "Failed to connect to namespace resource manager: %m");

        r = varlink_set_allow_fd_passing_output(vl, true);
        if (r < 0)
                return log_debug_errno(r, "Failed to enable varlink fd passing for write: %m");

        userns_fd_idx = varlink_push_dup_fd(vl, userns_fd);
        if (userns_fd_idx < 0)
                return log_debug_errno(userns_fd_idx, "Failed to push userns fd into varlink connection: %m");

        JsonVariant *reply = NULL;
        r = varlink_callb(vl,
                          "io.elogind.NamespaceResource.RegisterUserNamespace",
                          &reply,
                          &error_id,
                          JSON_BUILD_OBJECT(
                                          JSON_BUILD_PAIR("name", JSON_BUILD_STRING(name)),
                                          JSON_BUILD_PAIR("userNamespaceFileDescriptor", JSON_BUILD_UNSIGNED(userns_fd_idx))));
        if (r < 0)
                return log_debug_errno(r, "Failed to call RegisterUserNamespace() varlink call: %m");
        if (error_id)
                return log_debug_errno(varlink_error_to_errno(error_id, reply), "Failed to register user namespace: %s", error_id);

        return 0;
}

int nsresource_add_mount(int userns_fd, int mount_fd) {
        _cleanup_(varlink_unrefp) Varlink *vl = NULL;
        int r, userns_fd_idx, mount_fd_idx;
        const char *error_id;

        assert(mount_fd >= 0);

        if (userns_fd < 0) {
                int _userns_fd = namespace_open_by_type(NAMESPACE_USER);
                if (_userns_fd < 0)
                        return -errno;

                userns_fd = _userns_fd;
        }

        r = varlink_connect_address(&vl, "/run/systemd/io.systemd.NamespaceResource");
        if (r < 0)
                return log_error_errno(r, "Failed to connect to namespace resource manager: %m");

        r = varlink_set_allow_fd_passing_output(vl, true);
        if (r < 0)
                return log_error_errno(r, "Failed to enable varlink fd passing for write: %m");

        userns_fd_idx = varlink_push_dup_fd(vl, userns_fd);
        if (userns_fd_idx < 0)
                return log_error_errno(userns_fd_idx, "Failed to push userns fd into varlink connection: %m");

        mount_fd_idx = varlink_push_dup_fd(vl, mount_fd);
        if (mount_fd_idx < 0)
                return log_error_errno(mount_fd_idx, "Failed to push mount fd into varlink connection: %m");

        JsonVariant *reply = NULL;
        r = varlink_callb(vl,
                          "io.elogind.NamespaceResource.AddMountToUserNamespace",
                          &reply,
                          &error_id,
                          JSON_BUILD_OBJECT(
                                          JSON_BUILD_PAIR("userNamespaceFileDescriptor", JSON_BUILD_UNSIGNED(userns_fd_idx)),
                                          JSON_BUILD_PAIR("mountFileDescriptor", JSON_BUILD_UNSIGNED(mount_fd_idx))));
        if (r < 0)
                return log_error_errno(r, "Failed to call AddMountToUserNamespace() varlink call: %m");
        if (streq_ptr(error_id, "io.elogind.NamespaceResource.UserNamespaceNotRegistered")) {
                log_notice("User namespace has not been allocated via namespace resource registry, not adding mount to registration.");
                return 0;
        }
        if (error_id)
                return log_error_errno(varlink_error_to_errno(error_id, reply), "Failed to mount image: %s", error_id);

        return 1;
}

int nsresource_add_cgroup(int userns_fd, int cgroup_fd) {
        _cleanup_(varlink_unrefp) Varlink *vl = NULL;
        _cleanup_close_ int _userns_fd = -EBADF;
        int r, userns_fd_idx, cgroup_fd_idx;
        const char *error_id;

        assert(cgroup_fd >= 0);

        if (userns_fd < 0) {
                _userns_fd = namespace_open_by_type(NAMESPACE_USER);
                if (_userns_fd < 0)
                        return -errno;

                userns_fd = _userns_fd;
        }

        r = varlink_connect_address(&vl, "/run/systemd/io.systemd.NamespaceResource");
        if (r < 0)
                return log_debug_errno(r, "Failed to connect to namespace resource manager: %m");

        r = varlink_set_allow_fd_passing_output(vl, true);
        if (r < 0)
                return log_debug_errno(r, "Failed to enable varlink fd passing for write: %m");

        userns_fd_idx = varlink_push_dup_fd(vl, userns_fd);
        if (userns_fd_idx < 0)
                return log_debug_errno(userns_fd_idx, "Failed to push userns fd into varlink connection: %m");

        cgroup_fd_idx = varlink_push_dup_fd(vl, cgroup_fd);
        if (cgroup_fd_idx < 0)
                return log_debug_errno(userns_fd_idx, "Failed to push cgroup fd into varlink connection: %m");

        JsonVariant *reply = NULL;
        r = varlink_callb(vl,
                          "io.elogind.NamespaceResource.AddControlGroupToUserNamespace",
                          &reply,
                          &error_id,
                          JSON_BUILD_OBJECT(
                                          JSON_BUILD_PAIR("userNamespaceFileDescriptor", JSON_BUILD_UNSIGNED(userns_fd_idx)),
                                          JSON_BUILD_PAIR("controlGroupFileDescriptor", JSON_BUILD_UNSIGNED(cgroup_fd_idx))));
        if (r < 0)
                return log_debug_errno(r, "Failed to call AddControlGroupToUserNamespace() varlink call: %m");
        if (streq_ptr(error_id, "io.elogind.NamespaceResource.UserNamespaceNotRegistered")) {
                log_notice("User namespace has not been allocated via namespace resource registry, not adding cgroup to registration.");
                return 0;
        }
        if (error_id)
                return log_debug_errno(varlink_error_to_errno(error_id, reply), "Failed to add cgroup to user namespace: %s", error_id);

        return 1;
}

int nsresource_add_netif(
                int userns_fd,
                int netns_fd,
                const char *namespace_ifname,
                char **ret_host_ifname,
                char **ret_namespace_ifname) {

        _cleanup_close_ int _userns_fd = -EBADF, _netns_fd = -EBADF;
        _cleanup_(varlink_unrefp) Varlink *vl = NULL;
        int r, userns_fd_idx, netns_fd_idx;
        const char *error_id;

        if (userns_fd < 0) {
                _userns_fd = namespace_open_by_type(NAMESPACE_USER);
                if (_userns_fd < 0)
                        return -errno;

                userns_fd = _userns_fd;
        }

        if (netns_fd < 0) {
                _netns_fd = namespace_open_by_type(NAMESPACE_NET);
                if (_netns_fd < 0)
                        return -errno;

                netns_fd = _netns_fd;
        }

        r = varlink_connect_address(&vl, "/run/systemd/io.systemd.NamespaceResource");
        if (r < 0)
                return log_debug_errno(r, "Failed to connect to namespace resource manager: %m");

        r = varlink_set_allow_fd_passing_output(vl, true);
        if (r < 0)
                return log_debug_errno(r, "Failed to enable varlink fd passing for write: %m");

        userns_fd_idx = varlink_push_dup_fd(vl, userns_fd);
        if (userns_fd_idx < 0)
                return log_debug_errno(userns_fd_idx, "Failed to push userns fd into varlink connection: %m");

        netns_fd_idx = varlink_push_dup_fd(vl, netns_fd);
        if (netns_fd_idx < 0)
                return log_debug_errno(netns_fd_idx, "Failed to push netns fd into varlink connection: %m");

        JsonVariant *reply = NULL;
        r = varlink_callb(vl,
                          "io.elogind.NamespaceResource.AddNetworkToUserNamespace",
                          &reply,
                          &error_id,
                          JSON_BUILD_OBJECT(
                                          JSON_BUILD_PAIR("userNamespaceFileDescriptor", JSON_BUILD_UNSIGNED(userns_fd_idx)),
                                          JSON_BUILD_PAIR("networkNamespaceFileDescriptor", JSON_BUILD_UNSIGNED(netns_fd_idx)),
                                          JSON_BUILD_PAIR("mode", JSON_BUILD_CONST_STRING("veth")),
                                          JSON_BUILD_PAIR_CONDITION(namespace_ifname, "namespaceInterfaceName", JSON_BUILD_STRING(namespace_ifname))));
        if (r < 0)
                return log_debug_errno(r, "Failed to call AddNetworkToUserNamespace() varlink call: %m");
        if (streq_ptr(error_id, "io.elogind.NamespaceResource.UserNamespaceNotRegistered")) {
                log_notice("User namespace has not been allocated via namespace resource registry, not adding network to registration.");
                return 0;
        }
        if (error_id)
                return log_debug_errno(varlink_error_to_errno(error_id, reply), "Failed to add network to user namespace: %s", error_id);

        _cleanup_free_ char *host_interface_name = NULL, *namespace_interface_name = NULL;
        r = json_dispatch(
                        reply,
                        (const JsonDispatch[]) {
                                { "hostInterfaceName",      JSON_VARIANT_STRING, json_dispatch_string, PTR_TO_SIZE(&host_interface_name)      },
                                { "namespaceInterfaceName", JSON_VARIANT_STRING, json_dispatch_string, PTR_TO_SIZE(&namespace_interface_name) },
                        },
                        JSON_ALLOW_EXTENSIONS,
                        /* userdata= */ NULL);

        if (ret_host_ifname)
                *ret_host_ifname = TAKE_PTR(host_interface_name);
        if (ret_namespace_ifname)
                *ret_namespace_ifname = TAKE_PTR(namespace_interface_name);

        return 1;
}
