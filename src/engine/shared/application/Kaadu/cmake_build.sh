#!/bin/bash
# ======================================================================
#
# cmake_build.sh
#
# Linux/Unix CMake build script for AcmBuildTool
#
# ======================================================================

set -e  # Exit on error

echo "========================================"
echo "ACM Build Tool - CMake Build (Linux)"
echo "========================================"
echo ""

# Parse command line arguments
BUILD_TYPE="Release"
CLEAN_BUILD=0
GENERATOR=""
JOBS=$(nproc 2>/dev/null || echo 4)

while [[ $# -gt 0 ]]; do
    case $1 in
        debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        release)
            BUILD_TYPE="Release"
            shift
            ;;
        clean)
            CLEAN_BUILD=1
            shift
            ;;
        ninja)
            GENERATOR="-G Ninja"
            shift
            ;;
        -j*)
            JOBS="${1#-j}"
            shift
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 [debug|release] [clean] [ninja] [-j<jobs>]"
            exit 1
            ;;
    esac
done

echo "Build Type: $BUILD_TYPE"
echo "Generator: ${GENERATOR:-Unix Makefiles}"
echo "Parallel Jobs: $JOBS"
echo ""

# Check for CMake
if ! command -v cmake &> /dev/null; then
    echo "ERROR: CMake not found!"
    echo "Please install CMake: sudo apt-get install cmake"
    exit 1
fi

echo "CMake version: $(cmake --version | head -n 1)"
echo ""

# Clean build if requested
if [ $CLEAN_BUILD -eq 1 ]; then
    echo "Cleaning previous build..."
    rm -rf build bin
    echo ""
fi

# Create build directory
mkdir -p build
cd build

# Configure with CMake
echo "Configuring project with CMake..."
cmake .. $GENERATOR -DCMAKE_BUILD_TYPE=$BUILD_TYPE

echo ""

# Build
echo "Building project..."
cmake --build . --config $BUILD_TYPE --parallel $JOBS

echo ""
cd ..

echo "========================================"
echo "Build completed successfully!"
echo "========================================"
echo ""
echo "Executable location:"
if [ -f "build/bin/AcmBuildTool" ]; then
    echo "  build/bin/AcmBuildTool"
    ls -lh build/bin/AcmBuildTool
else
    echo "  build/bin/$BUILD_TYPE/AcmBuildTool"
    ls -lh build/bin/$BUILD_TYPE/AcmBuildTool 2>/dev/null || true
fi
echo ""
echo "Usage:"
echo "  ./build/bin/AcmBuildTool <base_path>"
echo ""
echo "To install system-wide:"
echo "  cd build && sudo cmake --install ."
echo ""
