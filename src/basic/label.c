/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <errno.h>
#include <stddef.h>

#include "label.h"

static const LabelOps *label_ops = NULL;

int label_ops_set(const LabelOps *ops) {
        if (label_ops)
                return -EBUSY;

        label_ops = ops;
        return 0;
}

int label_ops_pre(int dir_fd, const char *path, mode_t mode) {
        if (!label_ops || !label_ops->pre)
                return 0;

        return label_ops->pre(dir_fd, path, mode);
}

int label_ops_post(int dir_fd, const char *path) {
        if (!label_ops || !label_ops->post)
                return 0;

        return label_ops->post(dir_fd, path);
}
