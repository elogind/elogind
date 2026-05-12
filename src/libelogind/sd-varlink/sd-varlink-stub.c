/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>

#include "sd-event.h"
#include "sd-json.h"
#include "sd-varlink.h"

#include "alloc-util.h"
#include "macro.h"
#include "string-util.h"

_public_ int sd_varlink_connect_address(sd_varlink **ret, const char *address) {
        assert_return(ret, -EINVAL);
        assert_return(address, -EINVAL);

        *ret = NULL;
        return -EOPNOTSUPP;
}

_public_ int sd_varlink_connect_exec(sd_varlink **ret, const char *command, char **argv) {
        assert_return(ret, -EINVAL);
        assert_return(command, -EINVAL);

        *ret = NULL;
        return -EOPNOTSUPP;
}

_public_ int sd_varlink_connect_url(sd_varlink **ret, const char *url) {
        assert_return(ret, -EINVAL);
        assert_return(url, -EINVAL);

        *ret = NULL;
        return -EOPNOTSUPP;
}

_public_ int sd_varlink_connect_fd(sd_varlink **ret, int fd) {
        assert_return(ret, -EINVAL);
        assert_return(fd >= 0, -EBADF);

        *ret = NULL;
        return -EOPNOTSUPP;
}

_public_ int sd_varlink_connect_fd_pair(
                sd_varlink **ret,
                int input_fd,
                int output_fd,
                const struct ucred *override_ucred) {

        assert_return(ret, -EINVAL);
        assert_return(input_fd >= 0, -EBADF);
        assert_return(output_fd >= 0, -EBADF);

        *ret = NULL;
        return -EOPNOTSUPP;
}

_public_ sd_varlink* sd_varlink_ref(sd_varlink *link) {
        return link;
}

_public_ sd_varlink* sd_varlink_unref(sd_varlink *v) {
        return NULL;
}

_public_ int sd_varlink_get_fd(sd_varlink *v) {
        assert_return(v, -EINVAL);

        return -EOPNOTSUPP;
}

_public_ int sd_varlink_get_events(sd_varlink *v) {
        assert_return(v, -EINVAL);

        return -EOPNOTSUPP;
}

_public_ int sd_varlink_get_timeout(sd_varlink *v, uint64_t *ret) {
        assert_return(v, -EINVAL);
        assert_return(ret, -EINVAL);

        return -EOPNOTSUPP;
}

_public_ int sd_varlink_attach_event(sd_varlink *v, sd_event *e, int64_t priority) {
        assert_return(v, -EINVAL);

        return -EOPNOTSUPP;
}

_public_ void sd_varlink_detach_event(sd_varlink *v) {
}

_public_ sd_event *sd_varlink_get_event(sd_varlink *v) {
        return NULL;
}

_public_ int sd_varlink_process(sd_varlink *v) {
        assert_return(v, -EINVAL);

        return -EOPNOTSUPP;
}

_public_ int sd_varlink_wait(sd_varlink *v, uint64_t timeout) {
        assert_return(v, -EINVAL);

        return -EOPNOTSUPP;
}

_public_ int sd_varlink_is_idle(sd_varlink *v) {
        assert_return(v, -EINVAL);

        return -EOPNOTSUPP;
}

_public_ int sd_varlink_flush(sd_varlink *v) {
        assert_return(v, -EINVAL);

        return -EOPNOTSUPP;
}

_public_ int sd_varlink_close(sd_varlink *v) {
        assert_return(v, -EINVAL);

        return 0;
}

_public_ sd_varlink* sd_varlink_flush_close_unref(sd_varlink *v) {
        return NULL;
}

_public_ sd_varlink* sd_varlink_close_unref(sd_varlink *v) {
        return NULL;
}

_public_ int sd_varlink_send(sd_varlink *v, const char *method, sd_json_variant *parameters) {
        assert_return(v, -EINVAL);
        assert_return(method, -EINVAL);

        return -EOPNOTSUPP;
}

_public_ int sd_varlink_sendb(sd_varlink *v, const char *method, ...) {
        assert_return(v, -EINVAL);
        assert_return(method, -EINVAL);

        return -EOPNOTSUPP;
}

_public_ int sd_varlink_call_full(
                sd_varlink *v,
                const char *method,
                sd_json_variant *parameters,
                sd_json_variant **ret_parameters,
                const char **ret_error_id,
                sd_varlink_reply_flags_t *ret_flags) {

        assert_return(v, -EINVAL);
        assert_return(method, -EINVAL);

        if (ret_parameters)
                *ret_parameters = NULL;
        if (ret_error_id)
                *ret_error_id = NULL;
        if (ret_flags)
                *ret_flags = 0;

        return -EOPNOTSUPP;
}

_public_ int sd_varlink_call(
                sd_varlink *v,
                const char *method,
                sd_json_variant *parameters,
                sd_json_variant **ret_parameters,
                const char **ret_error_id) {

        return sd_varlink_call_full(v, method, parameters, ret_parameters, ret_error_id, NULL);
}

_public_ int sd_varlink_callb_ap(
                sd_varlink *v,
                const char *method,
                sd_json_variant **ret_parameters,
                const char **ret_error_id,
                sd_varlink_reply_flags_t *ret_flags,
                va_list ap) {

        return sd_varlink_call_full(v, method, NULL, ret_parameters, ret_error_id, ret_flags);
}

_public_ int sd_varlink_callb_full(
                sd_varlink *v,
                const char *method,
                sd_json_variant **ret_parameters,
                const char **ret_error_id,
                sd_varlink_reply_flags_t *ret_flags,
                ...) {

        return sd_varlink_call_full(v, method, NULL, ret_parameters, ret_error_id, ret_flags);
}

_public_ int sd_varlink_callb(
                sd_varlink *v,
                const char *method,
                sd_json_variant **ret_parameters,
                const char **ret_error_id,
                ...) {

        return sd_varlink_call_full(v, method, NULL, ret_parameters, ret_error_id, NULL);
}

_public_ int sd_varlink_collect_full(
                sd_varlink *v,
                const char *method,
                sd_json_variant *parameters,
                sd_json_variant **ret_parameters,
                const char **ret_error_id,
                sd_varlink_reply_flags_t *ret_flags) {

        return sd_varlink_call_full(v, method, parameters, ret_parameters, ret_error_id, ret_flags);
}

_public_ int sd_varlink_collect(
                sd_varlink *v,
                const char *method,
                sd_json_variant *parameters,
                sd_json_variant **ret_parameters,
                const char **ret_error_id) {

        return sd_varlink_call_full(v, method, parameters, ret_parameters, ret_error_id, NULL);
}

_public_ int sd_varlink_collectb(
                sd_varlink *v,
                const char *method,
                sd_json_variant **ret_parameters,
                const char **ret_error_id,
                ...) {

        return sd_varlink_call_full(v, method, NULL, ret_parameters, ret_error_id, NULL);
}

_public_ int sd_varlink_invoke(sd_varlink *v, const char *method, sd_json_variant *parameters) {
        return sd_varlink_send(v, method, parameters);
}

_public_ int sd_varlink_invokeb(sd_varlink *v, const char *method, ...) {
        return sd_varlink_send(v, method, NULL);
}

_public_ int sd_varlink_observe(sd_varlink *v, const char *method, sd_json_variant *parameters) {
        return sd_varlink_send(v, method, parameters);
}

_public_ int sd_varlink_observeb(sd_varlink *v, const char *method, ...) {
        return sd_varlink_send(v, method, NULL);
}

_public_ int sd_varlink_reply(sd_varlink *v, sd_json_variant *parameters) {
        assert_return(v, -EINVAL);

        return -EOPNOTSUPP;
}

_public_ int sd_varlink_replyb(sd_varlink *v, ...) {
        assert_return(v, -EINVAL);

        return -EOPNOTSUPP;
}

_public_ int sd_varlink_error(sd_varlink *v, const char *error_id, sd_json_variant *parameters) {
        assert_return(v, -EINVAL);
        assert_return(error_id, -EINVAL);

        return -EOPNOTSUPP;
}

_public_ int sd_varlink_errorb(sd_varlink *v, const char *error_id, ...) {
        assert_return(v, -EINVAL);
        assert_return(error_id, -EINVAL);

        return -EOPNOTSUPP;
}

_public_ int sd_varlink_error_invalid_parameter(sd_varlink *v, sd_json_variant *parameters) {
        assert_return(v, -EINVAL);

        return -EOPNOTSUPP;
}

_public_ int sd_varlink_error_invalid_parameter_name(sd_varlink *v, const char *name) {
        assert_return(v, -EINVAL);
        assert_return(name, -EINVAL);

        return -EOPNOTSUPP;
}

_public_ int sd_varlink_error_errno(sd_varlink *v, int error) {
        assert_return(v, -EINVAL);

        return -EOPNOTSUPP;
}

_public_ int sd_varlink_notify(sd_varlink *v, sd_json_variant *parameters) {
        assert_return(v, -EINVAL);

        return -EOPNOTSUPP;
}

_public_ int sd_varlink_notifyb(sd_varlink *v, ...) {
        assert_return(v, -EINVAL);

        return -EOPNOTSUPP;
}

_public_ int sd_varlink_dispatch_again(sd_varlink *v) {
        assert_return(v, -EINVAL);

        return -EOPNOTSUPP;
}

_public_ int sd_varlink_get_current_parameters(sd_varlink *v, sd_json_variant **ret) {
        assert_return(v, -EINVAL);
        assert_return(ret, -EINVAL);

        *ret = NULL;
        return -EOPNOTSUPP;
}

_public_ int sd_varlink_dispatch(
                sd_varlink *v,
                sd_json_variant *parameters,
                const sd_json_dispatch_field table[],
                void *userdata) {

        assert_return(v, -EINVAL);

        return -EOPNOTSUPP;
}

_public_ int sd_varlink_push_fd(sd_varlink *v, int fd) {
        assert_return(v, -EINVAL);
        assert_return(fd >= 0, -EBADF);

        return -EOPNOTSUPP;
}

_public_ int sd_varlink_push_dup_fd(sd_varlink *v, int fd) {
        assert_return(v, -EINVAL);
        assert_return(fd >= 0, -EBADF);

        return -EOPNOTSUPP;
}

_public_ int sd_varlink_peek_fd(sd_varlink *v, size_t i) {
        assert_return(v, -EINVAL);

        return -EOPNOTSUPP;
}

_public_ int sd_varlink_peek_dup_fd(sd_varlink *v, size_t i) {
        assert_return(v, -EINVAL);

        return -EOPNOTSUPP;
}

_public_ int sd_varlink_take_fd(sd_varlink *v, size_t i) {
        assert_return(v, -EINVAL);

        return -EOPNOTSUPP;
}

_public_ int sd_varlink_set_allow_fd_passing_input(sd_varlink *v, int b) {
        assert_return(v, -EINVAL);

        return -EOPNOTSUPP;
}

_public_ int sd_varlink_set_allow_fd_passing_output(sd_varlink *v, int b) {
        assert_return(v, -EINVAL);

        return -EOPNOTSUPP;
}

_public_ int sd_varlink_bind_reply(sd_varlink *v, sd_varlink_reply_t reply) {
        assert_return(v, -EINVAL);

        return -EOPNOTSUPP;
}

_public_ void* sd_varlink_set_userdata(sd_varlink *v, void *userdata) {
        return NULL;
}

_public_ void* sd_varlink_get_userdata(sd_varlink *v) {
        return NULL;
}

_public_ int sd_varlink_get_peer_uid(sd_varlink *v, uid_t *ret) {
        assert_return(v, -EINVAL);
        assert_return(ret, -EINVAL);

        return -EOPNOTSUPP;
}

_public_ int sd_varlink_get_peer_gid(sd_varlink *v, gid_t *ret) {
        assert_return(v, -EINVAL);
        assert_return(ret, -EINVAL);

        return -EOPNOTSUPP;
}

_public_ int sd_varlink_get_peer_pid(sd_varlink *v, pid_t *ret) {
        assert_return(v, -EINVAL);
        assert_return(ret, -EINVAL);

        return -EOPNOTSUPP;
}

_public_ int sd_varlink_get_peer_pidfd(sd_varlink *v) {
        assert_return(v, -EINVAL);

        return -EOPNOTSUPP;
}

_public_ int sd_varlink_set_relative_timeout(sd_varlink *v, uint64_t usec) {
        assert_return(v, -EINVAL);

        return -EOPNOTSUPP;
}

_public_ sd_varlink_server* sd_varlink_get_server(sd_varlink *v) {
        return NULL;
}

_public_ int sd_varlink_set_description(sd_varlink *v, const char *d) {
        assert_return(v, -EINVAL);

        return -EOPNOTSUPP;
}

_public_ int sd_varlink_set_input_sensitive(sd_varlink *v) {
        assert_return(v, -EINVAL);

        return -EOPNOTSUPP;
}

_public_ int sd_varlink_server_new(sd_varlink_server **ret, sd_varlink_server_flags_t flags) {
        assert_return(ret, -EINVAL);

        *ret = NULL;
        return -EOPNOTSUPP;
}

_public_ sd_varlink_server* sd_varlink_server_ref(sd_varlink_server *s) {
        return s;
}

_public_ sd_varlink_server* sd_varlink_server_unref(sd_varlink_server *s) {
        return NULL;
}

_public_ int sd_varlink_server_set_info(
                sd_varlink_server *s,
                const char *vendor,
                const char *product,
                const char *version,
                const char *url) {

        assert_return(s, -EINVAL);

        return -EOPNOTSUPP;
}

_public_ int sd_varlink_server_listen_address(sd_varlink_server *s, const char *address, mode_t mode) {
        assert_return(s, -EINVAL);
        assert_return(address, -EINVAL);

        return -EOPNOTSUPP;
}

_public_ int sd_varlink_server_listen_fd(sd_varlink_server *s, int fd) {
        assert_return(s, -EINVAL);
        assert_return(fd >= 0, -EBADF);

        return -EOPNOTSUPP;
}

_public_ int sd_varlink_server_listen_auto(sd_varlink_server *s) {
        assert_return(s, -EINVAL);

        return -EOPNOTSUPP;
}

_public_ int sd_varlink_server_add_connection(sd_varlink_server *s, int fd, sd_varlink **ret) {
        assert_return(s, -EINVAL);
        assert_return(fd >= 0, -EBADF);

        if (ret)
                *ret = NULL;

        return -EOPNOTSUPP;
}

_public_ int sd_varlink_server_add_connection_pair(
                sd_varlink_server *s,
                int input_fd,
                int output_fd,
                const struct ucred *ucred_override,
                sd_varlink **ret) {

        assert_return(s, -EINVAL);
        assert_return(input_fd >= 0, -EBADF);
        assert_return(output_fd >= 0, -EBADF);

        if (ret)
                *ret = NULL;

        return -EOPNOTSUPP;
}

_public_ int sd_varlink_server_add_connection_stdio(sd_varlink_server *s, sd_varlink **ret) {
        assert_return(s, -EINVAL);

        if (ret)
                *ret = NULL;

        return -EOPNOTSUPP;
}

_public_ int sd_varlink_server_bind_method(
                sd_varlink_server *s,
                const char *method,
                sd_varlink_method_t callback) {

        assert_return(s, -EINVAL);
        assert_return(method, -EINVAL);

        return -EOPNOTSUPP;
}

_public_ int sd_varlink_server_bind_method_many_internal(sd_varlink_server *s, ...) {
        assert_return(s, -EINVAL);

        return -EOPNOTSUPP;
}

_public_ int sd_varlink_server_bind_connect(sd_varlink_server *s, sd_varlink_connect_t connect) {
        assert_return(s, -EINVAL);

        return -EOPNOTSUPP;
}

_public_ int sd_varlink_server_bind_disconnect(sd_varlink_server *s, sd_varlink_disconnect_t disconnect) {
        assert_return(s, -EINVAL);

        return -EOPNOTSUPP;
}

_public_ int sd_varlink_server_add_interface(
                sd_varlink_server *s,
                const sd_varlink_interface *interface) {

        assert_return(s, -EINVAL);
        assert_return(interface, -EINVAL);

        return -EOPNOTSUPP;
}

_public_ int sd_varlink_server_add_interface_many_internal(sd_varlink_server *s, ...) {
        assert_return(s, -EINVAL);

        return -EOPNOTSUPP;
}

_public_ void* sd_varlink_server_set_userdata(sd_varlink_server *s, void *userdata) {
        return NULL;
}

_public_ void* sd_varlink_server_get_userdata(sd_varlink_server *s) {
        return NULL;
}

_public_ int sd_varlink_server_attach_event(sd_varlink_server *v, sd_event *e, int64_t priority) {
        assert_return(v, -EINVAL);

        return -EOPNOTSUPP;
}

_public_ int sd_varlink_server_detach_event(sd_varlink_server *v) {
        assert_return(v, -EINVAL);

        return -EOPNOTSUPP;
}

_public_ sd_event* sd_varlink_server_get_event(sd_varlink_server *v) {
        return NULL;
}

_public_ int sd_varlink_server_loop_auto(sd_varlink_server *server) {
        assert_return(server, -EINVAL);

        return -EOPNOTSUPP;
}

_public_ int sd_varlink_server_shutdown(sd_varlink_server *server) {
        assert_return(server, -EINVAL);

        return 0;
}

_public_ int sd_varlink_server_set_exit_on_idle(sd_varlink_server *s, int b) {
        assert_return(s, -EINVAL);

        return -EOPNOTSUPP;
}

_public_ unsigned sd_varlink_server_connections_max(sd_varlink_server *s) {
        return 0;
}

_public_ unsigned sd_varlink_server_connections_per_uid_max(sd_varlink_server *s) {
        return 0;
}

_public_ int sd_varlink_server_set_connections_per_uid_max(sd_varlink_server *s, unsigned m) {
        assert_return(s, -EINVAL);

        return -EOPNOTSUPP;
}

_public_ int sd_varlink_server_set_connections_max(sd_varlink_server *s, unsigned m) {
        assert_return(s, -EINVAL);

        return -EOPNOTSUPP;
}

_public_ unsigned sd_varlink_server_current_connections(sd_varlink_server *s) {
        return 0;
}

_public_ int sd_varlink_server_set_description(sd_varlink_server *s, const char *description) {
        assert_return(s, -EINVAL);

        return -EOPNOTSUPP;
}

_public_ int sd_varlink_invocation(sd_varlink_invocation_flags_t flags) {
        return -EOPNOTSUPP;
}

_public_ int sd_varlink_error_to_errno(const char *error, sd_json_variant *parameters) {
        static const struct {
                const char *error;
                int value;
        } table[] = {
                { SD_VARLINK_ERROR_DISCONNECTED,           -ECONNRESET    },
                { SD_VARLINK_ERROR_TIMEOUT,                -ETIMEDOUT     },
                { SD_VARLINK_ERROR_PROTOCOL,               -EPROTO        },
                { SD_VARLINK_ERROR_INTERFACE_NOT_FOUND,    -EADDRNOTAVAIL },
                { SD_VARLINK_ERROR_METHOD_NOT_FOUND,       -ENXIO         },
                { SD_VARLINK_ERROR_METHOD_NOT_IMPLEMENTED, -ENOTTY        },
                { SD_VARLINK_ERROR_INVALID_PARAMETER,      -EINVAL        },
                { SD_VARLINK_ERROR_PERMISSION_DENIED,      -EACCES        },
                { SD_VARLINK_ERROR_EXPECTED_MORE,          -EBADE         },
        };

        if (!error)
                return 0;

        FOREACH_ELEMENT(t, table)
                if (streq(error, t->error))
                        return t->value;

        return -EBADR;
}

_public_ int sd_varlink_error_is_invalid_parameter(
                const char *error,
                sd_json_variant *parameter,
                const char *name) {

        if (!streq_ptr(error, SD_VARLINK_ERROR_INVALID_PARAMETER))
                return false;

        return !name;
}

_public_ int sd_varlink_idl_dump(
                FILE *f,
                const sd_varlink_interface *interface,
                sd_varlink_idl_format_flags_t flags,
                size_t cols) {

        assert_return(f, -EINVAL);
        assert_return(interface, -EINVAL);

        return -EOPNOTSUPP;
}

_public_ int sd_varlink_idl_format_full(
                const sd_varlink_interface *interface,
                sd_varlink_idl_format_flags_t flags,
                size_t cols,
                char **ret) {

        assert_return(interface, -EINVAL);
        assert_return(ret, -EINVAL);

        *ret = NULL;
        return -EOPNOTSUPP;
}

_public_ int sd_varlink_idl_format(const sd_varlink_interface *interface, char **ret) {
        return sd_varlink_idl_format_full(interface, 0, 0, ret);
}
