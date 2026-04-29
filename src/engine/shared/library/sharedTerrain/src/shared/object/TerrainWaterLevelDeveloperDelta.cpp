// ======================================================================
//
// TerrainWaterLevelDeveloperDelta.cpp
//
// ======================================================================

#include "sharedTerrain/FirstSharedTerrain.h"
#include "sharedTerrain/TerrainWaterLevelDeveloperDelta.h"

#include "sharedTerrain/TerrainObject.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

namespace
{
	float const         cms_defaultPatchRadiusMeters = 256.f;
	size_t const        cms_maxPatches             = 48;

	std::vector<LocalWaterTablePatch> s_patches;

	std::string sanitizeSceneName (char const * sceneNameUtf8)
	{
		std::string s = sceneNameUtf8 ? sceneNameUtf8 : "default";
		for (size_t i = 0; i < s.size (); ++i)
		{
			char & c = s[i];
			if (!(std::isalnum (static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == '.'))
				c = '_';
		}
		return s;
	}

	std::string persistPathForScene (char const * sceneNameUtf8)
	{
		return std::string ("developer_local_water_") + sanitizeSceneName (sceneNameUtf8) + ".cfg";
	}

	bool pointInPatch (float x, float z, LocalWaterTablePatch const & p)
	{
		float const dx = x - p.centerX;
		float const dz = z - p.centerZ;
		return (dx * dx + dz * dz) <= (p.radius * p.radius);
	}

	uint32_t floatBits (float f)
	{
		union { float ff; uint32_t uu; } u;
		u.ff = f;
		return u.uu;
	}
}

// ----------------------------------------------------------------------

float TerrainWaterLevelDeveloperDelta::getDeltaAt (float const worldX, float const worldZ)
{
	float sum = 0.f;
	for (size_t i = 0; i < s_patches.size (); ++i)
	{
		LocalWaterTablePatch const & p = s_patches[i];
		if (pointInPatch (worldX, worldZ, p))
			sum += p.deltaMeters;
	}
	return sum;
}

float TerrainWaterLevelDeveloperDelta::getMaxAbsDelta ()
{
	float m = 0.f;
	for (size_t i = 0; i < s_patches.size (); ++i)
		m = std::max (m, std::fabs (s_patches[i].deltaMeters));
	return m;
}

uint32_t TerrainWaterLevelDeveloperDelta::getFingerprint ()
{
	uint32_t h = static_cast<uint32_t>(s_patches.size ());
	for (size_t i = 0; i < s_patches.size (); ++i)
	{
		LocalWaterTablePatch const & p = s_patches[i];
		h = h * 16777619u ^ floatBits (p.centerX);
		h = h * 16777619u ^ floatBits (p.centerZ);
		h = h * 16777619u ^ floatBits (p.radius);
		h = h * 16777619u ^ floatBits (p.deltaMeters);
	}
	return h;
}

void TerrainWaterLevelDeveloperDelta::applyDeltaAt (float const playerX, float const playerZ, float const deltaMeters)
{
	for (size_t i = 0; i < s_patches.size (); ++i)
	{
		if (pointInPatch (playerX, playerZ, s_patches[i]))
		{
			s_patches[i].deltaMeters += deltaMeters;
			syncTerrainCache ();
			return;
		}
	}

	if (s_patches.size () >= cms_maxPatches)
	{
		s_patches.erase (s_patches.begin ());
	}

	LocalWaterTablePatch p;
	p.centerX     = playerX;
	p.centerZ     = playerZ;
	p.radius      = cms_defaultPatchRadiusMeters;
	p.deltaMeters = deltaMeters;
	s_patches.push_back (p);
	syncTerrainCache ();
}

void TerrainWaterLevelDeveloperDelta::resetAll ()
{
	s_patches.clear ();
	syncTerrainCache ();
}

void TerrainWaterLevelDeveloperDelta::setDelta (float const deltaMeters)
{
	LocalWaterTablePatch p;
	p.centerX     = 0.f;
	p.centerZ     = 0.f;
	p.radius      = 1.0e7f;
	p.deltaMeters = deltaMeters;
	std::vector<LocalWaterTablePatch> v;
	v.push_back (p);
	setPatches (v);
}

void TerrainWaterLevelDeveloperDelta::setPatches (std::vector<LocalWaterTablePatch> const & patches)
{
	s_patches = patches;
	if (s_patches.size () > cms_maxPatches)
		s_patches.resize (cms_maxPatches);
	syncTerrainCache ();
}

std::vector<LocalWaterTablePatch> TerrainWaterLevelDeveloperDelta::getPatchesSnapshot ()
{
	return s_patches;
}

void TerrainWaterLevelDeveloperDelta::syncTerrainCache ()
{
	TerrainObject const * const terrain = TerrainObject::getConstInstance ();
	if (terrain)
		terrain->flushWaterHeightCache ();
}

bool TerrainWaterLevelDeveloperDelta::loadPersistedForScene (char const * sceneNameUtf8)
{
	std::string const path = persistPathForScene (sceneNameUtf8);
	FILE * const f = fopen (path.c_str (), "r");
	if (!f)
		return false;

	char line[1024];
	if (!std::fgets (line, sizeof (line), f))
	{
		fclose (f);
		return false;
	}

	std::vector<LocalWaterTablePatch> loaded;

	if (std::strncmp (line, "v2", 2) == 0)
	{
		int n = 0;
		if (std::fscanf (f, "%d", &n) != 1 || n < 0 || n > static_cast<int>(cms_maxPatches))
		{
			fclose (f);
			return false;
		}
		for (int i = 0; i < n; ++i)
		{
			LocalWaterTablePatch p;
			if (std::fscanf (f, "%f %f %f %f", &p.centerX, &p.centerZ, &p.radius, &p.deltaMeters) != 4)
				break;
			loaded.push_back (p);
		}
		if (n == 0)
		{
			fclose (f);
			setPatches (loaded);
			return true;
		}
	}
	else
	{
		float legacy = 0.f;
		if (sscanf (line, "%f", &legacy) == 1)
		{
			LocalWaterTablePatch p;
			p.centerX     = 0.f;
			p.centerZ     = 0.f;
			p.radius      = 1.0e7f;
			p.deltaMeters = legacy;
			loaded.push_back (p);
		}
	}

	fclose (f);

	if (loaded.empty ())
		return false;

	setPatches (loaded);
	return true;
}

bool TerrainWaterLevelDeveloperDelta::savePersistedForScene (char const * sceneNameUtf8)
{
	std::string const path = persistPathForScene (sceneNameUtf8);
	FILE * const f = fopen (path.c_str (), "w");
	if (!f)
		return false;

	fprintf (f, "v2\n");
	fprintf (f, "%zu\n", s_patches.size ());
	for (size_t i = 0; i < s_patches.size (); ++i)
	{
		LocalWaterTablePatch const & p = s_patches[i];
		fprintf (f, "%.6f %.6f %.6f %.6f\n", p.centerX, p.centerZ, p.radius, p.deltaMeters);
	}
	fclose (f);
	return true;
}
