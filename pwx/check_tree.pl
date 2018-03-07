#!/usr/bin/perl -w

# ================================================================
# ===        ==> --------     HISTORY      -------- <==        ===
# ================================================================
#
# Version  Date        Maintainer      Changes, Additions, Fixes
# 0.0.1    2017-04-08  sed, PrydeWorX  First basic design
# ... a lot of undocumented and partly lost development ...
# 0.8.0    2018-02-23  sed, PrydeWorX  2nd gen rewritten due to loss of the original thanks to stupidity.
# 0.8.1    2018-03-05  sed, PrydeWorX  Fixed all outstanding issues and optimized rewriting of shell files.
# 0.8.2    2018-03-06  sed, PrydeWorX  Added checks for elogind_*() function call removals.
#
# ========================
# === Little TODO list ===
# ========================
# - Add masking for XML code. It is a nuisance that the XML templates in the
#   python tools and man sources always generate useless patches. Real patches
#   are in danger of getting overlooked.
# - Add handling of the *.sym files.
# - Detect when "mask includes undos" are moved into the elogind block by diff.
# - Renames must also include "elogind <=> systemd-login"
# - Find a masking solution for login/logind-gperf.gperf
#
use strict;
use warnings;
use Cwd qw(getcwd abs_path);
use File::Basename;
use File::Find;
use Readonly;

# ================================================================
# ===        ==> ------ Help Text and Version ----- <==        ===
# ================================================================
Readonly my $VERSION     => "0.8.1"; ## Please keep this current!
Readonly my $VERSMIN     => "-" x length($VERSION);
Readonly my $PROGDIR     => dirname($0);
Readonly my $PROGNAME    => basename($0);
Readonly my $WORKDIR     => getcwd();
Readonly my $USAGE_SHORT => "$PROGNAME <--help|[OPTIONS] <path to upstream tree>>";
Readonly my $USAGE_LONG  => qq#
elogind git tree checker V$VERSION
--------------------------$VERSMIN

    Check the current tree, from where this program is called, against an
    upstream tree for changes, and generate a patchset out of the differences,
    that does not interfere with elogind development markings.

Usage :
-------
  $USAGE_SHORT

  The path to the upstream tree should have the same layout as the place from
  where this program is called. It is best to call this program from the
  elogind root path and use a systemd upstream root path for comparison.

Options :
---------
    -c|--commit <refid> | Check out <refid> in the upstream tree.
    -f|--file   <path>  | Do not search recursively, check only <path>.
                        | For the check of multiple files, you can either
                        | specify -f multiple times, or concatenate paths
                        | with a comma, or mix both methods.
    -h|--help           | Print this help text and exit.
#;

# ================================================================
# ===        ==> -------- Global variables -------- <==        ===
# ================================================================

my $file_fmt        = ""; ## Format string built from the longest file name in generate_file_list().
my $have_wanted     = 0;  ## Helper for finding relevant files (see wanted())
my %hWanted         = (); ## Helper hash for finding relevant files (see wanted())
my $in_else_block   = 0;  ## Set to 1 if we switched from mask/unmask to 'else'.
my $in_mask_block   = 0;  ## Set to 1 if we entered an elogind mask block
my $in_insert_block = 0;  ## Set to 1 if we entered an elogind addition block
my $main_result     = 1;  ## Used for parse_args() only, as simple $result is local everywhere.
my @only_here       = (); ## List of files that do not exist in $upstream_path.
my $previous_commit = ""; ## Store current upstream state, so we can revert afterwards.
my $show_help       = 0;
my @source_files    = (); ## Final file list to process, generated in in generate_file_list().
my $upstream_path     = "";
my $wanted_commit   = "";
my @wanted_files    = (); ## User given file list (if any) to limit generate_file_list()

# ================================================================
# ===        ==> ------- MAIN DATA STRUCTURES ------ <==       ===
# ================================================================
my %hFile = (); ## Main data structure to manage a complete compare of two files. (See: build_hFile() )
                ## Note: %hFile is used globaly for each file that is processed.
                ## The structure is:
                ## ( count  : Number of hunks stored
                ##   hunks  : Arrayref with the Hunks, stored in %hHunk instances
                ##   output : Arrayref with the lines of the final patch
                ##   part   : local relative file path
                ##   patch  : PROGDIR/patches/<part>.patch (With / replaced by _ in <part>)
                ##   source : WORKDIR/<part>
                ##   target : UPSTREAM/<part>
                ## )
my $hHunk = {}; ## Secondary data structure to describe one diff hunk.            (See: build_hHunk() )
                ## Note: $hHunk is used globally for each Hunk that is processed and points to the
                ##       current $hFile{hunks}[] instance.
                ## The structure is:
                ## { count      : Number of lines in {lines}
                ##   lines      : list of the lines themselves,
                ##   idx        : Index of this hunk in %hFile{hunks}
                ##   src_start  : line number this hunk starts in the source file.
                ##   tgt_start  : line number this hunk becomes in the target file.
                ##   useful     : 1 if the hunk is transported, 0 if it is to be omitted.
                ## }
my %hIncs  = ();## Hash for remembered includes over different hunks.
                ## Note: Only counted in the first step, actions are only in the second step.
                ## The structure is:
                ## { include => {
                ##     applied : Set to 1 once check_includes() has applied any rules on it.
                ##     elogind = { hunkid : Hunk id ; Position where a "needed by elogin" include is
                ##                 lineid : Line id ; wanted to be removed by the patch.
                ##     }
                ##     insert  = { hunkid  :  Hunk id ; Position where a commented out include is
                ##                 lineid  :  Line id ; wanted to be removed by the patch.
                ##                 spliceme: Set to 1 if this insert is later to be spliced.
                ##                 sysinc  : Set to 1 if it is <include>, 0 otherwise.
                ##     }
                ##     remove  = { hunkid : Hunk id ; Position where a new include is wanted to be
                ##                 lineid : Line id ; added by the patch.
                ##                 sysinc : Set to 1 if it is <include>, 0 otherwise.
                ##     }
                ## } }
my @lFails = ();## List of failed hunks. These are worth noting down in an extra structure, as these
                ## mean that something is fishy about a file, like elogind mask starts within masks.
                ## The structure is:
                ## ( { hunk : the failed hunk for later printing
                ##     msg  : a message set by the function that failed
                ##     part : local relative file part
                ##   }
                ## )

# ================================================================
# ===        ==> --------  Function list   -------- <==        ===
# ================================================================

sub build_hFile;        ## Inititializes and fills %hFile.
sub build_hHunk;        ## Adds a new $hFile{hunks}[] instance.
sub build_output;       ## Writes $hFile{output} from all useful $hFile{hunks}.
sub check_blanks;       ## Check that useful blank line additions aren't misplaced.
sub check_comments;     ## Check comments we added for elogind specific information.
sub check_debug;        ## Check for debug constructs
sub check_func_removes; ## Check for attempts to remove elogind_*() special function calls.
sub check_includes;     ## performe necessary actions with the help of %hIncs (built by read_includes())
sub check_masks;        ## Check for elogind preprocessor masks
sub check_musl;         ## Check for musl_libc compatibility blocks
sub check_name_reverts; ## Check for attempts to revert 'elogind' to 'systemd'
sub checkout_upstream;  ## Checkout the given refid on $upstream_path
sub clean_hFile;        ## Completely clean up the current %hFile data structure.
sub diff_hFile;         ## Generates the diff and builds $hFile{hunks} if needed.
sub generate_file_list; ## Find all relevant files and store them in @wanted_files
sub get_hunk_head;      ## Generate the "@@ -xx,n +yy,m @@" hunk header line out of $hHunk.
sub hunk_failed;        ## Generates a new @lFails entry and terminates the progress line.
sub hunk_is_useful;     ## Prunes the hunk and checks whether it stil does anything
sub parse_args;         ## Parse ARGV for the options we support
sub prepare_shell;      ## Prepare shell (and meson) files for our processing
sub prune_hunk;         ## remove unneeded prefix and postfix lines.
sub read_includes;      ## map include changes
sub splice_includes;    ## Splice all includes that were marked for splicing
sub unprepare_shell;    ## Unprepare shell (and meson) files after our processing
sub wanted;             ## Callback function for File::Find

# ================================================================
# ===        ==> --------    Prechecks     -------- <==        ==
# ================================================================

$main_result = parse_args(@ARGV);
( (!$main_result)                 ## Note: Error or --help given, then exit.
	or ( $show_help and print "$USAGE_LONG" ) )
	and exit(!$main_result);
length($wanted_commit)
	and (checkout_upstream($wanted_commit) ## Note: Does nothing if $wanted_commit is already checked out.
		or exit 1);
generate_file_list or exit 1;     ## Note: @wanted_files is heeded.

# ================================================================
# ===        ==> -------- = MAIN PROGRAM = -------- <==        ===
# ================================================================

for my $file_part (@source_files) {
	printf("$file_fmt: ", $file_part);

	build_hFile($file_part) or next;
	diff_hFile or next;

	# Reset global state helpers
	$in_else_block   = 0;
	$in_mask_block   = 0;
	$in_insert_block = 0;

	# Empty the include manipulation hash
	%hIncs = ();

	# ---------------------------------------------------------------------
	# --- Go through all hunks and check them against our various rules ---
	# ---------------------------------------------------------------------
	for (my $pos = 0; $pos < $hFile{count}; ++$pos) {
		$hHunk = $hFile{hunks}[$pos]; ## Global shortcut

		# === 1) Check for elogind masks ==================================
		check_masks and hunk_is_useful and prune_hunk or next;

		# === 2) Check for musl_libc compatibility blocks =================
		check_musl and hunk_is_useful and prune_hunk or next;

		# === 3) Check for debug constructs ===============================
		check_debug and hunk_is_useful and prune_hunk or next;

		# === 4) Check for elogind extra comments and information =========
		check_comments and hunk_is_useful and prune_hunk or next;

		# === 5) Check for useful blank line additions ====================
		check_blanks and hunk_is_useful and prune_hunk or next;

		# === 6) Check for 'elogind' => 'systemd' reverts =================
		check_name_reverts and hunk_is_useful and prune_hunk or next;

		# === 7) Check for elogind_*() function removals ==================
		check_func_removes and hunk_is_useful and prune_hunk or next;

		#  ===> IMPORTANT: From here on no more pruning, lines must *NOT* change any more! <===

		# === 8) Analyze includes and note their appearance in $hIncs =====
		read_includes; ## Never fails, doesn't change anything.

	} ## End of first hunk loop

	# ---------------------------------------------------------------------
	# --- Make sure saved include data is sane                          ---
	# ---------------------------------------------------------------------
	for my $inc (keys %hIncs) {
		$hIncs{$inc}{applied} = 0;
		defined ($hIncs{$inc}{elogind}) or $hIncs{$inc}{elogind} = { hunkid => -1, lineid => -1 };
		defined ($hIncs{$inc}{insert})  or $hIncs{$inc}{insert}  = { hunkid => -1, lineid => -1, spliceme => 0, sysinc => 0 };
		defined ($hIncs{$inc}{remove})  or $hIncs{$inc}{remove}  = { hunkid => -1, lineid => -1, sysinc => 0 };
	}

	# ---------------------------------------------------------------------
	# --- Go through all hunks and apply remaining changes and checks   ---
	# ---------------------------------------------------------------------
	for (my $pos = 0; $pos < $hFile{count}; ++$pos) {
		$hHunk = $hFile{hunks}[$pos]; ## Global shortcut

		# (pre -> early out)
		hunk_is_useful or next;

		# === 1) Apply what we learned to changed includes ================
		check_includes and hunk_is_useful or next;

	} ## End of second hunk loop

	# ---------------------------------------------------------------------
	# --- Splice all include insertions that are marked for splicing    ---
	# ---------------------------------------------------------------------
	splice_includes;

	# ---------------------------------------------------------------------
	# --- Go through all hunks for a last prune and check               ---
	# ---------------------------------------------------------------------
	my $have_hunk = 0;
	for (my $pos = 0; $pos < $hFile{count}; ++$pos) {
		$hHunk = $hFile{hunks}[$pos]; ## Global shortcut

		# (pre -> early out)
		hunk_is_useful or next;

		prune_hunk and ++$have_hunk;
	}

	# If we have at least 1 useful hunk, create the output and tell the user what we've got.
	$have_hunk
		and build_output # (Always returns 1)
		and printf("%d Hunk%s\n", $have_hunk, $have_hunk > 1 ? "s" : "")
		 or print("clean\n");

	# Shell and meson files must be unprepared. See unprepare_shell()
	$hFile{source} =~ m/\.pwx$/ and unprepare_shell;

	# Now skip the writing if there are no hunks
	$have_hunk or next;

	# That's it, write the file and be done!
	if (open(my $fOut, ">", $hFile{patch})) {
		for my $line (@{$hFile{output}}) {
			print $fOut "$line\n";
		}
		close($fOut);
	} else {
		printf("ERROR: %s could not be opened for writing!\n%s\n",
			$hFile{patch}, $!);
		die("Please fix this first!");
	}
} ## End of main loop

# -------------------------------------------------------------------------
# --- Print out the list of files that only exist here and not upstream ---
# -------------------------------------------------------------------------
if (scalar @only_here) {
	my $count = scalar @only_here;
	my $fmt   = sprintf("%%d %d: %%s\n", length("$count"));

	printf("\n%d file%s only found in $WORKDIR:\n", $count, $count > 1 ? "s": "");

	for (my $i = 0; $i < $count; ++$i) {
		printf($fmt, $i + 1, $only_here[$i]);
	}
}

# -------------------------------------------------------------------------
# --- Print out the list of files that only exist here and not upstream ---
# -------------------------------------------------------------------------
if (scalar @lFails) {
	my $count = scalar @lFails;

	printf("\n%d file%s %s have at least one fishy hunk:\n", $count,
	       $count > 1 ? "s" : "", $count > 1 ? "have" : "has");

	for (my $i = 0; $i < $count; ++$i) {
		print "=== $lFails[$i]{part} ===\n";
		print " => $lFails[$i]{msg} <=\n";
		print "---------------------------\n";
		print "$_\n" foreach(@{$lFails[$i]{hunk}});
	}
}

# ===========================
# === END OF MAIN PROGRAM ===
# ===========================

# ================================================================
# ===        ==> --------     Cleanup      -------- <==        ===
# ================================================================

length($previous_commit) and checkout_upstream($previous_commit);

# ================================================================
# ===        ==> ---- Function Implementations ---- <==        ===
# ================================================================

# -----------------------------------------------------------------------
# --- Inititializes and fills %hFile. Old values are discarded.       ---
# --- Adds files, that do not exist upstream, to @only_here.          ---
# --- Returns 1 on success, 0 otherwise.                              ---
# -----------------------------------------------------------------------
sub build_hFile {
	my ($part) = @_;

	defined($part) and length($part) or print("ERROR\n") and die("build_hfile: part is empty ???");

	# We only prefixed './' to unify things. Now it is no longer needed:
	$part =~ s,^\./,,;

	# Pre: erase current $hFile, as that is what is expected.
	clean_hFile();

	# Check the target file
	my $tgt = "$upstream_path/$part";
	$tgt =~ s/elogind/systemd/g;
	$tgt =~ s/\.pwx$//;
	-f $tgt or push(@only_here, $part)
		and print "only here\n"
		and return 0;

	# Build the patch name
	my $patch = $part;
	$patch =~ s/\//_/g;

	# Build the central data structure.
	%hFile = (
		count  => 0,
		hunks  => [ ],
		output => [ ],
		part   => "$part",
		patch  => "$PROGDIR/patches/${patch}.patch",
		source => "$WORKDIR/$part",
		target => "$tgt"
	);

	return 1;
}

# -----------------------------------------------------------------------
# --- Build a new $hHunk instance and add it to $hFile{hunks}         ---
# -----------------------------------------------------------------------
sub build_hHunk {
	my ($head, @lHunk) = @_;
	my $pos = $hFile{count}++;

	# The first line must be the hunk positional and size data.
	# Example: @@ -136,6 +136,8 @@
	# That is @@ -<source line>,<source length> +<target line>,<target length> @@
	if ( $head =~ m/^@@ -(\d+),\d+ \+(\d+),\d+ @@/ ) {
		%{$hFile{hunks}[$pos]} = (
			count      => 0,
			idx        => $pos,
			offset     => 0,
			src_start  => $1,
			tgt_start  => $2,
			useful     => 1
		);

		# We need to chomp the lines:
		for my $line (@lHunk) {
			defined($line) or next;
			chomp $line;
			push @{$hFile{hunks}[$pos]{lines}}, $line;
			$hFile{hunks}[$pos]{count}++;
		}
		return 1;
	}

	print "Illegal hunk no $hFile{count}\n(Head: \"$head\")\n";

	return 0;
}

# -----------------------------------------------------------------------
# --- Writes $hFile{output} from all useful $hFile{hunks}.            ---
# --- Important: No more checks, just do it!                          ---
# -----------------------------------------------------------------------
sub build_output {

	my $offset = 0; ## Count building up target offsets

	for (my $pos = 0; $pos < $hFile{count}; ++$pos) {
		$hHunk = $hFile{hunks}[$pos]; ## Global shortcut
		$hHunk->{useful} or next;     ## And the skip of the useless.

		# --- Add the header line -----------------
		# -----------------------------------------
		push(@{$hFile{output}}, get_hunk_head(\$offset));

		# --- Add the hunk lines ------------------
		# -----------------------------------------
		for my $line (@{$hHunk->{lines}}) {
			push(@{$hFile{output}}, $line);
		}
	} ## End of walking the hunks

	return 1;
}

# -----------------------------------------------------------------------
# --- Check that useful blank line additions aren't misplaced.        ---
# ---- Note: Sometimes the masks aren't placed correctly, and the diff---
# ----       wants to add a missing blank line. As it tried to remove ---
# ----       our mask first, it'll be added after. That's fine for    ---
# ----       #endif, but not for #if 0.                               ---
# -----------------------------------------------------------------------
sub check_blanks {

	# Early exits:
	defined($hHunk) or return 0;
	$hHunk->{useful} or return 0;

	for (my $i = 0; $i < $hHunk->{count}; ++$i) {
		my $line = \$hHunk->{lines}[$i]; ## Shortcut

		if ( ($$line =~ m/^\+\s*$/)
		  && ($i > 0)
		  && ($hHunk->{lines}[$i-1] =~ m/^[ -+]#if\s+[01].*elogind/) ) {
			# Simply swap the lines
			my $tmp = $$line;
			$$line = $hHunk->{lines}[$i-1];
			$hHunk->{lines}[$i-1] = $tmp;
			next;
		}
	}

	return 1;
}

# -----------------------------------------------------------------------
# --- Check comments we added for elogind specific information.       ---
# --- These are all comments, and can be both single and multi line.  ---
# -----------------------------------------------------------------------
sub check_comments {

	# Early exits:
	defined($hHunk) or return 0;
	$hHunk->{useful} or return 0;

	my $in_comment_block = 0;

	for (my $i = 0; $i < $hHunk->{count}; ++$i) {
		my $line = \$hHunk->{lines}[$i]; ## Shortcut

		# Check for comment block start
		# -----------------------------
		if ($$line =~ m,^-\s*(/[*]+|/[/]+).*elogind,) {

			# Sanity check:
			$in_comment_block
				and return hunk_failed("check_comments: Comment block start found in comment block!");

			substr($$line, 0, 1) = " ";

			# Only start the comment block if this is really a multiline comment
			(!($$line =~ m,\*/[^/]*$,) )
				and $in_comment_block = 1;

			next;
		}

		# Check for comment block end
		# -----------------------------
		if ($in_comment_block && ($$line =~ m,^-.*\*/\s*$,)) {
			substr($$line, 0, 1) = " ";
			$in_comment_block = 0;
			next;
		}

		# Check for comment block line
		# -----------------------------
		if ($in_comment_block && ($$line =~ m,^-,)) {
			# Note: We do not check for anything else, as empty lines must be allowed.
			substr($$line, 0, 1) = " ";
			next;
		}

		# If none of the above applied, the comment block is over.
		$in_comment_block = 0;
	}

	return 1;
}

# -----------------------------------------------------------------------
# --- Check for debug constructs                                      ---
# --- Rules: ENABLE_DEBUG_ELOGIND must be taken like #if 1, *but*     ---
# ---        here an #else is valid and must stay fully.              ---
# ---        Further there might be multiline calls to                ---
# ---        log_debug_elogind() that must not be removed either.     ---
# -----------------------------------------------------------------------
sub check_debug {

	# Early exits:
	defined($hHunk) or return 0;
	$hHunk->{useful} or return 0;

	# Count non-elogind block #ifs. This is needed, so normal
	# #if/#else/#/endif constructs can be put inside both the
	# debug and the release block.
	my $regular_ifs   = 0;
	my $is_debug_func = 0; ## Special for log_debug_elogind()

	for (my $i = 0; $i < $hHunk->{count}; ++$i) {
		my $line = \$hHunk->{lines}[$i]; ## Shortcut

		# Entering a debug construct block
		# ---------------------------------------
		if ( $$line =~ m/^-#if.+ENABLE_DEBUG_ELOGIND/ ) {
			## Note: Here it is perfectly fine to be in an elogind mask or insert block.
			substr($$line, 0, 1) = " "; ## Remove '-'
			$in_insert_block++; ## Increase instead of setting this to 1.
			next;
		}

		# Count regular #if
		$$line =~ m/^-#if/ and $in_insert_block and ++$regular_ifs;

		# Switching to the release variant.
		# ---------------------------------------
		if ( ($$line =~ m/^-#else/ ) && $in_insert_block && !$regular_ifs) {
			substr($$line, 0, 1) = " "; ## Remove '-'
			$in_else_block++; ## Increase instead of setting this to 1.
			next;
		}

		# Ending a debug construct block
		# ---------------------------------------
		if ( $$line =~ m,^-#endif\s*///?.*ENABLE_DEBUG_, ) {
			(!$in_insert_block)
				and return hunk_failed("check_debug: #endif // ENABLE_DEBUG_* found outside any debug construct");
			substr($$line, 0, 1) = " "; ## Remove '-'
			$in_insert_block--; ## Decrease instead of setting to 0. This allows such
			$in_else_block--;   ## blocks to reside in regular elogind mask/insert blocks.
			next;
		}

		# End regular #if
		$$line =~ m/^-#endif/ and $in_insert_block and --$regular_ifs;

		# Check for log_debug_elogind()
		# ---------------------------------------
		if ($$line =~ m/^-.*log_debug_elogind\s*\(/ ) {
			substr($$line, 0, 1) = " "; ## Remove '-'
			$$line =~ m/\)\s*;/ or ++$is_debug_func;
			next;
		}

		# Remove '-' prefixes in all lines within the debug construct block
		# -------------------------------------------------------------------
		if ( ($$line =~ m,^-,) && ($in_insert_block || $is_debug_func) ) {
			substr($$line, 0, 1) = " "; ## Remove '-'
			# Note: Everything in *any* insert block must not be removed anyway.
		}

		# Check for the end of a multiline log_debug_elogind() call
		# ---------------------------------------------------------
		$is_debug_func and $$line =~ m/\)\s*;/ and --$is_debug_func;

	} ## End of looping lines

	return 1;
}

# -----------------------------------------------------------------------
# --- Check for attempts to remove elogind_*() special function calls. --
# --- We have som special functions, needed oly by elogind.           ---
# --- One of the most important ones is elogind_set_program_name(),   ---
# --- which has an important role in musl_libc compatibility.         ---
# --- These calls must not be removed of course.                      ---
# -----------------------------------------------------------------------
sub check_func_removes  {

	# Early exits:
	defined($hHunk) or return 1;
	$hHunk->{useful} or return 1;

	# Needed for multi-line calls
	my $is_func_call = 0;

	for (my $i = 0; $i < $hHunk->{count}; ++$i) {
		my $line = \$hHunk->{lines}[$i]; ## Shortcut

		# Check for elogind_*() call
		# -------------------------------------------------------------------
		if ($$line =~ m/^-.*elogind_\S+\s*\(/ ) {
			substr($$line, 0, 1) = " "; ## Remove '-'
			$$line =~ m/\)\s*;/ or ++$is_func_call;
			next;
		}

		# Remove '-' prefixes in all lines that belong to an elogind_*() call
		# -------------------------------------------------------------------
		if ( ($$line =~ m,^-,) && $is_func_call ) {
			substr($$line, 0, 1) = " "; ## Remove '-'
		}

		# Check for the end of a multiline elogind_*() call
		# -------------------------------------------------------------------
		$is_func_call and $$line =~ m/\)\s*;/ and --$is_func_call;
	}

	return 1;
}

# -----------------------------------------------------------------------
# --- Check hunk for include manipulations we must step in            ---
# --- This is basically read_include(), but this time we actually act ---
# --- on what we found.                                               ---
# --- Rules:                                                          ---
# --- 1) Removals of includes that we commented out:                  ---
# ---    a) If there is no insertion of this include, let it be       ---
# ---       removed, it seems to be obsolete.                         ---
# ---    b) If there is an insertion of the very same include in one  ---
# ---       of the surrounding lines, mark the insert for splicing    ---
# ---       and undo the removal.                                     ---
# ---    c) If there is an insertion somewhere else, let it be        ---
# ---       removed here, and handle the insertion:                   ---
# ---       - Outside a "needed by elogind" block: comment out the    ---
# ---         inserted include.                                       ---
# ---       - Inside a "needed by elogind" block: undo the removal    ---
# ---         there.                                                  ---
# --- 2) Insertions of new includes, where 1) does not apply:         ---
# ---    a) If the include is new, comment it out to force a later    ---
# ---       check. The compiler will tell us if it is needed.         ---
# ---    b) If the include is part of the "needed by elogind" block   ---
# ---       already, allow the removal there and accept the regular   ---
# ---       insertion here.                                           ---
# --- 3) Removals of includes in "needed by elogind" blocks:          ---
# ---    As 1) and 2) do not apply, simply undo any removal here.     ---
# -----------------------------------------------------------------------
sub check_includes {

	# Early exits:
	defined($hHunk) or return 1;
	$hHunk->{useful} or return 1;

	# We must know when "needed by elogind blocks" start
	my $in_elogind_block = 0;

	# The simple undo check will fail, if we do at least one at once.
	# Delay the undoing of the removals until after the hunk was checked.
	my %undos = ();

	for (my $i = 0; $i < $hHunk->{count}; ++$i) {
		my $line = \$hHunk->{lines}[$i]; ## Shortcut

		# === Ruleset 1 : Handling of removals of includes we commented out ===
		# =====================================================================
		if ( $$line =~ m,^-\s*//+\s*#include\s+[<"']([^>"']+)[>"'], ) {
			$hIncs{$1}{applied} and next; ## No double handling

			my $inc = $1;

			# Pre: Sanity check:
			defined($hIncs{$inc}{remove}{hunkid}) and $hIncs{$inc}{remove}{hunkid} > -1
			 or return hunk_failed("check_includes: Unrecorded removal found!");

			# a) Check for removals of obsolete includes.
			$hIncs{$inc}{elogind}{hunkid} > -1        ## If no insert was found, then the include was
				 or $hIncs{$inc}{insert}{hunkid} > -1 ## removed by systemd devs for good.
				 or ( ++$hIncs{$inc}{applied} and next);

			# b) Check for a simple undo of our commenting out
			if ( ($hIncs{$inc}{insert}{hunkid} == $hIncs{$inc}{remove}{hunkid} )
			  && ($hIncs{$inc}{insert}{sysinc} == $hIncs{$inc}{remove}{sysinc} ) ) {
				my $ins_diff  = $hIncs{$inc}{insert}{lineid} - $hIncs{$inc}{remove}{lineid};
				my $all_same  = 1;
				my $direction = $ins_diff > 0 ? 1 : -1;

				# Let's see whether there are undos between this and its addition
				# in the same order, meaning there has been no resorting.
				for (my $j = $direction; $all_same && (abs($j) < abs($ins_diff)); $j += $direction) {
					$all_same = 0;

					if ( ( $hHunk->{lines}[$i+$j] =~ m,^-\s*//+\s*#include\s+[<"']([^>"']+)[>"'], )
					  || ( $hHunk->{lines}[$i+$j] =~ m,^\+\s*#include\s+[<"']([^>"']+)[>"'], ) ) {

						$hIncs{$1}{insert}{hunkid} == $hIncs{$1}{remove}{hunkid}
							and $ins_diff == ($hIncs{$1}{insert}{lineid} - $hIncs{$1}{remove}{lineid})
							and $hIncs{$inc}{insert}{sysinc} == $hIncs{$inc}{remove}{sysinc}
							and $all_same = 1;
					}
				}
				if ($all_same) {
					# The insertion is right before or after the removal. That's pointless.
					$undos{$i} = 1;
					$undos{$hIncs{$inc}{insert}{lineid}} = 1;
					$hIncs{$inc}{applied}  = 1;
					$hIncs{$inc}{insert}{spliceme} = 1;
					next;
				}
			} ## end of insert and remove being in the same hunk

			# c) Move somewhere else, or change include type. Can't be anything else here.
			if ($hIncs{$inc}{elogind}{hunkid} > -1) {
				# Just undo the removal of the elogind insert.
				my $hId = $hIncs{$inc}{elogind}{hunkid};
				my $lId = $hIncs{$inc}{elogind}{lineid};
				substr($hFile{hunks}[$hId]{lines}[$lId], 0, 1) = " ";
				$hIncs{$inc}{applied}  = 1;
			} else {
				# Just comment out the insert.
				my $hId = $hIncs{$inc}{insert}{hunkid};
				my $lId = $hIncs{$inc}{insert}{lineid};
				substr($hFile{hunks}[$hId]{lines}[$lId], 0, 1) = "+//";
				$hIncs{$inc}{applied}  = 1;
			}

			next;
		} ## End of ruleset 1

		# === Ruleset 2 : Handling of insertions, not handled by 1          ===
		# =====================================================================
		if ( $$line =~ m,^\+\s*#include\s+[<"']([^>"']+)[>"'], ) {
			$hIncs{$1}{applied} and next; ## No double handling

			# Pre: Sanity check:
			defined($hIncs{$1}{insert}{hunkid}) and $hIncs{$1}{insert}{hunkid} > -1
			 or return hunk_failed("check_includes: Unrecorded insertion found!");

			# Nicely enough we only have to check whether this is removed from
			# any elogind includes block or not.
			(-1 == $hIncs{$1}{elogind}{hunkid})
				and substr($$line, 0, 1) = "+//"; ## comment out for later check
			$hIncs{$1}{applied}  = 1;

			# That's it. Cool, eh?

			next;
		}

		# === Ruleset 3 : Handling of "needed by elogind" blocks            ===
		# =====================================================================
		if ( $in_elogind_block
		  && ($$line =~ m,^-\s*#include\s+[<"']([^>"']+)[>"'], ) ) {
			$hIncs{$1}{applied} and next; ## No double handling

			# Pre: Sanity check:
			defined($hIncs{$1}{elogind}{hunkid}) and $hIncs{$1}{elogind}{hunkid} > -1
			 or return hunk_failed("check_includes: Unrecorded elogind include found!");

			# As 1 and 2 do not apply, simply undo the removal.
			substr($$line, 0, 1) = " ";
			$hIncs{$1}{applied} = 1;

			next;
		}

		# === Other 1 : Look for "needed by elogind" block starts           ===
		# =====================================================================
		if ($$line =~ m,^[- ]\s*//+.*needed by elogind.*,i) {
			$in_elogind_block = 1;

			# Never remove the block start
			($$line =~ m,^-,) and substr($$line, 0, 1) = " ";

			# While we are here, we can see to it, that the additional empty
			# line above our marker does not get removed:
			($i > 0) and ($hHunk->{lines}[$i-1] =~ m/^-\s*$/)
			   and substr($hHunk->{lines}[$i-1], 0, 1) = " ";

			next;
		}

		# === Other 2 : elogind include blocks end, when the first not      ===
		# ===           removed line is found                               ===
		# =====================================================================
		$in_elogind_block and ($$line =~ m,^[ +]$,) and $in_elogind_block = 0;

		# === Other 3 : Undo all other removals in elogind include blocks   ===
		# =====================================================================
		$in_elogind_block and ($$line =~ m,^-,) and substr($$line, 0, 1) = " ";

		# Note: Although it looks like all rules are out the window here, all
		#       elogind includes that are handled above, end in a 'next', so
		#       those won't reach here. Actually 'Other 3' would be never
		#       reached with an #include line.

	} ## End of looping lines

	# Before we can leave, we have to neutralize the %undo lines:
	for my $lId (keys %undos) {
		substr($hHunk->{lines}[$lId], 0, 1) = " ";
	}

	return 1;
}

# -----------------------------------------------------------------------
# --- Check $hHunk for elogind preprocessor masks and additions       ---
# --- Rules:                                                          ---
# --- 1) If we need an alternative for an existing systemd code block,---
# ---    we do it like this:                                          ---
# ---      #if 0 /// <some comment with the word "elogind" in it>     ---
# ---        (... systemd code block, unaltered ...)                  ---
# ---        -> Any change in this block is okay.                     ---
# ---      #else                                                      ---
# ---        (... elogind alternative code ...)                       ---
# ---        -> Any change in this block is *NOT* okay.               ---
# ---      #endif // 0                                                ---
# --- 2) If we need an addition for elogind, we do it like this:      ---
# ---      #if 1 /// <some comment with the word "elogind" in it>     ---
# ---        (... elogind additional code ...)                        ---
# ---        -> Any change in this block is *NOT* okay.               ---
# ---      #endif // 1                                                ---
# -----------------------------------------------------------------------
sub check_masks {

	# Early exits:
	defined($hHunk) or return 0;
	$hHunk->{useful} or return 0;

	# Count non-elogind block #ifs. This is needed, so normal
	# #if/#else/#/endif constructs can be put inside elogind mask blocks.
	my $regular_ifs = 0;

	for (my $i = 0; $i < $hHunk->{count}; ++$i) {
		my $line = \$hHunk->{lines}[$i]; ## Shortcut

		# Entering an elogind mask
		# ---------------------------------------
		if ( $$line =~ m/^-#if\s+0.+elogind/ ) {
			$in_mask_block
				and return hunk_failed("check_masks: Mask start found while being in a mask block!");
			$in_insert_block
				and return hunk_failed("check_masks: Mask start found while being in an insert block!");
			substr($$line, 0, 1) = " "; ## Remove '-'
			$in_mask_block = 1;
			next;
		}

		# Entering an elogind insert
		# ---------------------------------------
		if ( $$line =~ m/^-#if\s+1.+elogind/ ) {
			$in_mask_block
				and return hunk_failed("check_masks: Insert start found while being in a mask block!");
			$in_insert_block
				and return hunk_failed("check_masks: Insert start found while being in an insert block!");
			substr($$line, 0, 1) = " "; ## Remove '-'
			$in_insert_block = 1;
			next;
		}

		# Count regular #if
		$$line =~ m/^-#if/ and $in_mask_block and ++$regular_ifs;

		# Switching from Mask to else.
		# Note: Inserts have no #else, they make no sense.
		# ---------------------------------------
		if ( ($$line =~ m/^-#else/ ) && $in_mask_block && !$regular_ifs) {
			substr($$line, 0, 1) = " "; ## Remove '-'
			$in_else_block = 1;
			next;
		}

		# Ending a Mask/Insert block
		# ---------------------------------------
		# Note: The regex supports "#endif /** 0 **/", too.
		if ( $$line =~ m,^-#endif\s*/(?:[*/]+)\s*([01]), ) {
			if (0 == $1) {
				(!$in_mask_block)
					and return hunk_failed("check_masks: #endif // 0 found outside any mask block");
				substr($$line, 0, 1) = " "; ## Remove '-'
				$in_mask_block = 0;
				$in_else_block = 0;
				next;
			}

			# As we explicitly ask for "#endif // [01]", only one is left.
			(!$in_insert_block)
				and return hunk_failed("check_masks: #endif // 1 found outside any insert block");
			substr($$line, 0, 1) = " "; ## Remove '-'
			$in_insert_block = 0;
			$in_else_block   = 0;
			next;
		}

		# End regular #if
		$$line =~ m/^-#endif/ and $in_mask_block and --$regular_ifs;

		# Remove '-' prefixes in all lines within insert and mask-else blocks
		# -------------------------------------------------------------------
		if ( ($$line =~ m,^-,)
		  && ( $in_insert_block || ($in_mask_block && $in_else_block) ) ) {
			substr($$line, 0, 1) = " "; ## Remove '-'
		}
	} ## End of looping lines

	return 1;
}

# -----------------------------------------------------------------------
# --- Check for musl_libc compatibility blocks                        ---
# --- Rules:                                                          ---
# --- For musl-libc compatibility, there are some                     ---
# ---   #ifdef __GLIBC__ (...) #else (...) #endif // __GLIBC__        ---
# --- helpers.                                                        ---
# --- These can also be "#if defined(__GLIBC__)"                      ---
# --- Note: We handle them like regular mask blocks, because the      ---
# ---       __GLIBC__ block is considered to be the original, while   ---
# ---       the musl_libc compat block is the #else block.            ---
# -----------------------------------------------------------------------
sub check_musl {

	# Early exits:
	defined($hHunk) or return 0;
	$hHunk->{useful} or return 0;

	# Count non-elogind block #ifs. This is needed, so normal
	# #if/#else/#/endif constructs can be put inside both the original
	# and the alternative block.
	my $regular_ifs = 0;

	for (my $i = 0; $i < $hHunk->{count}; ++$i) {
		my $line = \$hHunk->{lines}[$i]; ## Shortcut

		# Entering a __GLIBC__ block
		# ---------------------------------------
		if ( $$line =~ m/^-#if.+__GLIBC__/ ) {
			## Note: Here it is perfectly fine to be in an elogind mask block.
			substr($$line, 0, 1) = " "; ## Remove '-'
			$in_mask_block++; ## Increase instead of setting this to 1.
			next;
		}

		# Count regular #if
		$$line =~ m/^-#if/ and $in_mask_block and ++$regular_ifs;

		# Switching from __GLIBC__ to else - the alternative for musl_libc.
		# ---------------------------------------
		if ( ($$line =~ m/^-#else/ ) && $in_mask_block && !$regular_ifs) {
			substr($$line, 0, 1) = " "; ## Remove '-'
			$in_else_block++; ## Increase instead of setting this to 1.
			next;
		}

		# Ending a __GLBC__ block
		# ---------------------------------------
		if ( $$line =~ m,^-#endif\s*///?.*__GLIBC__, ) {
			(!$in_mask_block)
				and return hunk_failed("check_musl: #endif // __GLIBC__ found outside any __GLIBC__ block");
			substr($$line, 0, 1) = " "; ## Remove '-'
			$in_mask_block--; ## Decrease instead of setting to 0. This allows such
			$in_else_block--; ## blocks to reside in regular elogind mask/insert blocks.
			next;
		}

		# End regular #if
		$$line =~ m/^-#endif/ and $in_mask_block and --$regular_ifs;

		# Remove '-' prefixes in all lines within the musl (#else) blocks
		# -------------------------------------------------------------------
		if ( ($$line =~ m,^-,) && $in_mask_block && $in_else_block ) {
			substr($$line, 0, 1) = " "; ## Remove '-'
		}
	} ## End of looping lines

	return 1;
}

# -----------------------------------------------------------------------
# --- Check for attempts to revert 'elogind' to 'systemd'             ---
# --- Note: We only check for single line reverts like:               ---
# --- |-  // This is how elogind does it                              ---
# --- |+  // This is how systemd does it                              ---
# -----------------------------------------------------------------------
sub check_name_reverts {

	# Early exits:
	defined($hHunk) or return 0;
	$hHunk->{useful} or return 0;

	# Note down what is changed, so we can have inline updates
	my %hRemovals = ();

	for (my $i = 0; $i < $hHunk->{count}; ++$i) {
		my $line = \$hHunk->{lines}[$i]; ## Shortcut

		defined($$line) or return hunk_failed("check_name_reverts: Line " . ($i + 1) . "/$hHunk->{count} is undef?\n");

		# Note down removals
		# ---------------------------------
		if ($$line =~ m/^-\s*(.*elogind.*)\s*$/) {
			$hRemovals{$1}{line} = $i;
			next;
		}

		# Check Additions
		# ---------------------------------
		if ($$line =~ m/^\+\s*(.*systemd.*)\s*$/) {
			my $replace_text   = $1;
			my $our_text_long  = $replace_text;
			my $our_text_short = $our_text_long;
			$our_text_long  =~ s/systemd-logind/elogind/g;
			$our_text_short =~ s/systemd/elogind/g;

			# There is one speciality:
			# In some meson files, we need the variable "systemd_headers".
			# This refers to the systemd API headers that get installed,
			# and must therefore not be renamed to elogind_headers.
			$our_text_short =~ s/elogind_headers/systemd_headers/g;

			# If this is a simple switch, undo it:
			if ( defined($hRemovals{$our_text_short})
			  || defined($hRemovals{$our_text_long }) ) {
				defined($hRemovals{$our_text_short} )
					and substr($hHunk->{lines}[$hRemovals{$our_text_short}{line}], 0, 1) = " "
					 or substr($hHunk->{lines}[$hRemovals{$our_text_long }{line}], 0, 1) = " ";
				splice(@{$hHunk->{lines}}, $i--, 1);
				$hHunk->{count}--;
				next;
			}

			# Otherwise replace the addition with our text. ;-)
			$our_text_long eq $replace_text
				and $$line =~ s/^\+(\s*).*systemd.*(\s*)$/+${1}${our_text_short}${2}/
				 or $$line =~ s/^\+(\s*).*systemd.*(\s*)$/+${1}${our_text_long }${2}/;
		}
	}

	return 1;
}

# -----------------------------------------------------------------------
# --- Checkout the given refid on $upstream_path                      ---
# --- Returns 1 on success, 0 otherwise.                              ---
# -----------------------------------------------------------------------
sub checkout_upstream {
	my ($commit)   = @_;
	my $errmsg     = "";
	my $new_commit = "";

	# It is completely in order to not wanting to checkout a specific commit.
	defined($commit) and length($commit) or return 1;

	# Save the previous commit
	$previous_commit = qx(cd $upstream_path ; git rev-parse --short HEAD 2>&1);
	if ($?) {
		print "ERROR: Couldn't rev-parse $upstream_path HEAD\n";
		print "Exit Code : " . ($? >> 8) . "\n";
		print "$previous_commit\n";
		return 0;
	}

	# Get the shortened commit hash of $commit
	$new_commit = qx(cd $upstream_path ; git rev-parse --short "$commit" 2>&1);
	if ($?) {
		print "ERROR: Couldn't rev-parse $upstream_path \"$commit\"\n";
		print "Exit Code : " . ($? >> 8) . "\n";
		print "$new_commit\n";
		return 0;
	}

	# Now check it out, unless we are already there:
	if ($previous_commit ne $new_commit) {
		$errmsg = qx(cd $upstream_path ; git checkout "$new_commit" 2>&1);
		if ($?) {
			print "ERROR: Couldn't checkout \"new_commit\" in $upstream_path\n";
			print "Exit Code : " . ($? >> 8) . "\n";
			print "$errmsg\n";
			return 0;
		}
	}

	return 1;
}

# -----------------------------------------------------------------------
# --- Completely clean up the current %hFile data structure.          ---
# -----------------------------------------------------------------------
sub clean_hFile {
	defined($hFile{count}) or return 1;

	for (my $i = $hFile{count} - 1; $i > -1; --$i) {
		defined($hFile{hunks}[$i]) and undef(%{$hFile{hunks}[$i]});
	}

	$hFile{count}  = 0;
	$hFile{hunks}  = [ ];
	$hFile{output} = [ ];

	return 1;
}

# -----------------------------------------------------------------------
# --- Builds the diff between source and target file, and stores all  ---
# --- hunks in $hFile{hunks} - if any.                                ---
# --- Returns 1 on success and 0 if the files are the same.           ---
# -----------------------------------------------------------------------
sub diff_hFile {

	# Do they differ at all?
	`diff -qu "$hFile{source}" "$hFile{target}" 1>/dev/null 2>&1`;
	$? or print "same\n" and return 0;

	# Shell and meson files must be prepared. See prepare_meson()
	( $hFile{source} =~ m/meson/ or
	  $hFile{source} =~ m/\.in$/ or
	  $hFile{source} =~ m/\.pl$/ or
	  $hFile{source} =~ m/\.sh$/ ) and prepare_shell;

	# Let's have two shortcuts:
	my $src = $hFile{source};
	my $tgt = $hFile{target};
	my $prt = $hFile{part};

	# Now the diff can be built ...
	my @lDiff = `diff -u "$src" "$tgt"`;

	# ... the head of the output can be generated ...
	@{$hFile{output}} = splice(@lDiff, 0, 2);
	chomp $hFile{output}[0]; # These now have absolute paths, and source meson files have a
	chomp $hFile{output}[1]; # .pwx extensions. That is not what the result shall look like.
	$hFile{output}[0] =~ s,$src,a/$prt,; # But we have $hFile{part}, which is already the
	$hFile{output}[1] =~ s,$tgt,b/$prt,; # relative file name of the file we are processing.

	# ... and the raw hunks can be stored.
	for (my $line_no  = 1; $line_no < scalar @lDiff; ++$line_no) {
		("@@" eq substr($lDiff[$line_no], 0, 2))
			and (build_hHunk(splice(@lDiff, 0, $line_no)) or return 0)
			and $line_no = 0;
	}
	scalar @lDiff and build_hHunk(@lDiff);

	return 1;
}

# -----------------------------------------------------------------------
# --- Finds all relevant files and store them in @wanted_files        ---
# --- Returns 1 on success, 0 otherwise.                              ---
# -----------------------------------------------------------------------
sub generate_file_list {

	# Do some cleanup first. Just to be sure.
	`rm -rf build`;
	`find -iname '*.orig' -or -iname '*.bak' -or -iname '*.rej' -or -iname '*~' -or -iname '*.gc??' | xargs rm -f`;

	# Build wanted files hash
	while (my $want = shift @wanted_files) {
		$want =~ m,^\./, or $want = "./$want"; 
		$hWanted{$want} = 1;
		$want           =~ s/elogind/systemd/g;
		$hWanted{$want} = 1;
		$have_wanted    = 1;
	}

	# The idea now is, that we use File::Find to search for files
	# in all legal directories this program allows. Checking against
	# the built %hWanted ensures that a user provided list of files
	# is heeded.
	for my $xDir ("docs", "factory", "m4", "man", "po", "shell-completion", "src", "tools") {
		if ( -d "$xDir" ) {
			find(\&wanted, "$xDir");
		}
	}

	# Just to be sure...
	scalar @source_files
		 or print("ERROR: No source files found? Where the hell are we?\n")
		and return 0;

	# Get the maximum file length and build $file_fmt
	my $mlen = 0;
	for my $f (@source_files) {
		length($f) > $mlen and $mlen = length($f);
	}
	$file_fmt = sprintf("%%-%d%s", $mlen, "s");

	return 1;
}

# ------------------------------------------------------------------------
# --- Generate the "@@ -xx,n +yy,m @@" hunk header line out of $hHunk. ---
# --- Returns the generated string, with 0 values if $hHunk is undef.  ---
# --- IMPORTANT: This function does *NOT* prune $hHunk->{lines} !      ---
# ------------------------------------------------------------------------
sub get_hunk_head {
	my ($offset) = @_;

	my $src_len   = 0;
	my $tgt_len   = 0;
	my $lCount    = $hHunk->{count};
	my $src_start = $hHunk->{src_start};
	my $tgt_start = defined($offset) ? $src_start + $$offset : $hHunk->{tgt_start};

	for (my $i = 0; $i < $lCount; ++$i) {
		if ($hHunk->{lines}[$i] =~ m/^\+/ ) {
			$tgt_len++;
		} elsif ($hHunk->{lines}[$i] =~ m/^\-/ ) {
			$src_len++;
		} else {
			$src_len++;
			$tgt_len++;
		}
	}

	# If an offset reference was given, add back the size diff
	defined($offset)
		and $$offset += $tgt_len - $src_len;

	return sprintf("@@ -%d,%d +%d,%d @@", $src_start, $src_len, $tgt_start, $tgt_len);
}

# -----------------------------------------------------------------------
# --- Whenever a check finds an illegal situation, it has to call     ---
# --- this subroutine which terminates the progress line and creaes   ---
# --- an entry in @lFails.                                            ---
# --- Param: An error message, preferably with the name of the failed ---
# ---        check.                                                   ---
# --- Return: Always zero.                                            ---
# -----------------------------------------------------------------------
sub hunk_failed {
	my ($msg) = @_;
	my $num   = scalar @lFails;

	# Generate entry:
	push @lFails, {
		hunk => [ get_hunk_head ],
		msg  => $msg,
		part => $hFile{part}
	};

	# Add the hunk itself
	for my $line (@{$hHunk->{lines}}) {
		push @{$lFails[$num]{hunk}}, $line;
	}

	# And terminate the progress line:
	print "$msg\n";

	return 0;
}

# -----------------------------------------------------------------------
# --- Check the current $hHunk whether it still does anything.        ---
# --- While being at it, prune it to what a diff needs:               ---
# ---   3 lines before the first and 3 lines after the last change.   ---
# --- Returns 1 if at least one change was found, 0 otherwise.        ---
# -----------------------------------------------------------------------
sub hunk_is_useful {

	# Early exits:
	defined($hHunk) or return 0;
	$hHunk->{useful} or return 0;

	# Go through the lines and see whether we have any changes
	$hHunk->{useful} = 0;

	for (my $i = 0; $i < $hHunk->{count}; ++$i) {
		if ($hHunk->{lines}[$i] =~ m/^[-+]/ ) {
			$hHunk->{useful} = 1;
			return 1;
		}
	}

	return 0;
}

# -----------------------------------------------------------------------
# --- parse the given list for arguments.                             ---
# --- returns 1 on success, 0 otherwise.                              ---
# --- sets global $show_help to 1 if the long help should be printed. ---
# -----------------------------------------------------------------------
sub parse_args {
	my @args      = @_;
	my $result    = 1;

	for (my $i = 0; $i < @args; ++$i) {

		# Check for -c|--commit option
		# -------------------------------------------------------------------------------
		if ($args[$i] =~ m/^-(?:c|-commit)$/) {
			if ( ( ($i + 1) >= @args )
			  || ( $args[$i+1] =~ m,^[-/.], ) ) {
				print "ERROR: Option $args[$i] needs a refid as argument!\n\nUsage: $USAGE_SHORT\n";
				$result = 0;
				next;
			}
			$wanted_commit = $args[++$i];
		}

		# Check for -f|--file option
		# -------------------------------------------------------------------------------
		elsif ($args[$i] =~ m/^-(?:f|-file)$/) {
			if ( ( ($i + 1) >= @args )
			  || ( $args[$i+1] =~ m,^[-], ) ) {
				print "ERROR: Option $args[$i] needs a path as argument!\n\nUsage: $USAGE_SHORT\n";
				$result = 0;
				next;
			}
			push @wanted_files, split(/,/, $args[++$i]);
		}

		# Check for -h|--help option
		# -------------------------------------------------------------------------------
		elsif ($args[$i] =~ m/^-(?:h|-help)$/) {
			$show_help = 1;
		}

		# Check for unknown options:
		# -------------------------------------------------------------------------------
		elsif ($args[$i] =~ m/^-/) {
			print "ERROR: Unknown option \"$args[$1]\" encountered!\n\nUsage: $USAGE_SHORT\n";
			$result = 0;
		}

		# Everything else is considered to the path to upstream
		# -------------------------------------------------------------------------------
		else {
			# But only if it is not set, yet:
			if (length($upstream_path)) {
				print "ERROR: Superfluous upstream path \"$args[$1]\" found!\n\nUsage: $USAGE_SHORT\n";
				$result = 0;
				next;
			}
			if ( ! -d "$args[$i]") {
				print "ERROR: Upstream path \"$args[$i]\" does not exist!\n\nUsage: $USAGE_SHORT\n";
				$result = 0;
				next;
			}
			$upstream_path = $args[$i];
		}
	} ## End looping arguments

	# If we have no upstream path now, show short help.
	if ($result && !$show_help && !length($upstream_path)) {
		print "ERROR: Please provide a path to upstream!\n\nUsage: $USAGE_SHORT\n";
		$result = 0;
	}

	# If any of the wanted files do not exist, error out.
	if ($result && !$show_help && defined($wanted_files[0])) {
		foreach my $f (@wanted_files) {
			-f $f or print "ERROR: $f does not exist!\n" and $result = 0;
		}
	}

	return $result;
} ## parse_srgs() end

# -----------------------------------------------------------------------
# --- Prepare shell and meson files for our processing.               ---
# --- If this is a shell or meson file, we have to adapt it first:    ---
# --- To be able to use our patch building system, the files use the  ---
# --- same masking technology as the C files. But as these are not    ---
# --- handled by any preprocessor, it is necessary to comment out all ---
# --- masked blocks.                                                  ---
# --- If we do not do this, diff creates patches which move all       ---
# --- commented blocks behind the #endif and uncomment them.          ---
# -----------------------------------------------------------------------
sub prepare_shell {
	my $in   = $hFile{source};
	my $out  = $in . ".pwx";
	my @lIn  = ();
	my @lOut = ();

	# Leech the source file
	if (open(my $fIn, "<", $in)) {
		@lIn = <$fIn>;
		close($fIn);
	} else {
		die("$in can not be opened for reading! [$!]");
	}

	# Now prepare the output, line by line.
	my $is_block = 0;
	my $is_else  = 0;
	my $line_no  = 0;
	for my $line (@lIn) {

		chomp $line;
		++$line_no;

		if ($line =~ m,^[ ]?#if 0 /* .*elogind.*,) {
			if ($is_block) {
				print "ERROR: $in:$line_no : Mask start in mask!\n";
				die("Illegal file");
			}
			$is_block = 1;
		} elsif ($is_block && ($line =~ m,^[ ]?#else,) ) {
			$is_else = 1;
		} elsif ($line =~ m,^[ ]?#endif /* 0,) {
			if (!$is_block) {
				print "ERROR: $in:$line_no : Mask end outside mask!\n";
				die("Illegal file");
			}
			$is_block = 0;
			$is_else  = 0;
		} elsif ($is_block && !$is_else) {
			$line =~ s,^#\s?,,;
		}

		push @lOut, $line;
	}

	# Now write the outfile:
	if (open(my $fOut, ">", $out)) {
		for my $line (@lOut) {
			print $fOut "$line\n";
		}
		close($fOut);
	} else {
		die("$out can not be opened for writing! [$!]");
	}

	# The temporary file is our new source
	$hFile{source} = $out;

	return 1;
}

# -----------------------------------------------------------------------
# --- Remove unused prefix and postfix lines. Recalculates offsets.   ---
# -----------------------------------------------------------------------
sub prune_hunk {

	# Early exits:
	defined($hHunk) or return 0;
	$hHunk->{useful} or return 0;

	# Go through the lines and see what we've got.
	my $prefix  = 0;
	my $postfix = 0;
	my $changed = 0; ## Set to 1 once the first change was found.

	for (my $i = 0; $i < $hHunk->{count}; ++$i) {
		if ($hHunk->{lines}[$i] =~ m/^[-+]/ ) {
			$changed = 1;
			$postfix = 0;
		} else {
			$changed or ++$prefix;
			++$postfix;
		}
	}

	# Now let's prune it:
	if ($prefix > 3) {
		$prefix -= 3;
		splice(@{$hHunk->{lines}}, 0, $prefix);
		$hHunk->{src_start} += $prefix;
		$hHunk->{count}     -= $prefix;
	}
	if ($postfix > 3) {
		$postfix -= 3;
		splice(@{$hHunk->{lines}}, $hHunk->{count} - $postfix, $postfix);
		$hHunk->{count}     -= $postfix;
	}

	return 1;
}

# -----------------------------------------------------------------------
# --- Unprepare shell (and meson) files after our processing          ---
# --- In prepare_shell() we have commented in all content between our ---
# --- elogind markers, to help diff not to juggle our blocks around.  ---
# --- Now these blocks must be commented out again.                   ---
# --- We have an advantage and a disadvantage here. On one hand, we   ---
# --- can do the changes within $hFile{output}, as that is the final  ---
# --- patch. On the other hand, we do not know whether really all     ---
# --- lines in the source where commented out. The latter means, that ---
# --- if we blindly comment out all block lines, the resulting patch  ---
# --- may fail. We therefore write the temporary .pwx file back, and  ---
# --- and ensure that all lines are commented out.                    ---
# -----------------------------------------------------------------------
sub unprepare_shell {
	my $in   = $hFile{source};
	my $out  = substr($in, 0, -4);
	my @lIn  = ();
	my @lOut = ();

	# Leech the temporary file
	if (open(my $fIn, "<", $in)) {
		@lIn = <$fIn>;
		close($fIn);
	} else {
		die("$in can not be opened for reading! [$!]");
	}

	# Now prepare the output, line by line.
	my $is_block = 0;
	my $is_else  = 0;
	my $line_no  = 0;
	for my $line (@lIn) {

		chomp $line;
		++$line_no;

		if ($line =~ m,^[ ]?#if 0 /* .*elogind.*,) {
			if ($is_block) {
				print "ERROR: $in:$line_no : Mask start in mask!\n";
				die("Illegal file");
			}
			$is_block = 1;
		} elsif ($is_block && ($line =~ m,^[ ]?#else,) ) {
			$is_else = 1;
		} elsif ($line =~ m,^[ ]?#endif /* 0,) {
			if (!$is_block) {
				print "ERROR: $in:$line_no : Mask end outside mask!\n";
				die("Illegal file");
			}
			$is_block = 0;
			$is_else  = 0;
		} elsif ($is_block && !$is_else
		     && ("# " ne substr($line, 0, 2)) ) {
			$line = "# " . $line;
		}

		push @lOut, $line;
	}

	# Now write the outfile:
	if (open(my $fOut, ">", $out)) {
		for my $line (@lOut) {
			print $fOut "$line\n";
		}
		close($fOut);
	} else {
		die("$out can not be opened for writing! [$!]");
	}

	# Remove the temporary file
	unlink($in);

	# Now prepare the patch. It is like above, but with less checks.
	# We have to move out the lines first, and then write them back.
	@lIn = ();
	$is_block = 0;
	$is_else  = 0;
	@lIn = splice(@{$hFile{output}});
	for my $line (@lIn) {
		$line =~ m,^[ ]+#endif /* 0, and $is_block = 0;
		$is_block or $is_else = 0;
		$line =~ m,^[ ]+#if 0 /* .*elogind.*, and $is_block = 1;
		$is_block and $line =~ m,^[ ]?#else, and $is_else = 1;
		$is_block and (!$is_else) and (" # " ne substr($line, 0, 2))
			and $line = " #" . $line;

		push @{$hFile{output}}, $line;
	}

	# Now source is the written back original:
	$hFile{source} = $out;

	return 1;
}

# -----------------------------------------------------------------------
# --- Analyze the hunk and map all include changes                    ---
# --- The gathered knowledge is used in check_includes(), see there   ---
# --- for the rules applied.                                          ---
# -----------------------------------------------------------------------
sub read_includes {

	# Early exits:
	defined($hHunk) or return 1;
	$hHunk->{useful} or return 1;

	# We must know when "needed by elogind blocks" start
	my $in_elogind_block = 0;

	for (my $i = 0; $i < $hHunk->{count}; ++$i) {
		my $line = \$hHunk->{lines}[$i]; ## Shortcut

		# Note down removes of includes we commented out
		if ( $$line =~ m,^-\s*//+\s*#include\s+([<"'])([^>"']+)[>"'], ) {
			$hIncs{$2}{remove} = { hunkid => $hHunk->{idx}, lineid => $i, sysinc => $1 eq "<" };
			next;
		}

		# Note down inserts of possibly new includes we might want commented out
		if ( $$line =~ m,^\+\s*#include\s+([<"'])([^>"']+)[>"'], ) {
			$hIncs{$2}{insert} = { hunkid => $hHunk->{idx}, lineid => $i, spliceme => 0, sysinc => $1 eq "<" };
			next;
		}

		# Note down removals of includes we explicitly added for elogind
		if ( $in_elogind_block
		  && ($$line =~ m,^-\s*#include\s+([<"'])([^>"']+)[>"'], ) ) {
			$hIncs{$2}{elogind} = { hunkid => $hHunk->{idx}, lineid => $i };
			next;
		}

		# elogind include blocks are started by a comment featuring "needed by elogind"
		if ($$line =~ m,^-\s*//+.*needed by elogind.*,i) {
			$in_elogind_block = 1;
			next;
		}

		# elogind include blocks end, when the first not removed line is found
		$in_elogind_block and ($$line =~ m,^[ +]$,) and $in_elogind_block = 0;
	}

	return 1;
}

# -----------------------------------------------------------------------
# --- Splice all includes that were marked for splicing.              ---
# --- This is not as easy as it seems. It can be, that if we just go  ---
# --- through the %hIncs keys, that we splice one include that is     ---
# --- before another. That leads to the wrong line to be spliced, or  ---
# --- worse, the splicing being attempted out of bounds.              ---
# -----------------------------------------------------------------------
sub splice_includes {

	# First build a tree of the includes to splice:
	my %incMap = ();
	for my $inc (keys %hIncs) {
		if ($hIncs{$inc}{insert}{spliceme}) {
			my $hId = $hIncs{$inc}{insert}{hunkid};
			my $lId = $hIncs{$inc}{insert}{lineid};

			# Sanity checks:
			$hId > -1 or print "splice_includes : Inc $inc has Hunk Id -1!\n" and next;
			if ( -1 == $lId ) {
				$hHunk = $hFile{hunks}[$hId];
				hunk_failed("splice_includes: $inc has line id -1!");
				next;
			}
			if ( $lId >= $hFile{hunks}[$hId]{count} ) {
				$hHunk = $hFile{hunks}[$hId];
				hunk_failed("splice_includes: $inc line id $lId/$hFile{hunks}[$hId]{count}!");
				next;
			}

			# Record the include line
			$incMap{$hId}{$lId} = 1;
		}
	}

	# Now we can do the splicing in an ordered way:
	for my $hId (sort { $a <=> $b } keys %incMap) {
		# Go through the lines in reverse, that should be save:
		for my $lId (sort { $b <=> $a } keys %{$incMap{$hId}}) {
			splice(@{$hFile{hunks}[$hId]{lines}}, $lId, 1);
			$hFile{hunks}[$hId]{count}--;
		}
	}

	return 1;
}

# Callback function for File::Find
sub wanted {
	-f $_ and ( (0 == $have_wanted)
	         or defined($hWanted{$File::Find::name})
	         or defined($hWanted{"./$File::Find::name"}) )
	      and (! ($_ =~ m/\.pwx$/ ))
	      and push @source_files, $File::Find::name;
	return 1;
}
