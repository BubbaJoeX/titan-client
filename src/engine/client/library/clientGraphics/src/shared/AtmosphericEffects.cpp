// ======================================================================
//
// AtmosphericEffects.cpp
//
// ======================================================================

#include "clientGraphics/FirstClientGraphics.h"
#include "clientGraphics/AtmosphericEffects.h"

#include "sharedDebug/InstallTimer.h"
#include "sharedFoundation/ConfigFile.h"
#include "sharedFoundation/ExitChain.h"
#include "sharedFoundation/Misc.h"

namespace AtmosphericEffectsNamespace
{
	char const * const cms_section = "ClientGraphics/AtmosphericEffects";

	bool   ms_installed = false;

	bool   ms_enable                = true;
	float  ms_fogDensityScale       = 1.08f;
	float  ms_fogExponentScale      = 1.0f;
	float  ms_heatShimmerStrength   = 1.15f;
	float  ms_heatShimmerFrequency  = 7.0f;
}

using namespace AtmosphericEffectsNamespace;

// ======================================================================

void AtmosphericEffects::install()
{
	InstallTimer const installTimer("AtmosphericEffects::install");

	DEBUG_FATAL(ms_installed, ("AtmosphericEffects::install called twice"));

	ms_enable               = ConfigFile::getKeyBool(cms_section, "enable", true);
	ms_fogDensityScale      = clamp(0.05f, ConfigFile::getKeyFloat(cms_section, "fogDensityScale", ms_fogDensityScale), 4.0f);
	ms_fogExponentScale     = clamp(0.5f, ConfigFile::getKeyFloat(cms_section, "fogExponentScale", ms_fogExponentScale), 2.5f);
	ms_heatShimmerStrength  = clamp(0.0f, ConfigFile::getKeyFloat(cms_section, "heatShimmerStrength", ms_heatShimmerStrength), 4.0f);
	ms_heatShimmerFrequency = clamp(0.1f, ConfigFile::getKeyFloat(cms_section, "heatShimmerFrequency", ms_heatShimmerFrequency), 40.0f);

	ms_installed = true;
	ExitChain::add(AtmosphericEffects::remove, "AtmosphericEffects::remove");
}

// ----------------------------------------------------------------------

void AtmosphericEffects::remove()
{
	ms_installed = false;
}

// ----------------------------------------------------------------------

bool AtmosphericEffects::isInstalled()
{
	return ms_installed;
}

// ----------------------------------------------------------------------

float AtmosphericEffects::getFogDensityScale()
{
	return ms_installed && ms_enable ? ms_fogDensityScale : 1.f;
}

// ----------------------------------------------------------------------

float AtmosphericEffects::getFogExponentScale()
{
	return ms_installed && ms_enable ? ms_fogExponentScale : 1.f;
}

// ----------------------------------------------------------------------

float AtmosphericEffects::getHeatShimmerStrength()
{
	return ms_installed && ms_enable ? ms_heatShimmerStrength : 0.f;
}

// ----------------------------------------------------------------------

float AtmosphericEffects::getHeatShimmerFrequency()
{
	return ms_installed && ms_enable ? ms_heatShimmerFrequency : 0.f;
}

// ======================================================================
