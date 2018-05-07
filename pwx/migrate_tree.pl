#!/usr/bin/perl -w

# ================================================================
# ===        ==> --------     HISTORY      -------- <==        ===
# ================================================================
#
# Version  Date        Maintainer      Changes, Additions, Fixes
# 0.0.1    2017-05-02  sed, PrydeWorX  First basic design
# 0.0.2    2017-05-07  sed, PrydeWorX  Work flow integrated up to creating the formatted patches
#
# ========================
# === Little TODO list ===
# ========================
#
use strict;
use warnings;
use Cwd qw(getcwd abs_path);
use File::Basename;
use File::Find;
use Git::Wrapper;
use Readonly;
use Try::Tiny;

# ================================================================
# ===        ==> ------ Help Text and Version ----- <==        ===
# ================================================================
Readonly my $VERSION     => "0.0.2"; ## Please keep this current!
Readonly my $VERSMIN     => "-" x length($VERSION);
Readonly my $PROGDIR     => dirname($0);
Readonly my $PROGNAME    => basename($0);
Readonly my $WORKDIR     => getcwd();
Readonly my $CHECK_TREE  => abs_path($PROGDIR . "/check_tree.pl");
Readonly my $COMMIT_FILE => abs_path($PROGDIR . "/last_mutual_commits.csv");
Readonly my $USAGE_SHORT => "$PROGNAME <--help|[OPTIONS] <upstream path> <refid>>";
Readonly my $USAGE_LONG  => qq#
elogind git tree migration V$VERSION
----------------------------$VERSMIN

  Reset the git tree in <upstream path> to the <refid>. The latter can be any
  commit, branch or tag. Then search its history since the last mutual commit
  for any commit that touches at least one file in any subdirectory of the
  directory this script was called from.
  
  Please note that this program was written especially for elogind. It is very
  unlikely that it can be used in any other project.

USAGE:
  $USAGE_SHORT

OPTIONS:
     --advance       : Use the last upstream commit that has been written
                       into "$COMMIT_FILE" as the last
                       mutual commit to use. This is useful for continued
                       migration of branches. Incompatible with -c|--commit.
  -c|--commit <hash> : The mutual commit to use. If this option is not used,
                       the script looks into "$COMMIT_FILE"
                       and uses the commit noted for <refid>. Incompatible
                       with --advance.
  -h|--help            Show this help and exit.
  -o|--output <path> : Path to where to write the patches. The default is to
                       write into "$PROGDIR/patches".

Notes:
  - The upstream tree is reset and put back into the current state after the
    script finishes.
  - When the script succeeds, it adds a line to "$COMMIT_FILE"
    of the form:
    <tag>-last <newest found commit>. You can use that line for the next
    <refid> you wish to migrate to.
#;

# ================================================================
# ===        ==> -------- Global variables -------- <==        ===
# ================================================================

my $commit_count    = 0;  ## It is easiest to count the relevant commits globally.
my $do_advance      = 0;  ## If set by --advance, use src-<hash> as last commit.
my @lCommits        = (); ## List of all relevant commits that have been found, in topological order.
my @lPatches        = (); ## List of the formatted patches build from @lCommits.
my $main_result     = 1;  ## Used for parse_args() only, as simple $result is local everywhere.
my $mutual_commit   = ""; ## The last mutual commit to use. Will be read from csv if not set by args.
my $output_path     = abs_path("$PROGDIR/patches");
my $previous_refid  = ""; ## Store current upstream state, so we can revert afterwards.
my $show_help       = 0;
my @source_files    = (); ## Final file list to process, generated in in generate_file_list().
my $upstream_path   = "";
my $wanted_refid    = ""; ## The refid to reset the upstream tree to.


# ================================================================
# ===        ==> ------- MAIN DATA STRUCTURES ------ <==       ===
# ================================================================
my %hCommits = (); ## Hash of all upstream commits that touch at least one file:
                   ## ( refid : count of files touched )
my %hFiles   = (); ## List of all source files as %hFile structures with a simple
                   ## ( tgt : $hFile ) mapping.
my $hFile    = {}; ## Simple description of one file consisting of:
                   ## Note: The store is %hFiles, this is used as a global pointer.
                   ##       Further although the target is the key in %hFiles, we
                   ##       store it here, too, so we always no the $hFile's key.
                   ## ( patch: Full path to the patch that check_tree.pl would generate
                   ##   src  : The potential relative upstream path with 'elogind' substituted by 'systemd'
                   ##   tgt  : The found relative path in the local tree
                   ## )
my %hMutuals = (); ## Mapping of the $COMMIT_FILE, that works as follows:
                   ## CSV lines are structured as:
                   ## <refid> <hash> src-<hash> tgt-<hash>
                   ## They map as follows:
                   ## ( <refid> : {
                   ##       mutual : <hash> | This is the last mutual commit
                   ##       src    : <hash> | This is the last individual commit in the upstream tree (*)
                   ##       tgt    : <hash> | This is the last individual commit in the local tree    (*)
                   ##       } )
                   ## (*) When this entry was written. This means that src-<hash> can be used as
                   ##     the next last mutual commit, when this migration run is finished. To make
                   ##     this automatic, the --advance option triggers exactly that.
# ================================================================
# ===        ==> --------  Function list   -------- <==        ===
# ================================================================

sub build_hCommits;     ## Build a hash of commits for the current hFile.
sub build_hFile;        ## Add an entry to hFiles for a specific target file.
sub build_lCommits;     ## Build the topological list of all relevant commits.
sub build_lPatches;     ## Fill $output_path with formatted patches from @lCommits.
sub checkout_upstream;  ## Checkout the given refid on $upstream_path.
sub generate_file_list; ## Find all relevant files and store them in @wanted_files
sub get_last_mutual;    ## Find or read the last mutual refid between this and the upstream tree.
sub parse_args;         ## Parse ARGV for the options we support
sub wanted;             ## Callback function for File::Find

# ================================================================
# ===        ==> --------    Prechecks     -------- <==        ==
# ================================================================

-x $CHECK_TREE or die ("$CHECK_TREE not found!");

$main_result = parse_args(@ARGV);
( (!$main_result)                 ## Note: Error or --help given, then exit.
	 or ( $show_help and print "$USAGE_LONG" ) )
	and exit(!$main_result);
get_last_mutual and generate_file_list
	or exit 1;
checkout_upstream($wanted_refid) ## Note: Does nothing if $wanted_refid is already checked out.
	or exit 1;


# ================================================================
# ===        ==> -------- = MAIN PROGRAM = -------- <==        ===
# ================================================================

# -----------------------------------------------------------------
# --- 1) Go through all files and generate a list of all source ---
# ---    commits that touch the file.                           ---
# -----------------------------------------------------------------
print "Searching relevant commits ...";
for my $file_part (@source_files) {
	build_hFile($file_part) or next;
	build_hCommits or next;
}
printf(" %d commits found\n", $commit_count);

# -----------------------------------------------------------------
# --- 2) Get a list of all commits and build @lCommits, checking --
# ---    against the found hashes in $hCommits. This will build ---
# ---    a list that has the correct order the commits must be  ---
# ---    applied.                                               ---
# -----------------------------------------------------------------
build_lCommits or exit 1;

# -----------------------------------------------------------------
# --- 3) Go through the relevant commits and create formatted   ---
# ---    patches for them using.                                ---
# -----------------------------------------------------------------
build_lPatches or exit 1;


# ===========================
# === END OF MAIN PROGRAM ===
# ===========================

# ================================================================
# ===        ==> --------     Cleanup      -------- <==        ===
# ================================================================

length($previous_refid) and checkout_upstream($previous_refid);

# ================================================================
# ===        ==> ---- Function Implementations ---- <==        ===
# ================================================================


# ------------------------------------------------------
# --- Build a hash of commits for the current hFile. ---
# ------------------------------------------------------
sub build_hCommits {
	my $git = Git::Wrapper->new($upstream_path);
	
	my @lRev = $git->rev_list( {
			topo_order => 1,
			"reverse"  => 1,
			 oneline   => 1
		},
		"${mutual_commit}..${wanted_refid}",
		$hFile->{src} );

	for my $line (@lRev) {
		if ( $line =~ m/^(\S+)\s+/ ) {
			defined($hCommits{$1})
				 or ++$commit_count
				and $hCommits{$1} = 0;
			++$hCommits{$1};
		}
	}

	return 1;
}


# ------------------------------------------------------------------
# --- Build a list of the relevant commits in topological order. ---
# ------------------------------------------------------------------
sub build_lCommits {
	my $git = Git::Wrapper->new($upstream_path);

	my @lRev = $git->rev_list( {
			topo_order => 1,
			"reverse"  => 1,
			 oneline   => 1
		},
		"${mutual_commit}..${wanted_refid}" );

	for my $line (@lRev) {
		if ( $line =~ m/^(\S+)\s+/ ) {
			defined($hCommits{$1})
				and push @lCommits, "$1";
		}
	}
	
	return 1;
}


# ----------------------------------------------------------
# --- Add an entry to hFiles for a specific target file. ---
# ----------------------------------------------------------
sub build_hFile {
	my ($tgt) = @_;

	defined($tgt) and length($tgt) or print("ERROR\n") and die("build_hfile: tgt is empty ???");

	# We only prefixed './' to unify things. Now it is no longer needed:
	$tgt =~ s,^\./,,;

	# Check the target file
	my $src = "$tgt";
	$src =~ s/elogind/systemd/g;
	$src =~ s/\.pwx$//;
	-f "$upstream_path/$src" or return 0;

	# Build the patch name
	my $patch = $tgt;
	$patch =~ s/\//_/g;

	# Build the central data structure.
	%hFiles = (
		$tgt => {
			patch => $output_path . "/" . $patch . ".patch",
			src   => $src,
			tgt   => $tgt
		} );
	
	# This is now our current hFile
	$hFile = $hFiles{$tgt};

	return 1;
}


# ----------------------------------------------------------------
# --- Fill $output_path with formatted patches from @lCommits. ---
# ----------------------------------------------------------------
sub build_lPatches {
	my $git    = Git::Wrapper->new($upstream_path);
	my $cnt    = 0;
	my $curLen = 0;
	my $maxLen = 0;
	my @lRev   = ();
	my @lPath  = ();

	for my $refid (@lCommits) {
		@lRev = $git->rev_list( {
			"1"      => 1,
			 oneline => 1
		}, $refid );

		$curLen = length($lRev[0]);
		$curLen > $maxLen and $maxLen = $curLen;
		printf("\r%03d: %s", ++$cnt, $lRev[0]
			. ($maxLen > $curLen ? ' ' x ($maxLen - $curLen) : ""));

		try {
			@lPath = $git->format_patch({
				"1"                  => 1,
				"find-copies"        => 1,
				"find-copies-harder" => 1,
				"numbered"           => 1,
				"output-directory"   => $output_path
			},
			"--start-number=$cnt",
			$refid);
		} catch {
			print "\nERROR: Couldn't format-patch $refid\n";
			print "Exit Code : " . $_->status . "\n";
			print "Message   : " . $_->error  . "\n";
			return 0;
		};
	}
	$maxLen and print "\r" . (' ' x $maxLen) . "\r$cnt patches built\n";
	
	return 1;
}


# -----------------------------------------------------------------------
# --- Checkout the given refid on $upstream_path                      ---
# --- Returns 1 on success, 0 otherwise.                              ---
# -----------------------------------------------------------------------
sub checkout_upstream {
	my ($commit)   = @_;

	# It is completely in order to not wanting to checkout a specific commit.
	defined($commit) and length($commit) or return 1;

	my $new_commit = "";
	my $git        = Git::Wrapper->new($upstream_path);
	my @lOut       = ();

	# Save the previous commit
	try {
		@lOut = $git->rev_parse({short => 1}, "HEAD");
	} catch {
		print "ERROR: Couldn't rev-parse $upstream_path HEAD\n";
		print "Exit Code : " . $_->status . "\n";
		print "Message   : " . $_->error  . "\n";
		return 0;
	};
	$previous_refid = $lOut[0];

	# Get the shortened commit hash of $commit
	try {
		@lOut = $git->rev_parse({short => 1}, $commit);
	} catch {
		print "ERROR: Couldn't rev-parse $upstream_path \"$commit\"\n";
		print "Exit Code : " . $_->status . "\n";
		print "Message   : " . $_->error  . "\n";
		return 0;
	};
	$new_commit = $lOut[0];

	# Now check it out, unless we are already there:
	if ($previous_refid ne $new_commit) {
		print "Checking out $new_commit in upstream tree...";
		try {
			$git->checkout($new_commit);
		} catch {
			print "\nERROR: Couldn't checkout \"new_commit\" in $upstream_path\n";
			print "Exit Code : " . $_->status . "\n";
			print "Message   : " . $_->error  . "\n";
			return 0;
		};
		print " done\n";
	}

	return 1;
}


# -----------------------------------------------------------------------
# --- Finds all relevant files and store them in @wanted_files        ---
# --- Returns 1 on success, 0 otherwise.                              ---
# -----------------------------------------------------------------------
sub generate_file_list {

	# Do some cleanup first. Just to be sure.
	print "Cleaning up...";
	`rm -rf build`;
	`find -iname '*.orig' -or -iname '*.bak' -or -iname '*.rej' -or -iname '*~' -or -iname '*.gc??' | xargs rm -f`;
	print " done\n";

	# The idea now is, that we use File::Find to search for files
	# in all legal directories this program allows.
	print "Find relevant files...";
	for my $xDir ("docs", "factory", "m4", "man", "shell-completion", "src", "tools") {
		if ( -d "$xDir" ) {
			find(\&wanted, "$xDir");
		}
	}

	# There are also root files we need to check. Thanks to the usage of
	# check_tree.pl to generate the later commit diffs, these can be safely
	# added to our file list as well.
	for my $xFile ("Makefile", "Makefile.am", "TODO", "CODING_STYLE", "configure",
	               ".mailmap", "LICENSE.GPL2", "meson_options.txt", "NEWS",
	               "meson.build", "configure.ac", ".gitignore") {
		-f "$xFile" and push @source_files, "./$xFile";
	}
	print " done\n";

	# Just to be sure...
	scalar @source_files
		 or print("ERROR: No source files found? Where the hell are we?\n")
		and return 0;

	return 1;
}


# ------------------------------------------------------------------------------
# --- Find or read the last mutual refid between this and the upstream tree. ---
# ------------------------------------------------------------------------------
sub get_last_mutual {

	# No matter whether the commit is set or not, we need to read the
	# commit file now, and write it back later if we have changes.
	if ( -f $COMMIT_FILE ) {
		if (open my $fIn, "<", $COMMIT_FILE) {
			my @lLines = <$fIn>;
			close $fIn;
			
			for my $line (@lLines) {
				chomp $line;
				if ( $line =~ m/^\s*(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s*$/ ) {
					my $ref = $1;
					my $src = $3;
					my $tgt = $4;
					$hMutuals{$ref} = {
						mutual => $2,
						src    => undef,
						tgt    => undef
					};
					$src =~ m/^src-(\S+)$/ and $hMutuals{$ref}{src} = $1;
					$tgt =~ m/^tgt-(\S+)$/ and $hMutuals{$ref}{tgt} = $1;
				}
			}
		} else {
			print("ERROR: $COMMIT_FILE can not be read!\n$!\n");
			return 0;
		}
	}

	# If this is already set, we are fine.
	if ( length($mutual_commit) ) {
		$hMutuals{$wanted_refid}{mutual} = $mutual_commit;
		return 1;
	}
	
	# Now check against --advance and then set $mutual_commit accordingly.
	if (defined($hMutuals{$wanted_refid})) {
		if ($do_advance) {
			defined($hMutuals{$wanted_refid}{src})
				and $hMutuals{$wanted_refid}{mutual} = $hMutuals{$wanted_refid}{src}
				 or print "ERROR: --advance set, but no source hash found!\n"
				and return 0;
		}
		$mutual_commit = $hMutuals{$wanted_refid}{mutual};
		return 1;
	}
	
	print "ERROR: There is no last mutual commit known for refid \"$wanted_refid\"!\n";
	
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

		# Check --advance option
		if ($args[$i] =~ m/^--advance$/) {
			$do_advance = 1;
		}

		# Check for -c|--commit option
		# -------------------------------------------------------------------------------
		elsif ($args[$i] =~ m/^-(?:c|-commit)$/) {
			if ( ( ($i + 1) >= @args )
			  || ( $args[$i+1] =~ m,^[-/.], ) ) {
				print "ERROR: Option $args[$i] needs a refid as argument!\n\nUsage: $USAGE_SHORT\n";
				$result = 0;
				next;
			}
			$mutual_commit = $args[++$i];
		}

		# Check for -h|--help option
		# -------------------------------------------------------------------------------
		elsif ($args[$i] =~ m/^-(?:h|-help)$/) {
			$show_help = 1;
		}

		# Check for -o|--output option
		# -------------------------------------------------------------------------------
		elsif ($args[$i] =~ m/^-(?:o|-output)$/) {
			if ( ( ($i + 1) >= @args )
			  || ( $args[$i+1] =~ m,^[-/.], ) ) {
				print "ERROR: Option $args[$i] needs a path as argument!\n\nUsage: $USAGE_SHORT\n";
				$result = 0;
				next;
			}
			$output_path = abs_path($args[++$i]);
		}

		# Check for unknown options:
		# -------------------------------------------------------------------------------
		elsif ($args[$i] =~ m/^-/) {
			print "ERROR: Unknown option \"$args[$i]\" encountered!\n\nUsage: $USAGE_SHORT\n";
			$result = 0;
		}

		# Everything else is considered to the path to upstream first and refid second
		# -------------------------------------------------------------------------------
		else {
			# But only if they are not set, yet:
			if (length($upstream_path) && length($wanted_refid)) {
				print "ERROR: Superfluous argument \"$args[$i]\" found!\n\nUsage: $USAGE_SHORT\n";
				$result = 0;
				next;
			}
			if (length($upstream_path) ) {
				$wanted_refid = "$args[$i]";
			} else {
				if ( ! -d "$args[$i]") {
					print "ERROR: Upstream path \"$args[$i]\" does not exist!\n\nUsage: $USAGE_SHORT\n";
					$result = 0;
					next;
				}
				$upstream_path = abs_path($args[$i]);
			}
		}
	} ## End looping arguments

	# If we have no refid now, show short help.
	if ($result && !$show_help && !length($wanted_refid) ) {
		print "ERROR: Please provide a path to upstream and a refid!\n\nUsage: $USAGE_SHORT\n";
		$result = 0;
	}

	# If both --advance and --commit were used, we can not tell what the
	# user really wants. So better be safe here, or we might screw the tree!
	if ($do_advance && length($mutual_commit)) {
		print "ERROR: You have used both --advance and --commit.\n";
		print "       Which one is the one you really want?\n\n";
		print "Usage: $USAGE_SHORT\n";
		$result = 0;
	}

	return $result;
} ## parse_srgs() end


# Callback function for File::Find
sub wanted {
	my $f = $File::Find::name;

	$f =~ m,^\./, or $f = "./$f"; 

	-f $_ and (! ($_ =~ m/\.pwx$/ ) )
	      and push @source_files, $File::Find::name;

	return 1;
}
