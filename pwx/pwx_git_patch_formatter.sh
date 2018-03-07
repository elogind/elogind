#!/bin/bash
#
# (c) 2012 - 2017 PrydeWorX
#     Sven Eden, PrydeWorX - Bardowick, Germany
#     yamakuzure@users.sourceforge.net
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# History and Changelog:
#  Version   Date        Maintainer      Change(s)
#  0.0.1     2017-07-27  sed, PrydeWorX  First Design, forked off pwx_git_getter.sh
#

# Common functions
source pwx_git_funcs.sh

# Version, please keep this current
VERSION="0.0.1"

# Global values to be filled in:
SINCE_WHEN=""
OUTPUT="${HERE}/patches"
EXTRA_GIT_OPTS=""


# The options for the git format-patch command:
GIT_FP_OPTS="-1 -C --find-copies-harder -n"


# Options and the corresponding help text
OPT_SHORT=ho:
OPT_LONG=help,output:

HELP_TEXT="Usage: $0 [OPTIONS] <commit>

  Build formatted patches for all commits *after* <commit>
  up to HEAD.

OPTIONS:
  -h|--help            Show this help and exit.
  -o|--output <path> : Path to where to write the patches.
                       The default is to write into the
                       subdirectory 'patches' of the
                       current directory.
"


# =========================================
# === Use getopt (GNU enhanced version) ===
# =========================================

# Check the version first, so we do not run into trouble
getopt --test > /dev/null
if [[ $? -ne 4 ]]; then
	echo "ERROR: getopt is not the GNU enhanced version."
	exit 1
fi

# Store the output so we can check for errors.
OPT_PARSED=$(getopt --options $OPT_SHORT --longoptions $OPT_LONG --name "$0" -- "$@")

if [[ $? -ne 0 ]]; then
	# getopt has already complained about wrong arguments to stdout
	exit 2
fi

# Use eval with "$OPT_PARSED" to properly handle the quoting
eval set -- "$OPT_PARSED"

# --------------------
# --- Handle input ---
# --------------------
while true; do
	case "$1" in
		-h|--help)
			echo "$HELP_TEXT"
			exit 0
			;;
		-o|--output)
			OUTPUT="$2"
			shift 2
			;;
		--)
			shift
			break
			;;
		*)
			echo "Something went mysteriously wrong."
			exit 3
			;;
	esac
done

# At this point we must have <commit> left
if [[ $# -ne 1 ]]; then
    echo "$HELP_TEXT"
    exit 4
fi

# So these must be it:
SINCE_WHEN="$1"


# ==========================================================
# === The OUTPUT directory must exist and must be empty. ===
# ==========================================================
if [[ -n "$OUTPUT" ]]; then
	if [[ ! -d "$OUTPUT" ]]; then
		mkdir -p "$OUTPUT" || die "Can not create $OUTPUT [$?]"
	fi

	if [[ -n "$(ls "$OUTPUT"/????-*.patch 2>/dev/null)" ]]; then
		echo "ERROR: $OUTPUT already contains patches"
		exit 4
	fi
else
	echo "You have set the output directory to be"
	echo "an empty string. Where should I put the"
	echo "patches, then?"
	exit 5
fi


# --- Add an error log file to the list of temp files
PWX_ERR_LOG="/tmp/pwx_git_getter_$$.log"
add_temp "$PWX_ERR_LOG"
touch $PWX_ERR_LOG || die "Unable to create $PWX_ERR_LOG"


# =========================================
# === Step 1: Build the list of commits ===
# =========================================
echo -n "Building commit list..."

# lst_a is for the list of commits in reverse order
lst_a=/tmp/git_list_a_$$.lst
touch "$lst_a" || die "Can not create $lst_a"
truncate -s 0 $lst_a
add_temp "$lst_a"

git log ${SINCE_WHEN}..HEAD 2>/dev/null | \
	grep -P "^commit\s+" | \
	cut -d ' ' -f 2 | \
	tac > $lst_a

c_cnt=$(wc -l $lst_a | cut -d ' ' -f 1)
echo " $c_cnt commits found"


# ================================================================
# === Step 2: Now that we have a lst_b file with a list of all ===
# ===         relevant commits for all files found in the      ===
# ===         target, the commit patches can be build.         ===
# ================================================================
echo -n "Creating patches ..."

# To be able to apply the patches in the correct order, they need
# to be numbered. However, as git will only create one patch at a
# time, we have to provide the numbering by ourselves.
n=0
for c in $(cut -d ' ' -f 2 $lst_a) ; do
	n=$((n+1))
	n_str="$(printf "%04d" $n)"
	git format-patch $GIT_FP_OPTS -o $OUTPUT --start-number=$n $c \
		1>/dev/null 2>$PWX_ERR_LOG || die "git format-patch failed on $c"
done
echo " done"


# ========================================
# === Cleanup : Remove temporary files ===
# ========================================
cleanup
echo "All finished"

