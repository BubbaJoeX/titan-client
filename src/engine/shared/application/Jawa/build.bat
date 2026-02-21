@echo off
REM Quick build script for Jawa (Windows)

echo Building Jawa...
if not exist build mkdir build
cd build
cmake .. -G "Visual Studio 17 2022"
if %ERRORLEVEL% NEQ 0 cmake .. -G "Visual Studio 16 2019"
cmake --build . --config Release
cd ..
echo.
echo Build complete! Executable: build\bin\Jawa.exe
echo Usage: build\bin\Jawa.exe [--perl-lib ^<path^>] ^<base_path^>
pause
