#!/usr/bin/perl
#
# checkincludes: Find files included more than once in (other) files.
# Copyright abandoned, 2000, Niels Kristian Bech Jensen <nkbj@image.dk>.

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
