@echo off
REM ======================================================================
REM
REM run_example.bat
REM
REM Example script showing how to use AcmBuildTool on Windows
REM
REM ======================================================================

REM Set the base path to your compiled game directory
set BASE_PATH=..\..\sku.0\sys.shared\compiled\game\

REM Check if the tool exists
if not exist bin\AcmBuildTool.exe (
    echo ERROR: AcmBuildTool.exe not found!
    echo Please build the tool first by running: build.bat
    pause
    exit /b 1
)

REM Check if base path exists
if not exist "%BASE_PATH%" (
    echo ERROR: Base path not found: %BASE_PATH%
    echo Please update the BASE_PATH variable in this script to point to your compiled game directory.
    pause
    exit /b 1
)

echo Running ACM Build Tool...
echo Base Path: %BASE_PATH%
echo.

REM Run the tool
bin\AcmBuildTool.exe "%BASE_PATH%"

REM Check exit code
if %ERRORLEVEL% EQU 0 (
    echo.
    echo ACM build completed successfully!
    echo.
    echo Output files:
    echo   - client\customization\asset_customization_manager.iff
    echo   - client\customization\customization_id_manager.iff
) else (
    echo.
    echo ACM build failed! Check the error messages above.
    pause
    exit /b 1
)

pause
