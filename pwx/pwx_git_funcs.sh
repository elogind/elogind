#!/bin/bash
#
# (c) 2013 - 2016 PrydeWorX
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
#  0.1.0     2013-06-01  sed, PrydeWorX  First common function for
#                                         pwx_git_getter and pwx_git_applier.
#  0.2.0     2016-11-18  sed, PrydeWorX  Added exchanges array and
#                                         add_exchange() function.
#

# Make sure a known output is generated:
export LC_ALL="C"


# Error out if an unset variable or function is used
set -u


# Save IFS so it can be restored if someone ctrl+c's while IFS is patched
declare PWX_OLD_IFS="$IFS"


# @VARIABLE: PWX_EXCHANGES
# @DESCRIPTION:
# Array of <t:s> strings used by pwx_git_getter to substitute path names when
# searching for files, and after generating commit patches.
# This is put here as there might be a need for it in pwx_git_applier, too.
declare -a PWX_EXCHANGES=()


# @VARIABLE: PWX_TEMP_FILES
# @DESCRIPTION:
# Array of temporary files that get deleted when calling cleanup().
declare -a PWX_TEMP_FILES=()


# @VARIABLE: PWX_ERR_LOG
# @DESCRIPTION:
# Path to a file that gets dumped on the console when calling die().
declare PWX_ERR_LOG=""


# @VARIABLE: HERE
# @DESCRIPTION:
# Simple full path of the current working directory.
declare HERE="$(pwd -P)"


# @VARIABLE: PWX_COMMIT_FILE
# @DESCRIPTION
# The name of the file that stores mutual commit information.
declare PWX_COMMIT_FILE="${PROGDIR}/pwx_last_mutual_commits.txt"


# @VARIABLE: PWX_IS_PUSHD
# @DESCRIPTION
# If set to 1, the die() function knows that it has to perform a popd.
declare PWX_IS_PUSHD=0


# FUNCTION: add_exchange
# @USAGE: <string>
# @DESCRIPTION:
# Add <string> to the PWX_EXCHANGES array
add_exchange() {
	local f x present
	for f in "$@" ; do
		present=0
		for x in "${PWX_EXCHANGES[@]}" ; do
			if [[ "x$f" = "x$x" ]]; then
				present=1
				break
			fi
		done
		if [[ 0 -eq $present ]]; then
			PWX_EXCHANGES[${#PWX_EXCHANGES[@]}]="$f"
		fi
	done
}


# FUNCTION: add_temp
# @USAGE: <file [...]>
# @DESCRIPTION:
# Add 1..n files to the PWX_TEMP_FILES array
add_temp() {
	local f x present
	for f in "$@" ; do
		present=0
		for x in "${PWX_TEMP_FILES[@]}" ; do
			if [[ "x$f" = "x$x" ]]; then
				present=1
				break
			fi
		done
		if [[ 0 -eq $present ]]; then
			PWX_TEMP_FILES[${#PWX_TEMP_FILES[@]}]="$f"
		fi
	done
}


# FUNCTION: cleanup
# @USAGE:
# @DESCRIPTION:
# Remove all (present) files in the PWX_TEMP_FILES array
cleanup() {
	for f in "${PWX_TEMP_FILES[@]}" ; do
		[[ -f "$f" ]] && rm -f $f
	done
}


# @FUNCTION: die
# @USAGE: [message]
# @DESCRIPTION:
# print out [message], cat error log if not empty,
# delete all files listed in PWX_TEMP_FILES and exit
# with error code 255.
# If we are in pushd state, popd back before exiting
die() {
	local f msg="$@"

	# Reset IFS
	export IFS="$PWX_OLD_IFS"

	# Get the message out
	if [[ -n "$msg" ]]; then
		echo -e "\n$msg\n"
	else
		echo -e "\nDIE\n"
	fi
	
	if [[ -n "$PWX_ERR_LOG" ]] \
	&& [[ 0 -lt $(wc -l $PWX_ERR_LOG | cut -d ' ' -f 1) ]]; then
		echo -e "Last error log:\n========"
		cat $PWX_ERR_LOG
		echo "========"
	fi
	
	cleanup
	
	if [[ 0 -ne $PWX_IS_PUSHD ]]; then
		popd 1>/dev/null 2>&1
	fi
	
	exit 255
}


trap die SIGINT SIGKILL SIGTERM

return 0
