/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once

//#include <stdbool.h>

//#include "bpf-lsm.h"
#include "cgroup-util.h"
//#include "cpu-set-util.h"
#include "list.h"
#include "time-util.h"

#if 0 /// UNNEEDED by elogind
typedef struct TasksMax {
        /* If scale == 0, just use value; otherwise, value / scale.
         * See tasks_max_resolve(). */
        uint64_t value;
        uint64_t scale;
} TasksMax;

#define TASKS_MAX_UNSET ((TasksMax) { .value = UINT64_MAX, .scale = 0 })

static inline bool tasks_max_isset(const TasksMax *tasks_max) {
        return tasks_max->value != UINT64_MAX || tasks_max->scale != 0;
}

uint64_t tasks_max_resolve(const TasksMax *tasks_max);

typedef struct CGroupContext CGroupContext;
typedef struct CGroupDeviceAllow CGroupDeviceAllow;
typedef struct CGroupIODeviceWeight CGroupIODeviceWeight;
typedef struct CGroupIODeviceLimit CGroupIODeviceLimit;
typedef struct CGroupIODeviceLatency CGroupIODeviceLatency;
typedef struct CGroupBlockIODeviceWeight CGroupBlockIODeviceWeight;
typedef struct CGroupBlockIODeviceBandwidth CGroupBlockIODeviceBandwidth;
typedef struct CGroupBPFForeignProgram CGroupBPFForeignProgram;
typedef struct CGroupSocketBindItem CGroupSocketBindItem;

typedef enum CGroupDevicePolicy {
        /* When devices listed, will allow those, plus built-in ones, if none are listed will allow
         * everything. */
        CGROUP_DEVICE_POLICY_AUTO,

        /* Everything forbidden, except built-in ones and listed ones. */
        CGROUP_DEVICE_POLICY_CLOSED,

        /* Everything forbidden, except for the listed devices */
        CGROUP_DEVICE_POLICY_STRICT,

        _CGROUP_DEVICE_POLICY_MAX,
        _CGROUP_DEVICE_POLICY_INVALID = -EINVAL,
} CGroupDevicePolicy;

typedef enum FreezerAction {
        FREEZER_FREEZE,
        FREEZER_THAW,

        _FREEZER_ACTION_MAX,
        _FREEZER_ACTION_INVALID = -EINVAL,
} FreezerAction;

struct CGroupDeviceAllow {
        LIST_FIELDS(CGroupDeviceAllow, device_allow);
        char *path;
        bool r:1;
        bool w:1;
        bool m:1;
};

struct CGroupIODeviceWeight {
        LIST_FIELDS(CGroupIODeviceWeight, device_weights);
        char *path;
        uint64_t weight;
};

struct CGroupIODeviceLimit {
        LIST_FIELDS(CGroupIODeviceLimit, device_limits);
        char *path;
        uint64_t limits[_CGROUP_IO_LIMIT_TYPE_MAX];
};

struct CGroupIODeviceLatency {
        LIST_FIELDS(CGroupIODeviceLatency, device_latencies);
        char *path;
        usec_t target_usec;
};

struct CGroupBlockIODeviceWeight {
        LIST_FIELDS(CGroupBlockIODeviceWeight, device_weights);
        char *path;
        uint64_t weight;
};

struct CGroupBlockIODeviceBandwidth {
        LIST_FIELDS(CGroupBlockIODeviceBandwidth, device_bandwidths);
        char *path;
        uint64_t rbps;
        uint64_t wbps;
};

struct CGroupBPFForeignProgram {
        LIST_FIELDS(CGroupBPFForeignProgram, programs);
        uint32_t attach_type;
        char *bpffs_path;
};

struct CGroupSocketBindItem {
        LIST_FIELDS(CGroupSocketBindItem, socket_bind_items);
        int address_family;
        int ip_protocol;
        uint16_t nr_ports;
        uint16_t port_min;
};

typedef enum CGroupPressureWatch {
        CGROUP_PRESSURE_WATCH_OFF,      /* → tells the service payload explicitly not to watch for memory pressure */
        CGROUP_PRESSURE_WATCH_AUTO,     /* → on if memory account is on anyway for the unit, otherwise off */
        CGROUP_PRESSURE_WATCH_ON,
        CGROUP_PRESSURE_WATCH_SKIP,     /* → doesn't set up memory pressure watch, but also doesn't explicitly tell payload to avoid it */
        _CGROUP_PRESSURE_WATCH_MAX,
        _CGROUP_PRESSURE_WATCH_INVALID = -EINVAL,
} CGroupPressureWatch;

struct CGroupContext {
        bool cpu_accounting;
        bool io_accounting;
        bool blockio_accounting;
        bool memory_accounting;
        bool tasks_accounting;
        bool ip_accounting;

        /* Configures the memory.oom.group attribute (on unified) */
        bool memory_oom_group;

        bool delegate;
        CGroupMask delegate_controllers;
        CGroupMask disable_controllers;

        /* For unified hierarchy */
        uint64_t cpu_weight;
        uint64_t startup_cpu_weight;
        usec_t cpu_quota_per_sec_usec;
        usec_t cpu_quota_period_usec;

        CPUSet cpuset_cpus;
        CPUSet startup_cpuset_cpus;
        CPUSet cpuset_mems;
        CPUSet startup_cpuset_mems;

        uint64_t io_weight;
        uint64_t startup_io_weight;
        LIST_HEAD(CGroupIODeviceWeight, io_device_weights);
        LIST_HEAD(CGroupIODeviceLimit, io_device_limits);
        LIST_HEAD(CGroupIODeviceLatency, io_device_latencies);

        uint64_t default_memory_min;
        uint64_t default_memory_low;
        uint64_t default_startup_memory_low;
        uint64_t memory_min;
        uint64_t memory_low;
        uint64_t startup_memory_low;
        uint64_t memory_high;
        uint64_t startup_memory_high;
        uint64_t memory_max;
        uint64_t startup_memory_max;
        uint64_t memory_swap_max;
        uint64_t startup_memory_swap_max;
        uint64_t memory_zswap_max;
        uint64_t startup_memory_zswap_max;

        bool default_memory_min_set:1;
        bool default_memory_low_set:1;
        bool default_startup_memory_low_set:1;
        bool memory_min_set:1;
        bool memory_low_set:1;
        bool startup_memory_low_set:1;
        bool startup_memory_high_set:1;
        bool startup_memory_max_set:1;
        bool startup_memory_swap_max_set:1;
        bool startup_memory_zswap_max_set:1;

        Set *ip_address_allow;
        Set *ip_address_deny;
        /* These two flags indicate that redundant entries have been removed from
         * ip_address_allow/ip_address_deny, i.e. in_addr_prefixes_reduce() has already been called. */
        bool ip_address_allow_reduced;
        bool ip_address_deny_reduced;

        char **ip_filters_ingress;
        char **ip_filters_egress;
        LIST_HEAD(CGroupBPFForeignProgram, bpf_foreign_programs);

        Set *restrict_network_interfaces;
        bool restrict_network_interfaces_is_allow_list;

        /* For legacy hierarchies */
        uint64_t cpu_shares;
        uint64_t startup_cpu_shares;

        uint64_t blockio_weight;
        uint64_t startup_blockio_weight;
        LIST_HEAD(CGroupBlockIODeviceWeight, blockio_device_weights);
        LIST_HEAD(CGroupBlockIODeviceBandwidth, blockio_device_bandwidths);

        uint64_t memory_limit;

        CGroupDevicePolicy device_policy;
        LIST_HEAD(CGroupDeviceAllow, device_allow);

        LIST_HEAD(CGroupSocketBindItem, socket_bind_allow);
        LIST_HEAD(CGroupSocketBindItem, socket_bind_deny);

        /* Common */
        TasksMax tasks_max;

        /* Settings for systemd-oomd */
        ManagedOOMMode moom_swap;
        ManagedOOMMode moom_mem_pressure;
        uint32_t moom_mem_pressure_limit; /* Normalized to 2^32-1 == 100% */
        ManagedOOMPreference moom_preference;

        /* Memory pressure logic */
        CGroupPressureWatch memory_pressure_watch;
        usec_t memory_pressure_threshold_usec;
        /* NB: For now we don't make the period configurable, not the type, nor do we allow multiple
         * triggers, nor triggers for non-memory pressure. We might add that later. */
};

/* Used when querying IP accounting data */
typedef enum CGroupIPAccountingMetric {
        CGROUP_IP_INGRESS_BYTES,
        CGROUP_IP_INGRESS_PACKETS,
        CGROUP_IP_EGRESS_BYTES,
        CGROUP_IP_EGRESS_PACKETS,
        _CGROUP_IP_ACCOUNTING_METRIC_MAX,
        _CGROUP_IP_ACCOUNTING_METRIC_INVALID = -EINVAL,
} CGroupIPAccountingMetric;

/* Used when querying IO accounting data */
typedef enum CGroupIOAccountingMetric {
        CGROUP_IO_READ_BYTES,
        CGROUP_IO_WRITE_BYTES,
        CGROUP_IO_READ_OPERATIONS,
        CGROUP_IO_WRITE_OPERATIONS,
        _CGROUP_IO_ACCOUNTING_METRIC_MAX,
        _CGROUP_IO_ACCOUNTING_METRIC_INVALID = -EINVAL,
} CGroupIOAccountingMetric;

typedef struct Unit Unit;
typedef struct Manager Manager;

usec_t cgroup_cpu_adjust_period(usec_t period, usec_t quota, usec_t resolution, usec_t max_period);

void cgroup_context_init(CGroupContext *c);
void cgroup_context_done(CGroupContext *c);
void cgroup_context_dump(Unit *u, FILE* f, const char *prefix);
void cgroup_context_dump_socket_bind_item(const CGroupSocketBindItem *item, FILE *f);

void cgroup_context_free_device_allow(CGroupContext *c, CGroupDeviceAllow *a);
void cgroup_context_free_io_device_weight(CGroupContext *c, CGroupIODeviceWeight *w);
void cgroup_context_free_io_device_limit(CGroupContext *c, CGroupIODeviceLimit *l);
void cgroup_context_free_io_device_latency(CGroupContext *c, CGroupIODeviceLatency *l);
void cgroup_context_free_blockio_device_weight(CGroupContext *c, CGroupBlockIODeviceWeight *w);
void cgroup_context_free_blockio_device_bandwidth(CGroupContext *c, CGroupBlockIODeviceBandwidth *b);
void cgroup_context_remove_bpf_foreign_program(CGroupContext *c, CGroupBPFForeignProgram *p);
void cgroup_context_remove_socket_bind(CGroupSocketBindItem **head);

static inline bool cgroup_context_want_memory_pressure(const CGroupContext *c) {
        assert(c);

        return c->memory_pressure_watch == CGROUP_PRESSURE_WATCH_ON ||
                (c->memory_pressure_watch == CGROUP_PRESSURE_WATCH_AUTO && c->memory_accounting);
}

int cgroup_add_device_allow(CGroupContext *c, const char *dev, const char *mode);
int cgroup_add_bpf_foreign_program(CGroupContext *c, uint32_t attach_type, const char *path);

void cgroup_oomd_xattr_apply(Unit *u, const char *cgroup_path);
int cgroup_log_xattr_apply(Unit *u, const char *cgroup_path);

CGroupMask unit_get_own_mask(Unit *u);
CGroupMask unit_get_delegate_mask(Unit *u);
CGroupMask unit_get_members_mask(Unit *u);
CGroupMask unit_get_siblings_mask(Unit *u);
CGroupMask unit_get_ancestor_disable_mask(Unit *u);

CGroupMask unit_get_target_mask(Unit *u);
CGroupMask unit_get_enable_mask(Unit *u);

void unit_invalidate_cgroup_members_masks(Unit *u);

void unit_add_family_to_cgroup_realize_queue(Unit *u);

const char *unit_get_realized_cgroup_path(Unit *u, CGroupMask mask);
int unit_default_cgroup_path(const Unit *u, char **ret);
int unit_set_cgroup_path(Unit *u, const char *path);
int unit_pick_cgroup_path(Unit *u);

int unit_realize_cgroup(Unit *u);
void unit_prune_cgroup(Unit *u);
int unit_watch_cgroup(Unit *u);
int unit_watch_cgroup_memory(Unit *u);
void unit_add_to_cgroup_realize_queue(Unit *u);

void unit_release_cgroup(Unit *u);
/* Releases the cgroup only if it is recursively empty.
 * Returns true if the cgroup was released, false otherwise. */
bool unit_maybe_release_cgroup(Unit *u);

void unit_add_to_cgroup_empty_queue(Unit *u);
int unit_check_oomd_kill(Unit *u);
int unit_check_oom(Unit *u);

int unit_attach_pids_to_cgroup(Unit *u, Set *pids, const char *suffix_path);
#else // 0
# include "logind.h"
#endif // 0

int manager_setup_cgroup(Manager *m);
void manager_shutdown_cgroup(Manager *m, bool delete);

#if 0 /// UNNEEDED by elogind
unsigned manager_dispatch_cgroup_realize_queue(Manager *m);

Unit *manager_get_unit_by_cgroup(Manager *m, const char *cgroup);
Unit *manager_get_unit_by_pid_cgroup(Manager *m, pid_t pid);
Unit* manager_get_unit_by_pid(Manager *m, pid_t pid);

uint64_t unit_get_ancestor_memory_min(Unit *u);
uint64_t unit_get_ancestor_memory_low(Unit *u);
uint64_t unit_get_ancestor_startup_memory_low(Unit *u);

int unit_search_main_pid(Unit *u, pid_t *ret);
int unit_watch_all_pids(Unit *u);

int unit_synthesize_cgroup_empty_event(Unit *u);

int unit_get_memory_current(Unit *u, uint64_t *ret);
int unit_get_memory_available(Unit *u, uint64_t *ret);
int unit_get_tasks_current(Unit *u, uint64_t *ret);
int unit_get_cpu_usage(Unit *u, nsec_t *ret);
int unit_get_io_accounting(Unit *u, CGroupIOAccountingMetric metric, bool allow_cache, uint64_t *ret);
int unit_get_ip_accounting(Unit *u, CGroupIPAccountingMetric metric, uint64_t *ret);

int unit_reset_cpu_accounting(Unit *u);
int unit_reset_ip_accounting(Unit *u);
int unit_reset_io_accounting(Unit *u);
int unit_reset_accounting(Unit *u);

#define UNIT_CGROUP_BOOL(u, name)                       \
        ({                                              \
        CGroupContext *cc = unit_get_cgroup_context(u); \
        cc ? cc->name : false;                          \
        })

bool manager_owns_host_root_cgroup(Manager *m);
bool unit_has_host_root_cgroup(Unit *u);

bool unit_has_startup_cgroup_constraints(Unit *u);
#endif // 0

int manager_notify_cgroup_empty(Manager *m, const char *group);

#if 0 /// UNNEEDED by elogind
void unit_invalidate_cgroup(Unit *u, CGroupMask m);
void unit_invalidate_cgroup_bpf(Unit *u);

void manager_invalidate_startup_units(Manager *m);

const char* cgroup_device_policy_to_string(CGroupDevicePolicy i) _const_;
CGroupDevicePolicy cgroup_device_policy_from_string(const char *s) _pure_;

void unit_cgroup_catchup(Unit *u);

bool unit_cgroup_delegate(Unit *u);

int compare_job_priority(const void *a, const void *b);

int unit_get_cpuset(Unit *u, CPUSet *cpus, const char *name);
int unit_cgroup_freezer_action(Unit *u, FreezerAction action);

const char* freezer_action_to_string(FreezerAction a) _const_;
FreezerAction freezer_action_from_string(const char *s) _pure_;
#endif // 0

const char* cgroup_pressure_watch_to_string(CGroupPressureWatch a) _const_;
CGroupPressureWatch cgroup_pressure_watch_from_string(const char *s) _pure_;
