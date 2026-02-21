# ACM Build Tool - Configuration Summary

## What Was Added

The AcmBuildTool now supports fully configurable paths for all external dependencies and output locations.

### Configuration Capabilities

**1. Miff Compiler Path** (`--miff`)
   - Allows specifying custom Miff executable location
   - CMake variable: `ACM_MIFF_PATH`
   - Default: `Miff.exe` (Windows) or `Miff` (Linux)

**2. Perl Scripts** (`--collect-script`, `--build-script`)
   - Custom paths for `collectAssetCustomizationInfo.pl`
   - Custom paths for `buildAssetCustomizationManagerData.pl`
   - CMake variables: `ACM_COLLECT_SCRIPT`, `ACM_BUILD_SCRIPT`

**3. Base Path** (positional argument)
   - Path to `sys.shared/compiled/game/` directory
   - Can be set as CMake default: `ACM_BASE_PATH`

**4. Client Output Path** (`--client-path`)
   - Output directory for compiled `.iff` files
   - Supports `sys.client` or any custom location
   - CMake variable: `ACM_CLIENT_PATH`
   - Default: `client/`

**5. Perl Interpreter** (`--perl`)
   - Custom Perl executable/command
   - CMake variable: `ACM_PERL_INTERPRETER`
   - Default: `perl`

**6. Force Add File**
   - Configurable path to `force_add_variable_usage.dat`
   - CMake variable: `ACM_FORCE_ADD_FILE`
   - Default: `customization/force_add_variable_usage.dat`

## Quick Usage

### Compile-Time Configuration (CMake)

```bash
cmake -B build \
  -DACM_MIFF_PATH="/usr/local/bin/Miff" \
  -DACM_CLIENT_PATH="../../sku.0/sys.client/compiled/game/" \
  -DACM_COLLECT_SCRIPT="/opt/scripts/collectAssetCustomizationInfo.pl" \
  -DACM_BUILD_SCRIPT="/opt/scripts/buildAssetCustomizationManagerData.pl"

cmake --build build
```

### Runtime Configuration (Command-Line)

```bash
./build/bin/AcmBuildTool \
  --miff /custom/path/to/Miff \
  --client-path ../../sku.0/sys.client/compiled/game/ \
  --collect-script /custom/collectAssetCustomizationInfo.pl \
  --build-script /custom/buildAssetCustomizationManagerData.pl \
  --perl /custom/perl \
  --verbose \
  ../../sku.0/sys.shared/compiled/game/
```

## New Files Created

1. **AcmBuildConfig.h.in** - CMake template for configuration header
2. **CONFIG_OPTIONS.md** - Comprehensive configuration documentation  
3. **Updated CMakeLists.txt** - Added configuration cache variables
4. **Updated AcmBuildTool.h** - Added `AcmBuildConfig` struct and new methods
5. **Updated AcmBuildTool.cpp** - Implemented configuration parsing and usage

## Key Features

✅ **Flexible Paths** - Configure all external dependencies  
✅ **CMake Integration** - Bake defaults into binary  
✅ **Runtime Override** - Command-line options override CMake defaults  
✅ **Verbose Mode** - `--verbose` flag for debugging  
✅ **Cross-Platform** - Works on Windows and Linux  
✅ **Help & Version** - `-h`, `--help`, `-v`, `--version` flags  

## Example Scenarios

### Scenario 1: Standard Build

```bash
cmake -B build
cmake --build build
./build/bin/AcmBuildTool ../../sku.0/sys.shared/compiled/game/
```

### Scenario 2: Custom Miff Location

```bash
# At build time
cmake -B build -DACM_MIFF_PATH="/usr/local/bin/Miff"
cmake --build build
./build/bin/AcmBuildTool ../../sku.0/sys.shared/compiled/game/

# Or at runtime
./build/bin/AcmBuildTool --miff /usr/local/bin/Miff ../../sku.0/sys.shared/compiled/game/
```

### Scenario 3: sys.client Output

```bash
# Configure for sys.client output
cmake -B build -DACM_CLIENT_PATH="../../sku.0/sys.client/compiled/game/"
cmake --build build

# Run with configured default
./build/bin/AcmBuildTool ../../sku.0/sys.shared/compiled/game/

# Or override at runtime
./build/bin/AcmBuildTool \
  --client-path ../../sku.0/sys.client/compiled/game/ \
  ../../sku.0/sys.shared/compiled/game/
```

### Scenario 4: CI/CD Pipeline

```bash
# Build once with defaults
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Run with different configurations
./build/bin/AcmBuildTool \
  --verbose \
  --miff /build/tools/Miff \
  --client-path /build/output/client/ \
  /build/source/sys.shared/compiled/game/
```

## Configuration Hierarchy

1. **Highest Priority:** Command-line arguments (`--miff`, `--client-path`, etc.)
2. **Medium Priority:** CMake cache variables (set during configuration)
3. **Lowest Priority:** Built-in defaults (from `AcmBuildConfig.h.in`)

## Verification

Check your configuration:

```bash
# View CMake configuration
cmake -B build
# Look for "=== ACM Configuration Paths ===" section

# View tool version and defaults
./build/bin/AcmBuildTool --version

# Test with verbose mode
./build/bin/AcmBuildTool --verbose ../../sku.0/sys.shared/compiled/game/
```

## Documentation Files

- **CONFIG_OPTIONS.md** - Full configuration reference
- **CMAKE_BUILD.md** - CMake build instructions
- **README.md** - General usage
- **QUICKSTART.md** - Quick reference
- **IMPLEMENTATION.md** - Technical details

## Benefits

1. **No Hardcoded Paths** - All paths configurable
2. **Team Flexibility** - Each developer can customize paths
3. **CI/CD Ready** - Easy integration into build pipelines
4. **Cross-Platform** - Same tool, different configurations
5. **Backward Compatible** - Defaults work out of the box

## Next Steps

1. **Build the tool:**
   ```bash
   cd D:\titan\client\src\engine\shared\application\AcmBuildTool
   cmake -B build
   cmake --build build --config Release
   ```

2. **Test with your setup:**
   ```bash
   build\bin\Release\AcmBuildTool.exe --verbose --help
   ```

3. **Configure for your environment:**
   Edit `CMakeLists.txt` or use `-D` options with cmake

4. **Run the build:**
   ```bash
   build\bin\Release\AcmBuildTool.exe <your_base_path>
   ```

## Support

For issues or questions, see the comprehensive documentation in:
- `CONFIG_OPTIONS.md` for configuration help
- `CMAKE_BUILD.md` for build help
- `README.md` for usage help
