# ======================================================================
# CollectAcmInfo.pm
# Shared ACM raw-data collection for collectAssetCustomizationInfo.pl and buildACM.pl
# ======================================================================

package CollectAcmInfo;

use strict;
use warnings;

use AppearanceTemplate;
use BlueprintTextureRendererTemplate;
use ComponentAppearanceTemplate;
use CustomizableShaderTemplate;
use CustomizationVariableCollector;
use DetailAppearanceTemplate;
use LightsaberAppearanceTemplate;
use LodMeshGeneratorTemplate;
use MeshAppearanceTemplate;
use PortalAppearanceTemplate;
use SkeletalAppearanceTemplate;
use SkeletalMeshGeneratorTemplate;
use SwitchShaderTemplate;
use TreeFile;
use VehicleCustomizationVariableGenerator;

# Game-relative names (appearance/...) plus odd layouts where rel still contains a path segment.
my @PROCESS_FILE_PATTERNS = (
	'(^|/)texturerenderer/.+\.trt$',
	'(^|/)shader/.+\.sht$',
	'(^|/)appearance/.+\.(apt|cmp|lmg|lod|lsb|mgn|msh|sat|pob)$',
);

sub _initialize_collect_subsystems
{
	my ($lookup_path) = @_;
	die "CollectAcmInfo: lookup path required" unless defined $lookup_path && length $lookup_path;

	&AppearanceTemplate::install();
	&BlueprintTextureRendererTemplate::install();
	&ComponentAppearanceTemplate::install();
	&CustomizableShaderTemplate::install();
	&DetailAppearanceTemplate::install();
	&LightsaberAppearanceTemplate::install();
	&LodMeshGeneratorTemplate::install();
	&MeshAppearanceTemplate::install();
	&PortalAppearanceTemplate::install();
	&SkeletalAppearanceTemplate::install();
	&SkeletalMeshGeneratorTemplate::install();
	&SwitchShaderTemplate::install();
	&VehicleCustomizationVariableGenerator::install();

	open(my $data_file_handle, '<', $lookup_path)
		or die "CollectAcmInfo: failed to open [$lookup_path]: $!";
	TreeFile::loadFileLookupTable($data_file_handle);
	close($data_file_handle) or die "CollectAcmInfo: failed to close [$lookup_path]: $!";
}

# Run collectors; output goes to $opts{fh} (caller opens the handle). Optional: branch (default current).
sub collect_acm_info_to_fh
{
	my (%opts) = @_;
	my $lookup = $opts{lookup}  or die "CollectAcmInfo::collect_acm_info_to_fh: lookup required";
	my $fh     = $opts{fh}      or die "CollectAcmInfo::collect_acm_info_to_fh: fh required";
	my $branch = $opts{branch} // 'current';

	# Large trees: periodic stderr progress (disable with SWG_ACM_PROGRESS_EVERY=0).
	local $ENV{SWG_ACM_PROGRESS_EVERY} = $ENV{SWG_ACM_PROGRESS_EVERY} // 500;
	# After fork: non-zero child exit on one asset need not abort the whole collect (strict: =0).
	local $ENV{SWG_ACM_SKIP_CHILD_ERRORS} = $ENV{SWG_ACM_SKIP_CHILD_ERRORS} // '1';

	my $same_as_stdout =
		(fileno($fh) != -1 && fileno(STDOUT) != -1 && fileno($fh) == fileno(STDOUT));

	my $save_out;
	if (!$same_as_stdout)
	{
		open($save_out, '>&', \*STDOUT) or die "CollectAcmInfo: dup STDOUT: $!";
		open(STDOUT, '>&', $fh)       or die "CollectAcmInfo: redirect STDOUT: $!";
	}

	my $ok = eval {
		_initialize_collect_subsystems($lookup);
		CustomizationVariableCollector::collectData(@PROCESS_FILE_PATTERNS);
		print STDERR "CollectAcmInfo: vehicle customization pass (reads .tab under SWG_DATATABLES_PATH)...\n";
		VehicleCustomizationVariableGenerator::collectData($branch);
		1;
	};
	my $err = $@;

	if (!$same_as_stdout)
	{
		open(STDOUT, '>&', $save_out) or die "CollectAcmInfo: restore STDOUT: $!";
	}

	die $err if $err;
	return $ok;
}

# Template scan only (L/P/I lines). Same STDOUT redirect rules as collect_acm_info_to_fh.
sub collect_templates_acm_info_to_fh
{
	my (%opts) = @_;
	my $lookup = $opts{lookup} or die "CollectAcmInfo::collect_templates_acm_info_to_fh: lookup required";
	my $fh     = $opts{fh}     or die "CollectAcmInfo::collect_templates_acm_info_to_fh: fh required";

	local $ENV{SWG_ACM_PROGRESS_EVERY}     = $ENV{SWG_ACM_PROGRESS_EVERY}     // 500;
	local $ENV{SWG_ACM_SKIP_CHILD_ERRORS} = $ENV{SWG_ACM_SKIP_CHILD_ERRORS} // '1';

	my $same_as_stdout =
		(fileno($fh) != -1 && fileno(STDOUT) != -1 && fileno($fh) == fileno(STDOUT));

	my $save_out;
	if (!$same_as_stdout)
	{
		open($save_out, '>&', \*STDOUT) or die "CollectAcmInfo: dup STDOUT: $!";
		open(STDOUT, '>&', $fh)       or die "CollectAcmInfo: redirect STDOUT: $!";
	}

	my $allow = $opts{rel_allow_list};
	my @cvc = ();
	if ($allow && ref($allow) eq 'ARRAY')
	{
		push @cvc, $allow;
	}
	push @cvc, @PROCESS_FILE_PATTERNS;

	my $ok = eval {
		_initialize_collect_subsystems($lookup);
		CustomizationVariableCollector::collectData(@cvc);
		1;
	};
	my $err = $@;

	if (!$same_as_stdout)
	{
		open(STDOUT, '>&', $save_out) or die "CollectAcmInfo: restore STDOUT: $!";
	}

	die $err if $err;
	return $ok;
}

# Vehicle .tab pass only. Does not load TreeFile lookup.
sub collect_vehicle_acm_info_to_fh
{
	my (%opts) = @_;
	my $fh     = $opts{fh} or die "CollectAcmInfo::collect_vehicle_acm_info_to_fh: fh required";
	my $branch = $opts{branch} // 'current';

	my $same_as_stdout =
		(fileno($fh) != -1 && fileno(STDOUT) != -1 && fileno($fh) == fileno(STDOUT));

	my $save_out;
	if (!$same_as_stdout)
	{
		open($save_out, '>&', \*STDOUT) or die "CollectAcmInfo: dup STDOUT: $!";
		open(STDOUT, '>&', $fh)       or die "CollectAcmInfo: redirect STDOUT: $!";
	}

	my $ok = eval {
		print STDERR "CollectAcmInfo: vehicle customization pass (reads .tab under SWG_DATATABLES_PATH)...\n";
		VehicleCustomizationVariableGenerator::collectData($branch);
		1;
	};
	my $err = $@;

	if (!$same_as_stdout)
	{
		open(STDOUT, '>&', $save_out) or die "CollectAcmInfo: restore STDOUT: $!";
	}

	die $err if $err;
	return $ok;
}

1;
