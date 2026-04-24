// ======================================================================
//
// Report.cpp
// copyright 1999 Bootprint Entertainment
// copyright 2001 Sony Online Entertainment
//
// ======================================================================

#include "sharedDebug/FirstSharedDebug.h"
#include "sharedDebug/Report.h"

#include "sharedDebug/DebugFlags.h"
#include "sharedDebug/DebugMonitor.h"
#include "sharedFoundation/FloatingPointUnit.h"
#include "sharedFoundation/Os.h"
#include "sharedFoundation/PerThreadData.h"
#include "sharedFoundation/Production.h"

#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <string.h> // strnlen (MSVC cstring may omit global name)

#if defined(_MSC_VER)
#include <stdio.h> // _vsnprintf_s, _TRUNCATE
#endif

// ======================================================================

namespace ReportNamespace
{
	Report::Callback ms_logCallback;
	Report::Callback ms_warningCallback;
	Report::Callback ms_fatalCallback;
	bool             ms_logAllReports;
	int              ms_flags;
};
using namespace ReportNamespace;

// ======================================================================

void Report::install()
{
	DebugFlags::registerFlag(ms_logAllReports, "SharedDebug", "logAllReports");
}

// ----------------------------------------------------------------------

void Report::bindLogCallback(Callback callback)
{
	ms_logCallback = callback;
}

// ----------------------------------------------------------------------

void Report::bindWarningCallback(Callback callback)
{
	ms_warningCallback = callback;
}

// ----------------------------------------------------------------------

void Report::bindFatalCallback(Callback callback)
{
	ms_fatalCallback = callback;
}

// ======================================================================
/**
 * Setup the debug print flags for the next debug print from this thread
 *
 * This routine should never be called directly, but only through the DEBUG_PRINT macros.
 *
 * @internal
 */

void Report::setFlags(int flags)
{
	if (ms_logAllReports)
		flags |= RF_log;

	if (!PerThreadData::isThreadInstalled())
	{
		// if the per-thread-data isn't installed, then we know we're single-threaded and can use the static flags
		ms_flags = flags;
	}
	else
		PerThreadData::setDebugPrintFlags(flags);
}

// ----------------------------------------------------------------------

void Report::puts(const char *buffer)
{
	int flags;
	if (!PerThreadData::isThreadInstalled())
	{
		// if the per-thread-data isn't installed, then we know we're single-threaded and can use the static flags
		flags = ms_flags;
	}
	else
		flags = PerThreadData::getDebugPrintFlags();

	// handle logging callback functions
	if (flags & RF_fatal)
	{
		if (ms_fatalCallback)
			(*ms_fatalCallback)(buffer);
	}
	else
		if (flags & RF_warning)
		{
			if (ms_warningCallback)
				(*ms_warningCallback)(buffer);
		}
		else
			if (flags & RF_log)
			{
				if (ms_logCallback)
					(*ms_logCallback)(buffer);
			}

	if (flags & RF_print)
	{
#ifdef WIN32
		HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
		if (hStdOut)
		{
			// vprintf's buffer is 8*1024; do not unbounded-strlen a possibly non-terminated string.
			size_t const n = strnlen(buffer, 8U * 1024U);
			DWORD bytesWritten;
			WriteFile(hStdOut, buffer, static_cast<DWORD>(n), &bytesWritten, 0);
		}
#else
		fputs(buffer, stdout);
#endif
	}

#if PRODUCTION == 0
	if (flags & RF_print)
	{
		DebugMonitor::print(buffer);
		if (flags & (RF_fatal | RF_dialog))
			DebugMonitor::print("\n");
	}
#endif

	if (flags & RF_log)
	{
		// x64: do not get/set x87 + _controlfp_s around every OutputDebugString. That round-trip
		// (see FloatingPointUnit x87/MSVC mapping) has been a source of 0xC0000005 after high-volume
		// REPORT_LOG during startup. The x86 "debugger reset FPU precision" quirk is not worth it on Win64.
#if defined(_WIN32) && !defined(_M_X64)
		const WORD fp1 = FloatingPointUnit::getControlWord();
#endif

#ifdef WIN32
		OutputDebugString(buffer);
		if (flags &	RF_console)
			fputs(buffer, stderr);
#else
		fputs(buffer, stderr);
#endif

		// fatal strings and dialog messages do not have newlines on the end of them, but we want them in the logs
		if (flags & (RF_fatal | RF_dialog))
		{
#ifdef WIN32
			OutputDebugString("\n");
			if (flags &	RF_console)
				fputs("\n", stderr);
#else
			fputs("\n", stderr);
#endif
		}

#if defined(_WIN32) && !defined(_M_X64)
		const WORD fp2 = FloatingPointUnit::getControlWord();
		// -qq- HACK: work around OutputDebugString resetting the FPU precision (Win32; see above for Win64)
		if (fp1 != fp2)
			FloatingPointUnit::setControlWord(fp1);
#endif
	}

	// fatal strings should be made very obvious, so pop up a message box
	if ((flags & RF_dialog) && Os::isMainThread())
	{
		const char *title = "Report";
		if (flags & RF_fatal)
			title = "Fatal Report";

		MessageBox(NULL, buffer, title, MB_OK | MB_ICONEXCLAMATION);
	}
}

// ----------------------------------------------------------------------
/**
 * Format and print a debugging message.
 *
 * This routine should never be called directly, but only through the REPORT
 * macros.
 *
 * This routine will send the specified string to the DebugMonitor.  It will
 * also be logged it to the debugger if the RF_log enum was specified.  If
 * the RF_fatal flag was specified, the routine will display a message box
 * with the string in it as well.
 *
 * @internal
 */

void Report::vprintf(const char *format, va_list va)
{
	char buffer[8 * 1024];

	static const char prefix[] = "[Titan] ";
	static const int prefixLen = sizeof(prefix) - 1;

	memcpy(buffer, prefix, prefixLen);

	// make sure the buffer is always NULL terminated
	buffer[sizeof(buffer)-1] = '\0';

	// format the string into the space after the prefix
	// MSVC: use _vsnprintf_s for consistent varargs / buffer behavior on x64 (vs legacy vsnprintf).
#if defined(_MSC_VER)
	{
		size_t const room = sizeof(buffer) - static_cast<size_t>(prefixLen);
		IGNORE_RETURN(_vsnprintf_s(buffer + prefixLen, room, _TRUNCATE, format, va));
	}
#else
	IGNORE_RETURN(vsnprintf(buffer + prefixLen, sizeof(buffer) - prefixLen - 1, format, va));
#endif
	// Always cap (strlen can read past 8k if the formatted region was not terminated)
	buffer[sizeof(buffer) - 1] = '\0';
	// handle overflow reasonably nicely
	{
		size_t const n = strnlen(buffer, sizeof(buffer));
		if (n >= sizeof(buffer) - 1U)
		{
			buffer[sizeof(buffer) - 3U] = '+';
			buffer[sizeof(buffer) - 2U] = '\n';
		}
	}

	puts(buffer);
}

// ----------------------------------------------------------------------
/**
 * Format and print a debugging message.
 *
 * This routine should never be called directly, but only through the REPORT
 * macros.
 *
 * This routine will send the specified string to the DebugMonitor.  It will
 * also be logged it to the debugger if the RF_log enum was specified.  If
 * the RF_fatal flag was specified, the routine will display a message box
 * with the string in it as well.
 *
 * @internal
 */

void Report::printf(const char *format, ...)
{
	va_list va;

	va_start(va, format);

		vprintf(format, va);

	va_end(va);
}

// ======================================================================
