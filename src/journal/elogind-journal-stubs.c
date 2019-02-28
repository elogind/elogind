/***
  This file is part of elogind.

  Copyright 2019 Sven Eden

  elogind is free software; you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation; either version 2.1 of the License, or
  (at your option) any later version.

  elogind is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with elogind; If not, see <http://www.gnu.org/licenses/>.
***/

#include "elogind-journal-stubs.h"
#include "journal-internal.h"

_public_ int sd_journal_add_conjunction(sd_journal *j) {
        return -ENOSYS;
}

_public_ int sd_journal_add_disjunction(sd_journal *j) {
        return -ENOSYS;
}

_public_ int sd_journal_add_match(sd_journal *j, const void *data, size_t size) {
        return -ENOSYS;
}

_public_ int sd_journal_enumerate_data(sd_journal *j, const void **data, size_t *size) {
        return -ENOSYS;
}

_public_ int sd_journal_enumerate_fields(sd_journal *j, const char **field) {
        return -ENOSYS;
}

_public_ int sd_journal_enumerate_unique(sd_journal *j, const void **data, size_t *l) {
        return -ENOSYS;
}

_public_ int sd_journal_get_catalog_for_message_id(sd_id128_t id, char **ret) {
        return -ENOSYS;
}

_public_ int sd_journal_get_catalog(sd_journal *j, char **ret) {
        return -ENOSYS;
}

_public_ int sd_journal_get_cursor(sd_journal *j, char **cursor) {
        return -ENOSYS;
}

_public_ int sd_journal_get_cutoff_monotonic_usec(sd_journal *j, sd_id128_t boot_id, uint64_t *from, uint64_t *to) {
        return -ENOSYS;
}

_public_ int sd_journal_get_cutoff_realtime_usec(sd_journal *j, uint64_t *from, uint64_t *to) {
        return -ENOSYS;
}

_public_ int sd_journal_get_data(sd_journal *j, const char *field, const void **data, size_t *size) {
        return -ENOSYS;
}

_public_ int sd_journal_get_data_threshold(sd_journal *j, size_t *sz) {
        return -ENOSYS;
}

_public_ int sd_journal_get_events(sd_journal *j) {
        return -ENOSYS;
}

_public_ int sd_journal_get_fd(sd_journal *j) {
        return -ENOSYS;
}

_public_ int sd_journal_get_monotonic_usec(sd_journal *j, uint64_t *ret, sd_id128_t *ret_boot_id) {
        return -ENOSYS;
}

_public_ int sd_journal_get_realtime_usec(sd_journal *j, uint64_t *ret) {
        return -ENOSYS;
}

_public_ int sd_journal_get_timeout(sd_journal *j, uint64_t *timeout_usec) {
        return -ENOSYS;
}

_public_ int sd_journal_get_usage(sd_journal *j, uint64_t *bytes) {
        return -ENOSYS;
}

_public_ int sd_journal_has_persistent_files(sd_journal *j) {
        return -ENOSYS;
}

_public_ int sd_journal_has_runtime_files(sd_journal *j) {
        return -ENOSYS;
}

_public_ int sd_journal_next(sd_journal *j) {
        return -ENOSYS;
}

_public_ int sd_journal_next_skip(sd_journal *j, uint64_t skip) {
        return -ENOSYS;
}

_public_ int sd_journal_open_container(sd_journal **ret, const char *machine, int flags) {
        return -ENOSYS;
}

_public_ int sd_journal_open_directory_fd(sd_journal **ret, int fd, int flags) {
        return -ENOSYS;
}

_public_ int sd_journal_open_directory(sd_journal **ret, const char *path, int flags) {
        return -ENOSYS;
}

_public_ int sd_journal_open_files_fd(sd_journal **ret, int fds[], unsigned n_fds, int flags) {
        return -ENOSYS;
}

_public_ int sd_journal_open_files(sd_journal **ret, const char **paths, int flags) {
        return -ENOSYS;
}

_public_ int sd_journal_open(sd_journal **ret, int flags) {
        return -ENOSYS;
}

_public_ int sd_journal_perror(const char *message) {
        return -ENOSYS;
}

_public_ int sd_journal_perror_with_location(
                const char *file, const char *line,
                const char *func,
                const char *message) {
        return -ENOSYS;
}

_public_ int sd_journal_previous(sd_journal *j) {
        return -ENOSYS;
}

_public_ int sd_journal_previous_skip(sd_journal *j, uint64_t skip) {
        return -ENOSYS;
}

_public_ int sd_journal_print(int priority, const char *format, ...) {
        return -ENOSYS;
}

_public_ int sd_journal_printv(int priority, const char *format, va_list ap) {
        return -ENOSYS;
}

_public_ int sd_journal_printv_with_location(int priority, const char *file, const char *line, const char *func, const char *format, va_list ap) {
        return -ENOSYS;
}

_public_ int sd_journal_print_with_location(int priority, const char *file, const char *line, const char *func, const char *format, ...) {
        return -ENOSYS;
}

_public_ int sd_journal_process(sd_journal *j) {
        return -ENOSYS;
}

_public_ int sd_journal_query_unique(sd_journal *j, const char *field) {
        return -ENOSYS;
}

_public_ int sd_journal_reliable_fd(sd_journal *j) {
        return -ENOSYS;
}

_public_ int sd_journal_seek_cursor(sd_journal *j, const char *cursor) {
        return -ENOSYS;
}

_public_ int sd_journal_seek_head(sd_journal *j) {
        return -ENOSYS;
}

_public_ int sd_journal_seek_monotonic_usec(sd_journal *j, sd_id128_t boot_id, uint64_t usec) {
        return -ENOSYS;
}

_public_ int sd_journal_seek_realtime_usec(sd_journal *j, uint64_t usec) {
        return -ENOSYS;
}

_public_ int sd_journal_seek_tail(sd_journal *j) {
        return -ENOSYS;
}

_public_ int sd_journal_send(const char *format, ...) {
        return -ENOSYS;
}

_public_ int sd_journal_sendv(const struct iovec *iov, int n) {
        return -ENOSYS;
}

_public_ int sd_journal_sendv_with_location(
                const char *file, const char *line,
                const char *func,
                const struct iovec *iov, int n) {
        return -ENOSYS;
}

_public_ int sd_journal_send_with_location(const char *file, const char *line, const char *func, const char *format, ...) {
        return -ENOSYS;
}

_public_ int sd_journal_set_data_threshold(sd_journal *j, size_t sz) {
        return -ENOSYS;
}

_public_ int sd_journal_stream_fd(const char *identifier, int priority, int level_prefix) {
        return -ENOSYS;
}

_public_ int sd_journal_test_cursor(sd_journal *j, const char *cursor) {
        return -ENOSYS;
}

_public_ int sd_journal_wait(sd_journal *j, uint64_t timeout_usec) {
        return -ENOSYS;
}

_public_ void sd_journal_close(sd_journal *j) {
        return;
}

_public_ void sd_journal_flush_matches(sd_journal *j) {
        return;
}

_public_ void sd_journal_restart_data(sd_journal *j) {
        return;
}

_public_ void sd_journal_restart_fields(sd_journal *j) {
        return;
}

_public_ void sd_journal_restart_unique(sd_journal *j) {
        return;
}
