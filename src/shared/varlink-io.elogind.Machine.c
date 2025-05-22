/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include "varlink-idl.h"
#include "varlink-io.elogind.Machine.h"

static VARLINK_DEFINE_METHOD(
                Register,
                VARLINK_DEFINE_INPUT(name,              VARLINK_STRING, 0),
                VARLINK_DEFINE_INPUT(id,                VARLINK_STRING, VARLINK_NULLABLE),
                VARLINK_DEFINE_INPUT(service,           VARLINK_STRING, VARLINK_NULLABLE),
                VARLINK_DEFINE_INPUT(class,             VARLINK_STRING, 0),
                VARLINK_DEFINE_INPUT(leader,            VARLINK_INT,    VARLINK_NULLABLE),
                VARLINK_DEFINE_INPUT(rootDirectory,     VARLINK_STRING, VARLINK_NULLABLE),
                VARLINK_DEFINE_INPUT(ifIndices,         VARLINK_INT,    VARLINK_ARRAY|VARLINK_NULLABLE),
                VARLINK_DEFINE_INPUT(vSockCid,          VARLINK_INT,    VARLINK_NULLABLE),
                VARLINK_DEFINE_INPUT(sshAddress,        VARLINK_STRING, VARLINK_NULLABLE),
                VARLINK_DEFINE_INPUT(sshPrivateKeyPath, VARLINK_STRING, VARLINK_NULLABLE),
                VARLINK_FIELD_COMMENT("Whether to allow interactive authentication on this operation."),
                VARLINK_DEFINE_INPUT(allowInteractiveAuthentication, VARLINK_BOOL, VARLINK_NULLABLE));

static VARLINK_DEFINE_ERROR(MachineExists);

VARLINK_DEFINE_INTERFACE(
                io_elogind_Machine,
                "io.elogind.Machine",
                &vl_method_Register,
                &vl_error_MachineExists);
