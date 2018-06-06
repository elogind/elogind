#!/bin/bash

src_files=( $(find -type f -name '*.c') )
hdr_files=( $(find -type f -name '*.h') )

# Is this a meson build?
isMeson=0
if [[ -f meson.build ]]; then
	isMeson=1
fi

for hdr in $(find -type f -name '*.h') ; do
	h_dir="$(basename $(dirname $hdr))"
	h_file="$(basename $hdr .h)"

	# Is it listed in the Makefile.am or meson.build?
	if [[ 1 -eq $isMeson ]]; then
		if [[ 0 -lt $(grep -c "$h_dir/${h_file}.c" meson.build) ]] || \
		   [[ 0 -lt $(grep -c "$h_dir/${h_file}.h" meson.build) ]]; then
			# It is needed.
			continue 1
		fi
	else
		if [[ 0 -lt $(grep -c "$h_dir/${h_file}.c" Makefile.am) ]] || \
		   [[ 0 -lt $(grep -c "$h_dir/${h_file}.h" Makefile.am) ]]; then
			# It is needed.
			continue 1
		fi
	fi

	# Is it included in any source files?
	for src in "${src_files[@]}" ; do
		is_inc=$(grep -P '^#include' $src | grep -c "${h_file}.h")
		if [[ 0 -lt $is_inc ]]; then
			# it is indeed
			continue 2
		fi
	done

	# Is it included in any header files?
	for src in "${hdr_files[@]}" ; do

		# If we already removed $src, skip it
		[ -f "$src" ] || continue 1

		is_inc=$(grep '#include' $src | grep -c "${h_file}.h")
		if [[ 0 -lt $is_inc ]]; then
			# it is indeed
			continue 2
		fi
	done

	# As it was not included, remove it.
	git rm $hdr
done
