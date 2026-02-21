# ACM Build Tool - Implementation Summary

## Overview

This is a cross-platform C++ implementation of the Asset Customization Manager (ACM) build process, ported from the original Java implementation. The tool supports both **Linux** and **Windows** platforms with native implementations for each.

## Project Structure

```
src/engine/shared/application/AcmBuildTool/
├── src/
│   └── shared/
│       ├── AcmBuildTool.h          # Main class header
│       ├── AcmBuildTool.cpp        # Main implementation
│       ├── main.cpp                # Entry point
│       └── FirstAcmBuildTool.h     # Precompiled header
├── CMakeLists.txt                  # CMake build configuration
├── Makefile                        # Simple Makefile for Linux/MinGW
├── build.bat                       # Windows build script
├── build.sh                        # Linux build script
├── run_example.bat                 # Windows usage example
├── run_example.sh                  # Linux usage example
└── README.md                       # Documentation
```

## Key Features

### Cross-Platform Compatibility

The implementation uses platform-specific code where needed:

**Windows:**
- Uses `FindFirstFile`/`FindNextFile` for directory traversal
- Uses `_popen`/`_pclose` for process execution
- Uses Windows-style path separators (`\`)
- Includes `<windows.h>` for file operations

**Linux:**
- Uses `opendir`/`readdir` for directory traversal
- Uses `popen`/`pclose` for process execution
- Uses Unix-style path separators (`/`)
- Includes POSIX headers (`<unistd.h>`, `<dirent.h>`, etc.)

### Build Process (5 Steps)

1. **Create Lookup File**
   - Recursively scans `appearance/`, `shader/`, and `texturerenderer/` directories
   - Collects files with extensions: `.apt`, `.cmp`, `.lmg`, `.lod`, `.lsb`, `.mgn`, `.msh`, `.sat`, `.pob`, `.sht`, `.trt`
   - Generates `acm_lookup.dat` with paths

2. **Collect Asset Customization Info**
   - Executes `collectAssetCustomizationInfo.pl` Perl script
   - Appends `force_add_variable_usage.dat` contents
   - Generates `acm_collect_result.dat`

3. **Optimize Asset Customization Data**
   - Executes `buildAssetCustomizationManagerData.pl` with `-r` flag
   - Removes duplicates and errors
   - Generates `acm_collect_result-optimized.dat`

4. **Build Asset Customization Manager**
   - Executes `buildAssetCustomizationManagerData.pl` again
   - Creates MIFF files for ACM and CIM

5. **Compile**
   - Compiles MIFF files to IFF files using `Miff.exe` (Windows) or `Miff` (Linux)
   - Outputs to `client/customization/` directory

## Building the Tool

### Quick Build

**Linux:**
```bash
cd src/engine/shared/application/AcmBuildTool
chmod +x build.sh
./build.sh
```

**Windows:**
```cmd
cd src\engine\shared\application\AcmBuildTool
build.bat
```

### Alternative Build Methods

**Using Make:**
```bash
make          # Build
make clean    # Clean
make rebuild  # Clean and build
```

**Using CMake:**
```bash
mkdir build && cd build
cmake ..
make          # Linux
cmake --build . --config Release  # Windows
```

## Usage

```bash
# Linux
./bin/AcmBuildTool <base_path>

# Windows
bin\AcmBuildTool.exe <base_path>
```

**Example:**
```bash
./bin/AcmBuildTool ../../sku.0/sys.shared/compiled/game/
```

## Code Highlights

### File Gathering

The `gatherFilesForCompile` function uses platform-specific APIs:

```cpp
#ifdef _WIN32
    // Windows: Use FindFirstFile/FindNextFile
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(pattern.c_str(), &findData);
#else
    // Linux: Use opendir/readdir
    DIR *dir = opendir(path.c_str());
    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr)
#endif
```

### Process Execution

The `executeCommand` function abstracts process execution:

```cpp
#ifdef _WIN32
    FILE *pipe = _popen(command.c_str(), "r");
    // ... read output ...
    _pclose(pipe);
#else
    FILE *pipe = popen(fullCommand.c_str(), "r");
    // ... read output ...
    pclose(pipe);
#endif
```

### Path Handling

The tool uses platform-specific path separators:

```cpp
#ifdef _WIN32
    #define PATH_SEPARATOR "\\"
#else
    #define PATH_SEPARATOR "/"
#endif
```

## Dependencies

- **C++11 compiler** (GCC 4.8+, MSVC 2015+, or Clang 3.3+)
- **Perl** (for running `.pl` scripts)
- **Miff compiler** (for compiling MIFF files to IFF)

## Testing

To test the implementation:

1. Build the tool using one of the build methods
2. Run the example script:
   ```bash
   # Linux
   chmod +x run_example.sh
   ./run_example.sh

   # Windows
   run_example.bat
   ```
3. Verify output files are created in `client/customization/`

## Error Handling

The tool provides detailed error messages at each step:
- File I/O errors
- Process execution failures
- Missing files or directories
- Compilation errors

All errors are printed to `stderr` with context about which step failed.

## Differences from Java Implementation

1. **Manual memory management** (C++ vs Java garbage collection)
2. **Platform-specific code** (explicit `#ifdef` blocks vs Java's platform abstraction)
3. **Direct system calls** (no `Runtime.getRuntime().exec()` wrapper)
4. **Static methods** (no need for object instantiation)
5. **Manual file handling** (no try-with-resources, uses RAII instead)

## Future Enhancements

Possible improvements:
- Add multithreading for file gathering
- Implement progress bars for long operations
- Add verbose logging mode
- Support for configuration files
- Better error recovery and retry logic
- Unit tests

## Compatibility Notes

- **Windows 7+** (requires Windows API support)
- **Linux 2.6+** (requires POSIX.1-2001 support)
- **GCC 4.8+** or **MSVC 2015+** (requires C++11 support)

## License

Part of the SWG-Titan CSRC project.

## Author

Ported from Java to C++ by AI for improved cross-platform compatibility and performance. Thank you Aconite.
