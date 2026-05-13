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
#include <vector>
#include <map>
#include <string>

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
	static bool isInstalled();

	// Tool and brush settings
	void setToolMode(ToolMode mode);
	ToolMode getToolMode() const;

	void setBrushSize(float size);
	float getBrushSize() const;

	void setBrushStrength(float strength);
	float getBrushStrength() const;

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

	void setSelectedShaderFamily(int familyIndex);
	int getSelectedShaderFamily() const;

	void setSelectedFloraFamily(int familyIndex);
	int getSelectedFloraFamily() const;

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

	// Height modification queries (called by terrain system)
	static bool getModifiedHeight(float x, float z, float originalHeight, float& outHeight);
	bool getModifiedHeightInternal(float x, float z, float originalHeight, float& outHeight) const;

	// Shader modification queries
	static bool getModifiedShader(float x, float z, int originalFamilyId, int& outFamilyId, float& outFeather);
	bool getModifiedShaderInternal(float x, float z, int originalFamilyId, int& outFamilyId, float& outFeather) const;

	// Flora modification queries  
	static bool getModifiedFlora(float x, float z, int originalFamilyId, int& outFamilyId, float& outDensity);
	bool getModifiedFloraInternal(float x, float z, int originalFamilyId, int& outFamilyId, float& outDensity) const;

	// Undo/redo
	bool canUndo() const;
	bool canRedo() const;
	void undo();
	void redo();
	void clearHistory();

	// Flush changes to terrain (trigger regeneration)
	void flushTerrainChanges();

	// Render brush preview
	void renderBrushPreview(const Camera& camera) const;

	// God Client visualization overlays (debug primitives; wireframe is a coarse terrain grid)
	void renderTerrainDebugOverlays(const Camera& camera, bool wireframeGrid, bool heightColorGrid, bool chunkBoundsGrid) const;
	void renderRegionSelectionOverlay(const Camera& camera, float minX, float minZ, float maxX, float maxZ) const;

	// Apply sampled heights over a rectangle (row-major nx*nz), with editor undo support
	bool applyRectangularHeightSamples(float minX, float minZ, float maxX, float maxZ, int nx, int nz, const float* heightsRowMajor);

	/// Apply exclude + non-passable affectors inside a world rectangle (TerrainGenerator layers).
	bool applyRectangleExcludeAndNonPassable(float minX, float minZ, float maxX, float maxZ);

	static void nudgeGodClientCameraToRefreshDpvs();

	// Region selection
	void setRegionSelection(float minX, float minZ, float maxX, float maxZ);
	void clearRegionSelection();
	bool hasRegionSelection() const;

	// ======================================================================
	// Polyline Editing (Roads/Ribbons)
	// ======================================================================

	// Polyline edit mode
	void setPolylineEditMode(PolylineEditMode mode);
	PolylineEditMode getPolylineEditMode() const;

	// Start a new polyline for road or ribbon
	void beginPolyline(bool isRibbon);
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

	// Export all modifications to a new terrain layer
	bool exportModificationsToLayer(const char* layerName);

	// Import/export polyline data
	bool exportPolylineToFile(const char* filename) const;
	bool importPolylineFromFile(const char* filename);

	// Get modification statistics
	int getHeightModificationCount() const;
	int getShaderModificationCount() const;
	int getFloraModificationCount() const;
	const Rectangle2d& getModifiedRegionBounds() const;

private:

	GodClientTerrainEditor();
	~GodClientTerrainEditor();
	GodClientTerrainEditor(const GodClientTerrainEditor&);
	GodClientTerrainEditor& operator=(const GodClientTerrainEditor&);

	// Internal modification functions
	void modifyHeightRaise(float worldX, float worldZ, float strength);
	void modifyHeightLower(float worldX, float worldZ, float strength);
	void modifyHeightFlatten(float worldX, float worldZ, float targetHeight, float strength);
	void modifyHeightSmooth(float worldX, float worldZ, float strength);
	void modifyHeightNoise(float worldX, float worldZ, float amplitude, float frequency);
	void modifyHeightSet(float worldX, float worldZ, float targetHeight);

	// Shader modification functions
	void modifyShaderPaint(float worldX, float worldZ, int shaderFamilyId, float strength);

	// Flora modification functions
	void modifyFloraPaint(float worldX, float worldZ, int floraFamilyId, float density, bool collidable);
	void modifyFloraRemove(float worldX, float worldZ, float strength);

	// Brush calculation helpers
	float calculateFalloff(float distance, float radius) const;
	float calculateBrushEffect(float localX, float localZ) const;

	// Invalidate terrain in a region
	void invalidateTerrainRegion(float centerX, float centerZ, float radius);

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

private:

	static GodClientTerrainEditor* ms_instance;

	// Current tool settings
	ToolMode m_toolMode;
	BrushShape m_brushShape;
	FalloffType m_falloffType;
	float m_brushFeather;
	float m_brushSize;
	float m_brushStrength;
	float m_targetHeight;
	float m_noiseAmplitude;
	float m_noiseFrequency;
	int m_selectedShaderFamily;
	int m_selectedFloraFamily;
	bool m_floraCollidable;
	float m_floraDensity;

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
	ShaderModificationMap m_shaderModifications;

	// Flora modification map
	typedef std::map<uint64, FloraModification> FloraModificationMap;
	FloraModificationMap m_floraModifications;

	// Undo/redo stacks
	static const int MAX_UNDO_STROKES = 50;
	std::vector<BrushStroke> m_undoStack;
	std::vector<BrushStroke> m_redoStack;

	// Region selection
	bool m_hasRegionSelection;
	float m_regionMinX;
	float m_regionMinZ;
	float m_regionMaxX;
	float m_regionMaxZ;

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

	// Polyline editing state
	PolylineEditMode m_polylineEditMode;
	ActivePolyline m_activePolyline;
	int m_selectedPolylinePoint;
	Rectangle2d m_polylineExtent;

	// Environment zone editing
	EnvironmentZone m_activeEnvironmentZone;
	bool m_environmentZoneActive;
	int m_environmentFamilyId;

	// Bitmap stamp
	BitmapStamp m_bitmapStamp;
	std::vector<float> m_bitmapHeightData;
	std::vector<int> m_bitmapShaderData;
	int m_bitmapWidth;
	int m_bitmapHeight;

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
	return m_environmentZoneActive;
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
