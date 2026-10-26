//======================================================================
//
// SwgCuiHudActionSpace.h
// copyright (c) 2004 Sony Online Entertainment
//
//======================================================================

#ifndef INCLUDED_SwgCuiHudActionSpace_H
#define INCLUDED_SwgCuiHudActionSpace_H

#include "swgClientUserInterface/SwgCuiHudAction.h"

class SwgCuiHudWindowManagerSpace;

//======================================================================

class CuiAction;

class SwgCuiHudActionSpace : public SwgCuiHudAction
{
public:
	                         SwgCuiHudActionSpace (SwgCuiHud & mediator);
	                         ~SwgCuiHudActionSpace ();

	bool                     performAction (const std::string & id, const Unicode::String & params) const;

	// Titan: the ship actions (booster, target lock, weapon groups, component detail, hyperspace map...), shared with
	// the ground HUD in atmospheric flight, which runs the ground HUD while flying. True when `id` was one of them.
	static void              addShipActions      (CuiAction * action);
	static bool              performShipAction   (const std::string & id, const Unicode::String & params, int & boosterDebounceMs, int & targetLockDebounceMs);

private:
	                         SwgCuiHudActionSpace ();
	                         SwgCuiHudActionSpace (const SwgCuiHudActionSpace & rhs);
	SwgCuiHudActionSpace & operator= (const SwgCuiHudActionSpace & rhs);

	SwgCuiHudWindowManagerSpace & getWindowManagerSpace () const;

private:

	mutable int m_toggleDebounceCounterMsBooster;
	mutable int m_toggleDebounceCounterMsTargetLock;
};

//======================================================================

#endif
