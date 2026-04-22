// ======================================================================
//
// MemoryManagerHook.cpp
// copyright 1998 Bootprint Entertainment
// copyright 1998 Sony Online Entertainment
//
// ======================================================================

#include "FirstHeadless.h"

#include <intrin.h>

// ======================================================================

// we are using the arguments (except for file and line), but MSVC can't tell that.
#pragma warning(disable: 4100)

// ======================================================================

void *operator new(size_t size, MemoryManagerNotALeak)
{
	return MemoryManager::allocate(size, static_cast<uint32>(reinterpret_cast<uintptr_t>(_ReturnAddress())), false, false);
}

// ----------------------------------------------------------------------

void *operator new(size_t size)
{
	return MemoryManager::allocate(size, static_cast<uint32>(reinterpret_cast<uintptr_t>(_ReturnAddress())), false, true);
}

// ----------------------------------------------------------------------

void *operator new[](size_t size)
{
	return MemoryManager::allocate(size, static_cast<uint32>(reinterpret_cast<uintptr_t>(_ReturnAddress())), true, true);
}

// ----------------------------------------------------------------------

void *operator new(size_t size, const char *file, int line)
{
	return MemoryManager::allocate(size, static_cast<uint32>(reinterpret_cast<uintptr_t>(_ReturnAddress())), false, true);
}

// ----------------------------------------------------------------------

void *operator new[](size_t size, const char *file, int line)
{
	return MemoryManager::allocate(size, static_cast<uint32>(reinterpret_cast<uintptr_t>(_ReturnAddress())), true, true);
}

// ----------------------------------------------------------------------

void operator delete(void *pointer)
{
	if (pointer)
		MemoryManager::free(pointer, false);
}

// ----------------------------------------------------------------------

void operator delete[](void *pointer)
{
	if (pointer)
		MemoryManager::free(pointer, true);
}

// ----------------------------------------------------------------------

void operator delete(void *pointer, const char *file, int line)
{
	if (pointer)
		MemoryManager::free(pointer, true);
}

// ----------------------------------------------------------------------

void operator delete[](void *pointer, const char *file, int line)
{
	if (pointer)
		MemoryManager::free(pointer, true);
}

// ======================================================================
