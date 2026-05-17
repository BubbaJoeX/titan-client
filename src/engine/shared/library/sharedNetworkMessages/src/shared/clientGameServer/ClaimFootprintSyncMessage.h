// ======================================================================
//
// ClaimFootprintSyncMessage.h
//
// Syncs open-world claim boundary visuals (pylon + ribbon) to clients.
//
// ======================================================================

#ifndef INCLUDED_ClaimFootprintSyncMessage_H
#define INCLUDED_ClaimFootprintSyncMessage_H

// ======================================================================

#include "sharedFoundation/NetworkId.h"
#include "sharedNetworkMessages/GameNetworkMessage.h"

// ======================================================================

class ClaimFootprintSyncMessage : public GameNetworkMessage
{
public:
	static char const * const cms_name;

	ClaimFootprintSyncMessage(NetworkId const & markerId, float radiusMeters, bool active);
	explicit ClaimFootprintSyncMessage(Archive::ReadIterator & source);
	~ClaimFootprintSyncMessage();

	NetworkId const & getMarkerId() const;
	float               getRadiusMeters() const;
	bool                getActive() const;

private:
	Archive::AutoVariable<NetworkId> m_markerId;
	Archive::AutoVariable<float>     m_radiusMeters;
	Archive::AutoVariable<bool>      m_active;

	ClaimFootprintSyncMessage(ClaimFootprintSyncMessage const &);
	ClaimFootprintSyncMessage & operator=(ClaimFootprintSyncMessage const &);
};

// ======================================================================

#endif
