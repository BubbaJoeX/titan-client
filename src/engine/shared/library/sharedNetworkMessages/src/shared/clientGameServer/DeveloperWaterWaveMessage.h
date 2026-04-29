// ======================================================================
//
// DeveloperWaterWaveMessage.h
// Server -> client: places procedural wave poles at player-centered water.
//
// ======================================================================

#ifndef INCLUDED_DeveloperWaterWaveMessage_H
#define INCLUDED_DeveloperWaterWaveMessage_H

#include "Archive/AutoDeltaByteStream.h"
#include "sharedNetworkMessages/GameNetworkMessage.h"

class DeveloperWaterWaveMessage : public GameNetworkMessage
{
public:

	DeveloperWaterWaveMessage (float centerX, float centerZ, float radius, float amplitude);
	explicit DeveloperWaterWaveMessage (Archive::ReadIterator & source);
	virtual ~DeveloperWaterWaveMessage ();

	float getCenterX () const;
	float getCenterZ () const;
	float getRadius () const;
	float getAmplitude () const;

	static char const * const cms_name;

private:

	Archive::AutoVariable<float> m_centerX;
	Archive::AutoVariable<float> m_centerZ;
	Archive::AutoVariable<float> m_radius;
	Archive::AutoVariable<float> m_amplitude;
};

#endif
