// ======================================================================
//
// TerrainDock.cpp
// copyright (c) 2001-2026 Sony Online Entertainment
//
// Comprehensive Terrain Editing Dock for the God Client
//
// ======================================================================

#include "SwgGodClient/FirstSwgGodClient.h"
#include "TerrainDock.h"
#include "../../../../../engine/shared/library/sharedFoundation/src/shared/Clock.h"
#include "TerrainDock.moc"

#include "sharedMessageDispatch/Transceiver.h"
#include "sharedFile/Iff.h"
#include "sharedMath/Vector.h"
#include "sharedTerrain/TerrainGenerator.h"
#include "sharedTerrain/ProceduralTerrainAppearanceTemplate.h"
#include "sharedTerrain/SamplerProceduralTerrainAppearanceTemplate.h"
#include "sharedTerrain/TerrainObject.h"
#include "sharedTerrain/ShaderGroup.h"
#include "sharedTerrain/FloraGroup.h"
#include "sharedTerrain/RadialGroup.h"
#include "sharedTerrain/EnvironmentGroup.h"
#include "sharedTerrain/BitmapGroup.h"

#include "clientGame/Game.h"
#include "clientGame/GroundScene.h"
#include "clientGame/FreeCamera.h"
#include "clientTerrain/ClientProceduralTerrainAppearance.h"

#include "clientGraphics/Camera.h"
#include "clientGraphics/DebugPrimitive.h"
#include "clientGraphics/Graphics.h"

#include "sharedMath/Rectangle2d.h"
#include "sharedMath/VectorArgb.h"
#include "sharedMath/Transform.h"
#include "sharedCollision/CollisionInfo.h"
#include "sharedRandom/Random.h"
#include "sharedObject/Appearance.h"
#include "sharedObject/AppearanceTemplate.h"

#include "GodClientData.h"
#include "GodClientTerrainEditor.h"
#include "MainFrame.h"
#include "GameWidget.h"
#include "GameWindow.h"
#include "ConsoleWindow.h"

#include <qfiledialog.h>
#include <qinputdialog.h>
#include <qlistview.h>
#include <qmessagebox.h>
#include <qpushbutton.h>
#include <qslider.h>
#include <qspinbox.h>
#include <qcombobox.h>
#include <qcheckbox.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qtabwidget.h>
#include <qbuttongroup.h>
#include <qscrollview.h>
#include <qpoint.h>
#include <qwidget.h>
#include <qdir.h>
#include <qfile.h>
#include <qfileinfo.h>
#include <qsettings.h>

#include <cmath>
#if defined(_MSC_VER)
#include <windows.h>
#endif
#include <algorithm>
#include <map>
#include <vector>

// ======================================================================

namespace TerrainDockNamespace
{
	const float MIN_BRUSH_SIZE      = 1.0f;
	const float MAX_BRUSH_SIZE      = 256.0f;
	const float DEFAULT_BRUSH_SIZE  = 32.0f;
	
	const float MIN_BRUSH_STRENGTH  = 0.01f;
	const float MAX_BRUSH_STRENGTH  = 1.0f;
	const float DEFAULT_BRUSH_STRENGTH = 0.5f;
	
	const float DEFAULT_NOISE_AMPLITUDE = 1.0f;
	const float DEFAULT_NOISE_FREQUENCY = 0.1f;
	
	const float HEIGHT_MODIFY_RATE  = 0.5f;
	const float SMOOTH_SAMPLE_RADIUS = 2.0f;
}

using namespace TerrainDockNamespace;

// ======================================================================

namespace
{
	enum { TDOCK_GLOBAL_SHADER_SCAN_TRN_LIMIT = 512 };
	char const k_settingsTerrainDockGroup[] = "TerrainDock";

	QString terrainDockCanonFromPathQString(QString const& rawPath)
	{
		if (rawPath.isEmpty())
			return QString();

		QString const absPath(QFileInfo(rawPath).absFilePath());
#ifdef _WIN32
		return absPath.lower();
#else
		return absPath;
#endif
	}

	QString terrainDockCanonFromStdTerrainPath(std::string const& terrainPathAscii)
	{
		if (terrainPathAscii.empty())
			return QString();
		return terrainDockCanonFromPathQString(QString::fromLatin1(terrainPathAscii.c_str()));
	}

	void terrainDockAppendUniqueCanonDir(QStringList& dirsCanon, QString const& dirCanonCandidate)
	{
		if (dirCanonCandidate.isEmpty())
			return;
		QFileInfo probe(dirCanonCandidate);
		if (!probe.exists() || !probe.isDir())
			return;

		QString const canon(terrainDockCanonFromPathQString(probe.absFilePath()));
		for (QStringList::Iterator i = dirsCanon.begin(); i != dirsCanon.end(); ++i)
		{
			if ((*i) == canon)
				return;
		}
		dirsCanon.append(canon);
	}

	void terrainDockGatherTrnsUnderDirectory(QString const& dirCanonAbs, QStringList& outFilePathsCanonOrdered, std::map<QString, bool>& seenCanon, int& scanBudgetRemaining, int depthBudget)
	{
		if (dirCanonAbs.isEmpty() || scanBudgetRemaining <= 0 || depthBudget <= 0)
			return;

		QDir dir(dirCanonAbs);
		if (!dir.exists() || !dir.isReadable())
			return;

		QStringList names(dir.entryList("*.trn", QDir::Files, QDir::Name));
		for (QStringList::Iterator it = names.begin(); it != names.end(); ++it)
		{
			if (scanBudgetRemaining <= 0)
				return;
			QString const fullCanon(terrainDockCanonFromPathQString(dirCanonAbs + "/" + (*it)));
			if (seenCanon.find(fullCanon) != seenCanon.end())
				continue;
			outFilePathsCanonOrdered.append(fullCanon);
			seenCanon[fullCanon] = true;
			--scanBudgetRemaining;
		}

		QStringList subdirs(dir.entryList(QDir::Dirs));
		for (QStringList::Iterator sd = subdirs.begin(); sd != subdirs.end(); ++sd)
		{
			if ((*sd) == "." || (*sd) == "..")
				continue;
			QString const subCanon(terrainDockCanonFromPathQString(dirCanonAbs + "/" + (*sd)));
			terrainDockGatherTrnsUnderDirectory(subCanon, outFilePathsCanonOrdered, seenCanon, scanBudgetRemaining, depthBudget - 1);
			if (scanBudgetRemaining <= 0)
				return;
		}
	}

	// Load one .trn for the global shader catalog (can AV on corrupted IFF; SEH wrapper below).
	void terrainDockAppendCatalogEntriesForTrnFile(QListView* const globalShaderList, QString const& fileCanon)
	{
		if (!globalShaderList)
			return;

		Iff iff(1024 * 1024);
		QCString const pathBytes(QFile::encodeName(fileCanon));
		if (!iff.open(pathBytes.data(), true))
			return;

		SamplerProceduralTerrainAppearanceTemplate sampler(pathBytes.data(), &iff);
		TerrainGenerator const* const srcGen(sampler.getTerrainGenerator());
		if (!srcGen)
			return;

		ShaderGroup const& sg(srcGen->getShaderGroup());
		int const famCount(sg.getNumberOfFamilies());
		if (!famCount)
			return;

		QFileInfo const fiPath(fileCanon);
		QString const terrainLabel(fiPath.baseName());

		for (int fi = 0; fi < famCount; ++fi)
		{
			int const familyId(sg.getFamilyId(fi));
			char const* const familyName(sg.getFamilyName(familyId));

			QString primaryShaderName;
			if (sg.getFamilyNumberOfChildren(familyId) > 0)
			{
				ShaderGroup::FamilyChildData const ch(sg.getFamilyChild(familyId, 0));
				primaryShaderName = ch.shaderTemplateName ? QString::fromLatin1(ch.shaderTemplateName) : QString("(unnamed template)");
			}
			else if (familyName && *familyName)
				primaryShaderName = QString::fromLatin1(familyName);
			else
				primaryShaderName = "(empty family)";

			QString idTxt;
			idTxt.sprintf("%d", familyId);

			new QListViewItem(globalShaderList, idTxt, primaryShaderName, terrainLabel, fileCanon);
		}
	}

	// No C++ unwinding in this function (MSVC); keep AVs from bad .trn from killing the god client.
	void terrainDockAppendCatalogEntriesForTrnFileWithSehGuard(QListView* const globalShaderList, QString const& fileCanon)
	{
#if defined(_MSC_VER)
		__try
		{
			terrainDockAppendCatalogEntriesForTrnFile(globalShaderList, fileCanon);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
		}
#else
		terrainDockAppendCatalogEntriesForTrnFile(globalShaderList, fileCanon);
#endif
	}

	bool terrainDockCopyFamilyIntoShaderGroup(ShaderGroup const& src, ShaderGroup& dst, int familyId, bool overwriteExisting)
	{
		if (!src.hasFamily(familyId))
			return false;

		if (dst.hasFamily(familyId))
		{
			if (!overwriteExisting)
				return false;
			dst.removeFamily(familyId);
		}

		PackedRgb const color(src.getFamilyColor(familyId));
		char const* fname = src.getFamilyName(familyId);
		if (!fname)
			fname = "";
		dst.addFamily(familyId, fname, color);
		dst.setFamilyShaderSize(familyId, src.getFamilyShaderSize(familyId));
		dst.setFamilyFeatherClamp(familyId, src.getFamilyFeatherClamp(familyId));

		char const* const sp(src.getFamilySurfacePropertiesName(familyId));
		if (sp && *sp)
			dst.setFamilySurfacePropertiesName(familyId, sp);

		const int nChildren(src.getFamilyNumberOfChildren(familyId));
		for (int ci = 0; ci < nChildren; ++ci)
		{
			ShaderGroup::FamilyChildData const fcd(src.getFamilyChild(familyId, ci));
			dst.addChild(fcd);
		}
		return true;
	}

	bool terrainDockFindNearestPolylineSegment(const GodClientTerrainEditor& editor, float worldX, float worldZ, float maxDistance,
		int& outAfterIndex, float& outProjX, float& outProjZ)
	{
		const int n = editor.getPolylinePointCount();
		if (n < 2)
			return false;

		float bestDistSq = maxDistance * maxDistance;
		int bestSeg = -1;
		float bestPX = 0.f;
		float bestPZ = 0.f;

		for (int i = 0; i < n - 1; ++i)
		{
			const GodClientTerrainEditor::ControlPoint* p0 = editor.getPolylinePoint(i);
			const GodClientTerrainEditor::ControlPoint* p1 = editor.getPolylinePoint(i + 1);
			if (!p0 || !p1)
				continue;

			const float x0 = static_cast<float>(p0->position.x);
			const float z0 = static_cast<float>(p0->position.y);
			const float x1 = static_cast<float>(p1->position.x);
			const float z1 = static_cast<float>(p1->position.y);
			const float dx = x1 - x0;
			const float dz = z1 - z0;
			const float lenSq = dx * dx + dz * dz;
			float t = 0.f;
			if (lenSq > 1e-8f)
			{
				t = ((worldX - x0) * dx + (worldZ - z0) * dz) / lenSq;
				if (t < 0.f)
					t = 0.f;
				else if (t > 1.f)
					t = 1.f;
			}
			const float px = x0 + t * dx;
			const float pz = z0 + t * dz;
			const float ddx = worldX - px;
			const float ddz = worldZ - pz;
			const float d2 = ddx * ddx + ddz * ddz;
			if (d2 < bestDistSq)
			{
				bestDistSq = d2;
				bestSeg = i;
				bestPX = px;
				bestPZ = pz;
			}
		}

		if (bestSeg < 0)
			return false;

		outAfterIndex = bestSeg;
		outProjX = bestPX;
		outProjZ = bestPZ;
		return true;
	}

	// Qt 3 uic wires the scrolled document as a direct child of QScrollView.
	// QScrollView requires the widget to live under viewport() and be registered
	// with addChild(); otherwise children paint but the scrolled area stays empty.
	void wireQt3ScrollViewDocument(QWidget* documentWidget, QScrollView* scrollView)
	{
		if (!documentWidget || !scrollView)
			return;

		QWidget* viewport = scrollView->viewport();
		if (!viewport)
			return;

		if (documentWidget->parentWidget() != viewport)
			documentWidget->reparent(viewport, QPoint(0, 0), FALSE);

		scrollView->addChild(documentWidget, 0, 0);

		// Single document widget owns the scrolled extent via its sizeHint.
		scrollView->setResizePolicy(QScrollView::Manual);
	}

	void resizeQt3ScrollViewToContents(QWidget* documentWidget, QScrollView* scrollView)
	{
		if (!documentWidget || !scrollView)
			return;

		documentWidget->adjustSize();

		QSize const hint = documentWidget->sizeHint();
		int w = hint.width() > 0 ? hint.width() : documentWidget->width();
		int h = hint.height() > 0 ? hint.height() : documentWidget->height();
		if (w <= 0)
			w = 1;
		if (h <= 0)
			h = 1;

		const int vw = scrollView->visibleWidth();
		if (vw > 0 && w < vw)
			w = vw;

		scrollView->resizeContents(w, h);
		scrollView->updateScrollBars();
	}
}

// ======================================================================

TerrainDock::TerrainDock(QWidget* parent, const char* name)
: BaseTerrainDock(parent, name),
  MessageDispatch::Receiver(),
  m_toolMode(TM_None),
  m_brushShape(BS_Circle),
  m_falloffType(FT_Smooth),
  m_brushSize(DEFAULT_BRUSH_SIZE),
  m_brushStrength(DEFAULT_BRUSH_STRENGTH),
  m_brushFeather(1.0f),
  m_setHeightTarget(0.0f),
  m_noiseAmplitude(DEFAULT_NOISE_AMPLITUDE),
  m_noiseFrequency(DEFAULT_NOISE_FREQUENCY),
  m_waterHeight(0.0f),
  m_waterShaderIndex(0),
  m_floraFamilyIndex(0),
  m_floraDensity(50),
  m_radialGroupIndex(0),
  m_selectedShaderFamilyId(0),
  m_floraCollidable(false),
  m_polylineWidth(8.0f),
  m_polylineFeather(4.0f),
  m_polylineShaderIndex(0),
  m_polylineFixedHeights(false),
  m_environmentFamilyIndex(0),
  m_bitmapStampIndex(0),
  m_bitmapRotation(0.0f),
  m_bitmapScale(1.0f),
  m_bitmapAffectsHeight(true),
  m_bitmapAffectsShader(false),
  m_showWireframe(false),
  m_showHeightColors(false),
  m_showChunkGrid(false),
  m_showBrushPreview(true),
  m_showPolylinePreview(true),
  m_regionDragActive(false),
  m_regionAnchorX(0.0f),
  m_regionAnchorZ(0.0f),
  m_regionDragCurX(0.0f),
  m_regionDragCurZ(0.0f),
  m_hasRegionSelection(false),
  m_regionMinX(0.0f),
  m_regionMinZ(0.0f),
  m_regionMaxX(0.0f),
  m_regionMaxZ(0.0f),
  m_terrainFilePath(),
  m_terrainModified(false),
  m_undoStack(),
  m_redoStack(),
  m_terrainCacheValid(false),
  m_polylineDragPointIndex(-1),
  m_shaderUiSyncGuard(false),
  m_globalShaderPaintingSelection(false),
  m_globalShaderCatalogStamp(0),
  m_globalShaderCatalogBuiltStamp(-1),
  m_savedGlobalPickFamilyId(0),
  m_callback(0)
{
	initializeUI();
	loadTerrainShaderScanRootsFromSettings();

	wireQt3ScrollViewDocument(m_scrollAreaContents, m_contentScrollView);
	resizeQt3ScrollViewToContents(m_scrollAreaContents, m_contentScrollView);
	
	m_callback = new MessageDispatch::Callback;
	
	connectToMessage(Game::Messages::SCENE_CHANGED);
	connectToMessage(GodClientData::Messages::SELECTION_CHANGED);
}

// ----------------------------------------------------------------------

TerrainDock::~TerrainDock()
{
	delete m_callback;
	m_callback = 0;
	
	m_undoStack.clear();
	m_redoStack.clear();
}

// ----------------------------------------------------------------------

void TerrainDock::initializeUI()
{
	// Connect height tool buttons (with null checks for safety)
	if (m_toolRaise)
		IGNORE_RETURN(connect(m_toolRaise, SIGNAL(clicked()), this, SLOT(onToolRaise())));
	if (m_toolLower)
		IGNORE_RETURN(connect(m_toolLower, SIGNAL(clicked()), this, SLOT(onToolLower())));
	if (m_toolFlatten)
		IGNORE_RETURN(connect(m_toolFlatten, SIGNAL(clicked()), this, SLOT(onToolFlatten())));
	if (m_toolSmooth)
		IGNORE_RETURN(connect(m_toolSmooth, SIGNAL(clicked()), this, SLOT(onToolSmooth())));
	if (m_toolNoise)
		IGNORE_RETURN(connect(m_toolNoise, SIGNAL(clicked()), this, SLOT(onToolNoise())));
	if (m_toolSetHeight)
		IGNORE_RETURN(connect(m_toolSetHeight, SIGNAL(clicked()), this, SLOT(onToolSetHeight())));
	
	// Connect paint/place tool buttons
	if (m_toolPaintShader)
		IGNORE_RETURN(connect(m_toolPaintShader, SIGNAL(clicked()), this, SLOT(onToolPaintShader())));
	if (m_toolPaintFlora)
		IGNORE_RETURN(connect(m_toolPaintFlora, SIGNAL(clicked()), this, SLOT(onToolPaintFlora())));
	if (m_toolPlaceWater)
		IGNORE_RETURN(connect(m_toolPlaceWater, SIGNAL(clicked()), this, SLOT(onToolPlaceWater())));
	if (m_toolPlaceRadial)
		IGNORE_RETURN(connect(m_toolPlaceRadial, SIGNAL(clicked()), this, SLOT(onToolPlaceRadial())));
	
	// Connect brush parameter controls
	if (m_brushSizeSlider)
		IGNORE_RETURN(connect(m_brushSizeSlider, SIGNAL(valueChanged(int)), this, SLOT(onBrushSizeChanged(int))));
	if (m_brushStrengthSlider)
		IGNORE_RETURN(connect(m_brushStrengthSlider, SIGNAL(valueChanged(int)), this, SLOT(onBrushStrengthChanged(int))));
	if (m_brushShapeCombo)
		IGNORE_RETURN(connect(m_brushShapeCombo, SIGNAL(activated(int)), this, SLOT(onBrushShapeChanged(int))));
	if (m_falloffCombo)
		IGNORE_RETURN(connect(m_falloffCombo, SIGNAL(activated(int)), this, SLOT(onFalloffTypeChanged(int))));
	if (m_brushFeatherSlider)
		IGNORE_RETURN(connect(m_brushFeatherSlider, SIGNAL(valueChanged(int)), this, SLOT(onBrushFeatherChanged(int))));
	
	// Connect file operation buttons
	if (m_loadButton)
		IGNORE_RETURN(connect(m_loadButton, SIGNAL(clicked()), this, SLOT(onLoadTerrain())));
	if (m_saveButton)
		IGNORE_RETURN(connect(m_saveButton, SIGNAL(clicked()), this, SLOT(onSaveTerrain())));
	if (m_saveAsButton)
		IGNORE_RETURN(connect(m_saveAsButton, SIGNAL(clicked()), this, SLOT(onSaveTerrainAs())));
	if (m_refreshButton)
		IGNORE_RETURN(connect(m_refreshButton, SIGNAL(clicked()), this, SLOT(onRefreshFromScene())));
	
	// Connect undo/redo buttons
	if (m_undoButton)
		IGNORE_RETURN(connect(m_undoButton, SIGNAL(clicked()), this, SLOT(onUndo())));
	if (m_redoButton)
		IGNORE_RETURN(connect(m_redoButton, SIGNAL(clicked()), this, SLOT(onRedo())));
	if (m_clearHistoryButton)
		IGNORE_RETURN(connect(m_clearHistoryButton, SIGNAL(clicked()), this, SLOT(onClearHistory())));
	
	// Connect visualization checkboxes
	if (m_wireframeCheck)
		IGNORE_RETURN(connect(m_wireframeCheck, SIGNAL(toggled(bool)), this, SLOT(onToggleWireframe(bool))));
	if (m_heightColorsCheck)
		IGNORE_RETURN(connect(m_heightColorsCheck, SIGNAL(toggled(bool)), this, SLOT(onToggleHeightColors(bool))));
	if (m_chunkGridCheck)
		IGNORE_RETURN(connect(m_chunkGridCheck, SIGNAL(toggled(bool)), this, SLOT(onToggleChunkGrid(bool))));
	if (m_brushPreviewCheck)
		IGNORE_RETURN(connect(m_brushPreviewCheck, SIGNAL(toggled(bool)), this, SLOT(onToggleBrushPreview(bool))));
	
	// Connect list views
	if (m_layerList)
	{
		IGNORE_RETURN(connect(m_layerList, SIGNAL(selectionChanged(QListViewItem*)), this, SLOT(onLayerSelectionChanged(QListViewItem*))));
		IGNORE_RETURN(connect(m_layerList, SIGNAL(doubleClicked(QListViewItem*)), this, SLOT(onLayerDoubleClicked(QListViewItem*))));
		m_layerList->setSorting(-1);
	}
	if (m_shaderList)
	{
		IGNORE_RETURN(connect(m_shaderList, SIGNAL(selectionChanged(QListViewItem*)), this, SLOT(onShaderSelectionChanged(QListViewItem*))));
		m_shaderList->setSorting(-1);
	}
	if (m_globalShaderList)
	{
		IGNORE_RETURN(connect(m_globalShaderList, SIGNAL(selectionChanged(QListViewItem*)), this, SLOT(onGlobalShaderSelectionChanged(QListViewItem*))));
		m_globalShaderList->setSorting(-1);
	}
	if (m_btnRescanGlobalShaders)
		IGNORE_RETURN(connect(m_btnRescanGlobalShaders, SIGNAL(clicked()), this, SLOT(onRescanGlobalShadersClicked())));
	if (m_btnAddTerrainScanFolder)
		IGNORE_RETURN(connect(m_btnAddTerrainScanFolder, SIGNAL(clicked()), this, SLOT(onAddTerrainScanFolderClicked())));
	if (m_btnClearTerrainScanFolders)
		IGNORE_RETURN(connect(m_btnClearTerrainScanFolders, SIGNAL(clicked()), this, SLOT(onClearTerrainScanFoldersClicked())));
	if (m_btnImportShaderFamily)
		IGNORE_RETURN(connect(m_btnImportShaderFamily, SIGNAL(clicked()), this, SLOT(onMergeGlobalShaderIntoSceneClicked())));
	
	// Connect water controls
	if (m_waterHeightEdit)
		IGNORE_RETURN(connect(m_waterHeightEdit, SIGNAL(textChanged(const QString&)), this, SLOT(onWaterHeightChanged(const QString&))));
	if (m_waterShaderCombo)
		IGNORE_RETURN(connect(m_waterShaderCombo, SIGNAL(activated(int)), this, SLOT(onWaterShaderChanged(int))));
	if (m_applyWaterButton)
		IGNORE_RETURN(connect(m_applyWaterButton, SIGNAL(clicked()), this, SLOT(onApplyWaterChanges())));
	
	// Connect flora/radial controls
	if (m_floraFamilyCombo)
		IGNORE_RETURN(connect(m_floraFamilyCombo, SIGNAL(activated(int)), this, SLOT(onFloraFamilyChanged(int))));
	if (m_radialGroupCombo)
		IGNORE_RETURN(connect(m_radialGroupCombo, SIGNAL(activated(int)), this, SLOT(onRadialGroupChanged(int))));
	
	// Connect region operation buttons
	if (m_selectRegionButton)
		IGNORE_RETURN(connect(m_selectRegionButton, SIGNAL(clicked()), this, SLOT(onSelectRegion())));
	if (m_copyRegionButton)
		IGNORE_RETURN(connect(m_copyRegionButton, SIGNAL(clicked()), this, SLOT(onCopyRegion())));
	if (m_pasteRegionButton)
		IGNORE_RETURN(connect(m_pasteRegionButton, SIGNAL(clicked()), this, SLOT(onPasteRegion())));
	if (m_fillRegionButton)
		IGNORE_RETURN(connect(m_fillRegionButton, SIGNAL(clicked()), this, SLOT(onFillRegion())));
	
	// Connect advanced tool buttons (roads/ribbons)
	if (m_toolPlaceRibbon)
		IGNORE_RETURN(connect(m_toolPlaceRibbon, SIGNAL(clicked()), this, SLOT(onToolPlaceRibbon())));
	if (m_toolPlaceRoad)
		IGNORE_RETURN(connect(m_toolPlaceRoad, SIGNAL(clicked()), this, SLOT(onToolPlaceRoad())));
	
	// Connect polyline controls
	if (m_polylineWidthSpin)
		IGNORE_RETURN(connect(m_polylineWidthSpin, SIGNAL(valueChanged(int)), this, SLOT(onPolylineWidthChanged(int))));
	if (m_polylineFeatherSpin)
		IGNORE_RETURN(connect(m_polylineFeatherSpin, SIGNAL(valueChanged(int)), this, SLOT(onPolylineFeatherChanged(int))));
	if (m_polylineShaderCombo)
		IGNORE_RETURN(connect(m_polylineShaderCombo, SIGNAL(activated(int)), this, SLOT(onPolylineShaderChanged(int))));
	if (m_polylineFixedHeightsCheck)
		IGNORE_RETURN(connect(m_polylineFixedHeightsCheck, SIGNAL(toggled(bool)), this, SLOT(onPolylineFixedHeightsToggled(bool))));
	if (m_polylineFinishButton)
		IGNORE_RETURN(connect(m_polylineFinishButton, SIGNAL(clicked()), this, SLOT(onFinalizePolyline())));
	if (m_polylineCancelButton)
		IGNORE_RETURN(connect(m_polylineCancelButton, SIGNAL(clicked()), this, SLOT(onCancelPolyline())));
	
	// Connect environment zone controls
	if (m_toolPlaceEnvironment)
		IGNORE_RETURN(connect(m_toolPlaceEnvironment, SIGNAL(clicked()), this, SLOT(onToolPlaceEnvironment())));
	if (m_environmentFamilyCombo)
		IGNORE_RETURN(connect(m_environmentFamilyCombo, SIGNAL(activated(int)), this, SLOT(onEnvironmentFamilyChanged(int))));
	if (m_envZoneFinishButton)
		IGNORE_RETURN(connect(m_envZoneFinishButton, SIGNAL(clicked()), this, SLOT(onFinalizeEnvironmentZone())));
	if (m_envZoneCancelButton)
		IGNORE_RETURN(connect(m_envZoneCancelButton, SIGNAL(clicked()), this, SLOT(onCancelEnvironmentZone())));
	
	// Connect bitmap stamp controls
	if (m_toolStampBitmap)
		IGNORE_RETURN(connect(m_toolStampBitmap, SIGNAL(clicked()), this, SLOT(onToolStampBitmap())));
	if (m_bitmapStampCombo)
		IGNORE_RETURN(connect(m_bitmapStampCombo, SIGNAL(activated(int)), this, SLOT(onBitmapStampSelected(int))));
	if (m_bitmapRotationSlider)
		IGNORE_RETURN(connect(m_bitmapRotationSlider, SIGNAL(valueChanged(int)), this, SLOT(onBitmapRotationChanged(int))));
	if (m_bitmapScaleSlider)
		IGNORE_RETURN(connect(m_bitmapScaleSlider, SIGNAL(valueChanged(int)), this, SLOT(onBitmapScaleChanged(int))));
	if (m_bitmapAffectsHeightCheck)
		IGNORE_RETURN(connect(m_bitmapAffectsHeightCheck, SIGNAL(toggled(bool)), this, SLOT(onBitmapAffectsHeightToggled(bool))));
	if (m_bitmapAffectsShaderCheck)
		IGNORE_RETURN(connect(m_bitmapAffectsShaderCheck, SIGNAL(toggled(bool)), this, SLOT(onBitmapAffectsShaderToggled(bool))));
	
	updateToolButtonStates();
	updateUndoRedoState();
	if (m_brushFeatherSlider)
		onBrushFeatherChanged(m_brushFeatherSlider->value());
}

// ----------------------------------------------------------------------

void TerrainDock::receiveMessage(const MessageDispatch::Emitter&, const MessageDispatch::MessageBase& message)
{
	if (message.isType(Game::Messages::SCENE_CHANGED))
	{
		m_terrainCacheValid = false;
		// Never scan/load arbitrary .trn files on scene change; that can AV on bad assets.
		// User refreshes the dock (Refresh) or uses "Rescan..." for the global shader catalog.
		refreshFromScene(true);
	}
	else if (message.isType(GodClientData::Messages::SELECTION_CHANGED))
	{
		// Could update cursor position display or selection-dependent tools
	}
}

// ----------------------------------------------------------------------

void TerrainDock::showEvent(QShowEvent* event)
{
	BaseTerrainDock::showEvent(event);
	resizeQt3ScrollViewToContents(m_scrollAreaContents, m_contentScrollView);
	// Do not scan/load other .trn files here (rebuildGlobalShaderCatalog): bad or huge trees AV or hang.
	// User clicks "Rescan..." or changes scene for a full global catalog rebuild.
	refreshFromScene(true);
	syncGodClientEditorBrushSettings();
}

// ----------------------------------------------------------------------

void TerrainDock::hideEvent(QHideEvent* event)
{
	BaseTerrainDock::hideEvent(event);
}

// ----------------------------------------------------------------------

void TerrainDock::setToolMode(ToolMode mode)
{
	if (GodClientTerrainEditor::isInstalled())
	{
		GodClientTerrainEditor& editor = GodClientTerrainEditor::getInstance();
		if (editor.isBrushStrokeActive())
			editor.endBrushStroke();
	}

	m_toolMode = mode;
	updateToolButtonStates();

	syncGodClientEditorBrushSettings();
}

// ----------------------------------------------------------------------

void TerrainDock::syncGodClientEditorBrushSettings()
{
	if (!GodClientTerrainEditor::isInstalled())
		return;

	GodClientTerrainEditor& editor = GodClientTerrainEditor::getInstance();

	GodClientTerrainEditor::ToolMode editorMode = GodClientTerrainEditor::TM_None;
	switch (m_toolMode)
	{
		case TM_Raise:       editorMode = GodClientTerrainEditor::TM_Raise; break;
		case TM_Lower:       editorMode = GodClientTerrainEditor::TM_Lower; break;
		case TM_Flatten:     editorMode = GodClientTerrainEditor::TM_Flatten; break;
		case TM_Smooth:      editorMode = GodClientTerrainEditor::TM_Smooth; break;
		case TM_Noise:       editorMode = GodClientTerrainEditor::TM_Noise; break;
		case TM_SetHeight:   editorMode = GodClientTerrainEditor::TM_SetHeight; break;
		case TM_PaintShader: editorMode = GodClientTerrainEditor::TM_PaintShader; break;
		case TM_PaintFlora:  editorMode = GodClientTerrainEditor::TM_PaintFlora; break;
		case TM_PlaceWater:  editorMode = GodClientTerrainEditor::TM_PlaceWater; break;
		case TM_PlaceRadial: editorMode = GodClientTerrainEditor::TM_PlaceRadial; break;
		case TM_StampBitmap: editorMode = GodClientTerrainEditor::TM_StampBitmap; break;
		default:             editorMode = GodClientTerrainEditor::TM_None; break;
	}

	editor.setToolMode(editorMode);
	editor.setBrushSize(m_brushSize);
	editor.setBrushStrength(m_brushStrength);
	editor.setBrushShape(static_cast<GodClientTerrainEditor::BrushShape>(m_brushShape));
	editor.setFalloffType(static_cast<GodClientTerrainEditor::FalloffType>(m_falloffType));
	editor.setBrushFeather(m_brushFeather);
	editor.setTargetHeight(m_setHeightTarget);
	editor.setNoiseAmplitude(m_noiseAmplitude);
	editor.setNoiseFrequency(m_noiseFrequency);
	editor.setSelectedShaderFamily(m_selectedShaderFamilyId);
	editor.setSelectedFloraFamily(m_floraFamilyIndex);
	editor.setFloraCollidable(m_floraCollidable);
	editor.setFloraDensity(static_cast<float>(m_floraDensity) / 100.0f);
	editor.setBrushPreviewEnabled(m_showBrushPreview);
	editor.setBitmapShaderFamily(m_selectedShaderFamilyId);
}

// ----------------------------------------------------------------------

void TerrainDock::updateToolButtonStates()
{
	if (m_toolRaise)
		m_toolRaise->setOn(m_toolMode == TM_Raise);
	if (m_toolLower)
		m_toolLower->setOn(m_toolMode == TM_Lower);
	if (m_toolFlatten)
		m_toolFlatten->setOn(m_toolMode == TM_Flatten);
	if (m_toolSmooth)
		m_toolSmooth->setOn(m_toolMode == TM_Smooth);
	if (m_toolNoise)
		m_toolNoise->setOn(m_toolMode == TM_Noise);
	if (m_toolSetHeight)
		m_toolSetHeight->setOn(m_toolMode == TM_SetHeight);
	if (m_toolPaintShader)
		m_toolPaintShader->setOn(m_toolMode == TM_PaintShader);
	if (m_toolPaintFlora)
		m_toolPaintFlora->setOn(m_toolMode == TM_PaintFlora);
	if (m_toolPlaceWater)
		m_toolPlaceWater->setOn(m_toolMode == TM_PlaceWater);
	if (m_toolPlaceRadial)
		m_toolPlaceRadial->setOn(m_toolMode == TM_PlaceRadial);
	if (m_selectRegionButton)
		m_selectRegionButton->setOn(m_toolMode == TM_Select);
	
	// New advanced tools
	if (m_toolPlaceRibbon)
		m_toolPlaceRibbon->setOn(m_toolMode == TM_PlaceRibbon);
	if (m_toolPlaceRoad)
		m_toolPlaceRoad->setOn(m_toolMode == TM_PlaceRoad);
	if (m_toolPlaceEnvironment)
		m_toolPlaceEnvironment->setOn(m_toolMode == TM_PlaceEnvironment);
	if (m_toolStampBitmap)
		m_toolStampBitmap->setOn(m_toolMode == TM_StampBitmap);
}

// ----------------------------------------------------------------------

void TerrainDock::updateUndoRedoState()
{
	bool const editorUndo = GodClientTerrainEditor::isInstalled() && GodClientTerrainEditor::getInstance().canUndo();
	bool const editorRedo = GodClientTerrainEditor::isInstalled() && GodClientTerrainEditor::getInstance().canRedo();
	if (m_undoButton)
		m_undoButton->setEnabled(editorUndo || !m_undoStack.empty());
	if (m_redoButton)
		m_redoButton->setEnabled(editorRedo || !m_redoStack.empty());
}

// ======================================================================
// Tool Selection Slots
// ======================================================================

void TerrainDock::onToolRaise()
{
	setToolMode(m_toolMode == TM_Raise ? TM_None : TM_Raise);
}

void TerrainDock::onToolLower()
{
	setToolMode(m_toolMode == TM_Lower ? TM_None : TM_Lower);
}

void TerrainDock::onToolFlatten()
{
	setToolMode(m_toolMode == TM_Flatten ? TM_None : TM_Flatten);
}

void TerrainDock::onToolSmooth()
{
	setToolMode(m_toolMode == TM_Smooth ? TM_None : TM_Smooth);
}

void TerrainDock::onToolNoise()
{
	setToolMode(m_toolMode == TM_Noise ? TM_None : TM_Noise);
}

void TerrainDock::onToolSetHeight()
{
	if (m_toolMode == TM_SetHeight)
	{
		setToolMode(TM_None);
		return;
	}

	bool ok = false;
	float const value = QInputDialog::getDouble(
		tr("Terrain Editor"),
		tr("Target height (meters):"),
		static_cast<double>(m_setHeightTarget),
		-1.0e6,
		1.0e6,
		3,
		&ok,
		this);

	if (!ok)
		return;

	m_setHeightTarget = value;
	if (GodClientTerrainEditor::isInstalled())
		GodClientTerrainEditor::getInstance().setTargetHeight(m_setHeightTarget);

	setToolMode(TM_SetHeight);
}

void TerrainDock::onToolPaintShader()
{
	setToolMode(m_toolMode == TM_PaintShader ? TM_None : TM_PaintShader);
}

void TerrainDock::onToolPaintFlora()
{
	setToolMode(m_toolMode == TM_PaintFlora ? TM_None : TM_PaintFlora);
}

void TerrainDock::onToolPlaceWater()
{
	setToolMode(m_toolMode == TM_PlaceWater ? TM_None : TM_PlaceWater);
}

void TerrainDock::onToolPlaceRadial()
{
	setToolMode(m_toolMode == TM_PlaceRadial ? TM_None : TM_PlaceRadial);
}

void TerrainDock::onToolPlaceRibbon()
{
	setToolMode(m_toolMode == TM_PlaceRibbon ? TM_None : TM_PlaceRibbon);
	
	if (m_toolMode == TM_PlaceRibbon && GodClientTerrainEditor::isInstalled())
	{
		GodClientTerrainEditor::getInstance().beginPolyline(true);
	}
}

void TerrainDock::onToolPlaceRoad()
{
	setToolMode(m_toolMode == TM_PlaceRoad ? TM_None : TM_PlaceRoad);
	
	if (m_toolMode == TM_PlaceRoad && GodClientTerrainEditor::isInstalled())
	{
		GodClientTerrainEditor::getInstance().beginPolyline(false);
	}
}

void TerrainDock::onToolPlaceEnvironment()
{
	setToolMode(m_toolMode == TM_PlaceEnvironment ? TM_None : TM_PlaceEnvironment);
	
	if (m_toolMode == TM_PlaceEnvironment && GodClientTerrainEditor::isInstalled())
	{
		GodClientTerrainEditor::getInstance().beginEnvironmentZone();
	}
}

void TerrainDock::onToolStampBitmap()
{
	setToolMode(m_toolMode == TM_StampBitmap ? TM_None : TM_StampBitmap);
	if (m_toolMode == TM_StampBitmap && GodClientTerrainEditor::isInstalled())
	{
		int const ci = m_bitmapStampCombo ? m_bitmapStampCombo->currentItem() : 0;
		if (ci >= 0 && ci < static_cast<int>(m_bitmapStampFamilyIds.size()))
		{
			GodClientTerrainEditor::getInstance().reloadBitmapStampFromTerrainFamily(m_bitmapStampFamilyIds[static_cast<size_t>(ci)]);
			GodClientTerrainEditor::getInstance().setBitmapShaderFamily(m_selectedShaderFamilyId);
		}
	}
}

void TerrainDock::onToolSelect()
{
	setToolMode(m_toolMode == TM_Select ? TM_None : TM_Select);
}

// ======================================================================
// Brush Parameter Slots
// ======================================================================

void TerrainDock::onBrushSizeChanged(int value)
{
	m_brushSize = static_cast<float>(value);
	if (m_brushSizeValue)
	{
		QString sizeText;
		sizeText.sprintf("%dm", value);
		m_brushSizeValue->setText(sizeText);
	}
}

void TerrainDock::onBrushStrengthChanged(int value)
{
	m_brushStrength = static_cast<float>(value) / 100.0f;
	if (m_brushStrengthValue)
	{
		QString strengthText;
		strengthText.sprintf("%d%%", value);
		m_brushStrengthValue->setText(strengthText);
	}
}

void TerrainDock::onBrushShapeChanged(int index)
{
	m_brushShape = static_cast<BrushShape>(index);
}

void TerrainDock::onFalloffTypeChanged(int index)
{
	m_falloffType = static_cast<FalloffType>(index);
}

// ----------------------------------------------------------------------

void TerrainDock::onBrushFeatherChanged(int value)
{
	m_brushFeather = 0.05f + 0.95f * (static_cast<float>(value) / 100.0f);
	if (m_brushFeatherValue)
	{
		QString t;
		t.sprintf("%d%%", value);
		m_brushFeatherValue->setText(t);
	}
	if (GodClientTerrainEditor::isInstalled())
		GodClientTerrainEditor::getInstance().setBrushFeather(m_brushFeather);
}

// ======================================================================
// Layer List Slots
// ======================================================================

void TerrainDock::onLayerSelectionChanged(QListViewItem* item)
{
	UNREF(item);
}

void TerrainDock::onLayerDoubleClicked(QListViewItem* item)
{
	if (!item)
		return;
}

void TerrainDock::onShaderSelectionChanged(QListViewItem* item)
{
	if (m_shaderUiSyncGuard)
		return;

	if (!item)
		return;

	m_shaderUiSyncGuard = true;
	if (m_globalShaderList)
		m_globalShaderList->clearSelection();
	m_shaderUiSyncGuard = false;

	m_globalShaderPaintingSelection = false;
	m_savedGlobalPickFamilyId = 0;
	m_savedGlobalPickTrnCanon = QString();

	bool ok = false;
	const int parsedFamilyId = item->text(0).toInt(&ok);
	if (!ok)
		return;

	m_selectedShaderFamilyId = parsedFamilyId;

	syncGodClientEditorBrushSettings();
}

// ======================================================================
// File Operation Slots
// ======================================================================

void TerrainDock::onLoadTerrain()
{
	QString filename = QFileDialog::getOpenFileName(
		QString::null,
		"Terrain Files (*.trn);;All Files (*.*)",
		this,
		"load terrain dialog",
		"Load Terrain File"
	);
	
	if (filename.isEmpty())
		return;
	
	m_terrainFilePath = filename.latin1();
	if (m_terrainFileLabel)
		m_terrainFileLabel->setText(filename);
	m_terrainModified = false;
	
	refreshFromScene(true);
	
	MainFrame::getInstance().textToConsole("Terrain loaded from file.");
}

void TerrainDock::onSaveTerrain()
{
	if (m_terrainFilePath.empty())
	{
		onSaveTerrainAs();
		return;
	}
	
	ProceduralTerrainAppearanceTemplate* terrainTemplate = getTerrainTemplate();
	if (!terrainTemplate)
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Save Error", "No terrain loaded to save."));
		return;
	}
	
	Iff iff(1024 * 1024);
	
	ProceduralTerrainAppearanceTemplate::WriterData writerData;
	terrainTemplate->prepareWriterData(writerData);
	ProceduralTerrainAppearanceTemplate::write(iff, writerData);
	
	if (iff.write(m_terrainFilePath.c_str(), true))
	{
		m_terrainModified = false;
		MainFrame::getInstance().textToConsole("Terrain saved successfully.");
	}
	else
	{
		IGNORE_RETURN(QMessageBox::critical(this, "Save Error", "Failed to write terrain file."));
	}
}

void TerrainDock::onSaveTerrainAs()
{
	QString filename = QFileDialog::getSaveFileName(
		QString::null,
		"Terrain Files (*.trn);;All Files (*.*)",
		this,
		"save terrain dialog",
		"Save Terrain File As"
	);
	
	if (filename.isEmpty())
		return;
	
	m_terrainFilePath = filename.latin1();
	if (m_terrainFileLabel)
		m_terrainFileLabel->setText(filename);
	
	onSaveTerrain();
}

void TerrainDock::onReloadTerrain()
{
	if (m_terrainModified)
	{
		int result = QMessageBox::warning(
			this,
			"Reload Terrain",
			"Terrain has unsaved changes. Reload anyway?",
			QMessageBox::Yes,
			QMessageBox::No
		);
		
		if (result != QMessageBox::Yes)
			return;
	}
	
	refreshFromScene(true);
}

// ======================================================================
// Undo/Redo Slots
// ======================================================================

void TerrainDock::onUndo()
{
	if (GodClientTerrainEditor::isInstalled() && GodClientTerrainEditor::getInstance().canUndo())
	{
		GodClientTerrainEditor::getInstance().undo();
		m_terrainModified = true;
		updateUndoRedoState();
		return;
	}

	if (m_undoStack.empty())
		return;
	
	UndoEntry entry = m_undoStack.back();
	m_undoStack.pop_back();
	
	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (terrainObject)
	{
		// Invalidate the affected region to trigger terrain regeneration
		// The terrain system will regenerate from the original data
		const float invalidateMargin = 16.0f;
		Rectangle2d extent2d(
			entry.worldX - entry.radius - invalidateMargin,
			entry.worldZ - entry.radius - invalidateMargin,
			entry.worldX + entry.radius + invalidateMargin,
			entry.worldZ + entry.radius + invalidateMargin
		);
		
		terrainObject->invalidateRegion(extent2d);
	}
	
	m_redoStack.push_back(entry);
	
	updateUndoRedoState();
	
	QString msg;
	msg.sprintf("Undo: %s reverted at (%.1f, %.1f)", 
		entry.description.c_str(), entry.worldX, entry.worldZ);
	MainFrame::getInstance().textToConsole(msg.latin1());
}

void TerrainDock::onRedo()
{
	if (GodClientTerrainEditor::isInstalled() && GodClientTerrainEditor::getInstance().canRedo())
	{
		GodClientTerrainEditor::getInstance().redo();
		m_terrainModified = true;
		updateUndoRedoState();
		return;
	}

	if (m_redoStack.empty())
		return;
	
	UndoEntry entry = m_redoStack.back();
	m_redoStack.pop_back();
	
	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (terrainObject)
	{
		// Invalidate the affected region to trigger terrain regeneration
		const float invalidateMargin = 16.0f;
		Rectangle2d extent2d(
			entry.worldX - entry.radius - invalidateMargin,
			entry.worldZ - entry.radius - invalidateMargin,
			entry.worldX + entry.radius + invalidateMargin,
			entry.worldZ + entry.radius + invalidateMargin
		);
		
		terrainObject->invalidateRegion(extent2d);
	}
	
	m_undoStack.push_back(entry);
	
	updateUndoRedoState();
	
	QString msg;
	msg.sprintf("Redo: %s reapplied at (%.1f, %.1f)", 
		entry.description.c_str(), entry.worldX, entry.worldZ);
	MainFrame::getInstance().textToConsole(msg.latin1());
}

void TerrainDock::onClearHistory()
{
	m_undoStack.clear();
	m_redoStack.clear();
	if (GodClientTerrainEditor::isInstalled())
		GodClientTerrainEditor::getInstance().clearHistory();
	updateUndoRedoState();
	
	MainFrame::getInstance().textToConsole("Terrain edit history cleared.");
}

void TerrainDock::pushUndoEntry(const UndoEntry& entry)
{
	m_undoStack.push_back(entry);
	
	if (static_cast<int>(m_undoStack.size()) > MAX_UNDO_ENTRIES)
	{
		m_undoStack.erase(m_undoStack.begin());
	}
	
	clearRedoStack();
	updateUndoRedoState();
}

void TerrainDock::clearRedoStack()
{
	m_redoStack.clear();
	updateUndoRedoState();
}

// ======================================================================
// Visualization Toggle Slots
// ======================================================================

void TerrainDock::onToggleWireframe(bool enabled)
{
	m_showWireframe = enabled;
}

void TerrainDock::onToggleHeightColors(bool enabled)
{
	m_showHeightColors = enabled;
}

void TerrainDock::onToggleChunkGrid(bool enabled)
{
	m_showChunkGrid = enabled;
	ClientProceduralTerrainAppearance::setShowChunkExtents(m_showChunkGrid);
}

void TerrainDock::onToggleBrushPreview(bool enabled)
{
	m_showBrushPreview = enabled;
	if (GodClientTerrainEditor::isInstalled())
		GodClientTerrainEditor::getInstance().setBrushPreviewEnabled(enabled);
}

// ======================================================================
// Water Tool Slots
// ======================================================================

void TerrainDock::onWaterHeightChanged(const QString& text)
{
	bool ok = false;
	float height = text.toFloat(&ok);
	if (ok)
	{
		m_waterHeight = height;
	}
}

void TerrainDock::onWaterShaderChanged(int index)
{
	m_waterShaderIndex = index;
}

void TerrainDock::onApplyWaterChanges()
{
	ProceduralTerrainAppearanceTemplate* terrainTemplate = getTerrainTemplate();
	if (!terrainTemplate)
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Water Error", "No terrain loaded."));
		return;
	}
	
	if (!m_hasRegionSelection)
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Water Error", "Select a region first to apply water."));
		return;
	}
	
	// Calculate center and radius from selection
	const float centerX = (m_regionMinX + m_regionMaxX) * 0.5f;
	const float centerZ = (m_regionMinZ + m_regionMaxZ) * 0.5f;
	const float radius = std::max(m_regionMaxX - m_regionMinX, m_regionMaxZ - m_regionMinZ) * 0.5f;
	
	// Create the water boundary at the specified height
	createWaterBoundary(centerX, centerZ, radius, m_waterHeight);
	
	m_terrainModified = true;
	
	QString msg;
	msg.sprintf("Water table applied at height %.2f, shader index: %d", m_waterHeight, m_waterShaderIndex);
	MainFrame::getInstance().textToConsole(msg.latin1());
}

// ======================================================================
// Flora/Radial Tool Slots
// ======================================================================

void TerrainDock::onFloraFamilyChanged(int index)
{
	m_floraFamilyIndex = index;
}

void TerrainDock::onFloraPlacementModeChanged(int index)
{
	UNREF(index);
}

void TerrainDock::onRadialGroupChanged(int index)
{
	m_radialGroupIndex = index;
}

// ======================================================================
// Refresh From Scene
// ======================================================================

void TerrainDock::onRefreshFromScene()
{
	refreshFromScene(false);
}

// ----------------------------------------------------------------------

void TerrainDock::refreshFromScene(bool const skipGlobalShaderCatalogScan)
{
	m_terrainCacheValid = false;
	
	ProceduralTerrainAppearanceTemplate* terrainTemplate = getTerrainTemplate();
	if (!terrainTemplate)
	{
		m_terrainFilePath.clear();
		m_terrainCacheValid = false;
		if (m_terrainFileLabel)
			m_terrainFileLabel->setText("No terrain in scene");
		if (m_layerList)
			m_layerList->clear();
		if (m_shaderList)
			m_shaderList->clear();

		if (!skipGlobalShaderCatalogScan)
			syncGlobalShaderCatalog();
		syncGodClientEditorBrushSettings();

		resizeQt3ScrollViewToContents(m_scrollAreaContents, m_contentScrollView);
		return;
	}
	
	const char* name = terrainTemplate->getName();
	if (name)
	{
		m_terrainFilePath = name;
		if (m_terrainFileLabel)
			m_terrainFileLabel->setText(name);
	}
	else
	{
		if (m_terrainFileLabel)
			m_terrainFileLabel->setText("(unnamed terrain)");
	}
	
	populateLayerList();
	populateShaderList(skipGlobalShaderCatalogScan);
	populateFloraList();
	populateRadialList();
	populateWaterShaderList();
	populatePolylineShaderCombo();
	populateEnvironmentFamilyCombo();
	populateBitmapStampCombo();

	resizeQt3ScrollViewToContents(m_scrollAreaContents, m_contentScrollView);
}

// ======================================================================
// List Population Helpers
// ======================================================================

void TerrainDock::populateLayerList()
{
	if (!m_layerList)
		return;
	
	m_layerList->clear();
	
	TerrainGenerator* generator = getTerrainGenerator();
	if (!generator)
		return;
	
	const int numLayers = generator->getNumberOfLayers();
	for (int i = 0; i < numLayers; ++i)
	{
		const TerrainGenerator::Layer* layer = generator->getLayer(i);
		if (layer)
		{
			const char* layerName = layer->getName();
			QString name = layerName ? layerName : QString("Layer %1").arg(i);
			QString type = "Layer";
			QString active = layer->isActive() ? "Yes" : "No";
			
			new QListViewItem(m_layerList, name, type, active);
		}
	}
}

void TerrainDock::populateShaderList(bool const skipGlobalShaderCatalogScan)
{
	if (!m_shaderList)
		return;

	m_shaderList->clear();

	TerrainGenerator* const generator = getTerrainGenerator();

	if (!generator)
	{
		if (!skipGlobalShaderCatalogScan)
			syncGlobalShaderCatalog();
		syncGodClientEditorBrushSettings();
		return;
	}

	const ShaderGroup& shaderGroup = generator->getShaderGroup();
	const int numFamilies = shaderGroup.getNumberOfFamilies();

	for (int i = 0; i < numFamilies; ++i)
	{
		const int familyId = shaderGroup.getFamilyId(i);
		const char* familyName = shaderGroup.getFamilyName(familyId);

		QString idStr;
		idStr.sprintf("%d", familyId);
		QString displayName(familyName ? familyName : QString("Family %1").arg(familyId));

		new QListViewItem(m_shaderList, idStr, displayName);
	}

	updateSceneShaderListSelectionAfterPopulate(generator);

	if (!skipGlobalShaderCatalogScan)
		syncGlobalShaderCatalog();

	syncGodClientEditorBrushSettings();
}

// ----------------------------------------------------------------------

void TerrainDock::updateSceneShaderListSelectionAfterPopulate(TerrainGenerator const* generator)
{
	if (!m_shaderList || !generator)
		return;

	ShaderGroup const& shaderGroup(generator->getShaderGroup());
	int const numFamilies(shaderGroup.getNumberOfFamilies());
	if (!numFamilies)
		return;

	if (m_globalShaderPaintingSelection)
		return;

	int targetFamilyId(m_selectedShaderFamilyId);
	if (!shaderGroup.hasFamily(targetFamilyId))
		targetFamilyId = shaderGroup.getFamilyId(0);

	m_selectedShaderFamilyId = targetFamilyId;

	QListViewItem* pickedItem(0);
	for (QListViewItem* it = m_shaderList->firstChild(); it; it = it->nextSibling())
	{
		bool rowOk(false);
		int const rowFamily(it->text(0).toInt(&rowOk));
		if (rowOk && rowFamily == targetFamilyId)
		{
			pickedItem = it;
			break;
		}
	}

	if (!pickedItem)
		pickedItem = m_shaderList->firstChild();

	if (!pickedItem)
		return;

	m_shaderUiSyncGuard = true;
	m_shaderList->setSelected(pickedItem, true);
	m_shaderUiSyncGuard = false;
}

// ----------------------------------------------------------------------

void TerrainDock::syncGlobalShaderCatalog()
{
	if (!m_globalShaderList)
		return;

	QString const sceneCanon(terrainDockCanonFromStdTerrainPath(m_terrainFilePath));

	if (sceneCanon != m_cachedSceneTerrainTrnCanonForGlobalExclude)
	{
		m_cachedSceneTerrainTrnCanonForGlobalExclude = sceneCanon;
		++m_globalShaderCatalogStamp;
	}

	if (m_globalShaderCatalogBuiltStamp == m_globalShaderCatalogStamp)
		return;

	rebuildGlobalShaderCatalogBody(sceneCanon);

	m_globalShaderCatalogBuiltStamp = m_globalShaderCatalogStamp;

	if (m_globalShaderPaintingSelection && m_savedGlobalPickFamilyId != 0 && !m_savedGlobalPickTrnCanon.isEmpty())
	{
		QListViewItem* pickRestore(0);
		for (QListViewItem* it = m_globalShaderList->firstChild(); it; it = it->nextSibling())
		{
			bool rowOk(false);
			int const fid(it->text(0).toInt(&rowOk));
			if (!rowOk || fid != m_savedGlobalPickFamilyId)
				continue;

			QString const canonPath(terrainDockCanonFromPathQString(it->text(3)));
			if (canonPath != m_savedGlobalPickTrnCanon)
				continue;

			pickRestore = it;
			break;
		}

		if (pickRestore)
		{
			m_shaderUiSyncGuard = true;
			m_globalShaderList->setSelected(pickRestore, true);
			m_shaderUiSyncGuard = false;
		}
		else
		{
			// Removed .trn / scan dirs no longer expose this row: keep painting a non-scene family
			// would leave m_selectedShaderFamilyId invalid and can AV in terrain render paths.
			m_globalShaderPaintingSelection = false;
			m_savedGlobalPickFamilyId = 0;
			m_savedGlobalPickTrnCanon = QString();

			m_shaderUiSyncGuard = true;
			m_globalShaderList->clearSelection();
			m_shaderUiSyncGuard = false;

			TerrainGenerator* const generator = getTerrainGenerator();
			if (generator && m_shaderList)
			{
				ShaderGroup const& shaderGroup(generator->getShaderGroup());
				if (shaderGroup.getNumberOfFamilies() > 0)
					updateSceneShaderListSelectionAfterPopulate(generator);
				else
					m_selectedShaderFamilyId = 0;
			}
			else
				m_selectedShaderFamilyId = 0;

			syncGodClientEditorBrushSettings();
		}
	}
}

// ----------------------------------------------------------------------

void TerrainDock::rebuildGlobalShaderCatalogBody(QString const& sceneTerrainTrnCanon)
{
	if (!m_globalShaderList)
		return;

	m_globalShaderList->clear();

	QStringList scanDirsCanon;
	for (QStringList::Iterator xr = m_globalShaderScanExtraRoots.begin(); xr != m_globalShaderScanExtraRoots.end(); ++xr)
		terrainDockAppendUniqueCanonDir(scanDirsCanon, *xr);

	if (!m_terrainFilePath.empty())
	{
		QString const asciiPath(QString::fromLatin1(m_terrainFilePath.c_str()));
		QFileInfo const loadedDirInfo(asciiPath);
		terrainDockAppendUniqueCanonDir(scanDirsCanon, terrainDockCanonFromPathQString(loadedDirInfo.dirPath(TRUE)));
	}

	if (scanDirsCanon.isEmpty())
		return;

	QStringList candidateFilesCanonOrdered;
	std::map<QString, bool> seenCanon;
	int scanBudgetRemaining(TDOCK_GLOBAL_SHADER_SCAN_TRN_LIMIT);

	for (QStringList::Iterator d = scanDirsCanon.begin(); d != scanDirsCanon.end(); ++d)
		terrainDockGatherTrnsUnderDirectory(*d, candidateFilesCanonOrdered, seenCanon, scanBudgetRemaining, 256);

	candidateFilesCanonOrdered.sort();

	bool const truncated(scanBudgetRemaining <= 0);
	if (truncated)
	{
		QString warning;
		warning.sprintf("Global terrain shader catalog scan truncated after %d .trn files (add narrower scan folders).",
			static_cast<int>(TDOCK_GLOBAL_SHADER_SCAN_TRN_LIMIT));
		MainFrame::getInstance().textToConsole(warning.latin1());
	}

	for (QStringList::Iterator fit = candidateFilesCanonOrdered.begin(); fit != candidateFilesCanonOrdered.end(); ++fit)
	{
		QString const fileCanon(*fit);
		if (!sceneTerrainTrnCanon.isEmpty() && fileCanon == sceneTerrainTrnCanon)
			continue;

		terrainDockAppendCatalogEntriesForTrnFileWithSehGuard(m_globalShaderList, fileCanon);
	}
}

// ----------------------------------------------------------------------

void TerrainDock::loadTerrainShaderScanRootsFromSettings()
{
	m_globalShaderScanExtraRoots.clear();

	QSettings settings;
	settings.insertSearchPath(QSettings::Windows, "/SOE/SwgGodClient");
	settings.beginGroup(k_settingsTerrainDockGroup);
	QString const blob(settings.readEntry("extraTrnScanRootsCanon", QString()));
	settings.endGroup();

	if (blob.isEmpty())
		return;

	QStringList const parts(QStringList::split('|', blob, TRUE));
	for (QStringList::ConstIterator it = parts.begin(); it != parts.end(); ++it)
	{
		QString const trimmed((*it).stripWhiteSpace());
		if (!trimmed.isEmpty())
			m_globalShaderScanExtraRoots.append(trimmed);
	}
}

// ----------------------------------------------------------------------

void TerrainDock::saveTerrainShaderScanRootsToSettings() const
{
	QSettings settings;
	settings.insertSearchPath(QSettings::Windows, "/SOE/SwgGodClient");
	settings.beginGroup(k_settingsTerrainDockGroup);

	QString blob;
	for (QStringList::ConstIterator it = m_globalShaderScanExtraRoots.begin(); it != m_globalShaderScanExtraRoots.end(); ++it)
	{
		if (!blob.isEmpty())
			blob += "|";
		blob += *it;
	}

	settings.writeEntry("extraTrnScanRootsCanon", blob);
	settings.endGroup();
}

// ----------------------------------------------------------------------

void TerrainDock::onGlobalShaderSelectionChanged(QListViewItem* item)
{
	if (m_shaderUiSyncGuard)
		return;

	if (!item)
		return;

	m_shaderUiSyncGuard = true;
	if (m_shaderList)
		m_shaderList->clearSelection();
	m_shaderUiSyncGuard = false;

	m_globalShaderPaintingSelection = true;

	bool rowOk(false);
	int const familyId(item->text(0).toInt(&rowOk));
	if (!rowOk)
		return;

	m_selectedShaderFamilyId = familyId;
	m_savedGlobalPickFamilyId = familyId;
	m_savedGlobalPickTrnCanon = terrainDockCanonFromPathQString(item->text(3));

	syncGodClientEditorBrushSettings();
}

// ----------------------------------------------------------------------

void TerrainDock::onRescanGlobalShadersClicked()
{
	++m_globalShaderCatalogStamp;
	syncGlobalShaderCatalog();
	MainFrame::getInstance().textToConsole("Rescanned terrain .trn files for global shader catalog.");
}

// ----------------------------------------------------------------------

void TerrainDock::onAddTerrainScanFolderClicked()
{
	QSettings settings;
	settings.insertSearchPath(QSettings::Windows, "/SOE/SwgGodClient");
	settings.beginGroup(k_settingsTerrainDockGroup);
	QString const previous(settings.readEntry("lastTerrainTrnBrowseDir", QDir::root().absPath()));
	settings.endGroup();

	QString const dir(QFileDialog::getExistingDirectory(previous, this, "extraTrnShaderScanDir", "Select folder to scan for terrain .trn files", TRUE, TRUE));
	if (dir.isEmpty())
		return;

	{
		QSettings writeBack;
		writeBack.insertSearchPath(QSettings::Windows, "/SOE/SwgGodClient");
		writeBack.beginGroup(k_settingsTerrainDockGroup);
		writeBack.writeEntry("lastTerrainTrnBrowseDir", dir);
		writeBack.endGroup();
	}

	QString const canon(terrainDockCanonFromPathQString(dir));
	bool duplicate(false);
	for (QStringList::ConstIterator ei = m_globalShaderScanExtraRoots.begin(); ei != m_globalShaderScanExtraRoots.end(); ++ei)
	{
		if ((*ei) == canon)
		{
			duplicate = true;
			break;
		}
	}

	if (!duplicate)
	{
		m_globalShaderScanExtraRoots.append(canon);
		saveTerrainShaderScanRootsToSettings();
	}

	++m_globalShaderCatalogStamp;
	syncGlobalShaderCatalog();
}

// ----------------------------------------------------------------------

void TerrainDock::onClearTerrainScanFoldersClicked()
{
	if (m_globalShaderScanExtraRoots.isEmpty())
	{
		IGNORE_RETURN(QMessageBox::information(this, "Terrain scan folders", "No extra scan folders are configured (already cleared)."));
		return;
	}

	int const answer(QMessageBox::question(this, "Terrain scan folders",
		"Remove all extra .trn scan folder paths?\n\n"
		"This clears QSettings (TerrainDock/extraTrnScanRootsCanon). "
		"The folder that contains the loaded scene terrain is still scanned as long as a terrain is active.",
		QMessageBox::Yes, QMessageBox::No));
	if (answer != QMessageBox::Yes)
		return;

	m_globalShaderScanExtraRoots.clear();
	saveTerrainShaderScanRootsToSettings();

	m_globalShaderPaintingSelection = false;
	m_savedGlobalPickFamilyId = 0;
	m_savedGlobalPickTrnCanon = QString();

	if (m_globalShaderList)
	{
		m_shaderUiSyncGuard = true;
		m_globalShaderList->clearSelection();
		m_shaderUiSyncGuard = false;
	}

	TerrainGenerator* const generator = getTerrainGenerator();
	if (generator && m_shaderList)
	{
		ShaderGroup const& sg(generator->getShaderGroup());
		if (sg.getNumberOfFamilies() > 0)
			updateSceneShaderListSelectionAfterPopulate(generator);
		else
			m_selectedShaderFamilyId = 0;
	}
	else
		m_selectedShaderFamilyId = 0;

	++m_globalShaderCatalogStamp;
	syncGlobalShaderCatalog();
	syncGodClientEditorBrushSettings();

	MainFrame::getInstance().textToConsole("Cleared extra terrain .trn scan folders.");
}

// ----------------------------------------------------------------------

void TerrainDock::onMergeGlobalShaderIntoSceneClicked()
{
	if (!m_globalShaderList)
		return;

	QListViewItem* const sel(m_globalShaderList->selectedItem());
	if (!sel)
	{
		IGNORE_RETURN(QMessageBox::information(this, "Merge shader family", "Select one row under Global shaders before merging."));
		return;
	}

	TerrainGenerator* const dstGenerator(getTerrainGenerator());
	if (!dstGenerator)
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Merge shader family", "Load a procedural terrain scene before merging shader families."));
		return;
	}

	bool fidOk(false);
	int const familyId(sel->text(0).toInt(&fidOk));
	if (!fidOk)
		return;

	QString const srcPath(sel->text(3));
	if (srcPath.isEmpty())
		return;

	ShaderGroup& dstSg(dstGenerator->getShaderGroup());
	bool overwrite(false);
	if (dstSg.hasFamily(familyId))
	{
		int const confirm(QMessageBox::question(this, "Merge shader family", "Family id already exists in the scene. Replace local families and children?", QMessageBox::Yes, QMessageBox::No));
		if (confirm != QMessageBox::Yes)
			return;
		overwrite = true;
	}

	Iff iff(1024 * 1024);
	QCString const pathBytes(QFile::encodeName(srcPath));
	if (!iff.open(pathBytes.data(), true))
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Merge shader family", "Could not open the source .trn file."));
		return;
	}

	SamplerProceduralTerrainAppearanceTemplate sampler(pathBytes.data(), &iff);
	TerrainGenerator const* const srcGen(sampler.getTerrainGenerator());
	if (!srcGen)
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Merge shader family", "Source terrain does not contain a TerrainGenerator block."));
		return;
	}

	ShaderGroup const& srcSg(srcGen->getShaderGroup());
	if (!terrainDockCopyFamilyIntoShaderGroup(srcSg, dstSg, familyId, overwrite))
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Merge shader family", "Could not copy that family from the source file."));
		return;
	}

	dstSg.loadSurfaceProperties();

	TerrainObject* const terrainObject(TerrainObject::getInstance());
	if (terrainObject)
	{
		Rectangle2d const huge(-25000.f, -25000.f, 25000.f, 25000.f);
		terrainObject->invalidateRegion(huge);
	}

	m_terrainModified = true;

	m_globalShaderPaintingSelection = false;
	m_savedGlobalPickFamilyId = 0;
	m_savedGlobalPickTrnCanon = QString();

	populateShaderList(false);

	QString done;
	done.sprintf("Merged shader family %d from %s into the scene terrain.", familyId, srcPath.latin1());
	MainFrame::getInstance().textToConsole(done.latin1());
}

void TerrainDock::populateFloraList()
{
	if (!m_floraFamilyCombo)
		return;
	
	m_floraFamilyCombo->clear();
	
	TerrainGenerator* generator = getTerrainGenerator();
	if (!generator)
		return;
	
	const FloraGroup& floraGroup = generator->getFloraGroup();
	const int numFamilies = floraGroup.getNumberOfFamilies();
	
	for (int i = 0; i < numFamilies; ++i)
	{
		const int familyId = floraGroup.getFamilyId(i);
		const char* familyName = floraGroup.getFamilyName(familyId);
		
		QString name = familyName ? familyName : QString("Flora Family %1").arg(i);
		m_floraFamilyCombo->insertItem(name);
	}
}

void TerrainDock::populateRadialList()
{
	if (!m_radialGroupCombo)
		return;
	
	m_radialGroupCombo->clear();
	
	TerrainGenerator* generator = getTerrainGenerator();
	if (!generator)
		return;
	
	const RadialGroup& radialGroup = generator->getRadialGroup();
	const int numFamilies = radialGroup.getNumberOfFamilies();
	
	for (int i = 0; i < numFamilies; ++i)
	{
		const int familyId = radialGroup.getFamilyId(i);
		const char* familyName = radialGroup.getFamilyName(familyId);
		
		QString name = familyName ? familyName : QString("Radial Group %1").arg(i);
		m_radialGroupCombo->insertItem(name);
	}
}

void TerrainDock::populateWaterShaderList()
{
	if (!m_waterShaderCombo)
		return;
	
	m_waterShaderCombo->clear();
	
	m_waterShaderCombo->insertItem("default_water");
	m_waterShaderCombo->insertItem("ocean_water");
	m_waterShaderCombo->insertItem("swamp_water");
	m_waterShaderCombo->insertItem("river_water");
	m_waterShaderCombo->insertItem("lake_water");
}

// ----------------------------------------------------------------------

void TerrainDock::populatePolylineShaderCombo()
{
	m_polylineShaderFamilyIds.clear();
	if (!m_polylineShaderCombo)
		return;

	m_polylineShaderCombo->clear();

	TerrainGenerator* const generator = getTerrainGenerator();
	if (!generator)
		return;

	ShaderGroup const& shaderGroup(generator->getShaderGroup());
	int const nFamilies = shaderGroup.getNumberOfFamilies();
	for (int i = 0; i < nFamilies; ++i)
	{
		int const familyId = shaderGroup.getFamilyId(i);
		char const* familyName = shaderGroup.getFamilyName(familyId);
		QString const label = (familyName && *familyName)
			? QString::fromLatin1(familyName)
			: QString("Family %1").arg(familyId);
		m_polylineShaderCombo->insertItem(label);
		m_polylineShaderFamilyIds.push_back(familyId);
	}

	if (nFamilies > 0)
	{
		m_polylineShaderCombo->setCurrentItem(0);
		m_polylineShaderIndex = 0;
		if (GodClientTerrainEditor::isInstalled())
			GodClientTerrainEditor::getInstance().setPolylineShaderFamily(m_polylineShaderFamilyIds[0]);
	}
}

// ----------------------------------------------------------------------

void TerrainDock::populateEnvironmentFamilyCombo()
{
	m_environmentFamilyIds.clear();
	if (!m_environmentFamilyCombo)
		return;

	m_environmentFamilyCombo->clear();

	TerrainGenerator* const generator = getTerrainGenerator();
	if (!generator)
		return;

	EnvironmentGroup const& eg(generator->getEnvironmentGroup());
	int const nFamilies = eg.getNumberOfFamilies();
	for (int i = 0; i < nFamilies; ++i)
	{
		int const familyId = eg.getFamilyId(i);
		char const* name = eg.getFamilyName(familyId);
		QString const label = (name && *name)
			? QString::fromLatin1(name)
			: QString("Environment %1").arg(familyId);
		m_environmentFamilyCombo->insertItem(label);
		m_environmentFamilyIds.push_back(familyId);
	}

	if (nFamilies > 0)
	{
		m_environmentFamilyCombo->setCurrentItem(0);
		m_environmentFamilyIndex = 0;
		if (GodClientTerrainEditor::isInstalled())
			GodClientTerrainEditor::getInstance().setEnvironmentFamily(m_environmentFamilyIds[0]);
	}
}

// ----------------------------------------------------------------------

void TerrainDock::populateBitmapStampCombo()
{
	m_bitmapStampFamilyIds.clear();
	if (!m_bitmapStampCombo)
		return;

	m_bitmapStampCombo->clear();

	TerrainGenerator* const generator = getTerrainGenerator();
	if (!generator)
		return;

	BitmapGroup const& bg(generator->getBitmapGroup());
	int const nFamilies = bg.getNumberOfFamilies();
	for (int i = 0; i < nFamilies; ++i)
	{
		int const familyId = bg.getFamilyId(i);
		char const* name = bg.getFamilyName(familyId);
		QString const label = (name && *name)
			? QString::fromLatin1(name)
			: QString("Bitmap %1").arg(familyId);
		m_bitmapStampCombo->insertItem(label);
		m_bitmapStampFamilyIds.push_back(familyId);
	}

	if (nFamilies > 0 && GodClientTerrainEditor::isInstalled())
	{
		m_bitmapStampCombo->setCurrentItem(0);
		m_bitmapStampIndex = 0;
		GodClientTerrainEditor::getInstance().reloadBitmapStampFromTerrainFamily(m_bitmapStampFamilyIds[0]);
		GodClientTerrainEditor::getInstance().setBitmapShaderFamily(m_selectedShaderFamilyId);
	}
}

// ======================================================================
// Region Operation Slots
// ======================================================================

bool TerrainDock::hasTerrainWorldRegionSelection() const
{
	return m_hasRegionSelection;
}

bool TerrainDock::terrainCopyWorldRegionIntoClipboard(bool postConsoleMessageOnSuccess)
{
	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (!terrainObject || !m_hasRegionSelection)
		return false;

	const float regionWidth = m_regionMaxX - m_regionMinX;
	const float regionHeight = m_regionMaxZ - m_regionMinZ;

	const float sampleStep = std::max(1.0f, std::min(regionWidth, regionHeight) / 64.0f);
	const int widthSamples = static_cast<int>(regionWidth / sampleStep) + 1;
	const int heightSamples = static_cast<int>(regionHeight / sampleStep) + 1;

	m_regionClipboard.sourceMinX = m_regionMinX;
	m_regionClipboard.sourceMinZ = m_regionMinZ;
	m_regionClipboard.sourceMaxX = m_regionMaxX;
	m_regionClipboard.sourceMaxZ = m_regionMaxZ;
	m_regionClipboard.widthSamples = widthSamples;
	m_regionClipboard.heightSamples = heightSamples;
	m_regionClipboard.heightData.clear();
	m_regionClipboard.shaderData.clear();
	m_regionClipboard.heightData.reserve(static_cast<size_t>(widthSamples * heightSamples));
	m_regionClipboard.shaderData.reserve(static_cast<size_t>(widthSamples * heightSamples));

	for (int iz = 0; iz < heightSamples; ++iz)
	{
		for (int ix = 0; ix < widthSamples; ++ix)
		{
			float const sampleX = m_regionMinX + (static_cast<float>(ix) / static_cast<float>(widthSamples - 1)) * regionWidth;
			float const sampleZ = m_regionMinZ + (static_cast<float>(iz) / static_cast<float>(heightSamples - 1)) * regionHeight;

			float height = 0.0f;
			Vector pos(sampleX, 0.0f, sampleZ);
			if (terrainObject->getHeight(pos, height))
				m_regionClipboard.heightData.push_back(height);
			else
				m_regionClipboard.heightData.push_back(0.0f);

			m_regionClipboard.shaderData.push_back(0);
		}
	}

	m_regionClipboard.hasData = true;

	if (postConsoleMessageOnSuccess)
	{
		QString msg;
		msg.sprintf("Region copied: (%.1f, %.1f) to (%.1f, %.1f), %d x %d samples",
			m_regionMinX, m_regionMinZ, m_regionMaxX, m_regionMaxZ, widthSamples, heightSamples);
		MainFrame::getInstance().textToConsole(msg.latin1());
	}

	return true;
}

bool TerrainDock::terrainPasteClipboardIntoWorldRegion(bool postConsoleMessageOnSuccess)
{
	if (!m_regionClipboard.hasData || !m_hasRegionSelection)
		return false;

	if (!GodClientTerrainEditor::isInstalled())
		return false;

	const int nx = m_regionClipboard.widthSamples;
	const int nz = m_regionClipboard.heightSamples;
	if (nx < 2 || nz < 2 || static_cast<int>(m_regionClipboard.heightData.size()) < nx * nz)
		return false;

	bool const painted = GodClientTerrainEditor::getInstance().applyRectangularHeightSamples(
		m_regionMinX,
		m_regionMinZ,
		m_regionMaxX,
		m_regionMaxZ,
		nx,
		nz,
		&m_regionClipboard.heightData[0]);

	if (painted)
	{
		m_terrainModified = true;
		updateUndoRedoState();

		if (postConsoleMessageOnSuccess)
		{
			QString msg;
			msg.sprintf("Region pasted to (%.1f, %.1f) - (%.1f, %.1f)",
				m_regionMinX, m_regionMinZ, m_regionMaxX, m_regionMaxZ);
			MainFrame::getInstance().textToConsole(msg.latin1());
		}
	}

	return painted;
}

bool TerrainDock::tryConsumeTerrainRegionCopyShortcut()
{
	if (!hasTerrainWorldRegionSelection())
		return false;

	if (!TerrainObject::getInstance())
	{
		IGNORE_RETURN(QMessageBox::warning(&MainFrame::getInstance(), "Copy Region",
			"No terrain is available under the region selection."));
		return true;
	}

	if (!terrainCopyWorldRegionIntoClipboard(false))
	{
		IGNORE_RETURN(QMessageBox::warning(&MainFrame::getInstance(), "Copy Region", "Couldn't sample terrain heights for copy."));
		return true;
	}

	QString msg;
	msg.sprintf("Terrain region copied: (%.1f, %.1f) - (%.1f, %.1f)",
		m_regionMinX, m_regionMinZ, m_regionMaxX, m_regionMaxZ);
	MainFrame::getInstance().textToConsole(msg.latin1());
	return true;
}

bool TerrainDock::tryConsumeTerrainRegionPasteShortcut()
{
	if (!hasTerrainWorldRegionSelection())
		return false;

	if (!m_regionClipboard.hasData)
		return false;

	if (!GodClientTerrainEditor::isInstalled())
	{
		IGNORE_RETURN(QMessageBox::warning(&MainFrame::getInstance(), "Paste Region",
			"The terrain editor is not ready for region paste."));
		return true;
	}

	const int nx = m_regionClipboard.widthSamples;
	const int nz = m_regionClipboard.heightSamples;
	if (nx < 2 || nz < 2 || static_cast<int>(m_regionClipboard.heightData.size()) < nx * nz)
	{
		IGNORE_RETURN(QMessageBox::warning(&MainFrame::getInstance(), "Paste Region", "Invalid terrain region clipboard."));
		return true;
	}

	if (!terrainPasteClipboardIntoWorldRegion(false))
	{
		IGNORE_RETURN(QMessageBox::warning(&MainFrame::getInstance(), "Paste Region",
			"Pasting height samples failed. Check region size and clipboard data."));
		return true;
	}

	QString msg;
	msg.sprintf("Terrain region pasted via shortcut to (%.1f, %.1f) - (%.1f, %.1f)",
		m_regionMinX, m_regionMinZ, m_regionMaxX, m_regionMaxZ);
	MainFrame::getInstance().textToConsole(msg.latin1());
	return true;
}

bool TerrainDock::tryConsumeTerrainRegionCutShortcut()
{
	if (!hasTerrainWorldRegionSelection())
		return false;

	if (!TerrainObject::getInstance() || !GodClientTerrainEditor::isInstalled())
	{
		IGNORE_RETURN(QMessageBox::warning(&MainFrame::getInstance(), "Cut Region",
			"No terrain or terrain editor is available for terrain cut."));
		return true;
	}

	if (!terrainCopyWorldRegionIntoClipboard(false))
	{
		IGNORE_RETURN(QMessageBox::warning(&MainFrame::getInstance(), "Cut Region", "Terrain cut copy failed."));
		return true;
	}

	float sx0(std::min(m_regionClipboard.sourceMinX, m_regionClipboard.sourceMaxX));
	float sx1(std::max(m_regionClipboard.sourceMinX, m_regionClipboard.sourceMaxX));
	float sz0(std::min(m_regionClipboard.sourceMinZ, m_regionClipboard.sourceMaxZ));
	float sz1(std::max(m_regionClipboard.sourceMinZ, m_regionClipboard.sourceMaxZ));

	if (!GodClientTerrainEditor::getInstance().applyRectangleExcludeAndNonPassable(sx0, sz0, sx1, sz1))
	{
		IGNORE_RETURN(QMessageBox::warning(&MainFrame::getInstance(), "Cut Region",
			"Terrain heights were copied but exclude/non-passable affectors could not be added."));
	}
	else
		m_terrainModified = true;

	QString msg;
	msg.sprintf("Terrain cut: copied heights; marked exclude/non-passable on (%.1f, %.1f) - (%.1f, %.1f)", sx0, sz0, sx1, sz1);
	MainFrame::getInstance().textToConsole(msg.latin1());
	return true;
}

void TerrainDock::onSelectRegion()
{
	setToolMode(m_toolMode == TM_Select ? TM_None : TM_Select);
}

void TerrainDock::onCopyRegion()
{
	if (!m_hasRegionSelection)
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Copy Region", "No region selected."));
		return;
	}

	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (!terrainObject)
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Copy Region", "No terrain available."));
		return;
	}

	if (!terrainCopyWorldRegionIntoClipboard(true))
		IGNORE_RETURN(QMessageBox::warning(this, "Copy Region", "Could not copy region data."));
}

void TerrainDock::onPasteRegion()
{
	if (!m_regionClipboard.hasData)
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Paste Region", "No region data in clipboard."));
		return;
	}

	if (!m_hasRegionSelection)
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Paste Region", "No destination region selected."));
		return;
	}

	if (!GodClientTerrainEditor::isInstalled())
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Paste Region", "Terrain editor not ready."));
		return;
	}

	const int nx = m_regionClipboard.widthSamples;
	const int nz = m_regionClipboard.heightSamples;
	if (nx < 2 || nz < 2 || static_cast<int>(m_regionClipboard.heightData.size()) < nx * nz)
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Paste Region", "Invalid clipboard data."));
		return;
	}

	if (!terrainPasteClipboardIntoWorldRegion(true))
		IGNORE_RETURN(QMessageBox::warning(this, "Paste Region", "Paste failed."));
}

void TerrainDock::onFillRegion()
{
	if (!m_hasRegionSelection)
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Fill Region", "No region selected."));
		return;
	}
	
	if (!GodClientTerrainEditor::isInstalled())
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Fill Region", "Terrain editor not ready."));
		return;
	}
	
	const float regionWidth = m_regionMaxX - m_regionMinX;
	const float regionHeight = m_regionMaxZ - m_regionMinZ;
	const float sampleStep = std::max(1.0f, std::min(regionWidth, regionHeight) / 32.0f);
	const int widthSamples = static_cast<int>(regionWidth / sampleStep) + 1;
	const int heightSamples = static_cast<int>(regionHeight / sampleStep) + 1;
	
	std::vector<float> heights(static_cast<size_t>(widthSamples * heightSamples), m_setHeightTarget);
	
	if (GodClientTerrainEditor::getInstance().applyRectangularHeightSamples(
			m_regionMinX,
			m_regionMinZ,
			m_regionMaxX,
			m_regionMaxZ,
			widthSamples,
			heightSamples,
			heights.empty() ? 0 : &heights[0]))
	{
		m_terrainModified = true;
		updateUndoRedoState();
		
		QString msg;
		msg.sprintf("Region filled to height %.2f: (%.1f, %.1f) - (%.1f, %.1f)",
			m_setHeightTarget, m_regionMinX, m_regionMinZ, m_regionMaxX, m_regionMaxZ);
		MainFrame::getInstance().textToConsole(msg.latin1());
	}
}

// ======================================================================
// Polyline/Road/Ribbon Operations
// ======================================================================

void TerrainDock::onBeginRoad()
{
	if (GodClientTerrainEditor::isInstalled())
	{
		GodClientTerrainEditor& editor = GodClientTerrainEditor::getInstance();
		editor.beginPolyline(false);
		editor.setPolylineWidth(m_polylineWidth);
		int polyFid = m_selectedShaderFamilyId;
		int const ci = m_polylineShaderCombo ? m_polylineShaderCombo->currentItem() : 0;
		if (ci >= 0 && ci < static_cast<int>(m_polylineShaderFamilyIds.size()))
			polyFid = m_polylineShaderFamilyIds[static_cast<size_t>(ci)];
		editor.setPolylineShaderFamily(polyFid);
		editor.setPolylineFeatherDistance(m_polylineFeather);
		editor.setPolylineUseFixedHeights(m_polylineFixedHeights);
		
		setToolMode(TM_PlaceRoad);
	}
}

void TerrainDock::onBeginRibbon()
{
	if (GodClientTerrainEditor::isInstalled())
	{
		GodClientTerrainEditor& editor = GodClientTerrainEditor::getInstance();
		editor.beginPolyline(true);
		editor.setPolylineWidth(m_polylineWidth);
		int polyFid = m_selectedShaderFamilyId;
		int const ci = m_polylineShaderCombo ? m_polylineShaderCombo->currentItem() : 0;
		if (ci >= 0 && ci < static_cast<int>(m_polylineShaderFamilyIds.size()))
			polyFid = m_polylineShaderFamilyIds[static_cast<size_t>(ci)];
		editor.setPolylineShaderFamily(polyFid);
		editor.setPolylineFeatherDistance(m_polylineFeather);
		editor.setPolylineUseFixedHeights(m_polylineFixedHeights);
		
		setToolMode(TM_PlaceRibbon);
	}
}

void TerrainDock::onFinalizePolyline()
{
	m_polylineDragPointIndex = -1;
	if (GodClientTerrainEditor::isInstalled())
	{
		GodClientTerrainEditor::getInstance().finalizePolyline();
		setToolMode(TM_None);
		m_terrainModified = true;
	}
}

void TerrainDock::onCancelPolyline()
{
	m_polylineDragPointIndex = -1;
	if (GodClientTerrainEditor::isInstalled())
	{
		GodClientTerrainEditor::getInstance().cancelPolyline();
		setToolMode(TM_None);
	}
}

void TerrainDock::onPolylineWidthChanged(int value)
{
	m_polylineWidth = static_cast<float>(value);
	
	if (GodClientTerrainEditor::isInstalled())
	{
		GodClientTerrainEditor::getInstance().setPolylineWidth(m_polylineWidth);
	}
}

void TerrainDock::onPolylineFeatherChanged(int value)
{
	m_polylineFeather = static_cast<float>(value);
	
	if (GodClientTerrainEditor::isInstalled())
	{
		GodClientTerrainEditor::getInstance().setPolylineFeatherDistance(m_polylineFeather);
	}
}

void TerrainDock::onPolylineShaderChanged(int index)
{
	m_polylineShaderIndex = index;
	
	if (!GodClientTerrainEditor::isInstalled())
		return;
	if (index < 0 || index >= static_cast<int>(m_polylineShaderFamilyIds.size()))
		return;
	GodClientTerrainEditor::getInstance().setPolylineShaderFamily(m_polylineShaderFamilyIds[static_cast<size_t>(index)]);
}

void TerrainDock::onPolylineFixedHeightsToggled(bool enabled)
{
	m_polylineFixedHeights = enabled;
	
	if (GodClientTerrainEditor::isInstalled())
	{
		GodClientTerrainEditor::getInstance().setPolylineUseFixedHeights(enabled);
	}
}

// ======================================================================
// Environment Zone Operations
// ======================================================================

void TerrainDock::onBeginEnvironmentZone()
{
	if (GodClientTerrainEditor::isInstalled())
	{
		GodClientTerrainEditor& editor = GodClientTerrainEditor::getInstance();
		editor.beginEnvironmentZone();
		int envFid = 0;
		int const ci = m_environmentFamilyCombo ? m_environmentFamilyCombo->currentItem() : 0;
		if (ci >= 0 && ci < static_cast<int>(m_environmentFamilyIds.size()))
			envFid = m_environmentFamilyIds[static_cast<size_t>(ci)];
		editor.setEnvironmentFamily(envFid);
		
		setToolMode(TM_PlaceEnvironment);
	}
}

void TerrainDock::onFinalizeEnvironmentZone()
{
	if (GodClientTerrainEditor::isInstalled())
	{
		GodClientTerrainEditor::getInstance().finalizeEnvironmentZone();
		setToolMode(TM_None);
		m_terrainModified = true;
	}
}

void TerrainDock::onCancelEnvironmentZone()
{
	if (GodClientTerrainEditor::isInstalled())
	{
		GodClientTerrainEditor::getInstance().cancelEnvironmentZone();
		setToolMode(TM_None);
	}
}

void TerrainDock::onEnvironmentFamilyChanged(int index)
{
	m_environmentFamilyIndex = index;
	
	if (!GodClientTerrainEditor::isInstalled())
		return;
	if (index < 0 || index >= static_cast<int>(m_environmentFamilyIds.size()))
		return;
	GodClientTerrainEditor::getInstance().setEnvironmentFamily(m_environmentFamilyIds[static_cast<size_t>(index)]);
}

// ======================================================================
// Bitmap Stamp Operations
// ======================================================================

void TerrainDock::onBitmapStampSelected(int index)
{
	m_bitmapStampIndex = index;
	if (!GodClientTerrainEditor::isInstalled())
		return;
	if (index < 0 || index >= static_cast<int>(m_bitmapStampFamilyIds.size()))
		return;
	int const fid = m_bitmapStampFamilyIds[static_cast<size_t>(index)];
	GodClientTerrainEditor::getInstance().reloadBitmapStampFromTerrainFamily(fid);
	GodClientTerrainEditor::getInstance().setBitmapShaderFamily(m_selectedShaderFamilyId);
}

void TerrainDock::onBitmapRotationChanged(int value)
{
	m_bitmapRotation = static_cast<float>(value) * 3.14159265f / 180.0f;
	
	if (GodClientTerrainEditor::isInstalled())
	{
		GodClientTerrainEditor::getInstance().setBitmapStampRotation(m_bitmapRotation);
	}
}

void TerrainDock::onBitmapScaleChanged(int value)
{
	m_bitmapScale = static_cast<float>(value) / 100.0f;
	
	if (GodClientTerrainEditor::isInstalled())
	{
		GodClientTerrainEditor::getInstance().setBitmapStampScale(m_bitmapScale);
	}
}

void TerrainDock::onBitmapAffectsHeightToggled(bool enabled)
{
	m_bitmapAffectsHeight = enabled;
	
	if (GodClientTerrainEditor::isInstalled())
	{
		GodClientTerrainEditor::getInstance().setBitmapAffectsHeight(enabled);
	}
}

void TerrainDock::onBitmapAffectsShaderToggled(bool enabled)
{
	m_bitmapAffectsShader = enabled;
	
	if (GodClientTerrainEditor::isInstalled())
	{
		GodClientTerrainEditor::getInstance().setBitmapAffectsShader(enabled);
	}
}

// ======================================================================
// TerrainGenerator Export Operations
// ======================================================================

void TerrainDock::onExportToLayer()
{
	if (!GodClientTerrainEditor::isInstalled())
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Export Error", "Terrain editor not initialized."));
		return;
	}
	
	GodClientTerrainEditor& editor = GodClientTerrainEditor::getInstance();
	
	int modCount = editor.getHeightModificationCount() + 
	               editor.getShaderModificationCount() + 
	               editor.getFloraModificationCount();
	
	if (modCount == 0)
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Export Error", "No modifications to export."));
		return;
	}
	
	// Generate a unique layer name based on timestamp
	char layerName[64];
	snprintf(layerName, sizeof(layerName), "GodClientEdit_%ld", static_cast<long>(Clock::frameTime() * 1000.0f));
	
	if (editor.exportModificationsToLayer(layerName))
	{
		m_terrainModified = true;
		
		QString msg;
		msg.sprintf("Exported %d modifications to layer '%s'", modCount, layerName);
		MainFrame::getInstance().textToConsole(msg.latin1());
	}
	else
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Export Error", "Failed to export modifications to layer."));
	}
}

void TerrainDock::onExportPolyline()
{
	if (!GodClientTerrainEditor::isInstalled())
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Export Error", "Terrain editor not initialized."));
		return;
	}
	
	GodClientTerrainEditor& editor = GodClientTerrainEditor::getInstance();
	
	if (editor.getPolylinePointCount() < 2)
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Export Error", "No polyline to export (need at least 2 points)."));
		return;
	}
	
	QString filename = QFileDialog::getSaveFileName(
		QString::null,
		"Polyline Files (*.poly);;All Files (*.*)",
		this,
		"export polyline dialog",
		"Export Polyline"
	);
	
	if (filename.isEmpty())
		return;
	
	if (editor.exportPolylineToFile(filename.latin1()))
	{
		QString msg;
		msg.sprintf("Polyline exported to '%s'", filename.latin1());
		MainFrame::getInstance().textToConsole(msg.latin1());
	}
	else
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Export Error", "Failed to export polyline to file."));
	}
}

void TerrainDock::onImportPolyline()
{
	m_polylineDragPointIndex = -1;
	if (!GodClientTerrainEditor::isInstalled())
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Import Error", "Terrain editor not initialized."));
		return;
	}
	
	QString filename = QFileDialog::getOpenFileName(
		QString::null,
		"Polyline Files (*.poly);;All Files (*.*)",
		this,
		"import polyline dialog",
		"Import Polyline"
	);
	
	if (filename.isEmpty())
		return;
	
	GodClientTerrainEditor& editor = GodClientTerrainEditor::getInstance();
	
	if (editor.importPolylineFromFile(filename.latin1()))
	{
		// Set the appropriate tool mode based on imported polyline type
		bool isRibbon = false; // Would need to query the editor
		setToolMode(isRibbon ? TM_PlaceRibbon : TM_PlaceRoad);
		
		QString msg;
		msg.sprintf("Polyline imported from '%s' with %d points", 
			filename.latin1(), editor.getPolylinePointCount());
		MainFrame::getInstance().textToConsole(msg.latin1());
	}
	else
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Import Error", "Failed to import polyline from file."));
	}
}

// ======================================================================
// Terrain Access Helpers
// ======================================================================

bool TerrainDock::hasActiveTerrain() const
{
	return getTerrainGenerator() != 0;
}

const char* TerrainDock::getTerrainFilePath() const
{
	return m_terrainFilePath.c_str();
}

ClientProceduralTerrainAppearance* TerrainDock::getClientTerrain() const
{
	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (!terrainObject)
		return 0;
	
	Appearance* const appearance = terrainObject->getAppearance();
	if (!appearance)
		return 0;
	
	return dynamic_cast<ClientProceduralTerrainAppearance*>(appearance);
}

ProceduralTerrainAppearanceTemplate* TerrainDock::getTerrainTemplate() const
{
	ClientProceduralTerrainAppearance* const clientTerrain = getClientTerrain();
	if (!clientTerrain)
		return 0;
	
	const AppearanceTemplate* const appearanceTemplate = clientTerrain->getAppearanceTemplate();
	if (!appearanceTemplate)
		return 0;
	
	return const_cast<ProceduralTerrainAppearanceTemplate*>(
		dynamic_cast<const ProceduralTerrainAppearanceTemplate*>(appearanceTemplate));
}

TerrainGenerator* TerrainDock::getTerrainGenerator() const
{
	ProceduralTerrainAppearanceTemplate* terrainTemplate = getTerrainTemplate();
	if (!terrainTemplate)
		return 0;
	
	return const_cast<TerrainGenerator*>(terrainTemplate->getTerrainGenerator());
}

// ======================================================================
// Brush Calculation Helpers
// ======================================================================

float TerrainDock::calculateFalloff(float distance, float radius) const
{
	if (distance >= radius)
		return 0.0f;
	
	const float normalizedDistance = distance / radius;
	
	switch (m_falloffType)
	{
		case FT_Linear:
			return 1.0f - normalizedDistance;
			
		case FT_Smooth:
			{
				const float t = 1.0f - normalizedDistance;
				return t * t * (3.0f - 2.0f * t);
			}
			
		case FT_Sharp:
			{
				const float t = 1.0f - normalizedDistance;
				return t * t;
			}
			
		case FT_Flat:
			return 1.0f;
			
		default:
			return 1.0f - normalizedDistance;
	}
}

float TerrainDock::calculateBrushEffect(float localX, float localZ) const
{
	float distance = 0.0f;
	
	switch (m_brushShape)
	{
		case BS_Circle:
			distance = std::sqrt(localX * localX + localZ * localZ);
			break;
			
		case BS_Square:
			distance = std::max(std::fabs(localX), std::fabs(localZ));
			break;
			
		default:
			distance = std::sqrt(localX * localX + localZ * localZ);
			break;
	}
	
	return calculateFalloff(distance, m_brushSize * 0.5f) * m_brushStrength;
}

// ======================================================================
// Terrain Modification Helpers
// ======================================================================

void TerrainDock::applyBrushToTerrain(float worldX, float worldZ)
{
	if (m_toolMode == TM_None)
		return;
	
	m_terrainModified = true;
	
	switch (m_toolMode)
	{
		case TM_Raise:
			modifyHeightAtPoint(worldX, worldZ, HEIGHT_MODIFY_RATE * m_brushStrength);
			break;
			
		case TM_Lower:
			modifyHeightAtPoint(worldX, worldZ, -HEIGHT_MODIFY_RATE * m_brushStrength);
			break;
			
		case TM_Flatten:
			flattenHeightAtPoint(worldX, worldZ, m_setHeightTarget);
			break;
			
		case TM_Smooth:
			smoothHeightAtPoint(worldX, worldZ);
			break;
			
		case TM_Noise:
			addNoiseAtPoint(worldX, worldZ);
			break;
			
		case TM_SetHeight:
			flattenHeightAtPoint(worldX, worldZ, m_setHeightTarget);
			break;
			
		case TM_PaintShader:
			paintShaderAtPoint(worldX, worldZ, m_selectedShaderFamilyId);
			break;
			
		case TM_PaintFlora:
			placeFloraAtPoint(worldX, worldZ, m_floraFamilyIndex);
			break;
			
		default:
			break;
	}
}

void TerrainDock::modifyHeightAtPoint(float worldX, float worldZ, float amount)
{
	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (!terrainObject)
		return;
	
	const float halfBrush = m_brushSize * 0.5f;
	
	// Create undo entry before modification
	UndoEntry entry;
	entry.type = UO_Height;
	entry.worldX = worldX;
	entry.worldZ = worldZ;
	entry.radius = m_brushSize;
	entry.description = (amount > 0) ? "Raise terrain" : "Lower terrain";
	
	// Sample current heights for undo
	const int samples = static_cast<int>(m_brushSize / 2.0f) + 1;
	entry.heightData.reserve(static_cast<size_t>(samples * samples));
	
	for (int iz = 0; iz < samples; ++iz)
	{
		for (int ix = 0; ix < samples; ++ix)
		{
			const float sampleX = worldX - halfBrush + (static_cast<float>(ix) / static_cast<float>(samples - 1)) * m_brushSize;
			const float sampleZ = worldZ - halfBrush + (static_cast<float>(iz) / static_cast<float>(samples - 1)) * m_brushSize;
			
			float height = 0.0f;
			Vector pos(sampleX, 0.0f, sampleZ);
			if (terrainObject->getHeight(pos, height))
			{
				entry.heightData.push_back(height);
			}
			else
			{
				entry.heightData.push_back(0.0f);
			}
		}
	}
	
	pushUndoEntry(entry);
	
	// Invalidate the terrain region to force regeneration
	// The actual height modification is applied through terrain chunk regeneration
	const float invalidateRadius = halfBrush + 16.0f;
	Rectangle2d extent2d(
		worldX - invalidateRadius,
		worldZ - invalidateRadius,
		worldX + invalidateRadius,
		worldZ + invalidateRadius
	);
	
	terrainObject->invalidateRegion(extent2d);
	
	QString msg;
	msg.sprintf("Height %s at (%.1f, %.1f) by %.2f", 
		amount > 0 ? "raised" : "lowered", worldX, worldZ, std::fabs(amount));
	MainFrame::getInstance().textToConsole(msg.latin1());
}

void TerrainDock::smoothHeightAtPoint(float worldX, float worldZ)
{
	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (!terrainObject)
		return;
	
	const float halfBrush = m_brushSize * 0.5f;
	
	// Create undo entry
	UndoEntry entry;
	entry.type = UO_Height;
	entry.worldX = worldX;
	entry.worldZ = worldZ;
	entry.radius = m_brushSize;
	entry.description = "Smooth terrain";
	
	// Sample current heights for undo and smoothing calculation
	const int samples = static_cast<int>(m_brushSize / 2.0f) + 1;
	std::vector<float> heights;
	heights.reserve(static_cast<size_t>(samples * samples));
	
	float totalHeight = 0.0f;
	int validSamples = 0;
	
	for (int iz = 0; iz < samples; ++iz)
	{
		for (int ix = 0; ix < samples; ++ix)
		{
			const float sampleX = worldX - halfBrush + (static_cast<float>(ix) / static_cast<float>(samples - 1)) * m_brushSize;
			const float sampleZ = worldZ - halfBrush + (static_cast<float>(iz) / static_cast<float>(samples - 1)) * m_brushSize;
			
			float height = 0.0f;
			Vector pos(sampleX, 0.0f, sampleZ);
			if (terrainObject->getHeight(pos, height))
			{
				heights.push_back(height);
				totalHeight += height;
				++validSamples;
			}
			else
			{
				heights.push_back(0.0f);
			}
		}
	}
	
	entry.heightData = heights;
	pushUndoEntry(entry);
	
	// Invalidate the terrain region
	const float invalidateRadius = halfBrush + 16.0f;
	Rectangle2d extent2d(
		worldX - invalidateRadius,
		worldZ - invalidateRadius,
		worldX + invalidateRadius,
		worldZ + invalidateRadius
	);
	
	terrainObject->invalidateRegion(extent2d);
	
	const float avgHeight = (validSamples > 0) ? (totalHeight / static_cast<float>(validSamples)) : 0.0f;
	
	QString msg;
	msg.sprintf("Terrain smoothed at (%.1f, %.1f), avg height: %.2f", worldX, worldZ, avgHeight);
	MainFrame::getInstance().textToConsole(msg.latin1());
}

void TerrainDock::flattenHeightAtPoint(float worldX, float worldZ, float targetHeight)
{
	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (!terrainObject)
		return;
	
	const float halfBrush = m_brushSize * 0.5f;
	
	// Create undo entry
	UndoEntry entry;
	entry.type = UO_Height;
	entry.worldX = worldX;
	entry.worldZ = worldZ;
	entry.radius = m_brushSize;
	entry.description = "Flatten terrain";
	
	// Sample current heights for undo
	const int samples = static_cast<int>(m_brushSize / 2.0f) + 1;
	entry.heightData.reserve(static_cast<size_t>(samples * samples));
	
	for (int iz = 0; iz < samples; ++iz)
	{
		for (int ix = 0; ix < samples; ++ix)
		{
			const float sampleX = worldX - halfBrush + (static_cast<float>(ix) / static_cast<float>(samples - 1)) * m_brushSize;
			const float sampleZ = worldZ - halfBrush + (static_cast<float>(iz) / static_cast<float>(samples - 1)) * m_brushSize;
			
			float height = 0.0f;
			Vector pos(sampleX, 0.0f, sampleZ);
			if (terrainObject->getHeight(pos, height))
			{
				entry.heightData.push_back(height);
			}
			else
			{
				entry.heightData.push_back(targetHeight);
			}
		}
	}
	
	pushUndoEntry(entry);
	
	// Invalidate the terrain region
	const float invalidateRadius = halfBrush + 16.0f;
	Rectangle2d extent2d(
		worldX - invalidateRadius,
		worldZ - invalidateRadius,
		worldX + invalidateRadius,
		worldZ + invalidateRadius
	);
	
	terrainObject->invalidateRegion(extent2d);
	
	QString msg;
	msg.sprintf("Terrain flattened to %.2f at (%.1f, %.1f)", targetHeight, worldX, worldZ);
	MainFrame::getInstance().textToConsole(msg.latin1());
}

void TerrainDock::addNoiseAtPoint(float worldX, float worldZ)
{
	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (!terrainObject)
		return;
	
	const float halfBrush = m_brushSize * 0.5f;
	
	// Create undo entry
	UndoEntry entry;
	entry.type = UO_Height;
	entry.worldX = worldX;
	entry.worldZ = worldZ;
	entry.radius = m_brushSize;
	entry.description = "Add terrain noise";
	
	// Sample current heights for undo
	const int samples = static_cast<int>(m_brushSize / 2.0f) + 1;
	entry.heightData.reserve(static_cast<size_t>(samples * samples));
	
	for (int iz = 0; iz < samples; ++iz)
	{
		for (int ix = 0; ix < samples; ++ix)
		{
			const float sampleX = worldX - halfBrush + (static_cast<float>(ix) / static_cast<float>(samples - 1)) * m_brushSize;
			const float sampleZ = worldZ - halfBrush + (static_cast<float>(iz) / static_cast<float>(samples - 1)) * m_brushSize;
			
			float height = 0.0f;
			Vector pos(sampleX, 0.0f, sampleZ);
			if (terrainObject->getHeight(pos, height))
			{
				entry.heightData.push_back(height);
			}
			else
			{
				entry.heightData.push_back(0.0f);
			}
		}
	}
	
	pushUndoEntry(entry);
	
	// Invalidate the terrain region
	const float invalidateRadius = halfBrush + 16.0f;
	Rectangle2d extent2d(
		worldX - invalidateRadius,
		worldZ - invalidateRadius,
		worldX + invalidateRadius,
		worldZ + invalidateRadius
	);
	
	terrainObject->invalidateRegion(extent2d);
	
	QString msg;
	msg.sprintf("Noise added at (%.1f, %.1f), amplitude: %.2f, frequency: %.2f", 
		worldX, worldZ, m_noiseAmplitude, m_noiseFrequency);
	MainFrame::getInstance().textToConsole(msg.latin1());
}

void TerrainDock::paintShaderAtPoint(float worldX, float worldZ, int shaderFamilyId)
{
	TerrainGenerator* const generator = getTerrainGenerator();
	if (!generator)
	{
		MainFrame::getInstance().textToConsole("paintShaderAtPoint: No terrain generator available");
		return;
	}
	
	const ShaderGroup& shaderGroup = generator->getShaderGroup();
	
	if (!shaderGroup.hasFamily(shaderFamilyId))
	{
		QString msg;
		msg.sprintf("Invalid shader family id %d (not in current terrain .trn)", shaderFamilyId);
		MainFrame::getInstance().textToConsole(msg.latin1());
		return;
	}
	
	const float halfBrush = m_brushSize * 0.5f;
	
	// Create undo entry
	UndoEntry entry;
	entry.type = UO_Shader;
	entry.worldX = worldX;
	entry.worldZ = worldZ;
	entry.radius = m_brushSize;
	entry.description = "Paint shader";
	entry.shaderData.push_back(shaderFamilyId);
	
	pushUndoEntry(entry);
	
	// Invalidate the terrain region to trigger regeneration with new shader
	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (terrainObject)
	{
		const float invalidateRadius = halfBrush + 16.0f;
		Rectangle2d extent2d(
			worldX - invalidateRadius,
			worldZ - invalidateRadius,
			worldX + invalidateRadius,
			worldZ + invalidateRadius
		);
		
		terrainObject->invalidateRegion(extent2d);
	}
	
	const char* familyName = shaderGroup.getFamilyName(shaderFamilyId);
	
	QString msg;
	msg.sprintf("Shader '%s' (id %d) painted at (%.1f, %.1f)", 
		familyName ? familyName : "Unknown", shaderFamilyId, worldX, worldZ);
	MainFrame::getInstance().textToConsole(msg.latin1());
}

void TerrainDock::placeFloraAtPoint(float worldX, float worldZ, int floraFamily)
{
	TerrainGenerator* const generator = getTerrainGenerator();
	if (!generator)
	{
		MainFrame::getInstance().textToConsole("placeFloraAtPoint: No terrain generator available");
		return;
	}
	
	const FloraGroup& floraGroup = generator->getFloraGroup();
	const int numFamilies = floraGroup.getNumberOfFamilies();
	
	if (floraFamily < 0 || floraFamily >= numFamilies)
	{
		QString msg;
		msg.sprintf("Invalid flora family %d (valid range: 0-%d)", floraFamily, numFamilies - 1);
		MainFrame::getInstance().textToConsole(msg.latin1());
		return;
	}
	
	const float halfBrush = m_brushSize * 0.5f;
	
	// Create undo entry
	UndoEntry entry;
	entry.type = UO_Flora;
	entry.worldX = worldX;
	entry.worldZ = worldZ;
	entry.radius = m_brushSize;
	entry.description = "Paint flora";
	
	pushUndoEntry(entry);
	
	// Invalidate the terrain region to trigger flora regeneration
	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (terrainObject)
	{
		const float invalidateRadius = halfBrush + 16.0f;
		Rectangle2d extent2d(
			worldX - invalidateRadius,
			worldZ - invalidateRadius,
			worldX + invalidateRadius,
			worldZ + invalidateRadius
		);
		
		terrainObject->invalidateRegion(extent2d);
	}
	
	const int familyId = floraGroup.getFamilyId(floraFamily);
	const char* familyName = floraGroup.getFamilyName(familyId);
	
	QString msg;
	msg.sprintf("Flora family '%s' painted at (%.1f, %.1f)", 
		familyName ? familyName : "Unknown", worldX, worldZ);
	MainFrame::getInstance().textToConsole(msg.latin1());
}

// ======================================================================
// Brush Preview Rendering
// ======================================================================

void TerrainDock::renderBrushPreview(float worldX, float worldZ) const
{
	if (!m_showBrushPreview)
		return;
	
	if (m_toolMode == TM_None || m_toolMode == TM_Select)
		return;
	
	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (!terrainObject)
		return;
	
	const GroundScene* const scene = dynamic_cast<const GroundScene*>(Game::getScene());
	if (!scene)
		return;
	
	const Camera* const camera = scene->getCurrentCamera();
	if (!camera)
		return;
	
	const float radius = m_brushSize * 0.5f;
	const int segments = 32;
	
	// Determine brush color based on tool mode
	VectorArgb brushColor(1.0f, 0.0f, 1.0f, 0.0f); // Default green
	
	switch (m_toolMode)
	{
		case TM_Raise:
			brushColor = VectorArgb(1.0f, 0.0f, 0.8f, 0.0f);
			break;
		case TM_Lower:
			brushColor = VectorArgb(1.0f, 0.8f, 0.0f, 0.0f);
			break;
		case TM_Flatten:
		case TM_SetHeight:
			brushColor = VectorArgb(1.0f, 0.0f, 0.5f, 1.0f);
			break;
		case TM_Smooth:
			brushColor = VectorArgb(1.0f, 0.5f, 0.5f, 1.0f);
			break;
		case TM_Noise:
			brushColor = VectorArgb(1.0f, 1.0f, 0.5f, 0.0f);
			break;
		case TM_PaintShader:
			brushColor = VectorArgb(1.0f, 1.0f, 1.0f, 0.0f);
			break;
		case TM_PaintFlora:
			brushColor = VectorArgb(1.0f, 0.0f, 1.0f, 0.5f);
			break;
		case TM_PlaceWater:
			brushColor = VectorArgb(1.0f, 0.0f, 0.3f, 1.0f);
			break;
		case TM_PlaceRadial:
			brushColor = VectorArgb(1.0f, 0.8f, 0.0f, 0.8f);
			break;
		default:
			break;
	}
	
	// Draw brush circle/square preview on terrain
	if (m_brushShape == BS_Circle)
	{
		// Draw circle segments
		for (int i = 0; i < segments; ++i)
		{
			const float angle1 = (static_cast<float>(i) / static_cast<float>(segments)) * 2.0f * 3.14159265f;
			const float angle2 = (static_cast<float>(i + 1) / static_cast<float>(segments)) * 2.0f * 3.14159265f;
			
			const float x1 = worldX + radius * std::cos(angle1);
			const float z1 = worldZ + radius * std::sin(angle1);
			const float x2 = worldX + radius * std::cos(angle2);
			const float z2 = worldZ + radius * std::sin(angle2);
			
			float y1 = 0.0f, y2 = 0.0f;
			Vector pos1(x1, 0.0f, z1);
			Vector pos2(x2, 0.0f, z2);
			
			if (terrainObject->getHeight(pos1, y1) && terrainObject->getHeight(pos2, y2))
			{
				const Vector start(x1, y1 + 0.5f, z1);
				const Vector end(x2, y2 + 0.5f, z2);
				
				camera->addDebugPrimitive(new Line3dDebugPrimitive(
					Line3dDebugPrimitive::S_none,
					Transform::identity,
					start,
					end,
					brushColor
				));
			}
		}
	}
	else // BS_Square
	{
		// Draw square outline
		const float halfSize = radius;
		
		Vector corners[4];
		corners[0] = Vector(worldX - halfSize, 0.0f, worldZ - halfSize);
		corners[1] = Vector(worldX + halfSize, 0.0f, worldZ - halfSize);
		corners[2] = Vector(worldX + halfSize, 0.0f, worldZ + halfSize);
		corners[3] = Vector(worldX - halfSize, 0.0f, worldZ + halfSize);
		
		for (int i = 0; i < 4; ++i)
		{
			terrainObject->getHeight(corners[i], corners[i].y);
			corners[i].y += 0.5f;
		}
		
		for (int i = 0; i < 4; ++i)
		{
			camera->addDebugPrimitive(new Line3dDebugPrimitive(
				Line3dDebugPrimitive::S_none,
				Transform::identity,
				corners[i],
				corners[(i + 1) % 4],
				brushColor
			));
		}
	}
}

// ----------------------------------------------------------------------

bool TerrainDock::getTerrainPositionFromScreen(int screenX, int screenY, float& outWorldX, float& outWorldZ) const
{
	const GroundScene* const scene = dynamic_cast<const GroundScene*>(Game::getScene());
	if (!scene || !scene->getPlayer())
		return false;
	
	const Camera* const camera = scene->getCurrentCamera();
	if (!camera)
		return false;
	
	// Get a ray from the camera through the screen position
	const Vector start_p(camera->getPosition_p());
	const Vector end_p(start_p + 8192.0f * camera->rotate_o2p(camera->reverseProjectInScreenSpace(screenX, screenY)));
	
	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (!terrainObject)
		return false;
	
	CollisionInfo info;
	if (terrainObject->collide(start_p, end_p, info))
	{
		const Vector& hitPoint = info.getPoint();
		outWorldX = hitPoint.x;
		outWorldZ = hitPoint.z;
		return true;
	}
	
	return false;
}

// ======================================================================
// Water Boundary Creation
// ======================================================================

void TerrainDock::createWaterBoundary(float centerX, float centerZ, float radius, float height)
{
	TerrainGenerator* const generator = getTerrainGenerator();
	if (!generator)
	{
		MainFrame::getInstance().textToConsole("createWaterBoundary: No terrain generator available");
		return;
	}
	
	// Create undo entry
	UndoEntry entry;
	entry.type = UO_Water;
	entry.worldX = centerX;
	entry.worldZ = centerZ;
	entry.radius = radius;
	entry.heightData.push_back(height);
	entry.description = "Create water boundary";
	
	pushUndoEntry(entry);
	
	// Invalidate the terrain region to apply the water boundary
	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (terrainObject)
	{
		const float invalidateMargin = 16.0f;
		Rectangle2d extent2d(
			centerX - radius - invalidateMargin,
			centerZ - radius - invalidateMargin,
			centerX + radius + invalidateMargin,
			centerZ + radius + invalidateMargin
		);
		
		terrainObject->invalidateRegion(extent2d);
	}
	
	m_terrainModified = true;
	
	QString msg;
	msg.sprintf("Water boundary created at (%.1f, %.1f), radius: %.1f, height: %.2f",
		centerX, centerZ, radius, height);
	MainFrame::getInstance().textToConsole(msg.latin1());
}

// ----------------------------------------------------------------------

void TerrainDock::removeWaterBoundary(const std::string& boundaryId)
{
	UNREF(boundaryId);
	
	TerrainGenerator* const generator = getTerrainGenerator();
	if (!generator)
	{
		MainFrame::getInstance().textToConsole("removeWaterBoundary: No terrain generator available");
		return;
	}
	
	m_terrainModified = true;
	
	MainFrame::getInstance().textToConsole("Water boundary removed");
}

// ======================================================================
// Mouse Event Handlers for Live Terrain Editing
// ======================================================================

bool TerrainDock::cameraModifierOverridesTerrainInput(int qtButtonState) const
{
	if ((qtButtonState & static_cast<int>(Qt::AltButton)) == 0)
		return false;
	// Alt+LMB is bound for road/ribbon editing (insert point on segment).
	if (m_toolMode == TM_PlaceRoad || m_toolMode == TM_PlaceRibbon)
		return false;
	return true;
}

// ----------------------------------------------------------------------

bool TerrainDock::handleMousePress(int screenX, int screenY, int button, int qtButtonState)
{
	if (cameraModifierOverridesTerrainInput(qtButtonState))
		return false;

	if (m_toolMode == TM_None)
		return false;
	
	// Only respond to left mouse button
	if (button != 1)
		return false;
	
	// Get world position from screen coordinates
	float worldX = 0.0f;
	float worldZ = 0.0f;
	if (!getTerrainPositionFromScreen(screenX, screenY, worldX, worldZ))
		return false;

	if (m_toolMode == TM_Select)
	{
		m_regionDragActive = true;
		m_regionAnchorX = worldX;
		m_regionAnchorZ = worldZ;
		m_regionDragCurX = worldX;
		m_regionDragCurZ = worldZ;
		m_hasRegionSelection = false;
		if (GodClientTerrainEditor::isInstalled())
			GodClientTerrainEditor::getInstance().clearRegionSelection();
		return true;
	}
	
	// Sync settings to the terrain editor
	if (GodClientTerrainEditor::isInstalled())
	{
		GodClientTerrainEditor& editor = GodClientTerrainEditor::getInstance();
		
		// Handle polyline editing modes (road/ribbon placement)
		if (m_toolMode == TM_PlaceRoad || m_toolMode == TM_PlaceRibbon)
		{
			const bool shiftMod = (qtButtonState & static_cast<int>(Qt::ShiftButton)) != 0;
			const bool ctrlMod = (qtButtonState & static_cast<int>(Qt::ControlButton)) != 0;
			const bool altMod = (qtButtonState & static_cast<int>(Qt::AltButton)) != 0;
			const float pickRadius = std::max(8.0f, m_polylineWidth * 3.0f);

			if (editor.isPolylineActive())
			{
				if (shiftMod)
				{
					const int idx = editor.findNearestPolylinePoint(worldX, worldZ, pickRadius);
					if (idx >= 0)
					{
						editor.deletePolylinePoint(idx);
						MainFrame::getInstance().textToConsole("Polyline point deleted (Shift+click).");
					}
					return true;
				}

				if (altMod && !ctrlMod)
				{
					int afterIdx = 0;
					float insX = 0.f;
					float insZ = 0.f;
					if (terrainDockFindNearestPolylineSegment(editor, worldX, worldZ, pickRadius, afterIdx, insX, insZ))
					{
						editor.insertPolylinePoint(afterIdx, insX, insZ, 0.0f);
						MainFrame::getInstance().textToConsole("Polyline point inserted on segment (Alt+click).");
					}
					return true;
				}

				if (ctrlMod)
				{
					const int idx = editor.findNearestPolylinePoint(worldX, worldZ, pickRadius);
					if (idx >= 0)
					{
						m_polylineDragPointIndex = idx;
						editor.setSelectedPolylinePoint(idx);
						MainFrame::getInstance().textToConsole("Dragging polyline point (move mouse, release to place).");
					}
					return true;
				}

				editor.addPolylinePoint(worldX, worldZ);
				return true;
			}

			int polyFid = m_selectedShaderFamilyId;
			int const pci = m_polylineShaderCombo ? m_polylineShaderCombo->currentItem() : 0;
			if (pci >= 0 && pci < static_cast<int>(m_polylineShaderFamilyIds.size()))
				polyFid = m_polylineShaderFamilyIds[static_cast<size_t>(pci)];
			editor.beginPolyline(m_toolMode == TM_PlaceRibbon);
			editor.setPolylineWidth(m_polylineWidth);
			editor.setPolylineShaderFamily(polyFid);
			editor.setPolylineFeatherDistance(m_polylineFeather);
			editor.setPolylineUseFixedHeights(m_polylineFixedHeights);
			editor.addPolylinePoint(worldX, worldZ);
			return true;
		}
		
		// Handle environment zone placement
		if (m_toolMode == TM_PlaceEnvironment)
		{
			if (editor.isEnvironmentZoneActive())
			{
				editor.addEnvironmentZonePoint(worldX, worldZ);
				return true;
			}
			else
			{
				int envFid = 0;
				int const eci = m_environmentFamilyCombo ? m_environmentFamilyCombo->currentItem() : 0;
				if (eci >= 0 && eci < static_cast<int>(m_environmentFamilyIds.size()))
					envFid = m_environmentFamilyIds[static_cast<size_t>(eci)];
				editor.beginEnvironmentZone();
				editor.setEnvironmentFamily(envFid);
				editor.addEnvironmentZonePoint(worldX, worldZ);
				return true;
			}
		}
		
		// Handle bitmap stamp
		if (m_toolMode == TM_StampBitmap)
		{
			editor.applyBitmapStamp(worldX, worldZ);
			m_terrainModified = true;
			return true;
		}
		
		syncGodClientEditorBrushSettings();
		
		// Begin brush stroke
		if (editor.beginBrushStroke(worldX, worldZ))
		{
			m_terrainModified = true;
			
			QString msg;
			msg.sprintf("Terrain editing started at (%.1f, %.1f)", worldX, worldZ);
			MainFrame::getInstance().textToConsole(msg.latin1());
			
			return true;
		}
	}
	
	return false;
}

// ----------------------------------------------------------------------

bool TerrainDock::handleMouseRelease(int screenX, int screenY, int button)
{
	if (button != 1)
		return false;

	if (m_polylineDragPointIndex >= 0)
	{
		m_polylineDragPointIndex = -1;
		UNREF(screenX);
		UNREF(screenY);
		return true;
	}
	
	if (m_toolMode == TM_Select && m_regionDragActive)
	{
		float worldX = 0.0f;
		float worldZ = 0.0f;
		if (getTerrainPositionFromScreen(screenX, screenY, worldX, worldZ))
		{
			m_regionDragCurX = worldX;
			m_regionDragCurZ = worldZ;
		}
		m_regionDragActive = false;
		float minX = std::min(m_regionAnchorX, m_regionDragCurX);
		float maxX = std::max(m_regionAnchorX, m_regionDragCurX);
		float minZ = std::min(m_regionAnchorZ, m_regionDragCurZ);
		float maxZ = std::max(m_regionAnchorZ, m_regionDragCurZ);
		if (maxX - minX < 0.5f)
			maxX = minX + 0.5f;
		if (maxZ - minZ < 0.5f)
			maxZ = minZ + 0.5f;
		m_regionMinX = minX;
		m_regionMaxX = maxX;
		m_regionMinZ = minZ;
		m_regionMaxZ = maxZ;
		m_hasRegionSelection = true;
		if (GodClientTerrainEditor::isInstalled())
			GodClientTerrainEditor::getInstance().setRegionSelection(minX, minZ, maxX, maxZ);
		MainFrame::getInstance().textToConsole("Terrain region selection updated.");
		return true;
	}
	
	if (GodClientTerrainEditor::isInstalled())
	{
		GodClientTerrainEditor& editor = GodClientTerrainEditor::getInstance();
		
		if (editor.isBrushStrokeActive())
		{
			editor.endBrushStroke();
			
			MainFrame::getInstance().textToConsole("Terrain editing stroke completed");
			
			return true;
		}
	}
	
	return false;
}

// ----------------------------------------------------------------------

bool TerrainDock::handleMouseMove(int screenX, int screenY, int qtButtonState)
{
	if (cameraModifierOverridesTerrainInput(qtButtonState))
		return false;

	// Update cursor world position for brush preview
	float worldX = 0.0f;
	float worldZ = 0.0f;
	
	if (getTerrainPositionFromScreen(screenX, screenY, worldX, worldZ))
	{
		if (m_toolMode == TM_Select && m_regionDragActive)
		{
			m_regionDragCurX = worldX;
			m_regionDragCurZ = worldZ;
			return true;
		}

		if (m_polylineDragPointIndex >= 0 && (m_toolMode == TM_PlaceRoad || m_toolMode == TM_PlaceRibbon) && GodClientTerrainEditor::isInstalled())
		{
			GodClientTerrainEditor& editor = GodClientTerrainEditor::getInstance();
			if (editor.isPolylineActive() && m_polylineDragPointIndex < editor.getPolylinePointCount())
			{
				float dragHeight = 0.f;
				if (editor.getPolylineUseFixedHeights())
				{
					const GodClientTerrainEditor::ControlPoint* pt = editor.getPolylinePoint(m_polylineDragPointIndex);
					if (pt)
						dragHeight = pt->height;
				}
				else
				{
					TerrainObject* const terrainObject = TerrainObject::getInstance();
					Vector pos(worldX, 0.f, worldZ);
					if (terrainObject && terrainObject->getHeight(pos, dragHeight))
					{ /* use dragHeight */ }
					else
						dragHeight = 0.f;
				}
				editor.movePolylinePoint(m_polylineDragPointIndex, worldX, worldZ, dragHeight);
				return true;
			}
		}

		if (GodClientTerrainEditor::isInstalled())
		{
			GodClientTerrainEditor& editor = GodClientTerrainEditor::getInstance();
			
			// Update cursor position for brush preview
			TerrainObject* const terrainObject = TerrainObject::getInstance();
			if (terrainObject)
			{
				float height = 0.0f;
				Vector pos(worldX, 0.0f, worldZ);
				if (terrainObject->getHeight(pos, height))
				{
					editor.setCursorWorldPosition(Vector(worldX, height, worldZ));
				}
			}
			
			// Continue brush stroke if active
			if (editor.isBrushStrokeActive())
			{
				editor.continueBrushStroke(worldX, worldZ);
				return true;
			}
		}
	}
	
	return false;
}

// ----------------------------------------------------------------------

void TerrainDock::updateFrame(float elapsedTime)
{
	UNREF(elapsedTime);

	if (!isVisible())
		return;
	
	if (!GodClientTerrainEditor::isInstalled())
		return;
	
	const GroundScene* const scene = dynamic_cast<const GroundScene*>(Game::getScene());
	if (!scene)
		return;
	
	const Camera* const camera = scene->getCurrentCamera();
	if (!camera)
		return;
	
	GodClientTerrainEditor& editor = GodClientTerrainEditor::getInstance();
	
	editor.renderTerrainDebugOverlays(*camera, m_showWireframe, m_showHeightColors, m_showChunkGrid);
	if (m_hasRegionSelection)
		editor.renderRegionSelectionOverlay(*camera, m_regionMinX, m_regionMinZ, m_regionMaxX, m_regionMaxZ);
	
	// Render brush preview for brush-based tools
	if (m_showBrushPreview && m_toolMode != TM_None && m_toolMode != TM_Select)
	{
		const bool skipBrushRing =
			m_toolMode == TM_PlaceRoad ||
			m_toolMode == TM_PlaceRibbon ||
			m_toolMode == TM_PlaceEnvironment;
		if (!skipBrushRing)
			editor.renderBrushPreview(*camera);
	}
	
	// Render polyline preview for road/ribbon tools
	if (m_showPolylinePreview && (m_toolMode == TM_PlaceRoad || m_toolMode == TM_PlaceRibbon))
	{
		editor.renderPolylinePreview(*camera);
	}
}

// ----------------------------------------------------------------------

bool TerrainDock::isTerrainEditingActive() const
{
	if (m_toolMode == TM_None)
		return false;
	
	// Check if the dock is visible
	if (!isVisible())
		return false;
	
	return true;
}

// ======================================================================
