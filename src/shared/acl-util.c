/* SPDX-License-Identifier: LGPL-2.1-or-later */

#include <errno.h>
#include <stdbool.h>
//#include <sys/stat.h>
//#include <sys/types.h>

#include "acl-util.h"
//#include "alloc-util.h"
//#include "string-util.h"
//#include "strv.h"
#include "user-util.h"
//#include "util.h"

int acl_find_uid(acl_t acl, uid_t uid, acl_entry_t *ret_entry) {
        acl_entry_t i;
        int r;

        assert(acl);
        assert(uid_is_valid(uid));
        assert(ret_entry);

        for (r = acl_get_entry(acl, ACL_FIRST_ENTRY, &i);
             r > 0;
             r = acl_get_entry(acl, ACL_NEXT_ENTRY, &i)) {

                acl_tag_t tag;
                uid_t *u;
                bool b;

                if (acl_get_tag_type(i, &tag) < 0)
                        return -errno;

                if (tag != ACL_USER)
                        continue;

                u = acl_get_qualifier(i);
                if (!u)
                        return -errno;

                b = *u == uid;
                acl_free(u);

                if (b) {
                        *ret_entry = i;
                        return 1;
                }
        }
        if (r < 0)
                return -errno;

        *ret_entry = NULL;
        return 0;
}

#if 0 /// UNNEEDED by elogind
int calc_acl_mask_if_needed(acl_t *acl_p) {
        acl_entry_t i;
        int r;
        bool need = false;

        assert(acl_p);

        for (r = acl_get_entry(*acl_p, ACL_FIRST_ENTRY, &i);
             r > 0;
             r = acl_get_entry(*acl_p, ACL_NEXT_ENTRY, &i)) {
                acl_tag_t tag;

                if (acl_get_tag_type(i, &tag) < 0)
                        return -errno;

                if (tag == ACL_MASK)
                        return 0;

                if (IN_SET(tag, ACL_USER, ACL_GROUP))
                        need = true;
        }
        if (r < 0)
                return -errno;

        if (need && acl_calc_mask(acl_p) < 0)
                return -errno;

        return need;
}

int add_base_acls_if_needed(acl_t *acl_p, const char *path) {
        acl_entry_t i;
        int r;
        bool have_user_obj = false, have_group_obj = false, have_other = false;
        struct stat st;
        _cleanup_(acl_freep) acl_t basic = NULL;

        assert(acl_p);

        for (r = acl_get_entry(*acl_p, ACL_FIRST_ENTRY, &i);
             r > 0;
             r = acl_get_entry(*acl_p, ACL_NEXT_ENTRY, &i)) {
                acl_tag_t tag;

                if (acl_get_tag_type(i, &tag) < 0)
                        return -errno;

                if (tag == ACL_USER_OBJ)
                        have_user_obj = true;
                else if (tag == ACL_GROUP_OBJ)
                        have_group_obj = true;
                else if (tag == ACL_OTHER)
                        have_other = true;
                if (have_user_obj && have_group_obj && have_other)
                        return 0;
        }
        if (r < 0)
                return -errno;

        r = stat(path, &st);
        if (r < 0)
                return -errno;

        basic = acl_from_mode(st.st_mode);
        if (!basic)
                return -errno;

        for (r = acl_get_entry(basic, ACL_FIRST_ENTRY, &i);
             r > 0;
             r = acl_get_entry(basic, ACL_NEXT_ENTRY, &i)) {
                acl_tag_t tag;
                acl_entry_t dst;

                if (acl_get_tag_type(i, &tag) < 0)
                        return -errno;

                if ((tag == ACL_USER_OBJ && have_user_obj) ||
                    (tag == ACL_GROUP_OBJ && have_group_obj) ||
                    (tag == ACL_OTHER && have_other))
                        continue;

                r = acl_create_entry(acl_p, &dst);
                if (r < 0)
                        return -errno;

                r = acl_copy_entry(dst, i);
                if (r < 0)
                        return -errno;
        }
        if (r < 0)
                return -errno;
        return 0;
}

int acl_search_groups(const char *path, char ***ret_groups) {
        _cleanup_strv_free_ char **g = NULL;
        _cleanup_(acl_freep) acl_t acl = NULL;
        bool ret = false;
        acl_entry_t entry;
        int r;

        assert(path);

        acl = acl_get_file(path, ACL_TYPE_DEFAULT);
        if (!acl)
                return -errno;

        r = acl_get_entry(acl, ACL_FIRST_ENTRY, &entry);
        for (;;) {
                _cleanup_(acl_free_gid_tpp) gid_t *gid = NULL;
                acl_tag_t tag;

                if (r < 0)
                        return -errno;
                if (r == 0)
                        break;

                if (acl_get_tag_type(entry, &tag) < 0)
                        return -errno;

                if (tag != ACL_GROUP)
                        goto next;

                gid = acl_get_qualifier(entry);
                if (!gid)
                        return -errno;

                if (in_gid(*gid) > 0) {
                        if (!ret_groups)
                                return true;

                        ret = true;
                }

                if (ret_groups) {
                        char *name;

                        name = gid_to_name(*gid);
                        if (!name)
                                return -ENOMEM;

                        r = strv_consume(&g, name);
                        if (r < 0)
                                return r;
                }

        next:
                r = acl_get_entry(acl, ACL_NEXT_ENTRY, &entry);
        }

        if (ret_groups)
                *ret_groups = TAKE_PTR(g);

        return ret;
}

int parse_acl(const char *text, acl_t *acl_access, acl_t *acl_default, bool want_mask) {
        _cleanup_free_ char **a = NULL, **d = NULL; /* strings are not freed */
        _cleanup_strv_free_ char **split = NULL;
        int r = -EINVAL;
        _cleanup_(acl_freep) acl_t a_acl = NULL, d_acl = NULL;

        split = strv_split(text, ",");
        if (!split)
                return -ENOMEM;

        STRV_FOREACH(entry, split) {
                char *p;

                p = STARTSWITH_SET(*entry, "default:", "d:");
                if (p)
                        r = strv_push(&d, p);
                else
                        r = strv_push(&a, *entry);
                if (r < 0)
                        return r;
        }

        if (!strv_isempty(a)) {
                _cleanup_free_ char *join = NULL;

                join = strv_join(a, ",");
                if (!join)
                        return -ENOMEM;

                a_acl = acl_from_text(join);
                if (!a_acl)
                        return -errno;

                if (want_mask) {
                        r = calc_acl_mask_if_needed(&a_acl);
                        if (r < 0)
                                return r;
                }
        }

        if (!strv_isempty(d)) {
                _cleanup_free_ char *join = NULL;

                join = strv_join(d, ",");
                if (!join)
                        return -ENOMEM;

                d_acl = acl_from_text(join);
                if (!d_acl)
                        return -errno;

                if (want_mask) {
                        r = calc_acl_mask_if_needed(&d_acl);
                        if (r < 0)
                                return r;
                }
        }

        *acl_access = TAKE_PTR(a_acl);
        *acl_default = TAKE_PTR(d_acl);

        return 0;
}

static int acl_entry_equal(acl_entry_t a, acl_entry_t b) {
        acl_tag_t tag_a, tag_b;

        if (acl_get_tag_type(a, &tag_a) < 0)
                return -errno;

        if (acl_get_tag_type(b, &tag_b) < 0)
                return -errno;

        if (tag_a != tag_b)
                return false;

        switch (tag_a) {
        case ACL_USER_OBJ:
        case ACL_GROUP_OBJ:
        case ACL_MASK:
        case ACL_OTHER:
                /* can have only one of those */
                return true;
        case ACL_USER: {
                _cleanup_(acl_free_uid_tpp) uid_t *uid_a = NULL, *uid_b = NULL;

                uid_a = acl_get_qualifier(a);
                if (!uid_a)
                        return -errno;

                uid_b = acl_get_qualifier(b);
                if (!uid_b)
                        return -errno;

                return *uid_a == *uid_b;
        }
        case ACL_GROUP: {
                _cleanup_(acl_free_gid_tpp) gid_t *gid_a = NULL, *gid_b = NULL;

                gid_a = acl_get_qualifier(a);
                if (!gid_a)
                        return -errno;

                gid_b = acl_get_qualifier(b);
                if (!gid_b)
                        return -errno;

                return *gid_a == *gid_b;
        }
        default:
                assert_not_reached();
        }
}

static int find_acl_entry(acl_t acl, acl_entry_t entry, acl_entry_t *out) {
        acl_entry_t i;
        int r;

        for (r = acl_get_entry(acl, ACL_FIRST_ENTRY, &i);
             r > 0;
             r = acl_get_entry(acl, ACL_NEXT_ENTRY, &i)) {

                r = acl_entry_equal(i, entry);
                if (r < 0)
                        return r;
                if (r > 0) {
                        *out = i;
                        return 1;
                }
        }
        if (r < 0)
                return -errno;
        return 0;
}

int acls_for_file(const char *path, acl_type_t type, acl_t new, acl_t *acl) {
        _cleanup_(acl_freep) acl_t old;
        acl_entry_t i;
        int r;

        old = acl_get_file(path, type);
        if (!old)
                return -errno;

        for (r = acl_get_entry(new, ACL_FIRST_ENTRY, &i);
             r > 0;
             r = acl_get_entry(new, ACL_NEXT_ENTRY, &i)) {

                acl_entry_t j;

                r = find_acl_entry(old, i, &j);
                if (r < 0)
                        return r;
                if (r == 0)
                        if (acl_create_entry(&old, &j) < 0)
                                return -errno;

                if (acl_copy_entry(j, i) < 0)
                        return -errno;
        }
        if (r < 0)
                return -errno;

        *acl = TAKE_PTR(old);

        return 0;
}

/* POSIX says that ACL_{READ,WRITE,EXECUTE} don't have to be bitmasks. But that is a natural thing to do and
 * all extant implementations do it. Let's make sure that we fail verbosely in the (imho unlikely) scenario
 * that we get a new implementation that does not satisfy this. */
assert_cc(!(ACL_READ & ACL_WRITE));
assert_cc(!(ACL_WRITE & ACL_EXECUTE));
assert_cc(!(ACL_EXECUTE & ACL_READ));
assert_cc((unsigned) ACL_READ == ACL_READ);
assert_cc((unsigned) ACL_WRITE == ACL_WRITE);
assert_cc((unsigned) ACL_EXECUTE == ACL_EXECUTE);

int fd_add_uid_acl_permission(
                int fd,
                uid_t uid,
                unsigned mask) {

        _cleanup_(acl_freep) acl_t acl = NULL;
        acl_permset_t permset;
        acl_entry_t entry;
        int r;

        /* Adds an ACL entry for the specified file to allow the indicated access to the specified
         * user. Operates purely incrementally. */

        assert(fd >= 0);
        assert(uid_is_valid(uid));

        acl = acl_get_fd(fd);
        if (!acl)
                return -errno;

        r = acl_find_uid(acl, uid, &entry);
        if (r <= 0) {
                if (acl_create_entry(&acl, &entry) < 0 ||
                    acl_set_tag_type(entry, ACL_USER) < 0 ||
                    acl_set_qualifier(entry, &uid) < 0)
                        return -errno;
        }

        if (acl_get_permset(entry, &permset) < 0)
                return -errno;

        if ((mask & ACL_READ) && acl_add_perm(permset, ACL_READ) < 0)
                return -errno;
        if ((mask & ACL_WRITE) && acl_add_perm(permset, ACL_WRITE) < 0)
                return -errno;
        if ((mask & ACL_EXECUTE) && acl_add_perm(permset, ACL_EXECUTE) < 0)
                return -errno;

        r = calc_acl_mask_if_needed(&acl);
        if (r < 0)
                return r;

        if (acl_set_fd(fd, acl) < 0)
                return -errno;

        return 0;
}
#endif // 0
