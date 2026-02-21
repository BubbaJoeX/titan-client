# ACM Build Tool - Quick Reference

## Installation

```bash
# Clone the repository
cd src/engine/shared/application/AcmBuildTool

# Build (Linux)
./build.sh

# Build (Windows)
build.bat
```

## Usage

```bash
AcmBuildTool <base_path>
```

**base_path**: Path to `sku.0/sys.shared/compiled/game/`

## Example

```bash
# Linux
./bin/AcmBuildTool ../../sku.0/sys.shared/compiled/game/

# Windows
bin\AcmBuildTool.exe ..\..\sku.0\sys.shared\compiled\game\
```

## Output Files

| File | Description |
|------|-------------|
| `client/customization/asset_customization_manager.iff` | Compiled ACM file |
| `client/customization/customization_id_manager.iff` | Compiled CIM file |

## Build Process

1. **Gather Files** → `acm_lookup.dat`
2. **Collect Info** → `acm_collect_result.dat`
3. **Optimize** → `acm_collect_result-optimized.dat`
4. **Build MIFFs** → `.mif` files
5. **Compile IFFs** → `.iff` files

## Prerequisites

- C++11 compiler (GCC/MSVC)
- Perl interpreter
- Miff compiler executable

## Troubleshooting

| Problem | Solution |
|---------|----------|
| Command not found | Add Perl and Miff to PATH |
| File not found | Check base path is correct |
| Build failed | Check compiler is installed |
| Permission denied (Linux) | Run `chmod +x build.sh run_example.sh` |

## Platform-Specific Notes

### Linux
- Requires GCC 4.8+ or Clang 3.3+
- Requires POSIX.1-2001 support
- Shell scripts need execute permissions

### Windows
- Requires Visual Studio 2015+ or MinGW
- Can use CMD or PowerShell
- Batch files don't need special permissions

## File Extensions Processed

- **Appearance**: `.apt .cmp .lmg .lod .lsb .mgn .msh .sat .pob`
- **Shader**: `.sht`
- **Texture Renderer**: `.trt`

## Common Commands

```bash
# Build from scratch
make clean && make

# Run with verbose output
./bin/AcmBuildTool ../../sku.0/sys.shared/compiled/game/ 2>&1 | tee acm_build.log

# Check if output files were created
ls -l client/customization/*.iff

# Clean temporary files
rm -f acm_*.dat build_output.txt error_output.txt
```

## Project Structure

```
AcmBuildTool/
├── src/shared/          # Source code
├── build/               # Build artifacts (gitignored)
├── bin/                 # Executable (gitignored)
├── CMakeLists.txt       # CMake config
├── Makefile             # Make config
└── README.md            # Full documentation
```

## Support

For issues or questions:
1. Check README.md for detailed documentation
2. Check IMPLEMENTATION.md for technical details
3. Open an issue on GitHub

## License

Part of SWG-Titan CSRC project.
