#!/usr/bin/perl -w
#  ekglog.pl - log formatter for EKG
#  by Michal Miszewski <fixer@irc.pl>
#
#  version 0.2 (06.08.2003)
#    + support for logged sms
#    + some modifications of parsing hyperlinks
#    + e-mail addresses parsing
#    + corrected html dtd header
#    Grzesiek Kusnierz <koniu@bezkitu.com>:
#    + leading zero for time
#    + fixed parsing hyperlinks
#    Robert Osowiecki <magic.robson@rassun.art.pl>:
#    + corrected '&' with '>' (htmlize)
#
#  version 0.1 (18.06.2003)
#    first published version
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License version 2 as
#  published by the Free Software Foundation.

use Getopt::Long;

$ver = 0.2;
$mynick = "Ja";

sub fmttime {
  my $time = "";
  my $in = shift;
  @tm = gmtime $in if ($in ne "");
  $tm[3] = "0".$tm[3] if length($tm[3]) == 1;
  $tm[4]++;
  $tm[4] = "0".$tm[4] if length($tm[4]) == 1;
  $tm[2] = "0".$tm[2] if length($tm[2]) == 1;
  $tm[1] = "0".$tm[1] if length($tm[1]) == 1;
  $time = "$tm[3]-$tm[4]-".(1900 + $tm[5]) if (!$nodate);
  $time = $time." " if (!($nodate) && !($notime));
  $time = $time."$tm[2]:$tm[1]" if (!$notime);
  return $time;
}

sub htmlize {
  my $ln = shift;
  $ln =~ s/&/&amp;/g;
  $ln =~ s/\>/&gt;/g;
  $ln =~ s/\</&lt;/g;
  if ($ln =~ m/(^ +)/) {
    for (1 .. length $1) { $ln = "&nbsp;".$ln }
    $ln =~ s/ +//;
  }
  $ln =~ s/((www\.|(https?|ftp|telnet|gopher):\/\/)(\_|\-|\~|\d|\w|\/|\.|&|\?|\=|;)+)/<a href=\"$1\">$1\<\/a\>/gi;
  $ln =~ s/((\_|\-|\d|\w|\.)+\@(\_|\-|\d|\w|\.)+)/<a href=\"mailto:$1\">$1\<\/a\>/gi;
  $ln =~ s/[^\\]\\n/<br \/>/g;
  $ln =~ s/[^\\]\\r//g;
  $ln =~ s/\\([^\\])/$1/g;
  $ln =~ s/\\\\/\\/g;
  return $ln;
}

sub txtize {
  my $ln = shift;
  $ln =~ s/[^\\]\\n/\n/g;
  $ln =~ s/[^\\]\\r//g;
  $ln =~ s/\\([^\\])/$1/g;
  $ln =~ s/\\\\/\\/g;
  return $ln;
}

$notime = 0;
$nodate = 0;
GetOptions('text|x' => \$ptext, 'nohdrs|n' => \$nohdrs, 'mynum|m=i' => \$mynum,
  'mynick|y=s' => \$mynick, 'nocss|c' => \$nocss, 'help|h' => \$help,
  'nodate|d' => \$nodate, 'notime|t' => \$notime);
if ($help) {
  print "ekglog version $ver by Micha³ Miszewski <fixer\@irc.pl>\n";
  print "U¿ycie: $0 [-hxndtcmy] [pliki...]\n";
  print "  -h, --help\t\twy¶wietl ten tekst pomocy\n";
  print "  -x, --text\t\tczysty tekst na wyj¶ciu (bez HTML)\n";
  print "  -n, --nohdrs\t\tpomiñ nag³ówki HTML\n";
  print "  -d, --nodate\t\tpomiñ datowniki\n";
  print "  -t, --notime\t\tpomiñ znaczniki czasu\n";
  print "  -c, --nocss\t\tpomiñ styl CSS w kodzie HTML\n";
  print "  -m, --mynum=NUM\ttwój numer GG\n";
  print "  -y, --mynick=NUM\ttwój nick (domy¶lnie: $mynick)\n";
  exit 1;
}

if (!$nohdrs and !$ptext) {
print '<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="pl" lang="pl">
<head>
  <meta http-equiv="Content-Type" content="text/html; charset=iso-8859-2" />
  <meta http-equiv="generator" content="ekglog.pl" />
  <title>log</title>
';
print '  <style type="text/css">
    dt { background: #DDD; margin: 5px }
    .avail { color: yellow }
    .busy { color: green }
    .notavail { color: red }
    .invisible { color: teal }
  </style>
' if !$nocss;
print '</head>
<body>

<dl>
';
}

while (<ARGV>) {
  chomp;
  $text = '';
  if (m/^(chat|msg)(\w+)/) {
    $sent = '';
    if ("$2" eq 'recv') {
      @ln = split /,/, $_, 6;
      $sent = fmttime($ln[4]);
      $sent = " ($sent)" if ($sent ne "");
      if (m/\"/) { $text = substr $ln[5], 1, -1 } else { $text = $ln[5] };
      $nick = $ln[2];
      $num = "/".$ln[1];
    }
    if ("$2" eq 'send') {
      @ln = split /,/, $_, 5;
      if (m/\"/) { $text = substr $ln[4], 1, -1 } else { $text = $ln[4] };
      $nick = $mynick;
      if ($mynum) { $num = "/".$mynum } else { $num = "" }
    }
    $time = (fmttime($ln[3]).$sent);
    $time = $time." " if ($time ne "");
    if ($ptext) {
      $text = txtize $text;
      print "$time$nick$num\n  $text\n\n"
    } else {
      $text = htmlize $text;
      print "<dt>$time<b>$nick</b>$num</dt>\n<dd>$text</dd>\n";
    };
  }
  if (m/^smssend/) {
    @ln = split /,/, $_, 4;
    if (m/\"/) { $text = substr $ln[3], 1, -1 } else { $text = $ln[3] };
    $nick = $ln[1];
    $time = fmttime($ln[2]);
    if ($ptext) {
      $text = txtize $text;
      print "$time SMS -> $nick\n  $text\n\n"
    } else {
      $text = htmlize $text;
      print "<dt>$time <b>SMS -&gt; $nick</b></dt>\n<dd>$text</dd>\n";
    };
  }
  if (m/^status/) {
    @ln = split /,/, $_, 7;
    if (m/\"/) { $text = substr $ln[6], 1, -1 } else {
      $text = $ln[6] if defined($ln[6]);
    };
    $time = fmttime $ln[4];
    $time = $time." " if ($time ne "");
    my %state = (
      avail => "dostêpny",
      busy => "zajêty",
      invisible => "niewidoczny",
      notavail => "niedostêpny"
    );
    if ($text eq "") { $text = "." } else {
      if ($ptext) {
        $text = txtize $text;
        $text = ": ".$text
      } else {
        $text = htmlize $text;
        $text = ": <b>".$text."</b>";
      }
    }
    if ($ptext) {
      print "$time$ln[2]/$ln[1] jest $state{$ln[5]}$text\n\n"
    } else {
      print "<dt>$time<b>$ln[2]</b>/$ln[1] jest <b class=\"$ln[5]\">$state{$ln[5]}</b>$text</dt>\n"
    }
  }
}

print "</dl>\n\n</body>\n</html>\n" if (!$nohdrs and !$ptext);
