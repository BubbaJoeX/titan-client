#!/bin/bash
# ======================================================================
#
# build.sh
#
# Linux/Unix shell script for building AcmBuildTool
#
# ======================================================================

echo "======================================"
echo "ACM Build Tool - Linux Build Script"
echo "======================================"
echo ""

# Check for compiler
if ! command -v g++ &> /dev/null; then
    echo "ERROR: g++ compiler not found!"
    echo "Please install g++ (e.g., sudo apt-get install g++)"
    exit 1
fi

echo "Using g++ compiler..."
echo "Building AcmBuildTool..."

# Create directories
mkdir -p build
mkdir -p bin

# Compile
echo "Compiling source files..."
g++ -std=c++11 -Wall -O2 -DLINUX -Isrc/shared -c src/shared/AcmBuildTool.cpp -o build/AcmBuildTool.o
if [ $? -ne 0 ]; then
    echo "ERROR: Failed to compile AcmBuildTool.cpp"
    exit 1
fi

g++ -std=c++11 -Wall -O2 -DLINUX -Isrc/shared -c src/shared/main.cpp -o build/main.o
if [ $? -ne 0 ]; then
    echo "ERROR: Failed to compile main.cpp"
    exit 1
fi

# Link
echo "Linking..."
g++ build/AcmBuildTool.o build/main.o -o bin/AcmBuildTool -lpthread
if [ $? -ne 0 ]; then
    echo "ERROR: Failed to link executable"
    exit 1
fi

echo ""
echo "Build complete! Executable: bin/AcmBuildTool"
echo ""
echo "You can now run: ./bin/AcmBuildTool <base_path>"
echo ""
