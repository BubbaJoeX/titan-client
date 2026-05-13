// ======================================================================
//
// ClientPresentation.h
//
// Optional global visual tuning for the legacy SWG client (ground lighting,
// horizon clear color, environment fog density scale, minimum AF, sun specular,
// bloom presentation multipliers).
// Config section: [ClientGraphics/Presentation]
//
// Direct3d9_StaticShaderData reads the same section for minimumTextureAnisotropy
// (no extra DLL linkage).
//
// ======================================================================

#ifndef INCLUDED_ClientPresentation_H
#define INCLUDED_ClientPresentation_H

// ======================================================================

class PackedRgb;

// ======================================================================

class ClientPresentation
{
public:

	static void install();
	static void remove();

	static bool isInstalled();
	static bool isEnabled();

	static float getAmbientLightScale();
	static float getMainDiffuseScale();
	static float getSpecularLightScale();
	static float getFillLightScale();
	static float getBounceLightScale();
	static float getEnvironmentFogDensityScale();
	static float getSkyClearLiftFraction();

	/// Applied to Bloom Gaussian sigma when Presentation is enabled (multiplier).
	static float getBloomStandardDeviationScale();
	/// Applied to Bloom composite weights when Presentation is enabled (multiplier).
	static float getBloomWeightMultiplierScale();

	static void applySkyClearLift(PackedRgb & clearRgb);

private:

	ClientPresentation();
	ClientPresentation(ClientPresentation const &);
	ClientPresentation &operator=(ClientPresentation const &);
};

// ======================================================================

#endif
