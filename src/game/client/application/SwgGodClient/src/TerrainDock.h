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
#include <string>
#include <vector>

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
		TM_PlaceExcludeTerrain,
		TM_PlaceBoundaryPolygon,
		TM_PlaceBoundaryPolyline,
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

	/// Game window status strip: show while procedural terrain is loaded and a tool or region selection is active.
	bool        shouldShowTerrainGameWindowStatus() const;
	QString     terrainGameWindowStatusText() const;
	/// Region Operations geometry tools (exclude/mask/path/corridor), Select-region mode, or an active world region marquee.
	bool        hasRegionToolOrSelectionActive() const;

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
	void onToolExcludeTerrain();
	void onToolBoundaryPolygon();
	void onToolBoundaryPolyline();
	void onToolBoundaryPolyRoad();
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

	// Refresh from scene (slot: full refresh including global .trn shader catalog scan)
	void onRefreshFromScene();

	// Shader catalog (cross-planet .trn sources)
	void onGlobalShaderSelectionChanged(QListViewItem* item);
	void onRescanGlobalShadersClicked();
	void onAddTerrainScanFolderClicked();
	void onClearTerrainScanFoldersClicked();
	void onMergeGlobalShaderIntoSceneClicked();

	// Region operations
	void onSelectRegion();
	void onCopyRegion();
	void onPasteRegion();
	void onFillRegion();
	void onSaveRegionLay();
	void onLoadRegionLay();
	void onImportRegionLayAtCursor();
	void onRegionShapeChanged(int index);
	void onMapTemplateSettingsClicked();
	void onAddProceduralHeightConstantLayer();
	void onAddProceduralShaderConstantLayer();
	void onAddProceduralExcludeFromRegion();

	// TerrainGenerator layer list (live)
	void onLayerToggleActive();
	void onLayerPromote();
	void onLayerDemote();
	void onLayerRename();

	// Polyline/Road/Ribbon operations
	void onBeginRoad();
	void onBeginRibbon();
	void onFinalizePolyline();
	void onCancelPolyline();
	void onPolylineWidthChanged(int value);
	void onPolylineFeatherChanged(int value);
	void onPolylineShaderChanged(int index);
	void onPolylineFixedHeightsToggled(bool enabled);

	// Environment / polygon region operations
	void onBeginEnvironmentZone();
	void onFinalizePolygonDraw();
	void onCancelPolygonDraw();
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

	/// Clears exclude/mask/path/corridor tools and Select mode, cancels in-progress draws, clears world region selection.
	void clearRegionGeometryAndSelection();

signals:
	void terrainGameWindowStatusChanged();

protected:

	void showEvent(QShowEvent* event);
	void hideEvent(QHideEvent* event);

private slots:

	/// Runs after \ref Game::Messages::SCENE_CHANGED so terrain / scene pointers are stable.
	void onDeferredRefreshAfterSceneChange();

private:
	// Nested type used by region / .lay helpers below; full definition is with member data.
	struct RegionClipboard;

	// Disabled
	TerrainDock(const TerrainDock& rhs);
	TerrainDock& operator=(const TerrainDock& rhs);

	// Internal helpers
	void initializeUI();
	/// @param skipGlobalShaderCatalogScan if true, skips rebuildGlobalShaderCatalog (no recursive .trn load). Used when showing the dock to avoid AV from bad .trn on disk.
	void refreshFromScene(bool skipGlobalShaderCatalogScan);
	/// Before the old scene tears down procedural terrain: flush edits, prepare generator layers, and write the .trn if \ref m_terrainModified.
	void tryAutoSaveTerrainBeforeSceneChange();
	/// Flush live edits, run \ref TerrainGenerator::prepare, and write PTAT + generator + baked data to \a path.
	bool writeCurrentTerrainTemplateToFile(std::string const& path, bool clearModifiedOnSuccess);
	void updateMapParametersPanel();
	void syncMapTemplateEditorWidgetsFromScene();

	/// Region Operations: contextual copy for geometry tools; show/hide closed-polygon commit UI.
	void updateRegionGeometryUi();
	void populateLayerList();
	void populateShaderList(bool skipGlobalShaderCatalogScan);
	void syncGlobalShaderCatalog();
	void rebuildGlobalShaderCatalogBody(QString const& sceneTerrainTrnCanon);
	void updateSceneShaderListSelectionAfterPopulate(TerrainGenerator const* generator);
	void loadTerrainShaderScanRootsFromSettings();
	void saveTerrainShaderScanRootsToSettings() const;

	bool terrainCopyWorldRegionIntoClipboard(bool postConsoleMessageOnSuccess);
	bool terrainPasteClipboardIntoWorldRegion(bool postConsoleMessageOnSuccess);
	bool terrainWriteRegionClipboardToLayFile(QString const& path) const;
	bool terrainReadRegionLayFileIntoClipboard(QString const& path);

	bool terrainDecodeLayFromFile(QString const& path, RegionClipboard& dest);
	bool terrainApplyRegionClipboardAtOrigin(RegionClipboard const& clip, int gridX0, int gridZ0, bool postConsoleMessageOnSuccess);

	/// Rebuild procedural terrain after generator layer order/active/name edits.
	void terrainGeneratorLiveCommit();

	void syncRegionSelectionToEditor();
	int  selectedLayerListIndex() const;
	/// Row in \ref m_layerList -> actual `TerrainGenerator::getLayer` index (rows omit null slots).
	int  selectedTerrainGeneratorLayerIndex() const;

	void populateFloraList();
	void populateRadialList();
	void populateWaterShaderList();
	void populatePolylineShaderCombo();
	void populateEnvironmentFamilyCombo();
	void populateBitmapStampCombo();

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

	/// Copies catalog family + children (.sht template list) onto the scene generator when painting from Global shaders,
	/// and rebuilds ShaderCache sizing. Optionally remaps to a fresh family id if the catalog conflicts with scene contents.
	bool ensureLiveTerrainShaderFamilyForPaint(int catalogFamilyId, QString const& catalogSourceTrnPath);

	// Brush calculation helpers
	float calculateFalloff(float distance, float radius) const;
	float calculateBrushEffect(float localX, float localZ) const;

	// Brush preview rendering
	void renderBrushPreview(float worldX, float worldZ) const;
	bool getTerrainPositionFromScreen(int screenX, int screenY, float& outWorldX, float& outWorldZ) const;
	/// Fallback ground pick when meshes are rebuilding — keeps brush ring and drag stroke alive during live LOD work.
	bool pickTerrainGroundForLiveEdit(int screenX, int screenY, float& outWorldX, float& outWorldZ, float& outGroundY) const;
	/// When an LMB brush stroke is active but collide misses (holes / rebuild), intersect the view ray with the edit plane and sample height.
	bool pickTerrainGroundPlanarForLiveDrag(int screenX, int screenY, float& outWorldX, float& outWorldZ, float& outGroundY) const;

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

	enum RegionSelectionShape
	{
		RSS_Rectangle = 0,
		RSS_Circle
	};

public:
	// Mouse event handlers for terrain editing (called from GameWidget)
	bool handleMousePress(int screenX, int screenY, int button, int qtButtonState = 0);
	bool handleMouseRelease(int screenX, int screenY, int button);
	bool handleMouseMove(int screenX, int screenY, int qtButtonState = 0);
	
	// Frame update for brush preview rendering
	void updateFrame(float elapsedTime);
	
	// Check if terrain editing is active
	bool isTerrainEditingActive() const;

	/// Rebuild layer QListView after generator edits (safe to call from GodClientTerrainEditor).
	void refreshTerrainLayerListFromGenerator();

	/// True for brush-like tools and Select Region — suppresses orbit camera double-click.
	bool suppressCameraDoubleClickForTerrainTool() const;

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
	std::vector<std::string> m_waterShaderTemplateNames;

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
	RegionSelectionShape      m_regionSelectionShape;
	float                     m_regionCircleCenterX;
	float                     m_regionCircleCenterZ;
	float                     m_regionCircleRadius;

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
		int                   gridX0;
		int                   gridZ0;
		int                   widthSamples;
		int                   heightSamples;
		std::vector<float>    heightData;
		std::vector<int>      shaderData;
		std::vector<unsigned char> cellMask;
		bool                  hasData;
		bool                  hasCellMask;
		
		RegionClipboard() : sourceMinX(0), sourceMinZ(0), sourceMaxX(0), sourceMaxZ(0),
		                    gridX0(0), gridZ0(0),
		                    widthSamples(0), heightSamples(0), hasData(false), hasCellMask(false) {}
	};
	RegionClipboard           m_regionClipboard;

	/// Prevents reciprocal QListView handlers when clearing the other shader list selection.
	bool                       m_shaderUiSyncGuard;

	/// When true, paint Shader uses \ref m_selectedShaderFamilyId from the global shader catalog list.
	bool                       m_globalShaderPaintingSelection;

	/// Combo index -> shader family id (Scene shaders list); polyline road/ribbon used wrong row index as id before.
	std::vector<int>           m_polylineShaderFamilyIds;
	/// Combo index -> flora family id (FloraGroup row order).
	std::vector<int>           m_floraFamilyIds;
	/// Combo index -> radial flora family id (RadialGroup row order).
	std::vector<int>           m_radialFamilyIds;
	/// Combo index -> environment family id.
	std::vector<int>           m_environmentFamilyIds;
	/// Combo index -> bitmap stamp family id (BitmapGroup).
	std::vector<int>           m_bitmapStampFamilyIds;
	/// Parallel to each row in \ref m_layerList: generator slot index (skips null layers).
	std::vector<int>           m_layerListGeneratorIndices;

	/// Extra folders recursively scanned for .trn files (persisted via QSettings /SOE/SwgGodClient TerrainDock group).
	QStringList                m_globalShaderScanExtraRoots;

	QString                    m_cachedSceneTerrainTrnCanonForGlobalExclude;
	int                        m_globalShaderCatalogStamp;
	int                        m_globalShaderCatalogBuiltStamp;

	int                        m_savedGlobalPickFamilyId;
	QString                    m_savedGlobalPickTrnCanon;

	mutable bool               m_liveEditGroundPickFallbackValid;
	mutable float              m_liveEditGroundPickFallbackY;

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
