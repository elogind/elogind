/* SPDX-License-Identifier: LGPL-2.1+ */
/***
  This file is part of systemd.

  Copyright 2016 Zbigniew Jędrzejewski-Szmek
***/

#include <stdio.h>
#include <string.h>

#include "time-util.h"

/* Print information about various types. Useful when diagnosing
 * gcc diagnostics on an unfamiliar architecture. */

#pragma GCC diagnostic ignored "-Wtype-limits"

#define info(t)                                                 \
        printf("%s → %zu bits%s\n", STRINGIFY(t),               \
               sizeof(t)*CHAR_BIT,                              \
               strstr(STRINGIFY(t), "signed") ? "" :            \
               ((t)-1 < (t)0 ? ", signed" : ", unsigned"));

enum Enum {
        enum_value,
};

enum BigEnum {
        big_enum_value = UINT64_C(1),
};

enum BigEnum2 {
        big_enum2_pos = UINT64_C(1),
        big_enum2_neg = UINT64_C(-1),
};

int main(void) {
        info(char);
        info(signed char);
        info(unsigned char);
        info(short unsigned);
        info(unsigned);
        info(long unsigned);
        info(long long unsigned);
#ifdef __GLIBC__
        info(__syscall_ulong_t);
        info(__syscall_slong_t);
#endif // ifdef __GLIBC__

        info(float);
        info(double);
        info(long double);

#ifdef __HAVE_DISTINCT_FLOAT128X
        info(_Float128);
        info(_Float64);
        info(_Float64x);
        info(_Float32);
        info(_Float32x);
#endif

        info(size_t);
        info(ssize_t);
        info(time_t);
        info(usec_t);
#ifdef __GLIBC__
        info(__time_t);
#endif // ifdef __GLIBC__
        info(pid_t);
        info(uid_t);
        info(gid_t);

        info(enum Enum);
        info(enum BigEnum);
        info(enum BigEnum2);
        assert_cc(sizeof(enum BigEnum2) == 8);
        printf("big_enum2_pos → %zu\n", sizeof(big_enum2_pos));
        printf("big_enum2_neg → %zu\n", sizeof(big_enum2_neg));

        return 0;
}
