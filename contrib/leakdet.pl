#!/usr/bin/perl
# leakdet.pl
# wykrywa problemy z pamiêci± na podstawie tego, co wypluje ,,ltrace''.
#
# $Id$

while (<>)
{
	chomp;

	if (/^malloc\(([0-9]+)\)\s*=\s*(.*)/) {
		print "malloc($1) = $2\n";
		$chunks{$2} = $1;
	}

	if (/^realloc\(NULL, ([0-9]+)\)\s*=\s*(.*)/) {
		print "realloc(NULL, $1) = $2\n";
		$chunks{$2} = $1;
	}

	if (/^realloc\(([0-9a-fx]+), ([0-9]+)\)\s*=\s*(.*)/) {
		print "realloc($1, $2) = $3\n";
		delete $chunks{$1};
		$chunks{$3} = ($2) ? $2 : -1;
	}

	if (/^.*strdup\(.*\)\s*=\s*(.*)/) {
		print "strdup(...) = $1\n";
		$chunks{$1} = 1;
	}

	if (/^free\(([0-9a-fx]+)\)/) {
		print "free($1)\n";
		if (!$chunks{$1}) {
			print "warning: freeing unallocated chunk ($1)\n";
		} elsif ($chunks{$1} == -1) {
			print "warning: freeing previously freed chunk ($1)\n";
		} else {
			$chunks{$1} = -1;
		}
	}
}

foreach (keys %chunks) {
	if ($chunks{$_} != -1) {
		print "warning: unfreed chunk ($_)\n";
	}
}
