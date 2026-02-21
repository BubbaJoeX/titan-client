@echo off
REM ======================================================================
REM
REM cmake_build.bat
REM
REM Windows CMake build script for AcmBuildTool
REM
REM ======================================================================

echo ========================================
echo ACM Build Tool - CMake Build (Windows)
echo ========================================
echo.

REM Parse command line arguments
set BUILD_TYPE=Release
set GENERATOR=
set CLEAN_BUILD=0

:parse_args
if "%1"=="" goto end_parse
if /i "%1"=="debug" set BUILD_TYPE=Debug
if /i "%1"=="release" set BUILD_TYPE=Release
if /i "%1"=="clean" set CLEAN_BUILD=1
if /i "%1"=="vs2019" set GENERATOR=-G "Visual Studio 16 2019"
if /i "%1"=="vs2022" set GENERATOR=-G "Visual Studio 17 2022"
if /i "%1"=="ninja" set GENERATOR=-G "Ninja"
shift
goto parse_args
:end_parse

echo Build Type: %BUILD_TYPE%
echo Generator: %GENERATOR%
echo.

REM Clean build if requested
if %CLEAN_BUILD%==1 (
    echo Cleaning previous build...
    if exist build rmdir /s /q build
    if exist bin rmdir /s /q bin
    echo.
)

REM Create build directory
if not exist build mkdir build
cd build

REM Configure with CMake
echo Configuring project with CMake...
cmake .. %GENERATOR% -DCMAKE_BUILD_TYPE=%BUILD_TYPE%
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: CMake configuration failed!
    cd ..
    pause
    exit /b 1
)
echo.

REM Build
echo Building project...
cmake --build . --config %BUILD_TYPE% --parallel
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Build failed!
    cd ..
    pause
    exit /b 1
)
echo.

cd ..

echo ========================================
echo Build completed successfully!
echo ========================================
echo.
echo Executable location:
dir /b build\bin\*.exe 2>nul
if %ERRORLEVEL% EQU 0 (
    echo   build\bin\AcmBuildTool.exe
) else (
    dir /b build\bin\%BUILD_TYPE%\*.exe 2>nul
    if %ERRORLEVEL% EQU 0 (
        echo   build\bin\%BUILD_TYPE%\AcmBuildTool.exe
    )
)
echo.
echo Usage:
echo   build\bin\AcmBuildTool.exe ^<base_path^>
echo   or
echo   build\bin\%BUILD_TYPE%\AcmBuildTool.exe ^<base_path^>
echo.

pause
