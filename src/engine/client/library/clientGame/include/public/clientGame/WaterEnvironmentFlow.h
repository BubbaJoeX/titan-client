// ======================================================================
//
// WaterEnvironmentFlow.h
// Client-only procedural water motion: shared sampling for swim drift and
// global water mesh displacement (CPU path), plus shader constant packing.
//
// ======================================================================

#ifndef INCLUDED_WaterEnvironmentFlow_H
#define INCLUDED_WaterEnvironmentFlow_H

class Vector;

namespace WaterEnvironmentFlow
{
	/** Vertical displacement (meters) from turbulence + optional ripple poles at world XZ. */
	float sampleWaveDisplacementYWorld (float worldX, float worldZ, double timeSeconds);

	/** Horizontal flow for swimming (world XZ); zero when swim flow is disabled in config. */
	Vector computeSwimFlowXZWorld (Vector const &positionWorld, float timeSeconds);
}

#endif
