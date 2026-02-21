# CMake Build Instructions for AcmBuildTool

## Quick Start

### Windows

```cmd
cd src\engine\shared\application\AcmBuildTool
cmake_build.bat
```

### Linux

```bash
cd src/engine/shared/application/AcmBuildTool
chmod +x cmake_build.sh
./cmake_build.sh
```

## Detailed CMake Build Options

### Windows

**Basic build (Release):**
```cmd
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

**Debug build:**
```cmd
cmake_build.bat debug
```

**Clean and rebuild:**
```cmd
cmake_build.bat clean release
```

**Specify Visual Studio version:**
```cmd
cmake_build.bat vs2019
cmake_build.bat vs2022
```

**Use Ninja generator:**
```cmd
cmake_build.bat ninja
```

### Linux

**Basic build (Release):**
```bash
mkdir build
cd build
cmake ..
cmake --build .
```

**Debug build:**
```bash
./cmake_build.sh debug
```

**Clean and rebuild:**
```bash
./cmake_build.sh clean release
```

**Parallel build with 8 jobs:**
```bash
./cmake_build.sh -j8
```

**Use Ninja generator:**
```bash
./cmake_build.sh ninja
```

## Manual CMake Commands

### Configure

```bash
# Default generator
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Visual Studio (Windows)
cmake -B build -G "Visual Studio 17 2022"

# Ninja (cross-platform)
cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release

# Unix Makefiles (Linux)
cmake -B build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
```

### Build

```bash
# Build with default configuration
cmake --build build

# Build with specific configuration
cmake --build build --config Release

# Parallel build
cmake --build build --parallel 8

# Verbose output
cmake --build build --verbose
```

### Install

```bash
# Install to default prefix (/usr/local on Linux, C:\Program Files on Windows)
cd build
cmake --install .

# Install to custom prefix
cmake --install . --prefix /opt/acmbuild

# Install with sudo (Linux)
cd build
sudo cmake --install .
```

## Build Types

| Build Type | Optimization | Debug Info | Use Case |
|------------|--------------|------------|----------|
| **Debug** | None | Full | Development, debugging |
| **Release** | Full (-O3) | None | Production use |
| **RelWithDebInfo** | Optimized | Yes | Profiling |
| **MinSizeRel** | Size (-Os) | None | Embedded systems |

**Example:**
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
```

## Generator Options

### Windows Generators

- **Visual Studio 16 2019** - VS 2019
- **Visual Studio 17 2022** - VS 2022
- **Ninja** - Fast, cross-platform
- **NMake Makefiles** - Microsoft NMake

### Linux Generators

- **Unix Makefiles** - Standard make (default)
- **Ninja** - Fast, recommended for large projects

## Output Locations

After building, the executable will be located at:

**Windows (MSVC):**
```
build/bin/Release/AcmBuildTool.exe
build/bin/Debug/AcmBuildTool.exe
```

**Windows (MinGW) / Linux:**
```
build/bin/AcmBuildTool
build/bin/AcmBuildTool.exe  (MinGW)
```

## CMake Configuration Options

You can customize the build with CMake options:

```bash
# Set install prefix
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr/local

# Set C++ compiler
cmake -B build -DCMAKE_CXX_COMPILER=g++
cmake -B build -DCMAKE_CXX_COMPILER=clang++

# Enable verbose makefiles
cmake -B build -DCMAKE_VERBOSE_MAKEFILE=ON

# Specify toolchain file
cmake -B build -DCMAKE_TOOLCHAIN_FILE=path/to/toolchain.cmake
```

## IDE Integration

### Visual Studio

```cmd
# Generate VS solution
cmake -B build -G "Visual Studio 17 2022"

# Open solution
start build\AcmBuildTool.sln
```

### Visual Studio Code

1. Install CMake Tools extension
2. Open folder in VS Code
3. Press `Ctrl+Shift+P` → "CMake: Configure"
4. Press `F7` to build

### CLion

1. Open project folder
2. CLion automatically detects CMakeLists.txt
3. Click "Build" or press `Ctrl+F9`

## Troubleshooting

### "CMake not found"
**Windows:**
- Install CMake from https://cmake.org/download/
- Add to PATH: `C:\Program Files\CMake\bin`

**Linux:**
```bash
sudo apt-get install cmake  # Debian/Ubuntu
sudo yum install cmake      # RHEL/CentOS
```

### "Compiler not found"
**Windows:**
- Install Visual Studio 2019/2022 with C++ workload
- Or install MinGW-w64

**Linux:**
```bash
sudo apt-get install build-essential  # Debian/Ubuntu
sudo yum groupinstall "Development Tools"  # RHEL/CentOS
```

### "Ninja not found"
**Windows:**
```cmd
# Using Chocolatey
choco install ninja

# Or download from https://github.com/ninja-build/ninja/releases
```

**Linux:**
```bash
sudo apt-get install ninja-build  # Debian/Ubuntu
sudo yum install ninja-build      # RHEL/CentOS
```

### Build fails with "permission denied"
**Linux:**
```bash
chmod +x cmake_build.sh
```

### Out-of-date build
```bash
# Clean and rebuild
rm -rf build
cmake -B build
cmake --build build
```

## Advanced Usage

### Cross-Compilation

```bash
# Example: Windows to Linux (using MinGW)
cmake -B build \
    -DCMAKE_SYSTEM_NAME=Linux \
    -DCMAKE_C_COMPILER=x86_64-linux-gnu-gcc \
    -DCMAKE_CXX_COMPILER=x86_64-linux-gnu-g++
```

### Static Linking

```bash
# Link statically (Linux)
cmake -B build -DCMAKE_EXE_LINKER_FLAGS="-static"
```

### Custom Compiler Flags

```bash
# Add custom flags
cmake -B build -DCMAKE_CXX_FLAGS="-march=native -mtune=native"
```

### Installation Components

```bash
# Install only runtime (executable)
cmake --install build --component runtime

# Install only documentation
cmake --install build --component documentation
```

## Performance Tips

1. **Use Ninja for faster builds:**
   ```bash
   cmake -B build -G Ninja
   ```

2. **Enable parallel builds:**
   ```bash
   cmake --build build --parallel $(nproc)
   ```

3. **Use ccache for incremental builds:**
   ```bash
   cmake -B build -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
   ```

4. **Use precompiled headers (requires CMake 3.16+):**
   Add to CMakeLists.txt:
   ```cmake
   target_precompile_headers(AcmBuildTool PRIVATE src/shared/FirstAcmBuildTool.h)
   ```

## Summary

**Simplest build command:**
```bash
# Linux
./cmake_build.sh

# Windows
cmake_build.bat
```

The executable will be in `build/bin/` directory.
