/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include "af-list.h"
#include "capability-util.h"
#include "cgroup-setup.h"
#include "escape.h"
#include "exec-credential.h"
#include "execute-serialize.h"
#include "hexdecoct.h"
#include "fd-util.h"
#include "fileio.h"
#include "in-addr-prefix-util.h"
#include "parse-helpers.h"
#include "parse-util.h"
#include "percent-util.h"
#include "process-util.h"
#include "rlimit-util.h"
#include "serialize.h"
#include "string-util.h"
#include "strv.h"

static int serialize_std_out_err(const ExecContext *c, FILE *f, int fileno) {
        char *key, *value;
        const char *type;

        assert(c);
        assert(f);
        assert(IN_SET(fileno, STDOUT_FILENO, STDERR_FILENO));

        type = fileno == STDOUT_FILENO ? "output" : "error";

        switch (fileno == STDOUT_FILENO ? c->std_output : c->std_error) {
        case EXEC_OUTPUT_NAMED_FD:
                key = strjoina("exec-context-std-", type, "-fd-name");
                value = c->stdio_fdname[fileno];

                break;

        case EXEC_OUTPUT_FILE:
                key = strjoina("exec-context-std-", type, "-file");
                value = c->stdio_file[fileno];

                break;

        case EXEC_OUTPUT_FILE_APPEND:
                key = strjoina("exec-context-std-", type, "-file-append");
                value = c->stdio_file[fileno];

                break;

        case EXEC_OUTPUT_FILE_TRUNCATE:
                key = strjoina("exec-context-std-", type, "-file-truncate");
                value = c->stdio_file[fileno];

                break;

        default:
                return 0;
        }

        return serialize_item(f, key, value);
}

static int exec_context_serialize(const ExecContext *c, FILE *f) {
        int r;

        assert(f);

        if (!c)
                return 0;

        r = serialize_strv(f, "exec-context-environment", c->environment);
        if (r < 0)
                return r;

        r = serialize_strv(f, "exec-context-environment-files", c->environment_files);
        if (r < 0)
                return r;

        r = serialize_strv(f, "exec-context-pass-environment", c->pass_environment);
        if (r < 0)
                return r;

        r = serialize_strv(f, "exec-context-unset-environment", c->unset_environment);
        if (r < 0)
                return r;

        r = serialize_item(f, "exec-context-working-directory", c->working_directory);
        if (r < 0)
                return r;

        r = serialize_item(f, "exec-context-root-directory", c->root_directory);
        if (r < 0)
                return r;

        r = serialize_item(f, "exec-context-root-image", c->root_image);
        if (r < 0)
                return r;

        if (c->root_image_options) {
                _cleanup_free_ char *options = NULL;

                LIST_FOREACH(mount_options, o, c->root_image_options) {
                        if (isempty(o->options))
                                continue;

                        _cleanup_free_ char *escaped = NULL;
                        escaped = shell_escape(o->options, ":");
                        if (!escaped)
                                return log_oom_debug();

                        if (!strextend(&options,
                                        " ",
                                        partition_designator_to_string(o->partition_designator),
                                               ":",
                                               escaped))
                                        return log_oom_debug();
                }

                r = serialize_item(f, "exec-context-root-image-options", options);
                if (r < 0)
                        return r;
        }

        r = serialize_item(f, "exec-context-root-verity", c->root_verity);
        if (r < 0)
                return r;

        r = serialize_item(f, "exec-context-root-hash-path", c->root_hash_path);
        if (r < 0)
                return r;

        r = serialize_item(f, "exec-context-root-hash-sig-path", c->root_hash_sig_path);
        if (r < 0)
                return r;

        r = serialize_item_hexmem(f, "exec-context-root-hash", c->root_hash, c->root_hash_size);
        if (r < 0)
                return r;

        r = serialize_item_base64mem(f, "exec-context-root-hash-sig", c->root_hash_sig, c->root_hash_sig_size);
        if (r < 0)
                return r;

        r = serialize_bool_elide(f, "exec-context-root-ephemeral", c->root_ephemeral);
        if (r < 0)
                return r;

        r = serialize_item_format(f, "exec-context-umask", "%04o", c->umask);
        if (r < 0)
                return r;

        r = serialize_bool_elide(f, "exec-context-non-blocking", c->non_blocking);
        if (r < 0)
                return r;

        r = serialize_item_tristate(f, "exec-context-private-mounts", c->private_mounts);
        if (r < 0)
                return r;

        r = serialize_item_tristate(f, "exec-context-memory-ksm", c->memory_ksm);
        if (r < 0)
                return r;

        r = serialize_bool_elide(f, "exec-context-private-tmp", c->private_tmp);
        if (r < 0)
                return r;

        r = serialize_bool_elide(f, "exec-context-private-devices", c->private_devices);
        if (r < 0)
                return r;

        r = serialize_bool_elide(f, "exec-context-protect-kernel-tunables", c->protect_kernel_tunables);
        if (r < 0)
                return r;

        r = serialize_bool_elide(f, "exec-context-protect-kernel-modules", c->protect_kernel_modules);
        if (r < 0)
                return r;

        r = serialize_bool_elide(f, "exec-context-protect-kernel-logs", c->protect_kernel_logs);
        if (r < 0)
                return r;

        r = serialize_bool_elide(f, "exec-context-protect-clock", c->protect_clock);
        if (r < 0)
                return r;

        r = serialize_bool_elide(f, "exec-context-protect-control-groups", c->protect_control_groups);
        if (r < 0)
                return r;

        r = serialize_bool_elide(f, "exec-context-private-network", c->private_network);
        if (r < 0)
                return r;

        r = serialize_bool_elide(f, "exec-context-private-users", c->private_users);
        if (r < 0)
                return r;

        r = serialize_bool_elide(f, "exec-context-private-ipc", c->private_ipc);
        if (r < 0)
                return r;

        r = serialize_bool_elide(f, "exec-context-remove-ipc", c->remove_ipc);
        if (r < 0)
                return r;

        r = serialize_item(f, "exec-context-protect-home", protect_home_to_string(c->protect_home));
        if (r < 0)
                return r;

        r = serialize_item(f, "exec-context-protect-system", protect_system_to_string(c->protect_system));
        if (r < 0)
                return r;

        if (c->mount_apivfs_set) {
                r = serialize_bool(f, "exec-context-mount-api-vfs", c->mount_apivfs);
                if (r < 0)
                        return r;
        }

        r = serialize_bool_elide(f, "exec-context-same-pgrp", c->same_pgrp);
        if (r < 0)
                return r;

        r = serialize_bool_elide(f, "exec-context-cpu-sched-reset-on-fork", c->cpu_sched_reset_on_fork);
        if (r < 0)
                return r;

        r = serialize_bool(f, "exec-context-ignore-sigpipe", c->ignore_sigpipe);
        if (r < 0)
                return r;

        r = serialize_bool_elide(f, "exec-context-memory-deny-write-execute", c->memory_deny_write_execute);
        if (r < 0)
                return r;

        r = serialize_bool_elide(f, "exec-context-restrict-realtime", c->restrict_realtime);
        if (r < 0)
                return r;

        r = serialize_bool_elide(f, "exec-context-restrict-suid-sgid", c->restrict_suid_sgid);
        if (r < 0)
                return r;

        r = serialize_item(f, "exec-context-keyring-mode", exec_keyring_mode_to_string(c->keyring_mode));
        if (r < 0)
                return r;

        r = serialize_bool_elide(f, "exec-context-protect-hostname", c->protect_hostname);
        if (r < 0)
                return r;

        r = serialize_item(f, "exec-context-protect-proc", protect_proc_to_string(c->protect_proc));
        if (r < 0)
                return r;

        r = serialize_item(f, "exec-context-proc-subset", proc_subset_to_string(c->proc_subset));
        if (r < 0)
                return r;

        r = serialize_item(f, "exec-context-runtime-directory-preserve-mode", exec_preserve_mode_to_string(c->runtime_directory_preserve_mode));
        if (r < 0)
                return r;

        for (ExecDirectoryType dt = 0; dt < _EXEC_DIRECTORY_TYPE_MAX; dt++) {
                _cleanup_free_ char *key = NULL, *value = NULL;

                key = strjoin("exec-context-directories-", exec_directory_type_to_string(dt));
                if (!key)
                        return log_oom_debug();

                if (asprintf(&value, "%04o", c->directories[dt].mode) < 0)
                        return log_oom_debug();

                for (size_t i = 0; i < c->directories[dt].n_items; i++) {
                        _cleanup_free_ char *path_escaped = NULL;

                        path_escaped = shell_escape(c->directories[dt].items[i].path, ":");
                        if (!path_escaped)
                                return log_oom_debug();

                        if (!strextend(&value, " ", path_escaped))
                                return log_oom_debug();

                        if (!strextend(&value, ":", yes_no(c->directories[dt].items[i].only_create)))
                                return log_oom_debug();

                        STRV_FOREACH(d, c->directories[dt].items[i].symlinks) {
                                _cleanup_free_ char *link_escaped = NULL;

                                link_escaped = shell_escape(*d, ":");
                                if (!link_escaped)
                                        return log_oom_debug();

                                if (!strextend(&value, ":", link_escaped))
                                        return log_oom_debug();
                        }
                }

                r = serialize_item(f, key, value);
                if (r < 0)
                        return r;
        }

        r = serialize_usec(f, "exec-context-timeout-clean-usec", c->timeout_clean_usec);
        if (r < 0)
                return r;

        if (c->nice_set) {
                r = serialize_item_format(f, "exec-context-nice", "%i", c->nice);
                if (r < 0)
                        return r;
        }

        r = serialize_bool_elide(f, "exec-context-working-directory-missing-ok", c->working_directory_missing_ok);
        if (r < 0)
                return r;

        r = serialize_bool_elide(f, "exec-context-working-directory-home", c->working_directory_home);
        if (r < 0)
                return r;

        if (c->oom_score_adjust_set) {
                r = serialize_item_format(f, "exec-context-oom-score-adjust", "%i", c->oom_score_adjust);
                if (r < 0)
                        return r;
        }

        if (c->coredump_filter_set) {
                r = serialize_item_format(f, "exec-context-coredump-filter", "%"PRIx64, c->coredump_filter);
                if (r < 0)
                        return r;
        }

        for (unsigned i = 0; i < RLIM_NLIMITS; i++) {
                _cleanup_free_ char *key = NULL, *limit = NULL;

                if (!c->rlimit[i])
                        continue;

                key = strjoin("exec-context-limit-", rlimit_to_string(i));
                if (!key)
                        return log_oom_debug();

                r = rlimit_format(c->rlimit[i], &limit);
                if (r < 0)
                        return r;

                r = serialize_item(f, key, limit);
                if (r < 0)
                        return r;
        }

        if (c->ioprio_set) {
                r = serialize_item_format(f, "exec-context-ioprio", "%d", c->ioprio);
                if (r < 0)
                        return r;
        }

        if (c->cpu_sched_set) {
                _cleanup_free_ char *policy_str = NULL;

                r = sched_policy_to_string_alloc(c->cpu_sched_policy, &policy_str);
                if (r < 0)
                        return r;

                r = serialize_item(f, "exec-context-cpu-scheduling-policy", policy_str);
                if (r < 0)
                        return r;

                r = serialize_item_format(f, "exec-context-cpu-scheduling-priority", "%i", c->cpu_sched_priority);
                if (r < 0)
                        return r;

                r = serialize_bool_elide(f, "exec-context-cpu-scheduling-reset-on-fork", c->cpu_sched_reset_on_fork);
                if (r < 0)
                        return r;
        }

        if (c->cpu_set.set) {
                _cleanup_free_ char *affinity = NULL;

                affinity = cpu_set_to_range_string(&c->cpu_set);
                if (!affinity)
                        return log_oom_debug();

                r = serialize_item(f, "exec-context-cpu-affinity", affinity);
                if (r < 0)
                        return r;
        }

        if (mpol_is_valid(numa_policy_get_type(&c->numa_policy))) {
                _cleanup_free_ char *nodes = NULL;

                nodes = cpu_set_to_range_string(&c->numa_policy.nodes);
                if (!nodes)
                        return log_oom_debug();

                if (nodes) {
                        r = serialize_item(f, "exec-context-numa-mask", nodes);
                        if (r < 0)
                                return r;
                }

                r = serialize_item_format(f, "exec-context-numa-policy", "%d", c->numa_policy.type);
                if (r < 0)
                        return r;
        }

        r = serialize_bool_elide(f, "exec-context-cpu-affinity-from-numa", c->cpu_affinity_from_numa);
        if (r < 0)
                return r;

        if (c->timer_slack_nsec != NSEC_INFINITY) {
                r = serialize_item_format(f, "exec-context-timer-slack-nsec", NSEC_FMT, c->timer_slack_nsec);
                if (r < 0)
                        return r;
        }

        r = serialize_item(f, "exec-context-std-input", exec_input_to_string(c->std_input));
        if (r < 0)
                return r;

        r = serialize_item(f, "exec-context-std-output", exec_output_to_string(c->std_output));
        if (r < 0)
                return r;

        r = serialize_item(f, "exec-context-std-error", exec_output_to_string(c->std_error));
        if (r < 0)
                return r;

        r = serialize_bool_elide(f, "exec-context-stdio-as-fds", c->stdio_as_fds);
        if (r < 0)
                return r;

        switch (c->std_input) {
        case EXEC_INPUT_NAMED_FD:
                r = serialize_item(f, "exec-context-std-input-fd-name", c->stdio_fdname[STDIN_FILENO]);
                if (r < 0)
                        return r;
                break;

        case EXEC_INPUT_FILE:
                r = serialize_item(f, "exec-context-std-input-file", c->stdio_file[STDIN_FILENO]);
                if (r < 0)
                        return r;
                break;

        default:
                break;
        }

        r = serialize_std_out_err(c, f, STDOUT_FILENO);
        if (r < 0)
                return r;

        r = serialize_std_out_err(c, f, STDERR_FILENO);
        if (r < 0)
                return r;

        r = serialize_item_base64mem(f, "exec-context-stdin-data", c->stdin_data, c->stdin_data_size);
        if (r < 0)
                return r;

        r = serialize_item(f, "exec-context-tty-path", c->tty_path);
        if (r < 0)
                return r;

        r = serialize_bool_elide(f, "exec-context-tty-reset", c->tty_reset);
        if (r < 0)
                return r;

        r = serialize_bool_elide(f, "exec-context-tty-vhangup", c->tty_vhangup);
        if (r < 0)
                return r;

        r = serialize_bool_elide(f, "exec-context-tty-vt-disallocate", c->tty_vt_disallocate);
        if (r < 0)
                return r;

        r = serialize_item_format(f, "exec-context-tty-rows", "%u", c->tty_rows);
        if (r < 0)
                return r;

        r = serialize_item_format(f, "exec-context-tty-columns", "%u", c->tty_cols);
        if (r < 0)
                return r;

        r = serialize_item_format(f, "exec-context-syslog-priority", "%i", c->syslog_priority);
        if (r < 0)
                return r;

        r = serialize_bool(f, "exec-context-syslog-level-prefix", c->syslog_level_prefix);
        if (r < 0)
                return r;

        r = serialize_item(f, "exec-context-syslog-identifier", c->syslog_identifier);
        if (r < 0)
                return r;

        r = serialize_item_format(f, "exec-context-log-level-max", "%d", c->log_level_max);
        if (r < 0)
                return r;

        if (c->log_ratelimit_interval_usec > 0) {
                r = serialize_usec(f, "exec-context-log-ratelimit-interval-usec", c->log_ratelimit_interval_usec);
                if (r < 0)
                        return r;
        }

        if (c->log_ratelimit_burst > 0) {
                r = serialize_item_format(f, "exec-context-log-ratelimit-burst", "%u", c->log_ratelimit_burst);
                if (r < 0)
                        return r;
        }

        r = serialize_string_set(f, "exec-context-log-filter-allowed-patterns", c->log_filter_allowed_patterns);
        if (r < 0)
                return r;

        r = serialize_string_set(f, "exec-context-log-filter-denied-patterns", c->log_filter_denied_patterns);
        if (r < 0)
                return r;

        for (size_t j = 0; j < c->n_log_extra_fields; j++) {
                r = serialize_item(f, "exec-context-log-extra-fields", c->log_extra_fields[j].iov_base);
                if (r < 0)
                        return r;
        }

        r = serialize_item(f, "exec-context-log-namespace", c->log_namespace);
        if (r < 0)
                return r;

        if (c->secure_bits != 0) {
                r = serialize_item_format(f, "exec-context-secure-bits", "%d", c->secure_bits);
                if (r < 0)
                        return r;
        }

        if (c->capability_bounding_set != CAP_MASK_UNSET) {
                r = serialize_item_format(f, "exec-context-capability-bounding-set", "%" PRIu64, c->capability_bounding_set);
                if (r < 0)
                        return r;
        }

        if (c->capability_ambient_set != 0) {
                r = serialize_item_format(f, "exec-context-capability-ambient-set", "%" PRIu64, c->capability_ambient_set);
                if (r < 0)
                        return r;
        }

        if (c->user) {
                r = serialize_item(f, "exec-context-user", c->user);
                if (r < 0)
                        return r;
        }

        r = serialize_item(f, "exec-context-group", c->group);
        if (r < 0)
                return r;

        r = serialize_bool_elide(f, "exec-context-dynamic-user", c->dynamic_user);
        if (r < 0)
                return r;

        r = serialize_strv(f, "exec-context-supplementary-groups", c->supplementary_groups);
        if (r < 0)
                return r;

        r = serialize_item_tristate(f, "exec-context-set-login-environment", c->set_login_environment);
        if (r < 0)
                return r;

        r = serialize_item(f, "exec-context-pam-name", c->pam_name);
        if (r < 0)
                return r;

        r = serialize_strv(f, "exec-context-read-write-paths", c->read_write_paths);
        if (r < 0)
                return r;

        r = serialize_strv(f, "exec-context-read-only-paths", c->read_only_paths);
        if (r < 0)
                return r;

        r = serialize_strv(f, "exec-context-inaccessible-paths", c->inaccessible_paths);
        if (r < 0)
                return r;

        r = serialize_strv(f, "exec-context-exec-paths", c->exec_paths);
        if (r < 0)
                return r;

        r = serialize_strv(f, "exec-context-no-exec-paths", c->no_exec_paths);
        if (r < 0)
                return r;

        r = serialize_strv(f, "exec-context-exec-search-path", c->exec_search_path);
        if (r < 0)
                return r;

        r = serialize_item_format(f, "exec-context-mount-propagation-flag", "%lu", c->mount_propagation_flag);
        if (r < 0)
                return r;

        for (size_t i = 0; i < c->n_bind_mounts; i++) {
                _cleanup_free_ char *src_escaped = NULL, *dst_escaped = NULL;

                src_escaped = shell_escape(c->bind_mounts[i].source, ":");
                if (!src_escaped)
                        return log_oom_debug();

                dst_escaped = shell_escape(c->bind_mounts[i].destination, ":");
                if (!dst_escaped)
                        return log_oom_debug();

                r = serialize_item_format(f,
                                          c->bind_mounts[i].read_only ? "exec-context-bind-read-only-path" : "exec-context-bind-path",
                                          "%s%s:%s:%s",
                                          c->bind_mounts[i].ignore_enoent ? "-" : "",
                                          src_escaped,
                                          dst_escaped,
                                          c->bind_mounts[i].recursive ? "rbind" : "norbind");
                if (r < 0)
                        return r;
        }

        for (size_t i = 0; i < c->n_temporary_filesystems; i++) {
                const TemporaryFileSystem *t = c->temporary_filesystems + i;
                _cleanup_free_ char *escaped = NULL;

                if (!isempty(t->options)) {
                        escaped = shell_escape(t->options, ":");
                        if (!escaped)
                                return log_oom_debug();
                }

                r = serialize_item_format(f, "exec-context-temporary-filesystems", "%s%s%s",
                                          t->path,
                                          isempty(escaped) ? "" : ":",
                                          strempty(escaped));
                if (r < 0)
                        return r;
        }

        r = serialize_item(f, "exec-context-utmp-id", c->utmp_id);
        if (r < 0)
                return r;

        r = serialize_item(f, "exec-context-utmp-mode", exec_utmp_mode_to_string(c->utmp_mode));
        if (r < 0)
                return r;

        r = serialize_bool_elide(f, "exec-context-no-new-privileges", c->no_new_privileges);
        if (r < 0)
                return r;

        r = serialize_bool_elide(f, "exec-context-selinux-context-ignore", c->selinux_context_ignore);
        if (r < 0)
                return r;

        r = serialize_bool_elide(f, "exec-context-apparmor-profile-ignore", c->apparmor_profile_ignore);
        if (r < 0)
                return r;

        r = serialize_bool_elide(f, "exec-context-smack-process-label-ignore", c->smack_process_label_ignore);
        if (r < 0)
                return r;

        if (c->selinux_context) {
                r = serialize_item_format(f, "exec-context-selinux-context",
                                          "%s%s",
                                          c->selinux_context_ignore ? "-" : "",
                                          c->selinux_context);
                if (r < 0)
                        return r;
        }

        if (c->apparmor_profile) {
                r = serialize_item_format(f, "exec-context-apparmor-profile",
                                          "%s%s",
                                          c->apparmor_profile_ignore ? "-" : "",
                                          c->apparmor_profile);
                if (r < 0)
                        return r;
        }

        if (c->smack_process_label) {
                r = serialize_item_format(f, "exec-context-smack-process-label",
                                          "%s%s",
                                          c->smack_process_label_ignore ? "-" : "",
                                          c->smack_process_label);
                if (r < 0)
                        return r;
        }

        if (c->personality != PERSONALITY_INVALID) {
                r = serialize_item(f, "exec-context-personality", personality_to_string(c->personality));
                if (r < 0)
                        return r;
        }

        r = serialize_bool_elide(f, "exec-context-lock-personality", c->lock_personality);
        if (r < 0)
                return r;

#if HAVE_SECCOMP
        if (!hashmap_isempty(c->syscall_filter)) {
                void *errno_num, *id;
                HASHMAP_FOREACH_KEY(errno_num, id, c->syscall_filter) {
                        r = serialize_item_format(f, "exec-context-syscall-filter", "%d %d", PTR_TO_INT(id) - 1, PTR_TO_INT(errno_num));
                        if (r < 0)
                                return r;
                }
        }

        if (!set_isempty(c->syscall_archs)) {
                void *id;
                SET_FOREACH(id, c->syscall_archs) {
                        r = serialize_item_format(f, "exec-context-syscall-archs", "%u", PTR_TO_UINT(id) - 1);
                        if (r < 0)
                                return r;
                }
        }

        if (c->syscall_errno > 0) {
                r = serialize_item_format(f, "exec-context-syscall-errno", "%d", c->syscall_errno);
                if (r < 0)
                        return r;
        }

        r = serialize_bool_elide(f, "exec-context-syscall-allow-list", c->syscall_allow_list);
        if (r < 0)
                return r;

        if (!hashmap_isempty(c->syscall_log)) {
                void *errno_num, *id;
                HASHMAP_FOREACH_KEY(errno_num, id, c->syscall_log) {
                        r = serialize_item_format(f, "exec-context-syscall-log", "%d %d", PTR_TO_INT(id) - 1, PTR_TO_INT(errno_num));
                        if (r < 0)
                                return r;
                }
        }

        r = serialize_bool_elide(f, "exec-context-syscall-log-allow-list", c->syscall_log_allow_list);
        if (r < 0)
                return r;
#endif

        if (c->restrict_namespaces != NAMESPACE_FLAGS_INITIAL) {
                r = serialize_item_format(f, "exec-context-restrict-namespaces", "%lu", c->restrict_namespaces);
                if (r < 0)
                        return r;
        }

#if HAVE_LIBBPF
        if (exec_context_restrict_filesystems_set(c)) {
                char *fs;
                SET_FOREACH(fs, c->restrict_filesystems) {
                        r = serialize_item(f, "exec-context-restrict-filesystems", fs);
                        if (r < 0)
                                return r;
                }
        }

        r = serialize_bool_elide(f, "exec-context-restrict-filesystems-allow-list", c->restrict_filesystems_allow_list);
        if (r < 0)
                return r;
#endif

        if (!set_isempty(c->address_families)) {
                void *afp;

                SET_FOREACH(afp, c->address_families) {
                        int af = PTR_TO_INT(afp);

                        if (af <= 0 || af >= af_max())
                                continue;

                        r = serialize_item_format(f, "exec-context-address-families", "%d", af);
                        if (r < 0)
                                return r;
                }
        }

        r = serialize_bool_elide(f, "exec-context-address-families-allow-list", c->address_families_allow_list);
        if (r < 0)
                return r;

        r = serialize_item(f, "exec-context-network-namespace-path", c->network_namespace_path);
        if (r < 0)
                return r;

        r = serialize_item(f, "exec-context-ipc-namespace-path", c->ipc_namespace_path);
        if (r < 0)
                return r;

        for (size_t i = 0; i < c->n_mount_images; i++) {
                _cleanup_free_ char *s = NULL, *source_escaped = NULL, *dest_escaped = NULL;

                source_escaped = shell_escape(c->mount_images[i].source, " ");
                if (!source_escaped)
                        return log_oom_debug();

                dest_escaped = shell_escape(c->mount_images[i].destination, " ");
                if (!dest_escaped)
                        return log_oom_debug();

                s = strjoin(c->mount_images[i].ignore_enoent ? "-" : "",
                            source_escaped,
                            " ",
                            dest_escaped);
                if (!s)
                        return log_oom_debug();

                LIST_FOREACH(mount_options, o, c->mount_images[i].mount_options) {
                        _cleanup_free_ char *escaped = NULL;

                        if (isempty(o->options))
                                continue;

                        escaped = shell_escape(o->options, ":");
                        if (!escaped)
                                return log_oom_debug();

                        if (!strextend(&s,
                                       " ",
                                       partition_designator_to_string(o->partition_designator),
                                       ":",
                                       escaped))
                                return log_oom_debug();
                }

                r = serialize_item(f, "exec-context-mount-image", s);
                if (r < 0)
                        return r;
        }

        for (size_t i = 0; i < c->n_extension_images; i++) {
                _cleanup_free_ char *s = NULL, *source_escaped = NULL;

                source_escaped = shell_escape(c->extension_images[i].source, ":");
                if (!source_escaped)
                        return log_oom_debug();

                s = strjoin(c->extension_images[i].ignore_enoent ? "-" : "",
                            source_escaped);
                if (!s)
                        return log_oom_debug();

                LIST_FOREACH(mount_options, o, c->extension_images[i].mount_options) {
                        _cleanup_free_ char *escaped = NULL;

                        if (isempty(o->options))
                                continue;

                        escaped = shell_escape(o->options, ":");
                        if (!escaped)
                                return log_oom_debug();

                        if (!strextend(&s,
                                       " ",
                                       partition_designator_to_string(o->partition_designator),
                                       ":",
                                       escaped))
                                return log_oom_debug();
                }

                r = serialize_item(f, "exec-context-extension-image", s);
                if (r < 0)
                        return r;
        }

        r = serialize_strv(f, "exec-context-extension-directories", c->extension_directories);
        if (r < 0)
                return r;

        ExecSetCredential *sc;
        HASHMAP_FOREACH(sc, c->set_credentials) {
                _cleanup_free_ char *data = NULL;

                if (base64mem(sc->data, sc->size, &data) < 0)
                        return log_oom_debug();

                r = serialize_item_format(f, "exec-context-set-credentials", "%s %s %s", sc->id, yes_no(sc->encrypted), data);
                if (r < 0)
                        return r;
        }

        ExecLoadCredential *lc;
        HASHMAP_FOREACH(lc, c->load_credentials) {
                r = serialize_item_format(f, "exec-context-load-credentials", "%s %s %s", lc->id, yes_no(lc->encrypted), lc->path);
                if (r < 0)
                        return r;
        }

        if (!set_isempty(c->import_credentials)) {
                char *ic;
                SET_FOREACH(ic, c->import_credentials) {
                        r = serialize_item(f, "exec-context-import-credentials", ic);
                        if (r < 0)
                                return r;
                }
        }

        r = serialize_image_policy(f, "exec-context-root-image-policy", c->root_image_policy);
        if (r < 0)
                return r;

        r = serialize_image_policy(f, "exec-context-mount-image-policy", c->mount_image_policy);
        if (r < 0)
                return r;

        r = serialize_image_policy(f, "exec-context-extension-image-policy", c->extension_image_policy);
        if (r < 0)
                return r;

        fputc('\n', f); /* End marker */

        return 0;
}

static int exec_context_deserialize(ExecContext *c, FILE *f) {
        int r;

        assert(f);

        if (!c)
                return 0;

        for (;;) {
                _cleanup_free_ char *l = NULL;
                const char *val;

                r = deserialize_read_line(f, &l);
                if (r < 0)
                        return r;
                if (r == 0) /* eof or end marker */
                        break;

                if ((val = startswith(l, "exec-context-environment="))) {
                        r = deserialize_strv(&c->environment, val);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-environment-files="))) {
                        r = deserialize_strv(&c->environment_files, val);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-pass-environment="))) {
                        r = deserialize_strv(&c->pass_environment, val);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-unset-environment="))) {
                        r = deserialize_strv(&c->unset_environment, val);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-working-directory="))) {
                        r = free_and_strdup(&c->working_directory, val);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-root-directory="))) {
                        r = free_and_strdup(&c->root_directory, val);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-root-image="))) {
                        r = free_and_strdup(&c->root_image, val);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-root-image-options="))) {
                        for (;;) {
                                _cleanup_free_ char *word = NULL, *mount_options = NULL, *partition = NULL;
                                PartitionDesignator partition_designator;
                                MountOptions *o = NULL;
                                const char *p;

                                r = extract_first_word(&val, &word, NULL, 0);
                                if (r < 0)
                                        return r;
                                if (r == 0)
                                        break;

                                p = word;
                                r = extract_many_words(&p, ":", EXTRACT_CUNESCAPE|EXTRACT_UNESCAPE_SEPARATORS, &partition, &mount_options, NULL);
                                if (r < 0)
                                        return r;
                                if (r == 0)
                                        continue;

                                partition_designator = partition_designator_from_string(partition);
                                if (partition_designator < 0)
                                        return -EINVAL;

                                o = new(MountOptions, 1);
                                if (!o)
                                        return log_oom_debug();
                                *o = (MountOptions) {
                                        .partition_designator = partition_designator,
                                        .options = TAKE_PTR(mount_options),
                                };
                                LIST_APPEND(mount_options, c->root_image_options, o);
                        }
                } else if ((val = startswith(l, "exec-context-root-verity="))) {
                        r = free_and_strdup(&c->root_verity, val);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-root-hash-path="))) {
                        r = free_and_strdup(&c->root_hash_path, val);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-root-hash-sig-path="))) {
                        r = free_and_strdup(&c->root_hash_sig_path, val);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-root-hash="))) {
                        c->root_hash = mfree(c->root_hash);
                        r = unhexmem(val, strlen(val), &c->root_hash, &c->root_hash_size);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-root-hash-sig="))) {
                        c->root_hash_sig = mfree(c->root_hash_sig);
                        r= unbase64mem(val, strlen(val), &c->root_hash_sig, &c->root_hash_sig_size);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-root-ephemeral="))) {
                        r = parse_boolean(val);
                        if (r < 0)
                                return r;
                        c->root_ephemeral = r;
                } else if ((val = startswith(l, "exec-context-umask="))) {
                        r = parse_mode(val, &c->umask);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-private-non-blocking="))) {
                        r = parse_boolean(val);
                        if (r < 0)
                                return r;
                        c->non_blocking = r;
                } else if ((val = startswith(l, "exec-context-private-mounts="))) {
                        r = safe_atoi(val, &c->private_mounts);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-memory-ksm="))) {
                        r = safe_atoi(val, &c->memory_ksm);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-private-tmp="))) {
                        r = parse_boolean(val);
                        if (r < 0)
                                return r;
                        c->private_tmp = r;
                } else if ((val = startswith(l, "exec-context-private-devices="))) {
                        r = parse_boolean(val);
                        if (r < 0)
                                return r;
                        c->private_devices = r;
                } else if ((val = startswith(l, "exec-context-protect-kernel-tunables="))) {
                        r = parse_boolean(val);
                        if (r < 0)
                                return r;
                        c->protect_kernel_tunables = r;
                } else if ((val = startswith(l, "exec-context-protect-kernel-modules="))) {
                        r = parse_boolean(val);
                        if (r < 0)
                                return r;
                        c->protect_kernel_modules = r;
                } else if ((val = startswith(l, "exec-context-protect-kernel-logs="))) {
                        r = parse_boolean(val);
                        if (r < 0)
                                return r;
                        c->protect_kernel_logs = r;
                } else if ((val = startswith(l, "exec-context-protect-clock="))) {
                        r = parse_boolean(val);
                        if (r < 0)
                                return r;
                        c->protect_clock = r;
                } else if ((val = startswith(l, "exec-context-protect-control-groups="))) {
                        r = parse_boolean(val);
                        if (r < 0)
                                return r;
                        c->protect_control_groups = r;
                } else if ((val = startswith(l, "exec-context-private-network="))) {
                        r = parse_boolean(val);
                        if (r < 0)
                                return r;
                        c->private_network = r;
                } else if ((val = startswith(l, "exec-context-private-users="))) {
                        r = parse_boolean(val);
                        if (r < 0)
                                return r;
                        c->private_users = r;
                } else if ((val = startswith(l, "exec-context-private-ipc="))) {
                        r = parse_boolean(val);
                        if (r < 0)
                                return r;
                        c->private_ipc = r;
                } else if ((val = startswith(l, "exec-context-remove-ipc="))) {
                        r = parse_boolean(val);
                        if (r < 0)
                                return r;
                        c->remove_ipc = r;
                } else if ((val = startswith(l, "exec-context-protect-home="))) {
                        c->protect_home = protect_home_from_string(val);
                        if (c->protect_home < 0)
                                return -EINVAL;
                } else if ((val = startswith(l, "exec-context-protect-system="))) {
                        c->protect_system = protect_system_from_string(val);
                        if (c->protect_system < 0)
                                return -EINVAL;
                } else if ((val = startswith(l, "exec-context-mount-api-vfs="))) {
                        r = parse_boolean(val);
                        if (r < 0)
                                return r;
                        c->mount_apivfs = r;
                        c->mount_apivfs_set = true;
                } else if ((val = startswith(l, "exec-context-same-pgrp="))) {
                        r = parse_boolean(val);
                        if (r < 0)
                                return r;
                        c->same_pgrp = r;
                } else if ((val = startswith(l, "exec-context-cpu-sched-reset-on-fork="))) {
                        r = parse_boolean(val);
                        if (r < 0)
                                return r;
                        c->cpu_sched_reset_on_fork = r;
                } else if ((val = startswith(l, "exec-context-non-blocking="))) {
                        r = parse_boolean(val);
                        if (r < 0)
                                return r;
                        r = c->non_blocking;
                } else if ((val = startswith(l, "exec-context-ignore-sigpipe="))) {
                        r = parse_boolean(val);
                        if (r < 0)
                                return r;
                        c->ignore_sigpipe = r;
                } else if ((val = startswith(l, "exec-context-memory-deny-write-execute="))) {
                        r = parse_boolean(val);
                        if (r < 0)
                                return r;
                        c->memory_deny_write_execute = r;
                } else if ((val = startswith(l, "exec-context-restrict-realtime="))) {
                        r = parse_boolean(val);
                        if (r < 0)
                                return r;
                        c->restrict_realtime = r;
                } else if ((val = startswith(l, "exec-context-restrict-suid-sgid="))) {
                        r = parse_boolean(val);
                        if (r < 0)
                                return r;
                        c->restrict_suid_sgid = r;
                } else if ((val = startswith(l, "exec-context-keyring-mode="))) {
                        c->keyring_mode = exec_keyring_mode_from_string(val);
                        if (c->keyring_mode < 0)
                                return -EINVAL;
                } else if ((val = startswith(l, "exec-context-protect-hostname="))) {
                        r = parse_boolean(val);
                        if (r < 0)
                                return r;
                        c->protect_hostname = r;
                } else if ((val = startswith(l, "exec-context-protect-proc="))) {
                        c->protect_proc = protect_proc_from_string(val);
                        if (c->protect_proc < 0)
                                return -EINVAL;
                } else if ((val = startswith(l, "exec-context-proc-subset="))) {
                        c->proc_subset = proc_subset_from_string(val);
                        if (c->proc_subset < 0)
                                return -EINVAL;
                } else if ((val = startswith(l, "exec-context-runtime-directory-preserve-mode="))) {
                        c->runtime_directory_preserve_mode = exec_preserve_mode_from_string(val);
                        if (c->runtime_directory_preserve_mode < 0)
                                return -EINVAL;
                } else if ((val = startswith(l, "exec-context-directories-"))) {
                        _cleanup_free_ char *type = NULL, *mode = NULL;
                        ExecDirectoryType dt;

                        r = extract_many_words(&val, "= ", 0, &type, &mode, NULL);
                        if (r < 0)
                                return r;
                        if (r == 0 || !mode)
                                return -EINVAL;

                        dt = exec_directory_type_from_string(type);
                        if (dt < 0)
                                return -EINVAL;

                        r = parse_mode(mode, &c->directories[dt].mode);
                        if (r < 0)
                                return r;

                        for (;;) {
                                _cleanup_free_ char *tuple = NULL, *path = NULL, *only_create = NULL;
                                const char *p;

                                r = extract_first_word(&val, &tuple, WHITESPACE, EXTRACT_RETAIN_ESCAPE);
                                if (r < 0)
                                        return r;
                                if (r == 0)
                                        break;

                                p = tuple;
                                r = extract_many_words(&p, ":", EXTRACT_UNESCAPE_SEPARATORS, &path, &only_create, NULL);
                                if (r < 0)
                                        return r;
                                if (r < 2)
                                        continue;

                                r = exec_directory_add(&c->directories[dt], path, NULL);
                                if (r < 0)
                                        return r;

                                r = parse_boolean(only_create);
                                if (r < 0)
                                        return r;
                                c->directories[dt].items[c->directories[dt].n_items - 1].only_create = r;

                                if (isempty(p))
                                        continue;

                                for (;;) {
                                        _cleanup_free_ char *link = NULL;

                                        r = extract_first_word(&p, &link, ":", EXTRACT_UNESCAPE_SEPARATORS);
                                        if (r < 0)
                                                return r;
                                        if (r == 0)
                                                break;

                                        r = strv_consume(&c->directories[dt].items[c->directories[dt].n_items - 1].symlinks, TAKE_PTR(link));
                                        if (r < 0)
                                                return r;
                                }
                        }
                } else if ((val = startswith(l, "exec-context-timeout-clean-usec="))) {
                        r = deserialize_usec(val, &c->timeout_clean_usec);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-nice="))) {
                        r = safe_atoi(val, &c->nice);
                        if (r < 0)
                                return r;
                        c->nice_set = true;
                } else if ((val = startswith(l, "exec-context-working-directory-missing-ok="))) {
                        r = parse_boolean(val);
                        if (r < 0)
                                return r;
                        c->working_directory_missing_ok = r;
                } else if ((val = startswith(l, "exec-context-working-directory-home="))) {
                        r = parse_boolean(val);
                        if (r < 0)
                                return r;
                        c->working_directory_home = r;
                } else if ((val = startswith(l, "exec-context-oom-score-adjust="))) {
                        r = safe_atoi(val, &c->oom_score_adjust);
                        if (r < 0)
                                return r;
                        c->oom_score_adjust_set = true;
                } else if ((val = startswith(l, "exec-context-coredump-filter="))) {
                        r = safe_atoux64(val, &c->coredump_filter);
                        if (r < 0)
                                return r;
                        c->coredump_filter_set = true;
                } else if ((val = startswith(l, "exec-context-limit-"))) {
                        _cleanup_free_ struct rlimit *rlimit = NULL;
                        _cleanup_free_ char *limit = NULL;
                        int type;

                        r = extract_first_word(&val, &limit, "=", 0);
                        if (r < 0)
                                return r;
                        if (r == 0 || !val)
                                return -EINVAL;

                        type = rlimit_from_string(limit);
                        if (type < 0)
                                return -EINVAL;

                        if (!c->rlimit[type]) {
                                rlimit = new0(struct rlimit, 1);
                                if (!rlimit)
                                        return log_oom_debug();

                                r = rlimit_parse(type, val, rlimit);
                                if (r < 0)
                                        return r;

                                c->rlimit[type] = TAKE_PTR(rlimit);
                        } else {
                                r = rlimit_parse(type, val, c->rlimit[type]);
                                if (r < 0)
                                        return r;
                        }
                } else if ((val = startswith(l, "exec-context-ioprio="))) {
                        r = safe_atoi(val, &c->ioprio);
                        if (r < 0)
                                return r;
                        c->ioprio_set = true;
                } else if ((val = startswith(l, "exec-context-cpu-scheduling-policy="))) {
                        r = sched_policy_from_string(val);
                        if (r < 0)
                                return r;
                        c->cpu_sched_set = true;
                } else if ((val = startswith(l, "exec-context-cpu-scheduling-priority="))) {
                        r = safe_atoi(val, &c->cpu_sched_priority);
                        if (r < 0)
                                return r;
                        c->cpu_sched_set = true;
                } else if ((val = startswith(l, "exec-context-cpu-scheduling-reset-on-fork="))) {
                        r = parse_boolean(val);
                        if (r < 0)
                                return r;
                        c->cpu_sched_reset_on_fork = r;
                        c->cpu_sched_set = true;
                } else if ((val = startswith(l, "exec-context-cpu-affinity="))) {
                        if (c->cpu_set.set)
                                return -EINVAL; /* duplicated */

                        r = parse_cpu_set(val, &c->cpu_set);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-numa-mask="))) {
                        if (c->numa_policy.nodes.set)
                                return -EINVAL; /* duplicated */

                        r = parse_cpu_set(val, &c->numa_policy.nodes);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-numa-policy="))) {
                        r = safe_atoi(val, &c->numa_policy.type);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-cpu-affinity-from-numa="))) {
                        r = parse_boolean(val);
                        if (r < 0)
                                return r;
                        c->cpu_affinity_from_numa = r;
                } else if ((val = startswith(l, "exec-context-timer-slack-nsec="))) {
                        r = deserialize_usec(val, (usec_t *)&c->timer_slack_nsec);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-std-input="))) {
                        c->std_input = exec_input_from_string(val);
                        if (c->std_input < 0)
                                return c->std_input;
                } else if ((val = startswith(l, "exec-context-std-output="))) {
                        c->std_output = exec_output_from_string(val);
                        if (c->std_output < 0)
                                return c->std_output;
                } else if ((val = startswith(l, "exec-context-std-error="))) {
                        c->std_error = exec_output_from_string(val);
                        if (c->std_error < 0)
                                return c->std_error;
                } else if ((val = startswith(l, "exec-context-stdio-as-fds="))) {
                        r = parse_boolean(val);
                        if (r < 0)
                                return r;
                        c->stdio_as_fds = r;
                } else if ((val = startswith(l, "exec-context-std-input-fd-name="))) {
                        r = free_and_strdup(&c->stdio_fdname[STDIN_FILENO], val);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-std-output-fd-name="))) {
                        r = free_and_strdup(&c->stdio_fdname[STDOUT_FILENO], val);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-std-error-fd-name="))) {
                        r = free_and_strdup(&c->stdio_fdname[STDERR_FILENO], val);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-std-input-file="))) {
                        r = free_and_strdup(&c->stdio_file[STDIN_FILENO], val);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-std-output-file="))) {
                        r = free_and_strdup(&c->stdio_file[STDOUT_FILENO], val);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-std-output-file-append="))) {
                        r = free_and_strdup(&c->stdio_file[STDOUT_FILENO], val);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-std-output-file-truncate="))) {
                        r = free_and_strdup(&c->stdio_file[STDOUT_FILENO], val);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-std-error-file="))) {
                        r = free_and_strdup(&c->stdio_file[STDERR_FILENO], val);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-std-error-file-append="))) {
                        r = free_and_strdup(&c->stdio_file[STDERR_FILENO], val);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-std-error-file-truncate="))) {
                        r = free_and_strdup(&c->stdio_file[STDERR_FILENO], val);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-stdin-data="))) {
                        if (c->stdin_data)
                                return -EINVAL; /* duplicated */

                        r = unbase64mem(val, strlen(val), &c->stdin_data, &c->stdin_data_size);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-tty-path="))) {
                        r = free_and_strdup(&c->tty_path, val);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-tty-reset="))) {
                        r = parse_boolean(val);
                        if (r < 0)
                                return r;
                        c->tty_reset = r;
                } else if ((val = startswith(l, "exec-context-tty-vhangup="))) {
                        r = parse_boolean(val);
                        if (r < 0)
                                return r;
                        c->tty_vhangup = r;
                } else if ((val = startswith(l, "exec-context-tty-vt-disallocate="))) {
                        r = parse_boolean(val);
                        if (r < 0)
                                return r;
                        c->tty_vt_disallocate = r;
                } else if ((val = startswith(l, "exec-context-tty-rows="))) {
                        r = safe_atou(val, &c->tty_rows);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-tty-columns="))) {
                        r = safe_atou(val, &c->tty_cols);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-syslog-priority="))) {
                        r = safe_atoi(val, &c->syslog_priority);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-syslog-level-prefix="))) {
                        r = parse_boolean(val);
                        if (r < 0)
                                return r;
                        c->syslog_level_prefix = r;
                } else if ((val = startswith(l, "exec-context-syslog-identifier="))) {
                        r = free_and_strdup(&c->syslog_identifier, val);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-log-level-max="))) {
                        r = safe_atoi(val, &c->log_level_max);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-log-ratelimit-interval-usec="))) {
                        r = deserialize_usec(val, &c->log_ratelimit_interval_usec);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-log-ratelimit-burst="))) {
                        r = safe_atou(val, &c->log_ratelimit_burst);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-log-filter-allowed-patterns="))) {
                        r = set_put_strdup(&c->log_filter_allowed_patterns, val);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-log-filter-denied-patterns="))) {
                        r = set_put_strdup(&c->log_filter_denied_patterns, val);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-log-extra-fields="))) {
                        if (!GREEDY_REALLOC(c->log_extra_fields, c->n_log_extra_fields + 1))
                                return log_oom_debug();

                        c->log_extra_fields[c->n_log_extra_fields++].iov_base = strdup(val);
                        if (!c->log_extra_fields[c->n_log_extra_fields-1].iov_base)
                                return log_oom_debug();
                } else if ((val = startswith(l, "exec-context-log-namespace="))) {
                        r = free_and_strdup(&c->log_namespace, val);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-secure-bits="))) {
                        r = safe_atoi(val, &c->secure_bits);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-capability-bounding-set="))) {
                        r = safe_atou64(val, &c->capability_bounding_set);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-capability-ambient-set="))) {
                        r = safe_atou64(val, &c->capability_ambient_set);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-user="))) {
                        r = free_and_strdup(&c->user, val);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-group="))) {
                        r = free_and_strdup(&c->group, val);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-dynamic-user="))) {
                        r = parse_boolean(val);
                        if (r < 0)
                                return r;
                        c->dynamic_user = r;
                } else if ((val = startswith(l, "exec-context-supplementary-groups="))) {
                        r = deserialize_strv(&c->supplementary_groups, val);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-set-login-environment="))) {
                        r = safe_atoi(val, &c->set_login_environment);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-pam-name="))) {
                        r = free_and_strdup(&c->pam_name, val);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-read-write-paths="))) {
                        r = deserialize_strv(&c->read_write_paths, val);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-read-only-paths="))) {
                        r = deserialize_strv(&c->read_only_paths, val);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-inaccessible-paths="))) {
                        r = deserialize_strv(&c->inaccessible_paths, val);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-exec-paths="))) {
                        r = deserialize_strv(&c->exec_paths, val);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-no-exec-paths="))) {
                        r = deserialize_strv(&c->no_exec_paths, val);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-exec-search-path="))) {
                        r = deserialize_strv(&c->exec_search_path, val);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-mount-propagation-flag="))) {
                        r = safe_atolu(val, &c->mount_propagation_flag);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-bind-read-only-path="))) {
                        _cleanup_free_ char *source = NULL, *destination = NULL;
                        bool rbind = true, ignore_enoent = false;
                        char *s = NULL, *d = NULL;

                        r = extract_first_word(&val,
                                               &source,
                                               ":" WHITESPACE,
                                               EXTRACT_UNQUOTE|EXTRACT_DONT_COALESCE_SEPARATORS|EXTRACT_UNESCAPE_SEPARATORS);
                        if (r < 0)
                                return r;
                        if (r == 0)
                                return -EINVAL;

                        s = source;
                        if (s[0] == '-') {
                                ignore_enoent = true;
                                s++;
                        }

                        if (val && val[-1] == ':') {
                                r = extract_first_word(&val,
                                                       &destination,
                                                       ":" WHITESPACE,
                                                       EXTRACT_UNQUOTE|EXTRACT_DONT_COALESCE_SEPARATORS|EXTRACT_UNESCAPE_SEPARATORS);
                                if (r < 0)
                                        return r;
                                if (r == 0)
                                        continue;

                                d = destination;

                                if (val && val[-1] == ':') {
                                        _cleanup_free_ char *options = NULL;

                                        r = extract_first_word(&val, &options, NULL, EXTRACT_UNQUOTE);
                                        if (r < 0)
                                                return -r;

                                        if (isempty(options) || streq(options, "rbind"))
                                                rbind = true;
                                        else if (streq(options, "norbind"))
                                                rbind = false;
                                        else
                                                continue;
                                }
                        } else
                                d = s;

                        r = bind_mount_add(&c->bind_mounts, &c->n_bind_mounts,
                                        &(BindMount) {
                                                .source = s,
                                                .destination = d,
                                                .read_only = true,
                                                .recursive = rbind,
                                                .ignore_enoent = ignore_enoent,
                                        });
                        if (r < 0)
                                return log_oom_debug();
                } else if ((val = startswith(l, "exec-context-bind-path="))) {
                        _cleanup_free_ char *source = NULL, *destination = NULL;
                        bool rbind = true, ignore_enoent = false;
                        char *s = NULL, *d = NULL;

                        r = extract_first_word(&val,
                                               &source,
                                               ":" WHITESPACE,
                                               EXTRACT_UNQUOTE|EXTRACT_DONT_COALESCE_SEPARATORS|EXTRACT_UNESCAPE_SEPARATORS);
                        if (r < 0)
                                return r;
                        if (r == 0)
                                return -EINVAL;

                        s = source;
                        if (s[0] == '-') {
                                ignore_enoent = true;
                                s++;
                        }

                        if (val && val[-1] == ':') {
                                r = extract_first_word(&val,
                                                       &destination,
                                                       ":" WHITESPACE,
                                                       EXTRACT_UNQUOTE|EXTRACT_DONT_COALESCE_SEPARATORS|EXTRACT_UNESCAPE_SEPARATORS);
                                if (r < 0)
                                        return r;
                                if (r == 0)
                                        continue;

                                d = destination;

                                if (val && val[-1] == ':') {
                                        _cleanup_free_ char *options = NULL;

                                        r = extract_first_word(&val, &options, NULL, EXTRACT_UNQUOTE);
                                        if (r < 0)
                                                return -r;

                                        if (isempty(options) || streq(options, "rbind"))
                                                rbind = true;
                                        else if (streq(options, "norbind"))
                                                rbind = false;
                                        else
                                                continue;
                                }
                        } else
                                d = s;

                        r = bind_mount_add(&c->bind_mounts, &c->n_bind_mounts,
                                        &(BindMount) {
                                                .source = s,
                                                .destination = d,
                                                .read_only = false,
                                                .recursive = rbind,
                                                .ignore_enoent = ignore_enoent,
                                        });
                        if (r < 0)
                                return log_oom_debug();
                } else if ((val = startswith(l, "exec-context-temporary-filesystems="))) {
                        _cleanup_free_ char *path = NULL, *options = NULL;

                        r = extract_many_words(&val, ":", EXTRACT_CUNESCAPE|EXTRACT_UNESCAPE_SEPARATORS, &path, &options, NULL);
                        if (r < 0)
                                return r;
                        if (r < 1)
                                continue;

                        r = temporary_filesystem_add(&c->temporary_filesystems, &c->n_temporary_filesystems, path, options);
                        if (r < 0)
                                return log_oom_debug();
                } else if ((val = startswith(l, "exec-context-utmp-id="))) {
                        r = free_and_strdup(&c->utmp_id, val);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-utmp-mode="))) {
                        c->utmp_mode = exec_utmp_mode_from_string(val);
                        if (c->utmp_mode < 0)
                                return c->utmp_mode;
                } else if ((val = startswith(l, "exec-context-no-new-privileges="))) {
                        r = parse_boolean(val);
                        if (r < 0)
                                return r;
                        c->no_new_privileges = r;
                } else if ((val = startswith(l, "exec-context-selinux-context-ignore="))) {
                        r = parse_boolean(val);
                        if (r < 0)
                                return r;
                        c->selinux_context_ignore = r;
                } else if ((val = startswith(l, "exec-context-apparmor-profile-ignore="))) {
                        r = parse_boolean(val);
                        if (r < 0)
                                return r;
                        c->apparmor_profile_ignore = r;
                } else if ((val = startswith(l, "exec-context-smack-process-label-ignore="))) {
                        r = parse_boolean(val);
                        if (r < 0)
                                return r;
                        c->smack_process_label_ignore = r;
                } else if ((val = startswith(l, "exec-context-selinux-context="))) {
                        if (val[0] == '-') {
                                c->selinux_context_ignore = true;
                                val++;
                        }

                        r = free_and_strdup(&c->selinux_context, val);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-apparmor-profile="))) {
                        if (val[0] == '-') {
                                c->apparmor_profile_ignore = true;
                                val++;
                        }

                        r = free_and_strdup(&c->apparmor_profile, val);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-smack-process-label="))) {
                        if (val[0] == '-') {
                                c->smack_process_label_ignore = true;
                                val++;
                        }

                        r = free_and_strdup(&c->smack_process_label, val);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-personality=")))
                        c->personality = personality_from_string(val);
                else if ((val = startswith(l, "exec-context-lock-personality="))) {
                        r = parse_boolean(val);
                        if (r < 0)
                                return r;
                        c->lock_personality = r;
#if HAVE_SECCOMP
                } else if ((val = startswith(l, "exec-context-syscall-filter="))) {
                        _cleanup_free_ char *s_id = NULL, *s_errno_num = NULL;
                        int id, errno_num;

                        r = extract_many_words(&val, NULL, 0, &s_id, &s_errno_num, NULL);
                        if (r < 0)
                                return r;
                        if (r != 2)
                                continue;

                        r = safe_atoi(s_id, &id);
                        if (r < 0)
                                return r;

                        r = safe_atoi(s_errno_num, &errno_num);
                        if (r < 0)
                                return r;

                        r = hashmap_ensure_put(&c->syscall_filter, NULL, INT_TO_PTR(id + 1), INT_TO_PTR(errno_num));
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-syscall-archs="))) {
                        unsigned int id;

                        r = safe_atou(val, &id);
                        if (r < 0)
                                return r;

                        r = set_ensure_put(&c->syscall_archs, NULL, UINT_TO_PTR(id + 1));
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-syscall-errno="))) {
                        r = safe_atoi(val, &c->syscall_errno);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-syscall-allow-list="))) {
                        r = parse_boolean(val);
                        if (r < 0)
                                return r;
                        c->syscall_allow_list = r;
                } else if ((val = startswith(l, "exec-context-syscall-log="))) {
                        _cleanup_free_ char *s_id = NULL, *s_errno_num = NULL;
                        int id, errno_num;

                        r = extract_many_words(&val, " ", 0, &s_id, &s_errno_num, NULL);
                        if (r < 0)
                                return r;
                        if (r != 2)
                                continue;

                        r = safe_atoi(s_id, &id);
                        if (r < 0)
                                return r;

                        r = safe_atoi(s_errno_num, &errno_num);
                        if (r < 0)
                                return r;

                        r = hashmap_ensure_put(&c->syscall_log, NULL, INT_TO_PTR(id + 1), INT_TO_PTR(errno_num));
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-syscall-log-allow-list="))) {
                        r = parse_boolean(val);
                        if (r < 0)
                                return r;
                        c->syscall_log_allow_list = r;
#endif
                } else if ((val = startswith(l, "exec-context-restrict-namespaces="))) {
                        r = safe_atolu(val, &c->restrict_namespaces);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-restrict-filesystems="))) {
                        r = set_ensure_allocated(&c->restrict_filesystems, &string_hash_ops);
                        if (r < 0)
                                return r;

                        r = set_put_strdup(&c->restrict_filesystems, val);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-restrict-filesystems-allow-list="))) {
                        r = parse_boolean(val);
                        if (r < 0)
                                return r;
                        c->restrict_filesystems_allow_list = r;
                } else if ((val = startswith(l, "exec-context-address-families="))) {
                        int af;

                        r = safe_atoi(val, &af);
                        if (r < 0)
                                return r;

                        r = set_ensure_put(&c->address_families, NULL, INT_TO_PTR(af));
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-address-families-allow-list="))) {
                        r = parse_boolean(val);
                        if (r < 0)
                                return r;
                        c->address_families_allow_list = r;
                } else if ((val = startswith(l, "exec-context-network-namespace-path="))) {
                        r = free_and_strdup(&c->network_namespace_path, val);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-ipc-namespace-path="))) {
                        r = free_and_strdup(&c->ipc_namespace_path, val);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-mount-image="))) {
                        _cleanup_(mount_options_free_allp) MountOptions *options = NULL;
                        _cleanup_free_ char *source = NULL, *destination = NULL;
                        bool permissive = false;
                        char *s;

                        r = extract_many_words(&val,
                                               NULL,
                                               EXTRACT_UNQUOTE|EXTRACT_CUNESCAPE|EXTRACT_UNESCAPE_SEPARATORS,
                                               &source,
                                               &destination,
                                               NULL);
                        if (r < 0)
                                return r;
                        if (r == 0)
                                return -EINVAL;

                        s = source;
                        if (s[0] == '-') {
                                permissive = true;
                                s++;
                        }

                        if (isempty(destination))
                                continue;

                        for (;;) {
                                _cleanup_free_ char *tuple = NULL, *partition = NULL, *opts = NULL;
                                PartitionDesignator partition_designator;
                                MountOptions *o = NULL;
                                const char *p;

                                r = extract_first_word(&val, &tuple, NULL, EXTRACT_UNQUOTE|EXTRACT_RETAIN_ESCAPE);
                                if (r < 0)
                                        return r;
                                if (r == 0)
                                        break;

                                p = tuple;
                                r = extract_many_words(&p,
                                                       ":",
                                                       EXTRACT_CUNESCAPE|EXTRACT_UNESCAPE_SEPARATORS,
                                                       &partition,
                                                       &opts,
                                                       NULL);
                                if (r < 0)
                                        return r;
                                if (r == 0)
                                        continue;
                                if (r == 1) {
                                        o = new(MountOptions, 1);
                                        if (!o)
                                                return log_oom_debug();
                                        *o = (MountOptions) {
                                                .partition_designator = PARTITION_ROOT,
                                                .options = TAKE_PTR(partition),
                                        };
                                        LIST_APPEND(mount_options, options, o);

                                        continue;
                                }

                                partition_designator = partition_designator_from_string(partition);
                                if (partition_designator < 0)
                                        continue;

                                o = new(MountOptions, 1);
                                if (!o)
                                        return log_oom_debug();
                                *o = (MountOptions) {
                                        .partition_designator = partition_designator,
                                        .options = TAKE_PTR(opts),
                                };
                                LIST_APPEND(mount_options, options, o);
                        }

                        r = mount_image_add(&c->mount_images, &c->n_mount_images,
                                        &(MountImage) {
                                                .source = s,
                                                .destination = destination,
                                                .mount_options = options,
                                                .ignore_enoent = permissive,
                                                .type = MOUNT_IMAGE_DISCRETE,
                                        });
                        if (r < 0)
                                return log_oom_debug();
                } else if ((val = startswith(l, "exec-context-extension-image="))) {
                        _cleanup_(mount_options_free_allp) MountOptions *options = NULL;
                        _cleanup_free_ char *source = NULL;
                        bool permissive = false;
                        char *s;

                        r = extract_first_word(&val,
                                               &source,
                                               NULL,
                                               EXTRACT_UNQUOTE|EXTRACT_CUNESCAPE|EXTRACT_UNESCAPE_SEPARATORS);
                        if (r < 0)
                                return r;
                        if (r == 0)
                                return -EINVAL;

                        s = source;
                        if (s[0] == '-') {
                                permissive = true;
                                s++;
                        }

                        for (;;) {
                                _cleanup_free_ char *tuple = NULL, *partition = NULL, *opts = NULL;
                                PartitionDesignator partition_designator;
                                MountOptions *o = NULL;
                                const char *p;

                                r = extract_first_word(&val, &tuple, NULL, EXTRACT_UNQUOTE|EXTRACT_RETAIN_ESCAPE);
                                if (r < 0)
                                        return r;
                                if (r == 0)
                                        break;

                                p = tuple;
                                r = extract_many_words(&p,
                                                       ":",
                                                       EXTRACT_CUNESCAPE|EXTRACT_UNESCAPE_SEPARATORS,
                                                       &partition,
                                                       &opts,
                                                       NULL);
                                if (r < 0)
                                        return r;
                                if (r == 0)
                                        continue;
                                if (r == 1) {
                                        o = new(MountOptions, 1);
                                        if (!o)
                                                return log_oom_debug();
                                        *o = (MountOptions) {
                                                .partition_designator = PARTITION_ROOT,
                                                .options = TAKE_PTR(partition),
                                        };
                                        LIST_APPEND(mount_options, options, o);

                                        continue;
                                }

                                partition_designator = partition_designator_from_string(partition);
                                if (partition_designator < 0)
                                        continue;

                                o = new(MountOptions, 1);
                                if (!o)
                                        return log_oom_debug();
                                *o = (MountOptions) {
                                        .partition_designator = partition_designator,
                                        .options = TAKE_PTR(opts),
                                };
                                LIST_APPEND(mount_options, options, o);
                        }

                        r = mount_image_add(&c->extension_images, &c->n_extension_images,
                                        &(MountImage) {
                                                .source = s,
                                                .mount_options = options,
                                                .ignore_enoent = permissive,
                                                .type = MOUNT_IMAGE_EXTENSION,
                                        });
                        if (r < 0)
                                return log_oom_debug();
                } else if ((val = startswith(l, "exec-context-extension-directories="))) {
                        r = deserialize_strv(&c->extension_directories, val);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-set-credentials="))) {
                        _cleanup_(exec_set_credential_freep) ExecSetCredential *sc = NULL;
                        _cleanup_free_ char *id = NULL, *encrypted = NULL, *data = NULL;

                        r = extract_many_words(&val, " ", 0, &id, &encrypted, &data, NULL);
                        if (r < 0)
                                return r;
                        if (r != 3)
                                return -EINVAL;

                        r = parse_boolean(encrypted);
                        if (r < 0)
                                return r;

                        sc = new(ExecSetCredential, 1);
                        if (!sc)
                                return -ENOMEM;

                        *sc = (ExecSetCredential) {
                                .id =  TAKE_PTR(id),
                                .encrypted = r,
                        };

                        r = unbase64mem(data, strlen(data), &sc->data, &sc->size);
                        if (r < 0)
                                return r;

                        r = hashmap_ensure_put(&c->set_credentials, &exec_set_credential_hash_ops, sc->id, sc);
                        if (r < 0)
                                return r;

                        TAKE_PTR(sc);
                } else if ((val = startswith(l, "exec-context-load-credentials="))) {
                        _cleanup_(exec_load_credential_freep) ExecLoadCredential *lc = NULL;
                        _cleanup_free_ char *id = NULL, *encrypted = NULL, *path = NULL;

                        r = extract_many_words(&val, " ", 0, &id, &encrypted, &path, NULL);
                        if (r < 0)
                                return r;
                        if (r != 3)
                                return -EINVAL;

                        r = parse_boolean(encrypted);
                        if (r < 0)
                                return r;

                        lc = new(ExecLoadCredential, 1);
                        if (!lc)
                                return -ENOMEM;

                        *lc = (ExecLoadCredential) {
                                .id =  TAKE_PTR(id),
                                .path = TAKE_PTR(path),
                                .encrypted = r,
                        };

                        r = hashmap_ensure_put(&c->load_credentials, &exec_load_credential_hash_ops, lc->id, lc);
                        if (r < 0)
                                return r;

                        TAKE_PTR(lc);
                } else if ((val = startswith(l, "exec-context-import-credentials="))) {
                        r = set_ensure_allocated(&c->import_credentials, &string_hash_ops);
                        if (r < 0)
                                return r;

                        r = set_put_strdup(&c->import_credentials, val);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-root-image-policy="))) {
                        if (c->root_image_policy)
                                return -EINVAL; /* duplicated */

                        r = image_policy_from_string(val, &c->root_image_policy);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-mount-image-policy="))) {
                        if (c->mount_image_policy)
                                return -EINVAL; /* duplicated */

                        r = image_policy_from_string(val, &c->mount_image_policy);
                        if (r < 0)
                                return r;
                } else if ((val = startswith(l, "exec-context-extension-image-policy="))) {
                        if (c->extension_image_policy)
                                return -EINVAL; /* duplicated */

                        r = image_policy_from_string(val, &c->extension_image_policy);
                        if (r < 0)
                                return r;
                } else
                        log_warning("Failed to parse serialized line, ignoring: %s", l);
        }

        return 0;
}

int exec_serialize_invocation(
                FILE *f,
                FDSet *fds,
                const ExecContext *ctx) {

        int r;

        assert(f);
        assert(fds);

        r = exec_context_serialize(ctx, f);
        if (r < 0)
                return log_debug_errno(r, "Failed to serialize context: %m");

        return 0;
}

int exec_deserialize_invocation(
                FILE *f,
                FDSet *fds,
                ExecContext *ctx) {

        int r;

        assert(f);
        assert(fds);

        r = exec_context_deserialize(ctx, f);
        if (r < 0)
                return log_debug_errno(r, "Failed to deserialize context: %m");

        return 0;
}