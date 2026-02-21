#!/usr/bin/perl

use strict;
use warnings;

# ----------------------------------------------------------------------
# Utility functions
# ----------------------------------------------------------------------

sub appendNonCommentContents {
    my ($sourceFileName, $targetFileName) = @_;

    return unless -f $sourceFileName;

    open(my $src, "<", $sourceFileName) or die "Failed to open [$sourceFileName]: $!";
    open(my $tgt, ">>", $targetFileName) or die "Failed to open [$targetFileName] for appending: $!";

    while (<$src>) {
        chomp;
        next if /^\s*#/ || /^\s*$/;
        print $tgt "$_\n";
    }

    close($src);
    close($tgt);
}

sub sortUnique {
    my ($inputFile, $outputFile) = @_;
    open(my $in, '<', $inputFile) or die "Failed to open [$inputFile]: $!";
    my @lines = <$in>;
    close($in);

    open(my $out, '>', $outputFile) or die "Failed to open [$outputFile]: $!";
    my %seen = map { $_ => 1 } @lines;
    print $out sort keys %seen;
    close($out);
}

# ----------------------------------------------------------------------
# Configuration - LOCAL PATHS
# ----------------------------------------------------------------------

# Adjust these paths to your local build environment
my $branch = $ARGV[0] // 'current';
print "Using branch: $branch\n";

my $scriptDir       = 'D:/titan/client/tools/';
my $customizationDir = 'D:/titan/dsrc/sku.0/sys.shared/compiled/game/customization/';
my $treefileLookup  = "$scriptDir/treefile-xlat-$branch.dat"; # prebuilt or generated
my $forceAddVars    = "$customizationDir/force_add_variable_usage.dat";

my $acmMifFile = "$customizationDir/asset_customization_manager.mif";
my $cimMifFile = "$customizationDir/customization_id_manager.mif";
my $acmIffFile = "$customizationDir/asset_customization_manager.iff";
my $cimIffFile = "$customizationDir/customization_id_manager.iff";

my $unoptimizedFile = 'custinfo-raw.dat';
my $optimizedFile   = 'custinfo-raw-optimized.dat';
my $optimizedUnique = 'custinfo-raw-optimized-unique.dat';
my $artReport       = 'art-asset-customization-report.log';

# ----------------------------------------------------------------------
# Steps
# ----------------------------------------------------------------------

# Step 3a: Collect unoptimized customization info
print "Step 3a: Collecting unoptimized customization info...\n";
my $collectScript = "$scriptDir/collectAssetCustomizationInfo.pl";
system("perl $collectScript -b $branch -t $treefileLookup > $unoptimizedFile") == 0
    or die "Failed to collect unoptimized customization info";

# Step 3b: Append forced variable usage
print "Step 3b: Appending forced variable usage...\n";
appendNonCommentContents($forceAddVars, $unoptimizedFile);

# Step 4a: Optimize customization info
print "Step 4a: Optimizing customization info...\n";
my $buildScript = "$scriptDir/buildAssetCustomizationManagerData.pl";
system("perl $buildScript -a $artReport -i $unoptimizedFile -r -t $treefileLookup") == 0
    or die "Failed to optimize customization info";

# Step 4b: Remove duplicates
print "Step 4b: Removing duplicates...\n";
sortUnique($optimizedFile, $optimizedUnique);

# Step 5: Build ACM/CIM MIF files
print "Step 5: Building ACM/CIM MIF files...\n";
system("perl $buildScript -i $optimizedUnique -o $acmMifFile -m $cimMifFile -t $treefileLookup") == 0
    or die "Failed to build ACM/CIM MIF files";

# Step 6: Run MIFF
print "Step 6: Running MIFF on ACM/CIM...\n";
system("miff -i $acmMifFile -o $acmIffFile") == 0
    or die "Failed MIFF on ACM";
system("miff -i $cimMifFile -o $cimIffFile") == 0
    or die "Failed MIFF on CIM";

print "Done! All customization data built locally.\n";
