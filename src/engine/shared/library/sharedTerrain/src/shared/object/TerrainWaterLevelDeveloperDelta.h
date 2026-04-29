// ======================================================================
//
// TerrainWaterLevelDeveloperDelta.h
// Localized additive offsets (meters) for procedural water height queries.
//
// ======================================================================

#ifndef INCLUDED_TerrainWaterLevelDeveloperDelta_H
#define INCLUDED_TerrainWaterLevelDeveloperDelta_H

#include <cstdint>
#include <vector>

struct LocalWaterTablePatch
{
	float centerX;
	float centerZ;
	float radius;
	float deltaMeters;
};

class TerrainWaterLevelDeveloperDelta
{
public:

	static float getDeltaAt (float worldX, float worldZ);

	/** Sum |delta| over patches (conservative bound for culling). */
	static float getMaxAbsDelta ();

	static uint32_t getFingerprint ();

	/** Add delta to an existing patch that contains (px,pz), or create a new circular patch. */
	static void applyDeltaAt (float playerX, float playerZ, float deltaMeters);

	static void resetAll ();

	/** Replace patches with a single world-sized offset (legacy / simplified radial tools). */
	static void setDelta (float deltaMeters);

	static void setPatches (std::vector<LocalWaterTablePatch> const & patches);

	static std::vector<LocalWaterTablePatch> getPatchesSnapshot ();

	/** Per-scene cfg: v2 multi-patch or legacy single float line. */
	static bool loadPersistedForScene (char const * sceneNameUtf8);
	static bool savePersistedForScene (char const * sceneNameUtf8);

private:

	static void syncTerrainCache ();
};

#endif
