/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

/***
  This file is part of systemd.

  Copyright 2010 Lennart Poettering
***/

//#include <grp.h>
//#include <gshadow.h>
//#include <pwd.h>
//#include <shadow.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>

bool uid_is_valid(uid_t uid);

static inline bool gid_is_valid(gid_t gid) {
        return uid_is_valid((uid_t) gid);
}

int parse_uid(const char *s, uid_t* ret_uid);

static inline int parse_gid(const char *s, gid_t *ret_gid) {
        return parse_uid(s, (uid_t*) ret_gid);
}

char* getlogname_malloc(void);
#if 0 /// UNNEEDED by elogind
char* getusername_malloc(void);
#endif // 0

int get_user_creds(const char **username, uid_t *uid, gid_t *gid, const char **home, const char **shell);
#if 0 /// UNNEEDED by elogind
int get_user_creds_clean(const char **username, uid_t *uid, gid_t *gid, const char **home, const char **shell);
int get_group_creds(const char **groupname, gid_t *gid);
#endif // 0

char* uid_to_name(uid_t uid);
char* gid_to_name(gid_t gid);

#if 0 /// UNNEEDED by elogind
int in_gid(gid_t gid);
int in_group(const char *name);

int get_home_dir(char **ret);
int get_shell(char **_ret);
#endif // 0

int reset_uid_gid(void);

#if 0 /// UNNEEDED by elogind
int take_etc_passwd_lock(const char *root);
#endif // 0

#define UID_INVALID ((uid_t) -1)
#define GID_INVALID ((gid_t) -1)

#define UID_NOBODY ((uid_t) 65534U)
#define GID_NOBODY ((gid_t) 65534U)

#define ETC_PASSWD_LOCK_PATH "/etc/.pwd.lock"

#if 0 /// UNNEEDED by elogind
static inline bool uid_is_dynamic(uid_t uid) {
        return DYNAMIC_UID_MIN <= uid && uid <= DYNAMIC_UID_MAX;
}

static inline bool gid_is_dynamic(gid_t gid) {
        return uid_is_dynamic((uid_t) gid);
}
#endif // 0

static inline bool uid_is_system(uid_t uid) {
        return uid <= SYSTEM_UID_MAX;
}

static inline bool gid_is_system(gid_t gid) {
        return gid <= SYSTEM_GID_MAX;
}

/* The following macros add 1 when converting things, since UID 0 is a valid UID, while the pointer
 * NULL is special */
#define PTR_TO_UID(p) ((uid_t) (((uintptr_t) (p))-1))
#define UID_TO_PTR(u) ((void*) (((uintptr_t) (u))+1))

#define PTR_TO_GID(p) ((gid_t) (((uintptr_t) (p))-1))
#define GID_TO_PTR(u) ((void*) (((uintptr_t) (u))+1))

static inline bool userns_supported(void) {
        return access("/proc/self/uid_map", F_OK) >= 0;
}

bool valid_user_group_name(const char *u);
bool valid_user_group_name_or_id(const char *u);
bool valid_gecos(const char *d);
bool valid_home(const char *p);

static inline bool valid_shell(const char *p) {
        /* We have the same requirements, so just piggy-back on the home check.
         *
         * Let's ignore /etc/shells because this is only applicable to real and
         * not system users. It is also incompatible with the idea of empty /etc.
         */
        return valid_home(p);
}

int maybe_setgroups(size_t size, const gid_t *list);

bool synthesize_nobody(void);

int fgetpwent_sane(FILE *stream, struct passwd **pw);
int fgetspent_sane(FILE *stream, struct spwd **sp);
int fgetgrent_sane(FILE *stream, struct group **gr);
int putpwent_sane(const struct passwd *pw, FILE *stream);
int putspent_sane(const struct spwd *sp, FILE *stream);
int putgrent_sane(const struct group *gr, FILE *stream);
#ifdef ENABLE_GSHADOW
int fgetsgent_sane(FILE *stream, struct sgrp **sg);
int putsgent_sane(const struct sgrp *sg, FILE *stream);
#endif
