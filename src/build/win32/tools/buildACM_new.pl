#!/usr/bin/perl
# ======================================================================
# buildACM.pl v2.1.0
# Uses existing custinfo-raw-optimized.dat if available
# ======================================================================

use strict;
use warnings;
use File::Basename;
use File::Find;
use File::Path qw(make_path);
use Cwd qw(abs_path);
use Getopt::Long;

my $VERSION = "2.1.0";

# Options
my ($opt_verbose, $opt_debug, $opt_help, $opt_clean, $opt_keep_temp) = (0,0,0,0,0);
my ($opt_dsrc, $opt_client, $opt_data, $opt_miff, $opt_perllib, $opt_custinfo) = ("","","","","","");

# Paths
my ($root_dir, $dsrc_base, $client_data, $data_output, $perllib_dir, $miff_compiler);
my $optimized_file;

# Temp files
my $lookup_file = "acm_lookup.dat";
my $config_file = "acm_treefile.cfg";
my $collect_file = "acm_collect_result.dat";

# Extensions
my @extensions = qw(.apt .cmp .lmg .lod .lsb .mgn .msh .sat .pob .sht .trt .iff);

# ======================================================================
# MAIN
# ======================================================================

sub main {
    GetOptions(
        'verbose|v' => \$opt_verbose,
        'debug|d'   => \$opt_debug,
        'help|h'    => \$opt_help,
        'clean'     => \$opt_clean,
        'keep-temp' => \$opt_keep_temp,
        'dsrc=s'    => \$opt_dsrc,
        'client=s'  => \$opt_client,
        'data=s'    => \$opt_data,
        'miff=s'    => \$opt_miff,
        'perllib=s' => \$opt_perllib,
        'custinfo=s'=> \$opt_custinfo,
    );
    
    if ($opt_help) { print_usage(); return 0; }
    
    print "=" x 60 . "\nbuildACM.pl v$VERSION\n" . "=" x 60 . "\n\n";
    
    # Detect paths
    print "[0] Detecting paths...\n";
    detect_paths() or die "Path detection failed\n";
    
    # Find custinfo file
    find_custinfo();
    
    print_config() if $opt_verbose;
    clean_all() if $opt_clean;
    
    my $use_existing = ($optimized_file && -f $optimized_file);
    
    if ($use_existing) {
        print "  Using existing custinfo: $optimized_file\n";
        print "  Skipping collection/optimization steps.\n\n";
    }
    
    # Step 1: Gather files & create lookup
    print "[1/4] Creating lookup table...\n";
    my @files = gather_files();
    print "  Found " . scalar(@files) . " asset files\n" if $opt_verbose;
    create_lookup(\@files) or die "Lookup creation failed\n";
    
    # Step 2: Collect (skip if using existing)
    if (!$use_existing) {
        print "[2/4] Collecting customization info...\n";
        collect_info() or die "Collection failed\n";
        
        print "[3/4] Optimizing data...\n";
        optimize() or die "Optimization failed\n";
        $optimized_file = "acm_collect_result-optimized.dat";
    } else {
        print "[2/4] Skipping collection (using existing custinfo)\n";
        print "[3/4] Skipping optimization (using existing custinfo)\n";
    }
    
    # Step 3: Build MIF
    print "[4/4] Building MIF and compiling IFF...\n";
    build_mif() or die "MIF build failed\n";
    compile_iff() or die "IFF compile failed\n";
    
    cleanup() unless $opt_keep_temp;
    
    print "\n" . "=" x 60 . "\nSUCCESS!\n" . "=" x 60 . "\n";
    print "Output:\n";
    print "  $data_output/customization/asset_customization_manager.iff\n";
    print "  $data_output/customization/customization_id_manager.iff\n";
    
    return 0;
}

# ======================================================================
# PATH DETECTION
# ======================================================================

sub detect_paths {
    my $script_dir = dirname(abs_path(__FILE__));
    $script_dir =~ s/\\/\//g;
    
    $root_dir = $script_dir;
    $root_dir =~ s/\/client\/tools.*//i;
    
    $dsrc_base = $opt_dsrc || "$root_dir/dsrc/sku.0/sys.shared/compiled/game";
    $client_data = $opt_client || "$root_dir/data/sku.0/sys.client/compiled/game";
    $data_output = $opt_data || "$root_dir/data/sku.0/sys.client/compiled/game";
    $perllib_dir = $opt_perllib || "$root_dir/client/tools/perllib";
    
    unshift @INC, $perllib_dir if -d $perllib_dir;
    
    # Find Miff
    if ($opt_miff && -e $opt_miff) {
        $miff_compiler = $opt_miff;
    } else {
        for my $p ("$root_dir/exe/win32/Miff.exe", "$root_dir/exe/linux/Miff", "Miff.exe", "Miff") {
            if (-e $p) { $miff_compiler = $p; last; }
        }
    }
    
    # Create dirs
    make_path("$dsrc_base/customization") unless -d "$dsrc_base/customization";
    make_path("$data_output/customization") unless -d "$data_output/customization";
    
    return (-d $client_data && $miff_compiler);
}

sub find_custinfo {
    if ($opt_custinfo && -f $opt_custinfo) {
        $optimized_file = $opt_custinfo;
        return;
    }
    
    # Check default locations
    my @locs = (
        "$root_dir/client/tools/custinfo-raw-optimized.dat",
        "custinfo-raw-optimized.dat",
        "acm_collect_result-optimized.dat",
    );
    
    for my $loc (@locs) {
        if (-f $loc) {
            $optimized_file = $loc;
            print "  Found existing custinfo: $optimized_file\n" if $opt_verbose;
            return;
        }
    }
    
    $optimized_file = "";
}

sub print_config {
    print "\nConfiguration:\n";
    print "  Root:      $root_dir\n";
    print "  DSRC:      $dsrc_base\n";
    print "  Client:    $client_data\n";
    print "  Output:    $data_output\n";
    print "  Perllib:   $perllib_dir\n";
    print "  Miff:      $miff_compiler\n";
    print "  Custinfo:  $optimized_file\n" if $optimized_file;
    print "\n";
}

sub print_usage {
    print <<'END';
buildACM.pl v2.1.0 - Asset Customization Manager Build Script

Usage: perl buildACM.pl [options]

Options:
    -v, --verbose       Verbose output
    -d, --debug         Debug output
    -h, --help          Show help
    --clean             Clean artifacts first
    --keep-temp         Keep temp files
    
    --custinfo <path>   Use existing custinfo-raw-optimized.dat
                        (auto-detected if not specified)
    
    --dsrc <path>       DSRC base path
    --client <path>     Client data path (appearance, shader)
    --data <path>       Output path for IFF files
    --miff <path>       Miff compiler path
    --perllib <path>    Perl library path

Examples:
    # Auto-detect everything (recommended):
    perl buildACM.pl --verbose
    
    # Use specific custinfo file:
    perl buildACM.pl --custinfo "D:/titan/client/tools/custinfo-raw-optimized.dat" -v

END
}

# ======================================================================
# GATHER FILES
# ======================================================================

sub gather_files {
    my @files;
    my %ext = map { lc($_) => 1 } @extensions;
    
    for my $dir ("$client_data/appearance", "$client_data/shader", "$client_data/texturerenderer") {
        next unless -d $dir;
        print "  Scanning: $dir\n" if $opt_verbose;
        
        find(sub {
            return unless -f $_;
            my ($e) = $_ =~ /(\.[^.]+)$/;
            return unless $e && $ext{lc($e)};
            push @files, $File::Find::name =~ s/\\/\//gr;
        }, $dir);
    }
    
    return @files;
}

# ======================================================================
# LOOKUP TABLE
# ======================================================================

sub create_lookup {
    my $files = shift;
    
    # Create config
    open(my $cfg, '>', $config_file) or return 0;
    print $cfg "[SharedFile]\nsearchPath0=$client_data/\n";
    close($cfg);
    
    # Try TreeFile module
    eval {
        require ConfigFile;
        require TreeFile;
        ConfigFile::processConfigFile($config_file);
        TreeFile::buildFileLookupTable(0, $client_data);
        open(my $fh, '>', $lookup_file) or die;
        TreeFile::saveFileLookupTable($fh);
        close($fh);
    };
    
    if ($@) {
        print "  Using fallback lookup generation\n" if $opt_verbose;
        open(my $fh, '>', $lookup_file) or return 0;
        print $fh "p $client_data/:0\n";
        for my $f (@$files) {
            my $rel = $f;
            $rel =~ s/^\Q$client_data\E\/?//;
            print $fh "e $rel:0\n";
        }
        close($fh);
    }
    
    return 1;
}

# ======================================================================
# COLLECT INFO
# ======================================================================

sub collect_info {
    my $script = "$root_dir/client/tools/collectAssetCustomizationInfo.pl";
    return 0 unless -f $script;
    
    my $cmd = "perl -I\"$perllib_dir\" \"$script\" -t \"$lookup_file\"";
    $cmd .= " -d" if $opt_debug;
    
    print "  Running: $cmd\n" if $opt_debug;
    my $output = `$cmd 2>&1`;
    return 0 if $? != 0;
    
    open(my $fh, '>', $collect_file) or return 0;
    print $fh $output;
    
    # Append force_add if exists
    my $force = "$dsrc_base/customization/force_add_variable_usage.dat";
    if (-f $force) {
        print "  Appending force_add_variable_usage.dat\n" if $opt_verbose;
        open(my $fa, '<', $force);
        while (<$fa>) {
            next if /^\s*#/ || /^\s*$/;
            print $fh $_;
        }
        close($fa);
    }
    close($fh);
    
    # Count results
    my ($links, $pals, $ints) = (0, 0, 0);
    open(my $check, '<', $collect_file);
    while (<$check>) {
        $links++ if /^L /;
        $pals++ if /^P /;
        $ints++ if /^I /;
    }
    close($check);
    print "  Collected: $links links, $pals palettes, $ints ints\n" if $opt_verbose;
    
    return 1;
}

# ======================================================================
# OPTIMIZE
# ======================================================================

sub optimize {
    my $script = "$root_dir/client/tools/buildAssetCustomizationManagerData.pl";
    return 0 unless -f $script;
    
    my $cmd = "perl -I\"$perllib_dir\" \"$script\" -i \"$collect_file\" -r -t \"$lookup_file\"";
    $cmd .= " -d" if $opt_debug;
    
    print "  Running: $cmd\n" if $opt_debug;
    my $output = `$cmd 2>&1`;
    print $output if $opt_verbose;
    
    return -f "acm_collect_result-optimized.dat";
}

# ======================================================================
# BUILD MIF
# ======================================================================

sub build_mif {
    my $script = "$root_dir/client/tools/buildAssetCustomizationManagerData.pl";
    my $acm = "$dsrc_base/customization/asset_customization_manager.mif";
    my $cim = "$dsrc_base/customization/customization_id_manager.mif";
    
    my $cmd = "perl -I\"$perllib_dir\" \"$script\"";
    $cmd .= " -i \"$optimized_file\"";
    $cmd .= " -o \"$acm\"";
    $cmd .= " -m \"$cim\"";
    $cmd .= " -t \"$lookup_file\"";
    $cmd .= " -d" if $opt_debug;
    
    print "  Running: $cmd\n" if $opt_debug;
    my $output = `$cmd 2>&1`;
    print $output if $opt_verbose;
    
    return (-f $acm && -f $cim);
}

# ======================================================================
# COMPILE IFF
# ======================================================================

sub compile_iff {
    my $acm_mif = "$dsrc_base/customization/asset_customization_manager.mif";
    my $cim_mif = "$dsrc_base/customization/customization_id_manager.mif";
    my $acm_iff = "$data_output/customization/asset_customization_manager.iff";
    my $cim_iff = "$data_output/customization/customization_id_manager.iff";
    
    for my $pair ([$acm_mif, $acm_iff, "ACM"], [$cim_mif, $cim_iff, "CIM"]) {
        my ($mif, $iff, $name) = @$pair;
        return 0 unless -f $mif;
        
        my $cmd = "\"$miff_compiler\" -i \"$mif\" -o \"$iff\"";
        print "  Compiling $name: $cmd\n" if $opt_debug;
        system($cmd);
        
        unless (-f $iff) {
            print "  ERROR: Failed to create $iff\n";
            return 0;
        }
        print "  Created: $iff\n" if $opt_verbose;
    }
    return 1;
}

# ======================================================================
# CLEANUP
# ======================================================================

sub cleanup {
    for my $f ($lookup_file, $config_file, $collect_file) {
        unlink $f if -f $f;
    }
    # Don't delete user-provided custinfo
    if (!$opt_custinfo) {
        unlink "acm_collect_result-optimized.dat" if -f "acm_collect_result-optimized.dat";
    }
}

sub clean_all {
    print "Cleaning...\n";
    cleanup();
    unlink "$dsrc_base/customization/asset_customization_manager.mif";
    unlink "$dsrc_base/customization/customization_id_manager.mif";
}

# ======================================================================
# ENTRY
# ======================================================================

exit(main());
