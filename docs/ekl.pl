#!/usr/bin/perl -w
# ekg cat log - wy¶wietla logi ekg w nieco bardziej czytelnej formie.
# (c) 2001 Pawe³ Maziarz <drg@go2.pl>
# ;]

if (!$ARGV[0]) {
    print "U¿ycie: $0 plik_z_logiem\n";
    exit;
} 

while (<>) {
  @tab = split(' ', $_);
  if ($tab[0] =~ /<</ || $tab[0] =~ />>/) { 
      if ($tab[0] =~ />>/) {
          $I = "\033[1;32m";
      } else {
          $I = "\033[1;34m";
      }
      $buf=<>;
      print "\033[1;37m$tab[0] \033[1;33m[$tab[7]]$I [$tab[3]]: \033[0m$buf";
    }
}
