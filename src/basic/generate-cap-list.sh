#!/bin/sh
set -eu

#if 0 /// elogind needs musl_missing.sh, which is in shared.
# $1 -dM -include linux/capability.h -include "$2" -include "$3" - </dev/null | \
#     awk '/^#define[ \t]+CAP_[A-Z_]+[ \t]+/ { print $2; }' | \
#     grep -v CAP_LAST_CAP
#else // 0
$1 -dM -include linux/capability.h -I"$2"/src/shared -include "$3" -include "$4" - </dev/null | \
        awk '/^#define[ \t]+CAP_[A-Z_]+[ \t]+/ { print $2; }' | \
        grep -v CAP_LAST_CAP
#endif // 0
