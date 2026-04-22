// ======================================================================
//
// RegexServices.cpp
// Copyright 2003 Sony Online Entertainment, Inc.
// All Rights Reserved.
//
// ======================================================================

#include "sharedRegex/FirstSharedRegex.h"
#include "sharedRegex/RegexServices.h"

#include <intrin.h>

// ======================================================================

static void * __cdecl localAllocate(size_t size, uint32 owner, bool array, bool leakTest)
{
	return MemoryManager::allocate(size, owner, array, leakTest);
}

static void * regexAllocate(size_t size)
{
	return localAllocate(size, static_cast<uint32>(reinterpret_cast<uintptr_t>(_ReturnAddress())), false, true);
}

// ----------------------------------------------------------------------

void *RegexServices::allocateMemory(size_t byteCount)
{
	return regexAllocate(byteCount);
}

// ----------------------------------------------------------------------

void RegexServices::freeMemory(void *pointer)
{
	MemoryManager::free(pointer, false);
}

// ======================================================================
