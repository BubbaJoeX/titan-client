// ======================================================================
//
// InstallTimer.cpp
// Copyright 2004 Sony Online Entertainment Inc
// All Rights Reserved
//
// ======================================================================

#include "sharedDebug/FirstSharedDebug.h"
#include "sharedDebug/InstallTimer.h"

#include "sharedFoundation/ConfigFile.h"

#include <cstdint>
#include <cstring>
#include <cstdio>

#if defined(_WIN32)
#include <windows.h>
#endif

// ======================================================================

namespace InstallTimerNamespace
{
	bool ms_enabled;
	int ms_indent = 1;
}
using namespace InstallTimerNamespace;

// ======================================================================

void InstallTimer::enable()
{
	ms_enabled = true;
}

// ----------------------------------------------------------------------

void InstallTimer::checkConfigFile()
{
	ms_enabled = ConfigFile::getKeyBool("SharedDebug/InstallTimer", "enabled", false);
}

// ======================================================================

InstallTimer::InstallTimer(char const * description)
:
	m_description(description),
	m_performanceTimer(),
	m_startingNumberOfBytesAllocated(MemoryManager::getCurrentNumberOfBytesAllocated())
{
	NOT_NULL(m_description);
	m_performanceTimer.start();
	++ms_indent;
}

// ----------------------------------------------------------------------

InstallTimer::~InstallTimer()
{
	manualExit();
}

// ----------------------------------------------------------------------

void InstallTimer::manualExit()
{
	if (m_description)
	{
		m_performanceTimer.stop();
		unsigned long const endingNumberOfBytesAllocated = MemoryManager::getCurrentNumberOfBytesAllocated();
		--ms_indent;
		// Stack-safe line: avoid "%*c" in varargs (MSVC + x64: occasional bad interactions with
		// Report::vprintf) and avoid truncating a large 32-bit unsigned delta to int.
		char padding[200];
		{
			int const padW = (ms_indent >= 0) ? (ms_indent * 2) : 0;
			int n = (padW > static_cast<int>(sizeof(padding) - 1)) ? static_cast<int>(sizeof(padding) - 1) : padW;
			if (n < 0)
				n = 0;
			if (n > 0)
				(void)std::memset(padding, ' ', static_cast<size_t>(n));
			padding[n] = '\0';
		}
		int64_t const delta =
			static_cast<int64_t>(static_cast<uint32_t>(endingNumberOfBytesAllocated))
			- static_cast<int64_t>(static_cast<uint32_t>(m_startingNumberOfBytesAllocated));
		// Do not route through Report::vprintf/puts on Win32. On x64, that path (OutputDebugString +
		// FloatingPointUnit::setControlWord + optional log callbacks) has been a source of "silent" exits
		// right after subsystem milestones; this line only needs a DebugView string.
		if (ms_enabled)
		{
#if defined(_WIN32)
			char line[800];
			(void)_snprintf_s(
				line, sizeof(line), _TRUNCATE,
				"[Titan] InstallTimer:%s%6.4f %I64d %s\r\n",
				padding,
				m_performanceTimer.getElapsedTime(),
				static_cast<__int64>(delta),
				m_description);
			OutputDebugStringA(line);
#else
			REPORT_LOG_PRINT(
				true,
				("InstallTimer:%s%6.4f %I64d %s\n", padding, m_performanceTimer.getElapsedTime(), static_cast<__int64>(delta), m_description));
#endif
		}
		m_description = NULL;
	}
}

// ======================================================================
