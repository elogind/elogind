/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

/***
***/

#include <endian.h>

#include "macro.h"

/* Packet header */

struct _packed_ bus_header {
        /* The first four fields are identical for dbus1, and dbus2 */
        uint8_t endian;
        uint8_t type;
        uint8_t flags;
        uint8_t version;

        union _packed_ {
                /* dbus1: Used for SOCK_STREAM connections */
                struct _packed_ {
                        uint32_t body_size;

                        /* Note that what the bus spec calls "serial" we'll call
                           "cookie" instead, because we don't want to imply that the
                           cookie was in any way monotonically increasing. */
                        uint32_t serial;
                        uint32_t fields_size;
                } dbus1;

                /* dbus2: Used for kdbus connections */
                struct _packed_ {
                        uint32_t _reserved;
                        uint64_t cookie;
                } dbus2;

                /* Note that both header versions have the same size! */
        };
};

/* Endianness */

enum {
        _BUS_INVALID_ENDIAN = 0,
        BUS_LITTLE_ENDIAN   = 'l',
        BUS_BIG_ENDIAN      = 'B',
#if __BYTE_ORDER == __BIG_ENDIAN
        BUS_NATIVE_ENDIAN   = BUS_BIG_ENDIAN,
        BUS_REVERSE_ENDIAN  = BUS_LITTLE_ENDIAN
#else
        BUS_NATIVE_ENDIAN   = BUS_LITTLE_ENDIAN,
        BUS_REVERSE_ENDIAN  = BUS_BIG_ENDIAN
#endif
};

/* Flags */

enum {
        BUS_MESSAGE_NO_REPLY_EXPECTED = 1,
        BUS_MESSAGE_NO_AUTO_START = 2,
        BUS_MESSAGE_ALLOW_INTERACTIVE_AUTHORIZATION = 4,
};

/* Header fields */

enum {
        _BUS_MESSAGE_HEADER_INVALID = 0,
        BUS_MESSAGE_HEADER_PATH,
        BUS_MESSAGE_HEADER_INTERFACE,
        BUS_MESSAGE_HEADER_MEMBER,
        BUS_MESSAGE_HEADER_ERROR_NAME,
        BUS_MESSAGE_HEADER_REPLY_SERIAL,
        BUS_MESSAGE_HEADER_DESTINATION,
        BUS_MESSAGE_HEADER_SENDER,
        BUS_MESSAGE_HEADER_SIGNATURE,
        BUS_MESSAGE_HEADER_UNIX_FDS,
        _BUS_MESSAGE_HEADER_MAX
};

/* RequestName parameters */

enum  {
        BUS_NAME_ALLOW_REPLACEMENT = 1,
        BUS_NAME_REPLACE_EXISTING = 2,
        BUS_NAME_DO_NOT_QUEUE = 4
};

/* RequestName returns */
enum  {
        BUS_NAME_PRIMARY_OWNER = 1,
        BUS_NAME_IN_QUEUE = 2,
        BUS_NAME_EXISTS = 3,
        BUS_NAME_ALREADY_OWNER = 4
};

/* ReleaseName returns */
enum {
        BUS_NAME_RELEASED = 1,
        BUS_NAME_NON_EXISTENT = 2,
        BUS_NAME_NOT_OWNER = 3,
};

/* StartServiceByName returns */
enum {
        BUS_START_REPLY_SUCCESS = 1,
        BUS_START_REPLY_ALREADY_RUNNING = 2,
};

#define BUS_INTROSPECT_DOCTYPE                                       \
        "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n" \
        "\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"

#define BUS_INTROSPECT_INTERFACE_PEER                                \
        " <interface name=\"org.freedesktop.DBus.Peer\">\n"             \
        "  <method name=\"Ping\"/>\n"                                   \
        "  <method name=\"GetMachineId\">\n"                            \
        "   <arg type=\"s\" name=\"machine_uuid\" direction=\"out\"/>\n" \
        "  </method>\n"                                                 \
        " </interface>\n"

#define BUS_INTROSPECT_INTERFACE_INTROSPECTABLE                      \
        " <interface name=\"org.freedesktop.DBus.Introspectable\">\n"   \
        "  <method name=\"Introspect\">\n"                              \
        "   <arg name=\"data\" type=\"s\" direction=\"out\"/>\n"        \
        "  </method>\n"                                                 \
        " </interface>\n"

#define BUS_INTROSPECT_INTERFACE_PROPERTIES                          \
        " <interface name=\"org.freedesktop.DBus.Properties\">\n"       \
        "  <method name=\"Get\">\n"                                     \
        "   <arg name=\"interface\" direction=\"in\" type=\"s\"/>\n"    \
        "   <arg name=\"property\" direction=\"in\" type=\"s\"/>\n"     \
        "   <arg name=\"value\" direction=\"out\" type=\"v\"/>\n"       \
        "  </method>\n"                                                 \
        "  <method name=\"GetAll\">\n"                                  \
        "   <arg name=\"interface\" direction=\"in\" type=\"s\"/>\n"    \
        "   <arg name=\"properties\" direction=\"out\" type=\"a{sv}\"/>\n" \
        "  </method>\n"                                                 \
        "  <method name=\"Set\">\n"                                     \
        "   <arg name=\"interface\" direction=\"in\" type=\"s\"/>\n"    \
        "   <arg name=\"property\" direction=\"in\" type=\"s\"/>\n"     \
        "   <arg name=\"value\" direction=\"in\" type=\"v\"/>\n"        \
        "  </method>\n"                                                 \
        "  <signal name=\"PropertiesChanged\">\n"                       \
        "   <arg type=\"s\" name=\"interface\"/>\n"                     \
        "   <arg type=\"a{sv}\" name=\"changed_properties\"/>\n"        \
        "   <arg type=\"as\" name=\"invalidated_properties\"/>\n"       \
        "  </signal>\n"                                                 \
        " </interface>\n"

#define BUS_INTROSPECT_INTERFACE_OBJECT_MANAGER                      \
        " <interface name=\"org.freedesktop.DBus.ObjectManager\">\n"    \
        "  <method name=\"GetManagedObjects\">\n"                       \
        "   <arg type=\"a{oa{sa{sv}}}\" name=\"object_paths_interfaces_and_properties\" direction=\"out\"/>\n" \
        "  </method>\n"                                                 \
        "  <signal name=\"InterfacesAdded\">\n"                         \
        "   <arg type=\"o\" name=\"object_path\"/>\n"                   \
        "   <arg type=\"a{sa{sv}}\" name=\"interfaces_and_properties\"/>\n" \
        "  </signal>\n"                                                 \
        "  <signal name=\"InterfacesRemoved\">\n"                       \
        "   <arg type=\"o\" name=\"object_path\"/>\n"                   \
        "   <arg type=\"as\" name=\"interfaces\"/>\n"                   \
        "  </signal>\n"                                                 \
        " </interface>\n"
