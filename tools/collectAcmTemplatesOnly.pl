#!/usr/bin/perl
# Subprocess helper for buildACM.pl: template ACM collect only (L/P/I lines).
# Loads the full lookup passed on the command line; optional --filter lists
# TreeFile-relative paths to visit (batched collect, fresh interpreter per batch).

use strict;
use warnings;
use FindBin;
use lib "$FindBin::Bin/perllib";

use CollectAcmInfo;

my $truncate    = 0;
my $filter_path = '';
my @args        = @ARGV;

while (@args && $args[0] =~ /^--/)
{
	my $sw = shift @args;
	if ($sw eq '--truncate')
	{
		$truncate = 1;
	}
	elsif ($sw eq '--filter')
	{
		$filter_path = shift @args or die "collectAcmTemplatesOnly.pl: --filter needs a path\n";
	}
	else
	{
		die "collectAcmTemplatesOnly.pl: unknown option [$sw]\n";
	}
}

my ($lookup, $out_path) = @args;
die "usage: collectAcmTemplatesOnly.pl [--truncate] [--filter path] <lookup.dat> <out.dat>\n"
	unless defined $lookup && length $lookup && defined $out_path && length $out_path;

my @allow_list;
if ($filter_path)
{
	open(my $ff, '<', $filter_path) or die "$filter_path: $!\n";
	while (my $line = <$ff>)
	{
		$line =~ s/\r?\n$//;
		next if $line =~ /^\s*$/;
		push @allow_list, $line;
	}
	close $ff or die "$filter_path: $!\n";
}

open(my $out, $truncate ? '>' : '>>', $out_path) or die "$out_path: $!\n";

CollectAcmInfo::collect_templates_acm_info_to_fh(
	lookup => $lookup,
	fh     => $out,
	branch => 'current',
	( @allow_list ? (rel_allow_list => \@allow_list) : () ),
);

close $out or die "$out_path: $!\n";
