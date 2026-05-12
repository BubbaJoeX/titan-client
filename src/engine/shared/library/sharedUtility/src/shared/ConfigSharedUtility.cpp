// ======================================================================
//
// ConfigSharedUtility.cpp
// copyright 2004 Sony Online Entertainment
//
// ======================================================================

#include "sharedUtility/FirstSharedUtility.h"
#include "sharedUtility/ConfigSharedUtility.h"

#include "sharedFoundation/ConfigFile.h"

// ======================================================================

#define KEY_BOOL(a,b)    (ms_ ## a = ConfigFile::getKeyBool("SharedUtility",   #a, (b)))
#define KEY_INT(a,b)     (ms_ ## a = ConfigFile::getKeyInt("SharedUtility",    #a, (b)))
#define KEY_STRING(a,b)  (ms_ ## a = ConfigFile::getKeyString("SharedUtility", #a, (b)))

// ======================================================================

namespace ConfigSharedUtilityNamespace
{
	bool ms_disableFileCaching;
	char const * ms_useCacheFile;
	int ms_chunkSize;
	bool ms_logOptionManager;
	int ms_loadingScreenPreloadBudgetMs;
}

using namespace ConfigSharedUtilityNamespace;

// ======================================================================

void ConfigSharedUtility::install()
{
	KEY_BOOL(disableFileCaching, false);
	KEY_STRING(useCacheFile, "");
	KEY_INT(chunkSize, 32);
	KEY_BOOL(logOptionManager, false);
	KEY_INT(loadingScreenPreloadBudgetMs, 1000);
}

// ----------------------------------------------------------------------

bool ConfigSharedUtility::getDisableFileCaching()
{
	return ms_disableFileCaching;
}

// ----------------------------------------------------------------------

char const * ConfigSharedUtility::getUseCacheFile()
{
	return ms_useCacheFile;
}

// ----------------------------------------------------------------------

int ConfigSharedUtility::getChunkSize()
{
	return ms_chunkSize;
}

// ----------------------------------------------------------------------

bool ConfigSharedUtility::getLogOptionManager()
{
	return ms_logOptionManager;
}

// ----------------------------------------------------------------------

int ConfigSharedUtility::getLoadingScreenPreloadBudgetMs()
{
	int const budget = ms_loadingScreenPreloadBudgetMs;
	if (budget < 1)
		return 1;
	if (budget > 60000)
		return 60000;
	return budget;
}

// ======================================================================
