/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include "varlink-io.elogind.PCRExtend.h"

static VARLINK_DEFINE_METHOD(
                Extend,
                VARLINK_DEFINE_INPUT(pcr, VARLINK_INT, 0),
                VARLINK_DEFINE_INPUT(text, VARLINK_STRING, VARLINK_NULLABLE),
                VARLINK_DEFINE_INPUT(data, VARLINK_STRING, VARLINK_NULLABLE));

VARLINK_DEFINE_INTERFACE(
                io_elogind_PCRExtend,
                "io.elogind.PCRExtend",
                &vl_method_Extend);
