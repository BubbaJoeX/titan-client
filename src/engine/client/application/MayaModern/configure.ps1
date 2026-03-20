# Configure MayaModern for VS 2022 Insiders, x64
# Run from MayaModern directory: .\configure.ps1

$ErrorActionPreference = "Stop"
$buildDir = "build"

# Remove existing build to ensure clean config
if (Test-Path $buildDir) {
    Remove-Item -Recurse -Force $buildDir
}

# Use VS 2022 generator (works with VS 2022 Insiders)
# If you have multiple VS installs, set CMAKE_GENERATOR_INSTANCE to Insiders path
$cmake = "C:\Program Files\CMake\bin\cmake.exe"
& $cmake -B $buildDir -G "Visual Studio 17 2022" -A x64

Write-Host "`nOpen build\SwgMayaEditor.sln in VS 2022 Insiders to build."
