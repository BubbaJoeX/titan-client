#!/bin/bash
# Quick build script for Jawa (Linux)

echo "Building Jawa..."
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
echo ""
echo "Build complete! Executable: build/bin/Jawa"
echo "Usage: ./build/bin/Jawa [--perl-lib <path>] <base_path>"
