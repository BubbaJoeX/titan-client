//===================================================================
//
// StructurePlacementVisualState.h
//
// Owns transient presentation state for structure placement:
//   - Radial procedural flora draw suppression (see
//     ClientProceduralTerrainAppearance::setRadialFloraDrawSuppressed),
//     without touching the player's terrain flora option flags.
//   - Ctrl+H is sampled from StructurePlacementCamera::alter on Win32;
//     UI KeyDown does not carry that chord in this mode.
// World-object flora may still be skipped via ObjectList when this
// reports hidden (CollisionProperty::isFlora), independent of terrain.
//
//===================================================================

#ifndef INCLUDED_StructurePlacementVisualState_H
#define INCLUDED_StructurePlacementVisualState_H

//===================================================================

class StructurePlacementVisualState
{
public:

	typedef void (*HelpRefreshCallback)(void *context);

	static void setHelpRefreshCallback (HelpRefreshCallback cb, void *context);
	static void placementSessionBegin ();
	static void placementSessionEnd ();
	static void pollChordEachFrame ();
	static bool hidesFlora ();
	static void resetChordLatch ();

private:

	static void clearHelpRefreshCallback ();
};

//===================================================================

#endif
