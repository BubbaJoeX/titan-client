// ======================================================================
//
// CityTerrainLayerManager.h
// copyright 2026 Titan
//
// Client-side manager for city terrain modifications (shader overlays, flattening)
// ======================================================================

#ifndef INCLUDED_CityTerrainLayerManager_H
#define INCLUDED_CityTerrainLayerManager_H

#include "sharedFoundation/NetworkId.h"
#include "sharedMath/Vector.h"
#include "sharedMath/Vector2d.h"
#include <map>
#include <vector>
#include <string>

class Camera;
class Shader;
class TerrainGenerator;

// ======================================================================

class CityTerrainLayerManager
{
public:
	// Terrain region types
	enum RegionType
	{
		RT_SHADER_CIRCLE = 0,
		RT_SHADER_LINE = 1,
		RT_FLATTEN = 2
	};

	// Structure representing a terrain modification region
	struct TerrainRegion
	{
		std::string regionId;
		int32 cityId;
		RegionType type;
		std::string shaderTemplate;
		float centerX;
		float centerZ;
		float radius;
		float endX;
		float endZ;
		float width;
		float height;
		float blendDistance;
		Shader const * cachedShader;
		bool active;
		int64 timestamp;  // Creation timestamp for priority ordering (newer = higher priority)
		int32 priority;   // Explicit priority (higher = takes precedence)
	};

public:
	static void install();
	static void remove();
	static CityTerrainLayerManager & getInstance();
	static bool isInstalled();

	// Pending city ID for UI activation (set by GameNetwork, read by terrain UI mediators)
	static void setPendingCityId(int32 cityId);
	static int32 getPendingCityId();

	// OpenCityTerrainPainterMessage: pending id + optional callback when the painter is already active
	// (CuiMediator skips performActivate if already active, so the UI must apply the new city here).
	typedef void (*OpenCityTerrainPainterAlreadyActiveFn)();
	static void notifyOpenCityTerrainPainterRequested(int32 cityId);
	static void setOpenCityTerrainPainterAlreadyActiveFn(OpenCityTerrainPainterAlreadyActiveFn fn);
	static void clearOpenCityTerrainPainterAlreadyActiveFn();

	// OpenTerraformingUIMessage: same pattern as the terrain painter.
	typedef void (*OpenTerraformingAlreadyActiveFn)();
	static void notifyOpenTerraformingRequested(int32 cityId);
	static void setOpenTerraformingAlreadyActiveFn(OpenTerraformingAlreadyActiveFn fn);
	static void clearOpenTerraformingAlreadyActiveFn();

	// City radius tracking (for UI use)
	static void setCityRadius(int32 cityId, int32 radius);
	static int32 getCityRadius(int32 cityId);

	// Region management
	void addRegion(const TerrainRegion & region);
	void removeRegion(const std::string & regionId);
	void removeAllRegionsForCity(int32 cityId);
	void clearAllRegions();

	// Query regions
	bool hasRegion(const std::string & regionId) const;
	const TerrainRegion * getRegion(const std::string & regionId) const;
	void getRegionsAtLocation(float x, float z, std::vector<const TerrainRegion *> & outRegions) const;
	void getRegionsForCity(int32 cityId, std::vector<const TerrainRegion *> & outRegions) const;

	// UI listing: if preferredCityId != 0, same as getRegionsForCity; if 0, all loaded regions (sorted).
	void getRegionsForUiList(int32 preferredCityId, std::vector<const TerrainRegion *> & outRegions) const;

	// Shader enumeration: union of shader templates from all terrain/*.trn files (cached).
	void enumerateAvailableShaders(std::vector<std::string> & outTemplates, std::vector<std::string> & outNames) const;
	static void invalidateCachedTerrainShaderList();

	// Height modification for flatten regions (instance method)
	bool getModifiedHeightInternal(float x, float z, float originalHeight, float & outHeight) const;

	// Shader blending query - returns shader override if in a modified region (instance method)
	bool getShaderOverrideInternal(float x, float z, Shader const *& outShader, float & outBlendWeight) const;

	// Static wrappers that check if instance exists
	static bool getModifiedHeight(float x, float z, float originalHeight, float & outHeight);
	static bool getShaderOverride(float x, float z, Shader const *& outShader, float & outBlendWeight);

	// Update/render callback
	void update(float elapsedTime);

	// Sync from server
	void handleTerrainModifyMessage(int32 cityId, int32 modificationType, const std::string & regionId,
		const std::string & shaderTemplate, float centerX, float centerZ, float radius,
		float endX, float endZ, float width, float height, float blendDistance, bool regionActive = true);
	void handleTerrainSyncMessage(int32 cityId, const std::vector<TerrainRegion> & regions);

	// Force terrain regeneration for all modified areas to prevent edge gaps
	void flushTerrainUpdates();

	// Callback for UI to be notified when regions change
	typedef void (*RegionChangeCallback)(int32 cityId);
	static void setRegionChangeCallback(RegionChangeCallback callback);
	static void clearRegionChangeCallback();

	// Server paint RPC result (undo stack + error UI)
	typedef void (*PaintResponseCallback)(bool success, std::string const & regionId, std::string const & errorMessage);
	static void setPaintResponseCallback(PaintResponseCallback callback);
	static void clearPaintResponseCallback();
	static void dispatchPaintResponse(bool success, std::string const & regionId, std::string const & errorMessage);

	// Optional UI refresh (registered by game UI; clientGame must not include SWG headers).
	// cityId 0 means "refresh if the active UI matches" (e.g. paint response has no city id).
	typedef void (*CityTerrainUiRefreshFn)(int32 cityId);
	static void setCityTerrainUiRefreshFn(CityTerrainUiRefreshFn fn);
	static void clearCityTerrainUiRefreshFn();
	static void setCityTerrainUiRefreshSecondaryFn(CityTerrainUiRefreshFn fn);
	static void clearCityTerrainUiRefreshSecondaryFn();
	static void notifyCityTerrainUiRefresh(int32 cityId);

	// World-space terrain tile grid overlay (procedural terrain tileWidth), centered on camera XZ.
	static void setPaintTileGridOverlay(bool visible, int32 cityIdForSpan);
	void addPaintTileGridDebugPrimitives(Camera const & camera) const;

private:
	CityTerrainLayerManager();
	~CityTerrainLayerManager();
	CityTerrainLayerManager(const CityTerrainLayerManager &);
	CityTerrainLayerManager & operator=(const CityTerrainLayerManager &);

	void loadShaderForRegion(TerrainRegion & region);
	bool isPointInCircle(float px, float pz, float cx, float cz, float radius) const;
	bool isPointInLine(float px, float pz, float x1, float z1, float x2, float z2, float width) const;
	float calculateBlendWeight(float distance, float blendDistance) const;
	void rebuildCachedTerrainShaderList() const;

	static CityTerrainLayerManager * ms_instance;
	static int32 ms_pendingCityId;
	static std::map<int32, int32> ms_cityRadii;
	static RegionChangeCallback ms_regionChangeCallback;
	static PaintResponseCallback ms_paintResponseCallback;
	static CityTerrainUiRefreshFn ms_cityTerrainUiRefreshFn;
	static CityTerrainUiRefreshFn ms_cityTerrainUiRefreshSecondaryFn;
	static OpenCityTerrainPainterAlreadyActiveFn ms_openCityTerrainPainterAlreadyActiveFn;
	static OpenTerraformingAlreadyActiveFn ms_openTerraformingAlreadyActiveFn;
	static bool ms_paintTileGridVisible;
	static int32 ms_paintTileGridCityId;

	typedef std::map<std::string, TerrainRegion> RegionMap;
	RegionMap m_regions;

	typedef std::multimap<int32, std::string> CityRegionMap;
	CityRegionMap m_cityRegions;

	// Cached shader list (built from all terrain/*.trn shader groups; cleared via invalidateCachedTerrainShaderList)
	mutable std::vector<std::string> m_cachedShaderTemplates;
	mutable std::vector<std::string> m_cachedShaderNames;
	mutable bool m_terrainShaderCacheDirty;
};

// ======================================================================

#endif // INCLUDED_CityTerrainLayerManager_H

