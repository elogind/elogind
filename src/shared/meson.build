# SPDX-License-Identifier: LGPL-2.1+
#
# Copyright 2017 Zbigniew Jędrzejewski-Szmek
#
# elogind is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation; either version 2.1 of the License, or
# (at your option) any later version.
#
# elogind is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with elogind; If not, see <http://www.gnu.org/licenses/>.

#if 0 /// elogind has a much shorter list
# shared_sources = '''
#         acl-util.h
#         acpi-fpdt.c
#         acpi-fpdt.h
#         apparmor-util.c
#         apparmor-util.h
#         ask-password-api.c
#         ask-password-api.h
#         base-filesystem.c
#         base-filesystem.h
#         boot-timestamps.c
#         boot-timestamps.h
#         bootspec.c
#         bootspec.h
#         bus-unit-util.c
#         bus-unit-util.h
#         bus-util.c
#         bus-util.h
#         cgroup-show.c
#         cgroup-show.h
#         clean-ipc.c
#         clean-ipc.h
#         condition.c
#         condition.h
#         conf-parser.c
#         conf-parser.h
#         dev-setup.c
#         dev-setup.h
#         dissect-image.c
#         dissect-image.h
#         dns-domain.c
#         dns-domain.h
#         dropin.c
#         dropin.h
#         efivars.c
#         efivars.h
#         fdset.c
#         fdset.h
#         firewall-util.h
#         fstab-util.c
#         fstab-util.h
#         gcrypt-util.c
#         gcrypt-util.h
#         generator.c
#         generator.h
#         gpt.h
#         ima-util.c
#         ima-util.h
#         import-util.c
#         import-util.h
#         initreq.h
#         install.c
#         install.h
#         install-printf.c
#         install-printf.h
#         journal-util.c
#         journal-util.h
#         logs-show.c
#         logs-show.h
#         loop-util.c
#         loop-util.h
#         machine-image.c
#         machine-image.h
#         machine-pool.c
#         machine-pool.h
#         nsflags.c
#         nsflags.h
#         output-mode.c
#         output-mode.h
#         pager.c
#         pager.h
#         path-lookup.c
#         path-lookup.h
#         ptyfwd.c
#         ptyfwd.h
#         resolve-util.c
#         resolve-util.h
#         seccomp-util.h
#         sleep-config.c
#         sleep-config.h
#         spawn-ask-password-agent.c
#         spawn-ask-password-agent.h
#         spawn-polkit-agent.c
#         spawn-polkit-agent.h
#         specifier.c
#         specifier.h
#         switch-root.c
#         switch-root.h
#         sysctl-util.c
#         sysctl-util.h
#         tests.c
#         tests.h
#         tomoyo-util.c
#         tomoyo-util.h
#         udev-util.h
#         udev-util.c
#         uid-range.c
#         uid-range.h
#         utmp-wtmp.h
#         vlan-util.c
#         vlan-util.h
#         volatile-util.c
#         volatile-util.h
#         watchdog.c
#         watchdog.h
# '''.split()
#else
shared_sources = '''
        bus-util.c
        bus-util.h
        clean-ipc.c
        clean-ipc.h
        conf-parser.c
        conf-parser.h
        musl_missing.c
        musl_missing.h
        nsflags.c
        nsflags.h
        output-mode.c
        output-mode.h
        pager.c
        pager.h
        sleep-config.c
        sleep-config.h
        spawn-polkit-agent.c
        spawn-polkit-agent.h
        udev-util.c
        udev-util.h
'''.split()
#endif // 0

test_tables_h = files('test-tables.h')
shared_sources += [test_tables_h]

if conf.get('HAVE_ACL') == 1
        shared_sources += ['acl-util.c']
endif

if conf.get('ENABLE_UTMP') == 1
        shared_sources += ['utmp-wtmp.c']
endif

#if 0 /// UNNEEDED by elogind
# if conf.get('HAVE_SECCOMP') == 1
#         shared_sources += ['seccomp-util.c']
# endif
# 
# if conf.get('HAVE_LIBIPTC') == 1
#         shared_sources += ['firewall-util.c']
# endif
#endif // 0

libshared_name = 'elogind-shared-@0@'.format(meson.project_version())

#if 0 /// elogind doesn't need all this
# libshared_deps = [threads,
#                   librt,
#                   libcap,
#                   libacl,
#                   libcryptsetup,
#                   libgcrypt,
#                   libiptc,
#                   libseccomp,
#                   libselinux,
#                   libidn,
#                   libxz,
#                   liblz4,
#                   libblkid]
#else
libshared_deps = [threads,
                  libacl,
                  libcap,
                  libselinux]
#endif // 0

libshared_sym_path = '@0@/libshared.sym'.format(meson.current_source_dir())

libshared_static = static_library(
        libshared_name,
        shared_sources,
        include_directories : includes,
        dependencies : libshared_deps,
        c_args : ['-fvisibility=default'])

libshared = shared_library(
        libshared_name,
#if 0 /// UNNEEDED by elogind
#         journal_internal_sources,
#         libsystemd_sources,
#         libudev_sources,
#                      '-Wl,--version-script=' + libshared_sym_path],
#         link_whole : [libshared_static,
#                       libbasic],
#else
        libelogind_internal_sources,
#endif // 0
        include_directories : includes,
        link_args : ['-shared',
        c_args : ['-fvisibility=default'],
        dependencies : libshared_deps,
        install : true,
        install_dir : rootlibexecdir)
#if 1 /// need elogind internal library sources
        libelogind_internal_sources,
#endif // 1
