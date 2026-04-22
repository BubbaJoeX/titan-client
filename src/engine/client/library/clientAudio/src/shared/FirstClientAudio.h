// ======================================================================
//
// FirstClientAudio.h
// Copyright Sony Online Entertainment
//
// ======================================================================

#ifndef INCLUDED_FirstClientAudio_H
#define INCLUDED_FirstClientAudio_H

// ======================================================================

#include "../../../../../../engine/shared/library/sharedFoundation/include/public/sharedFoundation/FirstSharedFoundation.h"
#include "sharedDebug/FirstSharedDebug.h"
#include "sharedMemoryManager/FirstSharedMemoryManager.h"

#pragma warning(push, 3)
#if defined(SWG_USE_OPENAL)
#include "../win32/OpenALMssShim.h"
#else
#include <mss.h>
#endif
#pragma warning(pop)

// ======================================================================

#endif // INCLUDED_FirstClientAudio_H
