#!/usr/bin/perl
# prosty skrypt wyci±gaj±cy z ../src/themes.c wszystkie formaty pozwalaj±c
# na ³atw± edycjê. ziew.
# $Id$

open(FOO, "../src/themes.c") || die("Nie wstanê, tak bêdê le¿a³!");

while(<FOO>) {
	chomp;

	next if (!/\tformat_add\("/);

	s/.*format_add\("//;
	s/", "/ /;
	s/", 1\);//;

	print "$_\n";
}
