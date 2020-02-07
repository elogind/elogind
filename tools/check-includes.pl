# SPDX-License-Identifier: CC0-1.0
#!/usr/bin/env perl
#
# checkincludes: Find files included more than once in (other) files.

#if 0 // 5 errors and 2 warnings are inaccaptable for elogind - See PerlCritic.
# foreach $file (@ARGV) {
# 	open(FILE, $file) or die "Cannot open $file: $!.\n";
#
# 	my %includedfiles = ();
#
# 	while (<FILE>) {
# 		if (m/^\s*#\s*include\s*[<"](\S*)[>"]/o) {
# 			++$includedfiles{$1};
# 		}
# 	}
# 	foreach $filename (keys %includedfiles) {
# 		if ($includedfiles{$filename} > 1) {
# 			print "$file: $filename is included more than once.\n";
# 		}
# 	}
#
# 	close(FILE);
# }
#else // 0
use strict;
use warnings;

foreach my $file (@ARGV) {
	my %includedfiles = ();

	open(my $FILE, "<", $file) or die "Cannot open $file: $!.\n";
	while (<$FILE>) {
		if (m/^\s*#\s*include\s*[<"](\S*)[>"]/o) {
			++$includedfiles{$1};
		}
	}
	close($FILE);

	foreach my $filename (keys %includedfiles) {
		if ($includedfiles{$filename} > 1) {
			print "$file: $filename is included more than once.\n";
		}
	}
}
#endif // 0
