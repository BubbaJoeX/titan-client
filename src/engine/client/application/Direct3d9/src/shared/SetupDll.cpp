// ======================================================================
//
// SetupDll.cpp
// copyright 1999 Bootprint Entertainment
// copyright 2001 Sony Online Entertainment
//
// This module includes code that was originally from the MSJ BugSlayer
// articles, but has been modified for our purposes.
//
// ======================================================================

#include "FirstDirect3d9.h"
#include "SetupDll.h"

#include "sharedMath/Transform.h"
#include "sharedMath/VectorArgb.h"

#include <DelayImp.h>
#include <float.h>

// ======================================================================

const Transform  Transform::identity;

const VectorArgb VectorArgb::solidBlack  (CONST_REAL(1), CONST_REAL(0),   CONST_REAL(0),   CONST_REAL(0));
const VectorArgb VectorArgb::solidBlue   (CONST_REAL(1), CONST_REAL(0),   CONST_REAL(0),   CONST_REAL(1));
const VectorArgb VectorArgb::solidCyan   (CONST_REAL(1), CONST_REAL(0),   CONST_REAL(1),   CONST_REAL(1));
const VectorArgb VectorArgb::solidGreen  (CONST_REAL(1), CONST_REAL(0),   CONST_REAL(1),   CONST_REAL(0));
const VectorArgb VectorArgb::solidRed    (CONST_REAL(1), CONST_REAL(1),   CONST_REAL(0),   CONST_REAL(0));
const VectorArgb VectorArgb::solidMagenta(CONST_REAL(1), CONST_REAL(1),   CONST_REAL(0),   CONST_REAL(1));
const VectorArgb VectorArgb::solidYellow (CONST_REAL(1), CONST_REAL(1),   CONST_REAL(1),   CONST_REAL(0));
const VectorArgb VectorArgb::solidWhite  (CONST_REAL(1), CONST_REAL(1),   CONST_REAL(1),   CONST_REAL(1));
const VectorArgb VectorArgb::solidGray   (CONST_REAL(1), CONST_REAL(0.5), CONST_REAL(0.5), CONST_REAL(0.5));

// ======================================================================

static FARPROC WINAPI DliHook(unsigned dliNotify, PDelayLoadInfo  pdli)
{
	if (dliNotify == dliNotePreLoadLibrary && _stricmp(pdli->szDll, "dllexport.dll") == 0)
		return reinterpret_cast<FARPROC>(GetModuleHandle(NULL));

	return 0;
}

// ======================================================================

static void MaskFloatingPointExceptions()
{
	// Mask all x87 FPU exceptions to prevent crashes from invalid FPU operations
	// This is necessary because DirectX may trigger FPU exceptions if not masked
	_controlfp(_MCW_EM, _MCW_EM);
}

// ======================================================================

BOOL APIENTRY DllMain(HMODULE, DWORD reason, LPVOID)
{
#if _MSC_VER < 1300
	__pfnDliNotifyHook = DliHook;
#else
	__pfnDliNotifyHook2 = DliHook;
#endif

	if (reason == DLL_PROCESS_ATTACH || reason == DLL_THREAD_ATTACH)
	{
		// Mask floating-point exceptions to prevent crashes from invalid FPU/SSE operations
		// This must be done for each thread as FPU/MXCSR state is per-thread
		MaskFloatingPointExceptions();
	}

	return TRUE;
}

// ======================================================================
