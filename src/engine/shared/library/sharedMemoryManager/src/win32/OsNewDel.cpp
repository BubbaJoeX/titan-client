// ======================================================================
//
// OsNewDel.cpp
//
// Copyright 2002 Sony Online Entertainment
//
// ======================================================================

#include "sharedMemoryManager/FirstSharedMemoryManager.h"
#include "sharedMemoryManager/OsNewDel.h"

#include <intrin.h>

// ======================================================================

// We are using the arguments (except for file and line), but MSVC can't tell that.
#pragma warning(disable: 4100)

// ----------------------------------------------------------------------

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

#pragma warning(default: 4100)

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
	UNREF(file);
	UNREF(line);

	if (pointer)
		MemoryManager::free(pointer, false);
}

// ----------------------------------------------------------------------

void operator delete[](void *pointer, const char *file, int line)
{
	UNREF(file);
	UNREF(line);

	if (pointer)
		MemoryManager::free(pointer, true);
}

// ======================================================================
// WARNING!!!!!!!
// 
// The init_seg pragma command is used to create certain static objects first, before other static objects are created.
// However, multiple static variables that use the same init_seg category(i.e. compiler) are not guaranteed to destroy in any 
// particular order. It is completely random based on how all the linking of static objects occurs. Since this command is being
// used on our memory manager(to overwrite new/delete) - NO OTHER STATIC SHOULD EVER USE INIT_SEG(COMPILER)!!!! This static object
// *MUST* be the final static object that is destroyed.
//
#pragma warning(disable: 4074)
#pragma init_seg(compiler) // ^-Read warning above.-^
static MemoryManager memoryManager;

// ======================================================================

