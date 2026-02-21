@echo off
REM ======================================================================
REM
REM build.bat
REM
REM Windows batch file for building AcmBuildTool
REM
REM ======================================================================

echo ======================================
echo ACM Build Tool - Windows Build Script
echo ======================================
echo.

REM Check for compiler
where cl.exe >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    echo Using MSVC compiler...
    goto :build_msvc
)

where g++.exe >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    echo Using MinGW g++ compiler...
    goto :build_mingw
)

echo ERROR: No compatible compiler found!
echo Please install Visual Studio or MinGW.
pause
exit /b 1

:build_msvc
echo Building with MSVC...
if not exist build mkdir build
cd build
cmake .. -G "Visual Studio 16 2019"
cmake --build . --config Release
cd ..
echo Build complete! Executable: build\bin\Release\AcmBuildTool.exe
goto :end

:build_mingw
echo Building with MinGW...
if not exist build mkdir build
if not exist bin mkdir bin
g++ -std=c++11 -Wall -O2 -D_WIN32 -Isrc/shared -c src/shared/AcmBuildTool.cpp -o build/AcmBuildTool.o
g++ -std=c++11 -Wall -O2 -D_WIN32 -Isrc/shared -c src/shared/main.cpp -o build/main.o
g++ build/AcmBuildTool.o build/main.o -o bin/AcmBuildTool.exe
echo Build complete! Executable: bin\AcmBuildTool.exe
goto :end

:end
echo.
echo Build finished successfully!
pause
