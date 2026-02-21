# ACM Build Tool

Asset Customization Manager Build Tool - A cross-platform C++ implementation for building ACM and CIM files.

## Overview

This tool automates the 5-step process for building the Asset Customization Manager:

1. **Create Lookup File** - Gathers all appearance/shader/texture files into `acm-lookup.dat`
2. **Collect Asset Customization Info** - Runs `collectAssetCustomizationInfo.pl` to gather customization data
3. **Optimize Asset Customization Data** - Runs optimization to remove duplicates and errors
4. **Build Asset Customization Manager** - Generates ACM and CIM MIFF files
5. **Compile** - Compiles MIFF files into IFF files for the client

## Supported Platforms

- **Windows** (Visual Studio, MinGW, or CMake)
- **Linux** (GCC, CMake, or Make)

## Prerequisites

- C++11 compatible compiler
- Perl (for running the `.pl` scripts)
- Miff compiler executable (`Miff.exe` on Windows, `Miff` on Linux)

## Building

### Linux/Unix - Using Make

```bash
cd src/engine/shared/application/AcmBuildTool
make
```

The executable will be created in `bin/AcmBuildTool`.

### Linux/Unix - Using CMake

```bash
cd src/engine/shared/application/AcmBuildTool
mkdir build && cd build
cmake ..
make
```

The executable will be created in `build/bin/AcmBuildTool`.

### Windows - Using Make (MinGW)

```cmd
cd src\engine\shared\application\AcmBuildTool
make
```

The executable will be created in `bin\AcmBuildTool.exe`.

### Windows - Using CMake

```cmd
cd src\engine\shared\application\AcmBuildTool
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

### Windows - Using Visual Studio

1. Open the solution in Visual Studio
2. Add the AcmBuildTool project to your solution
3. Build the project

## Usage

```bash
AcmBuildTool <base_path>
```

**Parameters:**
- `<base_path>` - Path to the base directory (e.g., `../../sku.0/sys.shared/compiled/game/`)

**Example:**

```bash
# Linux
./bin/AcmBuildTool ../../sku.0/sys.shared/compiled/game/

# Windows
bin\AcmBuildTool.exe ..\..\sku.0\sys.shared\compiled\game\
```

## Output Files

The tool generates the following files:

**Intermediate files** (automatically cleaned up):
- `acm_lookup.dat` - Lookup table for file paths
- `acm_collect_result.dat` - Collected customization info
- `acm_collect_result-optimized.dat` - Optimized customization data

**Final output files**:
- `<base_path>/customization/asset_customization_manager.mif` - ACM MIFF file
- `<base_path>/customization/customization_id_manager.mif` - CIM MIFF file
- `client/customization/asset_customization_manager.iff` - Compiled ACM IFF
- `client/customization/customization_id_manager.iff` - Compiled CIM IFF

## File Types Processed

The tool searches for the following file types:

**Appearance files** (`appearance/` directory):
- `.apt`, `.cmp`, `.lmg`, `.lod`, `.lsb`, `.mgn`, `.msh`, `.sat`, `.pob`

**Shader files** (`shader/` directory):
- `.sht`

**Texture Renderer files** (`texturerenderer/` directory):
- `.trt`

## Additional Files Read

The tool also reads:
- `customization/force_add_variable_usage.dat` - Override file for special cases
- `mount/logical_saddle_name_map.tab` - Saddle name mappings (via Perl script)
- `mount/saddle_appearance_map.tab` - Saddle appearance mappings (via Perl script)
- `customization/vehicle_customizations.tab` - Vehicle customizations (via Perl script)

## Error Handling

If the build fails at any step, the tool will:
1. Print an error message indicating which step failed
2. Return a non-zero exit code
3. Preserve intermediate files for debugging

## Notes

- Ensure Perl is in your PATH
- Ensure the Perl scripts (`collectAssetCustomizationInfo.pl`, `buildAssetCustomizationManagerData.pl`) are in the current directory
- Ensure the Miff compiler is accessible in your PATH or current directory
- The tool will create the `client/customization/` directory if it doesn't exist

## Troubleshooting

**"Failed to execute command" errors:**
- Verify Perl is installed and in your PATH
- Check that the `.pl` scripts are in the current working directory

**"Failed to compile MIFF" errors:**
- Verify the Miff compiler is in your PATH
- On Linux, ensure the Miff binary has execute permissions: `chmod +x Miff`

**File gathering issues:**
- Verify the base path is correct
- Ensure the directory structure matches the expected layout

## License

This tool is part of the SWG-Titan CSRC project.

## Credits

Ported from the original Java implementation to C++ for better cross-platform compatibility.
