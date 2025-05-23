/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include "varlink-io.elogind.Machine.h"
#include "sd-varlink-idl.h"


static SD_VARLINK_DEFINE_METHOD(
                Register,
                SD_VARLINK_DEFINE_INPUT(name,              SD_VARLINK_STRING, 0),
                SD_VARLINK_DEFINE_INPUT(id,                SD_VARLINK_STRING, SD_VARLINK_NULLABLE),
                SD_VARLINK_DEFINE_INPUT(service,           SD_VARLINK_STRING, SD_VARLINK_NULLABLE),
                SD_VARLINK_DEFINE_INPUT(class,             SD_VARLINK_STRING, 0),
                SD_VARLINK_DEFINE_INPUT(leader,            SD_VARLINK_INT,    SD_VARLINK_NULLABLE),
                SD_VARLINK_DEFINE_INPUT(rootDirectory,     SD_VARLINK_STRING, SD_VARLINK_NULLABLE),
                SD_VARLINK_DEFINE_INPUT(ifIndices,         SD_VARLINK_INT,    SD_VARLINK_ARRAY|SD_VARLINK_NULLABLE),
                SD_VARLINK_DEFINE_INPUT(vSockCid,          SD_VARLINK_INT,    SD_VARLINK_NULLABLE),
                SD_VARLINK_DEFINE_INPUT(sshAddress,        SD_VARLINK_STRING, SD_VARLINK_NULLABLE),
                SD_VARLINK_DEFINE_INPUT(sshPrivateKeyPath, SD_VARLINK_STRING, SD_VARLINK_NULLABLE),
                SD_VARLINK_FIELD_COMMENT("Controls whether to allocate a scope unit for the machine to register. If false, the client already took care of that and registered a service/scope specific to the machine."),
                SD_VARLINK_DEFINE_INPUT(allocateUnit,      SD_VARLINK_BOOL,   SD_VARLINK_NULLABLE),
                SD_VARLINK_FIELD_COMMENT("Whether to allow interactive authentication on this operation."),
                SD_VARLINK_DEFINE_INPUT(allowInteractiveAuthentication, SD_VARLINK_BOOL, SD_VARLINK_NULLABLE));

static SD_VARLINK_DEFINE_STRUCT_TYPE(
                Timestamp,
                SD_VARLINK_FIELD_COMMENT("Timestamp in µs in the CLOCK_REALTIME clock (wallclock)"),
                SD_VARLINK_DEFINE_FIELD(realtime, SD_VARLINK_INT, SD_VARLINK_NULLABLE),
                SD_VARLINK_FIELD_COMMENT("Timestamp in µs in the CLOCK_MONOTONIC clock"),
                SD_VARLINK_DEFINE_FIELD(monotonic, SD_VARLINK_INT, SD_VARLINK_NULLABLE));

static SD_VARLINK_DEFINE_METHOD(
                Unregister,
                SD_VARLINK_FIELD_COMMENT("The name of a machine to unregister."),
                SD_VARLINK_DEFINE_INPUT(name, SD_VARLINK_STRING, 0));

static SD_VARLINK_DEFINE_METHOD(
                Terminate,
                SD_VARLINK_FIELD_COMMENT("The name of a machine to terminate."),
                SD_VARLINK_DEFINE_INPUT(name, SD_VARLINK_STRING, 0));

static SD_VARLINK_DEFINE_METHOD(
                Kill,
                SD_VARLINK_FIELD_COMMENT("The name of a machine to send signal to."),
                SD_VARLINK_DEFINE_INPUT(name, SD_VARLINK_STRING, 0),
                SD_VARLINK_FIELD_COMMENT("Identifier that specifies what precisely to send the signal to (either 'leader' or 'all')."),
                SD_VARLINK_DEFINE_INPUT(whom, SD_VARLINK_STRING, SD_VARLINK_NULLABLE),
                SD_VARLINK_FIELD_COMMENT("Numeric UNIX signal integer."),
                SD_VARLINK_DEFINE_INPUT(signal, SD_VARLINK_INT, 0));

static SD_VARLINK_DEFINE_METHOD_FULL(
                List,
                SD_VARLINK_SUPPORTS_MORE,
                SD_VARLINK_FIELD_COMMENT("If non-null the name of a running machine to report details on. If both 'name' and 'pid' are null/unspecified enumerates all running machines."),
                SD_VARLINK_DEFINE_INPUT(name, SD_VARLINK_STRING, SD_VARLINK_NULLABLE),
                SD_VARLINK_FIELD_COMMENT("If non-null the PID of a running machine to report details on."),
                SD_VARLINK_DEFINE_INPUT(pid, SD_VARLINK_INT, SD_VARLINK_NULLABLE),
                SD_VARLINK_FIELD_COMMENT("Name of the machine"),
                SD_VARLINK_DEFINE_OUTPUT(name, SD_VARLINK_STRING, 0),
                SD_VARLINK_FIELD_COMMENT("128bit ID identifying this machine, formatted in hexadecimal"),
                SD_VARLINK_DEFINE_OUTPUT(id, SD_VARLINK_STRING, SD_VARLINK_NULLABLE),
                SD_VARLINK_FIELD_COMMENT("Name of the software that registered this machine"),
                SD_VARLINK_DEFINE_OUTPUT(service, SD_VARLINK_STRING, SD_VARLINK_NULLABLE),
                SD_VARLINK_FIELD_COMMENT("The class of this machine"),
                SD_VARLINK_DEFINE_OUTPUT(class, SD_VARLINK_STRING, 0),
                SD_VARLINK_FIELD_COMMENT("Leader process PID of this machine"),
                SD_VARLINK_DEFINE_OUTPUT(leader, SD_VARLINK_INT, SD_VARLINK_NULLABLE),
                SD_VARLINK_FIELD_COMMENT("Root directory of this machine, if known, relative to host file system"),
                SD_VARLINK_DEFINE_OUTPUT(rootDirectory, SD_VARLINK_STRING, SD_VARLINK_NULLABLE),
                SD_VARLINK_FIELD_COMMENT("The service manager unit this machine resides in"),
                SD_VARLINK_DEFINE_OUTPUT(unit, SD_VARLINK_STRING, SD_VARLINK_NULLABLE),
                SD_VARLINK_FIELD_COMMENT("Timestamp when the machine was activated"),
                SD_VARLINK_DEFINE_OUTPUT_BY_TYPE(timestamp, Timestamp, SD_VARLINK_NULLABLE),
                SD_VARLINK_FIELD_COMMENT("AF_VSOCK CID of the machine if known and applicable"),
                SD_VARLINK_DEFINE_OUTPUT(vSockCid, SD_VARLINK_INT, SD_VARLINK_NULLABLE),
                SD_VARLINK_FIELD_COMMENT("SSH address to connect to"),
                SD_VARLINK_DEFINE_OUTPUT(sshAddress, SD_VARLINK_STRING, SD_VARLINK_NULLABLE));

static SD_VARLINK_DEFINE_ERROR(NoSuchMachine);
static SD_VARLINK_DEFINE_ERROR(MachineExists);

SD_VARLINK_DEFINE_INTERFACE(
                io_elogind_Machine,
                "io.elogind.Machine",
                SD_VARLINK_SYMBOL_COMMENT("A timestamp object consisting of both CLOCK_REALTIME and CLOCK_MONOTONIC timestamps"),
                &vl_type_Timestamp,
                &vl_method_Register,
                &vl_method_Unregister,
                SD_VARLINK_SYMBOL_COMMENT("Terminate machine, killing its processes"),
                &vl_method_Terminate,
                SD_VARLINK_SYMBOL_COMMENT("Send a UNIX signal to the machine's processes"),
                &vl_method_Kill,
                SD_VARLINK_SYMBOL_COMMENT("List running machines"),
                &vl_method_List,
                SD_VARLINK_SYMBOL_COMMENT("No matching machine currently running"),
                &vl_error_NoSuchMachine,
                &vl_error_MachineExists);
