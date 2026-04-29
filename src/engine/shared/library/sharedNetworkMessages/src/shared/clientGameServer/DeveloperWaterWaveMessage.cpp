// ======================================================================
//
// DeveloperWaterWaveMessage.cpp
//
// ======================================================================

#include "sharedNetworkMessages/FirstSharedNetworkMessages.h"
#include "sharedNetworkMessages/DeveloperWaterWaveMessage.h"

char const * const DeveloperWaterWaveMessage::cms_name = "DeveloperWaterWaveMessage";

DeveloperWaterWaveMessage::DeveloperWaterWaveMessage (float const centerX, float const centerZ, float const radius, float const amplitude) :
	GameNetworkMessage (DeveloperWaterWaveMessage::cms_name),
	m_centerX (centerX),
	m_centerZ (centerZ),
	m_radius (radius),
	m_amplitude (amplitude)
{
	addVariable (m_centerX);
	addVariable (m_centerZ);
	addVariable (m_radius);
	addVariable (m_amplitude);
}

DeveloperWaterWaveMessage::DeveloperWaterWaveMessage (Archive::ReadIterator & source) :
	GameNetworkMessage (DeveloperWaterWaveMessage::cms_name),
	m_centerX (),
	m_centerZ (),
	m_radius (),
	m_amplitude ()
{
	addVariable (m_centerX);
	addVariable (m_centerZ);
	addVariable (m_radius);
	addVariable (m_amplitude);
	unpack (source);
}

DeveloperWaterWaveMessage::~DeveloperWaterWaveMessage ()
{
}

float DeveloperWaterWaveMessage::getCenterX () const
{
	return m_centerX.get ();
}

float DeveloperWaterWaveMessage::getCenterZ () const
{
	return m_centerZ.get ();
}

float DeveloperWaterWaveMessage::getRadius () const
{
	return m_radius.get ();
}

float DeveloperWaterWaveMessage::getAmplitude () const
{
	return m_amplitude.get ();
}
