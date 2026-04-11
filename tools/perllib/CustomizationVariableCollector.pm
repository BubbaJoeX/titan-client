# ======================================================================
# CustomizationVariableCollector.pm
# Copyright 2003, Sony Online Entertainment
# All rights reserved.
# ======================================================================

package CustomizationVariableCollector;
use strict;

use Iff;
use TreeFile;

# ======================================================================
# CustomizationVariableCollector public variables.
# ======================================================================

# our $relativePathName;

# ======================================================================
# Setup variables that can be imported by Exporter into user modules.
# ======================================================================

use vars qw(@ISA @EXPORT_OK $VERSION);
use Exporter;
$VERSION = 1.00;
@ISA	 = qw(Exporter);

# These symbols are okay to export if specifically requested.
#@EXPORT_OK = qw(&buildFileLookupTable &saveFileLookupTable &loadFileLookupTable &getFullPathName);
@EXPORT_OK = qw(&logAssetLink &logPaletteColorVariable &logBasicRangedIntVariable &collectData);

# ======================================================================
# CustomizationVariableCollector private variables.
# ======================================================================

my %handlerByTag;
my $debug = 0;

# ======================================================================
# CustomizationVariableCollector public functions.
# ======================================================================

# ----------------------------------------------------------------------
# Collection Output Format
#
# Asset Linkage Information:
#	L userAssetTreeFilePath:usedAssetTreeFilePath
#
# Customization Variable Usage:
#
#	Palette Color Variable Type:
#	  P assetTreeFilePath:variablePathName:paletteFilename:defaultIndex
#
#	Basic Ranged Int Type:
#	  I assetTreeFilePath:variablePathName:minValue:maxValuePlusOne:defaultValue
# ----------------------------------------------------------------------

# Strip Windows/editor absolute prefixes from paths written to raw collect output
# (':' would break buildAssetCustomizationManagerData.pl field splitting).
sub _acm_normalize_logged_game_path
{
	my ($p) = @_;
	return "" unless defined $p;
	$p =~ s/^\s+|\s+$//g;
	$p =~ s!\\!/!g;

	my @extra = defined $ENV{SWG_ACM_STRIP_ABS_PREFIX} && length $ENV{SWG_ACM_STRIP_ABS_PREFIX}
		? split(/;/, $ENV{SWG_ACM_STRIP_ABS_PREFIX})
		: ();
	# Copy each entry to a mutable string (bare literals in the list are read-only).
	for my $raw ('C:/Users/Casey/Desktop/holo_sat', @extra)
	{
		next unless defined $raw && length $raw;
		my $strip = "$raw";
		$strip =~ s/^\s+|\s+$//g;
		$strip =~ s!\\!/!g;
		$strip =~ s!/+\z!!;
		next unless length $strip;
		my $q = quotemeta $strip;
		$q =~ s!\\!/!g;
		$p =~ s{^$q/*}{}i;
	}
	$p =~ s!//+!/!g;
	if ($p =~ m{((?:appearance|shader|texturerenderer|datatables|object)/.+)}i)
	{
		return $1;
	}
	return $p;
}

# ----------------------------------------------------------------------
# @syntax  logAssetLink(assetPathName, subordinateAssetPathName)
#
# Used to indicate that assetPathName makes use of
# subordianteAssetPathName.	 A user of assetPathName will get
# customization variables it declares and any variables declared by
# everything assetPathName links to directly
# (i.e. subordinateAssetPathName) and indirectly (e.g. things that
# subordinateAssetPathName links to and so on).
# ----------------------------------------------------------------------

sub logAssetLink
{
	my $assetPathName = shift;
	die "assetPathName arg not specified" if !defined ($assetPathName);
	$assetPathName =~ s!\\!/!g;

	my $subordinateAssetPathName = shift;
	die "subordinateAssetPathName arg not specified" if !defined ($subordinateAssetPathName);
	$subordinateAssetPathName =~ s!\\!/!g;

	$assetPathName            = _acm_normalize_logged_game_path($assetPathName);
	$subordinateAssetPathName = _acm_normalize_logged_game_path($subordinateAssetPathName);

	return if !length($assetPathName) || !length($subordinateAssetPathName);

	if ($subordinateAssetPathName eq $assetPathName)
	{
		return;
	}

	print "L $assetPathName:$subordinateAssetPathName\n";
}

# ----------------------------------------------------------------------
# @syntax  logPaletteColorVariable(assetPathName, variablePathName, palettePathName, defaultIndex)
#
# Used to indicate that the specified assetPathName makes use of a
# variable named variablePathName.	The variable controls a palette
# color selected from palettePathName.	A reasonable default color for
# the asset is the palette entry at defaultIndex.
# ----------------------------------------------------------------------

sub logPaletteColorVariable
{
	die "Not enough arguments" if scalar(@_) < 4;

	my $assetPathName	 = shift;
	$assetPathName =~ s!\\!/!g;

	my $variablePathName = shift;

	my $palettePathName	 = shift;
	$palettePathName =~ s!\\!/!g;

	my $defaultIndex	 = shift;

	$assetPathName   = _acm_normalize_logged_game_path($assetPathName);
	$palettePathName = _acm_normalize_logged_game_path($palettePathName);

	print "P $assetPathName:$variablePathName:$palettePathName:$defaultIndex\n" if length($assetPathName) && length($palettePathName);
}

# ----------------------------------------------------------------------
# @syntax  logBasicRangedIntVariable(assetPathName, variablePathName, minValueInclusive, maxValueExclusive, defaultValue)
#
# Used to indicate that the specified assetPathName makes use of a
# variable named variablePathName.	The variable controls a basic
# ranged int that somehow affects the visual appearance.  A reasonable
# default value for the asset is specified with defaultValue.
# ----------------------------------------------------------------------

sub logBasicRangedIntVariable
{
	die "Not enough arguments" if scalar(@_) < 5;

	my $assetPathName	  = shift;
	$assetPathName =~ s!\\!/!g;

	my $variablePathName  = shift;
	my $minValueInclusive = shift;
	my $maxValueExclusive = shift;
	my $defaultValue	  = shift;

	$assetPathName = _acm_normalize_logged_game_path($assetPathName);

	print "I $assetPathName:$variablePathName:$minValueInclusive:$maxValueExclusive:$defaultValue\n" if length($assetPathName);
}

# ----------------------------------------------------------------------
# @syntax  registerHandler(formTagHandled, handlerFunctionRef)
#
# Associates the specified handlerFunctionRef function with the
# specified top-level IFF form tag formTagHandled.	During execution
# of collectCustomizationData, any IFF file with a first form that
# matches formTagHandled will have an IFF opened and passed to
# handlerFunctionRef like this:
#
#	&$handlerFunctionRef(iffRef, treeFilePathName)
#
# The iff pointed to by iffRef will be at the very beginning of the file,
# in a completely unread state.	 The handlerFunction should return non-zero
# on success and zero on failure.  Failure will cancel the collection
# process with an error.
# ----------------------------------------------------------------------

sub registerHandler
{
	# Handle args.
	die "Too few arguments" if @_ < 2;
	my $formTag	   = shift;
	my $handlerRef = shift;

	# Ensure we don't clobber another handler.
	die "form tag [$formTag] is already handled, multiple handlers per tag not supported" if exists $handlerByTag{$formTag};

	# Assign the handler.
	$handlerByTag{$formTag} = $handlerRef;
}

# ----------------------------------------------------------------------
# @syntax  unregisterHandler(formTagHandled)
#
# Remove support for the specified top-level iff form.
# ----------------------------------------------------------------------

sub unregisterHandler
{
	# Handle args.
	die "Too few arguments" if @_ < 1;
	my $formTag	   = shift;

	# Ensure we have this handler.
	die "form tag [$formTag] is not currently handled" if !exists $handlerByTag{$formTag};

	# Remove the handler from the hash.
	delete $handlerByTag{$formTag};
}

# ----------------------------------------------------------------------
# Callback for TreeFile::findRelativeRegexMatch.
# ----------------------------------------------------------------------

sub processTreeFile
{
	# Iff slurps the whole file into RAM; skip huge files to avoid OOM / silent kernel kill.
	my $path = $TreeFile::fullPathName;
	my $max  = $ENV{SWG_ACM_MAX_TEMPLATE_BYTES};
	if (!defined $max || $max eq '')
	{
		$max = 200 * 1024 * 1024;
	}
	elsif ($max =~ /^[0-9]+$/)
	{
		$max = int($max);
	}
	else
	{
		$max = 200 * 1024 * 1024;
	}
	if ($max > 0 && defined $path && length $path)
	{
		my @st = stat($path);
		if (@st && $st[7] > $max)
		{
			print STDERR "CollectAcmInfo: skipping oversize template [$path] ($st[7] bytes; cap SWG_ACM_MAX_TEMPLATE_BYTES=$max, 0=unlimited)\n";
			return;
		}
	}

	# Open the file.
	my $fileHandle;
	die "failed to open [$path]: $!" if !open($fileHandle, "< " . $path);

	# Get an Iff for the specified pathname.
	my $iff = Iff->createFromFileHandle($fileHandle);

	# Close the file.
	die "failed to close [$path]: $!" if !close($fileHandle);

	# Skip the file if it wasn't a valid iff.
	if (!defined($iff))
	{
		print STDERR "iff error: [$path] does not appear to be a valid IFF, skipping.\n";
		return;
	}

	# Get the first name.
	my $name = $iff->getCurrentName();
	die "iff: getCurrentName() failed" if !defined($name);

	# Lookup handler function for name.
	my $handlerRef = $handlerByTag{$name};
	if (!defined($handlerRef))
	{
		undef $iff;
		return;
	}

	# Call handler function.
	my $handleResult = &$handlerRef($iff, $TreeFile::relativePathName, $TreeFile::fullPathName);
	undef $iff;
	die "iff error: handler failed to process file [$path]\n" if !$handleResult;

	print "debug: successfully processed [$TreeFile::relativePathName].\n" if $debug;
}

# ----------------------------------------------------------------------
# @syntax  collectData(treeFileRegexArray)
#
# Processes all treefiles with TreeFile-relative pathnames matching
# any one of the regex entries listed in treeFileRegexArray.  For
# files matching top-level-form file IDs registered for handling with 
# this module, the version handler will be called upon to process the
# data.	 These handlers will output asset linkage and customization
# variable data (basic ranged int type = default, min and max values;
# palette color type = default index and palette name).
# ----------------------------------------------------------------------

sub _acm_use_fork_per_template
{
	return 0 if $^O eq 'MSWin32';
	eval { require Config; 1 } or return 0;
	return 0 unless $Config::Config{d_fork};
	# Default off: ~136k forks makes the collect take hours. Enable only when debugging OOM/kills:
	# SWG_ACM_FORK_EACH=1
	return 1 if defined($ENV{SWG_ACM_FORK_EACH}) && $ENV{SWG_ACM_FORK_EACH} eq '1';
	return 0;
}

sub _acm_skip_child_errors
{
	return 0 if defined($ENV{SWG_ACM_SKIP_CHILD_ERRORS}) && $ENV{SWG_ACM_SKIP_CHILD_ERRORS} eq '0';
	return 1;
}

sub collectData
{
	my @patterns;
	my $allow;
	# Optional first argument: \@list of TreeFile-relative paths to visit (full lookup must still be loaded).
	if (@_ && ref($_[0]) eq 'ARRAY')
	{
		my $allow_list = shift;
		$allow = ($allow_list && @$allow_list) ? { map { $_ => 1 } @$allow_list } : {};
		@patterns = @_;
	}
	else
	{
		@patterns = @_;
	}

	my $expect = $allow
		? TreeFile::countRelativeRegexMatchAllowSet($allow, @patterns)
		: TreeFile::countRelativeRegexMatch(@patterns);
	print STDERR $allow
		? "CollectAcmInfo: $expect paths in this batch (allow-set over full lookup).\n"
		: "CollectAcmInfo: $expect paths match ACM patterns (this many files will be visited).\n";

	my $fork_each = _acm_use_fork_per_template();
	print STDERR "CollectAcmInfo: per-template fork isolation: "
		. ($fork_each ? "on (slow; SWG_ACM_FORK_EACH unset next run for normal speed)\n" : "off (SWG_ACM_FORK_EACH=1 if a single asset OOM-kills perl)\n");

	my $pe = $ENV{SWG_ACM_PROGRESS_EVERY};
	my $progress_every = (defined $pe && $pe =~ /^[1-9][0-9]*$/) ? int($pe) : 0;
	if ($fork_each && $progress_every > 0)
	{
		print STDERR "CollectAcmInfo: fork mode is slow (~0.05-0.4s/asset); full scan can take 2-12+ hours. "
			. "Progress every $progress_every files + line after file 1.\n";
	}
	elsif ($fork_each)
	{
		print STDERR "CollectAcmInfo: fork mode (SWG_ACM_PROGRESS_EVERY=0) will stay quiet until the pass finishes.\n";
	}

	my $skipped_after_signal       = 0;
	my $skipped_after_child_exit   = 0;
	my $skipped_after_inprocess_error = 0;
	my $skip_child_errors          = _acm_skip_child_errors();

	my $run_one = sub {
		my $full = $TreeFile::fullPathName;
		my $rel  = $TreeFile::relativePathName;

		if ($fork_each)
		{
			my $pid = fork();
			die "CollectAcmInfo: fork failed: $!" if !defined $pid;
			if ($pid == 0)
			{
				# Do not require POSIX::_exit here: require POSIX in a child often dies → exit 255 with no useful line.
				eval {
					my $code = 0;
					eval { processTreeFile(); 1 } or do {
						my $e = $@;
						chomp $e if defined $e;
						print STDERR "CollectAcmInfo: failed on [$full] ($rel): $e\n" if length $e;
						$code = 1;
					};
					exit($code);
				};
				print STDERR "CollectAcmInfo: child uncaught exception on [$full] ($rel): $@\n";
				exit(255);
			}
			my $waited = waitpid($pid, 0);
			die "CollectAcmInfo: waitpid: $!" if $waited != $pid;
			my $st = $?;
			if ($st == -1)
			{
				die "CollectAcmInfo: waitpid returned -1";
			}
			my $sig = $st & 127;
			my $core = $st & 128;
			my $code = $st >> 8;
			if ($sig != 0)
			{
				++$skipped_after_signal;
				print STDERR "CollectAcmInfo: child signal $sig on [$full] ($rel) (core dumped=" . ($core ? 1 : 0) . ") — skipped (OOM or native crash).\n";
				return;
			}
			if ($code != 0)
			{
				if ($skip_child_errors)
				{
					++$skipped_after_child_exit;
					print STDERR "CollectAcmInfo: child exit $code on [$full] ($rel) — skipped (set SWG_ACM_SKIP_CHILD_ERRORS=0 to abort instead).\n";
					return;
				}
				die "CollectAcmInfo: child exit $code on [$full] ($rel)\n";
			}
			return;
		}

		eval {
			processTreeFile();
			1;
		} or do {
			my $e = $@;
			chomp $e if defined $e;
			print STDERR "CollectAcmInfo: failed on [$full] ($rel): $e\n";
			if ($skip_child_errors)
			{
				++$skipped_after_inprocess_error;
				print STDERR "CollectAcmInfo: skipped (SWG_ACM_SKIP_CHILD_ERRORS=0 to abort whole collect).\n";
				return;
			}
			die $e;
		};
	};

	if ($progress_every > 0)
	{
		my $processed = 0;
		my $callback  = sub {
			if ($processed == 0)
			{
				print STDERR "CollectAcmInfo: first template [$TreeFile::relativePathName] ...\n";
			}
			$run_one->();
			++$processed;
			if ($processed == 1 || $processed % $progress_every == 0)
			{
				print STDERR "CollectAcmInfo: template files processed: $processed / $expect ...\n";
			}
		};
		if ($allow)
		{
			TreeFile::findRelativeRegexMatchAllowSet($callback, $allow, @patterns);
		}
		else
		{
			TreeFile::findRelativeRegexMatch($callback, @patterns);
		}
		print STDERR "CollectAcmInfo: template pass finished ($processed of $expect paths)"
			. _acm_collect_skip_summary($skipped_after_signal, $skipped_after_child_exit, $skipped_after_inprocess_error);
	}
	else
	{
		my $logged_first = 0;
		my $wrapped = sub {
			if (!$logged_first)
			{
				$logged_first = 1;
				print STDERR "CollectAcmInfo: first template [$TreeFile::relativePathName] ...\n";
			}
			$run_one->();
		};
		if ($allow)
		{
			TreeFile::findRelativeRegexMatchAllowSet($wrapped, $allow, @patterns);
		}
		else
		{
			TreeFile::findRelativeRegexMatch($wrapped, @patterns);
		}
		print STDERR "CollectAcmInfo: template pass finished ($expect paths)"
			. _acm_collect_skip_summary($skipped_after_signal, $skipped_after_child_exit, $skipped_after_inprocess_error);
	}
}

sub _acm_collect_skip_summary
{
	my ($sig_n, $exit_n, $inproc_n) = @_;
	$inproc_n //= 0;
	return ".\n" if (!$sig_n && !$exit_n && !$inproc_n);
	my $s = '';
	$s .= "; skipped after child signal: $sig_n" if $sig_n;
	$s .= "; skipped after child exit: $exit_n" if $exit_n;
	$s .= "; skipped after in-process template error: $inproc_n" if $inproc_n;
	return "$s\n";
}

# ======================================================================

1;
