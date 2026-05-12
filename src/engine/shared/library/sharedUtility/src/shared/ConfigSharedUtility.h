// ======================================================================
//
// ConfigSharedUtility.h
// Copyright 2004, Sony Online Entertainment Inc.
// All Rights Reserved.
//
// ======================================================================

#ifndef INCLUDED_ConfigSharedUtility_H
#define INCLUDED_ConfigSharedUtility_H

// ======================================================================

class ConfigSharedUtility
{
public:

	static void install();

	static bool getDisableFileCaching();
	static char const * getUseCacheFile();
	static int getChunkSize();
	static bool getLogOptionManager();

	/// Milliseconds per frame spent on loading-screen preload slices (file cache, world snapshot, space preload lists).
	static int getLoadingScreenPreloadBudgetMs();
};

// ======================================================================

#endif
