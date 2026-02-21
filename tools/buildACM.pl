#!/usr/bin/perl
# ======================================================================
# buildACM.pl
# 
# Asset Customization Manager (ACM) and Customization ID Manager (CIM)
# Build Script - Unified cross-platform implementation
#
# Generates:
#   - asset_customization_manager.iff
#   - customization_id_manager.iff
#
# Supports:
#   - Windows (D:/titan/...)
#   - Linux (/home/swg/swg-main/...)
#
# Usage:
#   perl buildACM.pl [options]
#
# ======================================================================

use strict;
use warnings;
use File::Basename;
use File::Find;
use File::Spec;
use File::Path qw(make_path);
use Cwd qw(abs_path getcwd);
use Getopt::Long;

# ======================================================================
# Configuration
# ======================================================================

my $VERSION = "1.0.0";

# Command line options
my $opt_verbose = 0;
my $opt_debug = 0;
my $opt_help = 0;
my $opt_version = 0;
my $opt_dsrc_path = "";
my $opt_data_path = "";
my $opt_miff_path = "";
my $opt_perllib_path = "";
my $opt_clean = 0;

# Path configuration (auto-detected or specified)
my $root_dir = "";
my $dsrc_base = "";
my $data_base = "";
my $miff_compiler = "";
my $perllib_dir = "";

# Working files
my $lookup_file = "acm_lookup.dat";
my $collect_result_file = "acm_collect_result.dat";
my $optimized_file = "acm_collect_result-optimized.dat";

# Asset extensions to process
my @asset_extensions = qw(.apt .cmp .lmg .lod .lsb .mgn .msh .sat .pob .sht .trt);

# ======================================================================
# Main Program
# ======================================================================

sub main
{
    parse_command_line();
    
    if ($opt_help)
    {
        print_usage();
        exit(0);
    }
    
    if ($opt_version)
    {
        print_version();
        exit(0);
    }
    
    print_banner();
    
    # Detect and validate paths
    if (!detect_paths())
    {
        print_error("Failed to detect paths. Use --dsrc and --data to specify manually.");
        exit(1);
    }
    
    print_config() if $opt_verbose;
    
    # Clean previous build artifacts if requested
    clean_artifacts() if $opt_clean;
    
    # Step 1: Gather files and create lookup table
    print_step(1, 6, "Gathering asset files and creating lookup table...");
    my @files = gather_asset_files();
    if (!create_lookup_file(\@files))
    {
        print_error("Failed to create lookup file.");
        exit(1);
    }
    print_info("Found " . scalar(@files) . " asset files.") if $opt_verbose;
    
    # Step 2: Collect asset customization info
    print_step(2, 6, "Collecting asset customization information...");
    if (!collect_customization_info())
    {
        print_error("Failed to collect asset customization info.");
        exit(1);
    }
    
    # Step 3: Optimize customization data
    print_step(3, 6, "Optimizing customization data...");
    if (!optimize_customization_data())
    {
        print_error("Failed to optimize customization data.");
        exit(1);
    }
    
    # Step 4: Build ACM and CIM .mif files
    print_step(4, 6, "Building ACM and CIM .mif files...");
    if (!build_mif_files())
    {
        print_error("Failed to build .mif files.");
        exit(1);
    }
    
    # Step 5: Compile .mif to .iff
    print_step(5, 6, "Compiling .mif files to .iff...");
    if (!compile_mif_to_iff())
    {
        print_error("Failed to compile .mif to .iff.");
        exit(1);
    }
    
    # Step 6: Cleanup
    print_step(6, 6, "Cleaning up temporary files...");
    cleanup_temp_files();
    
    print_success("ACM and CIM build completed successfully!");
    
    return 0;
}

# ======================================================================
# Command Line Processing
# ======================================================================

sub parse_command_line
{
    GetOptions(
        'verbose|v'     => \$opt_verbose,
        'debug|d'       => \$opt_debug,
        'help|h|?'      => \$opt_help,
        'version'       => \$opt_version,
        'dsrc=s'        => \$opt_dsrc_path,
        'data=s'        => \$opt_data_path,
        'miff=s'        => \$opt_miff_path,
        'perllib=s'     => \$opt_perllib_path,
        'clean'         => \$opt_clean,
    ) or die "Error parsing command line options.\n";
}

sub print_usage
{
    print <<'USAGE';
buildACM.pl - Asset Customization Manager Build Script

Usage:
    perl buildACM.pl [options]

Options:
    -v, --verbose       Enable verbose output
    -d, --debug         Enable debug output
    -h, --help          Show this help message
    --version           Show version information
    --dsrc <path>       Path to dsrc directory (auto-detected if not specified)
    --data <path>       Path to data directory (auto-detected if not specified)
    --miff <path>       Path to Miff compiler (auto-detected if not specified)
    --perllib <path>    Path to Perl library directory (auto-detected if not specified)
    --clean             Clean previous build artifacts before building

Examples:
    # Auto-detect paths (Windows):
    perl buildACM.pl --verbose

    # Auto-detect paths (Linux):
    perl buildACM.pl --verbose

    # Specify paths manually:
    perl buildACM.pl --dsrc "D:/titan/dsrc" --data "D:/titan/data" --verbose

    # Full manual specification:
    perl buildACM.pl \
        --dsrc "/home/swg/swg-main/dsrc" \
        --data "/home/swg/swg-main/data" \
        --miff "/home/swg/swg-main/exe/linux/Miff" \
        --perllib "/home/swg/swg-main/client/tools/perllib"

Path Structure Expected:
    <root>/
    ├── dsrc/
    │   └── sku.0/
    │       └── sys.shared/
    │           └── compiled/
    │               └── game/
    │                   ├── appearance/
    │                   ├── shader/
    │                   ├── texturerenderer/
    │                   ├── customization/
    │                   └── datatables/
    ├── data/
    │   └── sku.0/
    │       └── sys.client/
    │           └── compiled/
    │               └── game/
    │                   └── customization/
    ├── client/
    │   └── tools/
    │       ├── perllib/
    │       └── Miff (or Miff.exe)
    └── exe/
        ├── win32/ (Windows)
        └── linux/ (Linux)

Output Files:
    - <data>/sku.0/sys.client/compiled/game/customization/asset_customization_manager.iff
    - <data>/sku.0/sys.client/compiled/game/customization/customization_id_manager.iff

USAGE
}

sub print_version
{
    print "buildACM.pl version $VERSION\n";
    print "Asset Customization Manager Build Script\n";
    print "Cross-platform Perl implementation\n";
}

sub print_banner
{
    print "=" x 60 . "\n";
    print "buildACM.pl - Asset Customization Manager Build Script\n";
    print "Version $VERSION\n";
    print "=" x 60 . "\n\n";
}

# ======================================================================
# Path Detection
# ======================================================================

sub detect_paths
{
    # Detect root directory from script location
    my $script_dir = dirname(abs_path(__FILE__));
    $script_dir = normalize_path($script_dir);
    
    # Try to find root by looking for client/tools pattern
    $root_dir = $script_dir;
    $root_dir =~ s/\/client\/tools.*//i;
    
    # If that didn't work, try parent directories
    if ($root_dir eq $script_dir)
    {
        # Try going up directories
        my @parts = split(/\//, $script_dir);
        for (my $i = $#parts; $i >= 0; $i--)
        {
            my $test_root = join("/", @parts[0..$i]);
            if (-d "$test_root/dsrc" && -d "$test_root/client")
            {
                $root_dir = $test_root;
                last;
            }
        }
    }
    
    print_debug("Detected root directory: $root_dir") if $opt_debug;
    
    # Set dsrc path
    if ($opt_dsrc_path)
    {
        $dsrc_base = normalize_path($opt_dsrc_path);
    }
    else
    {
        $dsrc_base = "$root_dir/dsrc/sku.0/sys.shared/compiled/game";
    }
    
    # Set data path
    if ($opt_data_path)
    {
        $data_base = normalize_path($opt_data_path);
    }
    else
    {
        $data_base = "$root_dir/data/sku.0/sys.client/compiled/game";
    }
    
    # Set perllib path
    if ($opt_perllib_path)
    {
        $perllib_dir = normalize_path($opt_perllib_path);
    }
    else
    {
        $perllib_dir = "$root_dir/client/tools/perllib";
    }
    
    # Add perllib to @INC
    if (-d $perllib_dir)
    {
        unshift @INC, $perllib_dir;
    }
    
    # Set Miff compiler path
    if ($opt_miff_path)
    {
        $miff_compiler = normalize_path($opt_miff_path);
    }
    else
    {
        # Try to find Miff compiler
        my @miff_locations = (
            "$root_dir/exe/win32/Miff.exe",
            "$root_dir/exe/linux/Miff",
            "$root_dir/client/tools/Miff.exe",
            "$root_dir/client/tools/Miff",
            "Miff.exe",
            "Miff",
        );
        
        foreach my $loc (@miff_locations)
        {
            if (-x $loc || -e $loc)
            {
                $miff_compiler = $loc;
                last;
            }
        }
    }
    
    # Validate paths
    my $valid = 1;
    
    if (!-d $dsrc_base)
    {
        print_warning("DSRC path not found: $dsrc_base");
        $valid = 0;
    }
    
    if (!-d $perllib_dir)
    {
        print_warning("Perllib path not found: $perllib_dir");
        # Continue anyway, might work without it
    }
    
    if (!$miff_compiler || (!-x $miff_compiler && !-e $miff_compiler))
    {
        print_warning("Miff compiler not found: $miff_compiler");
        $valid = 0;
    }
    
    # Create data output directory if it doesn't exist
    my $output_dir = "$data_base/customization";
    if (!-d $output_dir)
    {
        print_info("Creating output directory: $output_dir") if $opt_verbose;
        make_path($output_dir) or print_warning("Could not create: $output_dir");
    }
    
    return $valid;
}

sub normalize_path
{
    my $path = shift;
    return "" unless defined $path;
    
    # Convert backslashes to forward slashes
    $path =~ s/\\/\//g;
    
    # Remove trailing slash
    $path =~ s/\/+$//;
    
    # Remove double slashes (except at start for network paths)
    $path =~ s/([^:])\/\/+/$1\//g;
    
    return $path;
}

sub print_config
{
    print "\nConfiguration:\n";
    print "  Root:     $root_dir\n";
    print "  DSRC:     $dsrc_base\n";
    print "  Data:     $data_base\n";
    print "  Perllib:  $perllib_dir\n";
    print "  Miff:     $miff_compiler\n";
    print "\n";
}

# ======================================================================
# Step 1: Gather Asset Files
# ======================================================================

sub gather_asset_files
{
    my @files;
    
    my @search_dirs = (
        "$dsrc_base/appearance",
        "$dsrc_base/shader",
        "$dsrc_base/texturerenderer",
    );
    
    # Build extension hash for quick lookup
    my %ext_hash = map { $_ => 1 } @asset_extensions;
    
    foreach my $dir (@search_dirs)
    {
        next unless -d $dir;
        
        print_debug("Scanning: $dir") if $opt_debug;
        
        find(
            {
                wanted => sub {
                    return unless -f $_;
                    my $ext = "";
                    if ($_ =~ /(\.[^.]+)$/)
                    {
                        $ext = lc($1);
                    }
                    if ($ext_hash{$ext})
                    {
                        my $full_path = normalize_path($File::Find::name);
                        push @files, $full_path;
                        print_debug("  Found: $full_path") if $opt_debug;
                    }
                },
                no_chdir => 1,
            },
            $dir
        );
    }
    
    return @files;
}

sub create_lookup_file
{
    my $files_ref = shift;
    
    unlink $lookup_file if -e $lookup_file;
    
    open(my $fh, '>', $lookup_file) or do {
        print_error("Cannot create lookup file: $lookup_file - $!");
        return 0;
    };
    
    # Write base path
    print $fh "p $dsrc_base/:0\n";
    
    # Write each file entry
    foreach my $file (@$files_ref)
    {
        # Extract relative path from dsrc_base
        my $rel_path = $file;
        $rel_path =~ s/^\Q$dsrc_base\E\/?//;
        
        print $fh "e $rel_path:0\n";
    }
    
    close($fh);
    
    print_info("Created lookup file with " . scalar(@$files_ref) . " entries.") if $opt_verbose;
    
    return 1;
}

# ======================================================================
# Step 2: Collect Customization Info
# ======================================================================

sub collect_customization_info
{
    my $collect_script = "$root_dir/client/tools/collectAssetCustomizationInfo.pl";
    
    if (!-f $collect_script)
    {
        print_error("Collect script not found: $collect_script");
        return 0;
    }
    
    # Build command
    my $cmd = "perl";
    $cmd .= " -I\"$perllib_dir\"" if -d $perllib_dir;
    $cmd .= " \"$collect_script\"";
    $cmd .= " -t \"$lookup_file\"";
    $cmd .= " -d" if $opt_debug;
    
    print_debug("Executing: $cmd") if $opt_debug;
    
    # Execute and capture output
    my $output = `$cmd 2>&1`;
    my $exit_code = $? >> 8;
    
    if ($exit_code != 0)
    {
        print_error("Collect script failed (exit code: $exit_code)");
        print_error($output) if $output;
        return 0;
    }
    
    # Write output to result file
    open(my $fh, '>', $collect_result_file) or do {
        print_error("Cannot create result file: $collect_result_file - $!");
        return 0;
    };
    
    print $fh $output;
    
    # Append force_add_variable_usage.dat if it exists
    my $force_add_file = "$dsrc_base/customization/force_add_variable_usage.dat";
    if (-f $force_add_file)
    {
        print_info("Appending force_add_variable_usage.dat") if $opt_verbose;
        open(my $fa_fh, '<', $force_add_file) or do {
            print_warning("Cannot read: $force_add_file - $!");
        };
        if ($fa_fh)
        {
            while (<$fa_fh>)
            {
                next if /^\s*#/;  # Skip comments
                next if /^\s*$/;  # Skip empty lines
                print $fh $_;
            }
            close($fa_fh);
        }
    }
    
    close($fh);
    
    return 1;
}

# ======================================================================
# Step 3: Optimize Customization Data
# ======================================================================

sub optimize_customization_data
{
    my $build_script = "$root_dir/client/tools/buildAssetCustomizationManagerData.pl";
    
    if (!-f $build_script)
    {
        print_error("Build script not found: $build_script");
        return 0;
    }
    
    # Build command for optimization pass
    my $cmd = "perl";
    $cmd .= " -I\"$perllib_dir\"" if -d $perllib_dir;
    $cmd .= " \"$build_script\"";
    $cmd .= " -i \"$collect_result_file\"";
    $cmd .= " -r";  # Optimize/remove entries
    $cmd .= " -t \"$lookup_file\"";
    $cmd .= " -d" if $opt_debug;
    
    print_debug("Executing: $cmd") if $opt_debug;
    
    my $output = `$cmd 2>&1`;
    my $exit_code = $? >> 8;
    
    if ($exit_code != 0)
    {
        print_error("Optimization failed (exit code: $exit_code)");
        print_error($output) if $output;
        return 0;
    }
    
    print $output if $opt_verbose && $output;
    
    # Verify optimized file was created
    if (!-f $optimized_file)
    {
        print_error("Optimized file was not created: $optimized_file");
        return 0;
    }
    
    return 1;
}

# ======================================================================
# Step 4: Build MIF Files
# ======================================================================

sub build_mif_files
{
    my $build_script = "$root_dir/client/tools/buildAssetCustomizationManagerData.pl";
    
    my $acm_mif = "$dsrc_base/customization/asset_customization_manager.mif";
    my $cim_mif = "$dsrc_base/customization/customization_id_manager.mif";
    
    # Ensure customization directory exists
    my $custom_dir = "$dsrc_base/customization";
    make_path($custom_dir) unless -d $custom_dir;
    
    # Build command for MIF generation
    my $cmd = "perl";
    $cmd .= " -I\"$perllib_dir\"" if -d $perllib_dir;
    $cmd .= " \"$build_script\"";
    $cmd .= " -i \"$optimized_file\"";
    $cmd .= " -o \"$acm_mif\"";
    $cmd .= " -m \"$cim_mif\"";
    $cmd .= " -t \"$lookup_file\"";
    $cmd .= " -d" if $opt_debug;
    
    print_debug("Executing: $cmd") if $opt_debug;
    
    my $output = `$cmd 2>&1`;
    my $exit_code = $? >> 8;
    
    if ($exit_code != 0)
    {
        print_error("MIF generation failed (exit code: $exit_code)");
        print_error($output) if $output;
        return 0;
    }
    
    print $output if $opt_verbose && $output;
    
    # Verify MIF files were created
    if (!-f $acm_mif)
    {
        print_error("ACM MIF file was not created: $acm_mif");
        return 0;
    }
    
    if (!-f $cim_mif)
    {
        print_error("CIM MIF file was not created: $cim_mif");
        return 0;
    }
    
    print_info("Created ACM MIF: $acm_mif") if $opt_verbose;
    print_info("Created CIM MIF: $cim_mif") if $opt_verbose;
    
    return 1;
}

# ======================================================================
# Step 5: Compile MIF to IFF
# ======================================================================

sub compile_mif_to_iff
{
    my $acm_mif = "$dsrc_base/customization/asset_customization_manager.mif";
    my $cim_mif = "$dsrc_base/customization/customization_id_manager.mif";
    
    my $acm_iff = "$data_base/customization/asset_customization_manager.iff";
    my $cim_iff = "$data_base/customization/customization_id_manager.iff";
    
    # Ensure output directory exists
    my $output_dir = "$data_base/customization";
    make_path($output_dir) unless -d $output_dir;
    
    # Compile ACM
    if (!compile_single_mif($acm_mif, $acm_iff, "ACM"))
    {
        return 0;
    }
    
    # Compile CIM
    if (!compile_single_mif($cim_mif, $cim_iff, "CIM"))
    {
        return 0;
    }
    
    return 1;
}

sub compile_single_mif
{
    my ($mif_file, $iff_file, $name) = @_;
    
    if (!-f $mif_file)
    {
        print_error("$name MIF file not found: $mif_file");
        return 0;
    }
    
    my $cmd = "\"$miff_compiler\" -i \"$mif_file\" -o \"$iff_file\"";
    
    print_debug("Executing: $cmd") if $opt_debug;
    
    my $output = `$cmd 2>&1`;
    my $exit_code = $? >> 8;
    
    if ($exit_code != 0)
    {
        print_error("$name compilation failed (exit code: $exit_code)");
        print_error($output) if $output;
        return 0;
    }
    
    if (!-f $iff_file)
    {
        print_error("$name IFF file was not created: $iff_file");
        return 0;
    }
    
    print_info("Compiled $name: $iff_file") if $opt_verbose;
    
    return 1;
}

# ======================================================================
# Step 6: Cleanup
# ======================================================================

sub cleanup_temp_files
{
    my @temp_files = (
        $lookup_file,
        $collect_result_file,
        $optimized_file,
    );
    
    foreach my $file (@temp_files)
    {
        if (-f $file)
        {
            unlink $file;
            print_debug("Removed: $file") if $opt_debug;
        }
    }
}

sub clean_artifacts
{
    print_info("Cleaning previous build artifacts...") if $opt_verbose;
    
    cleanup_temp_files();
    
    # Also clean generated MIF files if requested
    my @mif_files = (
        "$dsrc_base/customization/asset_customization_manager.mif",
        "$dsrc_base/customization/customization_id_manager.mif",
    );
    
    foreach my $file (@mif_files)
    {
        if (-f $file)
        {
            unlink $file;
            print_debug("Removed: $file") if $opt_debug;
        }
    }
}

# ======================================================================
# Output Functions
# ======================================================================

sub print_step
{
    my ($step, $total, $message) = @_;
    print "[$step/$total] $message\n";
}

sub print_info
{
    my $message = shift;
    print "  INFO: $message\n";
}

sub print_warning
{
    my $message = shift;
    print "  WARNING: $message\n";
}

sub print_error
{
    my $message = shift;
    print STDERR "  ERROR: $message\n";
}

sub print_debug
{
    my $message = shift;
    print "  DEBUG: $message\n" if $opt_debug;
}

sub print_success
{
    my $message = shift;
    print "\n";
    print "=" x 60 . "\n";
    print "SUCCESS: $message\n";
    print "=" x 60 . "\n";
}

# ======================================================================
# Entry Point
# ======================================================================

exit(main());

__END__

=head1 NAME

buildACM.pl - Asset Customization Manager Build Script

=head1 SYNOPSIS

    perl buildACM.pl [options]

=head1 DESCRIPTION

This script builds the Asset Customization Manager (ACM) and Customization ID
Manager (CIM) IFF files from game assets. It supports both Windows and Linux
platforms and automatically detects paths when possible.

=head1 OPTIONS

=over 4

=item B<-v, --verbose>

Enable verbose output.

=item B<-d, --debug>

Enable debug output (more verbose than --verbose).

=item B<-h, --help>

Show help message.

=item B<--version>

Show version information.

=item B<--dsrc PATH>

Specify the dsrc base path manually.

=item B<--data PATH>

Specify the data output path manually.

=item B<--miff PATH>

Specify the path to the Miff compiler.

=item B<--perllib PATH>

Specify the path to the Perl library directory.

=item B<--clean>

Clean previous build artifacts before building.

=back

=head1 EXAMPLES

    # Auto-detect paths:
    perl buildACM.pl --verbose

    # Manual paths (Windows):
    perl buildACM.pl --dsrc "D:/titan/dsrc/sku.0/sys.shared/compiled/game" \
                     --data "D:/titan/data/sku.0/sys.client/compiled/game" \
                     --verbose

    # Manual paths (Linux):
    perl buildACM.pl --dsrc "/home/swg/swg-main/dsrc/sku.0/sys.shared/compiled/game" \
                     --data "/home/swg/swg-main/data/sku.0/sys.client/compiled/game" \
                     --verbose

=head1 AUTHOR

SWG Development Team

=cut
