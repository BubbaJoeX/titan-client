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

ClaimFootprintSyncMessage::ClaimFootprintSyncMessage(NetworkId const & markerId, Vector const & center, float const radiusMeters, bool const active) :
	GameNetworkMessage(cms_name),
	m_markerId(markerId),
	m_centerX(center.x),
	m_centerY(center.y),
	m_centerZ(center.z),
	m_radiusMeters(radiusMeters),
	m_active(active)
{
	addVariable(m_markerId);
	addVariable(m_centerX);
	addVariable(m_centerY);
	addVariable(m_centerZ);
	addVariable(m_radiusMeters);
	addVariable(m_active);
}

// ----------------------------------------------------------------------

ClaimFootprintSyncMessage::ClaimFootprintSyncMessage(Archive::ReadIterator & source) :
	GameNetworkMessage(cms_name),
	m_markerId(),
	m_centerX(0.f),
	m_centerY(0.f),
	m_centerZ(0.f),
	m_radiusMeters(0.f),
	m_active(false)
{
	addVariable(m_markerId);
	addVariable(m_centerX);
	addVariable(m_centerY);
	addVariable(m_centerZ);
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

Vector const ClaimFootprintSyncMessage::getCenter() const
{
	return Vector(m_centerX.get(), m_centerY.get(), m_centerZ.get());
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
