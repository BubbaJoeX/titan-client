// ======================================================================
//
// CuiClaimManipulateState.cpp
//
// ======================================================================

#include "clientUserInterface/FirstClientUserInterface.h"
#include "clientUserInterface/CuiClaimManipulateState.h"

#include "clientGame/ClientClaimManipulateState.h"

// ======================================================================

bool CuiClaimManipulateState::ms_canDropInOpenClaim = false;

// ----------------------------------------------------------------------

void CuiClaimManipulateState::install()
{
	ClientClaimManipulateState::setNotifyFunc(&CuiClaimManipulateState::onNotify);
}

// ----------------------------------------------------------------------

bool CuiClaimManipulateState::canDropInOpenClaim()
{
	return ms_canDropInOpenClaim;
}

// ----------------------------------------------------------------------

void CuiClaimManipulateState::onNotify(bool const canManipulate)
{
	ms_canDropInOpenClaim = canManipulate;
}

// ======================================================================
