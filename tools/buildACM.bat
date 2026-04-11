@echo off
REM ======================================================================
REM buildACM.bat - Windows batch file to run buildACM.pl
REM Set BUILDACM_NO_PAUSE=1 for CI / scripts to skip "pause".
REM ======================================================================

setlocal

set SCRIPT_DIR=%~dp0

perl "%SCRIPT_DIR%buildACM.pl" --verbose %*
set ACM_EXIT=%ERRORLEVEL%

if %ACM_EXIT% neq 0 (
    echo.
    echo Build failed with error code %ACM_EXIT%
    if not defined BUILDACM_NO_PAUSE pause
    exit /b %ACM_EXIT%
)

echo.
echo Build completed successfully!
if not defined BUILDACM_NO_PAUSE pause
exit /b 0
