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
- Existing Perl modules in `tools/perllib/` (same directory level as `buildACM.pl`; on some Windows trees this is `client/tools/perllib/`):
  - TreeFile.pm
  - CustomizationVariableCollector.pm
  - VehicleCustomizationVariableGenerator.pm
  - AppearanceTemplate.pm (and related template modules)
- Miff compiler (Miff.exe on Windows, Miff on Linux)
- Template/asset IFFs under **`sys.shared/compiled/game`** (often `data/sku.0/.../game` when art is staged there, or `dsrc/...`; see defaults below)
- **Vehicle mount tables**: `customization/vehicle_customizations.tab` (and related mount tabs) under the same compiled-game tree as `--dsrc`. For standalone `collectAssetCustomizationInfo.pl`, set **`SWG_DATATABLES_PATH`** to that directory if auto-detect fails.

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

#### Linux (templates under `data/`):

```bash
perl buildACM.pl \
    --dsrc "/home/swg/swg-main/data/sku.0/sys.shared/compiled/game" \
    --data "/home/swg/swg-main/data/sku.0/sys.client/compiled/game" \
    --verbose
```

If you omit `--dsrc`, the script picks **`data/.../sys.shared/compiled/game`** when that folder exists, else **`dsrc/.../sys.shared/compiled/game`**.

### Full Manual Specification

```bash
perl buildACM.pl \
    --dsrc "/path/to/dsrc/sku.0/sys.shared/compiled/game" \
    --data "/path/to/data/sku.0/sys.client/compiled/game" \
    --miff "/path/to/Miff" \
    --perllib "/path/to/tools/perllib" \
    --verbose
```

## Command Line Options


| Option              | Description                                                                                    |
| ------------------- | ---------------------------------------------------------------------------------------------- |
| `-v, --verbose`     | Enable verbose output                                                                          |
| `-d, --debug`       | Enable debug output (more detailed)                                                            |
| `-h, --help`        | Show help message                                                                              |
| `--version`         | Show version information                                                                       |
| `--dsrc <path>`     | **sys.shared compiled/game** root (templates + `customization/` `.tab` files). Not passed: use **`data/.../game`** if that directory exists, otherwise **`dsrc/.../game`**. |
| `--data <path>`     | Path to data output directory                                                                  |
| `--miff <path>`     | Path to Miff compiler                                                                          |
| `--perllib <path>`  | Path to Perl library directory                                                                 |
| `--clean`           | Clean previous artifacts before building                                                       |
| `--fast`            | Skip collect + optimize if a cached optimized `.dat` exists in the tools directory (see below) |
| `--custinfo <path>` | Use this optimized raw file for MIF generation; skips collect + optimize                       |
| `--keep-temp`       | Keep `acm_lookup.dat`, collect result, and optimized intermediates next to `buildACM.pl`       |


**Intermediate files** are always written in the **tools directory** that contains `buildACM.pl` (e.g. `/home/swg/swg-main/tools` on Linux, or `client/tools` on some Windows layouts), not the shell’s current directory.

**Fast rebuild**: Place one of these files in that tools directory and run with `--fast`:

- `custinfo-raw-optimized.dat`
- `acm_collect_result-optimized.dat` (leftover from a full run with `--keep-temp`, or copied)

Or pass an explicit file with `--custinfo`. After a successful `--fast` / `--custinfo` run, the optimized `.dat` you used is **not** deleted at the end (unless `--clean` removed it earlier).

**Windows**: `set BUILDACM_NO_PAUSE=1` before `buildACM.bat` to skip interactive `pause` (automation / CI).

## Build Process

**Full** run (5 steps): gather, collect, **optimize + build MIF in one Perl process** (`buildAssetCustomizationManagerData.pl -r -o -m`), compile IFF, cleanup temps.

**Fast** run (4 steps): gather, build MIF (from cached optimized `.dat`), compile IFF, cleanup — when `--fast` or `--custinfo` supplies optimized data.

1. **Gather Files**: Recursively scans all of **`--dsrc`** for asset files (.apt, .cmp, .lmg, .lod, .lsb, .mgn, .msh, .sat, .pob, .sht, .trt) and lists them in the TreeFile lookup (`e` lines). A narrow subtree would leave the lookup empty and break template-driven link collection.
2. **Collect Info**: Loads `perllib/CollectAcmInfo.pm` in-process (same Perl interpreter as `buildACM.pl`) to scan assets and write the raw `.dat`; you can still run `collectAssetCustomizationInfo.pl` standalone for debugging *(skipped in fast mode)*
3. **Optimize + MIF**: Single run of `buildAssetCustomizationManagerData.pl` with `-r`, `-o`, and `-m` *(in fast mode, replaced by one MIF-only run from cached optimized `.dat`)*
4. **Compile IFF**: Uses Miff compiler to convert .mif to .iff
5. **Cleanup**: Removes temporary working files in the tools directory (unless `--keep-temp`)

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
├── tools/                               <- Typical Linux: <root>/tools
│   ├── perllib/                         <- Perl modules
│   ├── collectAssetCustomizationInfo.pl
│   ├── buildAssetCustomizationManagerData.pl
│   └── buildACM.pl                      <- This script
│   (On some Windows trees the same layout is under client/tools/ instead.)
└── exe/
    ├── win32/
    │   └── Miff.exe
    └── linux/
        └── Miff
```

## Output Files


| File                            | Location                | Description     |
| ------------------------------- | ----------------------- | --------------- |
| asset_customization_manager.iff | data/.../customization/ | ACM binary file |
| customization_id_manager.iff    | data/.../customization/ | CIM binary file |


## Troubleshooting

### "DSRC path not found"

- Verify the dsrc directory exists
- Use `--dsrc` to specify the correct path

### "Miff compiler not found"

- Discovery order: `<root>/exe/win32/Miff.exe` or `<root>/exe/linux/Miff`, then the tools directory, then `command -v Miff` / `miff` on Linux **PATH**. On Linux the binary must be **executable** (`chmod +x`).
- You can run gather/collect/MIF without Miff; the script only needs it for the **.mif → .iff** step. Use **`--miff /full/path/to/Miff`** if it is not found automatically. The compile step **re-runs** discovery if needed.

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
Version 1.2.3
============================================================

Configuration:
  Root:     D:/titan
  Tools:    D:/titan/client/tools
  DSRC:     D:/titan/dsrc/sku.0/sys.shared/compiled/game
  Data:     D:/titan/data/sku.0/sys.client/compiled/game
  Perllib:  D:/titan/client/tools/perllib
  Miff:     D:/titan/exe/win32/Miff.exe

[1/5] Gathering asset files and creating lookup table...
  INFO: Found 15234 asset files.
[2/5] Collecting asset customization information...
[3/5] Optimizing customization data and building ACM/CIM MIF files...
  INFO: Created ACM MIF: .../asset_customization_manager.mif
  INFO: Created CIM MIF: .../customization_id_manager.mif
[4/5] Compiling .mif files to .iff...
  INFO: Compiled ACM: .../asset_customization_manager.iff
  INFO: Compiled CIM: .../customization_id_manager.iff
[5/5] Cleaning up temporary files...

============================================================
SUCCESS: ACM and CIM build completed successfully!
============================================================
```

## License

Copyright Sony Online Entertainment / SWG Development Team