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
#   - Linux (/home/swg/swg-main/tools/...)
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
use IPC::Open3 qw(open3);
use Symbol qw(gensym);

# ======================================================================
# Configuration
# ======================================================================

my $VERSION = "1.2.19";

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
my $opt_fast = 0;
my $opt_custinfo_path = "";
my $opt_keep_temp = 0;

# Path configuration (auto-detected or specified)
my $root_dir = "";
my $dsrc_base = "";
my $data_base = "";
my $miff_compiler = "";
my $perllib_dir = "";
my $tools_dir = "";

# Resolved after path init: intermediates under the tools dir containing this script (not process CWD).
my $lookup_file = "";
my $collect_result_file = "";
my $optimized_file = "";
# Input for MIF generation (-i): cached optimized data or freshly built file.
my $mif_input_file = "";
my $skip_collect_and_optimize = 0;

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

    # Line-buffer stdout/stderr so step lines appear before long-running collect.
    {
        my $prev = select STDERR;
        $| = 1;
        select STDOUT;
        $| = 1;
        select $prev;
    }

    # Piping stderr (e.g. "2>&1 | tee log"): if tee exits or the pipe breaks, the next
    # stderr write can raise SIGPIPE and terminate perl with no clear error.
    $SIG{PIPE} = 'IGNORE';
    # RLIMIT_FSIZE (ulimit -f): collect stdout is a multi-GB file; default SIGXFSZ kills perl.
    $SIG{XFSZ} = 'IGNORE';

    init_tools_paths();

    # Detect and validate paths (uses $tools_dir from init_tools_paths)
    if (!detect_paths())
    {
        print_error("Failed to detect paths. Use --dsrc and --data to specify manually.");
        exit(1);
    }

    resolve_cached_optimized_input();

    print_config() if $opt_verbose;
    
    # Clean previous build artifacts if requested
    clean_artifacts() if $opt_clean;
    
    my $step = 0;
    # Full: gather, collect, optimize+MIF (one Perl), compile IFF, cleanup = 5. Fast: 4.
    my $total_steps = $skip_collect_and_optimize ? 4 : 5;
    
    # Step 1: Gather files and create lookup table
    ++$step;
    print_step($step, $total_steps, "Gathering asset files and creating lookup table...");
    my @asset_records = gather_asset_files();
    if (!create_lookup_file(\@asset_records))
    {
        print_error("Failed to create lookup file.");
        exit(1);
    }
    print_info("Found " . scalar(@asset_records) . " asset files (after de-dupe).") if $opt_verbose;
    
    if (!$skip_collect_and_optimize)
    {
        # Step 2: Collect asset customization info
        ++$step;
        print_step($step, $total_steps, "Collecting asset customization information...");
        if (!collect_customization_info(\@asset_records))
        {
            print_error("Failed to collect asset customization info.");
            exit(1);
        }
        
        # Optimize raw collect output and write ACM/CIM .mif in one buildAssetCustomizationManagerData.pl run
        ++$step;
        print_step($step, $total_steps, "Optimizing customization data and building ACM/CIM MIF files...");
        if (!optimize_and_build_mif_single_pass())
        {
            print_error("Failed to optimize customization data or build MIF files.");
            exit(1);
        }
        $mif_input_file = $optimized_file;
    }
    else
    {
        print_info("Skipped collect and optimize (using $mif_input_file).") if $opt_verbose;
        ++$step;
        print_step($step, $total_steps, "Building ACM and CIM .mif files from cached optimized data...");
        if (!build_mif_files())
        {
            print_error("Failed to build .mif files.");
            exit(1);
        }
    }
    
    # Compile .mif to .iff
    ++$step;
    print_step($step, $total_steps, "Compiling .mif files to .iff...");
    if (!compile_mif_to_iff())
    {
        print_error("Failed to compile .mif to .iff.");
        exit(1);
    }
    
    # Cleanup
    ++$step;
    print_step($step, $total_steps, $opt_keep_temp ? "Keeping temporary files (--keep-temp)." : "Cleaning up temporary files...");
    cleanup_temp_files() unless $opt_keep_temp;
    
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
        'fast'          => \$opt_fast,
        'custinfo=s'    => \$opt_custinfo_path,
        'keep-temp'     => \$opt_keep_temp,
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
    --dsrc <path>       compiled/game for tabs/MIF/SWG_DATATABLES_PATH; templates also scanned under data/sku.0, dsrc/sku.0, serverdata/
    --data <path>       Path to data directory (auto-detected if not specified)
    --miff <path>       Path to Miff compiler (auto-detected if not specified)
    --perllib <path>    Path to Perl library directory (auto-detected if not specified)
    --clean             Clean previous build artifacts before building
    --fast              Skip collect+optimize if a cached optimized .dat exists (see below)
    --custinfo <path>   Use this optimized raw file as MIF input; skips collect+optimize
    --keep-temp         Keep acm_lookup.dat / collect / optimized intermediates under <root>/tools (script dir)

  Cached optimized files (for --fast), first match wins (in the tools directory next to buildACM.pl):
    custinfo-raw-optimized.dat
    acm_collect_result-optimized.dat

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
        --perllib "/home/swg/swg-main/tools/perllib"

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
    ├── tools/                    (Linux swg-main; or client/tools/ on some Windows trees)
    │   ├── perllib/
    │   ├── buildACM.pl
    │   └── Miff (or Miff.exe)
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
    my $script_dir = $tools_dir;
    $script_dir = normalize_path($script_dir) if $script_dir;
    die "detect_paths: tools_dir not set; init_tools_paths must run first" unless $script_dir;

    $root_dir = $script_dir;

    # Layout: <root>/client/tools/buildACM.pl (e.g. Titan on Windows)
    if ($root_dir =~ m{^(.+)/client/tools$}i)
    {
        $root_dir = $1;
    }
    # Layout: <root>/tools/buildACM.pl (e.g. Linux swg-main)
    elsif ($root_dir =~ m{^(.+)/tools$}i)
    {
        $root_dir = $1;
    }
    else
    {
        # Walk up until we find a tree with dsrc/
        my $p = $script_dir;
        while (defined $p && length $p)
        {
            if (-d "$p/dsrc")
            {
                $root_dir = $p;
                last;
            }
            my $next = dirname($p);
            last if ($next eq $p);
            $p = $next;
        }
    }

    print_debug("Detected root directory: $root_dir") if $opt_debug;
    
    # Compiled shared-game tree (appearance/shader/… templates). Despite the name,
    # $dsrc_base may point at data/.../sys.shared/compiled/game when art lives there.
    if ($opt_dsrc_path)
    {
        $dsrc_base = normalize_path($opt_dsrc_path);
    }
    else
    {
        $dsrc_base = pick_default_shared_compiled_game_dir();
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
        $perllib_dir = "$tools_dir/perllib";
    }
    
    # Add perllib to @INC
    if (-d $perllib_dir)
    {
        unshift @INC, $perllib_dir;
    }
    
    assign_miff_compiler_from_search();
    
    # Validate paths
    my $valid = 1;
    
    if (!-d $dsrc_base)
    {
        print_warning("Compiled game (asset/template) path not found: $dsrc_base");
        $valid = 0;
    }
    
    if (!-d $perllib_dir)
    {
        print_warning("Perllib path not found: $perllib_dir");
        # Continue anyway, might work without it
    }
    
    if (!miff_compiler_is_usable())
    {
        print_warning("Miff compiler not found (gather/collect/MIF will still run). "
            . "Add build/bin/Miff or exe/linux/Miff, put Miff on PATH, or pass --miff for the .iff compile step.");
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

sub miff_path_is_runnable
{
    my $c = shift;
    return 0 unless defined $c && length $c;
    return 0 unless -e $c || -l $c;
    return 1 if $^O eq 'MSWin32';
    return -x $c;
}

sub miff_compiler_is_usable
{
    return miff_path_is_runnable($miff_compiler);
}

sub assign_miff_compiler_from_search
{
    $miff_compiler = "";

    if ($opt_miff_path)
    {
        my $p = normalize_path($opt_miff_path);
        $miff_compiler = $p if $p && (-e $p || -l $p);
        return;
    }

    my @miff_locations = (
        "$root_dir/build/bin/Miff",
        "$root_dir/exe/win32/Miff.exe",
        "$root_dir/exe/linux/Miff",
        "$tools_dir/Miff.exe",
        "$tools_dir/Miff",
        "$root_dir/client/tools/Miff.exe",
        "$root_dir/client/tools/Miff",
        "Miff.exe",
        "Miff",
    );

    foreach my $loc (@miff_locations)
    {
        next unless defined $loc && length $loc;
        next unless -e $loc || -l $loc;
        my $c = normalize_path($loc);
        next unless miff_path_is_runnable($c);
        $miff_compiler = $c;
        last;
    }

    if (!miff_compiler_is_usable() && $^O ne 'MSWin32')
    {
        foreach my $name (qw(Miff miff))
        {
            chomp(my $which = qx{command -v $name 2>/dev/null});
            next unless $which && (-e $which || -l $which);
            my $c = normalize_path($which);
            next unless miff_path_is_runnable($c);
            $miff_compiler = $c;
            last;
        }
    }

    $miff_compiler = "" unless miff_compiler_is_usable();
}

# Default sys.shared compiled/game directory: many trees keep templates under data/, not dsrc/.
# If both exist, prefer the tree that has customization/vehicle_customizations.tab (ACM vehicle pass).
sub pick_default_shared_compiled_game_dir
{
    my @candidates = (
        "$root_dir/data/sku.0/sys.shared/compiled/game",
        "$root_dir/dsrc/sku.0/sys.shared/compiled/game",
    );
    my @existing;
    for my $c (@candidates)
    {
        my $n = normalize_path($c);
        next unless $n && -d $n;
        push @existing, $n;
    }
    return normalize_path($candidates[1]) if !@existing;

    my $vehicle_tab = "customization/vehicle_customizations.tab";
    for my $n (@existing)
    {
        return $n if -f "$n/$vehicle_tab";
    }
    return $existing[0];
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

# Linux: warn if soft RLIMIT_FSIZE is finite and small (collect .dat often > 2GB).
sub acm_warn_if_rlimit_fsize_low
{
    return unless $^O eq 'linux';
    open(my $lim, '<', '/proc/self/limits') or return;
    while (<$lim>)
    {
        next unless /Max file size\s+(\S+)\s+(\S+)\s+bytes/;
        my $soft = $1;
        last if $soft eq 'unlimited';
        my $s = int($soft);
        my $four_gib = 4 * 1024 * 1024 * 1024;
        if ($s > 0 && $s < $four_gib)
        {
            print STDERR "buildACM: WARNING: Max file size (RLIMIT_FSIZE / ulimit -f) is only $s bytes.\n";
            print STDERR "buildACM: Collect output can exceed that (multi-GB). Run: ulimit -f unlimited\n";
            print STDERR "buildACM: before: perl buildACM.pl ...\n";
        }
        last;
    }
    close($lim);
}

# sku.0-style trees and Linux serverdata/ to scan for ACM template extensions.
sub acm_gather_scan_roots
{
    my @roots;
    my $push_root = sub {
        my ($dir) = @_;
        return unless $dir && -d $dir;
        $dir = normalize_path($dir);
        push @roots, $dir unless grep { $_ eq $dir } @roots;
    };

    $push_root->("$root_dir/data/sku.0");
    $push_root->("$root_dir/dsrc/sku.0");
    # Linux swg-main often keeps loose templates under <root>/serverdata (e.g. /home/swg/swg-main/serverdata).
    if (!defined $ENV{SWG_ACM_SCAN_SERVERDATA} || $ENV{SWG_ACM_SCAN_SERVERDATA} ne '0')
    {
        $push_root->("$root_dir/serverdata");
    }
    if (length $ENV{SWG_ACM_SERVERDATA})
    {
        $push_root->( $ENV{SWG_ACM_SERVERDATA} );
    }

    if (!@roots && $dsrc_base && -d $dsrc_base)
    {
        $push_root->( normalize_path($dsrc_base) );
    }
    return @roots;
}

# Map a discovered file to (TreeFile p base, game-relative path). Prefer */compiled/game/*.
sub acm_template_p_and_rel
{
    my ($full_path, $scan_root) = @_;
    $full_path = normalize_path($full_path);
    $scan_root = normalize_path($scan_root);

    if ($full_path =~ m!(.+)/compiled/game/(.+)$!)
    {
        my $p_base = normalize_path($1 . '/compiled/game');
        my $rel    = $2;
        $rel =~ s!^/+!!;
        return ($p_base, $rel) if length $p_base && length $rel;
    }
    if (length $scan_root && index($full_path, $scan_root) == 0)
    {
        my $rel = substr($full_path, length $scan_root);
        $rel =~ s!^/+!!;
        return ($scan_root, $rel) if length $rel;
    }
    return ();
}

sub acm_pick_best_template_candidate
{
    my (@opts) = @_;
    return $opts[0] unless @opts > 1;
    my ($pick) = grep { $_->{full} =~ m{/sys\.client/}i } @opts;
    return $pick if $pick;
    ($pick) = grep { $_->{full} =~ m{/sys\.shared/}i } @opts;
    return $pick if $pick;
    return (sort { $a->{full} cmp $b->{full} } @opts)[0];
}

sub init_tools_paths
{
    $tools_dir = normalize_path(dirname(abs_path(__FILE__)));
    $lookup_file         = "$tools_dir/acm_lookup.dat";
    $collect_result_file = "$tools_dir/acm_collect_result.dat";
    $optimized_file      = "$tools_dir/acm_collect_result-optimized.dat";
}

# Skip expensive collection/optimization when --custinfo or --fast finds a suitable .dat file.
sub resolve_cached_optimized_input
{
    $skip_collect_and_optimize = 0;
    $mif_input_file = "";

    if ($opt_custinfo_path)
    {
        my $p = normalize_path($opt_custinfo_path);
        if (!-f $p)
        {
            print_error("custinfo file not found: $p");
            exit(1);
        }
        $mif_input_file = $p;
        $skip_collect_and_optimize = 1;
        print_info("Using --custinfo for MIF input: $mif_input_file") if $opt_verbose;
        return;
    }

    if ($opt_fast)
    {
        for my $loc (
            "$tools_dir/custinfo-raw-optimized.dat",
            "$tools_dir/acm_collect_result-optimized.dat",
        )
        {
            next unless -f $loc;
            $mif_input_file = normalize_path($loc);
            $skip_collect_and_optimize = 1;
            print_info("Fast path: using cached optimized data $mif_input_file") if $opt_verbose;
            return;
        }
        print_info("--fast: no custinfo-raw-optimized.dat or acm_collect_result-optimized.dat in $tools_dir; running full collect/optimize.") if $opt_verbose;
    }
}

sub print_config
{
    print "\nConfiguration:\n";
    print "  Root:     $root_dir\n";
    print "  Tools:    $tools_dir\n";
    print "  Assets:   $dsrc_base  (compiled/game; datatables/MIF; --dsrc)\n";
    print "  ACM scan: data/sku.0 + dsrc/sku.0 + serverdata/ (+ SWG_ACM_SERVERDATA if set)\n";
    print "  Data:     $data_base\n";
    print "  Perllib:  $perllib_dir\n";
    print "  Miff:     " . ($miff_compiler ? $miff_compiler : "(not found; use --miff for .iff)") . "\n";
    if ($skip_collect_and_optimize)
    {
        print "  MIF in:   $mif_input_file (cached)\n";
    }
    print "\n";
}

# ======================================================================
# Step 1: Gather Asset Files
# ======================================================================

sub gather_asset_files
{
    # Scan entire data/sku.0 and dsrc/sku.0 (when present) for ACM extensions; de-dupe on game-relative name.
    my @scan_roots = acm_gather_scan_roots();
    unless (@scan_roots)
    {
        return ();
    }

    my %ext_hash = map { $_ => 1 } @asset_extensions;
    my %by_rel;

    for my $root (@scan_roots)
    {
        find(
            {
                wanted => sub {
                    return unless -f $File::Find::name;
                    my $ext = "";
                    if ($File::Find::name =~ /(\.[^.]+)$/)
                    {
                        $ext = lc($1);
                    }
                    return unless $ext_hash{$ext};

                    my $full_path = normalize_path($File::Find::name);
                    my ($p_base, $rel) = acm_template_p_and_rel($full_path, $root);
                    return unless length $rel && length $p_base;

                    push @{ $by_rel{$rel} }, { p_base => $p_base, full => $full_path };
                    print_debug("  Found: $full_path -> $rel") if $opt_debug;
                },
                no_chdir => 1,
            },
            $root
        );
    }

    my @records;
    for my $rel (keys %by_rel)
    {
        my $best = acm_pick_best_template_candidate(@{ $by_rel{$rel} });
        push @records, {
            rel    => $rel,
            p_base => $best->{p_base},
            full   => $best->{full},
        };
    }

    return sort { $a->{rel} cmp $b->{rel} } @records;
}

sub create_lookup_file
{
    my $records_ref = shift;
    
    unlink $lookup_file if -e $lookup_file;
    
    open(my $fh, '>', $lookup_file) or do {
        print_error("Cannot create lookup file: $lookup_file - $!");
        return 0;
    };

    # One or more p roots (client/shared compiled/game trees under sku.0).
    my %p_index;
    my @p_order;
    for my $rec (@$records_ref)
    {
        my $p = $rec->{p_base};
        next unless length $p;
        if (!exists $p_index{$p})
        {
            $p_index{$p} = scalar @p_order;
            push @p_order, $p;
        }
    }

    if (!@p_order && $dsrc_base && -d $dsrc_base)
    {
        my $p = normalize_path($dsrc_base);
        push @p_order, $p;
        $p_index{$p} = 0;
    }

    for my $p (@p_order)
    {
        print $fh "p $p/:0\n";
    }

    for my $rec (@$records_ref)
    {
        my $idx = $p_index{ $rec->{p_base} };
        unless (defined $idx)
        {
            print_error("create_lookup_file: missing p index for $rec->{p_base}");
            close($fh);
            return 0;
        }
        print $fh "e $rec->{rel}:$idx\n";
    }
    
    close($fh);
    
    print_info("Created lookup file with " . scalar(@$records_ref) . " entries, " . scalar(@p_order) . " path root(s).") if $opt_verbose;
    
    return 1;
}

# ======================================================================
# Step 2: Collect Customization Info
# ======================================================================

# Optional lines from dsrc customization/force_add_variable_usage.dat (comments and blanks skipped).
sub acm_append_force_add_to_fh
{
    my ($fh) = @_;

    my $force_add_file = "$dsrc_base/customization/force_add_variable_usage.dat";
    return 1 unless -f $force_add_file;

    print_info("Appending force_add_variable_usage.dat") if $opt_verbose;
    open(my $fa_fh, '<', $force_add_file) or do {
        print_warning("Cannot read: $force_add_file - $!");
        return 1;
    };
    while (<$fa_fh>)
    {
        next if /^\s*#/;
        next if /^\s*$/;
        print $fh $_;
    }
    close($fa_fh);
    return 1;
}

# Unix default batches template collect in fresh Perl processes (memory resets). Windows: one process unless set.
sub acm_collect_batch_size
{
    my $e = $ENV{SWG_ACM_COLLECT_BATCH};
    if (!defined $e || $e eq '')
    {
        return ($^O eq 'MSWin32') ? 0 : 10000;
    }
    return 0 if $e =~ /^0+$/;
    return int($e) if $e =~ /^[0-9]+$/;
    return 0;
}

sub collect_customization_info_batched
{
    my ($records_ref, $batch) = @_;

    my $script = "$tools_dir/collectAcmTemplatesOnly.pl";
    if (!-f $script)
    {
        print_error("collectAcmTemplatesOnly.pl not found: $script");
        return 0;
    }

    unlink $collect_result_file if -e $collect_result_file;

    my $n  = scalar @$records_ref;
    my $nb = int(($n + $batch - 1) / $batch);

    acm_warn_if_rlimit_fsize_low();

    my $prog_n = $ENV{SWG_ACM_PROGRESS_EVERY} // 500;
    print STDERR "buildACM: batched template collect: $nb fresh Perl run(s), up to $batch assets each; "
        . "each run loads the full lookup [$lookup_file] and a path filter (TreeFile resolution stays valid). "
        . "SWG_ACM_COLLECT_BATCH=0 uses one in-process collect. Progress every $prog_n applies inside each batch.\n";

    my $filter_file = "$tools_dir/acm_collect_batch_filter.rel";

    for my $i (0 .. $nb - 1)
    {
        my $from = $i * $batch;
        my $to   = $from + $batch - 1;
        $to = $n - 1 if $to >= $n;
        my @chunk = @{$records_ref}[ $from .. $to ];

        open(my $ff, '>', $filter_file) or do {
            print_error("Cannot write batch filter: $filter_file - $!");
            return 0;
        };
        print {$ff} "$_->{rel}\n" for grep { length $_->{rel} } @chunk;
        close($ff) or do {
            print_error("Cannot close $filter_file: $!");
            return 0;
        };

        print STDERR "buildACM: starting template batch " . ($i + 1) . "/$nb (" . scalar(@chunk) . " lookup rows).\n";

        my @cmd = ($^X);
        push @cmd, "-I$perllib_dir" if $perllib_dir && -d $perllib_dir;
        push @cmd, $script;
        push @cmd, '--truncate' if $i == 0;
        push @cmd, '--filter', $filter_file;
        push @cmd, $lookup_file, $collect_result_file;

        my $rc = system(@cmd);
        if ($rc == -1)
        {
            print_error("Failed to start batch subprocess: $!");
            unlink $collect_result_file if -e $collect_result_file;
            return 0;
        }
        my $exit = $rc >> 8;
        if ($exit != 0)
        {
            print_error("Template collect batch " . ($i + 1) . "/$nb failed (exit $exit).");
            unlink $collect_result_file if -e $collect_result_file;
            return 0;
        }
    }

    open(my $fh, '>>', $collect_result_file) or do {
        print_error("Cannot append for vehicle pass: $collect_result_file - $!");
        return 0;
    };

    my ($ok, $err);
    {
        local $ENV{SWG_DATATABLES_PATH} = $dsrc_base;
        print STDERR "buildACM: vehicle customization pass (in buildACM process)...\n";
        $ok = eval {
            CollectAcmInfo::collect_vehicle_acm_info_to_fh(
                fh     => $fh,
                branch => 'current',
            );
            1;
        };
        $err = $@;
    }

    if (!$ok || $err)
    {
        close($fh);
        unlink $collect_result_file if -e $collect_result_file;
        print STDERR "buildACM: vehicle collect failed: $err\n";
        print_error("Vehicle customization collect failed: $err");
        return 0;
    }

    acm_append_force_add_to_fh($fh);

    close($fh) or do {
        print_error("Cannot close $collect_result_file: $!");
        return 0;
    };

    print STDERR "buildACM: collect phase done; continuing pipeline.\n";
    print_info("Collect phase finished (MGN warnings and \"iff error ... skipping\" on stderr are usually non-fatal).");

    return 1;
}

sub collect_customization_info
{
    my $records_ref = shift;

    eval { require CollectAcmInfo; 1 } or do {
        print_error("Failed to load CollectAcmInfo: $@");
        return 0;
    };

    my $batch = acm_collect_batch_size();
    if ($batch > 0 && $records_ref && @$records_ref)
    {
        return collect_customization_info_batched($records_ref, $batch);
    }

    open(my $fh, '>', $collect_result_file) or do {
        print_error("Cannot create result file: $collect_result_file - $!");
        return 0;
    };
    acm_warn_if_rlimit_fsize_low();

    my $ok;
    my $err;
    {
        local $ENV{SWG_DATATABLES_PATH} = $dsrc_base;
        my $prog_n = $ENV{SWG_ACM_PROGRESS_EVERY} // 500;
        print STDERR "buildACM: starting collect phase (many files -> minutes of work; "
            . "progress every $prog_n; SWG_ACM_PROGRESS_EVERY=0 silences; "
            . "SWG_ACM_MAX_TEMPLATE_BYTES caps per-file read size, 0=unlimited; "
            . "SWG_ACM_FORK_EACH=1 enables slow per-file fork only if needed for OOM; "
            . "SWG_ACM_SKIP_CHILD_ERRORS=0 makes per-template failures fatal (fork + in-process); "
            . "on Unix default SWG_ACM_COLLECT_BATCH=10000 uses subprocess batches — set to 0 for single process).\n";
        $ok = eval {
            CollectAcmInfo::collect_acm_info_to_fh(
                lookup => $lookup_file,
                fh     => $fh,
                branch => 'current',
            );
            1;
        };
        $err = $@;
    }

    if (!$ok || $err)
    {
        close($fh);
        unlink $collect_result_file if -e $collect_result_file;
        print STDERR "buildACM: collect failed: $err\n";
        print_error("Customization collect failed: $err");
        return 0;
    }

    acm_append_force_add_to_fh($fh);

    close($fh) or do {
        print_error("Cannot close $collect_result_file: $!");
        return 0;
    };

    print STDERR "buildACM: collect phase done; continuing pipeline.\n";
    print_info("Collect phase finished (MGN warnings and \"iff error ... skipping\" on stderr are usually non-fatal).");

    return 1;
}

# ======================================================================
# Optimize collect output and emit ACM/CIM MIF (single Perl process)
# ======================================================================

sub optimize_and_build_mif_single_pass
{
    my $build_script = "$tools_dir/buildAssetCustomizationManagerData.pl";
    
    if (!-f $build_script)
    {
        print_error("Build script not found: $build_script");
        return 0;
    }

    my $acm_mif = "$dsrc_base/customization/asset_customization_manager.mif";
    my $cim_mif = "$dsrc_base/customization/customization_id_manager.mif";
    my $custom_dir = "$dsrc_base/customization";
    make_path($custom_dir) unless -d $custom_dir;
    
    my $cmd = "perl";
    $cmd .= " -I\"$perllib_dir\"" if -d $perllib_dir;
    $cmd .= " \"$build_script\"";
    $cmd .= " -i \"$collect_result_file\"";
    $cmd .= " -r";
    $cmd .= " -o \"$acm_mif\"";
    $cmd .= " -m \"$cim_mif\"";
    $cmd .= " -t \"$lookup_file\"";
    $cmd .= " -d" if $opt_debug;
    
    print_debug("Executing: $cmd") if $opt_debug;
    
    my $output = `$cmd 2>&1`;
    my $exit_code = $? >> 8;
    
    if ($exit_code != 0)
    {
        print_error("Optimize+MIF pipeline failed (exit code: $exit_code)");
        print_error($output) if $output;
        return 0;
    }
    
    print $output if $opt_verbose && $output;
    
    if (!-f $optimized_file)
    {
        print_error("Optimized file was not created: $optimized_file");
        return 0;
    }
    if (!-f $acm_mif || !-f $cim_mif)
    {
        print_error("MIF output missing after pipeline.");
        return 0;
    }
    
    return 1;
}

# ======================================================================
# Build MIF from existing optimized .dat (e.g. --fast / --custinfo)
# ======================================================================

sub build_mif_files
{
    my $build_script = "$tools_dir/buildAssetCustomizationManagerData.pl";
    
    my $acm_mif = "$dsrc_base/customization/asset_customization_manager.mif";
    my $cim_mif = "$dsrc_base/customization/customization_id_manager.mif";
    
    # Ensure customization directory exists
    my $custom_dir = "$dsrc_base/customization";
    make_path($custom_dir) unless -d $custom_dir;
    
    if (!$mif_input_file || !-f $mif_input_file)
    {
        print_error("MIF input missing: " . ($mif_input_file || "(none)"));
        return 0;
    }

    # Build command for MIF generation
    my $cmd = "perl";
    $cmd .= " -I\"$perllib_dir\"" if -d $perllib_dir;
    $cmd .= " \"$build_script\"";
    $cmd .= " -i \"$mif_input_file\"";
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

    if (!miff_compiler_is_usable())
    {
        assign_miff_compiler_from_search();
    }

    if (!miff_compiler_is_usable())
    {
        if ($miff_compiler && (-e $miff_compiler || -l $miff_compiler) && $^O ne 'MSWin32' && !-x $miff_compiler)
        {
            print_error("Miff is not executable: $miff_compiler (try: chmod +x $miff_compiler)");
        }
        else
        {
            print_error("Miff compiler not found. On Linux use <root>/build/bin/Miff (or exe/linux/Miff), "
                . "put Miff on PATH, or run with --miff /full/path/to/Miff");
        }
        return 0;
    }
    
    if (!-f $mif_file)
    {
        print_error("$name MIF file not found: $mif_file");
        return 0;
    }

    # Avoid shell interpolation (sh -c): paths or CRLF in --miff can break quoting and
    # make Miff's CommandLine parser return ERR_OPTIONS (exit 246 == unsigned -10).
    for my $p ($miff_compiler, $mif_file, $iff_file)
    {
        $p =~ tr/\r//d if defined $p;
    }

    my @miff_cmd = ($miff_compiler, '-i', $mif_file, '-o', $iff_file);
    print_debug("Executing: " . join(' ', map { qq{"$_"} } @miff_cmd)) if $opt_debug;

    my $err = gensym;
    my ($chld_in, $chld_out);
    my $pid = eval { open3($chld_in, $chld_out, $err, @miff_cmd) };
    if (!$pid || $@)
    {
        print_error("$name: could not run Miff: " . ($@ || $!));
        return 0;
    }
    close $chld_in;
    local $/;
    my $stdout = <$chld_out>;
    close $chld_out if $chld_out;
    my $stderr = <$err>;
    close $err if $err;
    waitpid($pid, 0);
    my $output = (defined $stdout ? $stdout : '') . (defined $stderr ? $stderr : '');
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
        "$tools_dir/acm_collect_batch_filter.rel",
    );

    # After a --fast / --custinfo run, keep the optimized .dat we used so the next --fast still works.
    my $preserve = ($skip_collect_and_optimize && $mif_input_file)
        ? normalize_path($mif_input_file) : "";

    foreach my $file (@temp_files)
    {
        next unless -f $file;
        if ($preserve && normalize_path($file) eq $preserve)
        {
            print_debug("Preserving cached MIF input: $file") if $opt_debug;
            next;
        }
        unlink $file;
        print_debug("Removed: $file") if $opt_debug;
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

sys.shared (or primary) F<compiled/game> directory used for vehicle/datatables paths, customization/ MIF
output, and C<SWG_DATATABLES_PATH> during collect. Template discovery also recursively scans F<data/sku.0/>, F<dsrc/sku.0/>, and F<serverdata/>
under the repo root (typical Linux: F</home/swg/swg-main/serverdata/>) for ACM file types,
normalizing names under F<compiled/game/>. Override or add a path with C<SWG_ACM_SERVERDATA>.
If omitted: prefers F<data/sku.0/sys.shared/compiled/game> when present, else F<dsrc/sku.0/sys.shared/compiled/game>; if I<both> exist, the tree that contains F<customization/vehicle_customizations.tab> is used (vehicle ACM data often lives only under F<dsrc/>).

=item B<--data PATH>

Specify the data output path manually.

=item B<--miff PATH>

Specify the path to the Miff compiler.

=item B<--perllib PATH>

Specify the path to the Perl library directory.

=item B<--clean>

Clean previous build artifacts before building.

=item B<--fast>

Skip collect and optimize when a cached optimized file exists in the tools directory (same folder as C<buildACM.pl>)
(C<custinfo-raw-optimized.dat> or C<acm_collect_result-optimized.dat>).

=item B<--custinfo PATH>

Use the given optimized raw data file as MIF input; skips collect and optimize.

=item B<--keep-temp>

Do not delete intermediate files under the tools directory after a successful run.

=back

=head1 ENVIRONMENT

Collect phase only. C<SWG_ACM_PROGRESS_EVERY> (default 500): stderr progress interval; C<0> disables. Stderr logs the first template and then periodic progress.
C<SWG_ACM_MAX_TEMPLATE_BYTES> (default about 200MiB): skip larger files before reading into memory (avoids OOM kills); C<0> disables the cap.

When stderr is piped (for example C<2>&1 | tee acm-build.log>), ensure the disk holding the log has free space: if C<tee> exits, the parent perl can receive C<SIGPIPE> on the next stderr write. C<buildACM.pl> sets C<$SIG{PIPE}> to C<IGNORE> so the run can continue; check C<tee>'s exit status separately if needed.

Collect writes a large intermediate file under the tools directory (often multiple GB). If the shell C<ulimit -f> (max file size) is not C<unlimited>, the run can stop mid-collect with no useful message. Use C<ulimit -f unlimited> before running. C<buildACM.pl> sets C<$SIG{XFSZ}> to C<IGNORE> where supported; raising C<ulimit -f> is still recommended.

By default templates are processed in-process (fast). Set C<SWG_ACM_FORK_EACH=1> on Unix to process each file in a child (slow: hours on huge trees) only when one bad asset otherwise OOM-kills the whole perl.

C<SWG_ACM_SKIP_CHILD_ERRORS> (default C<1>): on a per-template failure, log and continue. Applies both to forked children (C<SWG_ACM_FORK_EACH=1>) and to in-process collection (normal and batched runs). Set C<SWG_ACM_SKIP_CHILD_ERRORS=0> to fail the whole collect (strict).

C<SWG_ACM_COLLECT_BATCH>: on Unix, template collection runs in multiple fresh Perl subprocesses of at most this many lookup rows (default C<10000>), appending to the raw collect file. Each subprocess loads the I<full> lookup table and an allow-list filter so linked assets still resolve. Vehicle lines and force-add are applied once in the main C<buildACM.pl> process. Set C<0> for a single in-process collect (faster when RAM is not a limit). On Windows the default is C<0>.

Set C<SWG_ACM_SCAN_SERVERDATA=0> to skip scanning F<< <root>/serverdata >> during template discovery (fewer files and fewer invalid-IFF skips if you do not need that tree for ACM).

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
                     --miff "/home/swg/swg-main/build/bin/Miff" \
                     --verbose

=head1 AUTHOR

SWG Development Team

=cut
