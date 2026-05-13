// ======================================================================
//
// AtmosphericEffects.h
//
// Central knobs for optional world readability upgrades that reuse legacy
// D3D9 paths (vertex fog, heat-buffer composite PS constants).
//
// Fog: scales density before EXP2 upload (FFP + VS VSCR_fog). fogExponentScale reshapes distant falloff without new shaders.
// Heat shimmer: PS user constant packs (time, debugRects, strength, frequency) during heat composite—extend shader/2d_heat_composite
// to modulate UV/distortion from .z/.w (when disabled or pre-update shaders, .z/.w are zero / unused).
//
// True volumetrics (light shafts, depth-aware mist) need a depth RT + fullscreen pass—natural extension point is
// PostProcessingEffectsManager::postSceneRender alongside Bloom.
//
// ======================================================================

#ifndef INCLUDED_AtmosphericEffects_H
#define INCLUDED_AtmosphericEffects_H

// ======================================================================

class AtmosphericEffects
{
public:

	static void install();
	static void remove();

	static bool isInstalled();

	/// Multiplies CellProperty / terrain fog density sent through Graphics::setFog (EXP2 vertex fog pack).
	static float getFogDensityScale();

	/// Extra exponent on scaled fog density before VS constants upload (>1 thickens distant blanket faster).
	static float getFogExponentScale();

	/// Passed as pixel shader user constant Z when compositing heat buffer (`shader/2d_heat_composite`; shader must sample).
	static float getHeatShimmerStrength();

	/// Passed as pixel shader user constant W for shimmer UV phase multiplier.
	static float getHeatShimmerFrequency();

private:

	AtmosphericEffects();
};

// ======================================================================

#endif
