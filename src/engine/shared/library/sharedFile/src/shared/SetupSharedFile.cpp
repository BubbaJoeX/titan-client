// ======================================================================
//
// SetupSharedFile.cpp
// copyright (c) 2001 Sony Online Entertainment
//
// ======================================================================

#include "sharedFile/FirstSharedFile.h"
#include "sharedFile/SetupSharedFile.h"

#include "sharedFile/ConfigSharedFile.h"
#include "sharedFile/FileManifest.h"
#include "sharedFile/FileStreamer.h"
#include "sharedFile/FileStreamerFile.h"
#include "sharedFile/Iff.h"
#include "sharedFile/MemoryFile.h"
#include "sharedFile/OsFile.h"
#include "sharedFile/TreeFile.h"
#include "sharedFile/ZlibFile.h"
#include "sharedDebug/InstallTimer.h"

#include <cstdio>
#ifdef _WIN32
#include <windows.h>
#endif

namespace
{
void titanSetupFileOds(const char *const step)
{
#ifdef _WIN32
	char line[192];
	_snprintf_s(line, sizeof(line), _TRUNCATE, "[Titan] SetupSharedFile: %s\r\n", step ? step : "");
	OutputDebugStringA(line);
#else
	(void)step;
#endif
}
} // namespace

// ======================================================================

void SetupSharedFile::install(bool useFileStreamer, uint32 skuBits)
{
	InstallTimer const installTimer("SetupSharedFile::install");

	titanSetupFileOds("begin install");
	ConfigSharedFile::install();
	titanSetupFileOds("after ConfigSharedFile::install");
	FileStreamerFile::install();
	titanSetupFileOds("after FileStreamerFile::install");
	FileStreamer::install(useFileStreamer);
	titanSetupFileOds("after FileStreamer::install");
	OsFile::install();
	titanSetupFileOds("after OsFile::install");
	TreeFile::install(skuBits);
	titanSetupFileOds("after TreeFile::install (sku scan / path index)");
	FileManifest::install();
	titanSetupFileOds("after FileManifest::install");
	MemoryFile::install();
	titanSetupFileOds("after MemoryFile::install");
	ZlibFile::install();
	titanSetupFileOds("after ZlibFile::install");
	Iff::install();
	titanSetupFileOds("after Iff::install (SetupSharedFile complete)");
}

// ======================================================================
