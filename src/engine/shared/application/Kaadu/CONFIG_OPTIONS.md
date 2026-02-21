# CMake Configuration Options for AcmBuildTool

## Overview

The AcmBuildTool now supports configurable paths through CMake cache variables. These can be set at configuration time and will be baked into the executable as defaults.

## Configuration Variables

### CMake Cache Variables

Set these when running `cmake` to customize build-time defaults:

```bash
cmake -B build \
  -DACM_MIFF_PATH="/path/to/Miff" \
  -DACM_COLLECT_SCRIPT="/path/to/collectAssetCustomizationInfo.pl" \
  -DACM_BUILD_SCRIPT="/path/to/buildAssetCustomizationManagerData.pl" \
  -DACM_BASE_PATH="../../sku.0/sys.shared/compiled/game/" \
  -DACM_CLIENT_PATH="client/" \
  -DACM_PERL_INTERPRETER="perl"
```

### Available Options

| Variable | Default (Windows) | Default (Linux) | Description |
|----------|-------------------|-----------------|-------------|
| `ACM_MIFF_PATH` | `Miff.exe` | `Miff` | Path to Miff compiler executable |
| `ACM_COLLECT_SCRIPT` | `collectAssetCustomizationInfo.pl` | `collectAssetCustomizationInfo.pl` | Path to collect script |
| `ACM_BUILD_SCRIPT` | `buildAssetCustomizationManagerData.pl` | `buildAssetCustomizationManagerData.pl` | Path to build script |
| `ACM_BASE_PATH` | `../../sku.0/sys.shared/compiled/game/` | `../../sku.0/sys.shared/compiled/game/` | Default base path |
| `ACM_CLIENT_PATH` | `client/` | `client/` | Output path for client files |
| `ACM_FORCE_ADD_FILE` | `customization/force_add_variable_usage.dat` | `customization/force_add_variable_usage.dat` | Relative path to force add file |
| `ACM_PERL_INTERPRETER` | `perl` | `perl` | Perl interpreter command |

## Command-Line Options

The tool also supports runtime configuration through command-line arguments:

```bash
AcmBuildTool [OPTIONS] <base_path>

Options:
  --miff <path>            Override Miff compiler path
  --collect-script <path>  Override collect script path
  --build-script <path>    Override build script path
  --client-path <path>     Override client output path
  --perl <cmd>             Override Perl interpreter
  --verbose                Enable verbose output
  -v, --version           Show version
  -h, --help               Show help
```

## Usage Examples

### Example 1: Custom Miff Path

```bash
# Set at CMake configuration
cmake -B build -DACM_MIFF_PATH="/usr/local/bin/Miff"
cmake --build build

# Or override at runtime
./build/bin/AcmBuildTool --miff /usr/local/bin/Miff ../../sku.0/sys.shared/compiled/game/
```

### Example 2: Custom Script Paths

```bash
# CMake configuration
cmake -B build \
  -DACM_COLLECT_SCRIPT="/opt/scripts/collectAssetCustomizationInfo.pl" \
  -DACM_BUILD_SCRIPT="/opt/scripts/buildAssetCustomizationManagerData.pl"

# Or runtime override
./build/bin/AcmBuildTool \
  --collect-script /opt/scripts/collectAssetCustomizationInfo.pl \
  --build-script /opt/scripts/buildAssetCustomizationManagerData.pl \
  ../../sku.0/sys.shared/compiled/game/
```

### Example 3: Custom Client Output Path

For `sys.client` instead of default `client/`:

```bash
# CMake configuration
cmake -B build -DACM_CLIENT_PATH="../../sku.0/sys.client/compiled/game/"

# Or runtime override
./build/bin/AcmBuildTool --client-path ../../sku.0/sys.client/compiled/game/ \
  ../../sku.0/sys.shared/compiled/game/
```

### Example 4: All Custom Paths

```bash
# Windows example
cmake -B build -G "Visual Studio 17 2022" ^
  -DACM_MIFF_PATH="C:/Tools/Miff.exe" ^
  -DACM_COLLECT_SCRIPT="C:/Scripts/collectAssetCustomizationInfo.pl" ^
  -DACM_BUILD_SCRIPT="C:/Scripts/buildAssetCustomizationManagerData.pl" ^
  -DACM_BASE_PATH="D:/swg/sku.0/sys.shared/compiled/game/" ^
  -DACM_CLIENT_PATH="D:/swg/sku.0/sys.client/compiled/game/" ^
  -DACM_PERL_INTERPRETER="C:/Perl/bin/perl.exe"

cmake --build build --config Release

# Run with configured defaults
build\bin\Release\AcmBuildTool.exe D:/swg/sku.0/sys.shared/compiled/game/
```

### Example 5: Verbose Mode

```bash
./build/bin/AcmBuildTool --verbose ../../sku.0/sys.shared/compiled/game/
```

This will show:
- Configuration values being used
- Commands being executed
- File counts
- Detailed progress

## Configuration File Generation

CMake generates `AcmBuildConfig.h` with your settings:

```cpp
// Auto-generated configuration
#define ACM_DEFAULT_MIFF_PATH "/usr/local/bin/Miff"
#define ACM_DEFAULT_COLLECT_SCRIPT "collectAssetCustomizationInfo.pl"
// ... etc
```

Location: `build/generated/AcmBuildConfig.h`

## Checking Current Configuration

View the configured values during CMake configuration:

```bash
cmake -B build

# Output shows:
# === ACM Configuration Paths ===
# Miff compiler: Miff.exe
# Collect script: collectAssetCustomizationInfo.pl
# Build script: buildAssetCustomizationManagerData.pl
# Base path: ../../sku.0/sys.shared/compiled/game/
# Client output: client/
# Perl interpreter: perl
# ===============================
```

Or check at runtime:

```bash
./build/bin/AcmBuildTool --version
```

## Integration with Build Systems

### CMake Presets

Create `CMakePresets.json`:

```json
{
  "version": 3,
  "configurePresets": [
    {
      "name": "custom-paths",
      "cacheVariables": {
        "ACM_MIFF_PATH": "/usr/local/bin/Miff",
        "ACM_COLLECT_SCRIPT": "/opt/scripts/collectAssetCustomizationInfo.pl",
        "ACM_BUILD_SCRIPT": "/opt/scripts/buildAssetCustomizationManagerData.pl",
        "ACM_CLIENT_PATH": "../../sku.0/sys.client/compiled/game/"
      }
    }
  ]
}
```

Then build with:

```bash
cmake --preset custom-paths
cmake --build build
```

### Environment Variables

You can also set via environment:

```bash
export ACM_MIFF_PATH="/usr/local/bin/Miff"
cmake -B build -DACM_MIFF_PATH="$ACM_MIFF_PATH"
```

## Troubleshooting

### Scripts Not Found

**Problem:** Perl scripts not found at runtime

**Solutions:**
1. Use absolute paths in CMake config
2. Copy scripts to working directory
3. Use `--collect-script` and `--build-script` options

### Miff Compiler Not Found

**Problem:** Miff executable not found

**Solutions:**
1. Add Miff to PATH
2. Use `--miff` option with full path
3. Set `ACM_MIFF_PATH` in CMake

### Wrong Output Location

**Problem:** Client files in wrong directory

**Solution:**
Use `--client-path` to specify correct output location:

```bash
AcmBuildTool --client-path ../../sku.0/sys.client/compiled/game/ \
  ../../sku.0/sys.shared/compiled/game/
```

## Best Practices

1. **Development:** Use CMake cache variables for team-wide defaults
2. **CI/CD:** Use command-line options for flexibility
3. **Testing:** Use `--verbose` to diagnose issues
4. **Production:** Bake paths into binary via CMake for consistency

## Summary

```bash
# Configure with custom paths (one-time)
cmake -B build \
  -DACM_MIFF_PATH="/path/to/Miff" \
  -DACM_CLIENT_PATH="../../sku.0/sys.client/compiled/game/"

# Build
cmake --build build

# Run (uses configured defaults)
./build/bin/AcmBuildTool ../../sku.0/sys.shared/compiled/game/

# Or override at runtime
./build/bin/AcmBuildTool \
  --verbose \
  --miff /other/path/to/Miff \
  --client-path /custom/output/ \
  ../../sku.0/sys.shared/compiled/game/
```
