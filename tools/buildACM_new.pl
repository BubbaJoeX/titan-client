#!/usr/bin/perl
# Deprecated: use buildACM.pl (--fast, --custinfo, --keep-temp). Thin wrapper for old habits.
use strict;
use warnings;

use File::Basename qw(dirname);
use Cwd qw(abs_path);
use File::Spec;

my $here = dirname(abs_path(__FILE__));
my $main = File::Spec->catfile($here, 'buildACM.pl');

die "buildACM.pl not found next to buildACM_new.pl: $main\n" unless -f $main;

warn "[buildACM_new.pl] Use buildACM.pl directly; this wrapper forwards arguments.\n";

exec($^X, $main, @ARGV) or die "exec failed: $!\n";
