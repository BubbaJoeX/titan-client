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
#include "sharedFile/TreeFile.h"
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
#include "clientGame/GameNetwork.h"
#include "clientGame/GroundScene.h"
#include "clientGame/FreeCamera.h"
#include "clientTerrain/ClientProceduralTerrainAppearance.h"

#include "clientGraphics/Camera.h"
#include "clientGraphics/DebugPrimitive.h"
#include "clientGraphics/Graphics.h"

#include "sharedMath/Rectangle2d.h"
#include "sharedMath/PackedRgb.h"
#include "sharedMath/VectorArgb.h"
#include "sharedMath/Transform.h"
#include "sharedCollision/CollisionInfo.h"
#include "sharedRandom/Random.h"
#include "sharedObject/Appearance.h"
#include "sharedObject/AppearanceTemplate.h"
#include "sharedNetworkMessages/ProceduralTerrainSyncMessages.h"
#include "sharedFoundation/Crc.h"

#include "GodClientData.h"
#include "GodClientTerrainEditor.h"
#include "EnvironmentEditorWindow.h"
#include "ShaderFamilyEditorWindow.h"
#include "FloraFamilyEditorWindow.h"
#include "RadialFamilyEditorWindow.h"
#include "BitmapFamilyEditorWindow.h"
#include "MainFrame.h"
#include "GameWidget.h"
#include "ConsoleWindow.h"

#include <qfiledialog.h>
#include <qinputdialog.h>
#include <qlineedit.h>
#include <qlistview.h>
#include <qmessagebox.h>
#include <qpushbutton.h>
#include <qtooltip.h>
#include <qslider.h>
#include <qspinbox.h>
#include <qcombobox.h>
#include <qcheckbox.h>
#include <qcolor.h>
#include <qcolordialog.h>
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
#include <qtimer.h>
#if defined(_MSC_VER)
#include <windows.h>
#endif
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <qdialog.h>
#include <qlayout.h>
#include <qobject.h>
#include <qobjectlist.h>
#include <qsizepolicy.h>
#include <qevent.h>

// ======================================================================

namespace
{
	class MapTemplateSettingsDialog : public QDialog
	{
	public:
		MapTemplateSettingsDialog(QWidget* parent, ProceduralTerrainAppearanceTemplate const* tpl)
		: QDialog(parent, "MapTemplateSettingsDialog", true)
		{
			setCaption("Map size & template");

			QVBoxLayout* const outer = new QVBoxLayout(this, 8, 6);
			NOT_NULL(outer);

			QLabel* const warn = new QLabel(
				"Changing map width, chunk width, or tiles per chunk can reset static collidable flora maps to empty grids.\n"
				"OK writes the current .trn to disk and reloads the terrain appearance.",
				this);
			warn->setAlignment(Qt::AlignLeft | Qt::AlignTop);
			outer->addWidget(warn);

			QGridLayout* const grid = new QGridLayout(9, 2, 6);
			NOT_NULL(grid);

			int row = 0;
			grid->addWidget(new QLabel("Map width (meters)", this), row, 0);
			m_mapWidthEdit = new QLineEdit(this);
			grid->addWidget(m_mapWidthEdit, row++, 1);

			grid->addWidget(new QLabel("Chunk width (meters)", this), row, 0);
			m_chunkWidthEdit = new QLineEdit(this);
			grid->addWidget(m_chunkWidthEdit, row++, 1);

			grid->addWidget(new QLabel("Tiles per chunk", this), row, 0);
			m_tilesPerChunkSpin = new QSpinBox(this);
			m_tilesPerChunkSpin->setMinValue(1);
			m_tilesPerChunkSpin->setMaxValue(256);
			grid->addWidget(m_tilesPerChunkSpin, row++, 1);

			m_globalWaterCheck = new QCheckBox("Use global water table", this);
			grid->addMultiCellWidget(m_globalWaterCheck, row, row, 0, 1);
			++row;

			grid->addWidget(new QLabel("Global water height (m)", this), row, 0);
			m_waterHeightEdit = new QLineEdit(this);
			grid->addWidget(m_waterHeightEdit, row++, 1);

			grid->addWidget(new QLabel("Global water shader size", this), row, 0);
			m_waterShaderSizeEdit = new QLineEdit(this);
			grid->addWidget(m_waterShaderSizeEdit, row++, 1);

			grid->addWidget(new QLabel("Environment cycle (seconds)", this), row, 0);
			m_envCycleEdit = new QLineEdit(this);
			grid->addWidget(m_envCycleEdit, row++, 1);

			outer->addLayout(grid);

			QHBoxLayout* const buttons = new QHBoxLayout(6);
			NOT_NULL(buttons);
			QPushButton* const okButton = new QPushButton("OK", this);
			QPushButton* const cancelButton = new QPushButton("Cancel", this);
			buttons->addStretch(1);
			buttons->addWidget(okButton);
			buttons->addWidget(cancelButton);
			outer->addLayout(buttons);

			IGNORE_RETURN(connect(okButton, SIGNAL(clicked()), this, SLOT(accept())));
			IGNORE_RETURN(connect(cancelButton, SIGNAL(clicked()), this, SLOT(reject())));

			if (tpl)
			{
				char buf[128];
				snprintf(buf, sizeof(buf), "%.2f", tpl->getMapWidthInMeters());
				m_mapWidthEdit->setText(QString::fromLatin1(buf));
				snprintf(buf, sizeof(buf), "%.2f", tpl->getChunkWidthInMeters());
				m_chunkWidthEdit->setText(QString::fromLatin1(buf));
				m_tilesPerChunkSpin->setValue(tpl->getNumberOfTilesPerChunk());
				m_globalWaterCheck->setChecked(tpl->getUseGlobalWaterTable());
				snprintf(buf, sizeof(buf), "%.2f", tpl->getGlobalWaterTableHeight());
				m_waterHeightEdit->setText(QString::fromLatin1(buf));
				snprintf(buf, sizeof(buf), "%.2f", tpl->getGlobalWaterTableShaderSize());
				m_waterShaderSizeEdit->setText(QString::fromLatin1(buf));
				snprintf(buf, sizeof(buf), "%.2f", tpl->getEnvironmentCycleTime());
				m_envCycleEdit->setText(QString::fromLatin1(buf));
			}
		}

		bool readValues(
			float& mapW,
			float& chunkW,
			int& tiles,
			bool& useGlobalWater,
			float& waterH,
			float& waterShaderSize,
			float& envCycle) const
		{
			mapW = 0.f;
			chunkW = 0.f;
			tiles = 1;
			useGlobalWater = false;
			waterH = 0.f;
			waterShaderSize = 0.f;
			envCycle = 0.f;

			if (!m_mapWidthEdit || sscanf(m_mapWidthEdit->text().latin1(), "%f", &mapW) != 1 || mapW <= 0.f)
				return false;
			if (!m_chunkWidthEdit || sscanf(m_chunkWidthEdit->text().latin1(), "%f", &chunkW) != 1 || chunkW <= 0.f)
				return false;
			if (!m_tilesPerChunkSpin)
				return false;
			tiles = m_tilesPerChunkSpin->value();
			if (tiles < 1)
				return false;

			useGlobalWater = m_globalWaterCheck && m_globalWaterCheck->isChecked();

			if (!m_waterHeightEdit || sscanf(m_waterHeightEdit->text().latin1(), "%f", &waterH) != 1)
				return false;
			if (!m_waterShaderSizeEdit || sscanf(m_waterShaderSizeEdit->text().latin1(), "%f", &waterShaderSize) != 1)
				return false;
			if (!m_envCycleEdit || sscanf(m_envCycleEdit->text().latin1(), "%f", &envCycle) != 1 || envCycle <= 0.f)
				return false;

			return true;
		}

	private:
		QLineEdit*  m_mapWidthEdit;
		QLineEdit*  m_chunkWidthEdit;
		QSpinBox*   m_tilesPerChunkSpin;
		QCheckBox*  m_globalWaterCheck;
		QLineEdit*  m_waterHeightEdit;
		QLineEdit*  m_waterShaderSizeEdit;
		QLineEdit*  m_envCycleEdit;
	};

	void installTerrainRegionTooltips(
		QPushButton* exclude,
		QPushButton* mask,
		QPushButton* path,
		QPushButton* corridor,
		QWidget* loopGroup,
		QPushButton* discard)
	{
		if (exclude)
			QToolTip::add(exclude, QString::fromLatin1(
				"Exclude terrain: closed loop, click corners in the 3D view. Need 3+ corners, then Create. Interior skips procedural mesh."));
		if (mask)
			QToolTip::add(mask, QString::fromLatin1(
				"Boundary mask: same loop workflow as exclude, but adds a BoundaryPolygon mask for child affectors."));
		if (path)
			QToolTip::add(path, QString::fromLatin1(
				"Boundary path: open polyline with ordered clicks. Finish on Advanced, Roads / Ribbons."));
		if (corridor)
			QToolTip::add(corridor, QString::fromLatin1(
				"Boundary corridor: wide polyline corridor; finish on Advanced, Roads / Ribbons."));
		if (loopGroup)
			QToolTip::add(loopGroup, QString::fromLatin1(
				"Exclude / mask: Create commits the loop; Discard cancels."));
		if (discard)
			QToolTip::add(discard, QString::fromLatin1("Discard the in-progress loop."));
	}

	void installRegionOperationsTooltips(
		QPushButton* selectRegion,
		QPushButton* copyRegion,
		QPushButton* pasteRegion,
		QPushButton* fillRegion,
		QPushButton* saveLay,
		QPushButton* loadLay,
		QPushButton* importLay,
		QLabel* regionShapeLabel,
		QComboBox* regionShapeCombo,
		QLabel* mapSummaryLabel)
	{
		if (selectRegion)
			QToolTip::add(selectRegion, QString::fromLatin1(
				"Toggle region selection: drag in the 3D view to set rectangle or circle bounds."));
		if (copyRegion)
			QToolTip::add(copyRegion, QString::fromLatin1(
				"Copy region heights to the clipboard (one meter height cells; circle selection uses a mask)."));
		if (pasteRegion)
			QToolTip::add(pasteRegion, QString::fromLatin1(
				"Paste clipboard heights anchored to the floor of the current region minimum corner."));
		if (fillRegion)
			QToolTip::add(fillRegion, QString::fromLatin1(
				"Fill the selected region using the active height tool or shader paint mode."));
		if (saveLay)
			QToolTip::add(saveLay, QString::fromLatin1(
				"Save the clipboard region heights (and mask) to a .lay file on disk."));
		if (loadLay)
			QToolTip::add(loadLay, QString::fromLatin1(
				"Load a .lay file into the region clipboard for Paste or inspection."));
		if (importLay)
			QToolTip::add(importLay, QString::fromLatin1(
				"Import a .lay file and apply heights at the terrain editor cursor (floor of cursor X/Z)."));
		if (regionShapeLabel)
			QToolTip::add(regionShapeLabel, QString::fromLatin1(
				"Region selection shape: rectangle uses opposite corners; circle uses center and radius."));
		if (regionShapeCombo)
			QToolTip::add(regionShapeCombo, QString::fromLatin1(
				"Rectangle: axis-aligned min/max. Circle: distance from center with feather at the edge."));
		if (mapSummaryLabel)
			QToolTip::add(mapSummaryLabel, QString::fromLatin1(
				"Map width, chunk width, tiles per chunk, derived tile width, and number of loaded chunks."));
	}

	void installMapTemplateEditorTooltips(
		QLabel* hint,
		QPushButton* mapSettings,
		QPushButton* addHeightConst,
		QPushButton* addShaderConst,
		QPushButton* addExcludeRegion)
	{
		if (hint)
			QToolTip::add(hint, QString::fromLatin1(
				"Map size, global water, and environment cycle: open Map Parameters (dialog). OK saves the .trn and reloads terrain.\n"
				"Buttons below: Height Constant and Shader Constant add full-map procedural layers; exclude uses the current region."));
		if (mapSettings)
			QToolTip::add(mapSettings, QString::fromLatin1(
				"Map Parameters: edit map width, chunk width, tiles per chunk, global water height/shader size, and environment cycle. OK saves the .trn and reloads the terrain."));
		if (addHeightConst)
			QToolTip::add(addHeightConst, QString::fromLatin1(
				"Height Constant: append a full-map generator layer using AffectorHeightConstant (prompts for height in meters)."));
		if (addShaderConst)
			QToolTip::add(addShaderConst, QString::fromLatin1(
				"Shader Constant: append a full-map layer with AffectorShaderConstant using the shader family currently selected on the Shaders tab."));
		if (addExcludeRegion)
			QToolTip::add(addExcludeRegion, QString::fromLatin1(
				"Append an AffectorExclude layer for the current region rectangle (prompts for boundary feather in meters)."));
	}
}

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

	inline bool terrainDockIsPolylineStrokeTool(TerrainDock::ToolMode const m)
	{
		return m == TerrainDock::TM_PlaceRoad
			|| m == TerrainDock::TM_PlaceRibbon
			|| m == TerrainDock::TM_PlaceBoundaryPolyline
			|| m == TerrainDock::TM_PlaceBoundaryPolyRoad;
	}

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

	void terrainDockTryPushTerrainFileToLiveServer(std::string const & writtenDiskPath, char const * proceduralTerrainTemplateTreeName)
	{
		static uint32 const maxBytes = 32u * 1024u * 1024u;
		static uint32 const chunkBytes = 45000u;

		if (!proceduralTerrainTemplateTreeName || !proceduralTerrainTemplateTreeName[0])
			return;
		if (!GameNetwork::isConnectedToConnectionServer())
			return;

		std::ifstream input(writtenDiskPath.c_str(), std::ios::binary | std::ios::ate);
		if (!input)
			return;

		std::streamoff const endPos = input.tellg();
		if (endPos <= 0)
			return;

		size_t const fileSizeUnsigned = static_cast<size_t>(endPos);
		if (fileSizeUnsigned > static_cast<size_t>(maxBytes))
		{
			MainFrame::getInstance().textToConsole("Terrain not pushed: file exceeds procedural sync limit (32MiB).");
			return;
		}

		input.seekg(0);
		std::vector<unsigned char> bytes(fileSizeUnsigned);
		input.read(reinterpret_cast<char *>(&bytes[0]), static_cast<std::streamsize>(fileSizeUnsigned));
		if (!input)
			return;

		uint32 const totalSize = static_cast<uint32>(bytes.size());
		uint32 const crc = Crc::calculate(&bytes[0], static_cast<int>(bytes.size()), Crc::crcInit);
		std::string const terrainName(proceduralTerrainTemplateTreeName);

		for (uint32 offset = 0; offset < totalSize; )
		{
			uint32 const slice = std::min(chunkBytes, totalSize - offset);
			std::vector<unsigned char> chunk(
				bytes.begin() + static_cast<std::vector<unsigned char>::difference_type>(offset),
				bytes.begin() + static_cast<std::vector<unsigned char>::difference_type>(offset + slice));
			ProceduralTerrainSyncChunkMessage const chunkMessage(terrainName, totalSize, crc, offset, chunk);
			GameNetwork::send(chunkMessage, true);
			offset += slice;
		}

		MainFrame::getInstance().textToConsole("Terrain pushed to live server (procedural .trn replication).");
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

	// Resolve water shader templates through TreeFile virtual paths (archives + search roots): shader/wter_*.sht
	void terrainDockGatherWaterShaderTemplatesFromDisk(std::vector<std::pair<QString, std::string> >& out, std::set<std::string>& seen, int& budget)
	{
		if (budget <= 0)
			return;

		std::vector<std::string> paths;
		TreeFile::collectVirtualPathNamesWithPrefixAndSuffix("shader/wter_", ".sht", paths);
		std::sort(paths.begin(), paths.end());

		for (size_t i = 0; i < paths.size() && budget > 0; ++i)
		{
			std::string const& vp = paths[i];
			if (vp.find("shader/wter_") != 0)
				continue;

			size_t const slashPos = vp.find_last_of('/');
			std::string baseFile = (slashPos != std::string::npos) ? vp.substr(slashPos + 1) : vp;
			size_t const dotPos = baseFile.rfind('.');
			if (dotPos != std::string::npos)
				baseFile = baseFile.substr(0, dotPos);

			if (baseFile.size() < 5 || baseFile.compare(0, 5, "wter_") != 0)
				continue;

			std::string const tpl = baseFile;
			if (!seen.insert(tpl).second)
				continue;

			char const* const resolved = TreeFile::getShortestExistingPath(vp.c_str());
			QString const resolvedQs =
				QString::fromLatin1(resolved ? resolved : vp.c_str());
			QString const label =
				QString::fromLatin1("%1 (%2)")
					.arg(QString::fromLatin1(tpl.c_str()))
					.arg(resolvedQs);

			out.push_back(std::make_pair(label, tpl));
			--budget;
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

			new QListViewItem(globalShaderList, idTxt, primaryShaderName, fileCanon);
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

	bool terrainDockShaderFamilyChildrenMatch(ShaderGroup const& a, ShaderGroup const& b, int familyIdA, int familyIdB)
	{
		if (!a.hasFamily(familyIdA) || !b.hasFamily(familyIdB))
			return false;
		int const na = a.getFamilyNumberOfChildren(familyIdA);
		int const nb = b.getFamilyNumberOfChildren(familyIdB);
		if (na != nb)
			return false;
		for (int i = 0; i < na; ++i)
		{
			ShaderGroup::FamilyChildData const ca(a.getFamilyChild(familyIdA, i));
			ShaderGroup::FamilyChildData const cb(b.getFamilyChild(familyIdB, i));
			char const* namA = ca.shaderTemplateName ? ca.shaderTemplateName : "";
			char const* namB = cb.shaderTemplateName ? cb.shaderTemplateName : "";
			if (_stricmp(namA, namB) != 0)
				return false;
		}
		return true;
	}

	int terrainDockFindDstShaderFamilyMatchingChildren(ShaderGroup const& src, int const srcFamilyId, ShaderGroup const& dst)
	{
		if (!src.hasFamily(srcFamilyId))
			return -1;
		int const n = dst.getNumberOfFamilies();
		for (int i = 0; i < n; ++i)
		{
			int const dstFid = dst.getFamilyId(i);
			if (terrainDockShaderFamilyChildrenMatch(src, dst, srcFamilyId, dstFid))
				return dstFid;
		}
		return -1;
	}

	int terrainDockFindUnusedTerrainShaderFamilyId(ShaderGroup const& sg)
	{
		bool used[256];
		for (int u = 0; u < 256; ++u)
			used[u] = false;
		const int nf = sg.getNumberOfFamilies();
		for (int fi = 0; fi < nf; ++fi)
		{
			int const fid = sg.getFamilyId(fi);
			if (fid >= 0 && fid < 256)
				used[fid] = true;
		}
		for (int trial = 0; trial < 256; ++trial)
		{
			if (!used[trial])
				return trial;
		}
		return -1;
	}

	bool terrainDockCopyTerrainShaderFamilyRemapIds(ShaderGroup const& src, ShaderGroup& dst, int srcFamilyId, int dstFamilyId, bool overwriteExisting)
	{
		if (!src.hasFamily(srcFamilyId))
			return false;
		if (dst.hasFamily(dstFamilyId))
		{
			if (!overwriteExisting)
				return false;
			dst.removeFamily(dstFamilyId);
		}

		PackedRgb const color(src.getFamilyColor(srcFamilyId));
		char const* fname = src.getFamilyName(srcFamilyId);
		if (!fname)
			fname = "";
		dst.addFamily(dstFamilyId, fname, color);
		dst.setFamilyShaderSize(dstFamilyId, src.getFamilyShaderSize(srcFamilyId));
		dst.setFamilyFeatherClamp(dstFamilyId, src.getFamilyFeatherClamp(srcFamilyId));

		char const* const sp(src.getFamilySurfacePropertiesName(srcFamilyId));
		if (sp && *sp)
			dst.setFamilySurfacePropertiesName(dstFamilyId, sp);

		const int nChildren(src.getFamilyNumberOfChildren(srcFamilyId));
		for (int ci = 0; ci < nChildren; ++ci)
		{
			ShaderGroup::FamilyChildData fcd(src.getFamilyChild(srcFamilyId, ci));
			fcd.familyId = dstFamilyId;
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

	float raiseLowerSpeedFromSlider(int const sliderValue)
	{
		int const v = std::max(1, std::min(200, sliderValue));
		return 0.01f + (static_cast<float>(v - 1) / 199.f) * (4.0f - 0.01f);
	}

	void polishTerrainDockWideControls(QWidget* w)
	{
		if (!w)
			return;
		if (w->inherits("QPushButton"))
			w->setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum));
		else if (w->inherits("QComboBox") || w->inherits("QLineEdit"))
			w->setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
		else if (w->inherits("QCheckBox"))
			w->setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed));
		else if (w->inherits("QListView"))
			w->setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Expanding));
		else if (w->inherits("QTabWidget"))
			w->setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Expanding));

		QObjectList const* const kids = w->children();
		if (kids)
		{
			QObjectListIt it(*kids);
			QObject* o = 0;
			while ((o = it.current()) != 0)
			{
				++it;
				if (o->isWidgetType())
					polishTerrainDockWideControls(static_cast<QWidget*>(o));
			}
		}
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

		int const kMaxTerrainDockContentWidth = 525;
		int const vw = scrollView->visibleWidth();
		// Match document width to the viewport so controls do not sit under the vertical scrollbar.
		if (vw > 0)
			w = std::min(kMaxTerrainDockContentWidth, vw);
		else
			w = std::min(kMaxTerrainDockContentWidth, w);

		scrollView->resizeContents(w, h);
		documentWidget->resize(w, h);
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
  m_raiseLowerSpeed(0.25f),
  m_raiseLowerBias(0.f),
  m_raiseLowerClickRate(1.f),
  m_raiseLowerJitter(0.f),
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
  m_shaderColorConstantPaintEnabled(false),
  m_shaderColorConstantPickValid(false),
  m_shaderColorConstantR(128),
  m_shaderColorConstantG(128),
  m_shaderColorConstantB(128),
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
  m_regionSelectionShape(RSS_Rectangle),
  m_regionCircleCenterX(0.0f),
  m_regionCircleCenterZ(0.0f),
  m_regionCircleRadius(0.0f),
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
  m_callback(0),
  m_liveEditGroundPickFallbackValid(false),
  m_liveEditGroundPickFallbackY(0.0f)
{
	initializeUI();
	loadTerrainShaderScanRootsFromSettings();

	setMaximumWidth(525);

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
	if (m_shaderColorConstantCheck)
	{
		IGNORE_RETURN(connect(m_shaderColorConstantCheck, SIGNAL(toggled(bool)), this, SLOT(onShaderColorConstantToggled(bool))));
		QToolTip::add(
			m_shaderColorConstantCheck,
			QString::fromLatin1(
				"When on, shader paint uses the picked RGB matched to the nearest scene shader family preview color.\n"
				"Disabled while painting from the global catalog (use scene shader list)."));
	}
	if (m_shaderColorPickButton)
	{
		IGNORE_RETURN(connect(m_shaderColorPickButton, SIGNAL(clicked()), this, SLOT(onShaderColorConstantPickClicked())));
		QToolTip::add(
			m_shaderColorPickButton,
			QString::fromLatin1("Choose a color; painting applies the closest shader family by preview RGB."));
	}
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

	if (m_raiseLowerSpeedSlider)
		IGNORE_RETURN(connect(m_raiseLowerSpeedSlider, SIGNAL(valueChanged(int)), this, SLOT(onRaiseLowerSpeedChanged(int))));
	if (m_raiseLowerBiasSlider)
		IGNORE_RETURN(connect(m_raiseLowerBiasSlider, SIGNAL(valueChanged(int)), this, SLOT(onRaiseLowerBiasChanged(int))));
	if (m_raiseLowerClickSlider)
		IGNORE_RETURN(connect(m_raiseLowerClickSlider, SIGNAL(valueChanged(int)), this, SLOT(onRaiseLowerClickRateChanged(int))));
	if (m_raiseLowerJitterSlider)
		IGNORE_RETURN(connect(m_raiseLowerJitterSlider, SIGNAL(valueChanged(int)), this, SLOT(onRaiseLowerJitterChanged(int))));

	if (m_strengthLabel)
		QToolTip::add(m_strengthLabel, QString::fromLatin1(
			"Blend strength for Flatten, Smooth, Noise, and shader/flora paint. Raise and Lower use Speed in the Raise/Lower tuning group instead."));
	
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
	if (m_openShaderFamilyEditorButton)
	{
		IGNORE_RETURN(connect(m_openShaderFamilyEditorButton, SIGNAL(clicked()), this, SLOT(onOpenShaderFamilyEditor())));
		QToolTip::add(
			m_openShaderFamilyEditorButton,
			QString::fromLatin1("Inspect shader children and edit surface properties, feather clamp, and preview color."));
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
	if (m_openFloraFamilyEditorButton)
	{
		IGNORE_RETURN(connect(m_openFloraFamilyEditorButton, SIGNAL(clicked()), this, SLOT(onOpenFloraFamilyEditor())));
		QToolTip::add(
			m_openFloraFamilyEditorButton,
			QString::fromLatin1("Inspect flora children and edit density, floating placement, and preview color."));
	}
	if (m_radialGroupCombo)
		IGNORE_RETURN(connect(m_radialGroupCombo, SIGNAL(activated(int)), this, SLOT(onRadialGroupChanged(int))));
	if (m_openRadialFamilyEditorButton)
	{
		IGNORE_RETURN(connect(m_openRadialFamilyEditorButton, SIGNAL(clicked()), this, SLOT(onOpenRadialFamilyEditor())));
		QToolTip::add(
			m_openRadialFamilyEditorButton,
			QString::fromLatin1("Inspect radial children and edit density and preview color."));
	}
	
	// Connect region operation buttons
	if (m_selectRegionButton)
		IGNORE_RETURN(connect(m_selectRegionButton, SIGNAL(clicked()), this, SLOT(onSelectRegion())));
	if (m_copyRegionButton)
		IGNORE_RETURN(connect(m_copyRegionButton, SIGNAL(clicked()), this, SLOT(onCopyRegion())));
	if (m_pasteRegionButton)
		IGNORE_RETURN(connect(m_pasteRegionButton, SIGNAL(clicked()), this, SLOT(onPasteRegion())));
	if (m_fillRegionButton)
		IGNORE_RETURN(connect(m_fillRegionButton, SIGNAL(clicked()), this, SLOT(onFillRegion())));
	if (m_mapTemplateSettingsButton)
		IGNORE_RETURN(connect(m_mapTemplateSettingsButton, SIGNAL(clicked()), this, SLOT(onMapTemplateSettingsClicked())));
	if (m_addProcHeightConstButton)
		IGNORE_RETURN(connect(m_addProcHeightConstButton, SIGNAL(clicked()), this, SLOT(onAddProceduralHeightConstantLayer())));
	if (m_addProcShaderConstButton)
		IGNORE_RETURN(connect(m_addProcShaderConstButton, SIGNAL(clicked()), this, SLOT(onAddProceduralShaderConstantLayer())));
	if (m_addProcExcludeRegionButton)
		IGNORE_RETURN(connect(m_addProcExcludeRegionButton, SIGNAL(clicked()), this, SLOT(onAddProceduralExcludeFromRegion())));
	if (m_saveRegionLayButton)
		IGNORE_RETURN(connect(m_saveRegionLayButton, SIGNAL(clicked()), this, SLOT(onSaveRegionLay())));
	if (m_loadRegionLayButton)
		IGNORE_RETURN(connect(m_loadRegionLayButton, SIGNAL(clicked()), this, SLOT(onLoadRegionLay())));
	if (m_importRegionLayAtCursorButton)
		IGNORE_RETURN(connect(m_importRegionLayAtCursorButton, SIGNAL(clicked()), this, SLOT(onImportRegionLayAtCursor())));
	if (m_regionShapeCombo)
	{
		m_regionShapeCombo->clear();
		m_regionShapeCombo->insertItem("Rectangle");
		m_regionShapeCombo->insertItem("Circle");
		m_regionShapeCombo->setCurrentItem(static_cast<int>(m_regionSelectionShape));
		IGNORE_RETURN(connect(m_regionShapeCombo, SIGNAL(activated(int)), this, SLOT(onRegionShapeChanged(int))));
	}
	if (m_layerToggleActiveButton)
		IGNORE_RETURN(connect(m_layerToggleActiveButton, SIGNAL(clicked()), this, SLOT(onLayerToggleActive())));
	if (m_layerPromoteButton)
		IGNORE_RETURN(connect(m_layerPromoteButton, SIGNAL(clicked()), this, SLOT(onLayerPromote())));
	if (m_layerDemoteButton)
		IGNORE_RETURN(connect(m_layerDemoteButton, SIGNAL(clicked()), this, SLOT(onLayerDemote())));
	if (m_layerRenameButton)
		IGNORE_RETURN(connect(m_layerRenameButton, SIGNAL(clicked()), this, SLOT(onLayerRename())));
	
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
	if (m_toolApplyEnvironmentRegion)
	{
		IGNORE_RETURN(connect(m_toolApplyEnvironmentRegion, SIGNAL(clicked()), this, SLOT(onToolApplyEnvironmentRegion())));
		QToolTip::add(
			m_toolApplyEnvironmentRegion,
			QString::fromLatin1(
				"When on: left-click in the terrain view applies the selected environment family to the current region (8m feather).\n"
				"Use Fill Region for a feather prompt, or the Apply environment to region button."));
	}
	if (m_openEnvironmentEditorButton)
	{
		IGNORE_RETURN(connect(m_openEnvironmentEditorButton, SIGNAL(clicked()), this, SLOT(onOpenEnvironmentEditor())));
		QToolTip::add(
			m_openEnvironmentEditorButton,
			QString::fromLatin1("Edit environment family names, colors, and feather clamps for the live terrain generator."));
	}
	if (m_applyEnvironmentToRegionButton)
	{
		IGNORE_RETURN(connect(m_applyEnvironmentToRegionButton, SIGNAL(clicked()), this, SLOT(onApplyEnvironmentToRegion())));
		QToolTip::add(
			m_applyEnvironmentToRegionButton,
			QString::fromLatin1(
				"Adds a procedural layer (rectangle or circle boundary + environment affector) for the current region and selected family. Prompts for boundary feather."));
	}
	if (m_envZoneFinishButton)
		IGNORE_RETURN(connect(m_envZoneFinishButton, SIGNAL(clicked()), this, SLOT(onFinalizePolygonDraw())));
	if (m_envZoneCancelButton)
		IGNORE_RETURN(connect(m_envZoneCancelButton, SIGNAL(clicked()), this, SLOT(onCancelPolygonDraw())));
	if (m_regionPolyFinishButton)
		IGNORE_RETURN(connect(m_regionPolyFinishButton, SIGNAL(clicked()), this, SLOT(onFinalizePolygonDraw())));
	if (m_regionPolyCancelButton)
		IGNORE_RETURN(connect(m_regionPolyCancelButton, SIGNAL(clicked()), this, SLOT(onCancelPolygonDraw())));
	if (m_toolExcludeTerrain)
		IGNORE_RETURN(connect(m_toolExcludeTerrain, SIGNAL(clicked()), this, SLOT(onToolExcludeTerrain())));
	if (m_toolBoundaryPolygon)
		IGNORE_RETURN(connect(m_toolBoundaryPolygon, SIGNAL(clicked()), this, SLOT(onToolBoundaryPolygon())));
	if (m_toolBoundaryPolyline)
		IGNORE_RETURN(connect(m_toolBoundaryPolyline, SIGNAL(clicked()), this, SLOT(onToolBoundaryPolyline())));
	if (m_toolBoundaryPolyRoad)
		IGNORE_RETURN(connect(m_toolBoundaryPolyRoad, SIGNAL(clicked()), this, SLOT(onToolBoundaryPolyRoad())));

	installTerrainRegionTooltips(
		m_toolExcludeTerrain,
		m_toolBoundaryPolygon,
		m_toolBoundaryPolyline,
		m_toolBoundaryPolyRoad,
		m_regionPolygonCommitGroup,
		m_regionPolyCancelButton);

	installRegionOperationsTooltips(
		m_selectRegionButton,
		m_copyRegionButton,
		m_pasteRegionButton,
		m_fillRegionButton,
		m_saveRegionLayButton,
		m_loadRegionLayButton,
		m_importRegionLayAtCursorButton,
		m_regionShapeLabel,
		m_regionShapeCombo,
		m_mapParametersLabel);

	installMapTemplateEditorTooltips(
		m_mapTemplateHintLabel,
		m_mapTemplateSettingsButton,
		m_addProcHeightConstButton,
		m_addProcShaderConstButton,
		m_addProcExcludeRegionButton);
	
	// Connect bitmap stamp controls
	if (m_toolStampBitmap)
		IGNORE_RETURN(connect(m_toolStampBitmap, SIGNAL(clicked()), this, SLOT(onToolStampBitmap())));
	if (m_bitmapStampCombo)
		IGNORE_RETURN(connect(m_bitmapStampCombo, SIGNAL(activated(int)), this, SLOT(onBitmapStampSelected(int))));
	if (m_openBitmapFamilyEditorButton)
	{
		IGNORE_RETURN(connect(m_openBitmapFamilyEditorButton, SIGNAL(clicked()), this, SLOT(onOpenBitmapFamilyEditor())));
		QToolTip::add(
			m_openBitmapFamilyEditorButton,
			QString::fromLatin1("Rename bitmap stamp families and reload terrain/<basename>.tga height maps."));
	}
	if (m_bitmapRotationSlider)
		IGNORE_RETURN(connect(m_bitmapRotationSlider, SIGNAL(valueChanged(int)), this, SLOT(onBitmapRotationChanged(int))));
	if (m_bitmapScaleSlider)
		IGNORE_RETURN(connect(m_bitmapScaleSlider, SIGNAL(valueChanged(int)), this, SLOT(onBitmapScaleChanged(int))));
	if (m_bitmapAffectsHeightCheck)
		IGNORE_RETURN(connect(m_bitmapAffectsHeightCheck, SIGNAL(toggled(bool)), this, SLOT(onBitmapAffectsHeightToggled(bool))));
	if (m_bitmapAffectsShaderCheck)
		IGNORE_RETURN(connect(m_bitmapAffectsShaderCheck, SIGNAL(toggled(bool)), this, SLOT(onBitmapAffectsShaderToggled(bool))));

	if (m_scrollAreaContents)
		polishTerrainDockWideControls(m_scrollAreaContents);
	
	updateToolButtonStates();
	updateUndoRedoState();
	if (m_brushFeatherSlider)
		onBrushFeatherChanged(m_brushFeatherSlider->value());
	if (m_raiseLowerSpeedSlider)
		onRaiseLowerSpeedChanged(m_raiseLowerSpeedSlider->value());
	if (m_raiseLowerBiasSlider)
		onRaiseLowerBiasChanged(m_raiseLowerBiasSlider->value());
	if (m_raiseLowerClickSlider)
		onRaiseLowerClickRateChanged(m_raiseLowerClickSlider->value());
	if (m_raiseLowerJitterSlider)
		onRaiseLowerJitterChanged(m_raiseLowerJitterSlider->value());
	updateRegionGeometryUi();
	resizeQt3ScrollViewToContents(m_scrollAreaContents, m_contentScrollView);
}

// ----------------------------------------------------------------------

void TerrainDock::receiveMessage(const MessageDispatch::Emitter&, const MessageDispatch::MessageBase& message)
{
	if (message.isType(Game::Messages::SCENE_CHANGED))
	{
		m_terrainCacheValid = false;
		tryAutoSaveTerrainBeforeSceneChange();
		clearRegionGeometryAndSelection();
		IGNORE_RETURN(QTimer::singleShot(0, this, SLOT(onDeferredRefreshAfterSceneChange())));
		return;
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

void TerrainDock::resizeEvent(QResizeEvent* event)
{
	BaseTerrainDock::resizeEvent(event);
	resizeQt3ScrollViewToContents(m_scrollAreaContents, m_contentScrollView);
}

// ----------------------------------------------------------------------

void TerrainDock::setToolMode(ToolMode mode)
{
	if (GodClientTerrainEditor::isInstalled())
	{
		GodClientTerrainEditor& editor = GodClientTerrainEditor::getInstance();
		if (editor.isBrushStrokeActive())
			editor.endBrushStroke();

		if (editor.isPolygonDrawActive())
		{
			bool const polygonTool =
				mode == TM_PlaceEnvironment ||
				mode == TM_PlaceExcludeTerrain ||
				mode == TM_PlaceBoundaryPolygon;
			if (!polygonTool)
				editor.cancelPolygonDraw();
		}

		if (editor.isPolylineActive())
		{
			bool const polylineTool =
				mode == TM_PlaceRoad ||
				mode == TM_PlaceRibbon ||
				mode == TM_PlaceBoundaryPolyline ||
				mode == TM_PlaceBoundaryPolyRoad;
			if (!polylineTool)
				editor.cancelPolyline();
		}
	}

	m_toolMode = mode;
	updateToolButtonStates();

	syncGodClientEditorBrushSettings();
	updateRegionGeometryUi();
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
		case TM_PlaceRibbon: editorMode = GodClientTerrainEditor::TM_PlaceRibbon; break;
		case TM_PlaceRoad:   editorMode = GodClientTerrainEditor::TM_PlaceRoad; break;
		case TM_PlaceEnvironment: editorMode = GodClientTerrainEditor::TM_PlaceEnvironment; break;
		case TM_PlaceExcludeTerrain: editorMode = GodClientTerrainEditor::TM_PlaceExcludeTerrain; break;
		case TM_PlaceBoundaryPolygon: editorMode = GodClientTerrainEditor::TM_PlaceBoundaryPolygon; break;
		case TM_PlaceBoundaryPolyline: editorMode = GodClientTerrainEditor::TM_PlaceBoundaryPolyline; break;
		case TM_PlaceBoundaryPolyRoad: editorMode = GodClientTerrainEditor::TM_PlaceBoundaryPolyRoad; break;
		case TM_StampBitmap: editorMode = GodClientTerrainEditor::TM_StampBitmap; break;
		case TM_ApplyEnvironmentRegion: editorMode = GodClientTerrainEditor::TM_None; break;
		default:             editorMode = GodClientTerrainEditor::TM_None; break;
	}

	editor.setToolMode(editorMode);
	editor.setBrushSize(m_brushSize);
	editor.setBrushStrength(m_brushStrength);
	editor.setRaiseLowerSpeed(m_raiseLowerSpeed);
	editor.setRaiseLowerBias(m_raiseLowerBias);
	editor.setRaiseLowerClickRate(m_raiseLowerClickRate);
	editor.setRaiseLowerJitter(m_raiseLowerJitter);
	editor.setBrushShape(static_cast<GodClientTerrainEditor::BrushShape>(m_brushShape));
	editor.setFalloffType(static_cast<GodClientTerrainEditor::FalloffType>(m_falloffType));
	editor.setBrushFeather(m_brushFeather);
	editor.setTargetHeight(m_setHeightTarget);
	editor.setNoiseAmplitude(m_noiseAmplitude);
	editor.setNoiseFrequency(m_noiseFrequency);

	if (hasActiveTerrain() &&
		(editorMode == GodClientTerrainEditor::TM_PaintShader ||
		 editorMode == GodClientTerrainEditor::TM_StampBitmap))
	{
		QString const catalogPath = m_globalShaderPaintingSelection ? m_savedGlobalPickTrnCanon : QString();
		int const famToEnsure = m_selectedShaderFamilyId;
		if (!ensureLiveTerrainShaderFamilyForPaint(famToEnsure, catalogPath))
			clampSelectedShaderFamilyToLiveTerrain();
	}

	bool const tintOn = m_shaderColorConstantPaintEnabled && m_shaderColorConstantPickValid && !m_globalShaderPaintingSelection;
	editor.setShaderPaintTintMode(editorMode == GodClientTerrainEditor::TM_PaintShader && tintOn);
	if (editorMode == GodClientTerrainEditor::TM_PaintShader && tintOn)
		editor.setShaderPaintTintRgb(m_shaderColorConstantR, m_shaderColorConstantG, m_shaderColorConstantB);

	editor.setSelectedShaderFamily(m_selectedShaderFamilyId);
	{
		int floraFamilyId = 0;
		if (m_floraFamilyIndex >= 0 && m_floraFamilyIndex < static_cast<int>(m_floraFamilyIds.size()))
			floraFamilyId = m_floraFamilyIds[static_cast<size_t>(m_floraFamilyIndex)];
		editor.setSelectedFloraFamily(floraFamilyId);
		int radialFamilyId = 0;
		if (m_radialGroupIndex >= 0 && m_radialGroupIndex < static_cast<int>(m_radialFamilyIds.size()))
			radialFamilyId = m_radialFamilyIds[static_cast<size_t>(m_radialGroupIndex)];
		editor.setSelectedRadialFamily(radialFamilyId);
	}
	editor.setFloraCollidable(m_floraCollidable);
	editor.setFloraDensity(static_cast<float>(m_floraDensity) / 100.0f);
	editor.setBrushPreviewEnabled(m_showBrushPreview);
	editor.setBitmapShaderFamily(m_selectedShaderFamilyId);
	editor.setWaterPlacementHeight(m_waterHeight);
	std::string waterTpl("wter_ocean_water");
	if (m_waterShaderIndex >= 0 && m_waterShaderIndex < static_cast<int>(m_waterShaderTemplateNames.size()))
		waterTpl = m_waterShaderTemplateNames[static_cast<size_t>(m_waterShaderIndex)];
	editor.setWaterPlacementShaderTemplate(waterTpl.c_str());
	editor.setRibbonWaterShaderTemplate(waterTpl.c_str());

	syncRegionSelectionToEditor();
	updateShaderColorConstantControls();
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
	if (m_toolApplyEnvironmentRegion)
		m_toolApplyEnvironmentRegion->setOn(m_toolMode == TM_ApplyEnvironmentRegion);
	if (m_toolExcludeTerrain)
		m_toolExcludeTerrain->setOn(m_toolMode == TM_PlaceExcludeTerrain);
	if (m_toolBoundaryPolygon)
		m_toolBoundaryPolygon->setOn(m_toolMode == TM_PlaceBoundaryPolygon);
	if (m_toolBoundaryPolyline)
		m_toolBoundaryPolyline->setOn(m_toolMode == TM_PlaceBoundaryPolyline);
	if (m_toolBoundaryPolyRoad)
		m_toolBoundaryPolyRoad->setOn(m_toolMode == TM_PlaceBoundaryPolyRoad);
	if (m_toolStampBitmap)
		m_toolStampBitmap->setOn(m_toolMode == TM_StampBitmap);

	updateRaiseLowerTuneUi();
}

// ----------------------------------------------------------------------

void TerrainDock::updateRaiseLowerTuneUi()
{
	bool const enableRaiseLower = (m_toolMode == TM_Raise || m_toolMode == TM_Lower);
	if (m_raiseLowerTuneGroup)
		m_raiseLowerTuneGroup->setEnabled(enableRaiseLower);
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

void TerrainDock::onToolExcludeTerrain()
{
	if (m_toolMode == TM_PlaceExcludeTerrain)
	{
		setToolMode(TM_None);
		return;
	}
	if (GodClientTerrainEditor::isInstalled())
		GodClientTerrainEditor::getInstance().beginPolygonDraw(GodClientTerrainEditor::PDP_ExcludeTerrain);
	setToolMode(TM_PlaceExcludeTerrain);
}

void TerrainDock::onToolBoundaryPolygon()
{
	if (m_toolMode == TM_PlaceBoundaryPolygon)
	{
		setToolMode(TM_None);
		return;
	}
	if (GodClientTerrainEditor::isInstalled())
		GodClientTerrainEditor::getInstance().beginPolygonDraw(GodClientTerrainEditor::PDP_BoundaryPolygon);
	setToolMode(TM_PlaceBoundaryPolygon);
}

void TerrainDock::onToolBoundaryPolyline()
{
	setToolMode(m_toolMode == TM_PlaceBoundaryPolyline ? TM_None : TM_PlaceBoundaryPolyline);
	if (m_toolMode == TM_PlaceBoundaryPolyline && GodClientTerrainEditor::isInstalled())
	{
		GodClientTerrainEditor& ed = GodClientTerrainEditor::getInstance();
		ed.beginPolyline(false, GodClientTerrainEditor::PCK_BoundaryPolyline);
		ed.setPolylineWidth(std::max(2.f, static_cast<float>(m_polylineWidth)));
		ed.setPolylineFeatherDistance(m_polylineFeather);
	}
}

void TerrainDock::onToolBoundaryPolyRoad()
{
	setToolMode(m_toolMode == TM_PlaceBoundaryPolyRoad ? TM_None : TM_PlaceBoundaryPolyRoad);
	if (m_toolMode == TM_PlaceBoundaryPolyRoad && GodClientTerrainEditor::isInstalled())
	{
		GodClientTerrainEditor& ed = GodClientTerrainEditor::getInstance();
		ed.beginPolyline(false, GodClientTerrainEditor::PCK_BoundaryPolyRoad);
		ed.setPolylineWidth(std::max(8.f, static_cast<float>(m_polylineWidth)));
		ed.setPolylineFeatherDistance(m_polylineFeather);
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

// ----------------------------------------------------------------------

void TerrainDock::onToolApplyEnvironmentRegion()
{
	setToolMode(m_toolMode == TM_ApplyEnvironmentRegion ? TM_None : TM_ApplyEnvironmentRegion);
}

// ----------------------------------------------------------------------

void TerrainDock::onOpenEnvironmentEditor()
{
	if (!hasActiveTerrain())
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Environment", "Load procedural terrain first."));
		return;
	}
	EnvironmentEditorWindow::showSingleton(this);
}

// ----------------------------------------------------------------------

void TerrainDock::onOpenShaderFamilyEditor()
{
	if (!hasActiveTerrain())
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Shader families", "Load procedural terrain first."));
		return;
	}
	ShaderFamilyEditorWindow::showSingleton(this);
}

// ----------------------------------------------------------------------

void TerrainDock::onOpenFloraFamilyEditor()
{
	if (!hasActiveTerrain())
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Flora families", "Load procedural terrain first."));
		return;
	}
	FloraFamilyEditorWindow::showSingleton(this);
}

// ----------------------------------------------------------------------

void TerrainDock::onOpenRadialFamilyEditor()
{
	if (!hasActiveTerrain())
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Radial families", "Load procedural terrain first."));
		return;
	}
	RadialFamilyEditorWindow::showSingleton(this);
}

// ----------------------------------------------------------------------

void TerrainDock::onOpenBitmapFamilyEditor()
{
	if (!hasActiveTerrain())
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Bitmap families", "Load procedural terrain first."));
		return;
	}
	BitmapFamilyEditorWindow::showSingleton(this);
}

// ----------------------------------------------------------------------

void TerrainDock::onApplyEnvironmentToRegion()
{
	(void)tryApplyEnvironmentAffectorToCurrentRegion(true);
}

// ----------------------------------------------------------------------

void TerrainDock::updateEnvironmentAuthoringControls()
{
	bool const terrainOk = GodClientTerrainEditor::isInstalled() && hasActiveTerrain();
	if (m_openEnvironmentEditorButton)
		m_openEnvironmentEditorButton->setEnabled(terrainOk);
	if (m_openShaderFamilyEditorButton)
		m_openShaderFamilyEditorButton->setEnabled(terrainOk);
	if (m_openFloraFamilyEditorButton)
		m_openFloraFamilyEditorButton->setEnabled(terrainOk);
	if (m_openRadialFamilyEditorButton)
		m_openRadialFamilyEditorButton->setEnabled(terrainOk);
	if (m_openBitmapFamilyEditorButton)
		m_openBitmapFamilyEditorButton->setEnabled(terrainOk);

	bool const canApply = terrainOk && m_hasRegionSelection && !m_environmentFamilyIds.empty();
	if (m_applyEnvironmentToRegionButton)
		m_applyEnvironmentToRegionButton->setEnabled(canApply);
}

// ----------------------------------------------------------------------

bool TerrainDock::tryApplyEnvironmentAffectorToCurrentRegion(bool promptFeather)
{
	if (!GodClientTerrainEditor::isInstalled())
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Environment", "Terrain editor is not ready."));
		return false;
	}

	if (!m_hasRegionSelection)
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Environment", "Select a region first."));
		return false;
	}

	TerrainGenerator* const gen = GodClientTerrainEditor::getInstance().getTerrainGenerator();
	if (!gen)
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Environment", "No terrain generator."));
		return false;
	}

	int envFid = 0;
	int const eci = m_environmentFamilyCombo ? m_environmentFamilyCombo->currentItem() : 0;
	if (eci >= 0 && eci < static_cast<int>(m_environmentFamilyIds.size()))
		envFid = m_environmentFamilyIds[static_cast<size_t>(eci)];

	if (!gen->getEnvironmentGroup().hasFamily(envFid))
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Environment", "Choose a valid environment family."));
		return false;
	}

	float feather = 8.f;
	if (promptFeather)
	{
		bool ok = false;
		feather = static_cast<float>(QInputDialog::getDouble(
			tr("Environment"),
			tr("Boundary feather (meters):"),
			8.0,
			0.0,
			4096.0,
			2,
			&ok,
			this));
		if (!ok)
			return false;
	}

	syncGodClientEditorBrushSettings();
	if (!GodClientTerrainEditor::getInstance().addEnvironmentAffectorForCurrentRegionSelection(envFid, feather))
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Environment", "Could not add environment layer (region too small?)."));
		return false;
	}

	m_terrainModified = true;
	updateUndoRedoState();
	populateLayerList();
	MainFrame::getInstance().textToConsole("Added environment affector for current region.");
	return true;
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
	if (GodClientTerrainEditor::isInstalled())
		GodClientTerrainEditor::getInstance().setBrushSize(m_brushSize);
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
	if (GodClientTerrainEditor::isInstalled())
		GodClientTerrainEditor::getInstance().setBrushStrength(m_brushStrength);
}

void TerrainDock::onBrushShapeChanged(int index)
{
	m_brushShape = static_cast<BrushShape>(index);
	if (GodClientTerrainEditor::isInstalled())
		GodClientTerrainEditor::getInstance().setBrushShape(static_cast<GodClientTerrainEditor::BrushShape>(m_brushShape));
}

void TerrainDock::onFalloffTypeChanged(int index)
{
	m_falloffType = static_cast<FalloffType>(index);
	if (GodClientTerrainEditor::isInstalled())
		GodClientTerrainEditor::getInstance().setFalloffType(static_cast<GodClientTerrainEditor::FalloffType>(m_falloffType));
}

// ----------------------------------------------------------------------

void TerrainDock::onBrushFeatherChanged(int value)
{
	m_brushFeather = std::max(0.0f, std::min(1.0f, static_cast<float>(value) / 100.0f));
	if (m_brushFeatherValue)
	{
		QString t = (value <= 0) ? QString("None") : QString::number(value) + '%';
		m_brushFeatherValue->setText(t);
	}
	if (GodClientTerrainEditor::isInstalled())
		GodClientTerrainEditor::getInstance().setBrushFeather(m_brushFeather);
}

// ----------------------------------------------------------------------

void TerrainDock::onRaiseLowerSpeedChanged(int value)
{
	m_raiseLowerSpeed = raiseLowerSpeedFromSlider(value);
	if (m_raiseLowerSpeedValue)
	{
		QString t;
		t.sprintf("%.2fm", static_cast<double>(m_raiseLowerSpeed));
		m_raiseLowerSpeedValue->setText(t);
	}
	if (GodClientTerrainEditor::isInstalled())
		GodClientTerrainEditor::getInstance().setRaiseLowerSpeed(m_raiseLowerSpeed);
}

// ----------------------------------------------------------------------

void TerrainDock::onRaiseLowerBiasChanged(int value)
{
	m_raiseLowerBias = static_cast<float>(value) / 100.f;
	if (m_raiseLowerBiasValue)
	{
		if (value <= -15)
			m_raiseLowerBiasValue->setText(QString::fromLatin1("Valleys"));
		else if (value >= 15)
			m_raiseLowerBiasValue->setText(QString::fromLatin1("Peaks"));
		else
			m_raiseLowerBiasValue->setText(QString::fromLatin1("Center"));
	}
	if (GodClientTerrainEditor::isInstalled())
		GodClientTerrainEditor::getInstance().setRaiseLowerBias(m_raiseLowerBias);
}

// ----------------------------------------------------------------------

void TerrainDock::onRaiseLowerClickRateChanged(int value)
{
	m_raiseLowerClickRate = static_cast<float>(value) / 100.f;
	if (m_raiseLowerClickValue)
	{
		QString t;
		t.sprintf("%d%%", value);
		m_raiseLowerClickValue->setText(t);
	}
	if (GodClientTerrainEditor::isInstalled())
		GodClientTerrainEditor::getInstance().setRaiseLowerClickRate(m_raiseLowerClickRate);
}

// ----------------------------------------------------------------------

void TerrainDock::onRaiseLowerJitterChanged(int value)
{
	m_raiseLowerJitter = static_cast<float>(value) / 100.f;
	if (m_raiseLowerJitterValue)
	{
		QString t;
		t.sprintf("%d%%", value);
		m_raiseLowerJitterValue->setText(t);
	}
	if (GodClientTerrainEditor::isInstalled())
		GodClientTerrainEditor::getInstance().setRaiseLowerJitter(m_raiseLowerJitter);
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
	onLayerRename();
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
	
	if (!getTerrainTemplate())
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Save Error", "No terrain loaded to save."));
		return;
	}
	
	if (writeCurrentTerrainTemplateToFile(m_terrainFilePath, true))
		MainFrame::getInstance().textToConsole("Terrain saved successfully.");
	else
		IGNORE_RETURN(QMessageBox::critical(this, "Save Error", "Failed to write terrain file."));
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

bool TerrainDock::writeCurrentTerrainTemplateToFile(std::string const& path, bool const clearModifiedOnSuccess)
{
	if (path.empty())
		return false;

	ProceduralTerrainAppearanceTemplate* const terrainTemplate = getTerrainTemplate();
	if (!terrainTemplate)
		return false;

	if (GodClientTerrainEditor::isInstalled())
		GodClientTerrainEditor::getInstance().flushTerrainChanges();

	if (TerrainGenerator* const gen = getTerrainGenerator())
		gen->prepare();

	Iff iff(1024 * 1024);
	ProceduralTerrainAppearanceTemplate::WriterData writerData;
	terrainTemplate->prepareWriterData(writerData);
	ProceduralTerrainAppearanceTemplate::write(iff, writerData);

	if (!iff.write(path.c_str(), true))
		return false;

	if (clearModifiedOnSuccess)
		m_terrainModified = false;

	char const * const templateTreeName = terrainTemplate->getName();
	if (templateTreeName && templateTreeName[0])
		terrainDockTryPushTerrainFileToLiveServer(path, templateTreeName);

	return true;
}

void TerrainDock::tryAutoSaveTerrainBeforeSceneChange()
{
	if (!m_terrainModified)
		return;

	if (Game::getScene() != 0)
		return;

	if (!getTerrainTemplate())
		return;

	std::string path = m_terrainFilePath;
	if (path.empty())
	{
		char const* const templateName = getTerrainTemplate()->getName();
		if (templateName && templateName[0])
			path = templateName;
	}

	if (path.empty())
	{
		MainFrame::getInstance().textToConsole("Terrain: unsaved edits were not auto-saved (no .trn path). Use Save As.");
		return;
	}

	if (writeCurrentTerrainTemplateToFile(path, true))
	{
		m_terrainFilePath = path;
		if (m_terrainFileLabel)
			m_terrainFileLabel->setText(QString::fromLatin1(path.c_str()));
		MainFrame::getInstance().textToConsole("Terrain auto-saved before scene change.");
		updateUndoRedoState();
	}
	else
		MainFrame::getInstance().textToConsole("Terrain auto-save failed before scene change (disk write error).");
}

void TerrainDock::onDeferredRefreshAfterSceneChange()
{
	refreshFromScene(true);
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
		if (GodClientTerrainEditor::isInstalled())
			syncGodClientEditorBrushSettings();
	}
}

void TerrainDock::onWaterShaderChanged(int index)
{
	m_waterShaderIndex = index;
	syncGodClientEditorBrushSettings();
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
	syncGodClientEditorBrushSettings();
}

void TerrainDock::onFloraPlacementModeChanged(int index)
{
	UNREF(index);
}

void TerrainDock::onRadialGroupChanged(int index)
{
	m_radialGroupIndex = index;
	syncGodClientEditorBrushSettings();
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
		m_layerListGeneratorIndices.clear();
		if (m_shaderList)
			m_shaderList->clear();

		if (!skipGlobalShaderCatalogScan)
			syncGlobalShaderCatalog();
		syncGodClientEditorBrushSettings();
		updateMapParametersPanel();
		syncMapTemplateEditorWidgetsFromScene();
		updateRegionGeometryUi();

		resizeQt3ScrollViewToContents(m_scrollAreaContents, m_contentScrollView);
		return;
	}
	
	const char* name = terrainTemplate->getName();
	if (name)
	{
		if (!m_terrainFilePath.empty())
		{
			QString const oldCanon(terrainDockCanonFromStdTerrainPath(m_terrainFilePath));
			QString const incomingCanon(terrainDockCanonFromPathQString(QString::fromLatin1(name)));
			if (!oldCanon.isEmpty() && !incomingCanon.isEmpty() && oldCanon != incomingCanon)
				m_terrainModified = false;
		}

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

	syncGodClientEditorBrushSettings();
	updateMapParametersPanel();
	syncMapTemplateEditorWidgetsFromScene();
	updateRegionGeometryUi();

	resizeQt3ScrollViewToContents(m_scrollAreaContents, m_contentScrollView);
}

// ----------------------------------------------------------------------

void TerrainDock::syncMapTemplateEditorWidgetsFromScene()
{
	if (!m_mapTemplateSettingsButton)
		return;

	ProceduralTerrainAppearanceTemplate* const tpl = getTerrainTemplate();
	if (!tpl)
	{
		if (m_mapTemplateEditorGroup)
			m_mapTemplateEditorGroup->setEnabled(false);
		if (m_mapTemplateSettingsButton)
			m_mapTemplateSettingsButton->setEnabled(false);
		return;
	}

	if (m_mapTemplateEditorGroup)
		m_mapTemplateEditorGroup->setEnabled(true);
	if (m_mapTemplateSettingsButton)
		m_mapTemplateSettingsButton->setEnabled(true);
}

// ----------------------------------------------------------------------

void TerrainDock::updateMapParametersPanel()
{
	if (!m_mapParametersLabel)
		return;

	if (!hasActiveTerrain())
	{
		m_mapParametersLabel->setText(QString::fromLatin1("Map: (no procedural terrain)"));
		return;
	}

	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (!terrainObject)
	{
		m_mapParametersLabel->setText(QString::fromLatin1("Map: (no terrain object)"));
		return;
	}

	Appearance* const appearance = terrainObject->getAppearance();
	ClientProceduralTerrainAppearance* const cpta = dynamic_cast<ClientProceduralTerrainAppearance*>(appearance);
	if (!cpta)
	{
		m_mapParametersLabel->setText(QString::fromLatin1("Map: (non-procedural appearance)"));
		return;
	}

	float const mapW = cpta->getMapWidthInMeters();
	float const chunkW = cpta->getChunkWidthInMeters();
	int const tilesPerChunk = std::max(1, cpta->getNumberOfTilesPerChunk());
	float const tileW = chunkW / static_cast<float>(tilesPerChunk);
	int const chunks = cpta->getNumberOfChunks();

	QString msg;
	msg.sprintf(
		"%.0fm map | %.0fm chunk | %d tiles (%.1fm) | %d chks",
		mapW, chunkW, tilesPerChunk, tileW, chunks);
	m_mapParametersLabel->setText(msg);
}

// ----------------------------------------------------------------------

void TerrainDock::updateRegionGeometryUi()
{
	if (!m_mapParametersLabel)
	{
		emit terrainGameWindowStatusChanged();
		return;
	}

	bool const showClosedCommit =
		(m_toolMode == TM_PlaceExcludeTerrain || m_toolMode == TM_PlaceBoundaryPolygon);

	if (m_regionPolygonCommitGroup)
	{
		if (showClosedCommit)
			m_regionPolygonCommitGroup->show();
		else
			m_regionPolygonCommitGroup->hide();
	}

	int n = 0;
	if (GodClientTerrainEditor::isInstalled() && showClosedCommit)
		n = GodClientTerrainEditor::getInstance().getPolygonBoundaryPointCount();

	if (m_regionPolyFinishButton)
	{
		m_regionPolyFinishButton->setEnabled(showClosedCommit && n >= 3);
		if (showClosedCommit)
		{
			char tip[192];
			snprintf(tip, sizeof(tip),
				"%d corner(s). Create needs 3+ corners; interior tiles skip mesh (exclude) or use mask layering.",
				n);
			QToolTip::add(m_regionPolyFinishButton, QString::fromLatin1(tip));
		}
	}
	if (m_regionPolyCancelButton)
		m_regionPolyCancelButton->setEnabled(showClosedCommit);

	updateEnvironmentAuthoringControls();

	emit terrainGameWindowStatusChanged();
}

// ----------------------------------------------------------------------

bool TerrainDock::shouldShowTerrainGameWindowStatus() const
{
	return hasActiveTerrain() && (m_toolMode != TM_None || m_hasRegionSelection);
}

// ----------------------------------------------------------------------

bool TerrainDock::hasRegionToolOrSelectionActive() const
{
	if (m_hasRegionSelection)
		return true;
	switch (m_toolMode)
	{
	case TM_PlaceExcludeTerrain:
	case TM_PlaceBoundaryPolygon:
	case TM_PlaceBoundaryPolyline:
	case TM_PlaceBoundaryPolyRoad:
	case TM_ApplyEnvironmentRegion:
	case TM_Select:
		return true;
	default:
		return false;
	}
}

// ----------------------------------------------------------------------

QString TerrainDock::terrainGameWindowStatusText() const
{
	if (!hasActiveTerrain())
		return QString::fromLatin1("(no terrain)");

	QString toolLine;

	switch (m_toolMode)
	{
	case TM_None:
		toolLine = QString::fromLatin1("Idle");
		break;
	case TM_Raise:
		toolLine = QString::fromLatin1("Raise");
		break;
	case TM_Lower:
		toolLine = QString::fromLatin1("Lower");
		break;
	case TM_Flatten:
		toolLine = QString::fromLatin1("Flatten");
		break;
	case TM_Smooth:
		toolLine = QString::fromLatin1("Smooth");
		break;
	case TM_Noise:
		toolLine = QString::fromLatin1("Noise");
		break;
	case TM_SetHeight:
		toolLine = QString::fromLatin1("Set H");
		break;
	case TM_PaintShader:
		toolLine = QString::fromLatin1("Shader");
		break;
	case TM_PaintFlora:
		toolLine = QString::fromLatin1("Flora");
		break;
	case TM_PlaceWater:
		toolLine = QString::fromLatin1("Water");
		break;
	case TM_PlaceRadial:
		toolLine = QString::fromLatin1("Radial");
		break;
	case TM_PlaceRibbon:
		toolLine = QString::fromLatin1("Ribbon");
		break;
	case TM_PlaceRoad:
		toolLine = QString::fromLatin1("Road");
		break;
	case TM_PlaceEnvironment:
		toolLine = QString::fromLatin1("Env zone");
		break;
	case TM_PlaceExcludeTerrain:
		toolLine = QString::fromLatin1("Exclude");
		break;
	case TM_PlaceBoundaryPolygon:
		toolLine = QString::fromLatin1("Mask");
		break;
	case TM_PlaceBoundaryPolyline:
		toolLine = QString::fromLatin1("Path");
		break;
	case TM_PlaceBoundaryPolyRoad:
		toolLine = QString::fromLatin1("Corridor");
		break;
	case TM_StampBitmap:
		toolLine = QString::fromLatin1("Stamp");
		break;
	case TM_ApplyEnvironmentRegion:
		toolLine = QString::fromLatin1("Env+region");
		break;
	case TM_Select:
		toolLine = QString::fromLatin1("Select");
		break;
	default:
		toolLine = QString::fromLatin1("?");
		break;
	}

	QString extra;
	if (GodClientTerrainEditor::isInstalled())
	{
		GodClientTerrainEditor& ed = GodClientTerrainEditor::getInstance();
		if (m_toolMode == TM_PlaceExcludeTerrain || m_toolMode == TM_PlaceBoundaryPolygon)
		{
			int const c = ed.getPolygonBoundaryPointCount();
			QString cornerNote;
			cornerNote.sprintf(" %dc", c);
			extra += cornerNote;
		}
		else if (
			m_toolMode == TM_PlaceRibbon ||
			m_toolMode == TM_PlaceRoad ||
			m_toolMode == TM_PlaceBoundaryPolyline ||
			m_toolMode == TM_PlaceBoundaryPolyRoad)
		{
			int const c = ed.getPolylinePointCount();
			QString polyNote;
			polyNote.sprintf(" %dp", c);
			extra += polyNote;
		}
	}

	QString regionNote;
	if (m_hasRegionSelection)
		regionNote = QString::fromLatin1(" Sel");

	return toolLine + extra + regionNote;
}

// ----------------------------------------------------------------------

void TerrainDock::clearRegionGeometryAndSelection()
{
	m_regionDragActive = false;

	bool const hadRegionTool =
		m_toolMode == TM_PlaceExcludeTerrain ||
		m_toolMode == TM_PlaceBoundaryPolygon ||
		m_toolMode == TM_PlaceBoundaryPolyline ||
		m_toolMode == TM_PlaceBoundaryPolyRoad ||
		m_toolMode == TM_Select;

	if (m_hasRegionSelection)
	{
		m_hasRegionSelection = false;
		syncRegionSelectionToEditor();
	}

	if (hadRegionTool)
		setToolMode(TM_None);
	else
	{
		updateToolButtonStates();
		updateRegionGeometryUi();
	}

	MainFrame::getInstance().textToConsole("Region tools / world region selection cleared.");
}

// ======================================================================
// List Population Helpers
// ======================================================================

void TerrainDock::populateLayerList()
{
	if (!m_layerList)
		return;

	QString preservedLayerName;
	if (m_layerList->selectedItem())
		preservedLayerName = m_layerList->selectedItem()->text(0);

	m_layerList->clear();
	m_layerListGeneratorIndices.clear();
	
	TerrainGenerator* generator = getTerrainGenerator();
	if (!generator)
		return;
	
	const int numLayers = generator->getNumberOfLayers();
	QListViewItem* restoredSelection = 0;

	for (int i = 0; i < numLayers; ++i)
	{
		const TerrainGenerator::Layer* layer = generator->getLayer(i);
		if (layer)
		{
			m_layerListGeneratorIndices.push_back(i);
			const char* layerName = layer->getName();
			QString name = layerName ? layerName : QString("Layer %1").arg(i);
			QString type = "Layer";
			QString active = layer->isActive() ? "Yes" : "No";
			
			QListViewItem* const rowItem = new QListViewItem(m_layerList, name, type, active);

			if (!preservedLayerName.isEmpty() && preservedLayerName == name)
				restoredSelection = rowItem;
		}
	}

	if (restoredSelection)
	{
		m_layerList->setSelected(restoredSelection, true);
	}
}

// ----------------------------------------------------------------------

int TerrainDock::selectedLayerListIndex() const
{
	if (!m_layerList)
		return -1;

	QListViewItem* const sel = m_layerList->selectedItem();
	if (!sel)
		return -1;

	int idx = 0;
	for (QListViewItem* it = m_layerList->firstChild(); it; it = it->nextSibling(), ++idx)
	{
		if (it == sel)
			return idx;
	}

	return -1;
}

// ----------------------------------------------------------------------

int TerrainDock::selectedTerrainGeneratorLayerIndex() const
{
	const int row = selectedLayerListIndex();
	if (row < 0)
		return -1;
	if (row >= static_cast<int>(m_layerListGeneratorIndices.size()))
		return -1;
	return m_layerListGeneratorIndices[static_cast<size_t>(row)];
}

// ----------------------------------------------------------------------

void TerrainDock::syncRegionSelectionToEditor()
{
	if (!GodClientTerrainEditor::isInstalled())
		return;

	if (!m_hasRegionSelection)
	{
		GodClientTerrainEditor::getInstance().clearRegionSelection();
		return;
	}

	bool const circ = (m_regionSelectionShape == RSS_Circle);
	GodClientTerrainEditor::getInstance().setRegionSelection(
		m_regionMinX,
		m_regionMinZ,
		m_regionMaxX,
		m_regionMaxZ,
		circ,
		m_regionCircleCenterX,
		m_regionCircleCenterZ,
		m_regionCircleRadius);
}

// ----------------------------------------------------------------------

void TerrainDock::terrainGeneratorLiveCommit()
{
	TerrainGenerator* const gen = getTerrainGenerator();
	if (!gen)
		return;

	gen->prepare();

	if (ClientProceduralTerrainAppearance* const app = getClientTerrain())
		app->rebuildLocalWaterTablesFromTerrainGenerator();

	if (TerrainObject* const to = TerrainObject::getInstance())
	{
		static float const kHuge = 25000.f;
		Rectangle2d const huge(-kHuge, -kHuge, kHuge, kHuge);
		to->invalidateRegion(huge);
	}

	populateLayerList();
}

// ----------------------------------------------------------------------

void TerrainDock::markLiveTerrainModified()
{
	m_terrainModified = true;
}

// ----------------------------------------------------------------------

void TerrainDock::onRegionShapeChanged(int index)
{
	RegionSelectionShape const newShape = (index == 1) ? RSS_Circle : RSS_Rectangle;
	if (newShape == m_regionSelectionShape)
		return;

	m_regionSelectionShape = newShape;

	if (m_hasRegionSelection)
	{
		if (m_regionSelectionShape == RSS_Circle)
		{
			float const w = m_regionMaxX - m_regionMinX;
			float const h = m_regionMaxZ - m_regionMinZ;
			m_regionCircleCenterX = 0.5f * (m_regionMinX + m_regionMaxX);
			m_regionCircleCenterZ = 0.5f * (m_regionMinZ + m_regionMaxZ);
			m_regionCircleRadius = 0.5f * std::min(w, h);
			if (m_regionCircleRadius < 0.5f)
				m_regionCircleRadius = 0.5f;
			m_regionMinX = m_regionCircleCenterX - m_regionCircleRadius;
			m_regionMaxX = m_regionCircleCenterX + m_regionCircleRadius;
			m_regionMinZ = m_regionCircleCenterZ - m_regionCircleRadius;
			m_regionMaxZ = m_regionCircleCenterZ + m_regionCircleRadius;
		}

		syncRegionSelectionToEditor();
	}
}

// ----------------------------------------------------------------------

void TerrainDock::onLayerToggleActive()
{
	TerrainGenerator* const gen = getTerrainGenerator();
	if (!gen)
		return;

	int const li = selectedTerrainGeneratorLayerIndex();
	if (li < 0)
		return;

	TerrainGenerator::Layer* const layer = gen->getLayer(li);
	if (!layer)
		return;

	layer->setActive(!layer->isActive());
	terrainGeneratorLiveCommit();
	m_terrainModified = true;
}

// ----------------------------------------------------------------------

void TerrainDock::onLayerPromote()
{
	TerrainGenerator* const gen = getTerrainGenerator();
	if (!gen)
		return;

	int const li = selectedTerrainGeneratorLayerIndex();
	if (li < 0)
		return;

	TerrainGenerator::Layer* const layer = gen->getLayer(li);
	if (!layer)
		return;

	gen->promoteLayer(layer);
	terrainGeneratorLiveCommit();
	m_terrainModified = true;
}

// ----------------------------------------------------------------------

void TerrainDock::onLayerDemote()
{
	TerrainGenerator* const gen = getTerrainGenerator();
	if (!gen)
		return;

	int const li = selectedTerrainGeneratorLayerIndex();
	if (li < 0)
		return;

	TerrainGenerator::Layer* const layer = gen->getLayer(li);
	if (!layer)
		return;

	gen->demoteLayer(layer);
	terrainGeneratorLiveCommit();
	m_terrainModified = true;
}

// ----------------------------------------------------------------------

void TerrainDock::onLayerRename()
{
	TerrainGenerator* const gen = getTerrainGenerator();
	if (!gen)
		return;

	int const li = selectedTerrainGeneratorLayerIndex();
	if (li < 0)
		return;

	TerrainGenerator::Layer* const layer = gen->getLayer(li);
	if (!layer)
		return;

	char const* cur = layer->getName();
	QString const qcur = cur ? QString::fromLatin1(cur) : QString("Layer");

	bool ok = false;
	QString const qnew = QInputDialog::getText(
		"Rename terrain layer",
		"Layer name:",
		QLineEdit::Normal,
		qcur,
		&ok,
		this);

	if (!ok)
		return;

	if (qnew.isEmpty())
		return;

	layer->setName(qnew.latin1());
	terrainGeneratorLiveCommit();
	m_terrainModified = true;
}

// ----------------------------------------------------------------------

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

			QString const canonPath(terrainDockCanonFromPathQString(it->text(2)));
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
	m_savedGlobalPickTrnCanon = terrainDockCanonFromPathQString(item->text(2));

	if (hasActiveTerrain())
	{
		if (!ensureLiveTerrainShaderFamilyForPaint(m_selectedShaderFamilyId, m_savedGlobalPickTrnCanon))
		{
			MainFrame::getInstance().textToConsole(
				"Global shader pick could not be merged into the live terrain (missing .trn, unreadable IFF, or invalid family).");
			m_globalShaderPaintingSelection = false;
			m_savedGlobalPickFamilyId = 0;
			m_savedGlobalPickTrnCanon = QString();
			clampSelectedShaderFamilyToLiveTerrain();
		}
	}

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

	QString const srcPath(sel->text(2));
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
	m_floraFamilyIds.clear();
	
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
		m_floraFamilyIds.push_back(familyId);
	}
	if (!m_floraFamilyIds.empty())
	{
		if (m_floraFamilyIndex >= static_cast<int>(m_floraFamilyIds.size()))
			m_floraFamilyIndex = 0;
	}
	else
		m_floraFamilyIndex = 0;
}

void TerrainDock::populateRadialList()
{
	if (!m_radialGroupCombo)
		return;
	
	m_radialGroupCombo->clear();
	m_radialFamilyIds.clear();
	
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
		m_radialFamilyIds.push_back(familyId);
	}
	if (!m_radialFamilyIds.empty())
	{
		if (m_radialGroupIndex >= static_cast<int>(m_radialFamilyIds.size()))
			m_radialGroupIndex = 0;
	}
	else
		m_radialGroupIndex = 0;
}

void TerrainDock::populateWaterShaderList()
{
	if (!m_waterShaderCombo)
		return;
	
	m_waterShaderCombo->clear();
	m_waterShaderTemplateNames.clear();

	std::set<std::string> seen;
	std::vector<std::pair<QString, std::string> > rows;
	rows.reserve(256);

	int scanBudget = 384;
	terrainDockGatherWaterShaderTemplatesFromDisk(rows, seen, scanBudget);

	static struct WaterPresetSpec
	{
		char const* label;
		char const* shaderTemplate;
	}
	const presets[] =
	{
		{ "ocean_new (legacy)", "ocean_new" },
		{ "default_water", "default_water"},
		{ "ocean_water", "ocean_water"},
		{ "swamp_water", "swamp_water"},
		{ "river_water", "river_water"},
		{ "lake_water", "lake_water"}
	};

	for (size_t i = 0; i < sizeof(presets) / sizeof(presets[0]); ++i)
	{
		std::string const t(presets[i].shaderTemplate);
		if (!seen.insert(t).second)
			continue;
		rows.push_back(std::make_pair(QString::fromLatin1(presets[i].label), t));
	}

	for (size_t i = 0; i < rows.size(); ++i)
	{
		m_waterShaderCombo->insertItem(rows[i].first);
		m_waterShaderTemplateNames.push_back(rows[i].second);
	}

	if (m_waterShaderIndex >= static_cast<int>(m_waterShaderTemplateNames.size()))
		m_waterShaderIndex = 0;
	if (m_waterShaderCombo->count() > 0)
		m_waterShaderCombo->setCurrentItem(m_waterShaderIndex);
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
	{
		updateEnvironmentAuthoringControls();
		return;
	}

	m_environmentFamilyCombo->clear();

	TerrainGenerator* const generator = getTerrainGenerator();
	if (!generator)
	{
		updateEnvironmentAuthoringControls();
		return;
	}

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

	updateEnvironmentAuthoringControls();
}

// ----------------------------------------------------------------------

void TerrainDock::refreshEnvironmentFamilyComboFromGenerator()
{
	populateEnvironmentFamilyCombo();
}

// ----------------------------------------------------------------------

void TerrainDock::refreshShaderGroupUiFromGenerator()
{
	populateShaderList(false);
}

// ----------------------------------------------------------------------

void TerrainDock::refreshFloraFamilyComboFromGenerator()
{
	int prevFamilyId = -1;
	if (!m_floraFamilyIds.empty() &&
		m_floraFamilyIndex >= 0 &&
		m_floraFamilyIndex < static_cast<int>(m_floraFamilyIds.size()))
	{
		prevFamilyId = m_floraFamilyIds[static_cast<size_t>(m_floraFamilyIndex)];
	}

	populateFloraList();

	if (m_floraFamilyCombo && !m_floraFamilyIds.empty())
	{
		int newIndex = 0;
		if (prevFamilyId >= 0)
		{
			for (size_t i = 0; i < m_floraFamilyIds.size(); ++i)
			{
				if (m_floraFamilyIds[i] == prevFamilyId)
				{
					newIndex = static_cast<int>(i);
					break;
				}
			}
		}
		m_floraFamilyCombo->setCurrentItem(newIndex);
		onFloraFamilyChanged(newIndex);
	}
}

// ----------------------------------------------------------------------

void TerrainDock::refreshRadialFamilyComboFromGenerator()
{
	int prevFamilyId = -1;
	if (!m_radialFamilyIds.empty() &&
		m_radialGroupIndex >= 0 &&
		m_radialGroupIndex < static_cast<int>(m_radialFamilyIds.size()))
	{
		prevFamilyId = m_radialFamilyIds[static_cast<size_t>(m_radialGroupIndex)];
	}

	populateRadialList();

	if (m_radialGroupCombo && !m_radialFamilyIds.empty())
	{
		int newIndex = 0;
		if (prevFamilyId >= 0)
		{
			for (size_t i = 0; i < m_radialFamilyIds.size(); ++i)
			{
				if (m_radialFamilyIds[i] == prevFamilyId)
				{
					newIndex = static_cast<int>(i);
					break;
				}
			}
		}
		m_radialGroupCombo->setCurrentItem(newIndex);
		onRadialGroupChanged(newIndex);
	}
}

// ----------------------------------------------------------------------

void TerrainDock::refreshBitmapStampComboFromGenerator()
{
	populateBitmapStampCombo();
}

// ----------------------------------------------------------------------

void TerrainDock::reloadBitmapStampPreviewIfCurrentFamily(int familyId)
{
	if (!GodClientTerrainEditor::isInstalled())
		return;
	if (m_bitmapStampFamilyIds.empty())
		return;
	if (m_bitmapStampIndex < 0 || m_bitmapStampIndex >= static_cast<int>(m_bitmapStampFamilyIds.size()))
		return;
	if (m_bitmapStampFamilyIds[static_cast<size_t>(m_bitmapStampIndex)] != familyId)
		return;

	GodClientTerrainEditor::getInstance().reloadBitmapStampFromTerrainFamily(familyId);
	GodClientTerrainEditor::getInstance().setBitmapShaderFamily(m_selectedShaderFamilyId);
}

// ----------------------------------------------------------------------

void TerrainDock::populateBitmapStampCombo()
{
	int prevFamilyId = -1;
	if (!m_bitmapStampFamilyIds.empty() &&
		m_bitmapStampIndex >= 0 &&
		m_bitmapStampIndex < static_cast<int>(m_bitmapStampFamilyIds.size()))
	{
		prevFamilyId = m_bitmapStampFamilyIds[static_cast<size_t>(m_bitmapStampIndex)];
	}

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
		int newIndex = 0;
		if (prevFamilyId >= 0)
		{
			for (size_t i = 0; i < m_bitmapStampFamilyIds.size(); ++i)
			{
				if (m_bitmapStampFamilyIds[i] == prevFamilyId)
				{
					newIndex = static_cast<int>(i);
					break;
				}
			}
		}

		m_bitmapStampCombo->setCurrentItem(newIndex);
		m_bitmapStampIndex = newIndex;
		GodClientTerrainEditor::getInstance().reloadBitmapStampFromTerrainFamily(m_bitmapStampFamilyIds[static_cast<size_t>(newIndex)]);
		GodClientTerrainEditor::getInstance().setBitmapShaderFamily(m_selectedShaderFamilyId);
	}
}

// ======================================================================
// Region Operation Slots
// ======================================================================

namespace
{
	char const g_layMagic[8] = { 'S', 'W', 'G', 'O', 'D', 'L', 'A', 'Y' };
	unsigned int const g_layVersion = 1;
	long long const g_layMaxCells = 8LL * 1024 * 1024;

	bool writeU32le(QFile& f, unsigned int v)
	{
		unsigned char b[4];
		b[0] = static_cast<unsigned char>(v & 255u);
		b[1] = static_cast<unsigned char>((v >> 8) & 255u);
		b[2] = static_cast<unsigned char>((v >> 16) & 255u);
		b[3] = static_cast<unsigned char>((v >> 24) & 255u);
		return f.writeBlock(reinterpret_cast<char*>(b), 4) == 4;
	}

	bool readU32le(QFile& f, unsigned int& v)
	{
		unsigned char b[4];
		if (f.readBlock(reinterpret_cast<char*>(b), 4) != 4)
			return false;
		v = b[0] | (b[1] << 8u) | (b[2] << 16u) | (b[3] << 24u);
		return true;
	}

	bool writeS32le(QFile& f, int v)
	{
		return writeU32le(f, static_cast<unsigned int>(v));
	}

	bool readS32le(QFile& f, int& v)
	{
		unsigned int u = 0;
		if (!readU32le(f, u))
			return false;
		v = static_cast<int>(u);
		return true;
	}

	bool writeF32le(QFile& f, float value)
	{
		unsigned int u = 0;
		memcpy(&u, &value, sizeof(float));
		return writeU32le(f, u);
	}

	bool readF32le(QFile& f, float& out)
	{
		unsigned int u = 0;
		if (!readU32le(f, u))
			return false;
		memcpy(&out, &u, sizeof(float));
		return true;
	}
}

bool TerrainDock::hasTerrainWorldRegionSelection() const
{
	return m_hasRegionSelection;
}

bool TerrainDock::terrainCopyWorldRegionIntoClipboard(bool postConsoleMessageOnSuccess)
{
	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (!terrainObject || !m_hasRegionSelection)
		return false;

	float const rMinX = std::min(m_regionMinX, m_regionMaxX);
	float const rMaxX = std::max(m_regionMinX, m_regionMaxX);
	float const rMinZ = std::min(m_regionMinZ, m_regionMaxZ);
	float const rMaxZ = std::max(m_regionMinZ, m_regionMaxZ);

	float const eps = 1e-4f;
	int const gridX0 = static_cast<int>(std::floor(rMinX + eps));
	int const gridZ0 = static_cast<int>(std::floor(rMinZ + eps));
	int const gridX1 = static_cast<int>(std::floor(rMaxX + eps));
	int const gridZ1 = static_cast<int>(std::floor(rMaxZ + eps));

	int const widthSamples = gridX1 - gridX0 + 1;
	int const heightSamples = gridZ1 - gridZ0 + 1;
	if (widthSamples < 1 || heightSamples < 1)
		return false;

	m_regionClipboard.gridX0 = gridX0;
	m_regionClipboard.gridZ0 = gridZ0;
	m_regionClipboard.sourceMinX = static_cast<float>(gridX0);
	m_regionClipboard.sourceMinZ = static_cast<float>(gridZ0);
	m_regionClipboard.sourceMaxX = static_cast<float>(gridX0 + widthSamples - 1);
	m_regionClipboard.sourceMaxZ = static_cast<float>(gridZ0 + heightSamples - 1);
	m_regionClipboard.widthSamples = widthSamples;
	m_regionClipboard.heightSamples = heightSamples;
	m_regionClipboard.heightData.clear();
	m_regionClipboard.shaderData.clear();
	m_regionClipboard.cellMask.clear();
	m_regionClipboard.hasCellMask = false;
	m_regionClipboard.heightData.reserve(static_cast<size_t>(widthSamples * heightSamples));
	m_regionClipboard.shaderData.reserve(static_cast<size_t>(widthSamples * heightSamples));

	bool const useCircleMask = (m_regionSelectionShape == RSS_Circle && m_regionCircleRadius > 0.01f);
	float const r2 = m_regionCircleRadius * m_regionCircleRadius + 1e-2f;

	if (useCircleMask)
	{
		m_regionClipboard.hasCellMask = true;
		m_regionClipboard.cellMask.resize(static_cast<size_t>(widthSamples * heightSamples));
	}

	for (int iz = 0; iz < heightSamples; ++iz)
	{
		int const giz = gridZ0 + iz;
		for (int ix = 0; ix < widthSamples; ++ix)
		{
			int const gix = gridX0 + ix;
			float height = 0.0f;
			Vector pos(static_cast<float>(gix), 0.0f, static_cast<float>(giz));
			if (terrainObject->getHeight(pos, height))
				m_regionClipboard.heightData.push_back(height);
			else
				m_regionClipboard.heightData.push_back(0.0f);

			m_regionClipboard.shaderData.push_back(0);

			if (useCircleMask)
			{
				float const dx = static_cast<float>(gix) - m_regionCircleCenterX;
				float const dz = static_cast<float>(giz) - m_regionCircleCenterZ;
				m_regionClipboard.cellMask[static_cast<size_t>(iz * widthSamples + ix)] =
					(dx * dx + dz * dz <= r2) ? static_cast<unsigned char>(1) : static_cast<unsigned char>(0);
			}
		}
	}

	m_regionClipboard.hasData = true;

	if (postConsoleMessageOnSuccess)
	{
		QString msg;
		msg.sprintf("Region copied (1m grid): cells (%d,%d)-(%d,%d), %d x %d samples",
			gridX0, gridZ0, gridX0 + widthSamples - 1, gridZ0 + heightSamples - 1, widthSamples, heightSamples);
		MainFrame::getInstance().textToConsole(msg.latin1());
	}

	return true;
}

bool TerrainDock::terrainPasteClipboardIntoWorldRegion(bool postConsoleMessageOnSuccess)
{
	if (!m_regionClipboard.hasData || !m_hasRegionSelection)
		return false;

	float const eps = 1e-4f;
	int const destX0 = static_cast<int>(std::floor(std::min(m_regionMinX, m_regionMaxX) + eps));
	int const destZ0 = static_cast<int>(std::floor(std::min(m_regionMinZ, m_regionMaxZ) + eps));

	bool const painted = terrainApplyRegionClipboardAtOrigin(m_regionClipboard, destX0, destZ0, false);

	if (painted)
	{
		m_terrainModified = true;
		updateUndoRedoState();

		if (postConsoleMessageOnSuccess)
		{
			QString msg;
			msg.sprintf("Region pasted at cell origin (%d, %d)", destX0, destZ0);
			MainFrame::getInstance().textToConsole(msg.latin1());
		}
	}

	return painted;
}

bool TerrainDock::terrainApplyRegionClipboardAtOrigin(RegionClipboard const& clip, int gridX0, int gridZ0, bool postConsoleMessageOnSuccess)
{
	if (!clip.hasData || !GodClientTerrainEditor::isInstalled())
		return false;

	int const nx = clip.widthSamples;
	int const nz = clip.heightSamples;
	if (nx < 1 || nz < 1 || static_cast<int>(clip.heightData.size()) < nx * nz)
		return false;

	unsigned char const* maskPtr = 0;
	if (clip.hasCellMask && static_cast<int>(clip.cellMask.size()) >= nx * nz)
		maskPtr = &clip.cellMask[0];

	bool const painted = GodClientTerrainEditor::getInstance().applyRectangularHeightSamples(
		static_cast<float>(gridX0),
		static_cast<float>(gridZ0),
		static_cast<float>(gridX0 + nx - 1),
		static_cast<float>(gridZ0 + nz - 1),
		nx,
		nz,
		&clip.heightData[0],
		maskPtr);

	if (painted && postConsoleMessageOnSuccess)
	{
		QString msg;
		msg.sprintf("Applied height region at cell origin (%d, %d)", gridX0, gridZ0);
		MainFrame::getInstance().textToConsole(msg.latin1());
	}

	return painted;
}

bool TerrainDock::terrainWriteRegionClipboardToLayFile(QString const& path) const
{
	if (!m_regionClipboard.hasData)
		return false;

	int const nx = m_regionClipboard.widthSamples;
	int const nz = m_regionClipboard.heightSamples;
	if (nx < 1 || nz < 1 || static_cast<int>(m_regionClipboard.heightData.size()) < nx * nz)
		return false;

	QFile f(path);
	if (!f.open(IO_WriteOnly))
		return false;

	if (f.writeBlock(g_layMagic, 8) != 8)
		return false;
	if (!writeU32le(f, g_layVersion))
		return false;
	if (!writeS32le(f, m_regionClipboard.gridX0))
		return false;
	if (!writeS32le(f, m_regionClipboard.gridZ0))
		return false;
	if (!writeS32le(f, nx))
		return false;
	if (!writeS32le(f, nz))
		return false;
	unsigned int const flags = (m_regionClipboard.hasCellMask && static_cast<int>(m_regionClipboard.cellMask.size()) >= nx * nz) ? 1u : 0u;
	if (!writeU32le(f, flags))
		return false;

	for (int i = 0; i < nx * nz; ++i)
	{
		if (!writeF32le(f, m_regionClipboard.heightData[static_cast<size_t>(i)]))
			return false;
	}

	if (flags != 0u)
	{
		if (f.writeBlock(reinterpret_cast<char const*>(&m_regionClipboard.cellMask[0]), nx * nz) != nx * nz)
			return false;
	}

	return true;
}

bool TerrainDock::terrainDecodeLayFromFile(QString const& path, RegionClipboard& dest)
{
	QFile f(path);
	if (!f.open(IO_ReadOnly))
		return false;

	char magic[8];
	if (f.readBlock(magic, 8) != 8 || memcmp(magic, g_layMagic, 8) != 0)
		return false;

	unsigned int version = 0;
	if (!readU32le(f, version) || version != g_layVersion)
		return false;

	int fileGridX0 = 0;
	int fileGridZ0 = 0;
	int nx = 0;
	int nz = 0;
	if (!readS32le(f, fileGridX0) || !readS32le(f, fileGridZ0) || !readS32le(f, nx) || !readS32le(f, nz))
		return false;

	if (nx < 1 || nz < 1)
		return false;

	long long const totalCells = static_cast<long long>(nx) * static_cast<long long>(nz);
	if (totalCells > g_layMaxCells)
		return false;

	unsigned int flags = 0;
	if (!readU32le(f, flags))
		return false;

	dest.heightData.resize(static_cast<size_t>(nx * nz));
	for (long long i = 0; i < totalCells; ++i)
	{
		float h = 0.0f;
		if (!readF32le(f, h))
			return false;
		dest.heightData[static_cast<size_t>(i)] = h;
	}

	dest.shaderData.clear();
	dest.cellMask.clear();
	dest.hasCellMask = false;
	if ((flags & 1u) != 0u)
	{
		dest.cellMask.resize(static_cast<size_t>(nx * nz));
		if (f.readBlock(reinterpret_cast<char*>(&dest.cellMask[0]), nx * nz) != nx * nz)
			return false;
		dest.hasCellMask = true;
	}

	dest.gridX0 = fileGridX0;
	dest.gridZ0 = fileGridZ0;
	dest.sourceMinX = static_cast<float>(fileGridX0);
	dest.sourceMinZ = static_cast<float>(fileGridZ0);
	dest.sourceMaxX = static_cast<float>(fileGridX0 + nx - 1);
	dest.sourceMaxZ = static_cast<float>(fileGridZ0 + nz - 1);
	dest.widthSamples = nx;
	dest.heightSamples = nz;
	dest.hasData = true;
	return true;
}

bool TerrainDock::terrainReadRegionLayFileIntoClipboard(QString const& path)
{
	RegionClipboard loaded;
	if (!terrainDecodeLayFromFile(path, loaded))
		return false;
	m_regionClipboard = loaded;
	return true;
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
	if (nx < 1 || nz < 1 || static_cast<int>(m_regionClipboard.heightData.size()) < nx * nz)
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
	if (nx < 1 || nz < 1 || static_cast<int>(m_regionClipboard.heightData.size()) < nx * nz)
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Paste Region", "Invalid clipboard data."));
		return;
	}

	if (!terrainPasteClipboardIntoWorldRegion(true))
		IGNORE_RETURN(QMessageBox::warning(this, "Paste Region", "Paste failed."));
}

void TerrainDock::onSaveRegionLay()
{
	if (!m_regionClipboard.hasData)
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Save Region .lay", "Nothing to save — copy a region first."));
		return;
	}

	QString filename = QFileDialog::getSaveFileName(
		QString::null,
		"God Client region height (*.lay);;All Files (*.*)",
		this,
		"save region lay",
		"Save Region as .lay"
	);

	if (filename.isEmpty())
		return;

	if (!filename.endsWith(".lay", false))
		filename.append(".lay");

	if (!terrainWriteRegionClipboardToLayFile(filename))
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Save Region .lay", "Could not write the .lay file."));
		return;
	}

	MainFrame::getInstance().textToConsole(QString("Saved region .lay to %1").arg(filename).latin1());
}

void TerrainDock::onLoadRegionLay()
{
	QString filename = QFileDialog::getOpenFileName(
		QString::null,
		"God Client region height (*.lay);;All Files (*.*)",
		this,
		"load region lay",
		"Load Region .lay"
	);

	if (filename.isEmpty())
		return;

	if (!terrainReadRegionLayFileIntoClipboard(filename))
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Load Region .lay", "Could not read the .lay file (wrong format or version)."));
		return;
	}

	MainFrame::getInstance().textToConsole(QString("Loaded region .lay (%1 x %2 cells) into clipboard — use Paste or Import @ cursor.")
			.arg(m_regionClipboard.widthSamples)
			.arg(m_regionClipboard.heightSamples)
			.latin1());
}

void TerrainDock::onImportRegionLayAtCursor()
{
	if (!GodClientTerrainEditor::isInstalled())
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Import .lay @ Cursor", "Terrain editor not ready."));
		return;
	}

	QString filename = QFileDialog::getOpenFileName(
		QString::null,
		"God Client region height (*.lay);;All Files (*.*)",
		this,
		"import region lay cursor",
		"Import .lay at Cursor"
	);

	if (filename.isEmpty())
		return;

	RegionClipboard loaded;
	if (!terrainDecodeLayFromFile(filename, loaded))
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Import .lay @ Cursor", "Could not read the .lay file."));
		return;
	}

	Vector const& cursor = GodClientTerrainEditor::getInstance().getCursorWorldPosition();
	float const eps = 1e-4f;
	int const anchorX = static_cast<int>(std::floor(cursor.x + eps));
	int const anchorZ = static_cast<int>(std::floor(cursor.z + eps));

	if (!terrainApplyRegionClipboardAtOrigin(loaded, anchorX, anchorZ, false))
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Import .lay @ Cursor", "Import failed (no samples applied)."));
		return;
	}

	m_terrainModified = true;
	updateUndoRedoState();

	QString msg;
	msg.sprintf("Imported .lay at cell origin (%d, %d)", anchorX, anchorZ);
	MainFrame::getInstance().textToConsole(msg.latin1());
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

	syncGodClientEditorBrushSettings();

	if (m_toolMode == TM_ApplyEnvironmentRegion)
	{
		(void)tryApplyEnvironmentAffectorToCurrentRegion(true);
		return;
	}

	if (m_toolMode == TM_PaintShader)
	{
		if (m_shaderColorConstantPaintEnabled && !m_globalShaderPaintingSelection && !m_shaderColorConstantPickValid)
		{
			IGNORE_RETURN(QMessageBox::warning(this, "Fill Region", "Color constant paint: pick a color first."));
			return;
		}

		if (m_shaderColorConstantPaintEnabled && !m_globalShaderPaintingSelection && m_shaderColorConstantPickValid)
		{
			PackedRgb const rgb(m_shaderColorConstantR, m_shaderColorConstantG, m_shaderColorConstantB);
			bool const circ = (m_regionSelectionShape == RSS_Circle && m_regionCircleRadius > 0.01f);
			if (GodClientTerrainEditor::getInstance().applyRectangularVertexColorPaint(
					m_regionMinX,
					m_regionMinZ,
					m_regionMaxX,
					m_regionMaxZ,
					rgb,
					m_brushStrength,
					circ,
					m_regionCircleCenterX,
					m_regionCircleCenterZ,
					m_regionCircleRadius))
			{
				m_terrainModified = true;
				updateUndoRedoState();
				QString msg;
				msg.sprintf(
					"Region filled with vertex color tint RGB(%d,%d,%d): (%.1f, %.1f) - (%.1f, %.1f)",
					static_cast<int>(m_shaderColorConstantR),
					static_cast<int>(m_shaderColorConstantG),
					static_cast<int>(m_shaderColorConstantB),
					m_regionMinX,
					m_regionMinZ,
					m_regionMaxX,
					m_regionMaxZ);
				MainFrame::getInstance().textToConsole(msg.latin1());
			}
			else
			{
				IGNORE_RETURN(QMessageBox::warning(this, "Fill Region", "Could not apply color tint fill (no terrain or invalid region)."));
			}
			return;
		}

		int const fillFamily = m_selectedShaderFamilyId;
		QString const catalogPath = m_globalShaderPaintingSelection ? m_savedGlobalPickTrnCanon : QString();
		if (!ensureLiveTerrainShaderFamilyForPaint(fillFamily, catalogPath))
		{
			clampSelectedShaderFamilyToLiveTerrain();
			IGNORE_RETURN(QMessageBox::warning(this, "Fill Region", "Could not activate shader family for fill."));
			return;
		}

		bool const circ = (m_regionSelectionShape == RSS_Circle && m_regionCircleRadius > 0.01f);
		if (GodClientTerrainEditor::getInstance().applyRectangularShaderPaint(
				m_regionMinX,
				m_regionMinZ,
				m_regionMaxX,
				m_regionMaxZ,
				fillFamily,
				m_brushStrength,
				circ,
				m_regionCircleCenterX,
				m_regionCircleCenterZ,
				m_regionCircleRadius))
		{
			m_terrainModified = true;
			updateUndoRedoState();
			QString msg;
			msg.sprintf(
				"Region filled with shader family %d: (%.1f, %.1f) - (%.1f, %.1f)",
				fillFamily,
				m_regionMinX,
				m_regionMinZ,
				m_regionMaxX,
				m_regionMaxZ);
			MainFrame::getInstance().textToConsole(msg.latin1());
		}
		else
		{
			IGNORE_RETURN(QMessageBox::warning(this, "Fill Region", "Could not apply shader fill (no terrain or invalid region)."));
		}
		return;
	}

	switch (m_toolMode)
	{
	case TM_Raise:
	case TM_Lower:
	case TM_Flatten:
	case TM_Smooth:
	case TM_Noise:
	case TM_SetHeight:
		if (GodClientTerrainEditor::getInstance().applyRegionBrushFillHeightTools())
		{
			m_terrainModified = true;
			updateUndoRedoState();

			QString msg;
			msg.sprintf(
				"Region brush-fill applied: (%.1f, %.1f) - (%.1f, %.1f)",
				m_regionMinX,
				m_regionMinZ,
				m_regionMaxX,
				m_regionMaxZ);
			MainFrame::getInstance().textToConsole(msg.latin1());
		}
		else
		{
			IGNORE_RETURN(QMessageBox::warning(this, "Fill Region", "Brush fill failed (no terrain or no samples inside the region)."));
		}
		return;

	default:
		break;
	}

	IGNORE_RETURN(QMessageBox::warning(
		this,
		"Fill Region",
		"Choose a height tool, Paint Shader, Region environment (Fill), or use Apply environment to region."));
}

void TerrainDock::onMapTemplateSettingsClicked()
{
	ProceduralTerrainAppearanceTemplate* const tpl = getTerrainTemplate();
	if (!tpl)
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Map template", "No procedural terrain template."));
		return;
	}

	MapTemplateSettingsDialog dlg(this, tpl);
	if (dlg.exec() != QDialog::Accepted)
		return;

	float mapW = 0.f;
	float chunkW = 0.f;
	int tiles = 1;
	bool useGlobalWater = false;
	float wh = 0.f;
	float ws = 0.f;
	float envc = 0.f;
	if (!dlg.readValues(mapW, chunkW, tiles, useGlobalWater, wh, ws, envc))
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Map template", "Invalid values. Map and chunk width must be positive, tiles per chunk at least 1, environment cycle greater than zero."));
		return;
	}

	tpl->setMapLayoutParameters(mapW, chunkW, tiles);
	tpl->setUseGlobalWaterTable(useGlobalWater);
	tpl->setGlobalWaterTableHeight(wh);
	tpl->setGlobalWaterTableShaderSize(ws);
	tpl->setEnvironmentCycleTime(envc);

	TerrainGenerator* const gen = getTerrainGenerator();
	if (gen)
		gen->prepare();

	if (m_terrainFilePath.empty())
	{
		IGNORE_RETURN(QMessageBox::information(this, "Map template",
			"Choose a terrain file path (Save As) so the client can reload the .trn from disk."));
		onSaveTerrainAs();
	}

	if (m_terrainFilePath.empty())
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Map template", "No file path: changes were not saved and terrain was not reloaded."));
		m_terrainModified = true;
		updateUndoRedoState();
		refreshFromScene(true);
		return;
	}

	if (GodClientTerrainEditor::isInstalled())
		GodClientTerrainEditor::getInstance().flushTerrainChanges();

	m_terrainModified = true;
	onSaveTerrain();
	if (m_terrainModified)
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Map template", "Save failed; terrain was not reloaded."));
		refreshFromScene(true);
		return;
	}

	GroundScene* const gs = dynamic_cast<GroundScene*>(Game::getScene());
	if (gs)
		gs->reloadTerrain();
	else
		IGNORE_RETURN(QMessageBox::warning(this, "Map template", "No ground scene; terrain could not be reloaded."));

	refreshFromScene(true);
	MainFrame::getInstance().textToConsole("Map template updated, terrain saved and reloaded.");
}

void TerrainDock::onAddProceduralHeightConstantLayer()
{
	if (!GodClientTerrainEditor::isInstalled())
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Procedural", "Terrain editor is not ready."));
		return;
	}

	bool ok = false;
	float const h = QInputDialog::getDouble(
		tr("New height constant layer"),
		tr("Height (meters) for full-map layer:"),
		0.0,
		-1.0e6,
		1.0e6,
		2,
		&ok,
		this);

	if (!ok)
		return;

	if (!GodClientTerrainEditor::getInstance().addFullMapHeightConstantLayer(h, 32.f, 0))
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Procedural", "Could not add layer (no generator or invalid map size)."));
		return;
	}

	m_terrainModified = true;
	updateUndoRedoState();
	populateLayerList();
	MainFrame::getInstance().textToConsole("Added full-map AffectorHeightConstant layer.");
}

void TerrainDock::onAddProceduralShaderConstantLayer()
{
	if (!GodClientTerrainEditor::isInstalled())
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Procedural", "Terrain editor is not ready."));
		return;
	}

	if (m_selectedShaderFamilyId == 0)
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Procedural", "Select a shader family in the Shaders list first."));
		return;
	}

	if (!GodClientTerrainEditor::getInstance().addFullMapShaderConstantLayer(m_selectedShaderFamilyId, 48.f, 0))
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Procedural", "Could not add shader layer."));
		return;
	}

	m_terrainModified = true;
	updateUndoRedoState();
	populateLayerList();
	MainFrame::getInstance().textToConsole("Added full-map AffectorShaderConstant layer.");
}

void TerrainDock::onAddProceduralExcludeFromRegion()
{
	if (!GodClientTerrainEditor::isInstalled())
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Procedural", "Terrain editor is not ready."));
		return;
	}

	if (!m_hasRegionSelection)
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Procedural", "Select a rectangular region first."));
		return;
	}

	bool ok = false;
	float feather = QInputDialog::getDouble(
		tr("Exclude layer"),
		tr("Boundary feather (meters):"),
		8.0,
		0.0,
		4096.0,
		2,
		&ok,
		this);
	if (!ok)
		return;

	Rectangle2d const rect(
		std::min(m_regionMinX, m_regionMaxX),
		std::min(m_regionMinZ, m_regionMaxZ),
		std::max(m_regionMinX, m_regionMaxX),
		std::max(m_regionMinZ, m_regionMaxZ));

	if (!GodClientTerrainEditor::getInstance().addExcludeLayerForRectangle(rect, feather, 0))
	{
		IGNORE_RETURN(QMessageBox::warning(this, "Procedural", "Could not add exclude layer (region too small?)."));
		return;
	}

	m_terrainModified = true;
	updateUndoRedoState();
	populateLayerList();
	MainFrame::getInstance().textToConsole("Added AffectorExclude layer for current region.");
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

void TerrainDock::onFinalizePolygonDraw()
{
	if (GodClientTerrainEditor::isInstalled())
	{
		GodClientTerrainEditor::getInstance().finalizePolygonDraw();
		if (m_toolMode == TM_PlaceEnvironment ||
			m_toolMode == TM_PlaceExcludeTerrain ||
			m_toolMode == TM_PlaceBoundaryPolygon)
			setToolMode(TM_None);
		m_terrainModified = true;
	}
	updateRegionGeometryUi();
}

void TerrainDock::onCancelPolygonDraw()
{
	if (GodClientTerrainEditor::isInstalled())
	{
		GodClientTerrainEditor::getInstance().cancelPolygonDraw();
		if (m_toolMode == TM_PlaceEnvironment ||
			m_toolMode == TM_PlaceExcludeTerrain ||
			m_toolMode == TM_PlaceBoundaryPolygon)
			setToolMode(TM_None);
	}
	updateRegionGeometryUi();
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
	               editor.getVertexColorModificationCount() +
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

// ----------------------------------------------------------------------

void TerrainDock::clampSelectedShaderFamilyToLiveTerrain()
{
	TerrainGenerator* const g = getTerrainGenerator();
	if (!g)
	{
		m_selectedShaderFamilyId = 0;
		return;
	}

	ShaderGroup const& sg = g->getShaderGroup();
	if (sg.hasFamily(m_selectedShaderFamilyId))
		return;

	int const n = sg.getNumberOfFamilies();
	if (n > 0)
	{
		m_selectedShaderFamilyId = sg.getFamilyId(0);
		MainFrame::getInstance().textToConsole("Shader paint selection reset to first live terrain family (invalid or missing id).");
	}
	else
	{
		m_selectedShaderFamilyId = 0;
		MainFrame::getInstance().textToConsole("Shader paint selection cleared (terrain has no shader families).");
	}
}

// ======================================================================

bool TerrainDock::ensureLiveTerrainShaderFamilyForPaint(int catalogFamilyId, QString const& catalogSourceTrnPath)
{
	TerrainGenerator* const dstGenerator = getTerrainGenerator();
	if (!dstGenerator)
		return false;

	ShaderGroup& dstSg(dstGenerator->getShaderGroup());

	if (catalogSourceTrnPath.isEmpty())
		return dstSg.hasFamily(catalogFamilyId);

	QCString const pathBytes(QFile::encodeName(catalogSourceTrnPath));
	Iff iff(1024 * 1024);
	if (!iff.open(pathBytes.data(), true))
		return false;

	SamplerProceduralTerrainAppearanceTemplate sampler(pathBytes.data(), &iff);
	TerrainGenerator const* const srcGen(sampler.getTerrainGenerator());
	if (!srcGen || !srcGen->getShaderGroup().hasFamily(catalogFamilyId))
		return false;

	ShaderGroup const& srcSg = srcGen->getShaderGroup();

	bool mutated(false);
	bool collisionRemapped(false);
	bool globalAllocated(false);

	int const matchedDst = terrainDockFindDstShaderFamilyMatchingChildren(srcSg, catalogFamilyId, dstSg);
	if (matchedDst >= 0)
	{
		m_selectedShaderFamilyId = matchedDst;
	}
	else if (dstSg.hasFamily(catalogFamilyId))
	{
		if (terrainDockShaderFamilyChildrenMatch(srcSg, dstSg, catalogFamilyId, catalogFamilyId))
		{
			m_selectedShaderFamilyId = catalogFamilyId;
		}
		else
		{
			int const newId = terrainDockFindUnusedTerrainShaderFamilyId(dstSg);
			if (newId < 0)
				return false;
			if (!terrainDockCopyTerrainShaderFamilyRemapIds(srcSg, dstSg, catalogFamilyId, newId, false))
				return false;
			m_selectedShaderFamilyId = newId;
			mutated = true;
			collisionRemapped = true;
		}
	}
	else
	{
		// Pulling from another .trn: allocate a fresh live family id (never bind at catalogFamilyId),
		// so scene family 0 (or any reserved id) is never overwritten by catalog id collisions.
		int const newId = terrainDockFindUnusedTerrainShaderFamilyId(dstSg);
		if (newId < 0)
			return false;
		if (!terrainDockCopyTerrainShaderFamilyRemapIds(srcSg, dstSg, catalogFamilyId, newId, false))
			return false;
		m_selectedShaderFamilyId = newId;
		mutated = true;
		globalAllocated = true;
	}

	if (mutated)
	{
		dstSg.loadSurfaceProperties();
		ClientProceduralTerrainAppearance* const cterrain = getClientTerrain();
		if (cterrain)
			cterrain->rebuildShaderCacheFromGenerator();
	}

	if (collisionRemapped)
	{
		QString msg;
		msg.sprintf(
			"Global catalog family %d differed from the scene; cloned template list to unused family id %d so existing terrain families are untouched.",
			catalogFamilyId, m_selectedShaderFamilyId);
		MainFrame::getInstance().textToConsole(msg.latin1());
	}
	else if (globalAllocated)
	{
		QString msg;
		msg.sprintf(
			"Global catalog family %d imported as new live shader family id %d (catalog ids are not copied onto the scene when absent).",
			catalogFamilyId, m_selectedShaderFamilyId);
		MainFrame::getInstance().textToConsole(msg.latin1());
	}

	return dstSg.hasFamily(m_selectedShaderFamilyId);
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

void TerrainDock::updateShaderColorConstantControls()
{
	bool const sceneShaderList = hasActiveTerrain() && !m_globalShaderPaintingSelection;
	if (m_shaderColorConstantCheck)
	{
		if (!sceneShaderList && m_shaderColorConstantCheck->isChecked())
		{
			m_shaderColorConstantCheck->setChecked(false);
			m_shaderColorConstantPaintEnabled = false;
		}
		m_shaderColorConstantCheck->setEnabled(sceneShaderList);
	}
	if (m_shaderColorPickButton)
		m_shaderColorPickButton->setEnabled(sceneShaderList);
	if (!m_shaderColorSummaryLabel)
		return;

	if (!sceneShaderList)
	{
		m_shaderColorSummaryLabel->setText(QString::fromLatin1("(Color tint: scene shader list only.)"));
		return;
	}
	if (!m_shaderColorConstantPickValid)
	{
		m_shaderColorSummaryLabel->setText(QString::fromLatin1("No color picked."));
		return;
	}

	QString line;
	line.sprintf(
		"RGB(%d,%d,%d) — vertex tint (shader family unchanged)",
		static_cast<int>(m_shaderColorConstantR),
		static_cast<int>(m_shaderColorConstantG),
		static_cast<int>(m_shaderColorConstantB));
	m_shaderColorSummaryLabel->setText(line);
}

void TerrainDock::onShaderColorConstantToggled(bool enabled)
{
	m_shaderColorConstantPaintEnabled = enabled;
	updateShaderColorConstantControls();
	syncGodClientEditorBrushSettings();
}

void TerrainDock::onShaderColorConstantPickClicked()
{
	QColor const initial(m_shaderColorConstantR, m_shaderColorConstantG, m_shaderColorConstantB);
	QColor const c = QColorDialog::getColor(initial, this, "Pick terrain shader color");
	if (!c.isValid())
		return;

	m_shaderColorConstantR = static_cast<uint8>(std::max(0, std::min(255, c.red())));
	m_shaderColorConstantG = static_cast<uint8>(std::max(0, std::min(255, c.green())));
	m_shaderColorConstantB = static_cast<uint8>(std::max(0, std::min(255, c.blue())));
	m_shaderColorConstantPickValid = true;
	m_shaderColorConstantPaintEnabled = true;
	if (m_shaderColorConstantCheck)
		m_shaderColorConstantCheck->setChecked(true);
	updateShaderColorConstantControls();
	syncGodClientEditorBrushSettings();
}

void TerrainDock::paintShaderAtPoint(float worldX, float worldZ, int shaderFamilyId)
{
	UNREF(shaderFamilyId);

	TerrainGenerator* const generator = getTerrainGenerator();
	if (!generator)
	{
		MainFrame::getInstance().textToConsole("paintShaderAtPoint: No terrain generator available");
		return;
	}

	if (m_shaderColorConstantPaintEnabled && !m_globalShaderPaintingSelection && !m_shaderColorConstantPickValid)
	{
		MainFrame::getInstance().textToConsole("Color constant paint: pick a color first (Pick paint color…).");
		return;
	}

	if (m_shaderColorConstantPaintEnabled && !m_globalShaderPaintingSelection && m_shaderColorConstantPickValid)
	{
		if (!GodClientTerrainEditor::isInstalled())
		{
			MainFrame::getInstance().textToConsole("paintShaderAtPoint: Terrain editor not ready");
			return;
		}

		PackedRgb const rgb(m_shaderColorConstantR, m_shaderColorConstantG, m_shaderColorConstantB);
		GodClientTerrainEditor& terrainEditor = GodClientTerrainEditor::getInstance();
		terrainEditor.beginShaderUndoBatch();
		terrainEditor.applyVertexColorPaintDab(worldX, worldZ, rgb, m_brushStrength);
		terrainEditor.endShaderUndoBatch();
		updateUndoRedoState();
		m_terrainModified = true;

		QString msg;
		msg.sprintf(
			"Vertex color tint RGB(%d,%d,%d) at (%.1f, %.1f)",
			static_cast<int>(m_shaderColorConstantR),
			static_cast<int>(m_shaderColorConstantG),
			static_cast<int>(m_shaderColorConstantB),
			worldX,
			worldZ);
		MainFrame::getInstance().textToConsole(msg.latin1());
		return;
	}

	int const paintFamilyId = m_selectedShaderFamilyId;

	QString const catalogPath = m_globalShaderPaintingSelection ? m_savedGlobalPickTrnCanon : QString();
	if (!ensureLiveTerrainShaderFamilyForPaint(paintFamilyId, catalogPath))
	{
		clampSelectedShaderFamilyToLiveTerrain();
		QString msg;
		msg.sprintf("paintShaderAtPoint: Unable to activate shader family %d (merge global catalog families or refresh scene shaders).",
			paintFamilyId);
		MainFrame::getInstance().textToConsole(msg.latin1());
		return;
	}

	if (!GodClientTerrainEditor::isInstalled())
	{
		MainFrame::getInstance().textToConsole("paintShaderAtPoint: Terrain editor not ready");
		return;
	}

	ShaderGroup const& shaderGroup = generator->getShaderGroup();

	GodClientTerrainEditor& terrainEditor = GodClientTerrainEditor::getInstance();
	terrainEditor.beginShaderUndoBatch();
	terrainEditor.applyShaderPaintDab(worldX, worldZ, paintFamilyId, m_brushStrength);
	terrainEditor.endShaderUndoBatch();
	updateUndoRedoState();
	m_terrainModified = true;

	const char* familyName = shaderGroup.getFamilyName(paintFamilyId);

	QString msg;
	msg.sprintf(
		"Shader '%s' (id %d) painted at (%.1f, %.1f)",
		familyName ? familyName : "Unknown",
		paintFamilyId,
		worldX,
		worldZ);
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
	
	if (m_toolMode == TM_None || m_toolMode == TM_Select || m_toolMode == TM_ApplyEnvironmentRegion)
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

// ----------------------------------------------------------------------

bool TerrainDock::pickTerrainGroundForLiveEdit(int screenX, int screenY, float& outWorldX, float& outWorldZ, float& outGroundY) const
{
	const GroundScene* const scene = dynamic_cast<const GroundScene*>(Game::getScene());
	if (!scene || !scene->getPlayer())
		return false;

	const Camera* const camera = scene->getCurrentCamera();
	if (!camera)
		return false;

	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (!terrainObject)
		return false;

	Vector const start_p(camera->getPosition_p());
	Vector const delta(8192.0f * camera->rotate_o2p(camera->reverseProjectInScreenSpace(screenX, screenY)));

	CollisionInfo info;
	Vector const end_p(start_p + delta);
	if (terrainObject->collide(start_p, end_p, info))
	{
		Vector const& hitPoint = info.getPoint();
		outWorldX = hitPoint.x;
		outWorldZ = hitPoint.z;
		outGroundY = hitPoint.y;
		m_liveEditGroundPickFallbackValid = true;
		m_liveEditGroundPickFallbackY = hitPoint.y;
		return true;
	}

	Vector dir(delta);
	float const dirLen = dir.magnitude();
	if (dirLen < 1.e-5f)
		return false;
	dir /= dirLen;

	if (std::fabs(static_cast<double>(dir.y)) < static_cast<double>(1.e-8))
		return false;

	float const baselineY =
		m_liveEditGroundPickFallbackValid
			? m_liveEditGroundPickFallbackY
			: scene->getPlayer()->getPosition_w().y - 2.f;

	float const dy = baselineY - start_p.y;
	float const t = dy / dir.y;
	static float const tMaxFallback = 3.25e6f;
	if (!(t > 1.e-5f && t <= tMaxFallback))
		return false;

	Vector const planeHit(start_p + dir * t);

	float probeX = planeHit.x;
	float probeZ = planeHit.z;

	Vector top(probeX, 8000.f, probeZ);
	Vector bot(probeX, -8000.f, probeZ);
	if (terrainObject->collide(top, bot, info))
	{
		Vector const& hitPoint = info.getPoint();
		outWorldX = hitPoint.x;
		outWorldZ = hitPoint.z;
		outGroundY = hitPoint.y;
		m_liveEditGroundPickFallbackValid = true;
		m_liveEditGroundPickFallbackY = hitPoint.y;
		return true;
	}

	Vector xz(probeX, 0.f, probeZ);
	float h = baselineY;
	if (terrainObject->getHeight(xz, h))
	{
		outWorldX = probeX;
		outWorldZ = probeZ;
		outGroundY = h;
		m_liveEditGroundPickFallbackValid = true;
		m_liveEditGroundPickFallbackY = h;
		return true;
	}

	outWorldX = probeX;
	outWorldZ = probeZ;
	outGroundY = baselineY;
	m_liveEditGroundPickFallbackY = baselineY;
	m_liveEditGroundPickFallbackValid = true;
	return true;
}

// ----------------------------------------------------------------------

bool TerrainDock::pickTerrainGroundPlanarForLiveDrag(int screenX, int screenY, float& outWorldX, float& outWorldZ, float& outGroundY) const
{
	const GroundScene* const scene = dynamic_cast<const GroundScene*>(Game::getScene());
	if (!scene || !scene->getPlayer())
		return false;

	const Camera* const camera = scene->getCurrentCamera();
	if (!camera)
		return false;

	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (!terrainObject)
		return false;

	Vector const start_p(camera->getPosition_p());
	Vector const delta(8192.0f * camera->rotate_o2p(camera->reverseProjectInScreenSpace(screenX, screenY)));

	Vector dir(delta);
	float const dirLen = dir.magnitude();
	if (dirLen < 1.e-5f)
		return false;
	dir /= dirLen;

	if (std::fabs(static_cast<double>(dir.y)) < static_cast<double>(1.e-8))
		return false;

	float const baselineY =
		m_liveEditGroundPickFallbackValid
			? m_liveEditGroundPickFallbackY
			: scene->getPlayer()->getPosition_w().y - 2.f;

	float const dy = baselineY - start_p.y;
	float const t = dy / dir.y;
	static float const tMaxFallback = 3.25e6f;
	if (!(t > 1.e-5f && t <= tMaxFallback))
		return false;

	Vector const planeHit(start_p + dir * t);

	float probeX = planeHit.x;
	float probeZ = planeHit.z;

	CollisionInfo info;
	Vector top(probeX, 8000.f, probeZ);
	Vector bot(probeX, -8000.f, probeZ);
	if (terrainObject->collide(top, bot, info))
	{
		Vector const& hitPoint = info.getPoint();
		outWorldX = hitPoint.x;
		outWorldZ = hitPoint.z;
		outGroundY = hitPoint.y;
		m_liveEditGroundPickFallbackValid = true;
		m_liveEditGroundPickFallbackY = hitPoint.y;
		return true;
	}

	Vector xz(probeX, 0.f, probeZ);
	float h = baselineY;
	if (terrainObject->getHeight(xz, h))
	{
		outWorldX = probeX;
		outWorldZ = probeZ;
		outGroundY = h;
		m_liveEditGroundPickFallbackValid = true;
		m_liveEditGroundPickFallbackY = h;
		return true;
	}

	outWorldX = probeX;
	outWorldZ = probeZ;
	outGroundY = baselineY;
	m_liveEditGroundPickFallbackY = baselineY;
	m_liveEditGroundPickFallbackValid = true;
	return true;
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
	
	if (!GodClientTerrainEditor::isInstalled())
	{
		MainFrame::getInstance().textToConsole("createWaterBoundary: Terrain editor not installed");
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

	syncGodClientEditorBrushSettings();

	float const radialHalf = std::max(4.0f, radius);

	GodClientTerrainEditor::getInstance().installLocalWaterTableAxisAligned(centerX, centerZ, radialHalf, height);

	m_terrainModified = true;
	
	QString msg;
	msg.sprintf("Water table inserted at (%.1f, %.1f): halfExtent %.1f world, height %.2f.",
		centerX, centerZ, radialHalf, height);
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
	if (terrainDockIsPolylineStrokeTool(m_toolMode))
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
	
	float worldX = 0.0f;
	float worldZ = 0.0f;
	float groundY = 0.0f;
	if (!pickTerrainGroundForLiveEdit(screenX, screenY, worldX, worldZ, groundY))
		return false;

	{
		TerrainObject* const terrainObject = TerrainObject::getInstance();
		Vector xz(worldX, 0.0f, worldZ);
		float sampledY = m_liveEditGroundPickFallbackValid ? m_liveEditGroundPickFallbackY : groundY;
		if (terrainObject && terrainObject->getHeight(xz, sampledY))
		{
			m_liveEditGroundPickFallbackValid = true;
			m_liveEditGroundPickFallbackY = sampledY;
		}
	}

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
		
		// Handle polyline editing modes (road/ribbon/boundary corridor)
		if (terrainDockIsPolylineStrokeTool(m_toolMode))
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
						emit terrainGameWindowStatusChanged();
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
						emit terrainGameWindowStatusChanged();
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
				emit terrainGameWindowStatusChanged();
				return true;
			}

			int polyFid = m_selectedShaderFamilyId;
			int const pci = m_polylineShaderCombo ? m_polylineShaderCombo->currentItem() : 0;
			if (pci >= 0 && pci < static_cast<int>(m_polylineShaderFamilyIds.size()))
				polyFid = m_polylineShaderFamilyIds[static_cast<size_t>(pci)];
			GodClientTerrainEditor::PolylineCommitKind pck = GodClientTerrainEditor::PCK_RoadRibbon;
			if (m_toolMode == TM_PlaceBoundaryPolyline)
				pck = GodClientTerrainEditor::PCK_BoundaryPolyline;
			else if (m_toolMode == TM_PlaceBoundaryPolyRoad)
				pck = GodClientTerrainEditor::PCK_BoundaryPolyRoad;
			editor.beginPolyline(m_toolMode == TM_PlaceRibbon, pck);
			editor.setPolylineWidth(m_polylineWidth);
			editor.setPolylineShaderFamily(polyFid);
			editor.setPolylineFeatherDistance(m_polylineFeather);
			editor.setPolylineUseFixedHeights(m_polylineFixedHeights);
			editor.addPolylinePoint(worldX, worldZ);
			emit terrainGameWindowStatusChanged();
			return true;
		}
		
		// Handle environment / exclude / boundary polygon placement (shared point list)
		if (m_toolMode == TM_PlaceEnvironment ||
			m_toolMode == TM_PlaceExcludeTerrain ||
			m_toolMode == TM_PlaceBoundaryPolygon)
		{
			if (editor.isPolygonDrawActive())
			{
				editor.addEnvironmentZonePoint(worldX, worldZ);
				updateRegionGeometryUi();
				return true;
			}

			if (m_toolMode == TM_PlaceEnvironment)
			{
				int envFid = 0;
				int const eci = m_environmentFamilyCombo ? m_environmentFamilyCombo->currentItem() : 0;
				if (eci >= 0 && eci < static_cast<int>(m_environmentFamilyIds.size()))
					envFid = m_environmentFamilyIds[static_cast<size_t>(eci)];
				editor.beginEnvironmentZone();
				editor.setEnvironmentFamily(envFid);
				editor.addEnvironmentZonePoint(worldX, worldZ);
				updateRegionGeometryUi();
				return true;
			}

			if (m_toolMode == TM_PlaceExcludeTerrain)
				editor.beginPolygonDraw(GodClientTerrainEditor::PDP_ExcludeTerrain);
			else
				editor.beginPolygonDraw(GodClientTerrainEditor::PDP_BoundaryPolygon);

			editor.addEnvironmentZonePoint(worldX, worldZ);
			updateRegionGeometryUi();
			return true;
		}

		if (m_toolMode == TM_ApplyEnvironmentRegion)
		{
			if (tryApplyEnvironmentAffectorToCurrentRegion(false))
				emit terrainGameWindowStatusChanged();
			return true;
		}
		
		// Handle bitmap stamp
		if (m_toolMode == TM_StampBitmap)
		{
			editor.applyBitmapStamp(worldX, worldZ);
			m_terrainModified = true;
			return true;
		}
		
		syncGodClientEditorBrushSettings();

		// Region-wide shader fills use Fill Region — LMB always runs the brush stroke so paint
		// matches the yellow preview ring (Painting with an active selection still clips to region).

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
		float unusedY = 0.0f;
		if (pickTerrainGroundForLiveEdit(screenX, screenY, worldX, worldZ, unusedY))
		{
			m_regionDragCurX = worldX;
			m_regionDragCurZ = worldZ;
		}
		m_regionDragActive = false;
		if (m_regionSelectionShape == RSS_Circle)
		{
			m_regionCircleCenterX = m_regionAnchorX;
			m_regionCircleCenterZ = m_regionAnchorZ;
			float const dx = m_regionDragCurX - m_regionAnchorX;
			float const dz = m_regionDragCurZ - m_regionAnchorZ;
			m_regionCircleRadius = std::sqrt(dx * dx + dz * dz);
			if (m_regionCircleRadius < 0.5f)
				m_regionCircleRadius = 0.5f;
			m_regionMinX = m_regionCircleCenterX - m_regionCircleRadius;
			m_regionMaxX = m_regionCircleCenterX + m_regionCircleRadius;
			m_regionMinZ = m_regionCircleCenterZ - m_regionCircleRadius;
			m_regionMaxZ = m_regionCircleCenterZ + m_regionCircleRadius;
		}
		else
		{
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
		}
		m_hasRegionSelection = true;
		syncRegionSelectionToEditor();
		MainFrame::getInstance().textToConsole("Terrain region selection updated.");
		emit terrainGameWindowStatusChanged();
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

	float worldX = 0.0f;
	float worldZ = 0.0f;
	float groundY = 0.0f;

	bool pickedOk = pickTerrainGroundForLiveEdit(screenX, screenY, worldX, worldZ, groundY);
	if (!pickedOk && GodClientTerrainEditor::isInstalled())
	{
		GodClientTerrainEditor& editor = GodClientTerrainEditor::getInstance();
		bool const needPlanarDrag =
			editor.isBrushStrokeActive()
			|| (m_polylineDragPointIndex >= 0 && terrainDockIsPolylineStrokeTool(m_toolMode))
			|| (m_toolMode == TM_Select && m_regionDragActive);
		if (needPlanarDrag)
			pickedOk = pickTerrainGroundPlanarForLiveDrag(screenX, screenY, worldX, worldZ, groundY);
	}

	if (!pickedOk)
		return false;

	if (m_toolMode == TM_Select && m_regionDragActive)
	{
		m_regionDragCurX = worldX;
		m_regionDragCurZ = worldZ;
		return true;
	}

	if (m_polylineDragPointIndex >= 0 && terrainDockIsPolylineStrokeTool(m_toolMode) && GodClientTerrainEditor::isInstalled())
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
				if (!(terrainObject && terrainObject->getHeight(pos, dragHeight)))
					dragHeight = groundY;
			}
			editor.movePolylinePoint(m_polylineDragPointIndex, worldX, worldZ, dragHeight);
			return true;
		}
	}

	if (GodClientTerrainEditor::isInstalled())
	{
		GodClientTerrainEditor& editor = GodClientTerrainEditor::getInstance();

		editor.setCursorWorldPosition(Vector(worldX, groundY, worldZ));

		if (editor.isBrushStrokeActive())
		{
			editor.continueBrushStroke(worldX, worldZ);
			return true;
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

	float regionOverlayMinX = 0.f;
	float regionOverlayMinZ = 0.f;
	float regionOverlayMaxX = 0.f;
	float regionOverlayMaxZ = 0.f;
	bool showRegionOverlay = false;
	bool overlayCircular = false;
	float overlayCircleCx = 0.f;
	float overlayCircleCz = 0.f;
	float overlayCircleR = 0.f;

	if (m_toolMode == TM_Select && m_regionDragActive)
	{
		if (m_regionSelectionShape == RSS_Circle)
		{
			float const dx = m_regionDragCurX - m_regionAnchorX;
			float const dz = m_regionDragCurZ - m_regionAnchorZ;
			overlayCircleR = std::sqrt(dx * dx + dz * dz);
			if (overlayCircleR < 0.5f)
				overlayCircleR = 0.5f;
			overlayCircleCx = m_regionAnchorX;
			overlayCircleCz = m_regionAnchorZ;
			regionOverlayMinX = overlayCircleCx - overlayCircleR;
			regionOverlayMaxX = overlayCircleCx + overlayCircleR;
			regionOverlayMinZ = overlayCircleCz - overlayCircleR;
			regionOverlayMaxZ = overlayCircleCz + overlayCircleR;
			overlayCircular = true;
		}
		else
		{
			float minX = std::min(m_regionAnchorX, m_regionDragCurX);
			float maxX = std::max(m_regionAnchorX, m_regionDragCurX);
			float minZ = std::min(m_regionAnchorZ, m_regionDragCurZ);
			float maxZ = std::max(m_regionAnchorZ, m_regionDragCurZ);
			if (maxX - minX < 0.5f)
				maxX = minX + 0.5f;
			if (maxZ - minZ < 0.5f)
				maxZ = minZ + 0.5f;
			regionOverlayMinX = minX;
			regionOverlayMaxX = maxX;
			regionOverlayMinZ = minZ;
			regionOverlayMaxZ = maxZ;
		}
		showRegionOverlay = true;
	}
	else if (m_hasRegionSelection)
	{
		regionOverlayMinX = m_regionMinX;
		regionOverlayMaxX = m_regionMaxX;
		regionOverlayMinZ = m_regionMinZ;
		regionOverlayMaxZ = m_regionMaxZ;
		if (m_regionSelectionShape == RSS_Circle)
		{
			overlayCircular = true;
			overlayCircleCx = m_regionCircleCenterX;
			overlayCircleCz = m_regionCircleCenterZ;
			overlayCircleR = m_regionCircleRadius;
		}
		showRegionOverlay = true;
	}

	if (showRegionOverlay)
		editor.renderRegionSelectionOverlay(
			*camera,
			regionOverlayMinX,
			regionOverlayMinZ,
			regionOverlayMaxX,
			regionOverlayMaxZ,
			overlayCircular,
			overlayCircleCx,
			overlayCircleCz,
			overlayCircleR);
	
	// Render brush preview for brush-based tools
	if (m_showBrushPreview && m_toolMode != TM_None && m_toolMode != TM_Select)
	{
		const bool skipBrushRing =
			terrainDockIsPolylineStrokeTool(m_toolMode) ||
			m_toolMode == TM_PlaceEnvironment ||
			m_toolMode == TM_PlaceExcludeTerrain ||
			m_toolMode == TM_PlaceBoundaryPolygon ||
			m_toolMode == TM_ApplyEnvironmentRegion;
		if (!skipBrushRing)
			editor.renderBrushPreview(*camera);
	}
	
	// Render polyline preview for road/ribbon tools
	if (m_showPolylinePreview && terrainDockIsPolylineStrokeTool(m_toolMode))
	{
		editor.renderPolylinePreview(*camera);
	}
}

// ----------------------------------------------------------------------

void TerrainDock::refreshTerrainLayerListFromGenerator()
{
	populateLayerList();
}

// ----------------------------------------------------------------------

bool TerrainDock::suppressCameraDoubleClickForTerrainTool() const
{
	if (!isTerrainEditingActive())
		return false;
	switch (m_toolMode)
	{
	case TM_Select:
	case TM_Raise:
	case TM_Lower:
	case TM_Flatten:
	case TM_Smooth:
	case TM_Noise:
	case TM_SetHeight:
	case TM_PaintShader:
	case TM_PaintFlora:
	case TM_PlaceWater:
	case TM_PlaceRadial:
	case TM_PlaceRibbon:
	case TM_PlaceRoad:
	case TM_PlaceEnvironment:
	case TM_PlaceExcludeTerrain:
	case TM_PlaceBoundaryPolygon:
	case TM_PlaceBoundaryPolyline:
	case TM_PlaceBoundaryPolyRoad:
	case TM_StampBitmap:
	case TM_ApplyEnvironmentRegion:
		return true;
	default:
		return false;
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
