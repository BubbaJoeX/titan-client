// ======================================================================
//
// DeveloperWaterEnvironmentOverride.cpp
//
// ======================================================================

#include "clientGame/FirstClientGame.h"
#include "clientGame/DeveloperWaterEnvironmentOverride.h"

#include <cmath>

namespace DeveloperWaterEnvironmentOverrideNamespace
{
	bool   s_active      = false;
	float  s_centerX     = 0.f;
	float  s_centerZ     = 0.f;
	float  s_radius      = 55.f;
	float  s_amplitude   = 0.11f;

	float  s_pole0X      = 0.f;
	float  s_pole0Z      = 0.f;
	float  s_pole1X      = 0.f;
	float  s_pole1Z      = 0.f;
	float  s_r0          = 1.f;
	float  s_r1          = 1.f;
	float  s_amp0        = 0.f;
	float  s_amp1        = 0.f;
	float  s_speed0      = 1.35f;
	float  s_speed1      = 1.65f;
	float  s_phase0      = 0.f;
	float  s_phase1      = 2.2f;

	float smoothFalloff (float distanceSquared, float radiusSquared)
	{
		if (radiusSquared <= 1e-8f || distanceSquared >= radiusSquared)
			return 0.f;
		float const t = 1.f - distanceSquared / radiusSquared;
		return t * t;
	}

	float poleDisplacement (float x, float z, double timeSeconds, float poleX, float poleZ, float radius, float amplitude, float speed, float phase)
	{
		float const dx = x - poleX;
		float const dz = z - poleZ;
		float const d2 = dx * dx + dz * dz;
		float const r = (radius > 0.1f) ? radius : 0.1f;
		float const w = smoothFalloff (d2, r * r);
		float const t = static_cast<float>(timeSeconds);
		return w * amplitude * sinf (t * speed + phase);
	}
}

using namespace DeveloperWaterEnvironmentOverrideNamespace;

void DeveloperWaterEnvironmentOverride::applyServerWave (float const centerX, float const centerZ, float const radius, float const amplitude)
{
	s_active    = true;
	s_centerX   = centerX;
	s_centerZ   = centerZ;
	s_radius    = (radius > 1.f) ? radius : 55.f;
	s_amplitude = (amplitude > 1e-5f) ? amplitude : 0.11f;

	float const spread = s_radius * 0.42f;
	s_pole0X = centerX;
	s_pole0Z = centerZ;
	s_pole1X = centerX + spread;
	s_pole1Z = centerZ + spread * 0.73f;

	s_r0    = s_radius;
	s_r1    = s_radius * 0.88f;
	s_amp0  = s_amplitude;
	s_amp1  = s_amplitude * 0.82f;
	s_phase0 = 0.f;
	s_phase1 = 2.17f;
	s_speed0 = 1.35f;
	s_speed1 = 1.62f;
}

void DeveloperWaterEnvironmentOverride::clear ()
{
	s_active = false;
}

bool DeveloperWaterEnvironmentOverride::isActive ()
{
	return s_active;
}

float DeveloperWaterEnvironmentOverride::sampleDisplacement (float const worldX, float const worldZ, double const timeSeconds)
{
	if (!s_active)
		return 0.f;

	return poleDisplacement (worldX, worldZ, timeSeconds, s_pole0X, s_pole0Z, s_r0, s_amp0, s_speed0, s_phase0)
		+ poleDisplacement (worldX, worldZ, timeSeconds, s_pole1X, s_pole1Z, s_r1, s_amp1, s_speed1, s_phase1);
}
