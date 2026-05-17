// ======================================================================
//
// CuiClaimManipulateState.h
//
// Client UI cache of whether the player may drop in an open-world claim.
//
// ======================================================================

#ifndef INCLUDED_CuiClaimManipulateState_H
#define INCLUDED_CuiClaimManipulateState_H

// ======================================================================

class CuiClaimManipulateState
{
public:
	static void install();
	static bool canDropInOpenClaim();

private:
	static void onNotify(bool canManipulate);

	static bool ms_canDropInOpenClaim;
};

// ======================================================================

#endif
