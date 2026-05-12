// ======================================================================
//
// ConfigSharedFile.h
// Copyright 2002, Sony Online Entertainment Inc.
// All Rights Reserved.
//
// ======================================================================

#ifndef INCLUDED_ConfigSharedFile_H
#define INCLUDED_ConfigSharedFile_H

// ======================================================================

class ConfigSharedFile
{
public:
	static void install();

	static bool        getEnableAsynchronousLoader();
	static int         getAsynchronousLoaderPriority();
	static int         getAsynchronousLoaderCallbacksPerFrame();
	/// If > 0, the loader worker postpones new work while unreleased async payloads exceed this many bytes (2003-era RAM guard). Use 0 to disable.
	static int         getAsynchronousLoaderPostponeThresholdBytes();
	static bool        getValidateIff();
	static int         getNumberOfTreeFilePreloads();
	static char const * getTreeFilePreload(int index);
};

// ======================================================================

#endif
