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
#include "sharedCollision/FloorMesh.h"
#include "sharedCollision/BaseExtent.h"
#include "sharedFile/Iff.h"
#include "sharedMath/AxialBox.h"
#include "sharedMath/Triangle3d.h"
#include "sharedObject/Appearance.h"
#include "sharedObject/AppearanceTemplate.h"
#include "sharedObject/AppearanceTemplateList.h"
#include "sharedObject/Object.h"
#include "sharedObject/ObjectTemplateList.h"
#include "sharedObject/PortalPropertyTemplateList.h"
#include "clientGame/GameNetwork.h"
#include "clientGraphics/RenderWorld.h"
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
#include <limits>
#include <map>
#include <set>
#include <sstream>

namespace SwgCuiDynamicBunkerFloorplanNamespace
{
	char const * const s_placePointMarkerAppearance = "appearance/godclient_space_waypoint_generic.apt";
	char const * const s_placePointMarkerAppearanceFallback = "appearance/godclient_space_waypoint_patrol.apt";

	Object * createPlacePointMarker(Vector const & worldPos, CellProperty * parentCell)
	{
		Appearance * app = AppearanceTemplateList::createAppearance(s_placePointMarkerAppearance);
		if (!app)
			app = AppearanceTemplateList::createAppearance(s_placePointMarkerAppearanceFallback);
		if (!app)
			return 0;

		Object * const marker = new Object;
		marker->setAppearance(app);
		marker->setScale(Vector(1.0f, 1.0f, 1.0f));

		CellProperty * cell = parentCell;
		if (!cell)
		{
			Object const * const player = Game::getPlayer();
			if (player)
			cell = const_cast<CellProperty *>(player->getParentCell());
		}
		if (cell)
			marker->setParentCell(cell);

		marker->setPosition_w(worldPos);
		RenderWorld::addObjectNotifications(*marker);
		marker->addToWorld();
		IGNORE_RETURN(marker->alter(0.0f));
		marker->conclude();
		return marker;
	}

	bool pickWorldPointFromScreen(int mouseX, int mouseY, Vector & outWorldPoint, NetworkId & outCellNetworkId, Vector * outWorldNormal = 0)
	{
		outCellNetworkId = NetworkId::cms_invalid;
		if (outWorldNormal)
			*outWorldNormal = Vector::zero;

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
		if (outWorldNormal)
			*outWorldNormal = info.getNormal();

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

		Object const & cellOwner = cell->getOwner();
		if (!cellOwner.getPortalProperty())
			return;

		float cMinX = 0.0f;
		float cMaxX = 0.0f;
		float cMinZ = 0.0f;
		float cMaxZ = 0.0f;

		Transform const cellTransform = cellOwner.getTransform_o2p();
		Floor const * const floor = cell->getFloor();
		BaseExtent const * const extent = floor ? floor->getExtent_p() : 0;
		if (extent)
		{
			AxialBox const ab = extent->getBoundingBox();
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
		if (cellIndex < 1 || cellIndex >= portalProperty.getNumberOfCells())
			return false;

		CellProperty const * const cell = portalProperty.getCell(cellIndex);
		if (!cell)
			return false;

		Object const & cellOwner = cell->getOwner();
		if (!cellOwner.isInitialized() || cellOwner.getCellProperty() != cell)
			return false;

		Transform portalTransform;
		return portalProperty.getPortalSocketTransform_o2p(cellIndex, portalIndex, portalTransform);
	}

	enum SocketFlowRole
	{
		SocketFlowOpen,
		SocketFlowOut,
		SocketFlowIn,
		SocketFlowLinked
	};

	SocketFlowRole getSocketFlowRole(PortalProperty const * portalProperty, DynamicBunkerOpenFloorplanMessage::SocketEntry const & socket)
	{
		if (portalProperty)
		{
			PortalProperty::DynamicRoomGraft graft;
			if (portalProperty->findDynamicRoomGraftForSocket(socket.cellIndex, socket.portalIndex, graft))
			{
				if (graft.hostCellIndex == socket.cellIndex && graft.hostPortalIndex == socket.portalIndex)
					return SocketFlowOut;
				return SocketFlowIn;
			}
		}

		if (socket.open)
			return SocketFlowOpen;

		if (socket.linkedCellIndex >= 0)
			return SocketFlowLinked;

		return SocketFlowOpen;
	}

	bool isSocketHostSide(PortalProperty const * portalProperty, DynamicBunkerOpenFloorplanMessage::SocketEntry const & socket)
	{
		SocketFlowRole const role = getSocketFlowRole(portalProperty, socket);
		return role == SocketFlowOpen || role == SocketFlowOut;
	}

	char const * getSocketFlowTag(SocketFlowRole role)
	{
		switch (role)
		{
		case SocketFlowOpen:   return "OPEN";
		case SocketFlowOut:    return "OUT";
		case SocketFlowIn:     return "IN";
		case SocketFlowLinked: return "LINK";
		default:               return "?";
		}
	}

	void resolveGraftLinkEndpoints(
		PortalProperty const * portalProperty,
		DynamicBunkerOpenFloorplanMessage::SocketEntry const & socketA,
		DynamicBunkerOpenFloorplanMessage::SocketEntry const & socketB,
		int socketIndexA,
		int socketIndexB,
		int & outSocketIndex,
		int & inSocketIndex)
	{
		outSocketIndex = socketIndexA;
		inSocketIndex = socketIndexB;

		if (!portalProperty)
			return;

		PortalProperty::DynamicRoomGraft graft;
		if (portalProperty->findDynamicRoomGraftForSocket(socketA.cellIndex, socketA.portalIndex, graft))
		{
			if (graft.graftedCellIndex == socketA.cellIndex)
			{
				outSocketIndex = socketIndexB;
				inSocketIndex = socketIndexA;
			}
			return;
		}

		if (portalProperty->findDynamicRoomGraftForSocket(socketB.cellIndex, socketB.portalIndex, graft)
			&& graft.graftedCellIndex == socketB.cellIndex)
		{
			outSocketIndex = socketIndexA;
			inSocketIndex = socketIndexB;
		}
	}

	void resolveSocketMapCoordinates(PortalProperty const & portalProperty, DynamicBunkerOpenFloorplanMessage::SocketList & sockets)
	{
		for (size_t i = 0; i < sockets.size(); ++i)
		{
			DynamicBunkerOpenFloorplanMessage::SocketEntry & entry = sockets[i];
			if (isfinite(entry.mapX) && isfinite(entry.mapZ))
				continue;

			Transform portalTransform;
			if (portalProperty.getPortalSocketTransform_o2p(entry.cellIndex, entry.portalIndex, portalTransform))
			{
				entry.mapX = portalTransform.getPosition_p().x;
				entry.mapZ = portalTransform.getPosition_p().z;
				continue;
			}

			if (PortalProperty::isCustomSocketIndex(entry.portalIndex))
			{
				PortalProperty::CustomSocket customSocket;
				CellProperty const * const cell = portalProperty.getCell(entry.cellIndex);
				if (cell && portalProperty.findCustomSocket(entry.cellIndex, entry.portalIndex, customSocket))
				{
					Transform portalBuilding;
					portalBuilding.multiply(cell->getOwner().getTransform_o2p(), customSocket.doorTransform_o2p);
					entry.mapX = portalBuilding.getPosition_p().x;
					entry.mapZ = portalBuilding.getPosition_p().z;
					continue;
				}
			}

			CellProperty const * const cell = portalProperty.getCell(entry.cellIndex);
			if (cell)
			{
				Object const & cellOwner = cell->getOwner();
				if (cellOwner.isInitialized() && cellOwner.getCellProperty() == cell)
				{
					Vector const cellPos = cellOwner.getTransform_o2p().getPosition_p();
					entry.mapX = cellPos.x;
					entry.mapZ = cellPos.z;
				}
			}
		}
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
			entry.mapX = std::numeric_limits<float>::quiet_NaN();
			entry.mapZ = std::numeric_limits<float>::quiet_NaN();

			PortalProperty::DynamicRoomGraft graft;
			bool const hasGraft = portalProperty.findDynamicRoomGraftForSocket(socket.cellIndex, socket.portalIndex, graft);
			if (hasGraft)
			{
				entry.open = false;
				if (graft.hostCellIndex == socket.cellIndex && graft.hostPortalIndex == socket.portalIndex)
				{
					entry.linkedCellIndex = graft.graftedCellIndex;
					entry.linkedPortalIndex = graft.graftedPortalIndex;
				}
				else
				{
					entry.linkedCellIndex = graft.hostCellIndex;
					entry.linkedPortalIndex = graft.hostPortalIndex;
				}
			}
			else if (!entry.custom && canResolveSocketTransform(portalProperty, socket.cellIndex, socket.portalIndex))
			{
				int linkedCell = -1;
				int linkedPortal = -1;
				if (portalProperty.getPortalNeighbor(socket.cellIndex, socket.portalIndex, linkedCell, linkedPortal)
					&& portalProperty.getCell(linkedCell))
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
			else
			{
				CellProperty const * const cell = portalProperty.getCell(socket.cellIndex);
				if (cell)
				{
					Object const & cellOwner = cell->getOwner();
					if (cellOwner.isInitialized() && cellOwner.getCellProperty() == cell)
					{
						Vector const cellPos = cellOwner.getTransform_o2p().getPosition_p();
						entry.mapX = cellPos.x;
						entry.mapZ = cellPos.z;
					}
				}
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
				if (hasGraft)
				{
					if (graft.hostCellIndex == socket.cellIndex && graft.hostPortalIndex == socket.portalIndex)
						snprintf(linkBuf, sizeof(linkBuf), " -> IN c%d/p%d", entry.linkedCellIndex, entry.linkedPortalIndex);
					else
						snprintf(linkBuf, sizeof(linkBuf), " -> OUT c%d/p%d", entry.linkedCellIndex, entry.linkedPortalIndex);
				}
				else
				{
					snprintf(linkBuf, sizeof(linkBuf), " -> c%d/p%d", entry.linkedCellIndex, entry.linkedPortalIndex);
				}
				strncat(label, linkBuf, sizeof(label) - strlen(label) - 1);
			}
			else if (entry.custom)
			{
				strncat(label, " (OUT snap)", sizeof(label) - strlen(label) - 1);
			}
			entry.label = label;
			socketEntries.push_back(entry);
		}

		resolveSocketMapCoordinates(portalProperty, socketEntries);
	}

	void createMapCellOverlays(
		UIPage * canvas,
		std::vector<UIPage *> & overlays,
		PortalProperty const & portalProperty,
		LayoutMapProjection const & proj)
	{
		if (!canvas)
			return;

		int const cellCount = portalProperty.getNumberOfCells();
		for (int cellIndex = 1; cellIndex < cellCount; ++cellIndex)
		{
			CellProperty const * const cell = portalProperty.getCell(cellIndex);
			if (!cell)
				continue;

			float cMinX = 0.0f;
			float cMaxX = 0.0f;
			float cMinZ = 0.0f;
			float cMaxZ = 0.0f;
			bool haveCellBounds = false;
			accumulateCellO2pBounds(cell, cMinX, cMaxX, cMinZ, cMaxZ, haveCellBounds);
			if (!haveCellBounds)
				continue;

			float px0 = 0.0f;
			float py0 = 0.0f;
			float px1 = 0.0f;
			float py1 = 0.0f;
			proj.mapO2pToCanvas(cMinX, cMinZ, px0, py1);
			proj.mapO2pToCanvas(cMaxX, cMaxZ, px1, py0);

			float const left = std::min(px0, px1);
			float const top = std::min(py0, py1);
			float const width = std::max(4.0f, std::abs(px1 - px0));
			float const height = std::max(4.0f, std::abs(py1 - py0));

			UIPage * cellOverlay = new UIPage;
			cellOverlay->SetName("mapCell");
			if (portalProperty.isGraftedCell(cellIndex))
				cellOverlay->SetBackgroundColor(UIColor(48, 112, 72, 96));
			else
				cellOverlay->SetBackgroundColor(UIColor(48, 72, 96, 96));
			cellOverlay->SetLocation(UIPoint(static_cast<UIScalar>(left), static_cast<UIScalar>(top)));
			cellOverlay->SetSize(UISize(static_cast<UIScalar>(width), static_cast<UIScalar>(height)));
			cellOverlay->SetVisible(true);
			cellOverlay->SetEnabled(false);
			cellOverlay->SetGetsInput(false);
			canvas->AddChild(cellOverlay);
			cellOverlay->Link();
			overlays.push_back(cellOverlay);
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
	m_mapNodeTemplate(0),
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
	m_placeWallNormal_cell(),
	m_hasPlaceWallNormal(false),
	m_portalDoorTransform_cell(),
	m_placeDoorwayWidth(1.0f),
	m_placeDoorwayHeight(2.0f),
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
	m_deferredFloorMapRefresh(false),
	m_deferredPortalNotifyBuildingId(),
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
	getCodeDataObject(TUIButton, m_mapNodeTemplate, "mapNodeTemplate", true);
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
	m_mapNodeTemplate = 0;
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

	if (m_deferredPortalNotifyBuildingId.isValid())
	{
		NetworkId const buildingId = m_deferredPortalNotifyBuildingId;
		m_deferredPortalNotifyBuildingId = NetworkId::cms_invalid;
		notifyBuildingPortalsChanged(buildingId);
	}

	if (m_deferredFloorMapRefresh)
	{
		m_deferredFloorMapRefresh = false;
		if (m_tabs && m_tabs->GetActiveTab() == 1)
			refreshFloorMap();
	}

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
	clearFloorMapNodes();
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
		requestFloorMapRefresh();
		if (m_selectedSocketRow >= 0)
			selectSocketByIndex(m_selectedSocketRow);
		return;
	}

	requestFloorMapRefresh();
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
	requestFloorMapRefresh();
	updatePreview();
	updateActionButtons();
	updatePlaceCellDisplay();

	char buf[192];
	snprintf(buf, sizeof(buf), "%d rooms, %d portals. Use Layout Map to pick sockets; Place Portal for custom snaps.",
		static_cast<int>(m_rooms.size()), static_cast<int>(m_sockets.size()));
	updateStatus(buf);
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::deferPortalNotify(NetworkId const & buildingId)
{
	m_deferredPortalNotifyBuildingId = buildingId;
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::notifyBuildingPortalsChanged(NetworkId const & buildingId)
{
	if (!isActive())
		return;

	NetworkId const sessionBuilding = resolveBuildingId();
	if (sessionBuilding != buildingId && m_buildingId != buildingId)
		return;

	Object * const buildingObject = NetworkIdManager::getObjectById(sessionBuilding);
	PortalProperty * const portalProperty = buildingObject ? buildingObject->getPortalProperty() : 0;
	if (buildingObject && portalProperty)
	{
		DynamicBunkerClient::tryLinkAllPendingGrafts(*buildingObject, *portalProperty);
		DynamicBunkerClient::finalizeBuildingPortalChanges(*buildingObject, *portalProperty);
	}

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
					requestFloorMapRefresh();
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
		requestFloorMapRefresh();
		return;
	}

	refreshSocketList();
	updateActionButtons();
	requestFloorMapRefresh();

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
	if (m_tabs && m_tabs->GetActiveTab() == 1)
	{
		rebuildSocketsFromLocalBuilding();
		requestFloorMapRefresh();
	}
	else
		clearFloorMapNodes();
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::requestFloorMapRefresh()
{
	m_deferredFloorMapRefresh = true;
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
			selectSocketByIndex(m_selectedSocketRow);
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

	Object const * const building = NetworkIdManager::getObjectById(resolveBuildingId());
	PortalProperty const * const portalProperty = building ? building->getPortalProperty() : 0;

	ensureListDataSource(m_listSockets, "dsSockets");
	m_listSockets->Clear();
	for (size_t i = 0; i < m_sockets.size(); ++i)
	{
		DynamicBunkerOpenFloorplanMessage::SocketEntry const & socket = m_sockets[i];
		SocketFlowRole const role = getSocketFlowRole(portalProperty, socket);
		char buf[256];
		if (socket.linkedCellIndex >= 0)
		{
			snprintf(buf, sizeof(buf), "[%s] c%d/p%d -> c%d/p%d",
				getSocketFlowTag(role),
				socket.cellIndex, socket.portalIndex,
				socket.linkedCellIndex, socket.linkedPortalIndex);
		}
		else
		{
			snprintf(buf, sizeof(buf), "[%s] c%d/p%d  %s",
				getSocketFlowTag(role),
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

	Object const * const building = NetworkIdManager::getObjectById(resolveBuildingId());
	PortalProperty const * const portalProperty = building ? building->getPortalProperty() : 0;
	bool const hostSide = hasSocket && isSocketHostSide(portalProperty, m_sockets[static_cast<size_t>(m_selectedSocketRow)]);

	if (m_buttonAssign)
	{
		m_buttonAssign->SetEnabled(hostSide && m_selectedRoomIndex >= 0);
		m_buttonAssign->SetLocalText(Unicode::narrowToWide(open || !hasSocket ? "Assign" : "Replace"));
		m_buttonAssign->SetText(Unicode::narrowToWide(open || !hasSocket ? "Assign" : "Replace"));
	}
	if (m_buttonUnassign)
		m_buttonUnassign->SetEnabled(hostSide && !open);
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
		updateStatus("Select an OUT portal (yellow OPEN or orange OUT) first.");
		return;
	}

	Object const * const building = NetworkIdManager::getObjectById(resolveBuildingId());
	PortalProperty const * const portalProperty = building ? building->getPortalProperty() : 0;
	DynamicBunkerOpenFloorplanMessage::SocketEntry const & socket = m_sockets[static_cast<size_t>(m_selectedSocketRow)];
	if (!isSocketHostSide(portalProperty, socket))
	{
		updateStatus("Select the OUT (bunker) side of the portal, not the IN graft entrance.");
		return;
	}
	DynamicBunkerOpenFloorplanMessage::RoomEntry const & room = m_rooms[static_cast<size_t>(m_selectedRoomIndex)];

	DynamicBunkerAssignRoomMessage const msg(
		resolveBuildingId(),
		m_terminalId.isValid() ? m_terminalId : resolveBuildingId(),
		socket.cellIndex,
		socket.portalIndex,
		room.roomId);
	GameNetwork::send(msg, true);

	char buf[256];
	snprintf(buf, sizeof(buf), "%s '%s' at OUT portal c%d/p%d...",
		socket.open ? "Assigning" : "Replacing with",
		room.displayName.c_str(), socket.cellIndex, socket.portalIndex);
	updateStatus(buf);
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::unassignSelectedSocket()
{
	if (m_selectedSocketRow < 0 || m_selectedSocketRow >= static_cast<int>(m_sockets.size()))
	{
		updateStatus("Select the OUT side of a linked portal first.");
		return;
	}

	Object const * const building = NetworkIdManager::getObjectById(resolveBuildingId());
	PortalProperty const * const portalProperty = building ? building->getPortalProperty() : 0;
	DynamicBunkerOpenFloorplanMessage::SocketEntry const & socket = m_sockets[static_cast<size_t>(m_selectedSocketRow)];
	if (!isSocketHostSide(portalProperty, socket))
	{
		updateStatus("Unassign from the orange OUT portal, not the cyan IN graft entrance.");
		return;
	}
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
	clearLayoutMapOverlay();

	UIEventCallback * const callback = dynamic_cast<UIEventCallback *>(this);
	for (size_t i = 0; i < m_mapNodeButtons.size(); ++i)
	{
		UIWidget * const node = m_mapNodeButtons[i];
		if (!node)
			continue;

		if (callback && node->HasCallback(callback))
			node->RemoveCallback(callback);
		if (m_layoutMapCanvas && node->GetParent() == m_layoutMapCanvas)
			m_layoutMapCanvas->RemoveChild(node);
		node->Destroy();
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
			if (m_layoutMapCanvas && m_mapCellOverlays[i]->GetParent() == m_layoutMapCanvas)
				m_layoutMapCanvas->RemoveChild(m_mapCellOverlays[i]);
			m_mapCellOverlays[i]->Destroy();
		}
	}
	m_mapCellOverlays.clear();
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

	if (!isfinite(canvasX) || !isfinite(canvasY))
		return;

	UIScalar const canvasW = m_layoutMapCanvas->GetWidth();
	UIScalar const canvasH = m_layoutMapCanvas->GetHeight();
	float const nodeSize = 34.0f;
	if (canvasW < nodeSize || canvasH < nodeSize)
		return;

	canvasX = std::max(0.0f, std::min(canvasX - nodeSize * 0.5f, static_cast<float>(canvasW) - nodeSize));
	canvasY = std::max(0.0f, std::min(canvasY - nodeSize * 0.5f, static_cast<float>(canvasH) - nodeSize));

	DynamicBunkerOpenFloorplanMessage::SocketEntry const & socket = m_sockets[static_cast<size_t>(socketIndex)];

	Object const * const building = NetworkIdManager::getObjectById(resolveBuildingId());
	PortalProperty const * const portalProperty = building ? building->getPortalProperty() : 0;
	SocketFlowRole const role = getSocketFlowRole(portalProperty, socket);

	UIPage * node = new UIPage;
	node->SetName("mapNode");

	char label[16];
	snprintf(label, sizeof(label), "%s", getSocketFlowTag(role));

	UIText * const nodeLabel = new UIText;
	nodeLabel->SetName("label");
	nodeLabel->SetPreLocalized(true);
	nodeLabel->SetLocalText(Unicode::narrowToWide(label));
	nodeLabel->SetText(Unicode::narrowToWide(label));
	nodeLabel->SetTextColor(UIColor::white);
	nodeLabel->SetLocation(UIPoint(2, 8));
	nodeLabel->SetSize(UISize(30, 16));
	nodeLabel->SetEnabled(false);

	switch (role)
	{
	case SocketFlowOpen:
		if (socket.custom)
			node->SetBackgroundColor(UIColor(160, 96, 208, 224));
		else
			node->SetBackgroundColor(UIColor(224, 192, 64, 224));
		break;
	case SocketFlowOut:
		node->SetBackgroundColor(UIColor(224, 140, 64, 224));
		break;
	case SocketFlowIn:
		node->SetBackgroundColor(UIColor(64, 180, 224, 224));
		break;
	default:
		node->SetBackgroundColor(UIColor(64, 192, 96, 224));
		break;
	}

	if (m_selectedSocketRow == socketIndex)
		node->SetBackgroundColor(UIColor(255, 255, 255, 240));

	node->SetLocation(UIPoint(static_cast<UIScalar>(canvasX), static_cast<UIScalar>(canvasY)));
	node->SetSize(UISize(static_cast<UIScalar>(nodeSize), static_cast<UIScalar>(nodeSize)));
	node->SetVisible(true);
	node->SetEnabled(true);
	node->SetGetsInput(true);

	node->AddChild(nodeLabel);
	nodeLabel->Link();

	m_layoutMapCanvas->AddChild(node);
	node->Link();
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
	if (!m_layoutMapCanvas || !isActive())
		return;

	if (m_tabs && m_tabs->GetActiveTab() != 1)
		return;

	UIScalar canvasW = m_layoutMapCanvas->GetWidth();
	UIScalar canvasH = m_layoutMapCanvas->GetHeight();
	if (canvasW < 16.0f || canvasH < 16.0f)
	{
		canvasW = 856.0f;
		canvasH = 456.0f;
	}
	if (canvasW < 16.0f || canvasH < 16.0f)
	{
		m_deferredFloorMapRefresh = true;
		return;
	}

	Object const * const building = NetworkIdManager::getObjectById(resolveBuildingId());
	PortalProperty const * const portalProperty = building ? building->getPortalProperty() : 0;
	if (portalProperty)
		resolveSocketMapCoordinates(*portalProperty, m_sockets);

	if (m_sockets.empty() && !portalProperty)
	{
		clearFloorMapNodes();
		if (m_textLayoutHint)
			m_textLayoutHint->SetText(Unicode::narrowToWide("No portal sockets yet. Use Place Portal to add a custom snap."));
		return;
	}

	float boundsMinX = 0.0f;
	float boundsMaxX = 0.0f;
	float boundsMinZ = 0.0f;
	float boundsMaxZ = 0.0f;
	bool haveBounds = false;

	if (portalProperty)
	{
		int const cellCount = portalProperty->getNumberOfCells();
		for (int cellIndex = 1; cellIndex < cellCount; ++cellIndex)
		{
			CellProperty const * const cell = portalProperty->getCell(cellIndex);
			if (cell)
				accumulateCellO2pBounds(cell, boundsMinX, boundsMaxX, boundsMinZ, boundsMaxZ, haveBounds);
		}
	}

	std::vector<bool> socketPlottable(m_sockets.size(), false);
	for (size_t i = 0; i < m_sockets.size(); ++i)
	{
		DynamicBunkerOpenFloorplanMessage::SocketEntry const & socket = m_sockets[i];
		if (!isfinite(socket.mapX) || !isfinite(socket.mapZ))
			continue;

		socketPlottable[i] = true;

		if (!haveBounds)
		{
			boundsMinX = boundsMaxX = socket.mapX;
			boundsMinZ = boundsMaxZ = socket.mapZ;
			haveBounds = true;
		}
		else
		{
			boundsMinX = std::min(boundsMinX, socket.mapX);
			boundsMaxX = std::max(boundsMaxX, socket.mapX);
			boundsMinZ = std::min(boundsMinZ, socket.mapZ);
			boundsMaxZ = std::max(boundsMaxZ, socket.mapZ);
		}
	}

	clearFloorMapNodes();

	if (!haveBounds)
	{
		if (m_textLayoutHint)
			m_textLayoutHint->SetText(Unicode::narrowToWide("Layout map unavailable until building cells are loaded."));
		return;
	}

	if (m_textLayoutHint)
		m_textLayoutHint->SetText(Unicode::narrowToWide("Yellow OPEN = assign here. Orange OUT -> Cyan IN. Green = graft room. Click a node."));

	LayoutMapProjection const proj = LayoutMapProjection::build(
		boundsMinX, boundsMaxX, boundsMinZ, boundsMaxZ,
		static_cast<float>(canvasW), static_cast<float>(canvasH));

	if (portalProperty)
	{
		updateLayoutMapViewer();
		if (m_layoutMapViewer)
		{
			m_layoutMapViewer->SetVisible(true);
			m_layoutMapViewer->setPaused(false);
		}
		createMapCellOverlays(m_layoutMapCanvas, m_mapCellOverlays, *portalProperty, proj);
	}

	size_t const maxMapNodes = 128;
	std::vector<bool> used(m_sockets.size(), false);
	size_t mapNodeCount = 0;
	for (size_t i = 0; i < m_sockets.size(); ++i)
	{
		if (mapNodeCount >= maxMapNodes)
			break;

		if (used[i] || !socketPlottable[i])
			continue;

		DynamicBunkerOpenFloorplanMessage::SocketEntry const & socket = m_sockets[i];
		int const peerIndex = (socket.linkedCellIndex >= 0)
			? findSocketIndex(socket.linkedCellIndex, socket.linkedPortalIndex)
			: -1;

		float pixelX = 0.0f;
		float pixelY = 0.0f;
		proj.mapO2pToCanvas(socket.mapX, socket.mapZ, pixelX, pixelY);

		if (peerIndex >= 0
			&& peerIndex != static_cast<int>(i)
			&& peerIndex < static_cast<int>(m_sockets.size())
			&& socketPlottable[static_cast<size_t>(peerIndex)]
			&& !used[static_cast<size_t>(peerIndex)])
		{
			DynamicBunkerOpenFloorplanMessage::SocketEntry const & peer = m_sockets[static_cast<size_t>(peerIndex)];
			float peerPixelX = 0.0f;
			float peerPixelY = 0.0f;
			proj.mapO2pToCanvas(peer.mapX, peer.mapZ, peerPixelX, peerPixelY);

			int outIndex = static_cast<int>(i);
			int inIndex = peerIndex;
			resolveGraftLinkEndpoints(
				portalProperty,
				socket,
				peer,
				static_cast<int>(i),
				peerIndex,
				outIndex,
				inIndex);

			float outPixelX = pixelX;
			float outPixelY = pixelY;
			float inPixelX = peerPixelX;
			float inPixelY = peerPixelY;
			if (outIndex == peerIndex)
			{
				outPixelX = peerPixelX;
				outPixelY = peerPixelY;
				inPixelX = pixelX;
				inPixelY = pixelY;
			}

			float dx = inPixelX - outPixelX;
			float dy = inPixelY - outPixelY;
			float const len = sqrtf(dx * dx + dy * dy);
			if (len > 4.0f)
			{
				float const normX = dx / len;
				float const normY = dy / len;
				float const pixelOffset = 16.0f;
				createMapButton(outIndex, outPixelX - normX * pixelOffset, outPixelY - normY * pixelOffset);
				createMapButton(inIndex, inPixelX + normX * pixelOffset, inPixelY + normY * pixelOffset);
				mapNodeCount += 2;
			}
			else
			{
				createMapButton(outIndex, outPixelX - 18.0f, outPixelY);
				createMapButton(inIndex, inPixelX + 18.0f, inPixelY);
				mapNodeCount += 2;
			}

			used[i] = true;
			used[static_cast<size_t>(peerIndex)] = true;
		}
		else
		{
			createMapButton(static_cast<int>(i), pixelX, pixelY);
			++mapNodeCount;
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

	Object const * const building = NetworkIdManager::getObjectById(resolveBuildingId());
	PortalProperty const * const portalProperty = building ? building->getPortalProperty() : 0;
	DynamicBunkerOpenFloorplanMessage::SocketEntry const & socket = m_sockets[static_cast<size_t>(socketIndex)];
	SocketFlowRole const role = getSocketFlowRole(portalProperty, socket);

	char buf[256];
	switch (role)
	{
	case SocketFlowOpen:
		snprintf(buf, sizeof(buf),
			"OUT portal c%d/p%d is open — pick a catalog room and Assign.",
			socket.cellIndex, socket.portalIndex);
		break;
	case SocketFlowOut:
		snprintf(buf, sizeof(buf),
			"OUT portal c%d/p%d -> IN c%d/p%d. Replace or Unassign here.",
			socket.cellIndex, socket.portalIndex,
			socket.linkedCellIndex, socket.linkedPortalIndex);
		break;
	case SocketFlowIn:
		snprintf(buf, sizeof(buf),
			"IN portal c%d/p%d (grafted room). Select the orange OUT side to Replace or Unassign.",
			socket.cellIndex, socket.portalIndex);
		break;
	default:
		if (socket.linkedCellIndex >= 0)
		{
			snprintf(buf, sizeof(buf),
				"Linked portal c%d/p%d -> c%d/p%d.",
				socket.cellIndex, socket.portalIndex,
				socket.linkedCellIndex, socket.linkedPortalIndex);
		}
		else
		{
			snprintf(buf, sizeof(buf), "Portal c%d/p%d.", socket.cellIndex, socket.portalIndex);
		}
		break;
	}
	updateStatus(buf);
	updateActionButtons();
	requestFloorMapRefresh();
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
	Vector worldNormal;
	NetworkId cellNetworkId;
	if (!pickWorldPointFromScreen(mouseX, mouseY, worldPoint, cellNetworkId, &worldNormal))
	{
		updateStatus("Could not pick a surface. Aim at the wall or floor and click again.");
		return false;
	}

	addPlacePoint(worldPoint, cellNetworkId, &worldNormal);
	return true;
}

// ----------------------------------------------------------------------

bool SwgCuiDynamicBunkerFloorplan::OnMessage(UIWidget * /*context*/, UIMessage const & msg)
{
	if (msg.Type == UIMessage::LeftMouseDown && m_tabs && m_tabs->GetActiveTab() == 1 && m_layoutMapCanvas)
	{
		UIWidget * hit = dynamic_cast<UIWidget *>(m_layoutMapCanvas->GetWidgetFromPoint(msg.MouseCoords, false));
		while (hit && hit != m_layoutMapCanvas)
		{
			for (size_t i = 0; i < m_mapNodeButtons.size(); ++i)
			{
				if (hit == m_mapNodeButtons[i])
				{
					selectSocketByIndex(m_mapNodeSocketIndices[i]);
					return false;
				}
			}
			hit = dynamic_cast<UIWidget *>(hit->GetParent());
		}
	}

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
	m_hasPlaceWallNormal = false;
	m_placeWallNormal_cell = Vector::zero;
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
	updateStatus("Click two horizontal corners (left/right) on the wall you want to walk through.");
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::updatePlacePointVisuals()
{
	clearPlacePointMarkers();

	CellProperty * parentCell = 0;
	Object const * const player = Game::getPlayer();
	if (player)
		parentCell = const_cast<CellProperty *>(player->getParentCell());

	Object * const building = NetworkIdManager::getObjectById(resolveBuildingId());
	PortalProperty * const portalProperty = building ? building->getPortalProperty() : 0;
	int const cellIndex = resolvePlaceCellIndex();
	if (portalProperty && cellIndex >= 1)
	{
		CellProperty * const cellProperty = portalProperty->getCell(cellIndex);
		if (cellProperty)
			parentCell = cellProperty;
	}

	if (m_placePointCount >= 1)
	{
		Object * const marker = createPlacePointMarker(m_placePoint0_w, parentCell);
		if (marker)
			m_placePointMarkers.push_back(marker);
	}
	if (m_placePointCount >= 2)
	{
		Object * const marker = createPlacePointMarker(m_placePoint1_w, parentCell);
		if (marker)
			m_placePointMarkers.push_back(marker);
	}

	if (!m_placePointsReady || !portalProperty || cellIndex < 1)
		return;

	CellProperty * const cellProperty = portalProperty->getCell(cellIndex);
	CellObject * const cellObject = cellProperty ? CellObject::asCellObject(&cellProperty->getOwner()) : 0;
	if (!cellObject)
		return;

	Vector const pos = m_portalDoorTransform_cell.getPosition_p();
	Vector const axisI = m_portalDoorTransform_cell.getLocalFrameI_p();
	Vector const axisJ = m_portalDoorTransform_cell.getLocalFrameJ_p();
	float const halfWidth = m_placeDoorwayWidth * 0.5f;
	Vector const corners_cell[4] = {
		pos - axisI * halfWidth,
		pos + axisI * halfWidth,
		pos + axisI * halfWidth + axisJ * m_placeDoorwayHeight,
		pos - axisI * halfWidth + axisJ * m_placeDoorwayHeight
	};

	for (size_t ci = 0; ci < 4; ++ci)
	{
		Object * const marker = createPlacePointMarker(cellObject->rotateTranslate_o2w(corners_cell[ci]), cellProperty);
		if (marker)
			m_placePointMarkers.push_back(marker);
	}
}

// ----------------------------------------------------------------------

// ----------------------------------------------------------------------

namespace SwgCuiDynamicBunkerFloorplanNamespace
{
	Vector computeCellFloorCenter_cell(CellProperty const * cell)
	{
		if (!cell)
			return Vector::zero;

		Floor const * const floor = cell->getFloor();
		if (!floor)
			return Vector::zero;

		FloorMesh const * const mesh = floor->getFloorMesh();
		if (!mesh)
			return Vector::zero;

		AxialBox box;
		for (int tri = 0; tri < mesh->getTriCount(); ++tri)
		{
			Triangle3d const T = mesh->getTriangle(tri);
			for (int c = 0; c < 3; ++c)
				box.add(T.getCorner(c));
		}
		return box.getCenter();
	}
}

using namespace SwgCuiDynamicBunkerFloorplanNamespace;

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::computePortalTransformFromCorners(
	Vector const & corner0_cell,
	Vector const & corner1_cell,
	Transform & outTransform_cell,
	float & outDoorwayWidth,
	float & outDoorwayHeight,
	Vector const * wallNormal_cell,
	Vector const * playerPos_cell,
	Vector const * cellInteriorCenter_cell)
{
	float const minX = std::min(corner0_cell.x, corner1_cell.x);
	float const maxX = std::max(corner0_cell.x, corner1_cell.x);
	float const minY = std::min(corner0_cell.y, corner1_cell.y);
	float const maxY = std::max(corner0_cell.y, corner1_cell.y);
	float const minZ = std::min(corner0_cell.z, corner1_cell.z);
	float const maxZ = std::max(corner0_cell.z, corner1_cell.z);

	Vector const center((minX + maxX) * 0.5f, (minY + maxY) * 0.5f, (minZ + maxZ) * 0.5f);
	outDoorwayHeight = std::max(2.0f, maxY - minY);

	Vector const up(0.0f, 1.0f, 0.0f);
	Vector const cornerSpan = corner1_cell - corner0_cell;

	Vector wallNormal;
	bool hasWallNormal = false;
	if (wallNormal_cell && wallNormal_cell->magnitude() > 0.01f)
	{
		wallNormal = *wallNormal_cell;
		wallNormal.y = 0.0f;
		if (wallNormal.normalize() > 0.01f)
			hasWallNormal = true;
	}

	Vector right;
	Vector portalNormal;

	if (hasWallNormal)
	{
		portalNormal = wallNormal;
		right = up.cross(portalNormal);
		if (right.normalize() < 0.01f)
			right = Vector(-portalNormal.z, 0.0f, portalNormal.x);
	}
	else
	{
		Vector widthDir = cornerSpan;
		widthDir.y = 0.0f;
		if (widthDir.normalize() > 0.01f)
		{
			right = widthDir;
			portalNormal = Vector(-right.z, 0.0f, right.x);
			if (portalNormal.normalize() < 0.01f)
				portalNormal = Vector(0.0f, 0.0f, 1.0f);
		}
		else
		{
			float const spanX = maxX - minX;
			float const spanZ = maxZ - minZ;
			if (spanX >= spanZ)
			{
				right = Vector(1.0f, 0.0f, 0.0f);
				portalNormal = Vector(0.0f, 0.0f, 1.0f);
			}
			else
			{
				right = Vector(0.0f, 0.0f, 1.0f);
				portalNormal = Vector(1.0f, 0.0f, 0.0f);
			}
		}
	}

	float doorwayWidth = fabsf(cornerSpan.dot(right));
	if (doorwayWidth < 0.25f)
	{
		float const spanX = maxX - minX;
		float const spanZ = maxZ - minZ;
		doorwayWidth = std::max(0.5f, std::max(spanX, spanZ));
	}
	outDoorwayWidth = std::max(0.5f, doorwayWidth);

	if (cornerSpan.dot(right) < 0.0f)
		right = -right;

	if (cellInteriorCenter_cell)
	{
		Vector outward = center - *cellInteriorCenter_cell;
		outward.y = 0.0f;
		if (outward.normalize() > 0.01f && portalNormal.dot(outward) < 0.0f)
		{
			portalNormal = -portalNormal;
			right = up.cross(portalNormal);
			if (right.normalize() < 0.01f)
				right = Vector(-portalNormal.z, 0.0f, portalNormal.x);
			if (cornerSpan.dot(right) < 0.0f)
				right = -right;
		}
	}
	else if (playerPos_cell)
	{
		Vector toPlayer = *playerPos_cell - center;
		toPlayer.y = 0.0f;
		if (toPlayer.normalize() > 0.01f && portalNormal.dot(toPlayer) > 0.0f)
		{
			portalNormal = -portalNormal;
			right = up.cross(portalNormal);
			if (right.normalize() < 0.01f)
				right = Vector(-portalNormal.z, 0.0f, portalNormal.x);
			if (cornerSpan.dot(right) < 0.0f)
				right = -right;
		}
	}

	outTransform_cell.setLocalFrameIJK_p(right, up, portalNormal);
	outTransform_cell.setPosition_p(Vector(center.x, minY, center.z));
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::computePortalTransformFromPlacePoints()
{
	float doorwayWidth = 1.0f;
	float doorwayHeight = 2.0f;
	Vector const * wallNormal = m_hasPlaceWallNormal ? &m_placeWallNormal_cell : 0;
	Vector playerPos_cell;
	Vector const * playerPos = 0;
	Object const * const player = Game::getPlayer();
	if (player)
	{
		CellProperty const * const playerCell = player->getParentCell();
		if (playerCell && !playerCell->isWorldCell())
		{
			playerPos_cell = playerCell->getOwner().rotateTranslate_w2o(player->getPosition_w());
			playerPos = &playerPos_cell;
		}
	}
	Vector cellCenter_cell;
	Vector const * cellCenter = 0;
	Object * const building = NetworkIdManager::getObjectById(resolveBuildingId());
	PortalProperty * const portalProperty = building ? building->getPortalProperty() : 0;
	int const cellIndex = resolvePlaceCellIndex();
	if (portalProperty && cellIndex >= 1)
	{
		CellProperty * const cell = portalProperty->getCell(cellIndex);
		if (cell)
		{
			cellCenter_cell = computeCellFloorCenter_cell(cell);
			cellCenter = &cellCenter_cell;
		}
	}
	computePortalTransformFromCorners(
		m_placePoint0_cell, m_placePoint1_cell,
		m_portalDoorTransform_cell, doorwayWidth, doorwayHeight,
		wallNormal, playerPos, cellCenter);
	m_placeDoorwayWidth = doorwayWidth;
	m_placeDoorwayHeight = doorwayHeight;
	m_placePointsReady = true;
}

// ----------------------------------------------------------------------

void SwgCuiDynamicBunkerFloorplan::addPlacePoint(Vector const & worldPoint, NetworkId const & cellNetworkId, Vector const * worldNormal)
{
	if (!m_placingPortalPoints)
		return;

	int cellIndex = resolvePlaceCellIndex();
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

	CellObject * cellObject = 0;
	if (cellNetworkId.isValid())
	{
		cellObject = CellObject::asCellObject(NetworkIdManager::getObjectById(cellNetworkId));
		if (cellObject && cellObject->getCellProperty())
			cellIndex = cellObject->getCellProperty()->getCellIndex();
	}
	if (!cellObject)
	{
		CellProperty * const cellProperty = portalProperty->getCell(cellIndex);
		cellObject = cellProperty ? CellObject::asCellObject(&cellProperty->getOwner()) : 0;
	}
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

	if (worldNormal && worldNormal->magnitude() > 0.01f)
	{
		Vector normal_cell = cellObject->rotate_w2o(*worldNormal);
		normal_cell.y = 0.0f;
		if (normal_cell.normalize() > 0.01f)
		{
			// Hit normal points back into this cell; portal leads through the wall into the graft.
			normal_cell = -normal_cell;
			if (!m_hasPlaceWallNormal)
			{
				m_placeWallNormal_cell = normal_cell;
				m_hasPlaceWallNormal = true;
			}
			else
			{
				m_placeWallNormal_cell = (m_placeWallNormal_cell + normal_cell) * 0.5f;
				if (m_placeWallNormal_cell.normalize() < 0.01f)
					m_hasPlaceWallNormal = false;
			}
		}
	}

	if (m_placePointCount == 0)
	{
		m_placePoint0_w = worldPoint;
		m_placePoint0_cell = point_cell;
		m_placePointCount = 1;
		m_placePointsReady = false;
		updatePlacePointVisuals();
		updateStatus("First corner set. Click the opposite doorway corner on the same wall.");
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
		updateStatus("Doorway set. Create Snap adds a walk-through portal into the next room.");
		return;
	}

	// Restart placement if a third click arrives.
	m_placePoint0_w = worldPoint;
	m_placePoint0_cell = point_cell;
	m_placePoint1_w = Vector();
	m_placePoint1_cell = Vector();
	m_placePointCount = 1;
	m_placePointsReady = false;
	m_hasPlaceWallNormal = false;
	m_placeWallNormal_cell = Vector::zero;
	if (worldNormal && worldNormal->magnitude() > 0.01f)
	{
		Vector normal_cell = cellObject->rotate_w2o(*worldNormal);
		normal_cell.y = 0.0f;
		if (normal_cell.normalize() > 0.01f)
		{
			m_placeWallNormal_cell = -normal_cell;
			m_hasPlaceWallNormal = true;
		}
	}
	m_placingPortalPoints = true;
	s_activePlacePointsInstance = this;
	updatePlacePointVisuals();
	updateStatus("Restarting placement. Click the opposite doorway corner on the wall.");
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
	Vector playerPos_cell;
	Vector const * playerPos = 0;
	Object const * const player = Game::getPlayer();
	if (player)
	{
		CellProperty const * const playerCell = player->getParentCell();
		if (playerCell && !playerCell->isWorldCell())
		{
			playerPos_cell = playerCell->getOwner().rotateTranslate_w2o(player->getPosition_w());
			playerPos = &playerPos_cell;
		}
	}
	Vector const * wallNormal = m_hasPlaceWallNormal ? &m_placeWallNormal_cell : 0;
	Vector cellCenter_cell;
	Vector const * cellCenter = 0;
	Object * const building = NetworkIdManager::getObjectById(buildingId);
	PortalProperty * const portalProperty = building ? building->getPortalProperty() : 0;
	if (portalProperty)
	{
		CellProperty * const cell = portalProperty->getCell(cellIndex);
		if (cell)
		{
			cellCenter_cell = computeCellFloorCenter_cell(cell);
			cellCenter = &cellCenter_cell;
		}
	}
	computePortalTransformFromCorners(
		m_placePoint0_cell, m_placePoint1_cell,
		m_portalDoorTransform_cell, doorwayWidth, doorwayHeight,
		wallNormal, playerPos, cellCenter);
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
