/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include "varlink-io.elogind.PCRLock.h"

static VARLINK_DEFINE_METHOD(
                ReadEventLog);

static VARLINK_DEFINE_METHOD(
                MakePolicy,
                VARLINK_DEFINE_INPUT(force, VARLINK_BOOL, VARLINK_NULLABLE));

static VARLINK_DEFINE_METHOD(
                RemovePolicy);

VARLINK_DEFINE_ERROR(
                NoChange);

VARLINK_DEFINE_INTERFACE(
                io_elogind_PCRLock,
                "io.elogind.PCRLock",
                &vl_method_ReadEventLog,
                &vl_method_MakePolicy,
                &vl_method_RemovePolicy,
                &vl_error_NoChange);
