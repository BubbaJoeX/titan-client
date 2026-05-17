// ======================================================================
//
// ClientClaimManipulateState.cpp
//
// ======================================================================

#include "clientGame/FirstClientGame.h"
#include "clientGame/ClientClaimManipulateState.h"

// ======================================================================

ClientClaimManipulateState::NotifyFunc ClientClaimManipulateState::ms_notifyFunc = 0;

// ----------------------------------------------------------------------

void ClientClaimManipulateState::setNotifyFunc(NotifyFunc const func)
{
	ms_notifyFunc = func;
}

// ----------------------------------------------------------------------

void ClientClaimManipulateState::notify(bool const canManipulate)
{
	if (ms_notifyFunc)
		(*ms_notifyFunc)(canManipulate);
}

// ======================================================================
