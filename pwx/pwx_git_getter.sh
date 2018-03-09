#!/bin/bash
#
# (c) 2012 - 2016 PrydeWorX
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
#  0.0.1     2012-12-21  sed, PrydeWorX  First Design.
#  0.0.2     2012-12-29  sed, PrydeWorX  First working version.
#  0.0.3     2013-01-07  sed, PrydeWorX  Force LC_ALL=C
#  0.1.0     2013-01-09  sed, PrydeWorX  Initial private release.
#  0.1.1     2013-01-13  sed, PrydeWorX  Added logging and error handling.
#  0.1.2     2013-02-17  sed, PrydeWorX  Cleanup on trapped signals added.
#  0.1.3     2013-05-22  sed, PrydeWorX  Use function descriptions like in
#                                         Gentoo eclasses.
#  0.1.4     2013-06-01  sed, PrydeWorX  Put functions into pwx_git_funcs.sh
#                                         for shared use with pwx_git_applier.
#  0.2.0     2014-09-02  sed, PrydeWorX  Use GNU enhanced getopt for command
#                                         line parsing.
#  0.2.1     2015-03-07  sed, PrydeWorX  Make <source path> and <tag> mandatory
#                                         positional arguments.
#  0.3.0     2016-11-18  sed, PrydeWorX  Added -w|--exchange option for
#                                         word/path substitutions. (elogind)
#  0.3.1     2016-11-25  sed, PrydeWorX  Add generated *.<tag>[.diff] files to
#                                         the targets .gitignore.
#  0.3.2     2017-05-23  sed, PrydeWorX  It is not fatal if the source dir has
#                                         no 'master' branch.
#  0.3.3     2017-07-25  sed, PrydeWorX  Do not build root file diffs, use
#                                         include the workings of
#                                         ./get_build_file_diff.sh instead.
#  0.4.0     2017-08-08  sed, PrydeWorX  Include meson build files in normal
#                                         file list.

# Common functions
PROGDIR="$(readlink -f $(dirname $0))"
source ${PROGDIR}/pwx_git_funcs.sh

# Version, please keep this current
VERSION="0.4.0"

# Global values to be filled in:
SOURCE_TREE=""
TAG_TO_USE=""
LAST_MUTUAL_COMMIT=""
OUTPUT="${PROGDIR}/patches"
EXTRA_GIT_OPTS=""


# The options for the git format-patch command:
GIT_FP_OPTS="-1 -C --find-copies-harder -n"


# Options and the corresponding help text
OPT_SHORT=c:e:ho:
OPT_LONG=commit:,exchange:,help,output:

HELP_TEXT="Usage: $0 [OPTIONS] <source path> <tag>

  Reset the git tree in <source path> to the <tag>. Then
  search its history since the last mutual commit for any
  commit that touches at least one file in any subdirectory
  of the directory this script was called from.
  The root files, like Makefile.am or configure.ac, are not
  used by the search, but copied with the tag as a postfix
  if these files where changed. A diff file is created as
  well.

OPTIONS:
  -c|--commit <hash> : The mutual commit to use. If this
                       option is not used, the script looks
                       into \"${PWX_COMMIT_FILE}\"
                       and uses the commit noted for <tag>.
  -e|--exchange <t:s>: Exchanges t for s when searching for
                       commits and replaces s with t in the
                       formatted patches.
                       't' is the [t]arget string. (HERE)
                       's' is the [s]ource string. (THERE)
                       This option can be used more than
                       once. Substitutions are performed in
                       the same order as given on the
                       command line.
  -h|--help            Show this help and exit.
  -o|--output <path> : Path to where to write the patches.
                       The default is to write into the
                       subdirectory 'patches' of the
                       current directory.

Notes:
  - The source tree is not reset and kept in detached state
    after the script finishes.
  - When the script succeeds, it adds a line to the commit
  - file \"${PWX_COMMIT_FILE}\" of the form:
    <tag>-last <newest found commit>
    You can use that line for the next tag you wish to
    migrate.
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
		-c|--commit)
			LAST_MUTUAL_COMMIT="$2"
			shift 2
			;;
		-e|--exchange)
			add_exchange "$2"
			shift 2
			;;
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

# At this point we must have <source path> and <tag> left
if [[ $# -ne 2 ]]; then
    echo "$HELP_TEXT"
    exit 4
fi

# So these must be it:
SOURCE_TREE="$1"
TAG_TO_USE="$2"



# ======================================================
# === The directory to analyse must exist of course. ===
# ======================================================
THERE="$(readlink -f "$SOURCE_TREE")"
if [[ ! -d "$THERE" ]]; then
	echo "The directory \"$SOURCE_TREE\""
	if [[ -n "$THERE" ]] \
	&& [[ "x$THERE" != "x$SOURCE_TREE" ]]; then
		echo "          aka \"$THERE\""
	fi
	echo -e "could not be found"
	exit 1
fi


# ==============================================
# === It is futile to analyze this directory ===
# ==============================================
if [[ "x$HERE" = "x$THERE" ]]; then
	echo "No. Please use another directory."
	exit 1
fi


# =======================================================
# === If no last mutual commit was given, try to load ===
# === it from commit list.                            === 
# =======================================================
if [[ -z "$LAST_MUTUAL_COMMIT" ]]; then
	if [[ -f "${PWX_COMMIT_FILE}" ]]; then
		LAST_MUTUAL_COMMIT="$( \
			grep -s -P "^$TAG_TO_USE " $PWX_COMMIT_FILE 2>/dev/null | \
			cut -d ' ' -f 2)"
	else
		echo "ERROR: ${PWX_COMMIT_FILE} does not exist."
		echo "  Please provide a last mutual commit"
		echo "  to use."
		echo -e "\n$HELP_TEXT"
		exit 2
	fi

	# If the tag was not found in the commit file,
	# error out. Analysing the trees by this script is
	# almost guaranteed to go wrong.
	if [[ -z "$LAST_MUTUAL_COMMIT" ]] ; then
		echo "ERROR: $PWX_COMMIT_FILE does not contain"
		echo " \"$TAG_TO_USE\"  Please provide a last mutual"
		echo "  commit to use."
		echo -e "\n$HELP_TEXT"
		exit 3
	fi
fi
echo "Last mutual commit: $LAST_MUTUAL_COMMIT"


# ================================================================
# === The PWX_EXCHANGES array may consist of "t:s" strings which   ===
# === must be split. The first part is needed to rename paths  ===
# === for the source tree history search, the second to rename ===
# === those paths in the patch files back into what can be     ===
# === used here.                                               ===
# ================================================================
declare -a ex_src=()
declare -a ex_tgt=()
for x in "${!PWX_EXCHANGES[@]}" ; do
	exstr=${PWX_EXCHANGES[$x]}
	src=${exstr##*:}
	tgt=${exstr%%:*}
	if [[ "x$src" = "x$tgt" ]]; then
		echo "ERROR: The exchange string \"$exstr\" is invalid"
		exit 4
	fi
	# The reason we go by using two indexed arrays instead
	# of associative arrays is, that the output of
	# ${!ex_src[@]} and ${!ex_tgt[@]} would be sorted
	# alphanumerical. But to be able to do chained substitutions
	# as defined by the order on the command line, that sorting
	# is counterproductive.
	# An argument chain like:
	# --exchange foobar:baz --exchange bar:foo
	# would result in the wrong order for the target to source
	# substitutions, as 'bar' would be given before 'foobar'.
	ex_src[$x]="$src"
	ex_tgt[$x]="$tgt"
done


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


# =============================================================
# === We need two file lists.                               ===
# === A) The list of root files.                            ===
# ===    These are not analyzed beyond the question whether ===
# ===    they differ from our version or not. If they       ===
# ===    differ, we'll copy them with the used tag becoming ===
# ===    a postfix. They are further analyzed later.        ===
# === B) All files in the used subdirectories. We need all  ===
# ===    commits that change these files.                   ===
# =============================================================
echo -n "Building file lists ..."

# If this is a meson+ninja build, there might be a "build" subdirectory,
# that we surely do not want to get analyzed.
if [[ -d ./build ]]; then
	rm -rf ./build
fi

# --- File list a) Root files
declare -a root_files=( $(find ./ -mindepth 1 -maxdepth 1 -type f \
	-not -name '*~' -and \
	-not -name '*.diff' -and \
	-not -name '*.orig' -and \
	-not -name '*.bak'  -and \
	-not -name '*meson*' -printf "%f ") )

# --- File list b) Core files
declare -a core_files=( $(find ./ -mindepth 2 -type f \
	-not -name '*~' -and \
	-not -name '*.diff' -and \
	-not -name '*.orig' -and \
	-not -name '*.bak') )
echo " done"

# --- Add root meson files to the core list
core_files+=( $(find ./ -mindepth 1 -maxdepth 1 -type f \
	-not -name '*~' -and \
	-not -name '*.diff' -and \
	-not -name '*.orig' -and \
	-not -name '*.bak'  -and \
	     -name '*meson*') )

# --- Add an error log file to the list of temp files
PWX_ERR_LOG="/tmp/pwx_git_getter_$$.log"
add_temp "$PWX_ERR_LOG"
touch $PWX_ERR_LOG || die "Unable to create $PWX_ERR_LOG"


# ===============================================
# === Step 1: pushd into the source directory ===
# ===         and check out the tag to use.   ===
# ===============================================
pushd $THERE 1>/dev/null 2>$PWX_ERR_LOG || die "Can't pushd into \"$THERE\""
PWX_IS_PUSHD=1

# Reset to master first, to see whether this is clean
git checkout master 1>/dev/null 2>&1
git checkout $TAG_TO_USE 1>/dev/null 2>$PWX_ERR_LOG
res=$?
if [ 0 -ne $res ] ; then
	die "ERROR: tag \"$TAG_TO_USE\" is unknown [$res]"
fi


# ======================================
# === Step 2: Now that we are THERE, ===
# ===         copy the root files.   ===
# ======================================
echo -n "Copying root files that differ ..."

# Go through the files.
for f in "${root_files[@]}" ; do
	if [[ -f "$THERE/$f" ]] \
	&& [[ -n "$(diff -dqw "$HERE/$f" "$THERE/$f")" ]] ; then

		cp -uf "$THERE/$f" "$HERE/${f}.$TAG_TO_USE" 2>$PWX_ERR_LOG || \
			die "cp \"$THERE/$f\" \"$HERE/${f}.$TAG_TO_USE\" failed"
	fi
done

echo " done"


# =========================================
# === Step 3: Build the list of commits ===
# =========================================
echo -n "Building commit list..."

# lst_a is for the raw list of commits with time stamps
lst_a=/tmp/git_list_a_$$.lst
touch "$lst_a" || die "Can not create $lst_a"
truncate -s 0 $lst_a
add_temp "$lst_a"

# lst_b is for the ordered (by timestamp) and unified list
lst_b=/tmp/git_list_b_$$.lst
touch "$lst_b" || die "Can not create $lst_b"
truncate -s 0 $lst_b
add_temp "$lst_b"

for f in "${core_files[@]}" ; do
	# Before using that file to find log entries that match,
	# all target->source substitutions must be used.
	fsrc="$f"
	for i in "${!ex_tgt[@]}" ; do
		src="${ex_src[$i]//\//\\\/}"
		tgt="${ex_tgt[$i]//\//\\\/}"
		fsrc="${fsrc//$tgt/$src}"
	done

	# Now go looking what we've got
	git log ${LAST_MUTUAL_COMMIT}..HEAD $fsrc 2>/dev/null | \
		grep -P "^commit\s+" | \
		cut -d ' ' -f 2 >> $lst_a
	# Note: No PWX_ERR_LOG here. If we check a file, that ONLY
	#       exists in the target, git log will error out.
done
sort -u $lst_a > $lst_b
truncate -s 0 $lst_a

c_cnt=$(wc -l $lst_b | cut -d ' ' -f 1)
echo " $c_cnt commits found"


# ==============================================================
# === Step 4: Get a full list of all commits since the last  ===
# ===         mutual commit. These are tac'd and then used   ===
# ===         to build the final list of commits, now in the ===
# ===         correct order.                                 ===
# ==============================================================
echo -n "Ordering by commit time..."
for c in $(git log ${LAST_MUTUAL_COMMIT}..HEAD 2>/dev/null | \
		grep -P "^commit\s+" | \
		cut -d ' ' -f 2 | tac) ; do
	grep "$c" $lst_b >> $lst_a
done
echo " done"


# ================================================================
# === Step 5: Now that we have a lst_b file with a list of all ===
# ===         relevant commits for all files found in the      ===
# ===         target, the commit patches can be build.         ===
# ================================================================
echo -n "Creating patches ..."

# To be able to apply the patches in the correct order, they need
# to be numbered. However, as git will only create one patch at a
# time, we have to provide the numbering by ourselves.
n=0
LAST_COMMIT=""
for c in $(cut -d ' ' -f 2 $lst_a) ; do
	n=$((n+1))
	n_str="$(printf "%04d" $n)"
	git format-patch $GIT_FP_OPTS -o $OUTPUT --start-number=$n $c \
		1>/dev/null 2>$PWX_ERR_LOG || die "git format-patch failed on $c"

	# Again we have to apply the exchange substitutions, now in
	# reverse order. If we didn't do that, the patches would
	# try to modify files, that do not exist in the target.
	# Or worse, are different files.
	for i in "${!ex_src[@]}" ; do
		src="${ex_src[$i]//,/\\,}"
		tgt="${ex_tgt[$i]//,/\\,}"
		perl -p -i -e "s,$src,$tgt,g" $OUTPUT/${n_str}-*.patch
	done

	# Store commit so the last is known when the loop ends
	LAST_COMMIT="$c"
done
echo " done"


# =============================================
# === Step 6: Get the real root files diffs ===
# =============================================
echo -n "Building root file diffs ..."
git checkout ${LAST_MUTUAL_COMMIT} 1>/dev/null 2>$PWX_ERR_LOG
res=$?
if [ 0 -ne $res ] ; then
	die "ERROR: tag \"${LAST_MUTUAL_COMMIT}\" is unknown [$res]"
fi

# The work is done HERE
popd 1>/dev/null 2>&1
PWX_IS_PUSHD=0

# Now go through the files:
for t in "${root_files[@]}"; do
	f="${t}.${TAG_TO_USE}"
	if [[ ! -f "${f}" ]]; then
		continue
	fi
	
	case "$t" in
		autogen.sh|Makefile-man.am|README)
			rm -f "$f"
			;;
		CODING_STYLE|NEWS|TODO|.dir-locals.el|.mailmap|.vimrc|.ymc_extra_conf.py)
			mv "$f" "$t"
			;;
		.gitignore|configure.ac|Makefile.am|configure|Makefile)
			diff -dwu $THERE/$t "$f" | \
				sed -e "s,$THERE/$t,a/$t," \
				    -e "s,$f,b/$t," > ${t}.diff
			rm -f "${f}"
			;;
		*)
			echo "WHAT the hell should I do with \"$f\" -> \"$t\" ?"
			;;
	esac
done

# Get upstream back THERE
pushd $THERE 1>/dev/null 2>$PWX_ERR_LOG || die "Can't pushd into \"$THERE\""
PWX_IS_PUSHD=1
git checkout $TAG_TO_USE 1>/dev/null 2>&1


# ======================================================
# === Step 7: Make the last commit found to be known ===
# ======================================================
popd 1>/dev/null 2>&1
PWX_IS_PUSHD=0
if [[ -n "$LAST_COMMIT" ]]; then
	m="${TAG_TO_USE}-last"
	if [[ -f $PWX_COMMIT_FILE ]] \
	&& [[ 0 -lt $(grep -c "$m" $PWX_COMMIT_FILE) ]]; then
		echo "Substituting $m with $LAST_COMMIT"
		perl -p -i -e "s,^$m\s+\S+$,$m $LAST_COMMIT," $PWX_COMMIT_FILE
	else
		echo "Setting $m to $LAST_COMMIT"
		echo "$m $LAST_COMMIT" >> $PWX_COMMIT_FILE
	fi
fi


# ========================================
# === Cleanup : Remove temporary files ===
# ========================================
cleanup
echo "All finished"

