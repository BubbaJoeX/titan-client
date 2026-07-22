#pragma once

#include <cstdarg>

/**
 * Crash/freeze-safe import tracing — writes to a log file with fflush on every line.
 * Tail while Maya is hung:
 *   Get-Content $env:TEMP\SwgMayaEditor-import.log -Wait -Tail 40
 * Override path:
 *   set SWG_IMPORT_TRACE_LOG=D:\titan\pob-import.trace.log
 */

namespace SwgImportTrace
{
    /** Open/rotate log and write session banner. Safe to call repeatedly. */
    void beginSession(const char* operation, const char* targetPath);

    /** Mark a high-level pipeline step (always flushed). */
    void stage(const char* label);

    /** Formatted stage label (safe for long paths; always flushed). */
    void stagef(const char* fmt, ...);

    void log(const char* category, const char* fmt, ...);

    /** va_list variant for local log wrappers. */
    void logV(const char* category, const char* fmt, va_list args);

    /** Returns the active log file path, or empty if not yet opened. */
    const char* logFilePath();
}
