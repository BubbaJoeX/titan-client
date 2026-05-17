// ======================================================================
//
// ClaimFootprintSyncMessage.cpp
//
// ======================================================================

#include "sharedNetworkMessages/FirstSharedNetworkMessages.h"
#include "sharedNetworkMessages/ClaimFootprintSyncMessage.h"

// ======================================================================

char const * const ClaimFootprintSyncMessage::cms_name = "ClaimFootprintSyncMessage";

// ======================================================================

ClaimFootprintSyncMessage::ClaimFootprintSyncMessage(NetworkId const & markerId, float const radiusMeters, bool const active) :
	GameNetworkMessage(cms_name),
	m_markerId(markerId),
	m_radiusMeters(radiusMeters),
	m_active(active)
{
	addVariable(m_markerId);
	addVariable(m_radiusMeters);
	addVariable(m_active);
}

// ----------------------------------------------------------------------

ClaimFootprintSyncMessage::ClaimFootprintSyncMessage(Archive::ReadIterator & source) :
	GameNetworkMessage(cms_name),
	m_markerId(),
	m_radiusMeters(0.f),
	m_active(false)
{
	addVariable(m_markerId);
	addVariable(m_radiusMeters);
	addVariable(m_active);
	AutoByteStream::unpack(source);
}

// ----------------------------------------------------------------------

ClaimFootprintSyncMessage::~ClaimFootprintSyncMessage()
{
}

// ----------------------------------------------------------------------

NetworkId const & ClaimFootprintSyncMessage::getMarkerId() const
{
	return m_markerId.get();
}

// ----------------------------------------------------------------------

float ClaimFootprintSyncMessage::getRadiusMeters() const
{
	return m_radiusMeters.get();
}

// ----------------------------------------------------------------------

bool ClaimFootprintSyncMessage::getActive() const
{
	return m_active.get();
}

// ======================================================================
