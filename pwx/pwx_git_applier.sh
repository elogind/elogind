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
# History and ChangePWX_ERR_LOG:
#  Version   Date        Maintainer      Change(s)
#  0.0.1     2012-12-29  sed, PrydeWorX  First Design.
#  0.0.2     2013-01-08  sed, PrydeWorX  First working version.
#  0.1.0     2013-01-10  sed, PrydeWorX  Initial private release.
#  0.1.1     2013-06-01  sed, PrydeWorX  Use functions from pwx_git_funcs.sh.
#  0.2.0     2014-09-02  sed, PrydeWorX  Use GNU enhanced getopt for command
#                                         line parsing.
#  0.3.0     2016-11-18  sed, PrydeWorX  Added option -T|--theirs to force
#                                         merge errors to be resolved by
#                                         throwing away all local changes.
#  0.3.1     2017-03-22  sed, PrydeWorX  Show full part of a patch that is to be
#                                         deleted after manually fixing merge
#                                         conflicts.
#                                        Show tried command if we fail with
#                                        full rejects.
#  0.4.0     2017-04-24  sed, PrydeWorX  Remove some remote-is-right-automatisms
#  0.5.0     2017-07-04  sed, PrydeWorX  If normal processing fails, use
#                                        check_tree.pl to generate diffs for the
#                                        specific commit, and apply them after
#                                        letting the user have a look.

# Common functions
source pwx_git_funcs.sh

# Version, please keep this current
VERSION="0.5.0"


# Global values to be filled in:
PATCH_DIR="${HERE}/patches"
TAG_TO_USE=""


# Editor to edit individual patches when needed
PWX_EDIT=/usr/bin/kate


# The options for the git am command:
GIT_AM_OPTS="--committer-date-is-author-date"


# Options and the corresponding help text
OPT_SHORT=hi:T
OPT_LONG=help,input:,theirs

HELP_TEXT="Usage: $0 [OPTIONS] <source dir> <tag>

  Take all commit patches from the input directory and
  apply them to the local tree.
  They are assumed to be from tag <tag> of some source
  tree.

OPTIONS:
  -h|--help           Show this help and exit.
  -i|--input <path> : Path to where to patches are.
                      The default is to read from the
                      subdirectory 'patches' of the
                      current directory.
Notes:
  - When the script succeeds, it adds a line to the commit
  - file \"${PWX_COMMIT_FILE}\" of the form:
    <tag>-merged <last used commit>
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
		-i|--input)
			PATCH_DIR="$2"
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

# At this point we must have <source dir> and <tag> left
if [[ $# -ne 2 ]]; then
    echo "$HELP_TEXT"
    exit 4
fi

# So these must be it:
SOURCE_DIR="$1"
TAG_TO_USE="$2"

if [[ ! -d "$SOURCE_DIR" ]]; then
	echo "$SOURCE_DIR does not exist"
	echo
    echo "$HELP_TEXT"
    exit 5
fi


# ===========================================
# === The PATCH_DIR directory must exist. ===
# ===========================================
if [[ -n "$PATCH_DIR" ]]; then
	if [[ ! -d "$PATCH_DIR" ]]; then
		echo "ERROR: $PATCH_DIR does not exist"
		exit 4
	fi
else
	echo "You have set the patch directory to be"
	echo "an empty string. Where should I find the"
	echo "patches, then?"
	exit 5
fi


# =============================================================
# === We need two file lists.                               ===
# === A) The list of root files.                            ===
# ===    These have not been used to generate commit        ===
# ===    patches and must be ignored by 'git am'.           ===
# === B) The patches to work with.                          ===
# =============================================================
echo -n "Building file lists ..."

# --- File list a) Root files
declare -a root_files=( $(find ./ -mindepth 1 -maxdepth 1 -type f \
	-not -name '*~' -and \
	-not -name '*.diff' -and \
	-not -name '*.orig' -and \
	-not -name '*.bak'  -printf "%f ") )

# --- File list b) Patch files
# --- Here we might find patches that failed for single files. Those
# --- must not clutter the list, they wouldn't apply anyway.
declare -a patch_files=( $(find "$PATCH_DIR"/ -mindepth 1 -maxdepth 1 -type f \
	-name '????-*.patch' -and -not -name '*-failed_patch_for-*' \
	-printf "%f\n" | sort -n) )
echo " done"

# --- Add an error log file to the list of temp files
PWX_ERR_LOG="/tmp/pwx_git_applier_$$.log"
add_temp "$PWX_ERR_LOG"
touch $PWX_ERR_LOG || die "Unable to create $PWX_ERR_LOG"


# ===================================================
# === Build a basic exclude string to begin with. ===
# ===================================================
basic_excludes=""
for e in "${root_files[@]}" ; do
	if [[ "" = "$(echo -n "$basic_excludes" | \
			grep -P "^\sexclude=$e")" ]]; then
		basic_excludes+=" --exclude=$e"
	fi
done


# ============================================
# === Main loop over the found patch files ===
# ============================================
for p in "${patch_files[@]}" ; do

	# For further processing the number and the full path
	# are needed.
	pnum=${p%%-*}
	psrc="${PATCH_DIR}/${p}"

	# We start with normal 3-way-patching
	GIT_USE_TWP="-3 "


	# ====================================================
	# === Step 1) Reset the exclude list of root files ===
	# ====================================================
	excludes="$basic_excludes"


	# ==============================================
	# === Step 2) Start applying the patch as is ===
	# ==============================================
	echo -n "Applying $p ..."
	git am $GIT_USE_TWP$GIT_AM_OPTS$excludes < $psrc 1>/dev/null 2>$PWX_ERR_LOG
	res=$?
	echo " done [$res]"


	# ===========================================================
	# === Step 3) Look for reasons to not use 3-way patching  ===
	# ===         Symptom   : "could not build fake ancestor" ===
	# ===         Reason    : No common root can be built     ===
	# ===         Resolution: Do not use "-3" option          ===
	# ===========================================================
	if [[ 0 -ne $res ]] && \
	   [[ $(grep -c "could not build fake ancestor" $PWX_ERR_LOG) -gt 0 ]];
	   then
		echo -n "Trying again without 3-way-patching ..."
		GIT_USE_TWP=""
		git am --abort 1>/dev/null 2>&1
		git am $GIT_USE_TWP$GIT_AM_OPTS$excludes < $psrc 1>/dev/null 2>$PWX_ERR_LOG
		res=$?
		echo " done [$res]"
	fi


	# ====================================================================
	# === Step 4) Look for more files to exclude                       ===
	# ===         Symptom   : "error: <file>: does not exist in index" ===
	# ===         Reason    : The file to patch isn't there            ===
	# ===         Resolution: Exclude the offending file(s)            ===
	# ====================================================================
	if [[ 0 -ne $res ]] && \
	   [[ $(grep -c "does not exist in index" $PWX_ERR_LOG) -gt 0 ]];
	   then
		declare -a nff_files=( $( \
			grep "does not exist in index" $PWX_ERR_LOG | \
			cut -d ':' -f 2) )

		for nff in "${nff_files[@]}" ; do
			echo "Excluding $nff ..."
			excludes+=" --exclude=$nff"

			# A special an evil case is a rename copy of something non-existent.
			# git am then needs *two* excludes, one for the (non-existing)
			# source and one for the still not existing target.
			nff_tgt="$(grep -A 1 "copy from $nff" $psrc | \
				grep "copy to " | \
				cut -d ' ' -f 3)"
			if [[ "x" != "x$nff_tgt" ]]; then
				echo "Excluding $nff_tgt ..."
				excludes+=" --exclude=$nff_tgt"
			fi
		done

		echo -n "Trying again without non-existing files ..."
		git am --abort 1>/dev/null 2>&1
		git am $GIT_USE_TWP$GIT_AM_OPTS$excludes < $psrc 1>/dev/null 2>$PWX_ERR_LOG
		res=$?
		echo " done [$res]"
		unset nff_files
	fi


	# ====================================================================
	# === Step 5) If this still doesn't work, let check_tree.pl check  ===
	# ===         out the commit and build diffs for the touched files ===
	# ===         against the tree. Then let the user have a chance to ===
	# ===         decide what to apply before continuing.              ===
	# ====================================================================
	if [[ 0 -ne $res ]] && \
	   [[ $(grep -c "patch does not apply" $PWX_ERR_LOG) -gt 0 ]];
	   then
		res=0
		xCommit="$(head -n 1 $psrc | cut -d ' ' -f 2)"
		echo "git am failed to apply the patch automatically:"
		echo -e "\n--------"
		cat $PWX_ERR_LOG
		echo -e "--------\n"
		echo "Building patch diffs for commit $xCommit ..."

		# We need to know which files are relevant:
		xFiles=""
		xPatches=""
		for pF in $(grep "^diff \--git" $psrc | cut -d ' ' -f 3,4 | \
				while read a b ; do \
					echo -e "$a\n$b" | cut -d '/' -f 2- ; \
				done | sort -u) ; do
			xFiles+="$pF "
			xPatches+="patches/${pF//\//_}.patch "
		done

		# If we have our lists, do it:
		if [[ "x" != "x$xFiles" ]] && [[ "x" != "x$xPatches" ]]; then
			./check_tree.pl "$SOURCE_DIR" "$xCommit" $xFiles
			
			# Let's see which patches got built
			xResult=""
			for xP in $xPatches ; do
				echo -n "Checkin $xP : "
				if [[ -f "$xP" ]]; then
					echo "present"
					xResult+="$xP "
				else
					echo "missing"
				fi
			done
			
			# So, if no patches have been found, this whole thing
			# can be skipped.
			if [[ "x" = "x$xResult" ]]; then
				echo "No relevant patches found."
				echo -n "Skipping $psrc"
				git am --skip
			else
				# Okay, something to do found.
				echo "Please edit/delete the diffs as they are"
				echo "and then close $PWX_EDIT to continue"
				echo " ... to skip, just delete all patches"
				$PWX_EDIT $xResult 1>/dev/null 2>&1
				
				# Apply patch-by-patch and add to git.
				have_patch=0
				for xP in $xResult ; do
					while [[ -f "$xP" ]]; do
						have_patch=1
						echo "Applying $xP ..."
						patch -p 1 -i $xP
						if [[ 0 -ne $? ]]; then
							echo "Something is wrong with $xP"
							$PWX_EDIT $xP 1>/dev/null 2>&1
						else
							rm -f $xP
						fi
					done
				done
				
				if [[ 0 -eq $have_patch ]]; then
					echo "All patches deleted."
					git am --skip
				else
					git add man src
					
					git status
					echo "Patch: $psrc"
					echo -e -n "\nDoes this look in order to you? [Y/n]"
					read answer
					if [[ "xn" = "x$answer" ]]; then
						echo "Okay, then see what you can do"
						exit 0
					fi
					
					echo -n "Finishing $psrc ..."
					git am --continue 1>/dev/null 2>$PWX_ERR_LOG
					res=$?
					echo " done [$res]"
				fi
			fi
		else
			echo "ERROR: Could not get file list from $psrc"
			echo
			echo "You have to do this by hand. Sorry."
			exit 1
		fi
	fi


	# ===========================================
	# === Step 6) Exit if we couldn't help it ===
	# ===========================================
	if [[ $res -ne 0 ]]; then
		echo -e "\n ==> $psrc can not be applied"
		echo " ==> The command that failed was:"
		echo "git am $GIT_USE_TWP$GIT_AM_OPTS$excludes < $psrc"
		echo -e " ==> The Error message is:\n"
		cat $PWX_ERR_LOG
		echo -e "\nPlease try to apply the remaining parts by hand"
		echo "and then finish with 'git am --continue'"
		cleanup
		exit $res
	fi


	# ==================================
	# === Step 7) Clean up behind us ===
	# ==================================

	# The patch is applied or irrelevant, remove it.
	rm -f $psrc
	
	# Remove merge debris
	find -iname '*.orig' | xargs rm -f

	# Remove temp files
	cleanup
done

