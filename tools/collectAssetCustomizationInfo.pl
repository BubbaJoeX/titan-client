# ======================================================================
# collectAssetCustomizationInfo.pl
# Copyright 2003, Sony Online Entertainment, Inc.
# All rights reserved.
# ======================================================================

use strict;
use warnings;

use FindBin;
use lib "$FindBin::Bin/perllib";

use CollectAcmInfo;

# ======================================================================

my $branch = "current";
my $debug = 0;
my $treeFileLookupDataFile;

# ======================================================================

sub printUsage
{
	print "Usage:\n";
	print "\tperl collectAssetCustomizationInfo.pl [-d] [-h] [-b <branch>] -t <treefile lookup filename>\n";
	print "\n";
	print "-d: if specified, turns on debugging info (Default: off)\n";
	print "-h: print this help\n";
	print "-t: loads the TreeFile lookup data from the specified filename\n";
}

# ----------------------------------------------------------------------

sub processCommandLineArgs
{
	my $printHelp   = 0;
	my $requestedHelp = 0;

	while ((scalar @_) && !$printHelp)
	{
		if ($_[0] =~ m/^-h/)
		{
			$printHelp     = 1;
			$requestedHelp = 1;
		}
		elsif ($_[0] =~ m/^-b/)
		{
			shift;
			$branch = $_[0];
			if (!defined($branch))
			{
				print "User must specify a branch name after the -b option, printing help.\n";
				$printHelp = 1;
			}
			else
			{
				print "\$branch=[$branch]\n" if $debug;
			}
		}
		elsif ($_[0] =~ m/^-d/)
		{
			$debug = 1;
		}
		elsif ($_[0] =~ m/^-t/)
		{
			shift;
			$treeFileLookupDataFile = $_[0];
			if (!defined($treeFileLookupDataFile))
			{
				print "User must specify a treefile lookup data filename after the -t option, printing help.\n";
				$printHelp = 1;
			}
			else
			{
				print "\$treeFileLookupDataFile=[$treeFileLookupDataFile]\n" if $debug;
			}
		}
		else
		{
			print "Unsupported option [$_[0]], printing help.\n";
			$printHelp = 1;
		}

		shift;
	}

	if (!$requestedHelp)
	{
		if (!defined($treeFileLookupDataFile))
		{
			print "No TreeFile lookup data file specified, printing usage info.\n";
			$printHelp = 1;
		}
	}

	if ($printHelp)
	{
		printUsage();
		exit($requestedHelp ? 0 : -1);
	}
}

# ======================================================================
# Main Program
# ======================================================================

processCommandLineArgs(@ARGV);

CollectAcmInfo::collect_acm_info_to_fh(
	lookup => $treeFileLookupDataFile,
	fh     => \*STDOUT,
	branch => $branch,
);

exit 0;
