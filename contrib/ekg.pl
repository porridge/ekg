#!/usr/bin/perl -w

use Irssi;
use IPC::Open2;
use IO::Select;

#my $select_in;
#my $last_line = "";
#my ($msg_nick, $msg_time, $msg_type, $msg_add, $msg_content);
#my $win;

sub err {
  my $m = shift;
  $win->printformat(MSGLEVEL_CRAP, 'ekg_err', $m);
}

sub info {
  my $m = shift;
  $win->printformat(MSGLEVEL_CRAP, 'ekg_info', $m);
}

sub print_line {
  my $s = shift;
  
  $s =~ s/\\e.//g;
  if ($s =~ s/^INFO //) {
    info($s);
  } elsif ($s =~ s/^NOTICE //) {
    info($s);
  } elsif ($s =~ s/^ERROR //) {
    err($s);
  } elsif ($s =~ /^MSG_START ([^ ]+) ([^ ]+)( (\d+-\d+-\d+.\d+:\d+)( (.*) )?)?/) {
    ($msg_type, $msg_nick, $msg_time, $msg_add) = ($1, $2, $4, $6);
    $msg_content = "";
  } elsif ($s =~ /^MSG_LINE (.*)/) {
    $msg_content .= $1;
  } elsif ($s =~ /^MSG_END/) {
    if ($msg_type =~ /SENT/) {
      $win->printformat(MSGLEVEL_MSGS, 'ekg_own_msg', $msg_nick, $msg_content);
    } else {
      $win->printformat(MSGLEVEL_MSGS, 'ekg_msg', $msg_nick, $msg_content);
    }
  } else {
    info("? $s")
  }
}

sub handle_print {
  my $s = shift;
  my $l;
  foreach $l (split /\\n/, $s) {
    print_line($l);
  }
}

sub process_line {
  my $line = shift;
  my @argv = ();

  while ($line =~ s/([a-z][^ ]*|"([^\\"]|\\.)*") ?//) {
    my $word = $1;
    if ($word =~ /^"/) {
      $word =~ s/^"// or die;
      $word =~ s/"$// or die;
      $word =~ s/\\"/"/g;
      $word =~ s/\\\\/\\/g;
    }
    push @argv, $word;
  }

  my $cmd = $argv[0];

  if ($cmd eq "event") {
  } elsif ($cmd eq "print") {
    handle_print($argv[3]);
  } elsif ($cmd eq "beep") {
  } else {
    err("unrecognised command: @argv");
  }
}

sub time_step {
  if ($select_in->can_read(0)) {
    my ($data, $n);
    $n = sysread EKG_RD, $data, 1000;
    my @tab = split /\n/, $data;
    my ($el, $i);
    $i = 0;
    # perl is stupid: split /\n/, "a\n" gives ["a"] not ["a", ""]
    push @tab, "" if ($data =~ /\n$/);
    foreach $el (@tab) {
      if (++$i == @tab) {
        $last_line = $el;
      } else {
	$el = $last_line . $el if ($i == 1);
        process_line($el)
      }
    }
  }
}

sub cmd_eval {
  info("eval: $_[0]");
  eval "$_[0]";
}

sub cmd_ekg {
  print EKG_WR "$_[0]\n";
}

sub init {
  my $pid = open2(\*EKG_RD, \*EKG_WR, 
  		  "~/cvs/ekg/src/ekg --frontend automaton 2>&1") || die;
  $select_in = IO::Select->new();
  $select_in->add(\*EKG_RD);

  $win = Irssi::Windowitem::window_create(0, 0);

  Irssi::theme_register([
	'ekg_info', '$0',
	'ekg_err', 'error: $0',
	'ekg_msg', '{privmsgnick $0}$1',
	'ekg_own_msg', '{ownprivmsgnick {ownprivnick $0}}$1'
  ]);
  
  Irssi::timeout_add(100, "time_step", "");
  Irssi::command_bind("eeval", "cmd_eval");
  Irssi::command_bind("ekg", "cmd_ekg");
}

Irssi::print("loading ekg");
init();
