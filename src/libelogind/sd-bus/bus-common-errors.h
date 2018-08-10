/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

/***
***/

#include "bus-error.h"

#if 0 /// only system command elogind knows are needed
#define BUS_ERROR_NO_SUCH_UNIT "org.freedesktop.systemd1.NoSuchUnit"
#define BUS_ERROR_NO_UNIT_FOR_PID "org.freedesktop.systemd1.NoUnitForPID"
#define BUS_ERROR_NO_UNIT_FOR_INVOCATION_ID "org.freedesktop.elogind1.NoUnitForInvocationID"
#define BUS_ERROR_UNIT_EXISTS "org.freedesktop.systemd1.UnitExists"
#define BUS_ERROR_LOAD_FAILED "org.freedesktop.systemd1.LoadFailed"
#define BUS_ERROR_BAD_UNIT_SETTING "org.freedesktop.systemd1.BadUnitSetting"
#define BUS_ERROR_JOB_FAILED "org.freedesktop.systemd1.JobFailed"
#define BUS_ERROR_NO_SUCH_JOB "org.freedesktop.systemd1.NoSuchJob"
#define BUS_ERROR_NOT_SUBSCRIBED "org.freedesktop.systemd1.NotSubscribed"
#define BUS_ERROR_ALREADY_SUBSCRIBED "org.freedesktop.systemd1.AlreadySubscribed"
#define BUS_ERROR_ONLY_BY_DEPENDENCY "org.freedesktop.systemd1.OnlyByDependency"
#define BUS_ERROR_TRANSACTION_JOBS_CONFLICTING "org.freedesktop.systemd1.TransactionJobsConflicting"
#define BUS_ERROR_TRANSACTION_ORDER_IS_CYCLIC "org.freedesktop.systemd1.TransactionOrderIsCyclic"
#define BUS_ERROR_TRANSACTION_IS_DESTRUCTIVE "org.freedesktop.systemd1.TransactionIsDestructive"
#define BUS_ERROR_UNIT_MASKED "org.freedesktop.systemd1.UnitMasked"
#define BUS_ERROR_UNIT_GENERATED "org.freedesktop.systemd1.UnitGenerated"
#define BUS_ERROR_UNIT_LINKED "org.freedesktop.systemd1.UnitLinked"
#define BUS_ERROR_JOB_TYPE_NOT_APPLICABLE "org.freedesktop.systemd1.JobTypeNotApplicable"
#define BUS_ERROR_NO_ISOLATION "org.freedesktop.systemd1.NoIsolation"
#define BUS_ERROR_SHUTTING_DOWN "org.freedesktop.systemd1.ShuttingDown"
#define BUS_ERROR_SCOPE_NOT_RUNNING "org.freedesktop.systemd1.ScopeNotRunning"
#endif // 0
#define BUS_ERROR_NO_SUCH_DYNAMIC_USER "org.freedesktop.elogind1.NoSuchDynamicUser"
#define BUS_ERROR_NOT_REFERENCED "org.freedesktop.elogind1.NotReferenced"
#define BUS_ERROR_DISK_FULL "org.freedesktop.elogind1.DiskFull"

#if 0 /// no machined in elogind
#define BUS_ERROR_NO_SUCH_MACHINE "org.freedesktop.machine1.NoSuchMachine"
#define BUS_ERROR_NO_SUCH_IMAGE "org.freedesktop.machine1.NoSuchImage"
#define BUS_ERROR_NO_MACHINE_FOR_PID "org.freedesktop.machine1.NoMachineForPID"
#define BUS_ERROR_MACHINE_EXISTS "org.freedesktop.machine1.MachineExists"
#define BUS_ERROR_NO_PRIVATE_NETWORKING "org.freedesktop.machine1.NoPrivateNetworking"
#define BUS_ERROR_NO_SUCH_USER_MAPPING "org.freedesktop.machine1.NoSuchUserMapping"
#define BUS_ERROR_NO_SUCH_GROUP_MAPPING "org.freedesktop.machine1.NoSuchGroupMapping"
#endif // 0

#define BUS_ERROR_NO_SUCH_PORTABLE_IMAGE "org.freedesktop.portable1.NoSuchImage"

#define BUS_ERROR_NO_SUCH_SESSION "org.freedesktop.login1.NoSuchSession"
#define BUS_ERROR_NO_SESSION_FOR_PID "org.freedesktop.login1.NoSessionForPID"
#define BUS_ERROR_NO_SUCH_USER "org.freedesktop.login1.NoSuchUser"
#define BUS_ERROR_NO_USER_FOR_PID "org.freedesktop.login1.NoUserForPID"
#define BUS_ERROR_NO_SUCH_SEAT "org.freedesktop.login1.NoSuchSeat"
#define BUS_ERROR_SESSION_NOT_ON_SEAT "org.freedesktop.login1.SessionNotOnSeat"
#define BUS_ERROR_NOT_IN_CONTROL "org.freedesktop.login1.NotInControl"
#define BUS_ERROR_DEVICE_IS_TAKEN "org.freedesktop.login1.DeviceIsTaken"
#define BUS_ERROR_DEVICE_NOT_TAKEN "org.freedesktop.login1.DeviceNotTaken"
#define BUS_ERROR_OPERATION_IN_PROGRESS "org.freedesktop.login1.OperationInProgress"
#define BUS_ERROR_SLEEP_VERB_NOT_SUPPORTED "org.freedesktop.login1.SleepVerbNotSupported"
#define BUS_ERROR_SESSION_BUSY "org.freedesktop.login1.SessionBusy"

#if 0 /// more services unsupported by elogind
#define BUS_ERROR_AUTOMATIC_TIME_SYNC_ENABLED "org.freedesktop.timedate1.AutomaticTimeSyncEnabled"
#define BUS_ERROR_NO_NTP_SUPPORT "org.freedesktop.timedate1.NoNTPSupport"

#define BUS_ERROR_NO_SUCH_PROCESS "org.freedesktop.systemd1.NoSuchProcess"

#define BUS_ERROR_NO_NAME_SERVERS "org.freedesktop.resolve1.NoNameServers"
#define BUS_ERROR_INVALID_REPLY "org.freedesktop.resolve1.InvalidReply"
#define BUS_ERROR_NO_SUCH_RR "org.freedesktop.resolve1.NoSuchRR"
#define BUS_ERROR_CNAME_LOOP "org.freedesktop.resolve1.CNameLoop"
#define BUS_ERROR_ABORTED "org.freedesktop.resolve1.Aborted"
#define BUS_ERROR_NO_SUCH_SERVICE "org.freedesktop.resolve1.NoSuchService"
#define BUS_ERROR_DNSSEC_FAILED "org.freedesktop.resolve1.DnssecFailed"
#define BUS_ERROR_NO_TRUST_ANCHOR "org.freedesktop.resolve1.NoTrustAnchor"
#define BUS_ERROR_RR_TYPE_UNSUPPORTED "org.freedesktop.resolve1.ResourceRecordTypeUnsupported"
#define BUS_ERROR_NO_SUCH_LINK "org.freedesktop.resolve1.NoSuchLink"
#define BUS_ERROR_LINK_BUSY "org.freedesktop.resolve1.LinkBusy"
#define BUS_ERROR_NETWORK_DOWN "org.freedesktop.resolve1.NetworkDown"
#define BUS_ERROR_NO_SUCH_DNSSD_SERVICE "org.freedesktop.resolve1.NoSuchDnssdService"
#define BUS_ERROR_DNSSD_SERVICE_EXISTS "org.freedesktop.resolve1.DnssdServiceExists"
#define _BUS_ERROR_DNS "org.freedesktop.resolve1.DnsError."

#define BUS_ERROR_NO_SUCH_TRANSFER "org.freedesktop.import1.NoSuchTransfer"
#define BUS_ERROR_TRANSFER_IN_PROGRESS "org.freedesktop.import1.TransferInProgress"
#endif // 0

BUS_ERROR_MAP_ELF_USE(bus_common_errors);
