// ======================================================================
//
// ClientClaimFootprintManager.h
//
// Client-side persistent claim boundary visuals (pylons + ribbon).
//
// ======================================================================

#ifndef INCLUDED_ClientClaimFootprintManager_H
#define INCLUDED_ClientClaimFootprintManager_H

// ======================================================================

#include "sharedFoundation/NetworkId.h"
#include "sharedMath/Vector.h"

// ======================================================================

class ClientClaimFootprintManager
{
public:
	static void install();
	static void remove();

	static void handleSync(NetworkId const & markerId, Vector const & center_w, float radiusMeters, bool active);
	static void handleObjectBaselines(NetworkId const & networkId);
	static void handleManipulateState(bool canManipulate);
	static void clearAll();
	static void update(float elapsedTime);

private:
	ClientClaimFootprintManager();
};

// ======================================================================

#endif
