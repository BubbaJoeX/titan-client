// ======================================================================
//
// ClaimClientMessageDispatch.cpp
//
// ======================================================================

#include "sharedNetworkMessages/FirstSharedNetworkMessages.h"
#include "sharedNetworkMessages/ClaimClientMessageDispatch.h"

#include "sharedNetworkMessages/ClaimFootprintSyncMessage.h"
#include "sharedNetworkMessages/ClaimManipulateStateMessage.h"
#include "sharedNetworkMessages/SceneChannelMessages.h"

// ======================================================================

namespace ClaimClientMessageDispatch
{
	char const * const cms_claimFootprintSync = ClaimFootprintSyncMessage::cms_name;
	char const * const cms_claimManipulateState = ClaimManipulateStateMessage::cms_name;
	char const * const cms_sceneEndBaselines = "SceneEndBaselines";

	bool decodeManipulateState(Archive::ReadIterator & source)
	{
		ClaimManipulateStateMessage const msg(source);
		return msg.getCanManipulate();
	}

	void decodeFootprintSync(Archive::ReadIterator & source, NetworkId & markerIdOut, float & centerXOut, float & centerYOut, float & centerZOut, float & radiusMetersOut, bool & activeOut)
	{
		ClaimFootprintSyncMessage const msg(source);
		markerIdOut = msg.getMarkerId();
		Vector const center = msg.getCenter();
		centerXOut = center.x;
		centerYOut = center.y;
		centerZOut = center.z;
		radiusMetersOut = msg.getRadiusMeters();
		activeOut = msg.getActive();
	}

	void decodeSceneEndBaselines(Archive::ReadIterator & source, NetworkId & networkIdOut)
	{
		SceneEndBaselines const msg(source);
		networkIdOut = msg.getNetworkId();
	}
}

// ======================================================================
