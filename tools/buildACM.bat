@echo off
REM ======================================================================
REM buildACM.bat - Windows batch file to run buildACM.pl
REM ======================================================================

setlocal

REM Get the directory where this batch file is located
set SCRIPT_DIR=%~dp0

REM Run the Perl script with verbose output
perl "%SCRIPT_DIR%buildACM.pl" --verbose %*

if %ERRORLEVEL% neq 0 (
    echo.
    echo Build failed with error code %ERRORLEVEL%
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo Build completed successfully!
pause
