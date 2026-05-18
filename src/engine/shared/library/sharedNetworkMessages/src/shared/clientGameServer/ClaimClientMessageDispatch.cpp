// ======================================================================
//
// ClaimClientMessageDispatch.cpp
//
// ======================================================================

#include "sharedNetworkMessages/FirstSharedNetworkMessages.h"
#include "sharedNetworkMessages/ClaimClientMessageDispatch.h"

#include "sharedNetworkMessages/ClaimFootprintSyncMessage.h"
#include "sharedNetworkMessages/ClaimManipulateStateMessage.h"

// ======================================================================

namespace ClaimClientMessageDispatch
{
	char const * const cms_claimFootprintSync = ClaimFootprintSyncMessage::cms_name;
	char const * const cms_claimManipulateState = ClaimManipulateStateMessage::cms_name;

	bool decodeManipulateState(Archive::ReadIterator & source)
	{
		unsigned long cmd = 0;
		Archive::get(source, cmd);
		UNREF(cmd);

		unsigned char canManipulate = 0;
		Archive::get(source, canManipulate);
		return canManipulate != 0;
	}

	void decodeFootprintSync(Archive::ReadIterator & source, NetworkId & markerIdOut, float & centerXOut, float & centerYOut, float & centerZOut, float & radiusMetersOut, bool & activeOut)
	{
		unsigned long cmd = 0;
		Archive::get(source, cmd);
		UNREF(cmd);

		Archive::get(source, markerIdOut);
		Archive::get(source, centerXOut);
		Archive::get(source, centerYOut);
		Archive::get(source, centerZOut);
		Archive::get(source, radiusMetersOut);

		bool active = false;
		Archive::get(source, active);
		activeOut = active;
	}
}

// ======================================================================
