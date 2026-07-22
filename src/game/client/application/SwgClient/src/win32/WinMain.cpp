// ======================================================================
//
// WinMain.cpp
//
// ======================================================================

#include "FirstSwgClient.h"

#include "ClientMain.h"

#include "LocalizedString.h"
#include "StringId.h"

#include "Archive/ByteStream.h"
#include "clientGame/Game.h"
#include "../../../../../../engine/shared/library/sharedFoundation/include/public/sharedFoundation/Production.h"
#include "../../../../../../engine/shared/library/sharedGame/include/public/sharedGame/PlatformFeatureBits.h"

#include <shellapi.h>

extern void externalCommandHandler(const char*);

// ======================================================================

static bool SetUserSelectedMemoryManagerTarget()
{
	char buffer[32];
	DWORD result = GetEnvironmentVariable("SWGCLIENT_MEMORY_SIZE_MB", buffer, sizeof(buffer));

	// make sure the environment variable was set
	if (result <= 0 || result >= sizeof(buffer))
		return false;

	// inline atoi() because the crt hasn't been initialized yet
	int megabytes = 0;
	for (char const * b = buffer; *b; ++b)
	{
		// handle bad characters in the environment variable by ignoring the whole thing
		if (*b < '0' || *b > '9')
			return false;

		megabytes = (megabytes * 10) + (*b - '0');
	}

	MemoryManager::setLimit(megabytes, false, false);
	return true;
}

// ----------------------------------------------------------------------

static void SetDefaultMemoryManagerTargetSize()
{
	MEMORYSTATUSEX memoryStatus = { sizeof memoryStatus };
	GlobalMemoryStatusEx(&memoryStatus);
	int ramMB = static_cast<int>(memoryStatus.ullTotalPhys / 1048576);

#if defined(_WIN64)
	// x64: use 75% of physical RAM, capped at 12 GB. The legacy 1536 MB cap was a
	// 32-bit address-space workaround and causes a perf cliff on modern machines.
	int targetMB = static_cast<int>(ramMB * 0.75);
	if (targetMB > 12288)
		targetMB = 12288;
	MemoryManager::setLimit(targetMB, false, false);
#else
	// Win32: 75% of RAM, capped at 1536 MB when >= 2 GB (no-PAE safety).
	if (ramMB >= 2048)
		ramMB = 1536;
	else
		ramMB = static_cast<int>(ramMB * 0.75);
	MemoryManager::setLimit(ramMB, false, false);
#endif
}

void externalCommandHandler(const char* command)
{
	const StringId trialNagId("client", "npe_nag_url_trial");
	const StringId rentalNagId("client", "npe_nag_url_rental");

	Unicode::String url;

	if ((Game::getSubscriptionFeatureBits() & ClientSubscriptionFeature::NPENagForTrial) != 0)
	{
		url = trialNagId.localize();
	}

	if (!url.empty())
	{
		Unicode::NarrowString url8 = Unicode::wideToNarrow( url );

		HINSTANCE result = ShellExecute(NULL, "open", url8.c_str(), NULL, NULL, SW_SHOWNORMAL);

		if (reinterpret_cast<intptr_t>(result) <= 32) //Pulled straight from MSDN -ARH
		{
			WARNING(true, ("could not launch external application (%p)", result));
		}
		else
		{
			Game::quit();
		}
	}
}

// ======================================================================
// Entry point for the application
//
// Return Value:
//
//   Result code to return to the operating system
//
// Remarks:
//
//   This routine should set up the engine, invoke the main game loop,
//   and then tear down the engine.

int WINAPI WinMain(
	HINSTANCE hInstance,      // handle to current instance
	HINSTANCE hPrevInstance,  // handle to previous instance
	LPSTR     lpCmdLine,      // pointer to command line
	int       nCmdShow        // show state of window
	)
{
	if (!SetUserSelectedMemoryManagerTarget())
		SetDefaultMemoryManagerTargetSize();

	try
	{
		return ClientMain(hInstance, hPrevInstance, lpCmdLine, nCmdShow);
	}
	catch (Archive::ReadException const & ex)
	{
		MessageBoxA(NULL, ex.what(), "Archive::ReadException", MB_OK | MB_ICONERROR);
		return -1;
	}
}

// ======================================================================
