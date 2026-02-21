# buildACM.pl - Asset Customization Manager Build Script
## Overview

`buildACM.pl` is a unified, cross-platform Perl script that handles the complete
build process for generating ACM (Asset Customization Manager) and CIM 
(Customization ID Manager) IFF files.

## Features

- **Cross-Platform**: Works on both Windows (D:/titan/...) and Linux (/home/swg/...)
- **Auto-Detection**: Automatically detects paths based on script location
- **Self-Contained**: Single script handles entire build process
- **No Perforce**: Does not require Perforce or any version control system
- **Verbose Output**: Optional detailed progress information
- **Clean Builds**: Option to clean previous artifacts before building

## Requirements

- Perl 5.x
- Existing Perl modules in `client/tools/perllib/`:
  - TreeFile.pm
  - CustomizationVariableCollector.pm
  - VehicleCustomizationVariableGenerator.pm
  - AppearanceTemplate.pm (and related template modules)
- Miff compiler (Miff.exe on Windows, Miff on Linux)
- Existing asset files in dsrc directory

## Usage

### Basic Usage (Auto-Detection)

```bash
perl buildACM.pl --verbose
```

### Manual Path Specification

#### Windows:
```powershell
perl buildACM.pl `
    --dsrc "D:/titan/dsrc/sku.0/sys.shared/compiled/game" `
    --data "D:/titan/data/sku.0/sys.client/compiled/game" `
    --verbose
```

#### Linux:
```bash
perl buildACM.pl \
    --dsrc "/home/swg/swg-main/dsrc/sku.0/sys.shared/compiled/game" \
    --data "/home/swg/swg-main/data/sku.0/sys.client/compiled/game" \
    --verbose
```

### Full Manual Specification

```bash
perl buildACM.pl \
    --dsrc "/path/to/dsrc/sku.0/sys.shared/compiled/game" \
    --data "/path/to/data/sku.0/sys.client/compiled/game" \
    --miff "/path/to/Miff" \
    --perllib "/path/to/client/tools/perllib" \
    --verbose
```

## Command Line Options

| Option | Description |
|--------|-------------|
| `-v, --verbose` | Enable verbose output |
| `-d, --debug` | Enable debug output (more detailed) |
| `-h, --help` | Show help message |
| `--version` | Show version information |
| `--dsrc <path>` | Path to dsrc base directory |
| `--data <path>` | Path to data output directory |
| `--miff <path>` | Path to Miff compiler |
| `--perllib <path>` | Path to Perl library directory |
| `--clean` | Clean previous artifacts before building |

## Build Process

The script performs 6 steps:

1. **Gather Files**: Scans for asset files (.apt, .cmp, .lmg, .lod, .lsb, .mgn, .msh, .sat, .pob, .sht, .trt)
2. **Collect Info**: Runs collectAssetCustomizationInfo.pl to gather customization data
3. **Optimize**: Runs buildAssetCustomizationManagerData.pl with -r flag to optimize
4. **Build MIF**: Generates asset_customization_manager.mif and customization_id_manager.mif
5. **Compile IFF**: Uses Miff compiler to convert .mif to .iff
6. **Cleanup**: Removes temporary working files

## Expected Directory Structure

```
<root>/
├── dsrc/
│   └── sku.0/
│       └── sys.shared/
│           └── compiled/
│               └── game/
│                   ├── appearance/      <- Asset files here
│                   ├── shader/          <- Asset files here
│                   ├── texturerenderer/ <- Asset files here
│                   ├── customization/   <- MIF files generated here
│                   │   ├── asset_customization_manager.mif
│                   │   ├── customization_id_manager.mif
│                   │   ├── force_add_variable_usage.dat (optional)
│                   │   └── vehicle_customizations.tab
│                   └── datatables/
│                       └── mount/
│                           ├── logical_saddle_name_map.tab
│                           └── saddle_appearance_map.tab
├── data/
│   └── sku.0/
│       └── sys.client/
│           └── compiled/
│               └── game/
│                   └── customization/   <- IFF files generated here
│                       ├── asset_customization_manager.iff
│                       └── customization_id_manager.iff
├── client/
│   └── tools/
│       ├── perllib/                     <- Perl modules
│       ├── collectAssetCustomizationInfo.pl
│       ├── buildAssetCustomizationManagerData.pl
│       └── buildACM.pl                  <- This script
└── exe/
    ├── win32/
    │   └── Miff.exe
    └── linux/
        └── Miff
```

## Output Files

| File | Location | Description |
|------|----------|-------------|
| asset_customization_manager.iff | data/.../customization/ | ACM binary file |
| customization_id_manager.iff | data/.../customization/ | CIM binary file |

## Troubleshooting

### "DSRC path not found"
- Verify the dsrc directory exists
- Use `--dsrc` to specify the correct path

### "Miff compiler not found"
- Ensure Miff.exe (Windows) or Miff (Linux) exists
- Use `--miff` to specify the exact path

### "Collect script failed"
- Ensure perllib modules are available
- Use `--perllib` to specify the correct path
- Run with `--debug` for more details

### "MIF generation failed"
- Check that the optimized data file was created
- Run with `--debug` to see exact errors

### "Compilation failed"
- Verify Miff compiler is executable
- Check that output directory is writable
- Run with `--verbose` to see Miff output

## Example Session

```
$ perl buildACM.pl --verbose

============================================================
buildACM.pl - Asset Customization Manager Build Script
Version 1.0.0
============================================================

Configuration:
  Root:     D:/titan
  DSRC:     D:/titan/dsrc/sku.0/sys.shared/compiled/game
  Data:     D:/titan/data/sku.0/sys.client/compiled/game
  Perllib:  D:/titan/client/tools/perllib
  Miff:     D:/titan/exe/win32/Miff.exe

[1/6] Gathering asset files and creating lookup table...
  INFO: Found 15234 asset files.
[2/6] Collecting asset customization information...
[3/6] Optimizing customization data...
[4/6] Building ACM and CIM .mif files...
  INFO: Created ACM MIF: .../asset_customization_manager.mif
  INFO: Created CIM MIF: .../customization_id_manager.mif
[5/6] Compiling .mif files to .iff...
  INFO: Compiled ACM: .../asset_customization_manager.iff
  INFO: Compiled CIM: .../customization_id_manager.iff
[6/6] Cleaning up temporary files...

============================================================
SUCCESS: ACM and CIM build completed successfully!
============================================================
```

## License

Copyright Sony Online Entertainment / SWG Development Team
