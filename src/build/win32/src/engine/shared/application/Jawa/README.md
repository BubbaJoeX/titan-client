# Jawa - Asset Customization Manager Build Tool

Cross-platform C++ tool for building SWG Asset Customization Manager with Perl .pm module support.

## Features

- ✅ **Cross-platform** - Windows & Linux support
- ✅ **Configurable paths** - All tools and paths configurable via command-line or CMake
- ✅ **Perl .pm fix** - Automatically sets PERL5LIB to find .pm modules
- ✅ **Verbose mode** - Detailed output for debugging
- ✅ **5-step ACM build process** - Complete implementation

## Quick Start

### Build

**Windows:**
```cmd
cd src\engine\shared\application\Jawa
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

**Linux:**
```bash
cd src/engine/shared/application/Jawa
mkdir build && cd build
cmake ..
make -j4
```

### Usage

```bash
# Basic usage
./bin/Jawa ../../sku.0/sys.shared/compiled/game/

# With Perl library path (fixes .pm not found issue)
./bin/Jawa --perl-lib ./lib ../../sku.0/sys.shared/compiled/game/

# Verbose mode
./bin/Jawa --verbose --perl-lib /path/to/perl/lib ../../sku.0/sys.shared/compiled/game/

# Custom client output path
./bin/Jawa --client-path ../sys.client/compiled/game/ ../../sku.0/sys.shared/compiled/game/
```

## Perl .pm Module Fix

**Problem:** Perl scripts can't find `.pm` modules even when they exist.

**Solution:** Use `--perl-lib` to specify the path to Perl modules:

```bash
# Windows
Jawa.exe --perl-lib "C:\path\to\perl\lib" <base_path>

# Linux  
./Jawa --perl-lib "/usr/local/lib/perl5" <base_path>
```

This sets the `PERL5LIB` environment variable before running Perl scripts.

## Command-Line Options

| Option | Description |
|--------|-------------|
| `<base_path>` | **Required**. Path to sys.shared/compiled/game/ |
| `--help, -h` | Show help message |
| `--version, -v` | Show version |
| `--verbose` | Enable verbose output |
| `--miff <path>` | Path to Miff compiler |
| `--collect-script <path>` | Path to collectAssetCustomizationInfo.pl |
| `--build-script <path>` | Path to buildAssetCustomizationManagerData.pl |
| `--client-path <path>` | Output path for client .iff files |
| `--perl <cmd>` | Perl interpreter command |
| `--perl-lib <path>` | **Perl library path (PERL5LIB) - fixes .pm issues** |

## CMake Configuration

Set defaults at build time:

```bash
cmake -B build \
  -DJAWA_MIFF_PATH="/usr/local/bin/Miff" \
  -DJAWA_PERL_LIB_PATH="/usr/local/lib/perl5" \
  -DJAWA_CLIENT_PATH="../../sku.0/sys.client/compiled/game/"
  
cmake --build build
```

Available CMake options:
- `JAWA_MIFF_PATH` - Miff compiler path
- `JAWA_COLLECT_SCRIPT` - Collect script path
- `JAWA_BUILD_SCRIPT` - Build script path  
- `JAWA_BASE_PATH` - Default base path
- `JAWA_CLIENT_PATH` - Client output path
- `JAWA_PERL_INTERPRETER` - Perl command
- `JAWA_PERL_LIB_PATH` - Perl library path

## Build Process

Jawa performs a 5-step ACM build:

1. **Create Lookup File** - Gathers appearance/shader/texture files
2. **Collect Asset Info** - Runs collectAssetCustomizationInfo.pl
3. **Optimize Data** - Removes duplicates and errors
4. **Build ACM** - Generates .mif files
5. **Compile** - Converts .mif to .iff files

## Output Files

**Intermediate files** (auto-cleaned):
- `acm_lookup.dat`
- `acm_collect_result.dat`
- `acm_collect_result-optimized.dat`

**Final output:**
- `<base_path>/customization/asset_customization_manager.mif`
- `<base_path>/customization/customization_id_manager.mif`
- `<client_path>/customization/asset_customization_manager.iff`
- `<client_path>/customization/customization_id_manager.iff`

## Troubleshooting

### Perl can't find .pm modules

**Error:** `Can't locate SomeModule.pm in @INC`

**Fix:** Use `--perl-lib` to specify where Perl modules are located:

```bash
# Find your Perl lib path
perl -V:installsitelib

# Use it with Jawa
./Jawa --perl-lib /path/from/above <base_path>
```

### Miff compiler not found

**Fix:** Specify Miff path:
```bash
./Jawa --miff /path/to/Miff <base_path>
```

Or add Miff to PATH.

### Scripts not found

**Fix:** Run from the directory containing the .pl scripts, or use absolute paths:
```bash
./Jawa --collect-script /full/path/collectAssetCustomizationInfo.pl \
       --build-script /full/path/buildAssetCustomizationManagerData.pl \
       <base_path>
```

## Examples

### Basic build with verbose output
```bash
./Jawa --verbose ../../sku.0/sys.shared/compiled/game/
```

### Build with custom Perl library path
```bash
./Jawa --perl-lib ./perl5/lib/perl5 ../../sku.0/sys.shared/compiled/game/
```

### Full custom configuration
```bash
./Jawa \
  --verbose \
  --miff /opt/tools/Miff \
  --perl-lib /usr/local/lib/perl5 \
  --client-path ../../sku.0/sys.client/compiled/game/ \
  --collect-script ./scripts/collectAssetCustomizationInfo.pl \
  --build-script ./scripts/buildAssetCustomizationManagerData.pl \
  ../../sku.0/sys.shared/compiled/game/
```

## Requirements

- C++11 compiler (GCC 4.8+, MSVC 2015+, Clang 3.3+)
- CMake 3.10+
- Perl interpreter
- Miff compiler
- Perl .pm modules (in PERL5LIB path)

## License

Part of SWG-Titan CSRC project.
