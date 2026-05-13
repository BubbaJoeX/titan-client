// ======================================================================
//
// TerrainDock.h
// copyright (c) 2001-2026 Sony Online Entertainment
//
// Comprehensive Terrain Editing Dock for the God Client
// Provides height editing, shader painting, water/flora tools, and .trn export
//
// ======================================================================

#ifndef INCLUDED_TerrainDock_H
#define INCLUDED_TerrainDock_H

// ======================================================================

#include "BaseTerrainDock.h"
#include "sharedMessageDispatch/Receiver.h"

#include <qstring.h>
#include <qstringlist.h>

// ======================================================================

class ClientProceduralTerrainAppearance;
class ProceduralTerrainAppearanceTemplate;
class TerrainGenerator;
class QListViewItem;

namespace MessageDispatch
{
	class Callback;
}

// ======================================================================

class TerrainDock : public BaseTerrainDock, public MessageDispatch::Receiver
{
	Q_OBJECT; //lint !e1516 !e19 !e1924 !e1762 various deficiencies in the Qt macro

public:

	// Tool modes for terrain editing
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

	// Undo/redo operation types
	enum UndoOperationType
	{
		UO_Height = 0,
		UO_Shader,
		UO_Flora,
		UO_Water,
		UO_Radial,
		UO_Count
	};

	// Undo entry structure
	struct UndoEntry
	{
		UndoOperationType type;
		float             worldX;
		float             worldZ;
		float             radius;
		std::vector<float> heightData;
		std::vector<int>   shaderData;
		std::string        description;
	};

public:

	explicit TerrainDock(QWidget* parent = 0, const char* name = 0);
	virtual ~TerrainDock();

	// Tool state
	ToolMode    getToolMode() const;
	void        setToolMode(ToolMode mode);
	
	// Brush parameters
	float       getBrushSize() const;
	float       getBrushStrength() const;
	BrushShape  getBrushShape() const;
	FalloffType getFalloffType() const;

	// Terrain access
	bool        hasActiveTerrain() const;
	const char* getTerrainFilePath() const;

	/// True while a rectangular world region is selected (Select Region / drag tool).
	bool        hasTerrainWorldRegionSelection() const;

	/// When a world region is selected, Edit menu copy/paste/cut can target terrain instead of objects.
	bool        tryConsumeTerrainRegionCopyShortcut();
	bool        tryConsumeTerrainRegionPasteShortcut();
	bool        tryConsumeTerrainRegionCutShortcut();

	// MessageDispatch::Receiver interface
	virtual void receiveMessage(const MessageDispatch::Emitter& source, const MessageDispatch::MessageBase& message);

public slots:
	// Tool selection slots
	void onToolRaise();
	void onToolLower();
	void onToolFlatten();
	void onToolSmooth();
	void onToolNoise();
	void onToolSetHeight();
	void onToolPaintShader();
	void onToolPaintFlora();
	void onToolPlaceWater();
	void onToolPlaceRadial();
	void onToolPlaceRibbon();
	void onToolPlaceRoad();
	void onToolPlaceEnvironment();
	void onToolStampBitmap();
	void onToolSelect();

	// Brush parameter slots
	void onBrushSizeChanged(int value);
	void onBrushStrengthChanged(int value);
	void onBrushShapeChanged(int index);
	void onFalloffTypeChanged(int index);
	void onBrushFeatherChanged(int value);

	// Layer list slots
	void onLayerSelectionChanged(QListViewItem* item);
	void onLayerDoubleClicked(QListViewItem* item);
	void onShaderSelectionChanged(QListViewItem* item);

	// File operations
	void onLoadTerrain();
	void onSaveTerrain();
	void onSaveTerrainAs();
	void onReloadTerrain();

	// Undo/redo
	void onUndo();
	void onRedo();
	void onClearHistory();

	// Visualization toggles
	void onToggleWireframe(bool enabled);
	void onToggleHeightColors(bool enabled);
	void onToggleChunkGrid(bool enabled);
	void onToggleBrushPreview(bool enabled);

	// Water tools
	void onWaterHeightChanged(const QString& text);
	void onWaterShaderChanged(int index);
	void onApplyWaterChanges();

	// Flora/radial tools
	void onFloraFamilyChanged(int index);
	void onFloraPlacementModeChanged(int index);
	void onRadialGroupChanged(int index);

	// Refresh from scene
	void onRefreshFromScene();

	// Shader catalog (cross-planet .trn sources)
	void onGlobalShaderSelectionChanged(QListViewItem* item);
	void onRescanGlobalShadersClicked();
	void onAddTerrainScanFolderClicked();
	void onMergeGlobalShaderIntoSceneClicked();

	// Region operations
	void onSelectRegion();
	void onCopyRegion();
	void onPasteRegion();
	void onFillRegion();

	// Polyline/Road/Ribbon operations
	void onBeginRoad();
	void onBeginRibbon();
	void onFinalizePolyline();
	void onCancelPolyline();
	void onPolylineWidthChanged(int value);
	void onPolylineFeatherChanged(int value);
	void onPolylineShaderChanged(int index);
	void onPolylineFixedHeightsToggled(bool enabled);

	// Environment zone operations
	void onBeginEnvironmentZone();
	void onFinalizeEnvironmentZone();
	void onCancelEnvironmentZone();
	void onEnvironmentFamilyChanged(int index);

	// Bitmap stamp operations
	void onBitmapStampSelected(int index);
	void onBitmapRotationChanged(int value);
	void onBitmapScaleChanged(int value);
	void onBitmapAffectsHeightToggled(bool enabled);
	void onBitmapAffectsShaderToggled(bool enabled);

	// TerrainGenerator export operations
	void onExportToLayer();
	void onExportPolyline();
	void onImportPolyline();

protected:

	void showEvent(QShowEvent* event);
	void hideEvent(QHideEvent* event);

private:
	// Disabled
	TerrainDock(const TerrainDock& rhs);
	TerrainDock& operator=(const TerrainDock& rhs);

	// Internal helpers
	void initializeUI();
	void populateLayerList();
	void populateShaderList();
	void syncGlobalShaderCatalog();
	void rebuildGlobalShaderCatalogBody(QString const& sceneTerrainTrnCanon);
	void updateSceneShaderListSelectionAfterPopulate(TerrainGenerator const* generator);
	void loadTerrainShaderScanRootsFromSettings();
	void saveTerrainShaderScanRootsToSettings() const;

	bool terrainCopyWorldRegionIntoClipboard(bool postConsoleMessageOnSuccess);
	bool terrainPasteClipboardIntoWorldRegion(bool postConsoleMessageOnSuccess);

	void populateFloraList();
	void populateRadialList();
	void populateWaterShaderList();

	void updateToolButtonStates();
	void updateUndoRedoState();

	/// Push current dock tool + brush parameters into GodClientTerrainEditor (on tool change and before painting).
	void syncGodClientEditorBrushSettings();

	// Terrain modification helpers
	void applyBrushToTerrain(float worldX, float worldZ);
	void modifyHeightAtPoint(float worldX, float worldZ, float amount);
	void smoothHeightAtPoint(float worldX, float worldZ);
	void flattenHeightAtPoint(float worldX, float worldZ, float targetHeight);
	void addNoiseAtPoint(float worldX, float worldZ);
	void paintShaderAtPoint(float worldX, float worldZ, int shaderFamilyId);
	void placeFloraAtPoint(float worldX, float worldZ, int floraFamily);

	// Brush calculation helpers
	float calculateFalloff(float distance, float radius) const;
	float calculateBrushEffect(float localX, float localZ) const;

	// Brush preview rendering
	void renderBrushPreview(float worldX, float worldZ) const;
	bool getTerrainPositionFromScreen(int screenX, int screenY, float& outWorldX, float& outWorldZ) const;

	// Water boundary creation
	void createWaterBoundary(float centerX, float centerZ, float radius, float height);
	void removeWaterBoundary(const std::string& boundaryId);

	// Undo/redo helpers
	void pushUndoEntry(const UndoEntry& entry);
	void clearRedoStack();

	// Terrain data access
	ClientProceduralTerrainAppearance* getClientTerrain() const;
	ProceduralTerrainAppearanceTemplate* getTerrainTemplate() const;
	TerrainGenerator* getTerrainGenerator() const;

	/// Alt+Maya camera should win over brush tools unless the tool binds Alt (road/ribbon).
	bool cameraModifierOverridesTerrainInput(int qtButtonState) const;

public:
	// Mouse event handlers for terrain editing (called from GameWidget)
	bool handleMousePress(int screenX, int screenY, int button, int qtButtonState = 0);
	bool handleMouseRelease(int screenX, int screenY, int button);
	bool handleMouseMove(int screenX, int screenY, int qtButtonState = 0);
	
	// Frame update for brush preview rendering
	void updateFrame(float elapsedTime);
	
	// Check if terrain editing is active
	bool isTerrainEditingActive() const;

private:

	// Current tool state
	ToolMode                  m_toolMode;
	BrushShape                m_brushShape;
	FalloffType               m_falloffType;
	float                     m_brushSize;
	float                     m_brushStrength;
	float                     m_brushFeather;
	float                     m_setHeightTarget;
	float                     m_noiseAmplitude;
	float                     m_noiseFrequency;

	// Water settings
	float                     m_waterHeight;
	int                       m_waterShaderIndex;

	// Flora/radial settings
	int                       m_floraFamilyIndex;
	int                       m_floraDensity;
	int                       m_radialGroupIndex;

	// Selected shader for painting
	int                       m_selectedShaderFamilyId;

	// Flora settings
	bool                      m_floraCollidable;

	// Polyline settings (roads/ribbons)
	float                     m_polylineWidth;
	float                     m_polylineFeather;
	int                       m_polylineShaderIndex;
	bool                      m_polylineFixedHeights;

	// Environment zone settings
	int                       m_environmentFamilyIndex;

	// Bitmap stamp settings
	int                       m_bitmapStampIndex;
	float                     m_bitmapRotation;
	float                     m_bitmapScale;
	bool                      m_bitmapAffectsHeight;
	bool                      m_bitmapAffectsShader;

	// Visualization flags
	bool                      m_showWireframe;
	bool                      m_showHeightColors;
	bool                      m_showChunkGrid;
	bool                      m_showBrushPreview;
	bool                      m_showPolylinePreview;

	bool                      m_regionDragActive;
	float                     m_regionAnchorX;
	float                     m_regionAnchorZ;
	float                     m_regionDragCurX;
	float                     m_regionDragCurZ;

	// Region selection
	bool                      m_hasRegionSelection;
	float                     m_regionMinX;
	float                     m_regionMinZ;
	float                     m_regionMaxX;
	float                     m_regionMaxZ;

	int                       m_polylineDragPointIndex;

	// Terrain file path
	std::string               m_terrainFilePath;
	bool                      m_terrainModified;

	// Undo/redo stacks
	static const int          MAX_UNDO_ENTRIES = 100;
	std::vector<UndoEntry>    m_undoStack;
	std::vector<UndoEntry>    m_redoStack;

	// Cached terrain pointers (invalidated on scene change)
	mutable bool              m_terrainCacheValid;

	// Region clipboard data
	struct RegionClipboard
	{
		float                 sourceMinX;
		float                 sourceMinZ;
		float                 sourceMaxX;
		float                 sourceMaxZ;
		int                   widthSamples;
		int                   heightSamples;
		std::vector<float>    heightData;
		std::vector<int>      shaderData;
		bool                  hasData;
		
		RegionClipboard() : sourceMinX(0), sourceMinZ(0), sourceMaxX(0), sourceMaxZ(0),
		                    widthSamples(0), heightSamples(0), hasData(false) {}
	};
	RegionClipboard           m_regionClipboard;

	/// Prevents reciprocal QListView handlers when clearing the other shader list selection.
	bool                       m_shaderUiSyncGuard;

	/// When true, paint Shader uses \ref m_selectedShaderFamilyId from the global shader catalog list.
	bool                       m_globalShaderPaintingSelection;

	/// Extra folders recursively scanned for .trn files (persisted via QSettings /SOE/SwgGodClient TerrainDock group).
	QStringList                m_globalShaderScanExtraRoots;

	QString                    m_cachedSceneTerrainTrnCanonForGlobalExclude;
	int                        m_globalShaderCatalogStamp;
	int                        m_globalShaderCatalogBuiltStamp;

	int                        m_savedGlobalPickFamilyId;
	QString                    m_savedGlobalPickTrnCanon;

	// Message callback
	MessageDispatch::Callback* m_callback;
};

// ======================================================================

inline TerrainDock::ToolMode TerrainDock::getToolMode() const
{
	return m_toolMode;
}

// ----------------------------------------------------------------------

inline float TerrainDock::getBrushSize() const
{
	return m_brushSize;
}

// ----------------------------------------------------------------------

inline float TerrainDock::getBrushStrength() const
{
	return m_brushStrength;
}

// ----------------------------------------------------------------------

inline TerrainDock::BrushShape TerrainDock::getBrushShape() const
{
	return m_brushShape;
}

// ----------------------------------------------------------------------

inline TerrainDock::FalloffType TerrainDock::getFalloffType() const
{
	return m_falloffType;
}

// ======================================================================

#endif
