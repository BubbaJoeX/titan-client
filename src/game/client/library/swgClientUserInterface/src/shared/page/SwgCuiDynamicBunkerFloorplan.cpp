// ======================================================================
//
// SwgCuiDynamicBunkerFloorplan.cpp
// copyright 2026 Titan
//
// ======================================================================

#include "swgClientUserInterface/FirstSwgClientUserInterface.h"
#include "swgClientUserInterface/SwgCuiDynamicBunkerFloorplan.h"

#include "clientGame/ContainerInterface.h"
#include "clientGame/CreatureObject.h"
#include "clientGame/CellObject.h"
#include "clientGame/ClientWorld.h"
#include "clientGame/DynamicBunkerClient.h"
#include "clientGame/Game.h"
#include "clientGame/GroundScene.h"
#include "clientObject/GameCamera.h"
#include "sharedCollision/CollideParameters.h"
#include "sharedCollision/CollisionInfo.h"
#include "sharedObject/CellProperty.h"
#include "sharedObject/NetworkIdManager.h"
#include "sharedObject/PortalProperty.h"
#include "sharedObject/PortalPropertyTemplate.h"
#include "clientUserInterface/CuiManager.h"
#include "clientUserInterface/CuiIoWin.h"
#include "clientUserInterface/CuiPreferences.h"
#include "clientUserInterface/CuiWidget3dObjectListViewer.h"
#include "sharedFile/TreeFile.h"
#include "sharedFoundation/CrcLowerString.h"
#include "sharedGame/SharedObjectTemplate.h"
#include "sharedCollision/Floor.h"
#include "sharedFile/Iff.h"
#include "sharedMath/AxialBox.h"
#include "sharedObject/Appearance.h"
#include "sharedObject/AppearanceTemplate.h"
#include "sharedObject/AppearanceTemplateList.h"
#include "sharedObject/Object.h"
#include "sharedObject/ObjectTemplateList.h"
#include "sharedObject/PortalPropertyTemplateList.h"
#include "clientGame/GameNetwork.h"
#include "sharedNetworkMessages/DynamicBunkerMessages.h"
#include "sharedMath/Transform.h"
#include "sharedMath/Vector.h"

#include "UIButton.h"
#include "UIButtonStyle.h"
#include "UIBaseObject.h"
#include "UIData.h"
#include "UIDataSource.h"
#include "UIList.h"
#include "UIPage.h"
#include "UITabbedPane.h"
#include "UIText.h"
#include "UITextbox.h"
#include "UIMessage.h"
#include "UIManager.h"

#include <cmath>
#include <cctype>
#include <cstdio>
#include <map>
#include <set>
#include <sstream>

namespace SwgCuiDynamicBunkerFloorplanNamespace
{
	char const * const s_placePointMarkerAppearance = "appearance/godclient_space_waypoint_generic.apt";

	Object * createPlacePointMarker(Vector const & worldPos)
	{
		Appearance * const app = AppearanceTemplateList::createAppearance(s_placePointMarkerAppearance);
		if (!app)
			return 0;

		Object * const marker = new Object;
		marker->setAppearance(app);
		marker->setScale(Vector(0.35f, 0.35f, 0.35f));
		marker->setPosition_w(worldPos);
		IGNORE_RETURN(marker->alter(0.0f));
		marker->conclude();
		return marker;
	}

	bool pickWorldPointFromScreen(int mouseX, int mouseY, Vector & outWorldPoint, NetworkId & outCellNetworkId)
	{
		outCellNetworkId = NetworkId::cms_invalid;

		Camera const * const camera = Game::getCamera();
		if (!camera)
			return false;

		Vector const worldStart(camera->getPosition_w());
		Vector const worldEnd(
			worldStart +
			camera->rotate_o2w(camera->reverseProjectInScreenSpace(mouseX, mouseY)) * 10000.0f);

		CellProperty const * const cameraCell = camera->getParentCell();
		CollisionInfo info;
		uint16 const collisionFlags =
			ClientWorld::CF_interiorObjects |
			ClientWorld::CF_interiorGeometry |
			ClientWorld::CF_tangible |
			ClientWorld::CF_tangibleNotTargetable |
			ClientWorld::CF_childObjects |
			ClientWorld::CF_terrain;

		CollideParameters collideParams;
		collideParams.setQuality(CollideParameters::Q_high);

		Object const * const player = Game::getPlayer();
		if (!ClientWorld::collide(cameraCell, worldStart, worldEnd, collideParams, info, collisionFlags, player))
			return false;

		outWorldPoint = info.getPoint();

		Object const * const hitObject = info.getObject();
		if (hitObject)
		{
			CellProperty const * const hitCell = hitObject->getParentCell();
			if (hitCell && !hitCell->isWorldCell())
			{
				CellObject const * const cellObject = CellObject::asCellObject(&hitCell->getOwner());
				if (cellObject)
					outCellNetworkId = cellObject->getNetworkId();
			}
		}

		return true;
	}

	float snapYawToCardinal45(float yaw)
	{
		float const snap = 0.78539816339f;
		return floorf(yaw / snap + 0.5f) * snap;
	}
	void ensureListDataSource(UIList * list, char const * name)
	{
		if (!list)
			return;

		if (list->GetDataSource())
			return;

		UIDataSource * ds = new UIDataSource;
		ds->SetName(name);
		if (list->GetParent())
			list->GetParent()->AddChild(ds);
		else
			list->AddChild(ds);
		list->SetDataSource(ds);
	}

	bool containsIgnoreCase(std::string const & haystack, std::string const & needle)
	{
		if (needle.empty())
			return true;

		std::string h = haystack;
		std::string n = needle;
		for (size_t i = 0; i < h.size(); ++i)
			h[i] = static_cast<char>(tolower(static_cast<unsigned char>(h[i])));
		for (size_t i = 0; i < n.size(); ++i)
			n[i] = static_cast<char>(tolower(static_cast<unsigned char>(n[i])));
		return h.find(n) != std::string::npos;
	}

	bool pathLooksUseful(std::string const & pobPath)
	{
		if (pobPath.find("appearance/") == std::string::npos)
			return false;
		if (pobPath.find(".pob") == std::string::npos)
			return false;
		if (pobPath.find("space/") != std::string::npos)
			return false;
		if (pobPath.find("ship_") != std::string::npos)
			return false;
		return true;
	}

	bool templateNameLooksLikeBuilding(char const * templateName)
	{
		if (!templateName || !*templateName)
			return false;
		if (strstr(templateName, "object/building/") == 0 &&
			strstr(templateName, "object/installation/") == 0)
		{
			return false;
		}
		return true;
	}

	std::string makeRoomId(std::string const & donorPob, int cellIndex, int portalIndex)
	{
		std::ostringstream oss;
		oss << "dyn|" << donorPob << '|' << cellIndex << '|' << portalIndex;
		return oss.str();
	}

	std::string makeDisplayName(std::string const & donorPob, char const * cellName, int cellIndex, int portalIndex)
	{
		std::string leaf = donorPob;
		size_t slash = leaf.find_last_of("/\\");
		if (slash != std::string::npos)
			leaf = leaf.substr(slash + 1);
		size_t dot = leaf.rfind('.');
		if (dot != std::string::npos)
			leaf = leaf.substr(0, dot);

		for (size_t i = 0; i < leaf.size(); ++i)
		{
			if (leaf[i] == '_')
				leaf[i] = ' ';
		}

		char buf[320];
		if (cellName && *cellName)
			snprintf(buf, sizeof(buf), "%s / %s (p%d)", leaf.c_str(), cellName, portalIndex);
		else
			snprintf(buf, sizeof(buf), "%s / cell %d (p%d)", leaf.c_str(), cellIndex, portalIndex);
		return buf;
	}

	bool appearancePathIsLoadable(std::string path)
	{
		if (path.empty())
			return false;

		if (path.size() > 4 && path.substr(path.size() - 4) == ".pob")
			path.replace(path.size() - 4, 4, ".apt");

		if (!TreeFile::exists(path.c_str()))
			return false;

		Iff iff;
		return iff.open(path.c_str(), true);
	}

	Appearance * tryCreateAppearance(std::string path)
	{
		if (!appearancePathIsLoadable(path))
			return 0;

		bool found = false;
		AppearanceTemplate const * const tmpl = AppearanceTemplateList::fetch(path.c_str(), found);
		if (!found || !tmpl)
			return 0;

		Appearance * const app = tmpl->createAppearance();
		AppearanceTemplateList::release(tmpl);
		return app;
	}

	struct LayoutMapProjection
	{
		float minX;
		float minZ;
		float span;
		float canvasW;
		float canvasH;
		float drawOriginX;
		float drawOriginY;
		float pixelSpan;

		void mapO2pToCanvas(float x, float z, float & outX, float & outY) const
		{
			float const nx = (x - minX) / span;
			float const nz = (z - minZ) / span;
			outX = drawOriginX + nx * pixelSpan;
			outY = drawOriginY + (1.0f - nz) * pixelSpan;
		}

		static LayoutMapProjection build(
			float boundsMinX,
			float boundsMaxX,
			float boundsMinZ,
			float boundsMaxZ,
			float canvasW,
			float canvasH,
			float padMeters = 2.0f)
		{
			LayoutMapProjection proj;
			proj.canvasW = canvasW;
			proj.canvasH = canvasH;

			float const centerX = (boundsMinX + boundsMaxX) * 0.5f;
			float const centerZ = (boundsMinZ + boundsMaxZ) * 0.5f;
			float const spanX = std::max(1.0f, boundsMaxX - boundsMinX);
			float const spanZ = std::max(1.0f, boundsMaxZ - boundsMinZ);
			proj.span = std::max(spanX, spanZ) + padMeters * 2.0f;
			proj.minX = centerX - proj.span * 0.5f;
			proj.minZ = centerZ - proj.span * 0.5f;

			float const margin = 16.0f;
			float const drawW = std::max(1.0f, canvasW - margin * 2.0f);
			float const drawH = std::max(1.0f, canvasH - margin * 2.0f);
			proj.pixelSpan = std::min(drawW, drawH);
			proj.drawOriginX = margin + (drawW - proj.pixelSpan) * 0.5f;
			proj.drawOriginY = margin + (drawH - proj.pixelSpan) * 0.5f;
			return proj;
		}
	};

	void accumulateCellO2pBounds(CellProperty const * cell, float & minX, float & maxX, float & minZ, float & maxZ, bool & initialized)
	{
		if (!cell)
			return;

		float cMinX = 0.0f;
		float cMaxX = 0.0f;
		float cMinZ = 0.0f;
		float cMaxZ = 0.0f;

		Transform const cellTransform = cell->getOwner().getTransform_o2p();
		Floor const * const floor = cell->getFloor();
		if (floor && floor->getExtent_p())
		{
			AxialBox const ab = floor->getExtent_p()->getBoundingBox();
			bool firstCorner = true;
			for (int corner = 0; corner < 8; ++corner)
			{
				Vector const p = cellTransform.rotateTranslate_l2p(ab.getCorner(corner));
				if (firstCorner)
				{
					cMinX = cMaxX = p.x;
					cMinZ = cMaxZ = p.z;
					firstCorner = false;
				}
				else
				{
					cMinX = std::min(cMinX, p.x);
					cMaxX = std::max(cMaxX, p.x);
					cMinZ = std::min(cMinZ, p.z);
					cMaxZ = std::max(cMaxZ, p.z);
				}
			}
		}
		else
		{
			Vector const p = cellTransform.getPosition_p();
			float const half = 4.0f;
			cMinX = p.x - half;
			cMaxX = p.x + half;
			cMinZ = p.z - half;
			cMaxZ = p.z + half;
		}

		if (!initialized)
		{
			minX = cMinX;
			maxX = cMaxX;
			minZ = cMinZ;
			maxZ = cMaxZ;
			initialized = true;
		}
		else
		{
			minX = std::min(minX, cMinX);
			maxX = std::max(maxX, cMaxX);
			minZ = std::min(minZ, cMinZ);
			maxZ = std::max(maxZ, cMaxZ);
		}
	}

	bool canResolveSocketTransform(PortalProperty const & portalProperty, int cellIndex, int portalIndex)
	{
		if (cellIndex < 1)
			return false;

		if (!portalProperty.getCell(cellIndex))
			return false;

		Transform portalTransform;
		return portalProperty.getPortalSocketTransform_o2p(cellIndex, portalIndex, portalTransform);
	}

	void buildLocalSocketEntries(PortalProperty const & portalProperty, DynamicBunkerOpenFloorplanMessage::SocketList & socketEntries)
	{
		PortalProperty::PortalSocketInfoList sockets;
		portalProperty.collectPortalSockets(sockets);
		socketEntries.clear();
		socketEntries.reserve(sockets.size());

		for (size_t i = 0; i < sockets.size(); ++i)
		{
			PortalProperty::PortalSocketInfo const & socket = sockets[i];
			DynamicBunkerOpenFloorplanMessage::SocketEntry entry;
			entry.cellIndex = socket.cellIndex;
			entry.portalIndex = socket.portalIndex;
			entry.open = socket.open && socket.passable;
			entry.linkedCellIndex = -1;
			entry.linkedPortalIndex = -1;
			entry.custom = PortalProperty::isCustomSocketIndex(socket.portalIndex);
			entry.mapX = 0.0f;
			entry.mapZ = 0.0f;

			PortalProperty::DynamicRoomGraft graft;
			if (portalProperty.findDynamicRoomGraftForSocket(socket.cellIndex, socket.portalIndex, graft))
			{
				entry.open = false;
				entry.linkedCellIndex = graft.graftedCellIndex;
				entry.linkedPortalIndex = graft.graftedPortalIndex;
			}
			else if (!entry.custom)
			{
				int linkedCell = -1;
				int linkedPortal = -1;
				if (portalProperty.getPortalNeighbor(socket.cellIndex, socket.portalIndex, linkedCell, linkedPortal))
				{
					entry.linkedCellIndex = linkedCell;
					entry.linkedPortalIndex = linkedPortal;
					entry.open = false;
				}
			}

			Transform portalTransform;
			if (portalProperty.getPortalSocketTransform_o2p(socket.cellIndex, socket.portalIndex, portalTransform))
			{
				entry.mapX = portalTransform.getPosition_p().x;
				entry.mapZ = portalTransform.getPosition_p().z;
			}

			char label[160];
			if (entry.custom)
			{
				PortalProperty::CustomSocket customSocket;
				if (portalProperty.findCustomSocket(socket.cellIndex, socket.portalIndex, customSocket) && !customSocket.label.empty())
					snprintf(label, sizeof(label), "custom: %s", customSocket.label.c_str());
				else
					snprintf(label, sizeof(label), "custom snap %d", socket.portalIndex);
			}
			else
			{
				snprintf(label, sizeof(label), "cell %d / portal %d", socket.cellIndex, socket.portalIndex);
			}

			if (entry.linkedCellIndex >= 0)
			{
				char linkBuf[96];
				snprintf(linkBuf, sizeof(linkBuf), " -> cell %d / portal %d", entry.linkedCellIndex, entry.linkedPortalIndex);
				strncat(label, linkBuf, sizeof(label) - strlen(label) - 1);
			}
			entry.label = label;
			socketEntries.push_back(entry);
		}
	}

	void expandPob(std::string const & pobPath, DynamicBunkerOpenFloorplanMessage::RoomList & rooms, std::set<std::string> & byId)
	{
		if (!TreeFile::exists(pobPath.c_str()))
			return;

		PortalPropertyTemplate const * const tmpl = PortalPropertyTemplateList::fetch(CrcLowerString(pobPath.c_str()));
		if (!tmpl)
			return;

		int const cellCount = tmpl->getNumberOfCells();
		for (int cellIndex = 1; cellIndex < cellCount; ++cellIndex)
		{
			PortalPropertyTemplateCell const & cell = tmpl->getCell(cellIndex);
			PortalPropertyTemplateCell::PortalPropertyTemplateCellPortalList const * const portals = cell.getPortalList();
			if (!portals || portals->empty())
				continue;

			// One catalog entry per cell (first portal is the graft socket).
			int const portalIndex = 0;
			DynamicBunkerOpenFloorplanMessage::RoomEntry room;
			room.roomId = makeRoomId(pobPath, cellIndex, portalIndex);
			room.displayName = makeDisplayName(pobPath, cell.getName(), cellIndex, portalIndex);
			room.donorPob = pobPath;
			room.appearanceHint = cell.getAppearanceName() ? cell.getAppearanceName() : "";
			if (!room.appearanceHint.empty() && !appearancePathIsLoadable(room.appearanceHint))
				room.appearanceHint.clear();
			room.socketType = "auto";
			room.donorCellIndex = cellIndex;
			room.donorPortalIndex = portalIndex;

			if (byId.insert(room.roomId).second)
				rooms.push_back(room);
		}

		tmpl->release();
	}

	void buildRoomCatalog(DynamicBunkerOpenFloorplanMessage::RoomList & outRooms)
	{
		outRooms.clear();

		stdvector<char const *>::fwd templateNames;
		ObjectTemplateList::getAllTemplateNamesFromCrcStringTable(templateNames);

		std::set<std::string> uniquePobs;
		for (stdvector<char const *>::fwd::const_iterator it = templateNames.begin(); it != templateNames.end(); ++it)
		{
			char const * const name = *it;
			if (!templateNameLooksLikeBuilding(name))
				continue;

			ObjectTemplate const * const ot = ObjectTemplateList::fetch(name);
			if (!ot)
				continue;

			SharedObjectTemplate const * const shared = ot->asSharedObjectTemplate();
			if (shared)
			{
				std::string const & pob = shared->getPortalLayoutFilename();
				if (!pob.empty() && pathLooksUseful(pob) && TreeFile::exists(pob.c_str()))
					uniquePobs.insert(pob);
			}

			ot->releaseReference();
		}

		std::set<std::string> byId;
		outRooms.reserve(uniquePobs.size() * 4);
		for (std::set<std::string>::const_iterator it = uniquePobs.begin(); it != uniquePobs.end(); ++it)
			expandPob(*it, outRooms, byId);

		struct RoomDisplayLess
		{
			bool operator()(DynamicBunkerOpenFloorplanMessage::RoomEntry const & a, DynamicBunkerOpenFloorplanMessage::RoomEntry const & b) const
			{
				return a.displayName < b.displayName;
			}
		};
		std::sort(outRooms.begin(), outRooms.end(), RoomDisplayLess());
	}
}

using namespace SwgCuiDynamicBunkerFloorplanNamespace;

SwgCuiDynamicBunkerFloorplan * SwgCuiDynamicBunkerFloorplan::s_activePlacePointsInstance = 0;
SwgCuiDynamicBunkerFloorplan * SwgCuiDynamicBunkerFloorplan::s_activeFloorplanInstance = 0;

// ----------------------------------------------------------------------

SwgCuiDynamicBunkerFloorplan * SwgCuiDynamicBunkerFloorplan::getActiveInstance()
{
	return s_activeFloorplanInstance;
}

// ======================================================================

SwgCuiDynamicBunkerFloorplan::SwgCuiDynamicBunkerFloorplan(UIPage & page)
: CuiMediator("SwgCuiDynamicBunkerFloorplan", page),
	UIEventCallback(),
	m_buttonAssign(0),
	m_buttonUnassign(0),
	m_buttonCancel(0),
	m_buttonCreatePortal(0),
	m_buttonPlacePoints(0),
	m_listRooms(0),
	m_listSockets(0),
	m_textboxFilter(0),
	m_textboxPlaceCell(0),
	m_textStatus(0),
	m_textRoomName(0),
	m_textSocketHint(0),
	m_textLayoutHint(0),
	m_textPlaceHint(0),
	m_viewer(0),
	m_layoutMapViewer(0),
	m_layoutMapCanvas(0),
	m_tabs(0),
	m_buildingId(),
	m_terminalId(),
	m_selectedCellIndex(0),
	m_selectedPortalIndex(0),
	m_selectedRoomIndex(-1),
	m_selectedSocketRow(-1),
	m_selectedPlaceWall(-1),
	m_placingPortalPoints(false),
	m_placePointCount(0),
	m_placePointsReady(false),
	m_placePoint0_cell(),
	m_placePoint1_cell(),
	m_placePoint0_w(),
	m_placePoint1_w(),
	m_portalDoorTransform_cell(),
	m_placePointMarkers(),
	m_rooms(),
	m_sockets(),
	m_bridges(),
	m_filteredRoomIndices(),
	m_mapNodeButtons(),
	m_mapNodeSocketIndices(),
	m_mapCellOverlays(),
	m_previewObject(0),
	m_needsUiRefresh(false),
	m_suppressPortalRefresh(false),
	m_pendingCreateSnapCellIndex(-1),
	m_trackedPlaceCellIndex(-1),
	m_hasServerRoomCatalog(false),
	m_nextMapButtonId(0),
	m_overlayCallbackPage(0)
{
	for (int i = 0; i < 9; ++i)
		m_placeWallButtons[i] = 0;

	getCodeDataObject(TUIButton, m_buttonAssign, "buttonAssign", true);
	getCodeDataObject(TUIButton, m_buttonUnassign, "buttonUnassign", true);
	getCodeDataObject(TUIButton, m_buttonCancel, "buttonCancel", true);
	getCodeDataObject(TUIButton, m_buttonCreatePortal, "buttonCreatePortal", true);
	getCodeDataObject(TUIButton, m_buttonPlacePoints, "buttonPlacePoints", true);
	getCodeDataObject(TUIList, m_listRooms, "listRooms", true);
	getCodeDataObject(TUIList, m_listSockets, "listSockets", true);
	getCodeDataObject(TUITextbox, m_textboxFilter, "textboxFilter", true);
	getCodeDataObject(TUITextbox, m_textboxPlaceCell, "textboxPlaceCell", true);
	getCodeDataObject(TUIText, m_textStatus, "textStatus", true);
	getCodeDataObject(TUIText, m_textRoomName, "textRoomName", true);
	getCodeDataObject(TUIText, m_textSocketHint, "textSocketHint", true);
	getCodeDataObject(TUIText, m_textLayoutHint, "textLayoutHint", true);
	getCodeDataObject(TUIText, m_textPlaceHint, "textPlaceHint", true);
	getCodeDataObject(TUIPage, m_layoutMapCanvas, "layoutMapCanvas", true);
	getCodeDataObject(TUITabbedPane, m_tabs, "tabs", true);

	UIWidget * viewerWidget = 0;
	getCodeDataObject(TUIWidget, viewerWidget, "viewer", true);
	m_viewer = viewerWidget ? dynamic_cast<CuiWidget3dObjectListViewer *>(viewerWidget) : 0;

	UIWidget * layoutViewerWidget = 0;
	getCodeDataObject(TUIWidget, layoutViewerWidget, "layoutMapViewer", true);
	m_layoutMapViewer = layoutViewerWidget ? dynamic_cast<CuiWidget3dObjectListViewer *>(layoutViewerWidget) : 0;

	if (m_buttonAssign)
		registerWidget(m_buttonAssign);
	if (m_buttonUnassign)
		registerWidget(m_buttonUnassign);
	if (m_buttonCancel)
		registerWidget(m_buttonCancel);
	UIButton * headerClose = 0;
	getCodeDataObject(TUIButton, headerClose, "buttonClose", true);
	if (headerClose)
		registerWidget(headerClose);
	else if (getButtonClose())
		registerWidget(getButtonClose());
	if (m_buttonCreatePortal)
		registerWidget(m_buttonCreatePortal);
	if (m_buttonPlacePoints)
		registerWidget(m_buttonPlacePoints);
	if (m_listRooms)
		registerWidget(m_listRooms);
	if (m_listSockets)
		registerWidget(m_listSockets);
	if (m_textboxFilter)
		registerWidget(m_textboxFilter);
	if (m_textboxPlaceCell)
	{
		m_textboxPlaceCell->SetEnabled(false);
		registerWidget(m_textboxPlaceCell);
	}
	if (m_tabs)
		registerWidget(m_tabs);

	for (int i = 0; i < 9; ++i)
	{
		char name[32];
		snprintf(name, sizeof(name), "placeWall%d", i);
		UIWidget * widget = 0;
		getCodeDataObject(TUIWidget, widget, name, true);
		m_placeWallButtons[i] = dynamic_cast<UIButton *>(widget);
		registerWidget(m_placeWallButtons[i]);
	}

	setState(MS_closeDeactivates);

	ensureListDataSource(m_listRooms, "dsRooms");
	ensureListDataSource(m_listSockets, "dsSockets");
	if (!m_listRooms || !m_listSockets)
		WARNING(true, ("SwgCuiDynamicBunkerFloorplan - list widgets not resolved; check ui_dynamic_bunker_floorplan.inc CodeData paths"));
}

// ----------------------------------------------------------------------

SwgCuiDynamicBunkerFloorplan::~SwgCuiDynamicBunkerFloorplan()
{
	if (s_activeFloorplanInstance == this)
		s_activeFloorplanInstance = 0;
	if (s_activePlacePointsInstance == this)
		s_activePlacePointsInstance = 0;
	cancelPlacePointsMode();
	clearPreviewObject();
	clearFloorMapNodes();
	clearLayoutMapOverlay();
	m_buttonAssign = 0;
	m_buttonUnassign = 0;
	m_buttonCancel = 0;
	m_buttonCreatePortal = 0;
	m_listRooms = 0;
	m_listSockets = 0;
	m_textboxFilter = 0;
	m_textboxPlaceCell = 0;
	m_textStatus = 0;
	m_textRoomName = 0;
	m_textSocketHint = 0;
	m_textLayoutHint = 0;
	m_textPlaceHint = 0;
	m_viewer = 0;
	m_layoutMapViewer = 0;
	m_layoutMapCanvas = 0;
	m_tabs = 0;
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::registerWidget(UIBaseObject * widget)
{
	if (widget)
		registerMediatorObject(*widget, true);
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::performActivate()
{
	s_activeFloorplanInstance = this;
	setIsUpdating(true);
	CuiManager::requestPointer(true);
	setKeyboardInputActive(true);
	getPage().AddCallback(this);

	UIPage * const overlayPage = dynamic_cast<UIPage *>(getPage().GetParent());
	if (overlayPage && overlayPage != &getPage())
	{
		m_overlayCallbackPage = overlayPage;
		m_overlayCallbackPage->AddCallback(this);
	}

	if (m_viewer)
	{
		m_viewer->setPaused(false);
		m_viewer->setCameraForceTarget(true);
		m_viewer->setRotateSpeed(0.35f);
	}
	if (m_layoutMapViewer)
	{
		m_layoutMapViewer->setPaused(false);
		m_layoutMapViewer->setCameraForceTarget(true);
	}

	if (m_needsUiRefresh)
	{
		rebuildSocketsFromLocalBuilding();
		refreshUiFromSession();
	}

	updatePlaceCellDisplay();
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::update(float deltaTimeSecs)
{
	CuiMediator::update(deltaTimeSecs);
	if (!isActive())
		return;

	int const playerCell = resolvePlayerCellIndex();
	if (playerCell != m_trackedPlaceCellIndex)
		updatePlaceCellDisplay();
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::performDeactivate()
{
	setIsUpdating(false);
	if (s_activeFloorplanInstance == this)
		s_activeFloorplanInstance = 0;
	if (s_activePlacePointsInstance == this)
		s_activePlacePointsInstance = 0;
	cancelPlacePointsMode();
	getPage().RemoveCallback(this);
	if (m_overlayCallbackPage)
	{
		m_overlayCallbackPage->RemoveCallback(this);
		m_overlayCallbackPage = 0;
	}
	setKeyboardInputActive(false);
	CuiManager::requestPointer(false);
	clearPreviewObject();
	if (m_viewer)
		m_viewer->setPaused(true);
	if (m_layoutMapViewer)
		m_layoutMapViewer->setPaused(true);
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::buildLocalRoomCatalog()
{
	buildRoomCatalog(m_rooms);
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::setSession(DynamicBunkerOpenFloorplanMessage const & message)
{
	bool const refreshingSameBuilding = (m_buildingId == message.getBuildingId() && m_buildingId.isValid());
	applySessionData(message);
	m_needsUiRefresh = true;
	if (!isActive())
		return;

	rebuildSocketsFromLocalBuilding();
	refreshRoomList();
	refreshSocketList();
	updatePreview();
	updateActionButtons();
	updatePlaceCellDisplay();

	if (refreshingSameBuilding)
	{
		refreshFloorMap();
		if (m_selectedSocketRow >= 0)
			selectSocketByIndex(m_selectedSocketRow);
		return;
	}

	refreshFloorMap();
	m_needsUiRefresh = false;
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::applySessionData(DynamicBunkerOpenFloorplanMessage const & message)
{
	bool const refreshingSameBuilding = (m_buildingId == message.getBuildingId() && m_buildingId.isValid());
	int const preservedRoomIndex = m_selectedRoomIndex;
	std::string preservedFilter;
	if (refreshingSameBuilding && m_textboxFilter)
		preservedFilter = Unicode::wideToNarrow(m_textboxFilter->GetLocalText());

	m_buildingId = message.getBuildingId();
	m_terminalId = message.getTerminalId();
	if (!m_buildingId.isValid())
		m_buildingId = resolveBuildingId();
	if (!m_terminalId.isValid())
		m_terminalId = m_buildingId;
	m_selectedCellIndex = message.getSelectedCellIndex();
	m_selectedPortalIndex = message.getSelectedPortalIndex();
	if (!(refreshingSameBuilding && isActive()))
	{
		m_sockets = message.getSockets();
		m_bridges = message.getBridges();
	}
	else
	{
		rebuildSocketsFromLocalBuilding();
	}

	if (!message.getRooms().empty())
	{
		m_rooms = message.getRooms();
		m_hasServerRoomCatalog = true;
	}
	else if (!refreshingSameBuilding || m_rooms.empty())
	{
		buildLocalRoomCatalog();
		m_hasServerRoomCatalog = false;
	}

	m_selectedRoomIndex = m_rooms.empty() ? -1 : 0;
	if (refreshingSameBuilding && preservedRoomIndex >= 0 && preservedRoomIndex < static_cast<int>(m_rooms.size()))
		m_selectedRoomIndex = preservedRoomIndex;

	m_selectedSocketRow = -1;

	if (m_textboxFilter)
	{
		if (refreshingSameBuilding)
		{
			m_textboxFilter->SetLocalText(Unicode::narrowToWide(preservedFilter));
			m_textboxFilter->SetText(Unicode::narrowToWide(preservedFilter));
		}
		else
		{
			m_textboxFilter->SetLocalText(Unicode::emptyString);
			m_textboxFilter->SetText(Unicode::emptyString);
		}
	}

	for (size_t i = 0; i < m_sockets.size(); ++i)
	{
		if (m_sockets[i].cellIndex == m_selectedCellIndex && m_sockets[i].portalIndex == m_selectedPortalIndex)
		{
			m_selectedSocketRow = static_cast<int>(i);
			break;
		}
	}
	if (m_selectedSocketRow < 0)
	{
		for (size_t i = 0; i < m_sockets.size(); ++i)
		{
			if (m_sockets[i].open)
			{
				m_selectedSocketRow = static_cast<int>(i);
				m_selectedCellIndex = m_sockets[i].cellIndex;
				m_selectedPortalIndex = m_sockets[i].portalIndex;
				break;
			}
		}
	}
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::refreshUiFromSession()
{
	m_needsUiRefresh = false;

	rebuildSocketsFromLocalBuilding();
	refreshRoomList();
	refreshSocketList();
	refreshFloorMap();
	updatePreview();
	updateActionButtons();
	updatePlaceCellDisplay();

	char buf[192];
	snprintf(buf, sizeof(buf), "%d rooms, %d portals. Use Layout Map to pick sockets; Place Portal for custom snaps.",
		static_cast<int>(m_rooms.size()), static_cast<int>(m_sockets.size()));
	updateStatus(buf);
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::notifyBuildingPortalsChanged(NetworkId const & buildingId)
{
	if (!isActive())
		return;

	NetworkId const sessionBuilding = resolveBuildingId();
	if (sessionBuilding != buildingId && m_buildingId != buildingId)
		return;

	int const previousSocketRow = m_selectedSocketRow;
	int const pendingCreateSnapCell = m_pendingCreateSnapCellIndex;
	rebuildSocketsFromLocalBuilding();

	if (m_suppressPortalRefresh)
	{
		m_suppressPortalRefresh = false;
		refreshSocketList();
		updateActionButtons();
		if (pendingCreateSnapCell > 0)
		{
			m_pendingCreateSnapCellIndex = -1;
			for (int i = static_cast<int>(m_sockets.size()) - 1; i >= 0; --i)
			{
				DynamicBunkerOpenFloorplanMessage::SocketEntry const & socket = m_sockets[static_cast<size_t>(i)];
				if (socket.cellIndex == pendingCreateSnapCell
					&& socket.open
					&& PortalProperty::isCustomSocketIndex(socket.portalIndex))
				{
					selectSocketByIndex(i);
					refreshFloorMap();
					char buf[256];
					snprintf(buf, sizeof(buf),
						"Custom snap added in room %d (portal %d). Pick a catalog room and Assign.",
						socket.cellIndex, socket.portalIndex);
					updateStatus(buf);
					return;
				}
			}
		}
		if (previousSocketRow >= 0 && previousSocketRow < static_cast<int>(m_sockets.size()))
			selectSocketByIndex(previousSocketRow);
		else if (!m_sockets.empty())
			selectSocketByIndex(static_cast<int>(m_sockets.size()) - 1);
		refreshFloorMap();
		return;
	}

	refreshSocketList();
	updateActionButtons();
	refreshFloorMap();

	if (pendingCreateSnapCell > 0)
	{
		m_pendingCreateSnapCellIndex = -1;
		for (int i = static_cast<int>(m_sockets.size()) - 1; i >= 0; --i)
		{
			DynamicBunkerOpenFloorplanMessage::SocketEntry const & socket = m_sockets[static_cast<size_t>(i)];
			if (socket.cellIndex == pendingCreateSnapCell
				&& socket.open
				&& PortalProperty::isCustomSocketIndex(socket.portalIndex))
			{
				selectSocketByIndex(i);
				char buf[256];
				snprintf(buf, sizeof(buf),
					"Custom snap added in room %d (portal %d). Pick a catalog room and Assign.",
					socket.cellIndex, socket.portalIndex);
				updateStatus(buf);
				return;
			}
		}
	}

	if (previousSocketRow >= 0 && previousSocketRow < static_cast<int>(m_sockets.size()))
		selectSocketByIndex(previousSocketRow);
	else if (!m_sockets.empty())
		selectSocketByIndex(static_cast<int>(m_sockets.size()) - 1);
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::rebuildSocketsFromLocalBuilding()
{
	Object * const building = NetworkIdManager::getObjectById(resolveBuildingId());
	PortalProperty * const portalProperty = building ? building->getPortalProperty() : 0;
	if (!portalProperty)
	{
		m_sockets.clear();
		m_selectedSocketRow = -1;
		return;
	}

	int const preservedCell = m_selectedCellIndex;
	int const preservedPortal = m_selectedPortalIndex;

	buildLocalSocketEntries(*portalProperty, m_sockets);

	m_selectedSocketRow = -1;
	for (size_t i = 0; i < m_sockets.size(); ++i)
	{
		if (m_sockets[i].cellIndex == preservedCell && m_sockets[i].portalIndex == preservedPortal)
		{
			m_selectedSocketRow = static_cast<int>(i);
			break;
		}
	}
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::updateSocketMapCoordsFromLocalBuilding()
{
	Object * const building = NetworkIdManager::getObjectById(resolveBuildingId());
	PortalProperty * const portalProperty = building ? building->getPortalProperty() : 0;
	if (!portalProperty)
		return;

	for (size_t i = 0; i < m_sockets.size(); ++i)
	{
		DynamicBunkerOpenFloorplanMessage::SocketEntry & socket = m_sockets[i];
		if (!canResolveSocketTransform(*portalProperty, socket.cellIndex, socket.portalIndex))
			continue;

		Transform portalTransform;
		if (portalProperty->getPortalSocketTransform_o2p(socket.cellIndex, socket.portalIndex, portalTransform))
		{
			socket.mapX = portalTransform.getPosition_p().x;
			socket.mapZ = portalTransform.getPosition_p().z;
		}
	}
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::OnButtonPressed(UIWidget * context)
{
	if (context == getButtonClose() || context == m_buttonCancel
		|| (context && context->GetName() == "buttonClose"))
	{
		deactivate();
		return;
	}

	if (context == m_buttonAssign)
		assignSelectedRoom();
	else if (context == m_buttonUnassign)
		unassignSelectedSocket();
	else if (context == m_buttonCreatePortal)
		createCustomSocket();
	else if (context == m_buttonPlacePoints)
		startPlacePointsMode();
	else
	{
		for (int i = 0; i < 9; ++i)
		{
			if (context == m_placeWallButtons[i])
			{
				m_selectedPlaceWall = i;
				for (int j = 0; j < 9; ++j)
				{
					if (m_placeWallButtons[j])
						m_placeWallButtons[j]->SetEnabled(j != 4);
				}
				if (m_placeWallButtons[i])
					m_placeWallButtons[i]->SetEnabled(true);
				char buf[128];
				snprintf(buf, sizeof(buf), "Wall slot %d selected on cell. Click Create Snap to place portal.", i);
				updateStatus(buf);
				return;
			}
		}

		for (size_t i = 0; i < m_mapNodeButtons.size(); ++i)
		{
			if (context == m_mapNodeButtons[i])
			{
				selectSocketByIndex(m_mapNodeSocketIndices[i]);
				return;
			}
		}
	}
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::OnTabbedPaneChanged(UIWidget * context)
{
	UNREF(context);
	updateActionButtons();
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::OnTextboxChanged(UIWidget * context)
{
	if (context == m_textboxFilter)
		refreshRoomList();
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::OnGenericSelectionChanged(UIWidget * context)
{
	if (context == m_listRooms)
	{
		int const listRow = m_listRooms ? m_listRooms->GetLastSelectedRow() : -1;
		m_selectedRoomIndex = catalogIndexFromListRow(listRow);
		updatePreview();
	}
	else if (context == m_listSockets)
	{
		m_selectedSocketRow = m_listSockets ? m_listSockets->GetLastSelectedRow() : -1;
		if (m_selectedSocketRow >= 0 && m_selectedSocketRow < static_cast<int>(m_sockets.size()))
		{
			m_selectedCellIndex = m_sockets[static_cast<size_t>(m_selectedSocketRow)].cellIndex;
			m_selectedPortalIndex = m_sockets[static_cast<size_t>(m_selectedSocketRow)].portalIndex;
			char buf[160];
			snprintf(buf, sizeof(buf), "Snap socket cell %d portal %d (%s).",
				m_selectedCellIndex, m_selectedPortalIndex,
				m_sockets[static_cast<size_t>(m_selectedSocketRow)].open ? "OPEN" : "linked - Replace or Unassign");
			updateStatus(buf);
		}
		updateActionButtons();
	}
}

// ----------------------------------------------------------------------

int SwgCuiDynamicBunkerFloorplan::catalogIndexFromListRow(int listRow) const
{
	if (listRow < 0 || listRow >= static_cast<int>(m_filteredRoomIndices.size()))
		return -1;
	return m_filteredRoomIndices[static_cast<size_t>(listRow)];
}

// ----------------------------------------------------------------------

int SwgCuiDynamicBunkerFloorplan::listRowFromCatalogIndex(int catalogIndex) const
{
	for (size_t i = 0; i < m_filteredRoomIndices.size(); ++i)
	{
		if (m_filteredRoomIndices[i] == catalogIndex)
			return static_cast<int>(i);
	}
	return -1;
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::refreshRoomList()
{
	if (!m_listRooms)
		return;

	ensureListDataSource(m_listRooms, "dsRooms");
	m_listRooms->Clear();
	m_filteredRoomIndices.clear();

	std::string filter;
	if (m_textboxFilter)
		filter = Unicode::wideToNarrow(m_textboxFilter->GetLocalText());

	for (size_t i = 0; i < m_rooms.size(); ++i)
	{
		DynamicBunkerOpenFloorplanMessage::RoomEntry const & room = m_rooms[i];
		if (!containsIgnoreCase(room.displayName, filter) && !containsIgnoreCase(room.donorPob, filter))
			continue;

		m_filteredRoomIndices.push_back(static_cast<int>(i));

		std::string row = room.displayName;
		if (!room.socketType.empty())
		{
			row += "  [";
			row += room.socketType;
			row += "]";
		}

		char nameBuf[32];
		snprintf(nameBuf, sizeof(nameBuf), "r%u", static_cast<unsigned>(m_filteredRoomIndices.size()));
		m_listRooms->AddRow(Unicode::narrowToWide(row), nameBuf);
	}

	int const listRow = listRowFromCatalogIndex(m_selectedRoomIndex);
	if (listRow >= 0)
		m_listRooms->SelectRow(listRow);
	else if (!m_filteredRoomIndices.empty())
	{
		m_selectedRoomIndex = m_filteredRoomIndices[0];
		m_listRooms->SelectRow(0);
	}
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::refreshSocketList()
{
	if (!m_listSockets)
		return;

	ensureListDataSource(m_listSockets, "dsSockets");
	m_listSockets->Clear();
	for (size_t i = 0; i < m_sockets.size(); ++i)
	{
		DynamicBunkerOpenFloorplanMessage::SocketEntry const & socket = m_sockets[i];
		char buf[256];
		if (socket.linkedCellIndex >= 0)
		{
			snprintf(buf, sizeof(buf), "%s  c%d/p%d -> c%d/p%d",
				socket.open ? "[OPEN]" : "[LINKED]",
				socket.cellIndex, socket.portalIndex,
				socket.linkedCellIndex, socket.linkedPortalIndex);
		}
		else
		{
			snprintf(buf, sizeof(buf), "%s  c%d/p%d  %s",
				socket.open ? "[OPEN]" : "[LINKED]",
				socket.cellIndex, socket.portalIndex,
				socket.label.c_str());
		}
		char data[64];
		snprintf(data, sizeof(data), "%d:%d", socket.cellIndex, socket.portalIndex);
		m_listSockets->AddRow(Unicode::narrowToWide(buf), data);
	}

	if (m_selectedSocketRow >= 0 && m_selectedSocketRow < static_cast<int>(m_sockets.size()))
		m_listSockets->SelectRow(m_selectedSocketRow);

	updateActionButtons();
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::clearPreviewObject()
{
	if (m_viewer)
		m_viewer->clearObjects();

	delete m_previewObject;
	m_previewObject = 0;
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::updatePreview()
{
	clearPreviewObject();

	if (m_selectedRoomIndex < 0 || m_selectedRoomIndex >= static_cast<int>(m_rooms.size()))
	{
		if (m_textRoomName)
			m_textRoomName->SetText(Unicode::narrowToWide("Select a room module"));
		return;
	}

	DynamicBunkerOpenFloorplanMessage::RoomEntry const & room = m_rooms[static_cast<size_t>(m_selectedRoomIndex)];
	if (m_textRoomName)
		m_textRoomName->SetText(Unicode::narrowToWide(room.displayName));

	std::string appearance = room.appearanceHint;
	if (appearance.empty())
		appearance = room.donorPob;

	if (appearance.size() > 4 && appearance.substr(appearance.size() - 4) == ".pob")
		appearance.replace(appearance.size() - 4, 4, ".apt");

	Appearance * const app = tryCreateAppearance(appearance);
	if (!app)
	{
		updateStatus(("Preview unavailable for " + appearance).c_str());
		return;
	}

	m_previewObject = new Object;
	m_previewObject->setAppearance(app);
	IGNORE_RETURN(m_previewObject->alter(0.0f));
	m_previewObject->conclude();

	if (m_viewer)
	{
		m_viewer->addObject(*m_previewObject);
		m_viewer->setCameraForceTarget(true);
		m_viewer->recomputeZoom();
		m_viewer->setCameraForceTarget(false);
	}
}

// ----------------------------------------------------------------------

bool SwgCuiDynamicBunkerFloorplan::selectedSocketIsOpen() const
{
	if (m_selectedSocketRow < 0 || m_selectedSocketRow >= static_cast<int>(m_sockets.size()))
		return false;
	return m_sockets[static_cast<size_t>(m_selectedSocketRow)].open;
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::updateActionButtons()
{
	bool const hasSocket = (m_selectedSocketRow >= 0 && m_selectedSocketRow < static_cast<int>(m_sockets.size()));
	bool const open = selectedSocketIsOpen();

	if (m_buttonAssign)
	{
		m_buttonAssign->SetEnabled(hasSocket && m_selectedRoomIndex >= 0);
		m_buttonAssign->SetLocalText(Unicode::narrowToWide(open || !hasSocket ? "Assign" : "Replace"));
		m_buttonAssign->SetText(Unicode::narrowToWide(open || !hasSocket ? "Assign" : "Replace"));
	}
	if (m_buttonUnassign)
		m_buttonUnassign->SetEnabled(hasSocket && !open);
	if (m_buttonCreatePortal)
		m_buttonCreatePortal->SetEnabled(m_placePointsReady || (m_selectedPlaceWall >= 0 && m_selectedPlaceWall != 4));
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::assignSelectedRoom()
{
	if (m_selectedRoomIndex < 0 || m_selectedRoomIndex >= static_cast<int>(m_rooms.size()))
	{
		updateStatus("Select a floorplan room first.");
		return;
	}

	if (m_selectedSocketRow < 0 || m_selectedSocketRow >= static_cast<int>(m_sockets.size()))
	{
		updateStatus("Select a snap point first.");
		return;
	}

	DynamicBunkerOpenFloorplanMessage::SocketEntry const & socket = m_sockets[static_cast<size_t>(m_selectedSocketRow)];
	DynamicBunkerOpenFloorplanMessage::RoomEntry const & room = m_rooms[static_cast<size_t>(m_selectedRoomIndex)];

	DynamicBunkerAssignRoomMessage const msg(
		resolveBuildingId(),
		m_terminalId.isValid() ? m_terminalId : resolveBuildingId(),
		socket.cellIndex,
		socket.portalIndex,
		room.roomId);
	GameNetwork::send(msg, true);

	char buf[256];
	snprintf(buf, sizeof(buf), "%s '%s' on cell %d portal %d...",
		socket.open ? "Assigning" : "Replacing with",
		room.displayName.c_str(), socket.cellIndex, socket.portalIndex);
	updateStatus(buf);
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::unassignSelectedSocket()
{
	if (m_selectedSocketRow < 0 || m_selectedSocketRow >= static_cast<int>(m_sockets.size()))
	{
		updateStatus("Select a linked snap point first.");
		return;
	}

	DynamicBunkerOpenFloorplanMessage::SocketEntry const & socket = m_sockets[static_cast<size_t>(m_selectedSocketRow)];
	if (socket.open)
	{
		updateStatus("That snap point is already open.");
		return;
	}

	DynamicBunkerUnassignRoomMessage const msg(
		resolveBuildingId(),
		m_terminalId.isValid() ? m_terminalId : resolveBuildingId(),
		socket.cellIndex,
		socket.portalIndex);
	GameNetwork::send(msg, true);

	char buf[192];
	snprintf(buf, sizeof(buf), "Unassigning graft on cell %d portal %d...",
		socket.cellIndex, socket.portalIndex);
	updateStatus(buf);
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::updateStatus(char const * text)
{
	if (m_textStatus && text)
		m_textStatus->SetText(Unicode::narrowToWide(text));
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::clearFloorMapNodes()
{
	UIEventCallback * const callback = dynamic_cast<UIEventCallback *>(this);
	for (size_t i = 0; i < m_mapNodeButtons.size(); ++i)
	{
		if (m_mapNodeButtons[i])
		{
			if (callback && isActive() && m_mapNodeButtons[i]->HasCallback(callback))
				m_mapNodeButtons[i]->RemoveCallback(callback);
			unregisterMediatorObject(*m_mapNodeButtons[i]);
			if (m_layoutMapCanvas)
				m_layoutMapCanvas->RemoveChild(m_mapNodeButtons[i]);
			delete m_mapNodeButtons[i];
			m_mapNodeButtons[i] = 0;
		}
	}
	m_mapNodeButtons.clear();
	m_mapNodeSocketIndices.clear();
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::clearLayoutMapOverlay()
{
	for (size_t i = 0; i < m_mapCellOverlays.size(); ++i)
	{
		if (m_mapCellOverlays[i])
		{
			if (m_layoutMapCanvas)
				m_layoutMapCanvas->RemoveChild(m_mapCellOverlays[i]);
			delete m_mapCellOverlays[i];
		}
	}
	m_mapCellOverlays.clear();

	if (m_layoutMapViewer)
		m_layoutMapViewer->clearObjects();
}

// ----------------------------------------------------------------------

NetworkId SwgCuiDynamicBunkerFloorplan::resolveBuildingId() const
{
	if (m_buildingId.isValid())
		return m_buildingId;

	CreatureObject * const player = Game::getPlayerCreature();
	if (!player)
		return NetworkId::cms_invalid;

	Object * const topmost = ContainerInterface::getTopmostContainer(*player, false);
	return topmost ? topmost->getNetworkId() : NetworkId::cms_invalid;
}

// ----------------------------------------------------------------------

NetworkId SwgCuiDynamicBunkerFloorplan::getSessionBuildingId() const
{
	return resolveBuildingId();
}

// ----------------------------------------------------------------------

int SwgCuiDynamicBunkerFloorplan::resolvePlayerCellIndex() const
{
	Object const * const player = Game::getPlayer();
	if (!player)
		return -1;

	CellProperty const * const parentCell = player->getParentCell();
	if (!parentCell || parentCell->isWorldCell())
		return -1;

	return parentCell->getCellIndex();
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::updatePlaceCellDisplay()
{
	int const playerCell = resolvePlayerCellIndex();
	m_trackedPlaceCellIndex = playerCell;

	if (!m_textboxPlaceCell)
		return;

	if (playerCell > 0)
	{
		char cellBuf[32];
		snprintf(cellBuf, sizeof(cellBuf), "%d", playerCell);
		m_textboxPlaceCell->SetLocalText(Unicode::narrowToWide(cellBuf));
		m_textboxPlaceCell->SetText(Unicode::narrowToWide(cellBuf));
	}
	else
	{
		m_textboxPlaceCell->SetLocalText(Unicode::narrowToWide("--"));
		m_textboxPlaceCell->SetText(Unicode::narrowToWide("--"));
	}
}

// ----------------------------------------------------------------------

int SwgCuiDynamicBunkerFloorplan::findSocketIndex(int cellIndex, int portalIndex) const
{
	for (size_t i = 0; i < m_sockets.size(); ++i)
	{
		if (m_sockets[i].cellIndex == cellIndex && m_sockets[i].portalIndex == portalIndex)
			return static_cast<int>(i);
	}
	return -1;
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::createMapButton(int socketIndex, float canvasX, float canvasY)
{
	if (!m_layoutMapCanvas || socketIndex < 0 || socketIndex >= static_cast<int>(m_sockets.size()))
		return;

	DynamicBunkerOpenFloorplanMessage::SocketEntry const & socket = m_sockets[static_cast<size_t>(socketIndex)];

	UIButton * const node = new UIButton;
	char name[32];
	snprintf(name, sizeof(name), "mapNode%d_%d", socketIndex, m_nextMapButtonId++);
	node->SetName(name);
	if (m_buttonAssign && m_buttonAssign->GetStyle())
	{
		UIButtonStyle * const style = dynamic_cast<UIButtonStyle *>(m_buttonAssign->GetStyle());
		if (style)
			node->SetStyle(style);
	}

	char label[64];
	snprintf(label, sizeof(label), "%d:%d", socket.cellIndex, socket.portalIndex);
	node->SetLocalText(Unicode::narrowToWide(label));
	node->SetText(Unicode::narrowToWide(label));

	if (socket.custom)
		node->SetBackgroundColor(UIColor(160, 96, 208, 224));
	else if (socket.open)
		node->SetBackgroundColor(UIColor(224, 192, 64, 224));
	else
		node->SetBackgroundColor(UIColor(64, 192, 96, 224));

	float const nodeHalf = 17.0f;
	node->SetLocation(UIPoint(static_cast<UIScalar>(canvasX - nodeHalf), static_cast<UIScalar>(canvasY - nodeHalf)));
	node->SetSize(UISize(34, 34));
	node->SetEnabled(true);

	m_layoutMapCanvas->AddChild(node);
	registerWidget(node);
	m_mapNodeButtons.push_back(node);
	m_mapNodeSocketIndices.push_back(socketIndex);
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::updateLayoutMapViewer()
{
	if (!m_layoutMapViewer)
		return;

	m_layoutMapViewer->clearObjects();

	Object * const building = NetworkIdManager::getObjectById(resolveBuildingId());
	PortalProperty * const portalProperty = building ? building->getPortalProperty() : 0;
	if (!building || !portalProperty)
		return;

	m_layoutMapViewer->addObject(*building);
	int const cellCount = portalProperty->getNumberOfCells();
	for (int cellIndex = 1; cellIndex < cellCount; ++cellIndex)
	{
		CellProperty * const cell = portalProperty->getCell(cellIndex);
		if (cell)
			m_layoutMapViewer->addObject(cell->getOwner());
	}

	m_layoutMapViewer->setCameraTransformToObj(true);
	m_layoutMapViewer->setCameraYaw(0.0f);
	m_layoutMapViewer->setCameraPitch(1.5707963f);
	m_layoutMapViewer->setCameraForceTarget(true);
	m_layoutMapViewer->recomputeZoom();
	m_layoutMapViewer->setCameraForceTarget(false);
	m_layoutMapViewer->setPaused(false);
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::refreshFloorMap()
{
	if (!m_layoutMapCanvas || m_sockets.empty())
	{
		clearFloorMapNodes();
		clearLayoutMapOverlay();
		if (m_layoutMapViewer)
			m_layoutMapViewer->SetVisible(false);
		return;
	}

	Object * const building = NetworkIdManager::getObjectById(resolveBuildingId());
	if (building && building->getPortalProperty())
		updateSocketMapCoordsFromLocalBuilding();

	clearFloorMapNodes();
	clearLayoutMapOverlay();

	if (m_layoutMapViewer)
	{
		m_layoutMapViewer->SetVisible(false);
		m_layoutMapViewer->setPaused(true);
	}

	float boundsMinX = 0.0f;
	float boundsMaxX = 0.0f;
	float boundsMinZ = 0.0f;
	float boundsMaxZ = 0.0f;
	bool haveBounds = false;

	PortalProperty * const portalProperty = building ? building->getPortalProperty() : 0;
	if (portalProperty)
	{
		int const cellCount = portalProperty->getNumberOfCells();
		for (int cellIndex = 1; cellIndex < cellCount; ++cellIndex)
		{
			CellProperty * const cell = portalProperty->getCell(cellIndex);
			if (!cell)
				continue;

			accumulateCellO2pBounds(cell, boundsMinX, boundsMaxX, boundsMinZ, boundsMaxZ, haveBounds);
		}
	}

	for (size_t i = 0; i < m_sockets.size(); ++i)
	{
		if (!haveBounds)
		{
			boundsMinX = boundsMaxX = m_sockets[i].mapX;
			boundsMinZ = boundsMaxZ = m_sockets[i].mapZ;
			haveBounds = true;
		}
		else
		{
			boundsMinX = std::min(boundsMinX, m_sockets[i].mapX);
			boundsMaxX = std::max(boundsMaxX, m_sockets[i].mapX);
			boundsMinZ = std::min(boundsMinZ, m_sockets[i].mapZ);
			boundsMaxZ = std::max(boundsMaxZ, m_sockets[i].mapZ);
		}
	}

	if (!haveBounds)
		return;

	UIScalar const canvasW = m_layoutMapCanvas->GetWidth();
	UIScalar const canvasH = m_layoutMapCanvas->GetHeight();
	LayoutMapProjection const proj = LayoutMapProjection::build(
		boundsMinX, boundsMaxX, boundsMinZ, boundsMaxZ,
		static_cast<float>(canvasW), static_cast<float>(canvasH));

	if (portalProperty)
	{
		int const cellCount = portalProperty->getNumberOfCells();
		for (int cellIndex = 1; cellIndex < cellCount; ++cellIndex)
		{
			CellProperty * const cell = portalProperty->getCell(cellIndex);
			if (!cell)
				continue;

			float cellMinX = boundsMinX;
			float cellMaxX = boundsMaxX;
			float cellMinZ = boundsMinZ;
			float cellMaxZ = boundsMaxZ;
			bool cellHaveBounds = false;
			accumulateCellO2pBounds(cell, cellMinX, cellMaxX, cellMinZ, cellMaxZ, cellHaveBounds);
			if (!cellHaveBounds)
				continue;

			float tileX0 = 0.0f;
			float tileY0 = 0.0f;
			float tileX1 = 0.0f;
			float tileY1 = 0.0f;
			proj.mapO2pToCanvas(cellMinX, cellMinZ, tileX0, tileY0);
			proj.mapO2pToCanvas(cellMaxX, cellMaxZ, tileX1, tileY1);

			float const left = std::min(tileX0, tileX1);
			float const top = std::min(tileY0, tileY1);
			float const pixelW = std::max(12.0f, fabs(tileX1 - tileX0));
			float const pixelH = std::max(12.0f, fabs(tileY1 - tileY0));

			UIPage * const tile = new UIPage;
			char tileName[32];
			snprintf(tileName, sizeof(tileName), "cellTile%d", cellIndex);
			tile->SetName(tileName);
			tile->SetBackgroundColor(UIColor(48, 80, 96, 180));
			tile->SetLocation(UIPoint(static_cast<UIScalar>(left), static_cast<UIScalar>(top)));
			tile->SetSize(UISize(static_cast<UIScalar>(pixelW), static_cast<UIScalar>(pixelH)));
			m_layoutMapCanvas->AddChild(tile);
			m_mapCellOverlays.push_back(tile);
		}
	}

	std::vector<bool> used(m_sockets.size(), false);
	for (size_t i = 0; i < m_sockets.size(); ++i)
	{
		if (used[i])
			continue;

		DynamicBunkerOpenFloorplanMessage::SocketEntry const & socket = m_sockets[i];
		int const peerIndex = (socket.linkedCellIndex >= 0)
			? findSocketIndex(socket.linkedCellIndex, socket.linkedPortalIndex)
			: -1;

		float pixelX = 0.0f;
		float pixelY = 0.0f;
		proj.mapO2pToCanvas(socket.mapX, socket.mapZ, pixelX, pixelY);

		if (peerIndex >= 0 && peerIndex != static_cast<int>(i) && !used[static_cast<size_t>(peerIndex)])
		{
			DynamicBunkerOpenFloorplanMessage::SocketEntry const & peer = m_sockets[static_cast<size_t>(peerIndex)];
			float peerPixelX = 0.0f;
			float peerPixelY = 0.0f;
			proj.mapO2pToCanvas(peer.mapX, peer.mapZ, peerPixelX, peerPixelY);

			float dx = peerPixelX - pixelX;
			float dy = peerPixelY - pixelY;
			float const len = sqrtf(dx * dx + dy * dy);
			if (len > 4.0f)
			{
				float const normX = dx / len;
				float const normY = dy / len;
				float const pixelOffset = 16.0f;
				createMapButton(static_cast<int>(i), pixelX - normX * pixelOffset, pixelY - normY * pixelOffset);
				createMapButton(peerIndex, peerPixelX + normX * pixelOffset, peerPixelY + normY * pixelOffset);
			}
			else
			{
				createMapButton(static_cast<int>(i), pixelX - 18.0f, pixelY);
				createMapButton(peerIndex, peerPixelX + 18.0f, peerPixelY);
			}

			used[i] = true;
			used[static_cast<size_t>(peerIndex)] = true;
		}
		else
		{
			createMapButton(static_cast<int>(i), pixelX, pixelY);
			used[i] = true;
		}
	}
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::selectSocketByIndex(int socketIndex)
{
	if (socketIndex < 0 || socketIndex >= static_cast<int>(m_sockets.size()))
		return;

	m_selectedSocketRow = socketIndex;
	m_selectedCellIndex = m_sockets[static_cast<size_t>(socketIndex)].cellIndex;
	m_selectedPortalIndex = m_sockets[static_cast<size_t>(socketIndex)].portalIndex;

	if (m_listSockets)
		m_listSockets->SelectRow(socketIndex);

	char buf[256];
	DynamicBunkerOpenFloorplanMessage::SocketEntry const & socket = m_sockets[static_cast<size_t>(socketIndex)];
	if (socket.linkedCellIndex >= 0)
	{
		snprintf(buf, sizeof(buf), "Selected c%d/p%d -> c%d/p%d (%s).",
			socket.cellIndex, socket.portalIndex,
			socket.linkedCellIndex, socket.linkedPortalIndex,
			socket.open ? "open" : "linked");
	}
	else
	{
		snprintf(buf, sizeof(buf), "Selected c%d/p%d (%s).",
			socket.cellIndex, socket.portalIndex,
			socket.open ? "open" : "linked");
	}
	updateStatus(buf);
	updateActionButtons();
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::computeWallTransform(int wallIndex, Transform & outTransform_o2p) const
{
	float const half = 6.0f;
	outTransform_o2p = Transform::identity;

	switch (wallIndex)
	{
	case 0:
		outTransform_o2p.setPosition_p(Vector(-half, 0.0f, -half));
		outTransform_o2p.yaw_l(0.785398f);
		break;
	case 1:
		outTransform_o2p.setPosition_p(Vector(0.0f, 0.0f, -half));
		outTransform_o2p.yaw_l(0.0f);
		break;
	case 2:
		outTransform_o2p.setPosition_p(Vector(half, 0.0f, -half));
		outTransform_o2p.yaw_l(-0.785398f);
		break;
	case 3:
		outTransform_o2p.setPosition_p(Vector(-half, 0.0f, 0.0f));
		outTransform_o2p.yaw_l(1.570796f);
		break;
	case 5:
		outTransform_o2p.setPosition_p(Vector(half, 0.0f, 0.0f));
		outTransform_o2p.yaw_l(-1.570796f);
		break;
	case 6:
		outTransform_o2p.setPosition_p(Vector(-half, 0.0f, half));
		outTransform_o2p.yaw_l(2.35619f);
		break;
	case 7:
		outTransform_o2p.setPosition_p(Vector(0.0f, 0.0f, half));
		outTransform_o2p.yaw_l(3.141592f);
		break;
	case 8:
		outTransform_o2p.setPosition_p(Vector(half, 0.0f, half));
		outTransform_o2p.yaw_l(-2.35619f);
		break;
	default:
		outTransform_o2p.setPosition_p(Vector(0.0f, 0.0f, -half));
		outTransform_o2p.yaw_l(0.0f);
		break;
	}
}

// ----------------------------------------------------------------------

bool SwgCuiDynamicBunkerFloorplan::isClickInsideFloorplanPanel(UIBaseObject const * widget) const
{
	for (UIBaseObject const * current = widget; current; current = current->GetParent())
	{
		if (current == &getPage())
			return true;
		if (current->GetName() == "floorplanWindow")
			return true;
	}
	return false;
}

// ----------------------------------------------------------------------

bool SwgCuiDynamicBunkerFloorplan::handleWorldPlaceClick(int mouseX, int mouseY)
{
	if (!m_placingPortalPoints)
		return false;

	static float s_lastHandledClickTime = -1.0f;
	float const now = Game::getElapsedTime();
	if (now - s_lastHandledClickTime < 0.05f)
		return true;
	s_lastHandledClickTime = now;

	Vector worldPoint;
	NetworkId cellNetworkId;
	if (!pickWorldPointFromScreen(mouseX, mouseY, worldPoint, cellNetworkId))
	{
		updateStatus("Could not pick a surface. Aim at the wall or floor and click again.");
		return false;
	}

	addPlacePoint(worldPoint, cellNetworkId);
	return true;
}

// ----------------------------------------------------------------------

bool SwgCuiDynamicBunkerFloorplan::OnMessage(UIWidget * /*context*/, UIMessage const & msg)
{
	if (msg.Type == UIMessage::KeyDown && msg.Keystroke == UIMessage::Escape && m_placingPortalPoints)
	{
		cancelPlacePointsMode();
		updateStatus("Portal point placement cancelled.");
		return false;
	}

	if (m_placingPortalPoints && msg.Type == UIMessage::LeftMouseDown)
	{
		UIPage * const hitPage = m_overlayCallbackPage ? m_overlayCallbackPage : &getPage();
		UIWidget * const hit = hitPage->GetWidgetFromPoint(msg.MouseCoords, false);
		if (!isClickInsideFloorplanPanel(hit))
		{
			UIPoint const cursorPt = UIManager::gUIManager().GetLastMouseCoord();
			if (handleWorldPlaceClick(static_cast<int>(cursorPt.x), static_cast<int>(cursorPt.y)))
				return false;
		}
	}

	return true;
}

// ----------------------------------------------------------------------

bool SwgCuiDynamicBunkerFloorplan::tryConsumeWorldPlaceClick()
{
	if (!s_activePlacePointsInstance || !s_activePlacePointsInstance->isActive())
		return false;

	if (!s_activePlacePointsInstance->m_placingPortalPoints)
		return false;

	UIPoint const cursorPt = UIManager::gUIManager().GetLastMouseCoord();
	return s_activePlacePointsInstance->handleWorldPlaceClick(static_cast<int>(cursorPt.x), static_cast<int>(cursorPt.y));
}

// ----------------------------------------------------------------------

int SwgCuiDynamicBunkerFloorplan::resolvePlaceCellIndex() const
{
	int const playerCell = resolvePlayerCellIndex();
	if (playerCell > 0)
		return playerCell;

	if (m_trackedPlaceCellIndex > 0)
		return m_trackedPlaceCellIndex;

	return m_selectedCellIndex > 0 ? m_selectedCellIndex : 1;
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::clearPlacePointMarkers()
{
	for (size_t i = 0; i < m_placePointMarkers.size(); ++i)
	{
		Object * const marker = m_placePointMarkers[i];
		if (!marker)
			continue;
		if (marker->isInWorld())
			marker->removeFromWorld();
		delete marker;
	}
	m_placePointMarkers.clear();
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::cancelPlacePointsMode()
{
	bool const wasPlacing = m_placingPortalPoints;
	m_placingPortalPoints = false;
	m_placePointCount = 0;
	m_placePointsReady = false;
	if (s_activePlacePointsInstance == this)
		s_activePlacePointsInstance = 0;
	clearPlacePointMarkers();
	if (m_buttonPlacePoints)
		m_buttonPlacePoints->SetEnabled(true);
	if (wasPlacing && isActive())
		CuiManager::setPointerToggledOn(CuiPreferences::getMouseModeDefault());
	updateActionButtons();
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::startPlacePointsMode()
{
	cancelPlacePointsMode();
	updatePlaceCellDisplay();

	int const cellIndex = resolvePlaceCellIndex();
	if (cellIndex < 1)
	{
		updateStatus("Stand inside a building room before placing doorway corners.");
		return;
	}

	m_placingPortalPoints = true;
	s_activePlacePointsInstance = this;
	CuiManager::setPointerToggledOn(false);
	if (m_buttonPlacePoints)
		m_buttonPlacePoints->SetEnabled(false);
	updateStatus("Click the top-left corner of the doorway in the world, then the bottom-right.");
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::updatePlacePointVisuals()
{
	clearPlacePointMarkers();

	if (m_placePointCount >= 1)
	{
		Object * const marker = createPlacePointMarker(m_placePoint0_w);
		if (marker)
			m_placePointMarkers.push_back(marker);
	}
	if (m_placePointCount >= 2)
	{
		Object * const marker = createPlacePointMarker(m_placePoint1_w);
		if (marker)
			m_placePointMarkers.push_back(marker);
	}
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::computePortalTransformFromCorners(
	Vector const & corner0_cell,
	Vector const & corner1_cell,
	Transform & outTransform_cell,
	float & outDoorwayWidth,
	float & outDoorwayHeight)
{
	float const minX = std::min(corner0_cell.x, corner1_cell.x);
	float const maxX = std::max(corner0_cell.x, corner1_cell.x);
	float const minY = std::min(corner0_cell.y, corner1_cell.y);
	float const maxY = std::max(corner0_cell.y, corner1_cell.y);
	float const minZ = std::min(corner0_cell.z, corner1_cell.z);
	float const maxZ = std::max(corner0_cell.z, corner1_cell.z);

	Vector const center((minX + maxX) * 0.5f, (minY + maxY) * 0.5f, (minZ + maxZ) * 0.5f);
	float const spanX = maxX - minX;
	float const spanZ = maxZ - minZ;
	outDoorwayHeight = std::max(0.5f, maxY - minY);

	Vector widthDir;
	Vector portalNormal;

	if (spanX >= spanZ)
	{
		outDoorwayWidth = std::max(0.5f, spanX);
		widthDir = Vector(1.0f, 0.0f, 0.0f);
		portalNormal = Vector(0.0f, 0.0f, 1.0f);
	}
	else
	{
		outDoorwayWidth = std::max(0.5f, spanZ);
		widthDir = Vector(0.0f, 0.0f, 1.0f);
		portalNormal = Vector(1.0f, 0.0f, 0.0f);
	}

	Object const * const player = Game::getPlayer();
	if (player)
	{
		CellProperty const * const playerCell = player->getParentCell();
		if (playerCell && !playerCell->isWorldCell())
		{
			Vector const playerPos_cell = playerCell->getOwner().rotateTranslate_w2o(player->getPosition_w());
			Vector toPlayer = playerPos_cell - center;
			toPlayer.y = 0.0f;
			if (toPlayer.normalize() > 0.01f && portalNormal.dot(toPlayer) < 0.0f)
				portalNormal = -portalNormal;
		}
	}

	float const yaw = snapYawToCardinal45(atan2f(portalNormal.x, portalNormal.z));
	portalNormal = Vector(sinf(yaw), 0.0f, cosf(yaw));
	widthDir = Vector(portalNormal.z, 0.0f, -portalNormal.x);
	if (widthDir.normalize() < 0.01f)
		widthDir = Vector(1.0f, 0.0f, 0.0f);

	Vector const up(0.0f, 1.0f, 0.0f);
	outTransform_cell.setLocalFrameIJK_p(widthDir, up, portalNormal);
	outTransform_cell.setPosition_p(center);
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::computePortalTransformFromPlacePoints()
{
	float doorwayWidth = 1.0f;
	float doorwayHeight = 2.0f;
	computePortalTransformFromCorners(m_placePoint0_cell, m_placePoint1_cell, m_portalDoorTransform_cell, doorwayWidth, doorwayHeight);
	m_placePointsReady = true;
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::addPlacePoint(Vector const & worldPoint, NetworkId const & cellNetworkId)
{
	if (!m_placingPortalPoints)
		return;

	int const cellIndex = resolvePlaceCellIndex();
	if (cellIndex < 1)
	{
		updateStatus("Stand inside a building room before placing points.");
		return;
	}

	Object * const building = NetworkIdManager::getObjectById(resolveBuildingId());
	PortalProperty * const portalProperty = building ? building->getPortalProperty() : 0;
	if (!portalProperty)
	{
		updateStatus("Could not resolve building portal layout.");
		return;
	}

	CellProperty * const cellProperty = portalProperty->getCell(cellIndex);
	CellObject * const cellObject = cellProperty ? CellObject::asCellObject(&cellProperty->getOwner()) : 0;
	if (!cellObject)
	{
		updateStatus("Target cell is not loaded. Stand in that cell and try again.");
		return;
	}

	if (cellNetworkId.isValid() && cellNetworkId != cellObject->getNetworkId())
	{
		// Allow picks on building shell geometry that reports the world cell.
		CellProperty const * const playerCell = Game::getPlayer() ? Game::getPlayer()->getParentCell() : 0;
		if (!playerCell || playerCell->getCellIndex() != cellIndex)
		{
			updateStatus("Click a point on the target cell's walls or floor.");
			return;
		}
	}

	Vector const point_cell = cellObject->rotateTranslate_w2o(worldPoint);

	if (m_placePointCount == 0)
	{
		m_placePoint0_w = worldPoint;
		m_placePoint0_cell = point_cell;
		m_placePointCount = 1;
		m_placePointsReady = false;
		updatePlacePointVisuals();
		updateStatus("First corner set. Click the opposite doorway corner in the world.");
		return;
	}

	if (m_placePointCount == 1)
	{
		m_placePoint1_w = worldPoint;
		m_placePoint1_cell = point_cell;
		m_placePointCount = 2;
		computePortalTransformFromPlacePoints();
		updatePlacePointVisuals();
		m_placingPortalPoints = false;
		if (s_activePlacePointsInstance == this)
			s_activePlacePointsInstance = 0;
		if (isActive())
			CuiManager::setPointerToggledOn(CuiPreferences::getMouseModeDefault());
		if (m_buttonPlacePoints)
			m_buttonPlacePoints->SetEnabled(true);
		updateActionButtons();
		updateStatus("Both corners set. Click Create Snap to add the custom portal socket.");
		return;
	}

	// Restart placement if a third click arrives.
	m_placePoint0_w = worldPoint;
	m_placePoint0_cell = point_cell;
	m_placePoint1_w = Vector();
	m_placePoint1_cell = Vector();
	m_placePointCount = 1;
	m_placePointsReady = false;
	m_placingPortalPoints = true;
	s_activePlacePointsInstance = this;
	updatePlacePointVisuals();
	updateStatus("Restarting placement. Click the opposite doorway corner in the world.");
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::createCustomSocket()
{
	if (!m_placePointsReady)
	{
		updateStatus("Click Place Points and mark two doorway corners in the world first.");
		return;
	}

	NetworkId const buildingId = resolveBuildingId();
	NetworkId const terminalId = m_terminalId.isValid() ? m_terminalId : buildingId;
	if (!buildingId.isValid())
	{
		updateStatus("Could not resolve the building. Stand inside the POB and reopen the floorplan.");
		return;
	}

	int const cellIndex = resolvePlaceCellIndex();
	if (cellIndex < 1)
	{
		updateStatus("Stand inside a building room before creating a snap.");
		return;
	}

	float doorwayWidth = 1.0f;
	float doorwayHeight = 2.0f;
	computePortalTransformFromCorners(m_placePoint0_cell, m_placePoint1_cell, m_portalDoorTransform_cell, doorwayWidth, doorwayHeight);
	Transform const doorTransform = m_portalDoorTransform_cell;
	char label[64];
	snprintf(label, sizeof(label), "custom_%d_%d", cellIndex, static_cast<int>(m_sockets.size()));

	cancelPlacePointsMode();

	m_suppressPortalRefresh = true;
	m_pendingCreateSnapCellIndex = cellIndex;

	DynamicBunkerCreateCustomSocketMessage const msg(
		buildingId,
		terminalId,
		cellIndex,
		doorTransform,
		label,
		doorwayWidth,
		doorwayHeight);
	GameNetwork::send(msg, true);

	char buf[192];
	snprintf(buf, sizeof(buf), "Creating custom snap in room %d...", cellIndex);
	updateStatus(buf);
}

// ======================================================================
