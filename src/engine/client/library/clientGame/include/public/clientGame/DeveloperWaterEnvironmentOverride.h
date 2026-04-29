// ======================================================================
//
// DeveloperWaterEnvironmentOverride.h
// Runtime wave poles from /developer makeWave (server message).
//
// ======================================================================

#ifndef INCLUDED_DeveloperWaterEnvironmentOverride_H
#define INCLUDED_DeveloperWaterEnvironmentOverride_H

class DeveloperWaterEnvironmentOverride
{
public:

	static void applyServerWave (float centerX, float centerZ, float radius, float amplitude);
	static void clear ();

	static bool isActive ();

	/** Extra vertical displacement (meters), summed in WaterEnvironmentFlow. */
	static float sampleDisplacement (float worldX, float worldZ, double timeSeconds);
};

#endif
