#!/bin/sh
# SPDX-License-Identifier: LGPL-2.1+

case "$1" in

        recdiff)
                if [ "$2" = "" ] ; then
                        DIR="$PWD/.."
                else
                        DIR="$2"
                fi

                find $DIR -type f \( -name '*.c' -o -name '*.xml' \) -exec $0 diff \{\} \;
                ;;

        recpatch)
                if [ "$2" = "" ] ; then
                        DIR="$PWD/.."
                else
                        DIR="$2"
                fi

                find $DIR -type f \( -name '*.c' -o -name '*.xml' \) -exec $0 patch \{\} \;
                ;;

        diff)
                T=`mktemp`
                sed '/^$/N;/^\n$/D' < "$2" > "$T"
                diff -u "$2" "$T"
                rm -f "$T"
                ;;

        patch)
                sed -i '/^$/N;/^\n$/D' "$2"
                ;;

        *)
                echo "Expected recdiff|recpatch|diff|patch as verb." >&2
                ;;
esac
