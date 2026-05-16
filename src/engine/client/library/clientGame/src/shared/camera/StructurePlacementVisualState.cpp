//===================================================================
//
// StructurePlacementVisualState.cpp
//
// Session state for the structure placement UI overlay: transient
// radial-flora draw suppression and a Win32 Ctrl+H chord (polled from
// StructurePlacementCamera because letter keys are not delivered on the
// placement page while input-toggle is inactive).
//
//===================================================================

#include "clientGame/FirstClientGame.h"
#include "clientGame/StructurePlacementVisualState.h"

#include "clientTerrain/ClientProceduralTerrainAppearance.h"

#if defined(WIN32) || defined(_WIN32) || defined(PLATFORM_WIN32)
#include <windows.h>
#endif

//===================================================================

namespace StructurePlacementVisualStateNamespace
{
	bool                    s_sessionActive = false;
	bool                    s_hideFloraOverlay = false;
	bool                    s_chordWasDown = false;

	StructurePlacementVisualState::HelpRefreshCallback s_helpRefresh = 0;
	void*                                                s_helpContext = 0;

	// ----------------------------------------------------------------------
	void syncRadialFloraDrawSuppression ()
	{
		ClientProceduralTerrainAppearance::setRadialFloraDrawSuppressed (
			s_sessionActive && s_hideFloraOverlay);
	}
}

using namespace StructurePlacementVisualStateNamespace;

//===================================================================

void StructurePlacementVisualState::setHelpRefreshCallback (HelpRefreshCallback cb, void *context)
{
	s_helpRefresh = cb;
	s_helpContext = context;
}

//-------------------------------------------------------------------

void StructurePlacementVisualState::clearHelpRefreshCallback ()
{
	s_helpRefresh = 0;
	s_helpContext = 0;
}

//-------------------------------------------------------------------

void StructurePlacementVisualState::resetChordLatch ()
{
	s_chordWasDown = false;
}

//-------------------------------------------------------------------

void StructurePlacementVisualState::placementSessionBegin ()
{
	if (s_sessionActive)
	{
		DEBUG_WARNING (true, ("StructurePlacementVisualState::placementSessionBegin: duplicate begin"));
		return;
	}

	s_sessionActive       = true;
	s_hideFloraOverlay    = false;
	s_chordWasDown        = false;

	syncRadialFloraDrawSuppression ();
}

//-------------------------------------------------------------------

void StructurePlacementVisualState::placementSessionEnd ()
{
	clearHelpRefreshCallback ();

	if (!s_sessionActive)
	{
		ClientProceduralTerrainAppearance::setRadialFloraDrawSuppressed (false);
		return;
	}

	s_sessionActive        = false;
	s_hideFloraOverlay     = false;
	s_chordWasDown         = false;

	syncRadialFloraDrawSuppression ();
}

//-------------------------------------------------------------------

void StructurePlacementVisualState::pollChordEachFrame ()
{
	if (!s_sessionActive)
		return;

#if defined(WIN32) || defined(_WIN32) || defined(PLATFORM_WIN32)
	bool const ctrlDown =
		   ((GetAsyncKeyState (VK_LCONTROL) & 0x8000) != 0)
		|| ((GetAsyncKeyState (VK_RCONTROL) & 0x8000) != 0);
	bool const hDown = (GetAsyncKeyState ('H') & 0x8000) != 0;
	bool const chordDown = ctrlDown && hDown;
	bool const edge      = chordDown && !s_chordWasDown;
	s_chordWasDown = chordDown;

	if (edge)
	{
		s_hideFloraOverlay = !s_hideFloraOverlay;
		syncRadialFloraDrawSuppression ();

		if (s_helpRefresh)
			(*s_helpRefresh)(s_helpContext);
	}
#endif
}

//-------------------------------------------------------------------

bool StructurePlacementVisualState::hidesFlora ()
{
	return s_hideFloraOverlay;
}

//===================================================================
