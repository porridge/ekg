#!/usr/bin/perl -w
# ekg cat log - wy¶wietla logi ekg w nieco bardziej czytelnej formie.
# (c) 2001 Pawe³ Maziarz <drg@go2.pl>
# $Id$

if (!$ARGV[0]) {
    print "U¿ycie: $0 plik_z_logiem\n";
    exit;
} 

$OI = "\033[0m";
$II = "\033[1;36m";
$OO = "\033[1;35m";
$IO = "\033[1;33m";

while (<>) {
    @tab = split(' ', $_);
    if ($tab[0] && ($tab[0] =~ /<</ || $tab[0] =~ />>/)) { 
        if($tab[1] =~ /Rozmowa/) { $O = "R"; } else { $O = "W"; }
        if($tab[0] =~ /<</) { 
	    $OO = "\033[1;32m"; 
	} else {  
	    $OO = "\033[1;34m";
	}	
        $buf=<>;
        print "\[$IO$O$OI\] $OO$tab[0] $II\[$tab[7]\]$OO \[$tab[3]\]: $OI$buf";
    }
}
