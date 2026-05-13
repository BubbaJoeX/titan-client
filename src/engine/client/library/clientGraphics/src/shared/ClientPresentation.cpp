// ======================================================================
//
// ClientPresentation.cpp
//
// ======================================================================

#include "clientGraphics/FirstClientGraphics.h"
#include "clientGraphics/ClientPresentation.h"

#include "sharedDebug/InstallTimer.h"
#include "sharedFoundation/ConfigFile.h"
#include "sharedFoundation/ExitChain.h"
#include "sharedFoundation/Misc.h"
#include "sharedMath/PackedRgb.h"

namespace ClientPresentationNamespace
{
	char const * const cms_section = "ClientGraphics/Presentation";

	bool   ms_installed = false;

	bool   ms_enable                      = true;
	float  ms_ambientLightScale           = 1.06f;
	float  ms_mainDiffuseScale           = 1.03f;
	float  ms_specularLightScale         = 1.03f;
	float  ms_fillLightScale             = 1.02f;
	float  ms_bounceLightScale           = 1.02f;
	float  ms_environmentFogDensityScale = 1.08f;
	float  ms_skyClearLiftFraction       = 0.035f;
	float  ms_bloomStandardDeviationScale = 1.0f;
	float  ms_bloomWeightMultiplierScale  = 1.0f;
}

using namespace ClientPresentationNamespace;

// ======================================================================

void ClientPresentation::install()
{
	InstallTimer const installTimer("ClientPresentation::install");

	DEBUG_FATAL(ms_installed, ("ClientPresentation::install called twice"));

	ms_enable                      = ConfigFile::getKeyBool(cms_section, "enable", true);
	ms_ambientLightScale           = clamp(0.5f, ConfigFile::getKeyFloat(cms_section, "ambientLightScale", ms_ambientLightScale), 1.85f);
	ms_mainDiffuseScale            = clamp(0.5f, ConfigFile::getKeyFloat(cms_section, "mainDiffuseScale", ms_mainDiffuseScale), 1.85f);
	ms_specularLightScale          = clamp(0.5f, ConfigFile::getKeyFloat(cms_section, "specularLightScale", ms_specularLightScale), 1.85f);
	ms_fillLightScale              = clamp(0.5f, ConfigFile::getKeyFloat(cms_section, "fillLightScale", ms_fillLightScale), 1.85f);
	ms_bounceLightScale            = clamp(0.5f, ConfigFile::getKeyFloat(cms_section, "bounceLightScale", ms_bounceLightScale), 1.85f);
	ms_environmentFogDensityScale  = clamp(0.25f, ConfigFile::getKeyFloat(cms_section, "environmentFogDensityScale", ms_environmentFogDensityScale), 3.0f);
	ms_skyClearLiftFraction        = clamp(0.0f, ConfigFile::getKeyFloat(cms_section, "skyClearLiftFraction", ms_skyClearLiftFraction), 0.15f);
	ms_bloomStandardDeviationScale = clamp(0.25f, ConfigFile::getKeyFloat(cms_section, "bloomStandardDeviationScale", ms_bloomStandardDeviationScale), 3.0f);
	ms_bloomWeightMultiplierScale  = clamp(0.25f, ConfigFile::getKeyFloat(cms_section, "bloomWeightMultiplierScale", ms_bloomWeightMultiplierScale), 4.0f);

	ms_installed = true;
	ExitChain::add(ClientPresentation::remove, "ClientPresentation::remove");
}

// ----------------------------------------------------------------------

void ClientPresentation::remove()
{
	ms_installed = false;
}

// ----------------------------------------------------------------------

bool ClientPresentation::isInstalled()
{
	return ms_installed;
}

// ----------------------------------------------------------------------

bool ClientPresentation::isEnabled()
{
	return ms_installed && ms_enable;
}

// ----------------------------------------------------------------------

float ClientPresentation::getAmbientLightScale()
{
	return ms_ambientLightScale;
}

// ----------------------------------------------------------------------

float ClientPresentation::getMainDiffuseScale()
{
	return ms_mainDiffuseScale;
}

// ----------------------------------------------------------------------

float ClientPresentation::getSpecularLightScale()
{
	return ms_specularLightScale;
}

// ----------------------------------------------------------------------

float ClientPresentation::getFillLightScale()
{
	return ms_fillLightScale;
}

// ----------------------------------------------------------------------

float ClientPresentation::getBounceLightScale()
{
	return ms_bounceLightScale;
}

// ----------------------------------------------------------------------

float ClientPresentation::getEnvironmentFogDensityScale()
{
	return ms_environmentFogDensityScale;
}

// ----------------------------------------------------------------------

float ClientPresentation::getSkyClearLiftFraction()
{
	return ms_skyClearLiftFraction;
}

// ----------------------------------------------------------------------

float ClientPresentation::getBloomStandardDeviationScale()
{
	return ms_bloomStandardDeviationScale;
}

// ----------------------------------------------------------------------

float ClientPresentation::getBloomWeightMultiplierScale()
{
	return ms_bloomWeightMultiplierScale;
}

// ----------------------------------------------------------------------

void ClientPresentation::applySkyClearLift(PackedRgb & clearRgb)
{
	if (!isEnabled() || ms_skyClearLiftFraction <= 0.0f)
		return;

	float const delta = ms_skyClearLiftFraction * 255.0f;
	int const nr = static_cast<int>(static_cast<float>(clearRgb.r) + delta);
	int const ng = static_cast<int>(static_cast<float>(clearRgb.g) + delta);
	int const nb = static_cast<int>(static_cast<float>(clearRgb.b) + delta);
	clearRgb.r = static_cast<uint8>(clamp(0, nr, 255));
	clearRgb.g = static_cast<uint8>(clamp(0, ng, 255));
	clearRgb.b = static_cast<uint8>(clamp(0, nb, 255));
}

// ======================================================================
