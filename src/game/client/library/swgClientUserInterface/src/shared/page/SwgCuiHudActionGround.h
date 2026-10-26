//======================================================================
//
// SwgCuiHudActionGround.h
// copyright (c) 2004 Sony Online Entertainment
//
//======================================================================

#ifndef INCLUDED_SwgCuiHudActionGround_H
#define INCLUDED_SwgCuiHudActionGround_H

#include "swgClientUserInterface/SwgCuiHudAction.h"

class SwgCuiHudWindowManagerGround;

//======================================================================

class SwgCuiHudActionGround : public SwgCuiHudAction
{
public:
	                         SwgCuiHudActionGround (SwgCuiHud & mediator);
	                         ~SwgCuiHudActionGround ();

	bool                     performAction (const std::string & id, const Unicode::String & params) const;

private:
	                         SwgCuiHudActionGround ();
	                         SwgCuiHudActionGround (const SwgCuiHudActionGround & rhs);
	SwgCuiHudActionGround & operator= (const SwgCuiHudActionGround & rhs);

	SwgCuiHudWindowManagerGround & getWindowManagerGround () const;


	// Titan: the space HUD's ship actions, registered in atmospheric flight (SwgCuiHudActionSpace::performShipAction)
	mutable int m_shipBoosterDebounceMs;
	mutable int m_shipTargetLockDebounceMs;
};

//======================================================================

#endif
