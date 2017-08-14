#pragma once

/***
  This file is part of systemd.

  Copyright 2010 Lennart Poettering

  systemd is free software; you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation; either version 2.1 of the License, or
  (at your option) any later version.

  systemd is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with systemd; If not, see <http://www.gnu.org/licenses/>.
***/

#ifdef HAVE_PAM
#define _PAM_FEATURE_ "+PAM"
#else
#define _PAM_FEATURE_ "-PAM"
#endif

#ifdef HAVE_AUDIT
#define _AUDIT_FEATURE_ "+AUDIT"
#else
#define _AUDIT_FEATURE_ "-AUDIT"
#endif

#ifdef HAVE_SELINUX
#define _SELINUX_FEATURE_ "+SELINUX"
#else
#define _SELINUX_FEATURE_ "-SELINUX"
#endif

#ifdef HAVE_SMACK
#define _SMACK_FEATURE_ "+SMACK"
#else
#define _SMACK_FEATURE_ "-SMACK"
#endif

#ifdef HAVE_UTMP
#define _UTMP_FEATURE_ "+UTMP"
#else
#define _UTMP_FEATURE_ "-UTMP"
#endif

#ifdef HAVE_ACL
#define _ACL_FEATURE_ "+ACL"
#else
#define _ACL_FEATURE_ "-ACL"
#endif

#define _CGROUP_HIEARCHY_ "default-hierarchy=" DEFAULT_HIERARCHY_NAME

#define SYSTEMD_FEATURES                                                \
        _PAM_FEATURE_ " "                                               \
        _AUDIT_FEATURE_ " "                                             \
        _SELINUX_FEATURE_ " "                                           \
        _SMACK_FEATURE_ " "                                             \
        _UTMP_FEATURE_ " "                                              \
        _ACL_FEATURE_ " "                                               \
        _CGROUP_HIEARCHY_
