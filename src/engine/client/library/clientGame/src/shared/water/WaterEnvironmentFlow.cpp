// ======================================================================
//
// WaterEnvironmentFlow.cpp
//
// ======================================================================

#include "clientGame/FirstClientGame.h"
#include "clientGame/WaterEnvironmentFlow.h"

#include "clientGame/ConfigClientGame.h"

#include "sharedMath/Vector.h"

#include <cmath>

namespace WaterEnvironmentFlowNamespace
{
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

using namespace WaterEnvironmentFlowNamespace;

float WaterEnvironmentFlow::sampleWaveDisplacementYWorld (float const worldX, float const worldZ, double const timeSeconds)
{
	float h = 0.f;

	if (ConfigClientGame::getWaterEnvironmentFlowFieldEnabled ())
	{
		float const strength = ConfigClientGame::getWaterEnvironmentFlowStrength ();
		float const k = ConfigClientGame::getWaterEnvironmentFlowTurbulenceScale ();
		float const t = static_cast<float>(timeSeconds);
		h += strength * 0.08f * (sinf (t * 0.71f + worldX * k) * cosf (t * 0.53f + worldZ * k * 1.07f));
	}

	if (ConfigClientGame::getWaterEnvironmentPoleEnabled ())
	{
		h += poleDisplacement (worldX, worldZ, timeSeconds,
			ConfigClientGame::getWaterEnvironmentPole0X (),
			ConfigClientGame::getWaterEnvironmentPole0Z (),
			ConfigClientGame::getWaterEnvironmentPole0Radius (),
			ConfigClientGame::getWaterEnvironmentPole0Amplitude (),
			ConfigClientGame::getWaterEnvironmentPole0Speed (),
			ConfigClientGame::getWaterEnvironmentPole0Phase ());

		h += poleDisplacement (worldX, worldZ, timeSeconds,
			ConfigClientGame::getWaterEnvironmentPole1X (),
			ConfigClientGame::getWaterEnvironmentPole1Z (),
			ConfigClientGame::getWaterEnvironmentPole1Radius (),
			ConfigClientGame::getWaterEnvironmentPole1Amplitude (),
			ConfigClientGame::getWaterEnvironmentPole1Speed (),
			ConfigClientGame::getWaterEnvironmentPole1Phase ());
	}

	return h;
}

Vector WaterEnvironmentFlow::computeSwimFlowXZWorld (Vector const &positionWorld, float const timeSeconds)
{
	if (!ConfigClientGame::getWaterEnvironmentSwimFlowEnabled ())
		return Vector::zero;

	double const t = static_cast<double>(timeSeconds);
	float const x = positionWorld.x;
	float const z = positionWorld.z;

	float const eps = 0.35f;
	float const h0 = sampleWaveDisplacementYWorld (x, z, t);
	float const hx = sampleWaveDisplacementYWorld (x + eps, z, t);
	float const hz = sampleWaveDisplacementYWorld (x, z + eps, t);

	float const dHdx = (hx - h0) / eps;
	float const dHdz = (hz - h0) / eps;

	// Flow perpendicular to height gradient (small-scale stirring / directional swell feel).
	Vector v (-dHdz, 0.f, dHdx);

	float const mag = v.magnitude ();
	float const cap = ConfigClientGame::getWaterEnvironmentSwimFlowMaxSpeed ();
	if (mag > cap && mag > 1e-6f)
		v *= cap / mag;

	return v;
}
