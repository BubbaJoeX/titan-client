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

#include <cstdio>
#include <cstring>

extern void externalCommandHandler(const char*);

// Config and TreeFile paths in titan_d.cfg are relative to the process working directory.
// Visual Studio defaults the debugger CWD to $(ProjectDir) or $(OutDir), so titan_d.cfg / tres/
// are often missing and graphics or data init fails with no visible output. Always prefer the exe directory.
void TitanAppendBootLog(char const *line)
{
	char exePath[MAX_PATH];
	if (!GetModuleFileNameA(nullptr, exePath, MAX_PATH))
		return;
	char *const lastSlash = strrchr(exePath, '\\');
	if (lastSlash)
		*lastSlash = '\0';
	char logsDir[MAX_PATH];
	_snprintf_s(logsDir, sizeof(logsDir), _TRUNCATE, "%s\\logs", exePath);
	CreateDirectoryA(logsDir, nullptr);
	char logPath[MAX_PATH];
	_snprintf_s(logPath, sizeof(logPath), _TRUNCATE, "%s\\SwgTitan_boot.log", logsDir);
	FILE *fp = nullptr;
	if (fopen_s(&fp, logPath, "a") != 0 || !fp)
		return;
	fprintf(fp, "%s\n", line ? line : "");
	fclose(fp);
}

static void TitanSetWorkingDirectoryToExeDirectory()
{
	char skip[4];
	if (GetEnvironmentVariableA("SWGCLIENT_NO_SET_CWD", skip, sizeof(skip)) > 0)
	{
		TitanAppendBootLog("Titan: SWGCLIENT_NO_SET_CWD set; leaving process CWD unchanged");
		return;
	}
	char module[MAX_PATH];
	if (!GetModuleFileNameA(nullptr, module, MAX_PATH))
		return;
	char *const lastSlash = strrchr(module, '\\');
	if (!lastSlash)
		return;
	*lastSlash = '\0';
	if (!SetCurrentDirectoryA(module))
	{
		TitanAppendBootLog("Titan: SetCurrentDirectory to exe dir FAILED");
		return;
	}
	char cwd[MAX_PATH];
	if (GetCurrentDirectoryA(sizeof(cwd), cwd))
	{
		char msg[MAX_PATH + 64];
		_snprintf_s(msg, sizeof(msg), _TRUNCATE, "Titan: SetCurrentDirectory OK -> %s", cwd);
		TitanAppendBootLog(msg);
	}
}

#ifdef _DEBUG
// Set SWGCLIENT_ATTACH_CONSOLE=1 before launch to get a console for printf / stderr.
// Engine log lines still need [SharedLog] logStderr=true in titan_d.cfg (or use VS Output → Debug for OutputDebugString).
static void AttachDebugConsoleFromEnv()
{
	char buf[8];
	if (GetEnvironmentVariableA("SWGCLIENT_ATTACH_CONSOLE", buf, sizeof(buf)) == 0)
		return;
	if (!AllocConsole())
		return;
	FILE *fp = nullptr;
	IGNORE_RETURN(freopen_s(&fp, "CONOUT$", "w", stdout));
	IGNORE_RETURN(freopen_s(&fp, "CONOUT$", "w", stderr));
	IGNORE_RETURN(freopen_s(&fp, "CONIN$", "r", stdin));
	SetConsoleTitleA("SWG Titan (debug console)");
}
#endif

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
	// we use 75% of the available ram, up to 1536mb in the case of 2gb (32 bit without PAE limit)
	MEMORYSTATUSEX memoryStatus = { sizeof memoryStatus };
	GlobalMemoryStatusEx(&memoryStatus);
	int ramMB = (memoryStatus.ullTotalPhys / 1048576);

	// without PAE enabled 2048 is the max we can do, but SWG crashes if we give it all the RAM sometimes
	if (ramMB >= 2048)
	{
		ramMB = 1536;
	}
	else 
	{
		ramMB = (ramMB * .75);
	}

	MemoryManager::setLimit(ramMB, false, false);
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

		if (reinterpret_cast<intptr_t>(result) < 32) //Pulled straight from MSDN -ARH
		{
			WARNING(true, ("could not launch external application (%td)", reinterpret_cast<intptr_t>(result)));
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
	TitanAppendBootLog("Titan: WinMain entry");
#ifdef _DEBUG
	AttachDebugConsoleFromEnv();
#endif
	{
		char exePath[MAX_PATH];
		if (GetModuleFileNameA(nullptr, exePath, MAX_PATH))
		{
			char line[MAX_PATH + 32];
			_snprintf_s(line, sizeof(line), _TRUNCATE, "Titan: executable path: %s", exePath);
			TitanAppendBootLog(line);
		}
	}

	TitanSetWorkingDirectoryToExeDirectory();

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
