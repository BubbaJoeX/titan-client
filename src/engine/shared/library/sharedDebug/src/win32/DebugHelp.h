// ======================================================================
//
// DebugHelp.h
// copyright 2000 Verant Interactive
//
// ======================================================================

#ifndef DEBUG_HELP_H
#define DEBUG_HELP_H

#include <cstdint>

// ======================================================================

typedef unsigned long uint32;

// ======================================================================

class DebugHelp
{
public:

	static void install();
	static void remove();

	static bool loadSymbolsForDll(const char *name);

	/// Full-width program counter values (required on x64; Win32 is 32-bit uintptr_t).
	static void getCallStack(std::uintptr_t *callStack, int sizeOfCallStack);
	static void reportCallStack(int const maxStackDepth = 4);
	static bool lookupAddress(std::uintptr_t address, char *libName, char *fileName, int fileNameLength, int &line);

	static bool writeMiniDump(char const *miniDumpFileName=0, PEXCEPTION_POINTERS exceptionPointers=0);
};

// ======================================================================

#endif
