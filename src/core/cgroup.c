/* SPDX-License-Identifier: LGPL-2.1+ */
/***
  This file is part of systemd.

  Copyright 2013 Lennart Poettering
***/

#include <fcntl.h>
#include <fnmatch.h>

#include "alloc-util.h"
//#include "blockdev-util.h"
//#include "bpf-firewall.h"
//#include "bus-error.h"
#include "cgroup-util.h"
#include "cgroup.h"
#include "fd-util.h"
#include "fileio.h"
#include "fs-util.h"
#include "parse-util.h"
#include "path-util.h"
#include "process-util.h"
//#include "procfs-util.h"
//#include "special.h"
#include "stdio-util.h"
#include "string-table.h"
#include "string-util.h"
#include "virt.h"

#define CGROUP_CPU_QUOTA_PERIOD_USEC ((usec_t) 100 * USEC_PER_MSEC)

bool manager_owns_root_cgroup(Manager *m) {
        assert(m);

        /* Returns true if we are managing the root cgroup. Note that it isn't sufficient to just check whether the
         * group root path equals "/" since that will also be the case if CLONE_NEWCGROUP is in the mix. Since there's
         * appears to be no nice way to detect whether we are in a CLONE_NEWCGROUP namespace we instead just check if
         * we run in any kind of container virtualization. */

        if (detect_container() > 0)
                return false;

        return empty_or_root(m->cgroup_root);
}

#if 0 /// UNNEEDED by elogind
bool unit_has_root_cgroup(Unit *u) {
        assert(u);

        /* Returns whether this unit manages the root cgroup. This will return true if this unit is the root slice and
         * the manager manages the root cgroup. */

        if (!manager_owns_root_cgroup(u->manager))
                return false;

        return unit_has_name(u, SPECIAL_ROOT_SLICE);
}

static void cgroup_compat_warn(void) {
        static bool cgroup_compat_warned = false;

        if (cgroup_compat_warned)
                return;

        log_warning("cgroup compatibility translation between legacy and unified hierarchy settings activated. "
                    "See cgroup-compat debug messages for details.");

        cgroup_compat_warned = true;
}

#define log_cgroup_compat(unit, fmt, ...) do {                                  \
                cgroup_compat_warn();                                           \
                log_unit_debug(unit, "cgroup-compat: " fmt, ##__VA_ARGS__);     \
        } while (false)

void cgroup_context_init(CGroupContext *c) {
        assert(c);

        /* Initialize everything to the kernel defaults, assuming the
         * structure is preinitialized to 0 */

        c->cpu_weight = CGROUP_WEIGHT_INVALID;
        c->startup_cpu_weight = CGROUP_WEIGHT_INVALID;
        c->cpu_quota_per_sec_usec = USEC_INFINITY;

        c->cpu_shares = CGROUP_CPU_SHARES_INVALID;
        c->startup_cpu_shares = CGROUP_CPU_SHARES_INVALID;

        c->memory_high = CGROUP_LIMIT_MAX;
        c->memory_max = CGROUP_LIMIT_MAX;
        c->memory_swap_max = CGROUP_LIMIT_MAX;

        c->memory_limit = CGROUP_LIMIT_MAX;

        c->io_weight = CGROUP_WEIGHT_INVALID;
        c->startup_io_weight = CGROUP_WEIGHT_INVALID;

        c->blockio_weight = CGROUP_BLKIO_WEIGHT_INVALID;
        c->startup_blockio_weight = CGROUP_BLKIO_WEIGHT_INVALID;

        c->tasks_max = (uint64_t) -1;
}

void cgroup_context_free_device_allow(CGroupContext *c, CGroupDeviceAllow *a) {
        assert(c);
        assert(a);

        LIST_REMOVE(device_allow, c->device_allow, a);
        free(a->path);
        free(a);
}

void cgroup_context_free_io_device_weight(CGroupContext *c, CGroupIODeviceWeight *w) {
        assert(c);
        assert(w);

        LIST_REMOVE(device_weights, c->io_device_weights, w);
        free(w->path);
        free(w);
}

void cgroup_context_free_io_device_limit(CGroupContext *c, CGroupIODeviceLimit *l) {
        assert(c);
        assert(l);

        LIST_REMOVE(device_limits, c->io_device_limits, l);
        free(l->path);
        free(l);
}

void cgroup_context_free_blockio_device_weight(CGroupContext *c, CGroupBlockIODeviceWeight *w) {
        assert(c);
        assert(w);

        LIST_REMOVE(device_weights, c->blockio_device_weights, w);
        free(w->path);
        free(w);
}

void cgroup_context_free_blockio_device_bandwidth(CGroupContext *c, CGroupBlockIODeviceBandwidth *b) {
        assert(c);
        assert(b);

        LIST_REMOVE(device_bandwidths, c->blockio_device_bandwidths, b);
        free(b->path);
        free(b);
}

void cgroup_context_done(CGroupContext *c) {
        assert(c);

        while (c->io_device_weights)
                cgroup_context_free_io_device_weight(c, c->io_device_weights);

        while (c->io_device_limits)
                cgroup_context_free_io_device_limit(c, c->io_device_limits);

        while (c->blockio_device_weights)
                cgroup_context_free_blockio_device_weight(c, c->blockio_device_weights);

        while (c->blockio_device_bandwidths)
                cgroup_context_free_blockio_device_bandwidth(c, c->blockio_device_bandwidths);

        while (c->device_allow)
                cgroup_context_free_device_allow(c, c->device_allow);

        c->ip_address_allow = ip_address_access_free_all(c->ip_address_allow);
        c->ip_address_deny = ip_address_access_free_all(c->ip_address_deny);
}

void cgroup_context_dump(CGroupContext *c, FILE* f, const char *prefix) {
        CGroupIODeviceLimit *il;
        CGroupIODeviceWeight *iw;
        CGroupBlockIODeviceBandwidth *b;
        CGroupBlockIODeviceWeight *w;
        CGroupDeviceAllow *a;
        IPAddressAccessItem *iaai;
        char u[FORMAT_TIMESPAN_MAX];

        assert(c);
        assert(f);

        prefix = strempty(prefix);

        fprintf(f,
                "%sCPUAccounting=%s\n"
                "%sIOAccounting=%s\n"
                "%sBlockIOAccounting=%s\n"
                "%sMemoryAccounting=%s\n"
                "%sTasksAccounting=%s\n"
                "%sIPAccounting=%s\n"
                "%sCPUWeight=%" PRIu64 "\n"
                "%sStartupCPUWeight=%" PRIu64 "\n"
                "%sCPUShares=%" PRIu64 "\n"
                "%sStartupCPUShares=%" PRIu64 "\n"
                "%sCPUQuotaPerSecSec=%s\n"
                "%sIOWeight=%" PRIu64 "\n"
                "%sStartupIOWeight=%" PRIu64 "\n"
                "%sBlockIOWeight=%" PRIu64 "\n"
                "%sStartupBlockIOWeight=%" PRIu64 "\n"
                "%sMemoryLow=%" PRIu64 "\n"
                "%sMemoryHigh=%" PRIu64 "\n"
                "%sMemoryMax=%" PRIu64 "\n"
                "%sMemorySwapMax=%" PRIu64 "\n"
                "%sMemoryLimit=%" PRIu64 "\n"
                "%sTasksMax=%" PRIu64 "\n"
                "%sDevicePolicy=%s\n"
                "%sDelegate=%s\n",
                prefix, yes_no(c->cpu_accounting),
                prefix, yes_no(c->io_accounting),
                prefix, yes_no(c->blockio_accounting),
                prefix, yes_no(c->memory_accounting),
                prefix, yes_no(c->tasks_accounting),
                prefix, yes_no(c->ip_accounting),
                prefix, c->cpu_weight,
                prefix, c->startup_cpu_weight,
                prefix, c->cpu_shares,
                prefix, c->startup_cpu_shares,
                prefix, format_timespan(u, sizeof(u), c->cpu_quota_per_sec_usec, 1),
                prefix, c->io_weight,
                prefix, c->startup_io_weight,
                prefix, c->blockio_weight,
                prefix, c->startup_blockio_weight,
                prefix, c->memory_low,
                prefix, c->memory_high,
                prefix, c->memory_max,
                prefix, c->memory_swap_max,
                prefix, c->memory_limit,
                prefix, c->tasks_max,
                prefix, cgroup_device_policy_to_string(c->device_policy),
                prefix, yes_no(c->delegate));

        if (c->delegate) {
                _cleanup_free_ char *t = NULL;

                (void) cg_mask_to_string(c->delegate_controllers, &t);

                fprintf(f, "%sDelegateControllers=%s\n",
                        prefix,
                        strempty(t));
        }

        LIST_FOREACH(device_allow, a, c->device_allow)
                fprintf(f,
                        "%sDeviceAllow=%s %s%s%s\n",
                        prefix,
                        a->path,
                        a->r ? "r" : "", a->w ? "w" : "", a->m ? "m" : "");

        LIST_FOREACH(device_weights, iw, c->io_device_weights)
                fprintf(f,
                        "%sIODeviceWeight=%s %" PRIu64,
                        prefix,
                        iw->path,
                        iw->weight);

        LIST_FOREACH(device_limits, il, c->io_device_limits) {
                char buf[FORMAT_BYTES_MAX];
                CGroupIOLimitType type;

                for (type = 0; type < _CGROUP_IO_LIMIT_TYPE_MAX; type++)
                        if (il->limits[type] != cgroup_io_limit_defaults[type])
                                fprintf(f,
                                        "%s%s=%s %s\n",
                                        prefix,
                                        cgroup_io_limit_type_to_string(type),
                                        il->path,
                                        format_bytes(buf, sizeof(buf), il->limits[type]));
        }

        LIST_FOREACH(device_weights, w, c->blockio_device_weights)
                fprintf(f,
                        "%sBlockIODeviceWeight=%s %" PRIu64,
                        prefix,
                        w->path,
                        w->weight);

        LIST_FOREACH(device_bandwidths, b, c->blockio_device_bandwidths) {
                char buf[FORMAT_BYTES_MAX];

                if (b->rbps != CGROUP_LIMIT_MAX)
                        fprintf(f,
                                "%sBlockIOReadBandwidth=%s %s\n",
                                prefix,
                                b->path,
                                format_bytes(buf, sizeof(buf), b->rbps));
                if (b->wbps != CGROUP_LIMIT_MAX)
                        fprintf(f,
                                "%sBlockIOWriteBandwidth=%s %s\n",
                                prefix,
                                b->path,
                                format_bytes(buf, sizeof(buf), b->wbps));
        }

        LIST_FOREACH(items, iaai, c->ip_address_allow) {
                _cleanup_free_ char *k = NULL;

                (void) in_addr_to_string(iaai->family, &iaai->address, &k);
                fprintf(f, "%sIPAddressAllow=%s/%u\n", prefix, strnull(k), iaai->prefixlen);
        }

        LIST_FOREACH(items, iaai, c->ip_address_deny) {
                _cleanup_free_ char *k = NULL;

                (void) in_addr_to_string(iaai->family, &iaai->address, &k);
                fprintf(f, "%sIPAddressDeny=%s/%u\n", prefix, strnull(k), iaai->prefixlen);
        }
}

static int lookup_block_device(const char *p, dev_t *dev) {
        struct stat st;

        assert(p);
        assert(dev);

        if (stat(p, &st) < 0)
                return log_warning_errno(errno, "Couldn't stat device %s: %m", p);

        if (S_ISBLK(st.st_mode))
                *dev = st.st_rdev;
        else if (major(st.st_dev) != 0) {
                /* If this is not a device node then find the block
                 * device this file is stored on */
                *dev = st.st_dev;

                /* If this is a partition, try to get the originating
                 * block device */
                (void) block_get_whole_disk(*dev, dev);
        } else {
                log_warning("%s is not a block device and file system block device cannot be determined or is not local.", p);
                return -ENODEV;
        }

        return 0;
}

static int whitelist_device(const char *path, const char *node, const char *acc) {
        char buf[2+DECIMAL_STR_MAX(dev_t)*2+2+4];
        struct stat st;
        bool ignore_notfound;
        int r;

        assert(path);
        assert(acc);

        if (node[0] == '-') {
                /* Non-existent paths starting with "-" must be silently ignored */
                node++;
                ignore_notfound = true;
        } else
                ignore_notfound = false;

        if (stat(node, &st) < 0) {
                if (errno == ENOENT && ignore_notfound)
                        return 0;

                return log_warning_errno(errno, "Couldn't stat device %s: %m", node);
        }

        if (!S_ISCHR(st.st_mode) && !S_ISBLK(st.st_mode)) {
                log_warning("%s is not a device.", node);
                return -ENODEV;
        }

        sprintf(buf,
                "%c %u:%u %s",
                S_ISCHR(st.st_mode) ? 'c' : 'b',
                major(st.st_rdev), minor(st.st_rdev),
                acc);

        r = cg_set_attribute("devices", path, "devices.allow", buf);
        if (r < 0)
                log_full_errno(IN_SET(r, -ENOENT, -EROFS, -EINVAL, -EACCES) ? LOG_DEBUG : LOG_WARNING, r,
                               "Failed to set devices.allow on %s: %m", path);

        return r;
}

static int whitelist_major(const char *path, const char *name, char type, const char *acc) {
        _cleanup_fclose_ FILE *f = NULL;
        char line[LINE_MAX];
        bool good = false;
        int r;

        assert(path);
        assert(acc);
        assert(IN_SET(type, 'b', 'c'));

        f = fopen("/proc/devices", "re");
        if (!f)
                return log_warning_errno(errno, "Cannot open /proc/devices to resolve %s (%c): %m", name, type);

        FOREACH_LINE(line, f, goto fail) {
                char buf[2+DECIMAL_STR_MAX(unsigned)+3+4], *p, *w;
                unsigned maj;

                truncate_nl(line);

                if (type == 'c' && streq(line, "Character devices:")) {
                        good = true;
                        continue;
                }

                if (type == 'b' && streq(line, "Block devices:")) {
                        good = true;
                        continue;
                }

                if (isempty(line)) {
                        good = false;
                        continue;
                }

                if (!good)
                        continue;

                p = strstrip(line);

                w = strpbrk(p, WHITESPACE);
                if (!w)
                        continue;
                *w = 0;

                r = safe_atou(p, &maj);
                if (r < 0)
                        continue;
                if (maj <= 0)
                        continue;

                w++;
                w += strspn(w, WHITESPACE);

                if (fnmatch(name, w, 0) != 0)
                        continue;

                sprintf(buf,
                        "%c %u:* %s",
                        type,
                        maj,
                        acc);

                r = cg_set_attribute("devices", path, "devices.allow", buf);
                if (r < 0)
                        log_full_errno(IN_SET(r, -ENOENT, -EROFS, -EINVAL, -EACCES) ? LOG_DEBUG : LOG_WARNING, r,
                                       "Failed to set devices.allow on %s: %m", path);
        }

        return 0;

fail:
        return log_warning_errno(errno, "Failed to read /proc/devices: %m");
}

static bool cgroup_context_has_cpu_weight(CGroupContext *c) {
        return c->cpu_weight != CGROUP_WEIGHT_INVALID ||
                c->startup_cpu_weight != CGROUP_WEIGHT_INVALID;
}

static bool cgroup_context_has_cpu_shares(CGroupContext *c) {
        return c->cpu_shares != CGROUP_CPU_SHARES_INVALID ||
                c->startup_cpu_shares != CGROUP_CPU_SHARES_INVALID;
}

static uint64_t cgroup_context_cpu_weight(CGroupContext *c, ManagerState state) {
        if (IN_SET(state, MANAGER_STARTING, MANAGER_INITIALIZING) &&
            c->startup_cpu_weight != CGROUP_WEIGHT_INVALID)
                return c->startup_cpu_weight;
        else if (c->cpu_weight != CGROUP_WEIGHT_INVALID)
                return c->cpu_weight;
        else
                return CGROUP_WEIGHT_DEFAULT;
}

static uint64_t cgroup_context_cpu_shares(CGroupContext *c, ManagerState state) {
        if (IN_SET(state, MANAGER_STARTING, MANAGER_INITIALIZING) &&
            c->startup_cpu_shares != CGROUP_CPU_SHARES_INVALID)
                return c->startup_cpu_shares;
        else if (c->cpu_shares != CGROUP_CPU_SHARES_INVALID)
                return c->cpu_shares;
        else
                return CGROUP_CPU_SHARES_DEFAULT;
}

static void cgroup_apply_unified_cpu_config(Unit *u, uint64_t weight, uint64_t quota) {
        char buf[MAX(DECIMAL_STR_MAX(uint64_t) + 1, (DECIMAL_STR_MAX(usec_t) + 1) * 2)];
        int r;

        xsprintf(buf, "%" PRIu64 "\n", weight);
        r = cg_set_attribute("cpu", u->cgroup_path, "cpu.weight", buf);
        if (r < 0)
                log_unit_full(u, IN_SET(r, -ENOENT, -EROFS, -EACCES) ? LOG_DEBUG : LOG_WARNING, r,
                              "Failed to set cpu.weight: %m");

        if (quota != USEC_INFINITY)
                xsprintf(buf, USEC_FMT " " USEC_FMT "\n",
                         quota * CGROUP_CPU_QUOTA_PERIOD_USEC / USEC_PER_SEC, CGROUP_CPU_QUOTA_PERIOD_USEC);
        else
                xsprintf(buf, "max " USEC_FMT "\n", CGROUP_CPU_QUOTA_PERIOD_USEC);

        r = cg_set_attribute("cpu", u->cgroup_path, "cpu.max", buf);

        if (r < 0)
                log_unit_full(u, IN_SET(r, -ENOENT, -EROFS, -EACCES) ? LOG_DEBUG : LOG_WARNING, r,
                              "Failed to set cpu.max: %m");
}

static void cgroup_apply_legacy_cpu_config(Unit *u, uint64_t shares, uint64_t quota) {
        char buf[MAX(DECIMAL_STR_MAX(uint64_t), DECIMAL_STR_MAX(usec_t)) + 1];
        int r;

        xsprintf(buf, "%" PRIu64 "\n", shares);
        r = cg_set_attribute("cpu", u->cgroup_path, "cpu.shares", buf);
        if (r < 0)
                log_unit_full(u, IN_SET(r, -ENOENT, -EROFS, -EACCES) ? LOG_DEBUG : LOG_WARNING, r,
                              "Failed to set cpu.shares: %m");

        xsprintf(buf, USEC_FMT "\n", CGROUP_CPU_QUOTA_PERIOD_USEC);
        r = cg_set_attribute("cpu", u->cgroup_path, "cpu.cfs_period_us", buf);
        if (r < 0)
                log_unit_full(u, IN_SET(r, -ENOENT, -EROFS, -EACCES) ? LOG_DEBUG : LOG_WARNING, r,
                              "Failed to set cpu.cfs_period_us: %m");

        if (quota != USEC_INFINITY) {
                xsprintf(buf, USEC_FMT "\n", quota * CGROUP_CPU_QUOTA_PERIOD_USEC / USEC_PER_SEC);
                r = cg_set_attribute("cpu", u->cgroup_path, "cpu.cfs_quota_us", buf);
        } else
                r = cg_set_attribute("cpu", u->cgroup_path, "cpu.cfs_quota_us", "-1");
        if (r < 0)
                log_unit_full(u, IN_SET(r, -ENOENT, -EROFS, -EACCES) ? LOG_DEBUG : LOG_WARNING, r,
                              "Failed to set cpu.cfs_quota_us: %m");
}

static uint64_t cgroup_cpu_shares_to_weight(uint64_t shares) {
        return CLAMP(shares * CGROUP_WEIGHT_DEFAULT / CGROUP_CPU_SHARES_DEFAULT,
                     CGROUP_WEIGHT_MIN, CGROUP_WEIGHT_MAX);
}

static uint64_t cgroup_cpu_weight_to_shares(uint64_t weight) {
        return CLAMP(weight * CGROUP_CPU_SHARES_DEFAULT / CGROUP_WEIGHT_DEFAULT,
                     CGROUP_CPU_SHARES_MIN, CGROUP_CPU_SHARES_MAX);
}

static bool cgroup_context_has_io_config(CGroupContext *c) {
        return c->io_accounting ||
                c->io_weight != CGROUP_WEIGHT_INVALID ||
                c->startup_io_weight != CGROUP_WEIGHT_INVALID ||
                c->io_device_weights ||
                c->io_device_limits;
}

static bool cgroup_context_has_blockio_config(CGroupContext *c) {
        return c->blockio_accounting ||
                c->blockio_weight != CGROUP_BLKIO_WEIGHT_INVALID ||
                c->startup_blockio_weight != CGROUP_BLKIO_WEIGHT_INVALID ||
                c->blockio_device_weights ||
                c->blockio_device_bandwidths;
}

static uint64_t cgroup_context_io_weight(CGroupContext *c, ManagerState state) {
        if (IN_SET(state, MANAGER_STARTING, MANAGER_INITIALIZING) &&
            c->startup_io_weight != CGROUP_WEIGHT_INVALID)
                return c->startup_io_weight;
        else if (c->io_weight != CGROUP_WEIGHT_INVALID)
                return c->io_weight;
        else
                return CGROUP_WEIGHT_DEFAULT;
}

static uint64_t cgroup_context_blkio_weight(CGroupContext *c, ManagerState state) {
        if (IN_SET(state, MANAGER_STARTING, MANAGER_INITIALIZING) &&
            c->startup_blockio_weight != CGROUP_BLKIO_WEIGHT_INVALID)
                return c->startup_blockio_weight;
        else if (c->blockio_weight != CGROUP_BLKIO_WEIGHT_INVALID)
                return c->blockio_weight;
        else
                return CGROUP_BLKIO_WEIGHT_DEFAULT;
}

static uint64_t cgroup_weight_blkio_to_io(uint64_t blkio_weight) {
        return CLAMP(blkio_weight * CGROUP_WEIGHT_DEFAULT / CGROUP_BLKIO_WEIGHT_DEFAULT,
                     CGROUP_WEIGHT_MIN, CGROUP_WEIGHT_MAX);
}

static uint64_t cgroup_weight_io_to_blkio(uint64_t io_weight) {
        return CLAMP(io_weight * CGROUP_BLKIO_WEIGHT_DEFAULT / CGROUP_WEIGHT_DEFAULT,
                     CGROUP_BLKIO_WEIGHT_MIN, CGROUP_BLKIO_WEIGHT_MAX);
}

static void cgroup_apply_io_device_weight(Unit *u, const char *dev_path, uint64_t io_weight) {
        char buf[DECIMAL_STR_MAX(dev_t)*2+2+DECIMAL_STR_MAX(uint64_t)+1];
        dev_t dev;
        int r;

        r = lookup_block_device(dev_path, &dev);
        if (r < 0)
                return;

        xsprintf(buf, "%u:%u %" PRIu64 "\n", major(dev), minor(dev), io_weight);
        r = cg_set_attribute("io", u->cgroup_path, "io.weight", buf);
        if (r < 0)
                log_unit_full(u, IN_SET(r, -ENOENT, -EROFS, -EACCES) ? LOG_DEBUG : LOG_WARNING, r,
                              "Failed to set io.weight: %m");
}

static void cgroup_apply_blkio_device_weight(Unit *u, const char *dev_path, uint64_t blkio_weight) {
        char buf[DECIMAL_STR_MAX(dev_t)*2+2+DECIMAL_STR_MAX(uint64_t)+1];
        dev_t dev;
        int r;

        r = lookup_block_device(dev_path, &dev);
        if (r < 0)
                return;

        xsprintf(buf, "%u:%u %" PRIu64 "\n", major(dev), minor(dev), blkio_weight);
        r = cg_set_attribute("blkio", u->cgroup_path, "blkio.weight_device", buf);
        if (r < 0)
                log_unit_full(u, IN_SET(r, -ENOENT, -EROFS, -EACCES) ? LOG_DEBUG : LOG_WARNING, r,
                              "Failed to set blkio.weight_device: %m");
}

static unsigned cgroup_apply_io_device_limit(Unit *u, const char *dev_path, uint64_t *limits) {
        char limit_bufs[_CGROUP_IO_LIMIT_TYPE_MAX][DECIMAL_STR_MAX(uint64_t)];
        char buf[DECIMAL_STR_MAX(dev_t)*2+2+(6+DECIMAL_STR_MAX(uint64_t)+1)*4];
        CGroupIOLimitType type;
        dev_t dev;
        unsigned n = 0;
        int r;

        r = lookup_block_device(dev_path, &dev);
        if (r < 0)
                return 0;

        for (type = 0; type < _CGROUP_IO_LIMIT_TYPE_MAX; type++) {
                if (limits[type] != cgroup_io_limit_defaults[type]) {
                        xsprintf(limit_bufs[type], "%" PRIu64, limits[type]);
                        n++;
                } else {
                        xsprintf(limit_bufs[type], "%s", limits[type] == CGROUP_LIMIT_MAX ? "max" : "0");
                }
        }

        xsprintf(buf, "%u:%u rbps=%s wbps=%s riops=%s wiops=%s\n", major(dev), minor(dev),
                 limit_bufs[CGROUP_IO_RBPS_MAX], limit_bufs[CGROUP_IO_WBPS_MAX],
                 limit_bufs[CGROUP_IO_RIOPS_MAX], limit_bufs[CGROUP_IO_WIOPS_MAX]);
        r = cg_set_attribute("io", u->cgroup_path, "io.max", buf);
        if (r < 0)
                log_unit_full(u, IN_SET(r, -ENOENT, -EROFS, -EACCES) ? LOG_DEBUG : LOG_WARNING, r,
                              "Failed to set io.max: %m");
        return n;
}

static unsigned cgroup_apply_blkio_device_limit(Unit *u, const char *dev_path, uint64_t rbps, uint64_t wbps) {
        char buf[DECIMAL_STR_MAX(dev_t)*2+2+DECIMAL_STR_MAX(uint64_t)+1];
        dev_t dev;
        unsigned n = 0;
        int r;

        r = lookup_block_device(dev_path, &dev);
        if (r < 0)
                return 0;

        if (rbps != CGROUP_LIMIT_MAX)
                n++;
        sprintf(buf, "%u:%u %" PRIu64 "\n", major(dev), minor(dev), rbps);
        r = cg_set_attribute("blkio", u->cgroup_path, "blkio.throttle.read_bps_device", buf);
        if (r < 0)
                log_unit_full(u, IN_SET(r, -ENOENT, -EROFS, -EACCES) ? LOG_DEBUG : LOG_WARNING, r,
                              "Failed to set blkio.throttle.read_bps_device: %m");

        if (wbps != CGROUP_LIMIT_MAX)
                n++;
        sprintf(buf, "%u:%u %" PRIu64 "\n", major(dev), minor(dev), wbps);
        r = cg_set_attribute("blkio", u->cgroup_path, "blkio.throttle.write_bps_device", buf);
        if (r < 0)
                log_unit_full(u, IN_SET(r, -ENOENT, -EROFS, -EACCES) ? LOG_DEBUG : LOG_WARNING, r,
                              "Failed to set blkio.throttle.write_bps_device: %m");

        return n;
}

static bool cgroup_context_has_unified_memory_config(CGroupContext *c) {
        return c->memory_low > 0 || c->memory_high != CGROUP_LIMIT_MAX || c->memory_max != CGROUP_LIMIT_MAX || c->memory_swap_max != CGROUP_LIMIT_MAX;
}

static void cgroup_apply_unified_memory_limit(Unit *u, const char *file, uint64_t v) {
        char buf[DECIMAL_STR_MAX(uint64_t) + 1] = "max";
        int r;

        if (v != CGROUP_LIMIT_MAX)
                xsprintf(buf, "%" PRIu64 "\n", v);

        r = cg_set_attribute("memory", u->cgroup_path, file, buf);
        if (r < 0)
                log_unit_full(u, IN_SET(r, -ENOENT, -EROFS, -EACCES) ? LOG_DEBUG : LOG_WARNING, r,
                              "Failed to set %s: %m", file);
}

static void cgroup_apply_firewall(Unit *u) {
        assert(u);

        /* Best-effort: let's apply IP firewalling and/or accounting if that's enabled */

        if (bpf_firewall_compile(u) < 0)
                return;

        (void) bpf_firewall_install(u);
}

static void cgroup_context_apply(
                Unit *u,
                CGroupMask apply_mask,
                bool apply_bpf,
                ManagerState state) {

        const char *path;
        CGroupContext *c;
        bool is_root;
        int r;

        assert(u);

        /* Nothing to do? Exit early! */
        if (apply_mask == 0 && !apply_bpf)
                return;

        /* Some cgroup attributes are not supported on the root cgroup, hence silently ignore */
        is_root = unit_has_root_cgroup(u);

        assert_se(c = unit_get_cgroup_context(u));
        assert_se(path = u->cgroup_path);

        if (is_root) /* Make sure we don't try to display messages with an empty path. */
                path = "/";

        /* We generally ignore errors caused by read-only mounted
         * cgroup trees (assuming we are running in a container then),
         * and missing cgroups, i.e. EROFS and ENOENT. */

        if ((apply_mask & CGROUP_MASK_CPU) && !is_root) {
                bool has_weight, has_shares;

                has_weight = cgroup_context_has_cpu_weight(c);
                has_shares = cgroup_context_has_cpu_shares(c);

                if (cg_all_unified() > 0) {
                        uint64_t weight;

                        if (has_weight)
                                weight = cgroup_context_cpu_weight(c, state);
                        else if (has_shares) {
                                uint64_t shares = cgroup_context_cpu_shares(c, state);

                                weight = cgroup_cpu_shares_to_weight(shares);

                                log_cgroup_compat(u, "Applying [Startup]CpuShares %" PRIu64 " as [Startup]CpuWeight %" PRIu64 " on %s",
                                                  shares, weight, path);
                        } else
                                weight = CGROUP_WEIGHT_DEFAULT;

                        cgroup_apply_unified_cpu_config(u, weight, c->cpu_quota_per_sec_usec);
                } else {
                        uint64_t shares;

                        if (has_weight) {
                                uint64_t weight = cgroup_context_cpu_weight(c, state);

                                shares = cgroup_cpu_weight_to_shares(weight);

                                log_cgroup_compat(u, "Applying [Startup]CpuWeight %" PRIu64 " as [Startup]CpuShares %" PRIu64 " on %s",
                                                  weight, shares, path);
                        } else if (has_shares)
                                shares = cgroup_context_cpu_shares(c, state);
                        else
                                shares = CGROUP_CPU_SHARES_DEFAULT;

                        cgroup_apply_legacy_cpu_config(u, shares, c->cpu_quota_per_sec_usec);
                }
        }

        if (apply_mask & CGROUP_MASK_IO) {
                bool has_io = cgroup_context_has_io_config(c);
                bool has_blockio = cgroup_context_has_blockio_config(c);

                if (!is_root) {
                        char buf[8+DECIMAL_STR_MAX(uint64_t)+1];
                        uint64_t weight;

                        if (has_io)
                                weight = cgroup_context_io_weight(c, state);
                        else if (has_blockio) {
                                uint64_t blkio_weight = cgroup_context_blkio_weight(c, state);

                                weight = cgroup_weight_blkio_to_io(blkio_weight);

                                log_cgroup_compat(u, "Applying [Startup]BlockIOWeight %" PRIu64 " as [Startup]IOWeight %" PRIu64,
                                                  blkio_weight, weight);
                        } else
                                weight = CGROUP_WEIGHT_DEFAULT;

                        xsprintf(buf, "default %" PRIu64 "\n", weight);
                        r = cg_set_attribute("io", path, "io.weight", buf);
                        if (r < 0)
                                log_unit_full(u, IN_SET(r, -ENOENT, -EROFS, -EACCES) ? LOG_DEBUG : LOG_WARNING, r,
                                              "Failed to set io.weight: %m");

                        if (has_io) {
                                CGroupIODeviceWeight *w;

                                /* FIXME: no way to reset this list */
                                LIST_FOREACH(device_weights, w, c->io_device_weights)
                                        cgroup_apply_io_device_weight(u, w->path, w->weight);
                        } else if (has_blockio) {
                                CGroupBlockIODeviceWeight *w;

                                /* FIXME: no way to reset this list */
                                LIST_FOREACH(device_weights, w, c->blockio_device_weights) {
                                        weight = cgroup_weight_blkio_to_io(w->weight);

                                        log_cgroup_compat(u, "Applying BlockIODeviceWeight %" PRIu64 " as IODeviceWeight %" PRIu64 " for %s",
                                                          w->weight, weight, w->path);

                                        cgroup_apply_io_device_weight(u, w->path, weight);
                                }
                        }
                }

                /* Apply limits and free ones without config. */
                if (has_io) {
                        CGroupIODeviceLimit *l, *next;

                        LIST_FOREACH_SAFE(device_limits, l, next, c->io_device_limits) {
                                if (!cgroup_apply_io_device_limit(u, l->path, l->limits))
                                        cgroup_context_free_io_device_limit(c, l);
                        }
                } else if (has_blockio) {
                        CGroupBlockIODeviceBandwidth *b, *next;

                        LIST_FOREACH_SAFE(device_bandwidths, b, next, c->blockio_device_bandwidths) {
                                uint64_t limits[_CGROUP_IO_LIMIT_TYPE_MAX];
                                CGroupIOLimitType type;

                                for (type = 0; type < _CGROUP_IO_LIMIT_TYPE_MAX; type++)
                                        limits[type] = cgroup_io_limit_defaults[type];

                                limits[CGROUP_IO_RBPS_MAX] = b->rbps;
                                limits[CGROUP_IO_WBPS_MAX] = b->wbps;

                                log_cgroup_compat(u, "Applying BlockIO{Read|Write}Bandwidth %" PRIu64 " %" PRIu64 " as IO{Read|Write}BandwidthMax for %s",
                                                  b->rbps, b->wbps, b->path);

                                if (!cgroup_apply_io_device_limit(u, b->path, limits))
                                        cgroup_context_free_blockio_device_bandwidth(c, b);
                        }
                }
        }

        if (apply_mask & CGROUP_MASK_BLKIO) {
                bool has_io = cgroup_context_has_io_config(c);
                bool has_blockio = cgroup_context_has_blockio_config(c);

                if (!is_root) {
                        char buf[DECIMAL_STR_MAX(uint64_t)+1];
                        uint64_t weight;

                        if (has_io) {
                                uint64_t io_weight = cgroup_context_io_weight(c, state);

                                weight = cgroup_weight_io_to_blkio(cgroup_context_io_weight(c, state));

                                log_cgroup_compat(u, "Applying [Startup]IOWeight %" PRIu64 " as [Startup]BlockIOWeight %" PRIu64,
                                                  io_weight, weight);
                        } else if (has_blockio)
                                weight = cgroup_context_blkio_weight(c, state);
                        else
                                weight = CGROUP_BLKIO_WEIGHT_DEFAULT;

                        xsprintf(buf, "%" PRIu64 "\n", weight);
                        r = cg_set_attribute("blkio", path, "blkio.weight", buf);
                        if (r < 0)
                                log_unit_full(u, IN_SET(r, -ENOENT, -EROFS, -EACCES) ? LOG_DEBUG : LOG_WARNING, r,
                                              "Failed to set blkio.weight: %m");

                        if (has_io) {
                                CGroupIODeviceWeight *w;

                                /* FIXME: no way to reset this list */
                                LIST_FOREACH(device_weights, w, c->io_device_weights) {
                                        weight = cgroup_weight_io_to_blkio(w->weight);

                                        log_cgroup_compat(u, "Applying IODeviceWeight %" PRIu64 " as BlockIODeviceWeight %" PRIu64 " for %s",
                                                          w->weight, weight, w->path);

                                        cgroup_apply_blkio_device_weight(u, w->path, weight);
                                }
                        } else if (has_blockio) {
                                CGroupBlockIODeviceWeight *w;

                                /* FIXME: no way to reset this list */
                                LIST_FOREACH(device_weights, w, c->blockio_device_weights)
                                        cgroup_apply_blkio_device_weight(u, w->path, w->weight);
                        }
                }

                /* Apply limits and free ones without config. */
                if (has_io) {
                        CGroupIODeviceLimit *l, *next;

                        LIST_FOREACH_SAFE(device_limits, l, next, c->io_device_limits) {
                                log_cgroup_compat(u, "Applying IO{Read|Write}Bandwidth %" PRIu64 " %" PRIu64 " as BlockIO{Read|Write}BandwidthMax for %s",
                                                  l->limits[CGROUP_IO_RBPS_MAX], l->limits[CGROUP_IO_WBPS_MAX], l->path);

                                if (!cgroup_apply_blkio_device_limit(u, l->path, l->limits[CGROUP_IO_RBPS_MAX], l->limits[CGROUP_IO_WBPS_MAX]))
                                        cgroup_context_free_io_device_limit(c, l);
                        }
                } else if (has_blockio) {
                        CGroupBlockIODeviceBandwidth *b, *next;

                        LIST_FOREACH_SAFE(device_bandwidths, b, next, c->blockio_device_bandwidths)
                                if (!cgroup_apply_blkio_device_limit(u, b->path, b->rbps, b->wbps))
                                        cgroup_context_free_blockio_device_bandwidth(c, b);
                }
        }

        if ((apply_mask & CGROUP_MASK_MEMORY) && !is_root) {
                if (cg_all_unified() > 0) {
                        uint64_t max, swap_max = CGROUP_LIMIT_MAX;

                        if (cgroup_context_has_unified_memory_config(c)) {
                                max = c->memory_max;
                                swap_max = c->memory_swap_max;
                        } else {
                                max = c->memory_limit;

                                if (max != CGROUP_LIMIT_MAX)
                                        log_cgroup_compat(u, "Applying MemoryLimit %" PRIu64 " as MemoryMax", max);
                        }

                        cgroup_apply_unified_memory_limit(u, "memory.low", c->memory_low);
                        cgroup_apply_unified_memory_limit(u, "memory.high", c->memory_high);
                        cgroup_apply_unified_memory_limit(u, "memory.max", max);
                        cgroup_apply_unified_memory_limit(u, "memory.swap.max", swap_max);
                } else {
                        char buf[DECIMAL_STR_MAX(uint64_t) + 1];
                        uint64_t val;

                        if (cgroup_context_has_unified_memory_config(c)) {
                                val = c->memory_max;
                                log_cgroup_compat(u, "Applying MemoryMax %" PRIi64 " as MemoryLimit", val);
                        } else
                                val = c->memory_limit;

                        if (val == CGROUP_LIMIT_MAX)
                                strncpy(buf, "-1\n", sizeof(buf));
                        else
                                xsprintf(buf, "%" PRIu64 "\n", val);

                        r = cg_set_attribute("memory", path, "memory.limit_in_bytes", buf);
                        if (r < 0)
                                log_unit_full(u, IN_SET(r, -ENOENT, -EROFS, -EACCES) ? LOG_DEBUG : LOG_WARNING, r,
                                              "Failed to set memory.limit_in_bytes: %m");
                }
        }

        if ((apply_mask & CGROUP_MASK_DEVICES) && !is_root) {
                CGroupDeviceAllow *a;

                /* Changing the devices list of a populated cgroup
                 * might result in EINVAL, hence ignore EINVAL
                 * here. */

                if (c->device_allow || c->device_policy != CGROUP_AUTO)
                        r = cg_set_attribute("devices", path, "devices.deny", "a");
                else
                        r = cg_set_attribute("devices", path, "devices.allow", "a");
                if (r < 0)
                        log_unit_full(u, IN_SET(r, -ENOENT, -EROFS, -EINVAL, -EACCES) ? LOG_DEBUG : LOG_WARNING, r,
                                      "Failed to reset devices.list: %m");

                if (c->device_policy == CGROUP_CLOSED ||
                    (c->device_policy == CGROUP_AUTO && c->device_allow)) {
                        static const char auto_devices[] =
                                "/dev/null\0" "rwm\0"
                                "/dev/zero\0" "rwm\0"
                                "/dev/full\0" "rwm\0"
                                "/dev/random\0" "rwm\0"
                                "/dev/urandom\0" "rwm\0"
                                "/dev/tty\0" "rwm\0"
                                "/dev/ptmx\0" "rwm\0"
                                /* Allow /run/systemd/inaccessible/{chr,blk} devices for mapping InaccessiblePaths */
                                "-/run/systemd/inaccessible/chr\0" "rwm\0"
                                "-/run/systemd/inaccessible/blk\0" "rwm\0";

                        const char *x, *y;

                        NULSTR_FOREACH_PAIR(x, y, auto_devices)
                                whitelist_device(path, x, y);

                        /* PTS (/dev/pts) devices may not be duplicated, but accessed */
                        whitelist_major(path, "pts", 'c', "rw");
                }

                LIST_FOREACH(device_allow, a, c->device_allow) {
                        char acc[4], *val;
                        unsigned k = 0;

                        if (a->r)
                                acc[k++] = 'r';
                        if (a->w)
                                acc[k++] = 'w';
                        if (a->m)
                                acc[k++] = 'm';

                        if (k == 0)
                                continue;

                        acc[k++] = 0;

                        if (path_startswith(a->path, "/dev/"))
                                whitelist_device(path, a->path, acc);
                        else if ((val = startswith(a->path, "block-")))
                                whitelist_major(path, val, 'b', acc);
                        else if ((val = startswith(a->path, "char-")))
                                whitelist_major(path, val, 'c', acc);
                        else
                                log_unit_debug(u, "Ignoring device %s while writing cgroup attribute.", a->path);
                }
        }

        if (apply_mask & CGROUP_MASK_PIDS) {

                if (is_root) {
                        /* So, the "pids" controller does not expose anything on the root cgroup, in order not to
                         * replicate knobs exposed elsewhere needlessly. We abstract this away here however, and when
                         * the knobs of the root cgroup are modified propagate this to the relevant sysctls. There's a
                         * non-obvious asymmetry however: unlike the cgroup properties we don't really want to take
                         * exclusive ownership of the sysctls, but we still want to honour things if the user sets
                         * limits. Hence we employ sort of a one-way strategy: when the user sets a bounded limit
                         * through us it counts. When the user afterwards unsets it again (i.e. sets it to unbounded)
                         * it also counts. But if the user never set a limit through us (i.e. we are the default of
                         * "unbounded") we leave things unmodified. For this we manage a global boolean that we turn on
                         * the first time we set a limit. Note that this boolean is flushed out on manager reload,
                         * which is desirable so that there's an offical way to release control of the sysctl from
                         * systemd: set the limit to unbounded and reload. */

                        if (c->tasks_max != CGROUP_LIMIT_MAX) {
                                u->manager->sysctl_pid_max_changed = true;
                                r = procfs_tasks_set_limit(c->tasks_max);
                        } else if (u->manager->sysctl_pid_max_changed)
                                r = procfs_tasks_set_limit(TASKS_MAX);
                        else
                                r = 0;

                        if (r < 0)
                                log_unit_full(u, IN_SET(r, -ENOENT, -EROFS, -EACCES) ? LOG_DEBUG : LOG_WARNING, r,
                                              "Failed to write to tasks limit sysctls: %m");

                } else {
                        if (c->tasks_max != CGROUP_LIMIT_MAX) {
                                char buf[DECIMAL_STR_MAX(uint64_t) + 2];

                                sprintf(buf, "%" PRIu64 "\n", c->tasks_max);
                                r = cg_set_attribute("pids", path, "pids.max", buf);
                        } else
                                r = cg_set_attribute("pids", path, "pids.max", "max");
                        if (r < 0)
                                log_unit_full(u, IN_SET(r, -ENOENT, -EROFS, -EACCES) ? LOG_DEBUG : LOG_WARNING, r,
                                              "Failed to set pids.max: %m");
                }
        }

        if (apply_bpf)
                cgroup_apply_firewall(u);
}

CGroupMask cgroup_context_get_mask(CGroupContext *c) {
        CGroupMask mask = 0;

        /* Figure out which controllers we need */

        if (c->cpu_accounting ||
            cgroup_context_has_cpu_weight(c) ||
            cgroup_context_has_cpu_shares(c) ||
            c->cpu_quota_per_sec_usec != USEC_INFINITY)
                mask |= CGROUP_MASK_CPUACCT | CGROUP_MASK_CPU;

        if (cgroup_context_has_io_config(c) || cgroup_context_has_blockio_config(c))
                mask |= CGROUP_MASK_IO | CGROUP_MASK_BLKIO;

        if (c->memory_accounting ||
            c->memory_limit != CGROUP_LIMIT_MAX ||
            cgroup_context_has_unified_memory_config(c))
                mask |= CGROUP_MASK_MEMORY;

        if (c->device_allow ||
            c->device_policy != CGROUP_AUTO)
                mask |= CGROUP_MASK_DEVICES;

        if (c->tasks_accounting ||
            c->tasks_max != CGROUP_LIMIT_MAX)
                mask |= CGROUP_MASK_PIDS;

        return mask;
}

CGroupMask unit_get_own_mask(Unit *u) {
        CGroupContext *c;

        /* Returns the mask of controllers the unit needs for itself */

        c = unit_get_cgroup_context(u);
        if (!c)
                return 0;

        return cgroup_context_get_mask(c) | unit_get_delegate_mask(u);
}

CGroupMask unit_get_delegate_mask(Unit *u) {
        CGroupContext *c;

        /* If delegation is turned on, then turn on selected controllers, unless we are on the legacy hierarchy and the
         * process we fork into is known to drop privileges, and hence shouldn't get access to the controllers.
         *
         * Note that on the unified hierarchy it is safe to delegate controllers to unprivileged services. */

        if (!unit_cgroup_delegate(u))
                return 0;

        if (cg_all_unified() <= 0) {
                ExecContext *e;

                e = unit_get_exec_context(u);
                if (e && !exec_context_maintains_privileges(e))
                        return 0;
        }

        assert_se(c = unit_get_cgroup_context(u));
        return c->delegate_controllers;
}

CGroupMask unit_get_members_mask(Unit *u) {
        assert(u);

        /* Returns the mask of controllers all of the unit's children require, merged */

        if (u->cgroup_members_mask_valid)
                return u->cgroup_members_mask;

        u->cgroup_members_mask = 0;

        if (u->type == UNIT_SLICE) {
                void *v;
                Unit *member;
                Iterator i;

                HASHMAP_FOREACH_KEY(v, member, u->dependencies[UNIT_BEFORE], i) {

                        if (member == u)
                                continue;

                        if (UNIT_DEREF(member->slice) != u)
                                continue;

                        u->cgroup_members_mask |= unit_get_subtree_mask(member); /* note that this calls ourselves again, for the children */
                }
        }

        u->cgroup_members_mask_valid = true;
        return u->cgroup_members_mask;
}

CGroupMask unit_get_siblings_mask(Unit *u) {
        assert(u);

        /* Returns the mask of controllers all of the unit's siblings
         * require, i.e. the members mask of the unit's parent slice
         * if there is one. */

        if (UNIT_ISSET(u->slice))
                return unit_get_members_mask(UNIT_DEREF(u->slice));

        return unit_get_subtree_mask(u); /* we are the top-level slice */
}

CGroupMask unit_get_subtree_mask(Unit *u) {

        /* Returns the mask of this subtree, meaning of the group
         * itself and its children. */

        return unit_get_own_mask(u) | unit_get_members_mask(u);
}

CGroupMask unit_get_target_mask(Unit *u) {
        CGroupMask mask;

        /* This returns the cgroup mask of all controllers to enable
         * for a specific cgroup, i.e. everything it needs itself,
         * plus all that its children need, plus all that its siblings
         * need. This is primarily useful on the legacy cgroup
         * hierarchy, where we need to duplicate each cgroup in each
         * hierarchy that shall be enabled for it. */

        mask = unit_get_own_mask(u) | unit_get_members_mask(u) | unit_get_siblings_mask(u);
        mask &= u->manager->cgroup_supported;

        return mask;
}

CGroupMask unit_get_enable_mask(Unit *u) {
        CGroupMask mask;

        /* This returns the cgroup mask of all controllers to enable
         * for the children of a specific cgroup. This is primarily
         * useful for the unified cgroup hierarchy, where each cgroup
         * controls which controllers are enabled for its children. */

        mask = unit_get_members_mask(u);
        mask &= u->manager->cgroup_supported;

        return mask;
}

bool unit_get_needs_bpf(Unit *u) {
        CGroupContext *c;
        Unit *p;
        assert(u);

        c = unit_get_cgroup_context(u);
        if (!c)
                return false;

        if (c->ip_accounting ||
            c->ip_address_allow ||
            c->ip_address_deny)
                return true;

        /* If any parent slice has an IP access list defined, it applies too */
        for (p = UNIT_DEREF(u->slice); p; p = UNIT_DEREF(p->slice)) {
                c = unit_get_cgroup_context(p);
                if (!c)
                        return false;

                if (c->ip_address_allow ||
                    c->ip_address_deny)
                        return true;
        }

        return false;
}

/* Recurse from a unit up through its containing slices, propagating
 * mask bits upward. A unit is also member of itself. */
void unit_update_cgroup_members_masks(Unit *u) {
        CGroupMask m;
        bool more;

        assert(u);

        /* Calculate subtree mask */
        m = unit_get_subtree_mask(u);

        /* See if anything changed from the previous invocation. If
         * not, we're done. */
        if (u->cgroup_subtree_mask_valid && m == u->cgroup_subtree_mask)
                return;

        more =
                u->cgroup_subtree_mask_valid &&
                ((m & ~u->cgroup_subtree_mask) != 0) &&
                ((~m & u->cgroup_subtree_mask) == 0);

        u->cgroup_subtree_mask = m;
        u->cgroup_subtree_mask_valid = true;

        if (UNIT_ISSET(u->slice)) {
                Unit *s = UNIT_DEREF(u->slice);

                if (more)
                        /* There's more set now than before. We
                         * propagate the new mask to the parent's mask
                         * (not caring if it actually was valid or
                         * not). */

                        s->cgroup_members_mask |= m;

                else
                        /* There's less set now than before (or we
                         * don't know), we need to recalculate
                         * everything, so let's invalidate the
                         * parent's members mask */

                        s->cgroup_members_mask_valid = false;

                /* And now make sure that this change also hits our
                 * grandparents */
                unit_update_cgroup_members_masks(s);
        }
}

const char *unit_get_realized_cgroup_path(Unit *u, CGroupMask mask) {

        /* Returns the realized cgroup path of the specified unit where all specified controllers are available. */

        while (u) {

                if (u->cgroup_path &&
                    u->cgroup_realized &&
                    FLAGS_SET(u->cgroup_realized_mask, mask))
                        return u->cgroup_path;

                u = UNIT_DEREF(u->slice);
        }

        return NULL;
}

static const char *migrate_callback(CGroupMask mask, void *userdata) {
        return unit_get_realized_cgroup_path(userdata, mask);
}

char *unit_default_cgroup_path(Unit *u) {
        _cleanup_free_ char *escaped = NULL, *slice = NULL;
        int r;

        assert(u);

        if (unit_has_name(u, SPECIAL_ROOT_SLICE))
                return strdup(u->manager->cgroup_root);

        if (UNIT_ISSET(u->slice) && !unit_has_name(UNIT_DEREF(u->slice), SPECIAL_ROOT_SLICE)) {
                r = cg_slice_to_path(UNIT_DEREF(u->slice)->id, &slice);
                if (r < 0)
                        return NULL;
        }

        escaped = cg_escape(u->id);
        if (!escaped)
                return NULL;

        if (slice)
                return strjoin(u->manager->cgroup_root, "/", slice, "/",
                               escaped);
        else
                return strjoin(u->manager->cgroup_root, "/", escaped);
}

int unit_set_cgroup_path(Unit *u, const char *path) {
        _cleanup_free_ char *p = NULL;
        int r;

        assert(u);

        if (path) {
                p = strdup(path);
                if (!p)
                        return -ENOMEM;
        } else
                p = NULL;

        if (streq_ptr(u->cgroup_path, p))
                return 0;

        if (p) {
                r = hashmap_put(u->manager->cgroup_unit, p, u);
                if (r < 0)
                        return r;
        }

        unit_release_cgroup(u);

        u->cgroup_path = TAKE_PTR(p);

        return 1;
}

int unit_watch_cgroup(Unit *u) {
        _cleanup_free_ char *events = NULL;
        int r;

        assert(u);

        if (!u->cgroup_path)
                return 0;

        if (u->cgroup_inotify_wd >= 0)
                return 0;

        /* Only applies to the unified hierarchy */
        r = cg_unified_controller(SYSTEMD_CGROUP_CONTROLLER);
        if (r < 0)
                return log_error_errno(r, "Failed to determine whether the name=systemd hierarchy is unified: %m");
        if (r == 0)
                return 0;

        /* Don't watch the root slice, it's pointless. */
        if (unit_has_name(u, SPECIAL_ROOT_SLICE))
                return 0;

        r = hashmap_ensure_allocated(&u->manager->cgroup_inotify_wd_unit, &trivial_hash_ops);
        if (r < 0)
                return log_oom();

        r = cg_get_path(SYSTEMD_CGROUP_CONTROLLER, u->cgroup_path, "cgroup.events", &events);
        if (r < 0)
                return log_oom();

        u->cgroup_inotify_wd = inotify_add_watch(u->manager->cgroup_inotify_fd, events, IN_MODIFY);
        if (u->cgroup_inotify_wd < 0) {

                if (errno == ENOENT) /* If the directory is already
                                      * gone we don't need to track
                                      * it, so this is not an error */
                        return 0;

                return log_unit_error_errno(u, errno, "Failed to add inotify watch descriptor for control group %s: %m", u->cgroup_path);
        }

        r = hashmap_put(u->manager->cgroup_inotify_wd_unit, INT_TO_PTR(u->cgroup_inotify_wd), u);
        if (r < 0)
                return log_unit_error_errno(u, r, "Failed to add inotify watch descriptor to hash map: %m");

        return 0;
}

int unit_pick_cgroup_path(Unit *u) {
        _cleanup_free_ char *path = NULL;
        int r;

        assert(u);

        if (u->cgroup_path)
                return 0;

        if (!UNIT_HAS_CGROUP_CONTEXT(u))
                return -EINVAL;

        path = unit_default_cgroup_path(u);
        if (!path)
                return log_oom();

        r = unit_set_cgroup_path(u, path);
        if (r == -EEXIST)
                return log_unit_error_errno(u, r, "Control group %s exists already.", path);
        if (r < 0)
                return log_unit_error_errno(u, r, "Failed to set unit's control group path to %s: %m", path);

        return 0;
}

static int unit_create_cgroup(
                Unit *u,
                CGroupMask target_mask,
                CGroupMask enable_mask,
                bool needs_bpf) {

        CGroupContext *c;
        int r;

        assert(u);

        c = unit_get_cgroup_context(u);
        if (!c)
                return 0;

        /* Figure out our cgroup path */
        r = unit_pick_cgroup_path(u);
        if (r < 0)
                return r;

        /* First, create our own group */
        r = cg_create_everywhere(u->manager->cgroup_supported, target_mask, u->cgroup_path);
        if (r < 0)
                return log_unit_error_errno(u, r, "Failed to create cgroup %s: %m", u->cgroup_path);

        /* Start watching it */
        (void) unit_watch_cgroup(u);

        /* Enable all controllers we need */
        r = cg_enable_everywhere(u->manager->cgroup_supported, enable_mask, u->cgroup_path);
        if (r < 0)
                log_unit_warning_errno(u, r, "Failed to enable controllers on cgroup %s, ignoring: %m", u->cgroup_path);

        /* Keep track that this is now realized */
        u->cgroup_realized = true;
        u->cgroup_realized_mask = target_mask;
        u->cgroup_enabled_mask = enable_mask;
        u->cgroup_bpf_state = needs_bpf ? UNIT_CGROUP_BPF_ON : UNIT_CGROUP_BPF_OFF;

        if (u->type != UNIT_SLICE && !unit_cgroup_delegate(u)) {

                /* Then, possibly move things over, but not if
                 * subgroups may contain processes, which is the case
                 * for slice and delegation units. */
                r = cg_migrate_everywhere(u->manager->cgroup_supported, u->cgroup_path, u->cgroup_path, migrate_callback, u);
                if (r < 0)
                        log_unit_warning_errno(u, r, "Failed to migrate cgroup from to %s, ignoring: %m", u->cgroup_path);
        }

        return 0;
}

static int unit_attach_pid_to_cgroup_via_bus(Unit *u, pid_t pid, const char *suffix_path) {
        _cleanup_(sd_bus_error_free) sd_bus_error error = SD_BUS_ERROR_NULL;
        char *pp;
        int r;

        assert(u);

        if (MANAGER_IS_SYSTEM(u->manager))
                return -EINVAL;

        if (!u->manager->system_bus)
                return -EIO;

        if (!u->cgroup_path)
                return -EINVAL;

        /* Determine this unit's cgroup path relative to our cgroup root */
        pp = path_startswith(u->cgroup_path, u->manager->cgroup_root);
        if (!pp)
                return -EINVAL;

        pp = strjoina("/", pp, suffix_path);
        path_simplify(pp, false);

        r = sd_bus_call_method(u->manager->system_bus,
                               "org.freedesktop.systemd1",
                               "/org/freedesktop/systemd1",
                               "org.freedesktop.systemd1.Manager",
                               "AttachProcessesToUnit",
                               &error, NULL,
                               "ssau",
                               NULL /* empty unit name means client's unit, i.e. us */, pp, 1, (uint32_t) pid);
        if (r < 0)
                return log_unit_debug_errno(u, r, "Failed to attach unit process " PID_FMT " via the bus: %s", pid, bus_error_message(&error, r));

        return 0;
}

int unit_attach_pids_to_cgroup(Unit *u, Set *pids, const char *suffix_path) {
        CGroupMask delegated_mask;
        const char *p;
        Iterator i;
        void *pidp;
        int r, q;

        assert(u);

        if (!UNIT_HAS_CGROUP_CONTEXT(u))
                return -EINVAL;

        if (set_isempty(pids))
                return 0;

        r = unit_realize_cgroup(u);
        if (r < 0)
                return r;

        if (isempty(suffix_path))
                p = u->cgroup_path;
        else
                p = strjoina(u->cgroup_path, "/", suffix_path);

        delegated_mask = unit_get_delegate_mask(u);

        r = 0;
        SET_FOREACH(pidp, pids, i) {
                pid_t pid = PTR_TO_PID(pidp);
                CGroupController c;

                /* First, attach the PID to the main cgroup hierarchy */
                q = cg_attach(SYSTEMD_CGROUP_CONTROLLER, p, pid);
                if (q < 0) {
                        log_unit_debug_errno(u, q, "Couldn't move process " PID_FMT " to requested cgroup '%s': %m", pid, p);

                        if (MANAGER_IS_USER(u->manager) && IN_SET(q, -EPERM, -EACCES)) {
                                int z;

                                /* If we are in a user instance, and we can't move the process ourselves due to
                                 * permission problems, let's ask the system instance about it instead. Since it's more
                                 * privileged it might be able to move the process across the leaves of a subtree who's
                                 * top node is not owned by us. */

                                z = unit_attach_pid_to_cgroup_via_bus(u, pid, suffix_path);
                                if (z < 0)
                                        log_unit_debug_errno(u, z, "Couldn't move process " PID_FMT " to requested cgroup '%s' via the system bus either: %m", pid, p);
                                else
                                        continue; /* When the bus thing worked via the bus we are fully done for this PID. */
                        }

                        if (r >= 0)
                                r = q; /* Remember first error */

                        continue;
                }

                q = cg_all_unified();
                if (q < 0)
                        return q;
                if (q > 0)
                        continue;

                /* In the legacy hierarchy, attach the process to the request cgroup if possible, and if not to the
                 * innermost realized one */

                for (c = 0; c < _CGROUP_CONTROLLER_MAX; c++) {
                        CGroupMask bit = CGROUP_CONTROLLER_TO_MASK(c);
                        const char *realized;

                        if (!(u->manager->cgroup_supported & bit))
                                continue;

                        /* If this controller is delegated and realized, honour the caller's request for the cgroup suffix. */
                        if (delegated_mask & u->cgroup_realized_mask & bit) {
                                q = cg_attach(cgroup_controller_to_string(c), p, pid);
                                if (q >= 0)
                                        continue; /* Success! */

                                log_unit_debug_errno(u, q, "Failed to attach PID " PID_FMT " to requested cgroup %s in controller %s, falling back to unit's cgroup: %m",
                                                     pid, p, cgroup_controller_to_string(c));
                        }

                        /* So this controller is either not delegate or realized, or something else weird happened. In
                         * that case let's attach the PID at least to the closest cgroup up the tree that is
                         * realized. */
                        realized = unit_get_realized_cgroup_path(u, bit);
                        if (!realized)
                                continue; /* Not even realized in the root slice? Then let's not bother */

                        q = cg_attach(cgroup_controller_to_string(c), realized, pid);
                        if (q < 0)
                                log_unit_debug_errno(u, q, "Failed to attach PID " PID_FMT " to realized cgroup %s in controller %s, ignoring: %m",
                                                     pid, realized, cgroup_controller_to_string(c));
                }
        }

        return r;
}

static void cgroup_xattr_apply(Unit *u) {
        char ids[SD_ID128_STRING_MAX];
        int r;

        assert(u);

        if (!MANAGER_IS_SYSTEM(u->manager))
                return;

        if (sd_id128_is_null(u->invocation_id))
                return;

        r = cg_set_xattr(SYSTEMD_CGROUP_CONTROLLER, u->cgroup_path,
                         "trusted.invocation_id",
                         sd_id128_to_string(u->invocation_id, ids), 32,
                         0);
        if (r < 0)
                log_unit_debug_errno(u, r, "Failed to set invocation ID on control group %s, ignoring: %m", u->cgroup_path);
}

static bool unit_has_mask_realized(
                Unit *u,
                CGroupMask target_mask,
                CGroupMask enable_mask,
                bool needs_bpf) {

        assert(u);

        return u->cgroup_realized &&
                u->cgroup_realized_mask == target_mask &&
                u->cgroup_enabled_mask == enable_mask &&
                ((needs_bpf && u->cgroup_bpf_state == UNIT_CGROUP_BPF_ON) ||
                 (!needs_bpf && u->cgroup_bpf_state == UNIT_CGROUP_BPF_OFF));
}

static void unit_add_to_cgroup_realize_queue(Unit *u) {
        assert(u);

        if (u->in_cgroup_realize_queue)
                return;

        LIST_PREPEND(cgroup_realize_queue, u->manager->cgroup_realize_queue, u);
        u->in_cgroup_realize_queue = true;
}

static void unit_remove_from_cgroup_realize_queue(Unit *u) {
        assert(u);

        if (!u->in_cgroup_realize_queue)
                return;

        LIST_REMOVE(cgroup_realize_queue, u->manager->cgroup_realize_queue, u);
        u->in_cgroup_realize_queue = false;
}

/* Check if necessary controllers and attributes for a unit are in place.
 *
 * If so, do nothing.
 * If not, create paths, move processes over, and set attributes.
 *
 * Returns 0 on success and < 0 on failure. */
static int unit_realize_cgroup_now(Unit *u, ManagerState state) {
        CGroupMask target_mask, enable_mask;
        bool needs_bpf, apply_bpf;
        int r;

        assert(u);

        unit_remove_from_cgroup_realize_queue(u);

        target_mask = unit_get_target_mask(u);
        enable_mask = unit_get_enable_mask(u);
        needs_bpf = unit_get_needs_bpf(u);

        if (unit_has_mask_realized(u, target_mask, enable_mask, needs_bpf))
                return 0;

        /* Make sure we apply the BPF filters either when one is configured, or if none is configured but previously
         * the state was anything but off. This way, if a unit with a BPF filter applied is reconfigured to lose it
         * this will trickle down properly to cgroupfs. */
        apply_bpf = needs_bpf || u->cgroup_bpf_state != UNIT_CGROUP_BPF_OFF;

        /* First, realize parents */
        if (UNIT_ISSET(u->slice)) {
                r = unit_realize_cgroup_now(UNIT_DEREF(u->slice), state);
                if (r < 0)
                        return r;
        }

        /* And then do the real work */
        r = unit_create_cgroup(u, target_mask, enable_mask, needs_bpf);
        if (r < 0)
                return r;

        /* Finally, apply the necessary attributes. */
        cgroup_context_apply(u, target_mask, apply_bpf, state);
        cgroup_xattr_apply(u);

        return 0;
}

unsigned manager_dispatch_cgroup_realize_queue(Manager *m) {
        ManagerState state;
        unsigned n = 0;
        Unit *i;
        int r;

        assert(m);

        state = manager_state(m);

        while ((i = m->cgroup_realize_queue)) {
                assert(i->in_cgroup_realize_queue);

                if (UNIT_IS_INACTIVE_OR_FAILED(unit_active_state(i))) {
                        /* Maybe things changed, and the unit is not actually active anymore? */
                        unit_remove_from_cgroup_realize_queue(i);
                        continue;
                }

                r = unit_realize_cgroup_now(i, state);
                if (r < 0)
                        log_warning_errno(r, "Failed to realize cgroups for queued unit %s, ignoring: %m", i->id);

                n++;
        }

        return n;
}

static void unit_add_siblings_to_cgroup_realize_queue(Unit *u) {
        Unit *slice;

        /* This adds the siblings of the specified unit and the
         * siblings of all parent units to the cgroup queue. (But
         * neither the specified unit itself nor the parents.) */

        while ((slice = UNIT_DEREF(u->slice))) {
                Iterator i;
                Unit *m;
                void *v;

                HASHMAP_FOREACH_KEY(v, m, u->dependencies[UNIT_BEFORE], i) {
                        if (m == u)
                                continue;

                        /* Skip units that have a dependency on the slice
                         * but aren't actually in it. */
                        if (UNIT_DEREF(m->slice) != slice)
                                continue;

                        /* No point in doing cgroup application for units
                         * without active processes. */
                        if (UNIT_IS_INACTIVE_OR_FAILED(unit_active_state(m)))
                                continue;

                        /* If the unit doesn't need any new controllers
                         * and has current ones realized, it doesn't need
                         * any changes. */
                        if (unit_has_mask_realized(m,
                                                   unit_get_target_mask(m),
                                                   unit_get_enable_mask(m),
                                                   unit_get_needs_bpf(m)))
                                continue;

                        unit_add_to_cgroup_realize_queue(m);
                }

                u = slice;
        }
}

int unit_realize_cgroup(Unit *u) {
        assert(u);

        if (!UNIT_HAS_CGROUP_CONTEXT(u))
                return 0;

        /* So, here's the deal: when realizing the cgroups for this
         * unit, we need to first create all parents, but there's more
         * actually: for the weight-based controllers we also need to
         * make sure that all our siblings (i.e. units that are in the
         * same slice as we are) have cgroups, too. Otherwise, things
         * would become very uneven as each of their processes would
         * get as much resources as all our group together. This call
         * will synchronously create the parent cgroups, but will
         * defer work on the siblings to the next event loop
         * iteration. */

        /* Add all sibling slices to the cgroup queue. */
        unit_add_siblings_to_cgroup_realize_queue(u);

        /* And realize this one now (and apply the values) */
        return unit_realize_cgroup_now(u, manager_state(u->manager));
}

void unit_release_cgroup(Unit *u) {
        assert(u);

        /* Forgets all cgroup details for this cgroup */

        if (u->cgroup_path) {
                (void) hashmap_remove(u->manager->cgroup_unit, u->cgroup_path);
                u->cgroup_path = mfree(u->cgroup_path);
        }

        if (u->cgroup_inotify_wd >= 0) {
                if (inotify_rm_watch(u->manager->cgroup_inotify_fd, u->cgroup_inotify_wd) < 0)
                        log_unit_debug_errno(u, errno, "Failed to remove cgroup inotify watch %i for %s, ignoring", u->cgroup_inotify_wd, u->id);

                (void) hashmap_remove(u->manager->cgroup_inotify_wd_unit, INT_TO_PTR(u->cgroup_inotify_wd));
                u->cgroup_inotify_wd = -1;
        }
}

void unit_prune_cgroup(Unit *u) {
        int r;
        bool is_root_slice;

        assert(u);

        /* Removes the cgroup, if empty and possible, and stops watching it. */

        if (!u->cgroup_path)
                return;

        (void) unit_get_cpu_usage(u, NULL); /* Cache the last CPU usage value before we destroy the cgroup */

        is_root_slice = unit_has_name(u, SPECIAL_ROOT_SLICE);

        r = cg_trim_everywhere(u->manager->cgroup_supported, u->cgroup_path, !is_root_slice);
        if (r < 0) {
                log_unit_debug_errno(u, r, "Failed to destroy cgroup %s, ignoring: %m", u->cgroup_path);
                return;
        }

        if (is_root_slice)
                return;

        unit_release_cgroup(u);

        u->cgroup_realized = false;
        u->cgroup_realized_mask = 0;
        u->cgroup_enabled_mask = 0;
}

int unit_search_main_pid(Unit *u, pid_t *ret) {
        _cleanup_fclose_ FILE *f = NULL;
        pid_t pid = 0, npid, mypid;
        int r;

        assert(u);
        assert(ret);

        if (!u->cgroup_path)
                return -ENXIO;

        r = cg_enumerate_processes(SYSTEMD_CGROUP_CONTROLLER, u->cgroup_path, &f);
        if (r < 0)
                return r;

        mypid = getpid_cached();
        while (cg_read_pid(f, &npid) > 0)  {
                pid_t ppid;

                if (npid == pid)
                        continue;

                /* Ignore processes that aren't our kids */
                if (get_process_ppid(npid, &ppid) >= 0 && ppid != mypid)
                        continue;

                if (pid != 0)
                        /* Dang, there's more than one daemonized PID
                        in this group, so we don't know what process
                        is the main process. */

                        return -ENODATA;

                pid = npid;
        }

        *ret = pid;
        return 0;
}

static int unit_watch_pids_in_path(Unit *u, const char *path) {
        _cleanup_closedir_ DIR *d = NULL;
        _cleanup_fclose_ FILE *f = NULL;
        int ret = 0, r;

        assert(u);
        assert(path);

        r = cg_enumerate_processes(SYSTEMD_CGROUP_CONTROLLER, path, &f);
        if (r < 0)
                ret = r;
        else {
                pid_t pid;

                while ((r = cg_read_pid(f, &pid)) > 0) {
                        r = unit_watch_pid(u, pid);
                        if (r < 0 && ret >= 0)
                                ret = r;
                }

                if (r < 0 && ret >= 0)
                        ret = r;
        }

        r = cg_enumerate_subgroups(SYSTEMD_CGROUP_CONTROLLER, path, &d);
        if (r < 0) {
                if (ret >= 0)
                        ret = r;
        } else {
                char *fn;

                while ((r = cg_read_subgroup(d, &fn)) > 0) {
                        _cleanup_free_ char *p = NULL;

                        p = strjoin(path, "/", fn);
                        free(fn);

                        if (!p)
                                return -ENOMEM;

                        r = unit_watch_pids_in_path(u, p);
                        if (r < 0 && ret >= 0)
                                ret = r;
                }

                if (r < 0 && ret >= 0)
                        ret = r;
        }

        return ret;
}

int unit_synthesize_cgroup_empty_event(Unit *u) {
        int r;

        assert(u);

        /* Enqueue a synthetic cgroup empty event if this unit doesn't watch any PIDs anymore. This is compatibility
         * support for non-unified systems where notifications aren't reliable, and hence need to take whatever we can
         * get as notification source as soon as we stopped having any useful PIDs to watch for. */

        if (!u->cgroup_path)
                return -ENOENT;

        r = cg_unified_controller(SYSTEMD_CGROUP_CONTROLLER);
        if (r < 0)
                return r;
        if (r > 0) /* On unified we have reliable notifications, and don't need this */
                return 0;

        if (!set_isempty(u->pids))
                return 0;

        unit_add_to_cgroup_empty_queue(u);
        return 0;
}

int unit_watch_all_pids(Unit *u) {
        int r;

        assert(u);

        /* Adds all PIDs from our cgroup to the set of PIDs we
         * watch. This is a fallback logic for cases where we do not
         * get reliable cgroup empty notifications: we try to use
         * SIGCHLD as replacement. */

        if (!u->cgroup_path)
                return -ENOENT;

        r = cg_unified_controller(SYSTEMD_CGROUP_CONTROLLER);
        if (r < 0)
                return r;
        if (r > 0) /* On unified we can use proper notifications */
                return 0;

        return unit_watch_pids_in_path(u, u->cgroup_path);
}

static int on_cgroup_empty_event(sd_event_source *s, void *userdata) {
        Manager *m = userdata;
        Unit *u;
        int r;

        assert(s);
        assert(m);

        u = m->cgroup_empty_queue;
        if (!u)
                return 0;

        assert(u->in_cgroup_empty_queue);
        u->in_cgroup_empty_queue = false;
        LIST_REMOVE(cgroup_empty_queue, m->cgroup_empty_queue, u);

        if (m->cgroup_empty_queue) {
                /* More stuff queued, let's make sure we remain enabled */
                r = sd_event_source_set_enabled(s, SD_EVENT_ONESHOT);
                if (r < 0)
                        log_debug_errno(r, "Failed to reenable cgroup empty event source, ignoring: %m");
        }

        unit_add_to_gc_queue(u);

        if (UNIT_VTABLE(u)->notify_cgroup_empty)
                UNIT_VTABLE(u)->notify_cgroup_empty(u);

        return 0;
}

void unit_add_to_cgroup_empty_queue(Unit *u) {
        int r;

        assert(u);

        /* Note that there are four different ways how cgroup empty events reach us:
         *
         * 1. On the unified hierarchy we get an inotify event on the cgroup
         *
         * 2. On the legacy hierarchy, when running in system mode, we get a datagram on the cgroup agent socket
         *
         * 3. On the legacy hierarchy, when running in user mode, we get a D-Bus signal on the system bus
         *
         * 4. On the legacy hierarchy, in service units we start watching all processes of the cgroup for SIGCHLD as
         *    soon as we get one SIGCHLD, to deal with unreliable cgroup notifications.
         *
         * Regardless which way we got the notification, we'll verify it here, and then add it to a separate
         * queue. This queue will be dispatched at a lower priority than the SIGCHLD handler, so that we always use
         * SIGCHLD if we can get it first, and only use the cgroup empty notifications if there's no SIGCHLD pending
         * (which might happen if the cgroup doesn't contain processes that are our own child, which is typically the
         * case for scope units). */

        if (u->in_cgroup_empty_queue)
                return;

        /* Let's verify that the cgroup is really empty */
        if (!u->cgroup_path)
                return;
        r = cg_is_empty_recursive(SYSTEMD_CGROUP_CONTROLLER, u->cgroup_path);
        if (r < 0) {
                log_unit_debug_errno(u, r, "Failed to determine whether cgroup %s is empty: %m", u->cgroup_path);
                return;
        }
        if (r == 0)
                return;

        LIST_PREPEND(cgroup_empty_queue, u->manager->cgroup_empty_queue, u);
        u->in_cgroup_empty_queue = true;

        /* Trigger the defer event */
        r = sd_event_source_set_enabled(u->manager->cgroup_empty_event_source, SD_EVENT_ONESHOT);
        if (r < 0)
                log_debug_errno(r, "Failed to enable cgroup empty event source: %m");
}

static int on_cgroup_inotify_event(sd_event_source *s, int fd, uint32_t revents, void *userdata) {
        Manager *m = userdata;

        assert(s);
        assert(fd >= 0);
        assert(m);

        for (;;) {
                union inotify_event_buffer buffer;
                struct inotify_event *e;
                ssize_t l;

                l = read(fd, &buffer, sizeof(buffer));
                if (l < 0) {
                        if (IN_SET(errno, EINTR, EAGAIN))
                                return 0;

                        return log_error_errno(errno, "Failed to read control group inotify events: %m");
                }

                FOREACH_INOTIFY_EVENT(e, buffer, l) {
                        Unit *u;

                        if (e->wd < 0)
                                /* Queue overflow has no watch descriptor */
                                continue;

                        if (e->mask & IN_IGNORED)
                                /* The watch was just removed */
                                continue;

                        u = hashmap_get(m->cgroup_inotify_wd_unit, INT_TO_PTR(e->wd));
                        if (!u) /* Not that inotify might deliver
                                 * events for a watch even after it
                                 * was removed, because it was queued
                                 * before the removal. Let's ignore
                                 * this here safely. */
                                continue;

                        unit_add_to_cgroup_empty_queue(u);
                }
        }
}
#endif // 0

int manager_setup_cgroup(Manager *m) {
        _cleanup_free_ char *path = NULL;
        const char *scope_path;
        CGroupController c;
        int r, all_unified;
#if 0 /// UNNEEDED by elogind
        char *e;
#endif // 0

        assert(m);

        /* 1. Determine hierarchy */
        m->cgroup_root = mfree(m->cgroup_root);
#if 0 /// elogind is not init and must therefore search for PID 1 instead of self.
        r = cg_pid_get_path(SYSTEMD_CGROUP_CONTROLLER, 0, &m->cgroup_root);
#else
        r = cg_pid_get_path(SYSTEMD_CGROUP_CONTROLLER, 1, &m->cgroup_root);
#endif // 0
        if (r < 0)
                return log_error_errno(r, "Cannot determine cgroup we are running in: %m");

#if 0 /// elogind does not support systemd scopes and slices
        /* Chop off the init scope, if we are already located in it */
        e = endswith(m->cgroup_root, "/" SPECIAL_INIT_SCOPE);

        /* LEGACY: Also chop off the system slice if we are in
         * it. This is to support live upgrades from older systemd
         * versions where PID 1 was moved there. Also see
         * cg_get_root_path(). */
        if (!e && MANAGER_IS_SYSTEM(m)) {
                e = endswith(m->cgroup_root, "/" SPECIAL_SYSTEM_SLICE);
                if (!e)
                        e = endswith(m->cgroup_root, "/system"); /* even more legacy */
        }
        if (e)
                *e = 0;
#endif // 0

        log_debug_elogind("Cgroup Controller \"%s\" -> root \"%s\"",
                          SYSTEMD_CGROUP_CONTROLLER, m->cgroup_root);
        /* And make sure to store away the root value without trailing slash, even for the root dir, so that we can
         * easily prepend it everywhere. */
        delete_trailing_chars(m->cgroup_root, "/");

        /* 2. Show data */
        r = cg_get_path(SYSTEMD_CGROUP_CONTROLLER, m->cgroup_root, NULL, &path);
        if (r < 0)
                return log_error_errno(r, "Cannot find cgroup mount point: %m");

        r = cg_unified_flush();
        if (r < 0)
                return log_error_errno(r, "Couldn't determine if we are running in the unified hierarchy: %m");

        all_unified = cg_all_unified();
        if (all_unified < 0)
                return log_error_errno(all_unified, "Couldn't determine whether we are in all unified mode: %m");
        if (all_unified > 0)
                log_debug("Unified cgroup hierarchy is located at %s.", path);
        else {
                r = cg_unified_controller(SYSTEMD_CGROUP_CONTROLLER);
                if (r < 0)
                        return log_error_errno(r, "Failed to determine whether systemd's own controller is in unified mode: %m");
                if (r > 0)
                        log_debug("Unified cgroup hierarchy is located at %s. Controllers are on legacy hierarchies.", path);
                else
                        log_debug("Using cgroup controller " SYSTEMD_CGROUP_CONTROLLER_LEGACY ". File system hierarchy is at %s.", path);
        }

#if 0 /// elogind is not init, and does not install the agent here.
        /* 3. Allocate cgroup empty defer event source */
        m->cgroup_empty_event_source = sd_event_source_unref(m->cgroup_empty_event_source);
        r = sd_event_add_defer(m->event, &m->cgroup_empty_event_source, on_cgroup_empty_event, m);
        if (r < 0)
                return log_error_errno(r, "Failed to create cgroup empty event source: %m");

        r = sd_event_source_set_priority(m->cgroup_empty_event_source, SD_EVENT_PRIORITY_NORMAL-5);
        if (r < 0)
                return log_error_errno(r, "Failed to set priority of cgroup empty event source: %m");

        r = sd_event_source_set_enabled(m->cgroup_empty_event_source, SD_EVENT_OFF);
        if (r < 0)
                return log_error_errno(r, "Failed to disable cgroup empty event source: %m");

        (void) sd_event_source_set_description(m->cgroup_empty_event_source, "cgroup-empty");

        /* 4. Install notifier inotify object, or agent */
        if (cg_unified_controller(SYSTEMD_CGROUP_CONTROLLER) > 0) {

                /* In the unified hierarchy we can get cgroup empty notifications via inotify. */

                m->cgroup_inotify_event_source = sd_event_source_unref(m->cgroup_inotify_event_source);
                safe_close(m->cgroup_inotify_fd);

                m->cgroup_inotify_fd = inotify_init1(IN_NONBLOCK|IN_CLOEXEC);
                if (m->cgroup_inotify_fd < 0)
                        return log_error_errno(errno, "Failed to create control group inotify object: %m");

                r = sd_event_add_io(m->event, &m->cgroup_inotify_event_source, m->cgroup_inotify_fd, EPOLLIN, on_cgroup_inotify_event, m);
                if (r < 0)
                        return log_error_errno(r, "Failed to watch control group inotify object: %m");

                /* Process cgroup empty notifications early, but after service notifications and SIGCHLD. Also
                 * see handling of cgroup agent notifications, for the classic cgroup hierarchy support. */
                r = sd_event_source_set_priority(m->cgroup_inotify_event_source, SD_EVENT_PRIORITY_NORMAL-4);
                if (r < 0)
                        return log_error_errno(r, "Failed to set priority of inotify event source: %m");

                (void) sd_event_source_set_description(m->cgroup_inotify_event_source, "cgroup-inotify");

        } else if (MANAGER_IS_SYSTEM(m) && m->test_run_flags == 0) {

                /* On the legacy hierarchy we only get notifications via cgroup agents. (Which isn't really reliable,
                 * since it does not generate events when control groups with children run empty. */

                r = cg_install_release_agent(SYSTEMD_CGROUP_CONTROLLER, SYSTEMD_CGROUP_AGENT_PATH);
                if (r < 0)
                        log_warning_errno(r, "Failed to install release agent, ignoring: %m");
                else if (r > 0)
                        log_debug("Installed release agent.");
                else if (r == 0)
                        log_debug("Release agent already installed.");
        }

        /* 5. Make sure we are in the special "init.scope" unit in the root slice. */
        scope_path = strjoina(m->cgroup_root, "/" SPECIAL_INIT_SCOPE);
        r = cg_create_and_attach(SYSTEMD_CGROUP_CONTROLLER, scope_path, 0);
        if (r >= 0) {
                /* Also, move all other userspace processes remaining in the root cgroup into that scope. */
                r = cg_migrate(SYSTEMD_CGROUP_CONTROLLER, m->cgroup_root, SYSTEMD_CGROUP_CONTROLLER, scope_path, 0);
                if (r < 0)
                        log_warning_errno(r, "Couldn't move remaining userspace processes, ignoring: %m");
#else
        /* Note:
                * This method is in core, and normally called by systemd
                * being init. As elogind is never init, we can not install
                * our agent here. We do so when mounting our cgroup file
                * system, so only if elogind is its own tiny controller.
                * Further, elogind is not meant to run in systemd init scope. */
        if (MANAGER_IS_SYSTEM(m))
                // we are our own cgroup controller
                scope_path = strjoina("");
        else if (streq(m->cgroup_root, "/elogind"))
                // root already is our cgroup
                scope_path = strjoina(m->cgroup_root);
        else
                // we have to create our own group
                scope_path = strjoina(m->cgroup_root, "/elogind");
        r = cg_create_and_attach(SYSTEMD_CGROUP_CONTROLLER, scope_path, 0);
#endif // 0
        log_debug_elogind("Created control group \"%s\"", scope_path);

                /* 6. And pin it, so that it cannot be unmounted */
                safe_close(m->pin_cgroupfs_fd);
                m->pin_cgroupfs_fd = open(path, O_RDONLY|O_CLOEXEC|O_DIRECTORY|O_NOCTTY|O_NONBLOCK);
                if (m->pin_cgroupfs_fd < 0)
                        return log_error_errno(errno, "Failed to open pin file: %m");

#if 0 /// this is from the cgroup migration above that elogind does not need.
        } else if (r < 0 && !m->test_run_flags)
                return log_error_errno(r, "Failed to create %s control group: %m", scope_path);
#endif // 0

        /* 7. Always enable hierarchical support if it exists... */
        if (!all_unified && m->test_run_flags == 0)
                (void) cg_set_attribute("memory", "/", "memory.use_hierarchy", "1");

        /* 8. Figure out which controllers are supported, and log about it */
        r = cg_mask_supported(&m->cgroup_supported);
        if (r < 0)
                return log_error_errno(r, "Failed to determine supported controllers: %m");
        for (c = 0; c < _CGROUP_CONTROLLER_MAX; c++)
                log_debug("Controller '%s' supported: %s", cgroup_controller_to_string(c), yes_no(m->cgroup_supported & CGROUP_CONTROLLER_TO_MASK(c)));

        return 0;
}

void manager_shutdown_cgroup(Manager *m, bool delete) {
        assert(m);

#if 0 /// elogind is not init
        /* We can't really delete the group, since we are in it. But
         * let's trim it. */
        if (delete && m->cgroup_root && m->test_run_flags != MANAGER_TEST_RUN_MINIMAL)
                (void) cg_trim(SYSTEMD_CGROUP_CONTROLLER, m->cgroup_root, false);

        m->cgroup_empty_event_source = sd_event_source_unref(m->cgroup_empty_event_source);

        m->cgroup_inotify_wd_unit = hashmap_free(m->cgroup_inotify_wd_unit);

        m->cgroup_inotify_event_source = sd_event_source_unref(m->cgroup_inotify_event_source);
        m->cgroup_inotify_fd = safe_close(m->cgroup_inotify_fd);
#endif // 0

        m->pin_cgroupfs_fd = safe_close(m->pin_cgroupfs_fd);

        m->cgroup_root = mfree(m->cgroup_root);
}

#if 0 /// UNNEEDED by elogind
Unit* manager_get_unit_by_cgroup(Manager *m, const char *cgroup) {
        char *p;
        Unit *u;

        assert(m);
        assert(cgroup);

        u = hashmap_get(m->cgroup_unit, cgroup);
        if (u)
                return u;

        p = strdupa(cgroup);
        for (;;) {
                char *e;

                e = strrchr(p, '/');
                if (!e || e == p)
                        return hashmap_get(m->cgroup_unit, SPECIAL_ROOT_SLICE);

                *e = 0;

                u = hashmap_get(m->cgroup_unit, p);
                if (u)
                        return u;
        }
}

Unit *manager_get_unit_by_pid_cgroup(Manager *m, pid_t pid) {
        _cleanup_free_ char *cgroup = NULL;

        assert(m);

        if (!pid_is_valid(pid))
                return NULL;

        if (cg_pid_get_path(SYSTEMD_CGROUP_CONTROLLER, pid, &cgroup) < 0)
                return NULL;

        return manager_get_unit_by_cgroup(m, cgroup);
}

Unit *manager_get_unit_by_pid(Manager *m, pid_t pid) {
        Unit *u, **array;

        assert(m);

        /* Note that a process might be owned by multiple units, we return only one here, which is good enough for most
         * cases, though not strictly correct. We prefer the one reported by cgroup membership, as that's the most
         * relevant one as children of the process will be assigned to that one, too, before all else. */

        if (!pid_is_valid(pid))
                return NULL;

        if (pid == getpid_cached())
                return hashmap_get(m->units, SPECIAL_INIT_SCOPE);

        u = manager_get_unit_by_pid_cgroup(m, pid);
        if (u)
                return u;

        u = hashmap_get(m->watch_pids, PID_TO_PTR(pid));
        if (u)
                return u;

        array = hashmap_get(m->watch_pids, PID_TO_PTR(-pid));
        if (array)
                return array[0];

        return NULL;
}
#endif // 0

#if 0 /// elogind must substitute this with its own variant
int manager_notify_cgroup_empty(Manager *m, const char *cgroup) {
        Unit *u;

        assert(m);
        assert(cgroup);

        /* Called on the legacy hierarchy whenever we get an explicit cgroup notification from the cgroup agent process
         * or from the --system instance */

        log_debug("Got cgroup empty notification for: %s", cgroup);

        u = manager_get_unit_by_cgroup(m, cgroup);
        if (!u)
                return 0;

        unit_add_to_cgroup_empty_queue(u);
        return 1;
}
#else
int manager_notify_cgroup_empty(Manager *m, const char *cgroup) {
        Session *s;

        assert(m);
        assert(cgroup);

        log_debug("Got cgroup empty notification for: %s", cgroup);

        s = hashmap_get(m->sessions, cgroup);

        if (s) {
                session_finalize(s);
                session_free(s);
        } else
                log_warning("Session not found: %s", cgroup);

        return 0;
}
#endif // 0
#if 0 /// UNNEEDED by elogind
int unit_get_memory_current(Unit *u, uint64_t *ret) {
        _cleanup_free_ char *v = NULL;
        int r;

        assert(u);
        assert(ret);

        if (!UNIT_CGROUP_BOOL(u, memory_accounting))
                return -ENODATA;

        if (!u->cgroup_path)
                return -ENODATA;

        /* The root cgroup doesn't expose this information, let's get it from /proc instead */
        if (unit_has_root_cgroup(u))
                return procfs_memory_get_current(ret);

        if ((u->cgroup_realized_mask & CGROUP_MASK_MEMORY) == 0)
                return -ENODATA;

        r = cg_all_unified();
        if (r < 0)
                return r;
        if (r > 0)
                r = cg_get_attribute("memory", u->cgroup_path, "memory.current", &v);
        else
                r = cg_get_attribute("memory", u->cgroup_path, "memory.usage_in_bytes", &v);
        if (r == -ENOENT)
                return -ENODATA;
        if (r < 0)
                return r;

        return safe_atou64(v, ret);
}

int unit_get_tasks_current(Unit *u, uint64_t *ret) {
        _cleanup_free_ char *v = NULL;
        int r;

        assert(u);
        assert(ret);

        if (!UNIT_CGROUP_BOOL(u, tasks_accounting))
                return -ENODATA;

        if (!u->cgroup_path)
                return -ENODATA;

        /* The root cgroup doesn't expose this information, let's get it from /proc instead */
        if (unit_has_root_cgroup(u))
                return procfs_tasks_get_current(ret);

        if ((u->cgroup_realized_mask & CGROUP_MASK_PIDS) == 0)
                return -ENODATA;

        r = cg_get_attribute("pids", u->cgroup_path, "pids.current", &v);
        if (r == -ENOENT)
                return -ENODATA;
        if (r < 0)
                return r;

        return safe_atou64(v, ret);
}

static int unit_get_cpu_usage_raw(Unit *u, nsec_t *ret) {
        _cleanup_free_ char *v = NULL;
        uint64_t ns;
        int r;

        assert(u);
        assert(ret);

        if (!u->cgroup_path)
                return -ENODATA;

        /* The root cgroup doesn't expose this information, let's get it from /proc instead */
        if (unit_has_root_cgroup(u))
                return procfs_cpu_get_usage(ret);

        r = cg_all_unified();
        if (r < 0)
                return r;
        if (r > 0) {
                _cleanup_free_ char *val = NULL;
                uint64_t us;

                if ((u->cgroup_realized_mask & CGROUP_MASK_CPU) == 0)
                        return -ENODATA;

                r = cg_get_keyed_attribute("cpu", u->cgroup_path, "cpu.stat", STRV_MAKE("usage_usec"), &val);
                if (r < 0)
                        return r;
                if (IN_SET(r, -ENOENT, -ENXIO))
                        return -ENODATA;

                r = safe_atou64(val, &us);
                if (r < 0)
                        return r;

                ns = us * NSEC_PER_USEC;
        } else {
                if ((u->cgroup_realized_mask & CGROUP_MASK_CPUACCT) == 0)
                        return -ENODATA;

                r = cg_get_attribute("cpuacct", u->cgroup_path, "cpuacct.usage", &v);
                if (r == -ENOENT)
                        return -ENODATA;
                if (r < 0)
                        return r;

                r = safe_atou64(v, &ns);
                if (r < 0)
                        return r;
        }

        *ret = ns;
        return 0;
}

int unit_get_cpu_usage(Unit *u, nsec_t *ret) {
        nsec_t ns;
        int r;

        assert(u);

        /* Retrieve the current CPU usage counter. This will subtract the CPU counter taken when the unit was
         * started. If the cgroup has been removed already, returns the last cached value. To cache the value, simply
         * call this function with a NULL return value. */

        if (!UNIT_CGROUP_BOOL(u, cpu_accounting))
                return -ENODATA;

        r = unit_get_cpu_usage_raw(u, &ns);
        if (r == -ENODATA && u->cpu_usage_last != NSEC_INFINITY) {
                /* If we can't get the CPU usage anymore (because the cgroup was already removed, for example), use our
                 * cached value. */

                if (ret)
                        *ret = u->cpu_usage_last;
                return 0;
        }
        if (r < 0)
                return r;

        if (ns > u->cpu_usage_base)
                ns -= u->cpu_usage_base;
        else
                ns = 0;

        u->cpu_usage_last = ns;
        if (ret)
                *ret = ns;

        return 0;
}

int unit_get_ip_accounting(
                Unit *u,
                CGroupIPAccountingMetric metric,
                uint64_t *ret) {

        uint64_t value;
        int fd, r;

        assert(u);
        assert(metric >= 0);
        assert(metric < _CGROUP_IP_ACCOUNTING_METRIC_MAX);
        assert(ret);

        if (!UNIT_CGROUP_BOOL(u, ip_accounting))
                return -ENODATA;

        fd = IN_SET(metric, CGROUP_IP_INGRESS_BYTES, CGROUP_IP_INGRESS_PACKETS) ?
                u->ip_accounting_ingress_map_fd :
                u->ip_accounting_egress_map_fd;
        if (fd < 0)
                return -ENODATA;

        if (IN_SET(metric, CGROUP_IP_INGRESS_BYTES, CGROUP_IP_EGRESS_BYTES))
                r = bpf_firewall_read_accounting(fd, &value, NULL);
        else
                r = bpf_firewall_read_accounting(fd, NULL, &value);
        if (r < 0)
                return r;

        /* Add in additional metrics from a previous runtime. Note that when reexecing/reloading the daemon we compile
         * all BPF programs and maps anew, but serialize the old counters. When deserializing we store them in the
         * ip_accounting_extra[] field, and add them in here transparently. */

        *ret = value + u->ip_accounting_extra[metric];

        return r;
}

int unit_reset_cpu_accounting(Unit *u) {
        nsec_t ns;
        int r;

        assert(u);

        u->cpu_usage_last = NSEC_INFINITY;

        r = unit_get_cpu_usage_raw(u, &ns);
        if (r < 0) {
                u->cpu_usage_base = 0;
                return r;
        }

        u->cpu_usage_base = ns;
        return 0;
}

int unit_reset_ip_accounting(Unit *u) {
        int r = 0, q = 0;

        assert(u);

        if (u->ip_accounting_ingress_map_fd >= 0)
                r = bpf_firewall_reset_accounting(u->ip_accounting_ingress_map_fd);

        if (u->ip_accounting_egress_map_fd >= 0)
                q = bpf_firewall_reset_accounting(u->ip_accounting_egress_map_fd);

        zero(u->ip_accounting_extra);

        return r < 0 ? r : q;
}

void unit_invalidate_cgroup(Unit *u, CGroupMask m) {
        assert(u);

        if (!UNIT_HAS_CGROUP_CONTEXT(u))
                return;

        if (m == 0)
                return;

        /* always invalidate compat pairs together */
        if (m & (CGROUP_MASK_IO | CGROUP_MASK_BLKIO))
                m |= CGROUP_MASK_IO | CGROUP_MASK_BLKIO;

        if (m & (CGROUP_MASK_CPU | CGROUP_MASK_CPUACCT))
                m |= CGROUP_MASK_CPU | CGROUP_MASK_CPUACCT;

        if ((u->cgroup_realized_mask & m) == 0) /* NOP? */
                return;

        u->cgroup_realized_mask &= ~m;
        unit_add_to_cgroup_realize_queue(u);
}

void unit_invalidate_cgroup_bpf(Unit *u) {
        assert(u);

        if (!UNIT_HAS_CGROUP_CONTEXT(u))
                return;

        if (u->cgroup_bpf_state == UNIT_CGROUP_BPF_INVALIDATED) /* NOP? */
                return;

        u->cgroup_bpf_state = UNIT_CGROUP_BPF_INVALIDATED;
        unit_add_to_cgroup_realize_queue(u);

        /* If we are a slice unit, we also need to put compile a new BPF program for all our children, as the IP access
         * list of our children includes our own. */
        if (u->type == UNIT_SLICE) {
                Unit *member;
                Iterator i;
                void *v;

                HASHMAP_FOREACH_KEY(v, member, u->dependencies[UNIT_BEFORE], i) {
                        if (member == u)
                                continue;

                        if (UNIT_DEREF(member->slice) != u)
                                continue;

                        unit_invalidate_cgroup_bpf(member);
                }
        }
}

bool unit_cgroup_delegate(Unit *u) {
        CGroupContext *c;

        assert(u);

        if (!UNIT_VTABLE(u)->can_delegate)
                return false;

        c = unit_get_cgroup_context(u);
        if (!c)
                return false;

        return c->delegate;
}

void manager_invalidate_startup_units(Manager *m) {
        Iterator i;
        Unit *u;

        assert(m);

        SET_FOREACH(u, m->startup_units, i)
                unit_invalidate_cgroup(u, CGROUP_MASK_CPU|CGROUP_MASK_IO|CGROUP_MASK_BLKIO);
}

static const char* const cgroup_device_policy_table[_CGROUP_DEVICE_POLICY_MAX] = {
        [CGROUP_AUTO] = "auto",
        [CGROUP_CLOSED] = "closed",
        [CGROUP_STRICT] = "strict",
};

DEFINE_STRING_TABLE_LOOKUP(cgroup_device_policy, CGroupDevicePolicy);
#endif // 0
