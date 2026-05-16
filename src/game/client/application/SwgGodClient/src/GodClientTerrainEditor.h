// ======================================================================
//
// GodClientTerrainEditor.h
// copyright (c) 2001-2026 Sony Online Entertainment
//
// Real-time terrain editing system for the God Client
// Provides direct terrain height/shader modification with visual feedback
// Full integration with TerrainGenerator for .trn export
//
// ======================================================================

#ifndef INCLUDED_GodClientTerrainEditor_H
#define INCLUDED_GodClientTerrainEditor_H

// ======================================================================

#include "sharedMath/Vector.h"
#include "sharedMath/Vector2d.h"
#include "sharedMath/Rectangle2d.h"
#include "sharedMath/PackedRgb.h"
#include "sharedSynchronization/Mutex.h"
#include <vector>
#include <map>
#include <string>
#include <set>

class Camera;
class TerrainObject;
class ClientProceduralTerrainAppearance;
class TerrainGenerator;
class AffectorRibbon;
class AffectorRoad;
class AffectorShaderConstant;
class AffectorFloraStaticCollidableConstant;
class AffectorFloraStaticNonCollidableConstant;
class BoundaryCircle;
class BoundaryRectangle;
class BoundaryPolygon;

// ======================================================================

class GodClientTerrainEditor
{
public:

	// Tool modes
	enum ToolMode
	{
		TM_None = 0,
		TM_Raise,
		TM_Lower,
		TM_Flatten,
		TM_Smooth,
		TM_Noise,
		TM_SetHeight,
		TM_PaintShader,
		TM_PaintFlora,
		TM_PlaceWater,
		TM_PlaceRadial,
		TM_PlaceRibbon,
		TM_PlaceRoad,
		TM_PlaceEnvironment,
		/// Closed polygon: marks tiles excluded (underground / no procedural mesh in volume).
		TM_PlaceExcludeTerrain,
		/// Closed polygon boundary layer (feather from environment zone feather if UI adds it later; default 0).
		TM_PlaceBoundaryPolygon,
		/// Open polyline boundary corridor (width from polyline width; min enforced in commit).
		TM_PlaceBoundaryPolyline,
		/// Same as boundary polyline but intended for wide road masks (defaults wider in TerrainDock).
		TM_PlaceBoundaryPolyRoad,
		TM_StampBitmap,
		TM_Select,
		TM_Count
	};

	// Brush shapes
	enum BrushShape
	{
		BS_Circle = 0,
		BS_Square,
		BS_Count
	};

	// Falloff types
	enum FalloffType
	{
		FT_Linear = 0,
		FT_Smooth,
		FT_Sharp,
		FT_Flat,
		FT_Count
	};

	// Polyline editing state
	enum PolylineEditMode
	{
		PEM_None = 0,
		PEM_AddPoints,
		PEM_MovePoint,
		PEM_DeletePoint,
		PEM_InsertPoint
	};

	/// What finalizePolyline() should build (roads/ribbons vs boundary polylines).
	enum PolylineCommitKind
	{
		PCK_RoadRibbon = 0,
		PCK_BoundaryPolyline,
		PCK_BoundaryPolyRoad
	};

	/// LMB polygon drawing for environment / exclude / boundary polygon tools.
	enum PolygonDrawPurpose
	{
		PDP_None = 0,
		PDP_EnvironmentZone,
		PDP_ExcludeTerrain,
		PDP_BoundaryPolygon
	};

	// Height modification entry (for undo and real-time modification)
	struct HeightModification
	{
		float worldX;
		float worldZ;
		float originalHeight;
		float modifiedHeight;
		int64 timestamp;
	};

	// Shader modification entry
	struct ShaderModification
	{
		float worldX;
		float worldZ;
		int originalFamilyId;
		int modifiedFamilyId;
		float featherAmount;
	};

	// Flora modification entry
	struct FloraModification
	{
		float worldX;
		float worldZ;
		int originalFamilyId;
		int modifiedFamilyId;
		float density;
		bool collidable;
	};

	// Dynamic radial flora (RadialGroup paint) overlay
	struct RadialModification
	{
		float worldX;
		float worldZ;
		int originalFamilyId;
		int modifiedFamilyId;
		float childChoice;
	};

	/// Snapshot of shader overlay map edits for undo/redo (exact cell keys).
	struct ShaderStrokeRecord
	{
		uint64 key;
		bool hadPrior;
		ShaderModification prior;
		ShaderModification after;
	};

	struct VertexColorModification
	{
		float worldX;
		float worldZ;
		PackedRgb color;
		float blendAmount;
	};

	struct VertexColorStrokeRecord
	{
		uint64 key;
		bool hadPrior;
		VertexColorModification prior;
		VertexColorModification after;
	};

	// Brush stroke data
	struct BrushStroke
	{
		float centerX;
		float centerZ;
		float radius;
		float strength;
		ToolMode tool;
		float targetHeight;
		std::vector<HeightModification> modifications;
		std::vector<ShaderModification> shaderModifications;
		std::vector<FloraModification> floraModifications;
		std::vector<ShaderStrokeRecord> shaderStrokeRecords;
		std::vector<VertexColorStrokeRecord> vertexColorStrokeRecords;
	};

	// Polyline control point with height
	struct ControlPoint
	{
		Vector2d position;
		float height;
		float width;
	};

	// Active polyline being edited (for roads/ribbons)
	struct ActivePolyline
	{
		std::vector<ControlPoint> controlPoints;
		float width;
		int shaderFamilyId;
		float featherDistance;
		bool hasFixedHeights;
		bool isRibbon;
		PolylineCommitKind commitKind;
		std::string name;
	};

	// Environment zone definition
	struct EnvironmentZone
	{
		std::vector<Vector2d> boundaryPoints;
		int environmentFamilyId;
		float featherDistance;
		std::string name;
	};

	// Bitmap stamp definition
	struct BitmapStamp
	{
		std::string bitmapName;
		float rotation;
		float scale;
		bool affectsHeight;
		bool affectsShader;
		float heightScale;
		int shaderFamilyId;
	};

public:

	static void install();
	static void remove();
	static GodClientTerrainEditor& getInstance();
	/// Nullable; usable in Release when \ref getInstance's DEBUG_FATAL checks are stripped.
	static GodClientTerrainEditor* getInstanceNullable();
	static bool isInstalled();

	// Tool and brush settings
	void setToolMode(ToolMode mode);
	ToolMode getToolMode() const;

	void setBrushSize(float size);
	float getBrushSize() const;

	void setBrushStrength(float strength);
	float getBrushStrength() const;

	float getRaiseLowerSpeed() const;
	float getRaiseLowerBias() const;
	float getRaiseLowerClickRate() const;
	float getRaiseLowerJitter() const;

	/// Raise/Lower: meters applied at brush center per dab (before falloff / bias / jitter).
	void setRaiseLowerSpeed(float metersPerDab);
	/// -1 favors valleys, +1 favors peaks (relative to brush-area average height).
	void setRaiseLowerBias(float bias);
	/// Multiplier on click throttle when not stroke-painting (1 = default ~80Hz cap).
	void setRaiseLowerClickRate(float multiplier);
	/// 0..1 random scale variation per dab (requires Random::install).
	void setRaiseLowerJitter(float jitter);

	void setBrushShape(BrushShape shape);
	BrushShape getBrushShape() const;

	void setFalloffType(FalloffType type);
	FalloffType getFalloffType() const;

	void setBrushFeather(float feather);
	float getBrushFeather() const;

	void setTargetHeight(float height);
	float getTargetHeight() const;

	void setNoiseAmplitude(float amplitude);
	float getNoiseAmplitude() const;

	void setNoiseFrequency(float frequency);
	float getNoiseFrequency() const;

	void setSelectedShaderFamily(int familyId);
	int getSelectedShaderFamily() const;

	/// When true, TM_PaintShader brush applies a vertex color tint overlay instead of changing shader family.
	void setShaderPaintTintMode(bool enabled);
	bool getShaderPaintTintMode() const;
	void setShaderPaintTintRgb(uint8 r, uint8 g, uint8 b);

	void setSelectedFloraFamily(int familyId);
	int getSelectedFloraFamily() const;

	void setSelectedRadialFamily(int familyId);
	int getSelectedRadialFamily() const;

	void setFloraCollidable(bool collidable);
	bool getFloraCollidable() const;

	void setFloraDensity(float density);
	float getFloraDensity() const;

	// Brush preview
	void setBrushPreviewEnabled(bool enabled);
	bool isBrushPreviewEnabled() const;

	void setCursorWorldPosition(const Vector& position);
	const Vector& getCursorWorldPosition() const;

	// Terrain modification
	bool beginBrushStroke(float worldX, float worldZ);
	void continueBrushStroke(float worldX, float worldZ);
	void endBrushStroke();
	bool isBrushStrokeActive() const;

	// Apply brush at a single point
	void applyBrushAtPoint(float worldX, float worldZ);

	/// Single shader paint application (e.g. TerrainDock); does not manage stroke dirty rects.
	void applyShaderPaintDab(float worldX, float worldZ, int shaderFamilyId, float strength);

	/// Vertex color tint dab (live overlay; does not select a shader family).
	void applyVertexColorPaintDab(float worldX, float worldZ, PackedRgb const& rgb, float strength);

	// Height modification queries (called by terrain system)
	static bool getModifiedHeight(float x, float z, float originalHeight, float& outHeight);
	bool getModifiedHeightInternal(float x, float z, float originalHeight, float& outHeight) const;

	// Shader modification queries
	static bool getModifiedShader(float x, float z, int originalFamilyId, int& outFamilyId, float& outFeather);
	bool getModifiedShaderInternal(float x, float z, int originalFamilyId, int& outFamilyId, float& outFeather) const;

	static bool getModifiedVertexColor(float x, float z, PackedRgb const& original, PackedRgb& out);
	bool getModifiedVertexColorInternal(float x, float z, PackedRgb const& original, PackedRgb& out) const;

	// Flora modification queries  
	static bool getModifiedFlora(float x, float z, int originalFamilyId, int& outFamilyId, float& outDensity);
	bool getModifiedFloraInternal(float x, float z, int originalFamilyId, int& outFamilyId, float& outDensity) const;

	static bool getModifiedRadial(float x, float z, int originalFamilyId, int& outFamilyId, float& outChildChoice);
	bool getModifiedRadialInternal(float x, float z, int originalFamilyId, int& outFamilyId, float& outChildChoice) const;

	// Undo/redo
	bool canUndo() const;
	bool canRedo() const;
	void undo();
	void redo();
	void clearHistory();

	// Flush changes to terrain (trigger regeneration)
	void flushTerrainChanges();

	void setWaterPlacementHeight(float height);
	float getWaterPlacementHeight() const;
	void setWaterPlacementShaderTemplate(char const* shaderTemplateName);
	/// Ribbon water surface template basename (e.g. wter_river_water); used when committing ribbons.
	void setRibbonWaterShaderTemplate(char const* shaderTemplateName);

	/// Uses an axis-aligned square [center±halfExtent] clipped as a BoundaryRectangle water table (+ generator + rebuild client meshes).
	void installLocalWaterTableAxisAligned(float centerWorldX, float centerWorldZ, float halfExtentSquare, float tableHeight);

	// Render brush preview
	void renderBrushPreview(const Camera& camera) const;

	// God Client visualization overlays (debug primitives; wireframe is a coarse terrain grid)
	void renderTerrainDebugOverlays(const Camera& camera, bool wireframeGrid, bool heightColorGrid, bool chunkBoundsGrid) const;
	void renderRegionSelectionOverlay(
		const Camera& camera,
		float minX,
		float minZ,
		float maxX,
		float maxZ,
		bool circularSelection,
		float circleCenterX,
		float circleCenterZ,
		float circleRadius) const;

	// Apply sampled heights (row-major nx*nz) with editor undo support. World cell (base+ix, base+iz) where
	// baseX = floor(min(minX,maxX)), baseZ = floor(min(minZ,maxZ)). When cellMaskRowMajor is non-null, only non-zero mask cells are written.
	bool applyRectangularHeightSamples(
		float minX,
		float minZ,
		float maxX,
		float maxZ,
		int nx,
		int nz,
		const float* heightsRowMajor,
		const unsigned char* cellMaskRowMajor = 0);

	/// Repeat the active height brush (raise/lower/flatten/smooth/noise/set height) across the region using brush size/shape/falloff.
	/// Respects circular vs rectangular region selection. One undo stroke for the whole fill when possible.
	bool applyRegionBrushFillHeightTools();

	/// Begin/end batching shader-overlay undo records for operations that are not a tracked brush drag (dab / rect fill).
	void beginShaderUndoBatch();
	void endShaderUndoBatch();

	/// Fill the axis-aligned rectangle (world XZ) with live shader paint (same overlay as brush).
	/// When circularClip is true, only cells inside the world XZ circle are painted (axis-aligned bounds still min/max).
	bool applyRectangularShaderPaint(
		float minX,
		float minZ,
		float maxX,
		float maxZ,
		int shaderFamilyId,
		float strength,
		bool circularClip = false,
		float circleCenterX = 0.f,
		float circleCenterZ = 0.f,
		float circleRadius = 0.f);

	/// Fill a world XZ rectangle (optionally circular) with the vertex color tint overlay.
	bool applyRectangularVertexColorPaint(
		float minX,
		float minZ,
		float maxX,
		float maxZ,
		PackedRgb const& rgb,
		float strength,
		bool circularClip = false,
		float circleCenterX = 0.f,
		float circleCenterZ = 0.f,
		float circleRadius = 0.f);

	/// Apply exclude + non-passable affectors inside a world rectangle (TerrainGenerator layers).
	bool applyRectangleExcludeAndNonPassable(float minX, float minZ, float maxX, float maxZ);

	static void nudgeGodClientCameraToRefreshDpvs();

	// Region selection (bounds are always axis-aligned; circular mode uses center + radius for overlay and masks).
	void setRegionSelection(
		float minX,
		float minZ,
		float maxX,
		float maxZ,
		bool circularSelection = false,
		float circleCenterX = 0.f,
		float circleCenterZ = 0.f,
		float circleRadius = 0.f);
	void clearRegionSelection();
	bool hasRegionSelection() const;

	// ======================================================================
	// Polyline Editing (Roads/Ribbons)
	// ======================================================================

	// Polyline edit mode
	void setPolylineEditMode(PolylineEditMode mode);
	PolylineEditMode getPolylineEditMode() const;

	// Start a new polyline for road, ribbon, or boundary polyline tools
	void beginPolyline(bool isRibbon, PolylineCommitKind commitKind = PCK_RoadRibbon);
	void addPolylinePoint(float worldX, float worldZ, float height = 0.0f);
	void movePolylinePoint(int pointIndex, float worldX, float worldZ, float height);
	void deletePolylinePoint(int pointIndex);
	void insertPolylinePoint(int afterIndex, float worldX, float worldZ, float height);
	void finalizePolyline();
	void cancelPolyline();
	bool isPolylineActive() const;
	int getPolylinePointCount() const;
	const ControlPoint* getPolylinePoint(int index) const;

	// Polyline properties
	void setPolylineWidth(float width);
	float getPolylineWidth() const;
	void setPolylineShaderFamily(int familyId);
	int getPolylineShaderFamily() const;
	void setPolylineFeatherDistance(float distance);
	float getPolylineFeatherDistance() const;
	void setPolylineUseFixedHeights(bool useFixed);
	bool getPolylineUseFixedHeights() const;
	void setPolylineName(const char* name);
	const char* getPolylineName() const;

	// Point selection/highlighting for polyline editing
	int findNearestPolylinePoint(float worldX, float worldZ, float maxDistance) const;
	void setSelectedPolylinePoint(int index);
	int getSelectedPolylinePoint() const;

	// Render polyline preview
	void renderPolylinePreview(const Camera& camera) const;

	// ======================================================================
	// Environment Zones
	// ======================================================================

	void beginEnvironmentZone();
	void addEnvironmentZonePoint(float worldX, float worldZ);
	void finalizeEnvironmentZone();
	void cancelEnvironmentZone();
	bool isEnvironmentZoneActive() const;
	/// True while placing points for environment zone, exclude terrain, or boundary polygon.
	bool isPolygonDrawActive() const;

	void beginPolygonDraw(PolygonDrawPurpose purpose);
	void finalizePolygonDraw();
	void cancelPolygonDraw();
	/// Vertices collected for the active environment / exclude / boundary-polygon draw.
	int getPolygonBoundaryPointCount() const;
	void setEnvironmentFamily(int familyId);
	int getEnvironmentFamily() const;

	// ======================================================================
	// Bitmap Stamps
	// ======================================================================

	void setBitmapStamp(const char* bitmapName);
	const char* getBitmapStampName() const;
	void setBitmapStampRotation(float rotation);
	float getBitmapStampRotation() const;
	void setBitmapStampScale(float scale);
	float getBitmapStampScale() const;
	void setBitmapAffectsHeight(bool affects);
	bool getBitmapAffectsHeight() const;
	void setBitmapAffectsShader(bool affects);
	bool getBitmapAffectsShader() const;
	void setBitmapHeightScale(float scale);
	float getBitmapHeightScale() const;
	void setBitmapShaderFamily(int familyId);
	int getBitmapShaderFamily() const;
	void applyBitmapStamp(float worldX, float worldZ);
	/// Load raster for TM_StampBitmap from the active terrain's BitmapGroup (by family id).
	void reloadBitmapStampFromTerrainFamily(int familyId);

	// ======================================================================
	// TerrainGenerator Integration
	// ======================================================================

	// Access to terrain generator (for layer management)
	TerrainGenerator* getTerrainGenerator() const;

	// Create affector layers from current modifications
	bool createHeightAffectorLayer(const char* layerName);
	bool createShaderAffectorLayer(const char* layerName);
	bool createFloraAffectorLayer(const char* layerName);
	bool createRoadFromPolyline(const char* name);
	bool createRibbonFromPolyline(const char* name);
	bool createEnvironmentZoneAffector(const char* name);
	bool createTerrainExcludeFromPolygon(const char* name);
	bool createBoundaryPolygonLayer(const char* name);
	bool createBoundaryPolylineLayer(const char* name, float corridorWidth);

	// Export all modifications to a new terrain layer
	bool exportModificationsToLayer(const char* layerName);

	/// TerrainEditor-style procedural layers (full-map boundary uses planet map width).
	bool addFullMapHeightConstantLayer(float height, float featherDistance, char const* optionalLayerNameBase = 0);
	bool addFullMapShaderConstantLayer(int shaderFamilyId, float featherDistance, char const* optionalLayerNameBase = 0);
	bool addExcludeLayerForRectangle(Rectangle2d const& rectXZ, float featherDistance, char const* optionalLayerNameBase = 0);

	/// Full-map lattice of live height edits from a luminance raster (PNG / TGA via \ref ImageFormatList). Saves with .trn like brush edits when exported.
	bool applyImportedHeightRasterFromImageFile(char const* localFilesystemPath, int elevationMinMeters, int elevationMaxMeters, int latticePointsPerEdge, bool invertLuminance);

	/// Append a procedural layer that paints environment inside the current region selection (rectangle or circle).
	bool addEnvironmentAffectorForCurrentRegionSelection(int familyId, float featherDistance);

	// Import/export polyline data
	bool exportPolylineToFile(const char* filename) const;
	bool importPolylineFromFile(const char* filename);

	// Get modification statistics
	int getHeightModificationCount() const;
	int getShaderModificationCount() const;
	int getVertexColorModificationCount() const;
	int getFloraModificationCount() const;
	const Rectangle2d& getModifiedRegionBounds() const;

private:

	GodClientTerrainEditor();
	~GodClientTerrainEditor();
	GodClientTerrainEditor(const GodClientTerrainEditor&);
	GodClientTerrainEditor& operator=(const GodClientTerrainEditor&);

	// Internal modification functions
	void modifyHeightRaise(float worldX, float worldZ);
	void modifyHeightLower(float worldX, float worldZ);
	void modifyHeightFlatten(float worldX, float worldZ, float targetHeight, float strength);
	void modifyHeightSmooth(float worldX, float worldZ, float strength);
	void modifyHeightNoise(float worldX, float worldZ, float amplitude, float frequency);
	void modifyHeightSet(float worldX, float worldZ, float targetHeight);

	// Shader modification functions
	void modifyShaderPaint(float worldX, float worldZ, int shaderFamilyId, float strength);
	void modifyVertexColorPaint(float worldX, float worldZ, PackedRgb const& color, float strength);

	// Flora modification functions
	void modifyFloraPaint(float worldX, float worldZ, int floraFamilyId, float density, bool collidable);
	void modifyFloraRemove(float worldX, float worldZ, float strength);

	void modifyRadialPaint(float worldX, float worldZ, int radialFamilyId, float childChoiceStrength);

	// Brush calculation helpers
	float calculateFalloff(float distance, float radius) const;
	float calculateBrushEffect(float localX, float localZ) const;

	bool isWorldPositionInActiveRegion(float worldX, float worldZ) const;
	Rectangle2d clipBoundaryRectangleToActiveRegion(Rectangle2d const & worldRect) const;

	void recordShaderStrokePending(uint64 key);
	void sealShaderStrokeRecords(BrushStroke & stroke);
	bool shouldRecordShaderUndo() const;
	void restoreShaderModificationsFromStrokeRecords(std::vector<ShaderStrokeRecord> const & recs, bool usePriorState);

	void recordVertexColorStrokePending(uint64 key);
	void sealVertexColorStrokeRecords(BrushStroke & stroke);
	void restoreVertexColorModificationsFromStrokeRecords(std::vector<VertexColorStrokeRecord> const & recs, bool usePriorState);

	/// Extends (or creates) a top-most generator layer that marks the live-edit footprint for .trn authoring.
	void godClientSyncLiveStagingAoiLayer(Rectangle2d const & worldExtentFootprintXZ);

	// Invalidate terrain in a region (full extent; prefer live-stroke helpers while dragging).
	void invalidateTerrainRegion(float centerX, float centerZ, float radius);

	// Stroke session: accumulate full stroke AABB for end-of-stroke finalize; throttled rolling invalidates + AOI sync while dragging; full AABB again on release.
	void accumulateStrokeFinalizeDirtyRect(float worldX, float worldZ, float regionRadius);
	void invalidateTerrainMeshesForLiveBrushSample(float worldX, float worldZ, float regionRadius, float currentTime);

	// Sample terrain heights in brush area
	void sampleBrushArea(float centerX, float centerZ, float radius, std::vector<HeightModification>& outSamples) const;

	// Get average height in region
	float getAverageHeight(float centerX, float centerZ, float radius) const;

	// Noise generation
	float generateNoise(float x, float z, float frequency) const;

	// Polyline helpers
	void recalculatePolylineExtent();
	void renderPolylineSegment(const Camera& camera, const ControlPoint& p1, const ControlPoint& p2, bool isRibbon) const;
	float getTerrainHeightAtPoint(float worldX, float worldZ) const;

	// Bitmap stamp helpers
	void loadBitmapStampData(const char* filename);
	void loadBitmapStampDataFromImage(class Image const* image);
	float sampleBitmapHeight(float normalizedX, float normalizedZ) const;
	int sampleBitmapShader(float normalizedX, float normalizedZ) const;

	// Track modified region bounds
	void expandModifiedBounds(float worldX, float worldZ, float radius);

	void placeWaterBrushDab(float worldX, float worldZ, float currentFrameTime);

	static GodClientTerrainEditor* ms_instance;

	// Current tool settings
	ToolMode m_toolMode;
	BrushShape m_brushShape;
	FalloffType m_falloffType;
	float m_brushFeather;
	float m_brushSize;
	float m_brushStrength;
	float m_raiseLowerSpeed;
	float m_raiseLowerBias;
	float m_raiseLowerClickRate;
	float m_raiseLowerJitter;
	float m_targetHeight;
	float m_noiseAmplitude;
	float m_noiseFrequency;
	int m_selectedShaderFamily;
	int m_selectedFloraFamily;
	int m_selectedRadialFamily;
	bool m_floraCollidable;
	float m_floraDensity;
	bool m_shaderPaintTintMode;
	uint8 m_shaderPaintTintR;
	uint8 m_shaderPaintTintG;
	uint8 m_shaderPaintTintB;

	// Brush preview
	bool m_brushPreviewEnabled;
	Vector m_cursorWorldPosition;
	bool m_cursorPositionValid;

	// Active brush stroke
	bool m_brushStrokeActive;
	BrushStroke m_currentStroke;
	float m_lastStrokeX;
	float m_lastStrokeZ;

	// Height modification map (world position -> modification data)
	typedef std::map<uint64, HeightModification> HeightModificationMap;
	HeightModificationMap m_heightModifications;

	// Shader modification map
	typedef std::map<uint64, ShaderModification> ShaderModificationMap;
	mutable Mutex m_shaderModificationMutex;
	ShaderModificationMap m_shaderModifications;

	typedef std::map<uint64, VertexColorModification> VertexColorModificationMap;
	VertexColorModificationMap m_vertexColorModifications;

	// RAII guard for concurrent access from procedural chunk worker threads vs main/UI thread.
	struct ShaderMapLock
	{
		explicit ShaderMapLock(GodClientTerrainEditor& editor);
		~ShaderMapLock();
	private:
		Mutex& m_mutex;
		ShaderMapLock(ShaderMapLock const&);
		ShaderMapLock& operator=(ShaderMapLock const&);
	};

	// Flora modification map
	typedef std::map<uint64, FloraModification> FloraModificationMap;
	FloraModificationMap m_floraModifications;

	// Radial (dynamic flora) overlay map
	typedef std::map<uint64, RadialModification> RadialModificationMap;
	RadialModificationMap m_radialModifications;

	// Undo/redo stacks
	static const int MAX_UNDO_STROKES = 50;
	std::vector<BrushStroke> m_undoStack;
	std::vector<BrushStroke> m_redoStack;

	int m_shaderUndoBatch;
	typedef std::vector<std::pair<uint64, std::pair<bool, ShaderModification> > > ShaderStrokePendingVector;
	ShaderStrokePendingVector m_shaderStrokePending;
	std::set<uint64> m_shaderStrokePendingKeys;

	typedef std::vector<std::pair<uint64, std::pair<bool, VertexColorModification> > > VertexColorStrokePendingVector;
	VertexColorStrokePendingVector m_vertexColorStrokePending;
	std::set<uint64> m_vertexColorStrokePendingKeys;

	// Region selection
	bool m_hasRegionSelection;
	float m_regionMinX;
	float m_regionMinZ;
	float m_regionMaxX;
	float m_regionMaxZ;
	bool m_regionSelectionCircular;
	float m_regionCircleCenterX;
	float m_regionCircleCenterZ;
	float m_regionCircleRadius;

	// Modification rate limiting
	float m_lastModificationTime;
	static const float MIN_MODIFICATION_INTERVAL;

	// Real-time editing optimization: rate-limited terrain invalidation
	float m_lastInvalidationTime;
	float m_dirtyRegionMinX;
	float m_dirtyRegionMinZ;
	float m_dirtyRegionMaxX;
	float m_dirtyRegionMaxZ;
	bool m_hasDirtyRegion;
	static const float REALTIME_INVALIDATION_INTERVAL;

	// While dragging: mesh invalidates use a rolling segment union + throttle; accumulated stroke bounds still finalize on release.
	bool m_liveStrokeInvalidateHasPrior;
	float m_liveStrokeInvalidatePriorX;
	float m_liveStrokeInvalidatePriorZ;

	// Polyline editing state
	PolylineEditMode m_polylineEditMode;
	ActivePolyline m_activePolyline;
	int m_selectedPolylinePoint;
	Rectangle2d m_polylineExtent;

	// Environment zone editing (also stores points for exclude / boundary polygon draws)
	EnvironmentZone m_activeEnvironmentZone;
	PolygonDrawPurpose m_polygonDrawPurpose;
	int m_environmentFamilyId;

	// Bitmap stamp
	BitmapStamp m_bitmapStamp;
	std::vector<float> m_bitmapHeightData;
	std::vector<int> m_bitmapShaderData;
	int m_bitmapWidth;
	int m_bitmapHeight;

	float m_waterPlacementHeight;
	std::string m_waterPlacementShaderTemplate;
	std::string m_ribbonWaterShaderTemplate;
	float m_lastWaterDabTime;

	// Track overall modified region for export
	Rectangle2d m_modifiedBounds;
	bool m_hasModifiedBounds;

	// Created affector layers (for tracking what we've added to TerrainGenerator)
	std::vector<std::string> m_createdLayers;
};

// ======================================================================

inline GodClientTerrainEditor::ToolMode GodClientTerrainEditor::getToolMode() const
{
	return m_toolMode;
}

inline float GodClientTerrainEditor::getBrushSize() const
{
	return m_brushSize;
}

inline float GodClientTerrainEditor::getBrushStrength() const
{
	return m_brushStrength;
}

inline float GodClientTerrainEditor::getRaiseLowerSpeed() const
{
	return m_raiseLowerSpeed;
}

inline float GodClientTerrainEditor::getRaiseLowerBias() const
{
	return m_raiseLowerBias;
}

inline float GodClientTerrainEditor::getRaiseLowerClickRate() const
{
	return m_raiseLowerClickRate;
}

inline float GodClientTerrainEditor::getRaiseLowerJitter() const
{
	return m_raiseLowerJitter;
}

inline GodClientTerrainEditor::BrushShape GodClientTerrainEditor::getBrushShape() const
{
	return m_brushShape;
}

inline GodClientTerrainEditor::FalloffType GodClientTerrainEditor::getFalloffType() const
{
	return m_falloffType;
}

inline float GodClientTerrainEditor::getBrushFeather() const
{
	return m_brushFeather;
}

inline float GodClientTerrainEditor::getTargetHeight() const
{
	return m_targetHeight;
}

inline float GodClientTerrainEditor::getNoiseAmplitude() const
{
	return m_noiseAmplitude;
}

inline float GodClientTerrainEditor::getNoiseFrequency() const
{
	return m_noiseFrequency;
}

inline int GodClientTerrainEditor::getSelectedShaderFamily() const
{
	return m_selectedShaderFamily;
}

inline int GodClientTerrainEditor::getSelectedFloraFamily() const
{
	return m_selectedFloraFamily;
}

inline int GodClientTerrainEditor::getSelectedRadialFamily() const
{
	return m_selectedRadialFamily;
}

inline bool GodClientTerrainEditor::getFloraCollidable() const
{
	return m_floraCollidable;
}

inline float GodClientTerrainEditor::getFloraDensity() const
{
	return m_floraDensity;
}

inline bool GodClientTerrainEditor::isBrushPreviewEnabled() const
{
	return m_brushPreviewEnabled;
}

inline const Vector& GodClientTerrainEditor::getCursorWorldPosition() const
{
	return m_cursorWorldPosition;
}

inline bool GodClientTerrainEditor::isBrushStrokeActive() const
{
	return m_brushStrokeActive;
}

inline bool GodClientTerrainEditor::hasRegionSelection() const
{
	return m_hasRegionSelection;
}

inline GodClientTerrainEditor::PolylineEditMode GodClientTerrainEditor::getPolylineEditMode() const
{
	return m_polylineEditMode;
}

inline bool GodClientTerrainEditor::isPolylineActive() const
{
	return !m_activePolyline.controlPoints.empty();
}

inline int GodClientTerrainEditor::getPolylinePointCount() const
{
	return static_cast<int>(m_activePolyline.controlPoints.size());
}

inline float GodClientTerrainEditor::getPolylineWidth() const
{
	return m_activePolyline.width;
}

inline int GodClientTerrainEditor::getPolylineShaderFamily() const
{
	return m_activePolyline.shaderFamilyId;
}

inline float GodClientTerrainEditor::getPolylineFeatherDistance() const
{
	return m_activePolyline.featherDistance;
}

inline bool GodClientTerrainEditor::getPolylineUseFixedHeights() const
{
	return m_activePolyline.hasFixedHeights;
}

inline int GodClientTerrainEditor::getSelectedPolylinePoint() const
{
	return m_selectedPolylinePoint;
}

inline bool GodClientTerrainEditor::isEnvironmentZoneActive() const
{
	return m_polygonDrawPurpose == PDP_EnvironmentZone;
}

inline bool GodClientTerrainEditor::isPolygonDrawActive() const
{
	return m_polygonDrawPurpose != PDP_None;
}

inline int GodClientTerrainEditor::getPolygonBoundaryPointCount() const
{
	return static_cast<int>(m_activeEnvironmentZone.boundaryPoints.size());
}

inline int GodClientTerrainEditor::getEnvironmentFamily() const
{
	return m_environmentFamilyId;
}

inline const char* GodClientTerrainEditor::getBitmapStampName() const
{
	return m_bitmapStamp.bitmapName.c_str();
}

inline float GodClientTerrainEditor::getBitmapStampRotation() const
{
	return m_bitmapStamp.rotation;
}

inline float GodClientTerrainEditor::getBitmapStampScale() const
{
	return m_bitmapStamp.scale;
}

inline bool GodClientTerrainEditor::getBitmapAffectsHeight() const
{
	return m_bitmapStamp.affectsHeight;
}

inline bool GodClientTerrainEditor::getBitmapAffectsShader() const
{
	return m_bitmapStamp.affectsShader;
}

inline float GodClientTerrainEditor::getBitmapHeightScale() const
{
	return m_bitmapStamp.heightScale;
}

inline int GodClientTerrainEditor::getBitmapShaderFamily() const
{
	return m_bitmapStamp.shaderFamilyId;
}

inline int GodClientTerrainEditor::getHeightModificationCount() const
{
	return static_cast<int>(m_heightModifications.size());
}

inline int GodClientTerrainEditor::getShaderModificationCount() const
{
	return static_cast<int>(m_shaderModifications.size());
}

inline int GodClientTerrainEditor::getVertexColorModificationCount() const
{
	return static_cast<int>(m_vertexColorModifications.size());
}

inline int GodClientTerrainEditor::getFloraModificationCount() const
{
	return static_cast<int>(m_floraModifications.size());
}

inline const Rectangle2d& GodClientTerrainEditor::getModifiedRegionBounds() const
{
	return m_modifiedBounds;
}

// ======================================================================

#endif // INCLUDED_GodClientTerrainEditor_H
