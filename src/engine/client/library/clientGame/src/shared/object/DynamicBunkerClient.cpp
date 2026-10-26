// ======================================================================

//

// DynamicBunkerClient.cpp

// copyright 2026 Titan

//

// ======================================================================



#include "clientGame/FirstClientGame.h"

#include "clientGame/DynamicBunkerClient.h"

#include "clientGame/CellObject.h"
#include "clientGame/ClientWorld.h"
#include "sharedCollision/CollideParameters.h"
#include "sharedCollision/CollisionInfo.h"



#include "clientGame/Game.h"

#include "sharedFoundation/ExitChain.h"

#include "sharedMessageDispatch/Message.h"

#include "sharedMessageDispatch/Receiver.h"

#include "sharedNetworkMessages/DynamicBunkerMessages.h"

#include "sharedNetworkMessages/GameNetworkMessage.h"

#include "sharedObject/Appearance.h"

#include "sharedObject/AppearanceTemplateList.h"

#include "sharedObject/CellProperty.h"

#include "sharedObject/NetworkIdManager.h"

#include "sharedObject/ObjectList.h"

#include "sharedObject/Portal.h"

#include "sharedObject/PortalProperty.h"
#include "sharedObject/PortalPropertyTemplate.h"
#include "sharedObject/PortalPropertyTemplateList.h"
#include "sharedFoundation/CrcLowerString.h"
#include "sharedMath/IndexedTriangleList.h"

#include "clientObject/ComponentAppearance.h"
#include "clientObject/DetailAppearance.h"
#include "clientObject/MeshAppearance.h"
#include "clientGraphics/RenderWorld.h"
#include "clientGraphics/ShaderPrimitiveSet.h"
#include "clientGraphics/StaticIndexBuffer.h"
#include "clientGraphics/StaticVertexBuffer.h"
#include "clientGraphics/VertexBufferIterator.h"
#include "sharedMath/AxialBox.h"
#include "sharedObject/Appearance.h"
#include "sharedCollision/Floor.h"
#include "sharedCollision/FloorLocator.h"
#include "sharedCollision/Intersect3d.h"
#include "sharedMath/Segment3d.h"
#include "sharedMath/Triangle3d.h"
#include "sharedMath/Vector.h"

#include <cmath>

#include <map>

#include <set>

#include <vector>



// ======================================================================



namespace DynamicBunkerClientNamespace

{

	char const * const s_bridgeAppearance = "appearance/godclient_space_waypoint_corridor.apt";
	char const * const s_doorFrameAppearance = "appearance/door_bunker_rebel.apt";
	char const * const s_doorAlcoveAppearance = "appearance/poi_all_impl_bunker_vertical_door.apt";

	typedef std::vector<Object *> BridgeObjectList;
	typedef std::map<NetworkId, BridgeObjectList> BuildingBridgeMap;
	typedef std::vector<Object *> DoorwayObjectList;
	typedef std::map<NetworkId, DoorwayObjectList> BuildingDoorwayMap;

	BuildingBridgeMap s_buildingBridges;
	BuildingDoorwayMap s_buildingDoorways;

	NetworkId s_wallCutRefreshBuilding;
	std::set<NetworkId> s_wallCutHookCells;
	int s_wallCutRenderHookThrottle = 0;

	struct PreviewWallHit
	{
		int cellIndex;
		NetworkId hitObjectId;
	};

	std::map<NetworkId, PreviewWallHit> s_previewWallHits;

	struct ActivePreviewWallCut
	{
		NetworkId buildingId;
		int cellIndex;
		NetworkId hitObjectId;
		Transform doorTransform_cell;
		float width;
		float height;
		PortalVolumeCut volume;
		bool hasVolume;
		bool active;

		ActivePreviewWallCut()
		: buildingId(),
		  cellIndex(-1),
		  hitObjectId(),
		  doorTransform_cell(),
		  width(1.0f),
		  height(2.0f),
		  volume(),
		  hasVolume(false),
		  active(false)
		{
		}
	};

	ActivePreviewWallCut s_activePreviewWallCut;

	struct SocketWallHitKey
	{
		NetworkId buildingId;
		int cellIndex;
		int socketIndex;

		bool operator<(SocketWallHitKey const & rhs) const
		{
			if (buildingId < rhs.buildingId)
				return true;
			if (rhs.buildingId < buildingId)
				return false;
			if (cellIndex < rhs.cellIndex)
				return true;
			if (rhs.cellIndex < cellIndex)
				return false;
			return socketIndex < rhs.socketIndex;
		}
	};

	std::map<SocketWallHitKey, NetworkId> s_socketWallHits;

	struct PendingSocketWallHit
	{
		NetworkId buildingId;
		int cellIndex;
		NetworkId hitObjectId;

		PendingSocketWallHit()
		: buildingId(),
		  cellIndex(-1),
		  hitObjectId()
		{
		}
	};

	PendingSocketWallHit s_pendingSocketWallHit;

	struct PendingGraftCellLoad
	{
		NetworkId buildingId;
		NetworkId cellId;
		int graftedCellIndex;
		Transform cellTransform;

		PendingGraftCellLoad()
		: buildingId(),
		  cellId(),
		  graftedCellIndex(-1),
		  cellTransform()
		{
		}
	};

	std::vector<PendingGraftCellLoad> s_pendingGraftCellLoads;

	bool customSocketAlreadyMaterialized(PortalProperty const & portalProperty, int cellIndex, int customSocketIndex)
	{
		PortalProperty::CustomSocket customSocket;
		return portalProperty.findCustomSocket(cellIndex, customSocketIndex, customSocket)
			&& customSocket.materializedPortalIndex >= 0;
	}

	void queuePendingGraftCellLoad(
		NetworkId const & buildingId,
		NetworkId const & cellId,
		int graftedCellIndex,
		Transform const & cellTransform)
	{
		if (!buildingId.isValid() || !cellId.isValid() || graftedCellIndex < 1)
			return;

		for (size_t i = 0; i < s_pendingGraftCellLoads.size(); ++i)
		{
			if (s_pendingGraftCellLoads[i].buildingId == buildingId
				&& s_pendingGraftCellLoads[i].graftedCellIndex == graftedCellIndex)
			{
				s_pendingGraftCellLoads[i].cellId = cellId;
				s_pendingGraftCellLoads[i].cellTransform = cellTransform;
				return;
			}
		}

		PendingGraftCellLoad pending;
		pending.buildingId = buildingId;
		pending.cellId = cellId;
		pending.graftedCellIndex = graftedCellIndex;
		pending.cellTransform = cellTransform;
		s_pendingGraftCellLoads.push_back(pending);
	}

	void tryLoadPendingGraftCells(Object & building, PortalProperty & portalProperty)
	{
		NetworkId const buildingId = building.getNetworkId();
		for (size_t i = 0; i < s_pendingGraftCellLoads.size(); )
		{
			PendingGraftCellLoad const & pending = s_pendingGraftCellLoads[i];
			if (pending.buildingId != buildingId)
			{
				++i;
				continue;
			}

			if (portalProperty.getCell(pending.graftedCellIndex))
			{
				s_pendingGraftCellLoads.erase(s_pendingGraftCellLoads.begin() + static_cast<std::vector<PendingGraftCellLoad>::difference_type>(i));
				continue;
			}

			Object * const cellObject = NetworkIdManager::getObjectById(pending.cellId);
			if (!cellObject)
			{
				++i;
				continue;
			}

			portalProperty.cellLoaded(pending.graftedCellIndex, *cellObject, true);
			Transform cellTransform = pending.cellTransform;
			DynamicBunkerClient::ensureRightHandedCellTransform(cellTransform);
			cellObject->setTransform_o2p(cellTransform);
			s_pendingGraftCellLoads.erase(s_pendingGraftCellLoads.begin() + static_cast<std::vector<PendingGraftCellLoad>::difference_type>(i));
		}
	}

	bool isGraftAlreadyLinked(PortalProperty const & portalProperty, PortalProperty::DynamicRoomGraft const & graft);

	void finalizeLinkedCustomSocketGrafts(PortalProperty & portalProperty)
	{
		PortalProperty::DynamicRoomGraftList const & grafts = portalProperty.getDynamicRoomGrafts();
		for (size_t i = 0; i < grafts.size(); ++i)
		{
			PortalProperty::DynamicRoomGraft const & graft = grafts[i];
			if (!PortalProperty::isCustomSocketIndex(graft.hostPortalIndex))
				continue;
			if (!isGraftAlreadyLinked(portalProperty, graft))
				continue;

			IGNORE_RETURN(portalProperty.finalizeCustomSocketPortalWalkthrough(graft.hostCellIndex, graft.hostPortalIndex));
			portalProperty.refreshCellWallCuts(graft.hostCellIndex);
		}

		portalProperty.refreshDynamicGraftPortalDpvs();
	}

	void clearSocketWallHitsForBuilding(NetworkId const & buildingId)
	{
		for (std::map<SocketWallHitKey, NetworkId>::iterator it = s_socketWallHits.begin(); it != s_socketWallHits.end(); )
		{
			if (it->first.buildingId == buildingId)
				s_socketWallHits.erase(it++);
			else
				++it;
		}

		if (s_pendingSocketWallHit.buildingId == buildingId)
			s_pendingSocketWallHit = PendingSocketWallHit();

		if (s_activePreviewWallCut.buildingId == buildingId)
			s_activePreviewWallCut = ActivePreviewWallCut();

		s_previewWallHits.erase(buildingId);
	}

	NetworkId lookupSocketWallHitObject(NetworkId const & buildingId, int cellIndex, int socketIndex)
	{
		SocketWallHitKey key;
		key.buildingId = buildingId;
		key.cellIndex = cellIndex;
		key.socketIndex = socketIndex;
		std::map<SocketWallHitKey, NetworkId>::const_iterator it = s_socketWallHits.find(key);
		if (it != s_socketWallHits.end())
			return it->second;
		return NetworkId();
	}

	void registerPendingSocketWallHit(NetworkId const & buildingId, int cellIndex, int socketIndex)
	{
		if (!s_pendingSocketWallHit.hitObjectId.isValid())
			return;
		if (s_pendingSocketWallHit.buildingId != buildingId || s_pendingSocketWallHit.cellIndex != cellIndex)
			return;

		SocketWallHitKey key;
		key.buildingId = buildingId;
		key.cellIndex = cellIndex;
		key.socketIndex = socketIndex;
		s_socketWallHits[key] = s_pendingSocketWallHit.hitObjectId;
		s_pendingSocketWallHit = PendingSocketWallHit();
	}

	void snapDoorTransformYToCellFloor(CellProperty const * cell, Transform & doorTransform_cell)
	{
		if (!cell)
			return;

		Floor const * const floor = cell->getFloor();
		if (!floor)
			return;

		Vector const pos = doorTransform_cell.getPosition_p();
		FloorLocator loc;
		if (floor->findClosestLocation(pos, loc))
			doorTransform_cell.setPosition_p(Vector(pos.x, loc.getPosition_p().y, pos.z));
	}

	void disableDetailWriteModelForCell(CellProperty * cell)
	{
		if (!cell)
			return;

		Object * const appearanceObject = cell->getAppearanceObject();
		if (!appearanceObject)
			return;

		Appearance * const appearance = appearanceObject->getAppearance();
		if (!appearance)
			return;

		DetailAppearance * const detailApp = appearance->asDetailAppearance();
		if (!detailApp)
			return;

		detailApp->disableDpvsWriteModel();
	}

	void restoreDetailWriteModelForCell(CellProperty * cell)
	{
		if (!cell)
			return;

		Object * const appearanceObject = cell->getAppearanceObject();
		if (!appearanceObject)
			return;

		Appearance * const appearance = appearanceObject->getAppearance();
		if (!appearance)
			return;

		DetailAppearance * const detailApp = appearance->asDetailAppearance();
		if (!detailApp)
			return;

		detailApp->restoreDpvsWriteModel();
	}

	void applyWallVisualCuts(PortalProperty & portalProperty);
	void forceLoadAllCellAppearances(PortalProperty & portalProperty);

	void onCellEnterRenderRefreshWallCuts()
	{
		if (!s_wallCutRefreshBuilding.isValid())
			return;

		Object const * const player = Game::getPlayer();
		if (!player)
			return;

		CellProperty const * const playerCell = player->getParentCell();
		if (!playerCell || playerCell->isWorldCell())
			return;

		PortalProperty const * const playerPortal = playerCell->getPortalProperty();
		if (!playerPortal || playerPortal->getOwner().getNetworkId() != s_wallCutRefreshBuilding)
			return;

		if (s_activePreviewWallCut.active
			&& s_activePreviewWallCut.buildingId == s_wallCutRefreshBuilding)
			return;

		if (++s_wallCutRenderHookThrottle < 2)
			return;
		s_wallCutRenderHookThrottle = 0;

		Object * const building = NetworkIdManager::getObjectById(s_wallCutRefreshBuilding);
		if (!building)
			return;

		PortalProperty * const portalProperty = building->getPortalProperty();
		if (!portalProperty || portalProperty->getCustomSockets().empty())
			return;

		forceLoadAllCellAppearances(*portalProperty);
		applyWallVisualCuts(*portalProperty);
	}

	void ensureWallCutRenderHooks(PortalProperty & portalProperty)
	{
		s_wallCutRefreshBuilding = portalProperty.getOwner().getNetworkId();
		s_wallCutRenderHookThrottle = 0;

		int const cellCount = portalProperty.getNumberOfCells();
		for (int cellIndex = 1; cellIndex < cellCount; ++cellIndex)
		{
			CellProperty * const cell = portalProperty.getCell(cellIndex);
			if (!cell)
				continue;

			CellObject * const cellObject = CellObject::asCellObject(&cell->getOwner());
			if (!cellObject)
				continue;

			NetworkId const cellId = cellObject->getNetworkId();
			if (s_wallCutHookCells.find(cellId) != s_wallCutHookCells.end())
				continue;

			cell->addEnterRenderHookFunction(onCellEnterRenderRefreshWallCuts);
			s_wallCutHookCells.insert(cellId);
		}
	}



	class Listener : public MessageDispatch::Receiver

	{

	public:

		Listener();

		virtual void receiveMessage(MessageDispatch::Emitter const & source, MessageDispatch::MessageBase const & message);

	};



	Listener *s_listener = 0;



	void clearBuildingBridges(NetworkId const & buildingId)

	{

		BuildingBridgeMap::iterator const it = s_buildingBridges.find(buildingId);

		if (it == s_buildingBridges.end())

			return;



		for (size_t i = 0; i < it->second.size(); ++i)

			delete it->second[i];

		it->second.clear();

	}



	void clearBuildingDoorways(NetworkId const & buildingId)
	{
		BuildingDoorwayMap::iterator const it = s_buildingDoorways.find(buildingId);
		if (it == s_buildingDoorways.end())
			return;

		for (size_t i = 0; i < it->second.size(); ++i)
			delete it->second[i];

		it->second.clear();
	}



	bool getPortalBuildingTransform(PortalProperty const & portalProperty, int cellIndex, int portalIndex, Transform &outTransform_o2p)
	{
		return portalProperty.getPortalSocketTransform_o2p(cellIndex, portalIndex, outTransform_o2p);
	}



	void addBridgeVisual(Object & building, PortalProperty::BridgeSegment const & segment)
	{
		Appearance * const app = AppearanceTemplateList::createAppearance(s_bridgeAppearance);
		if (!app)
			return;

		Transform worldTransform;
		worldTransform.multiply(building.getTransform_o2w(), segment.transform_o2p);

		Object * const bridgeObject = new Object;
		bridgeObject->setAppearance(app);
		bridgeObject->setTransform_o2w(worldTransform);
		float const scaleAlong = std::max(0.25f, segment.length / 4.0f);
		float const scaleWidth = std::max(0.25f, segment.width / 2.5f);
		float const scaleHeight = std::max(0.25f, segment.height / 3.0f);
		bridgeObject->setScale(Vector(scaleWidth, scaleHeight, scaleAlong));
		IGNORE_RETURN(bridgeObject->alter(0.0f));
		bridgeObject->conclude();

		s_buildingBridges[building.getNetworkId()].push_back(bridgeObject);
	}

	void mergePortalVolumeCuts(
		PortalVolumeCut const & wallVolume,
		PortalVolumeCut const * floorVolume,
		PortalVolumeCut & outCombined)
	{
		outCombined = wallVolume;
		if (!floorVolume)
			return;

		outCombined.minI = std::min(wallVolume.minI, floorVolume->minI);
		outCombined.maxI = std::max(wallVolume.maxI, floorVolume->maxI);
		outCombined.minJ = std::min(wallVolume.minJ, floorVolume->minJ);
		outCombined.maxJ = std::max(wallVolume.maxJ, floorVolume->maxJ);
		outCombined.inwardDepth = std::max(wallVolume.inwardDepth, floorVolume->inwardDepth);
		outCombined.outwardDepth = std::max(wallVolume.outwardDepth, floorVolume->outwardDepth);
	}

	void addDoorVisualToCell(
		Object & building,
		CellProperty const * cell,
		Transform const & doorTransform_cell,
		float width,
		float height,
		char const * appearancePath,
		Vector const & scale,
		Vector const & localOffset)
	{
		if (!cell || !appearancePath || !appearancePath[0])
			return;

		Appearance * const doorApp = AppearanceTemplateList::createAppearance(appearancePath);
		if (!doorApp)
			return;

		Transform doorTransform = doorTransform_cell;
		if (localOffset.x != 0.0f || localOffset.y != 0.0f || localOffset.z != 0.0f)
			doorTransform.move_p(localOffset);

		Object * const doorObject = new Object;
		doorObject->setAppearance(doorApp);
		doorObject->setParentCell(const_cast<CellProperty *>(cell));
		doorObject->setTransform_o2p(doorTransform);
		doorObject->setScale(scale);
		RenderWorld::addObjectNotifications(*doorObject);
		doorObject->addToWorld();
		IGNORE_RETURN(doorObject->alter(0.0f));
		doorObject->conclude();
		UNREF(building);
		s_buildingDoorways[building.getNetworkId()].push_back(doorObject);
	}

	void addCustomSocketDoorVisual(
		Object & building,
		PortalProperty const & portalProperty,
		PortalProperty::CustomSocket const & socket)
	{
		if (socket.materializedPortalIndex < 0)
			return;

		CellProperty const * const cell = portalProperty.getCell(socket.cellIndex);
		if (!cell)
			return;

		float width = socket.doorwayWidth > 0.01f ? socket.doorwayWidth : 1.0f;
		float height = socket.doorwayHeight > 0.01f ? socket.doorwayHeight : 2.0f;

		Transform doorTransform_cell = socket.doorTransform_o2p;
		snapDoorTransformYToCellFloor(cell, doorTransform_cell);

		char const * const doorStyle = Portal::pickDynamicBunkerDoorStyle(doorTransform_cell);
		std::string doorAppearancePath;
		if (!Portal::lookupDoorStyleAppearance(doorStyle, doorAppearancePath))
			doorAppearancePath = s_doorFrameAppearance;

		Vector const frameScale(
			std::max(0.5f, width / 1.6f),
			std::max(0.5f, height / 2.4f),
			1.0f);
		addDoorVisualToCell(building, cell, doorTransform_cell, width, height, doorAppearancePath.c_str(), frameScale, Vector::zero);

		Vector const alcoveOffset = doorTransform_cell.getLocalFrameK_p() * -0.35f;
		Vector const alcoveScale(
			std::max(0.5f, width / 1.8f),
			std::max(0.5f, height / 2.6f),
			0.65f);
		addDoorVisualToCell(building, cell, doorTransform_cell, width, height, s_doorAlcoveAppearance, alcoveScale, alcoveOffset);
	}

	void addDoorwayVisual(Object & building, PortalProperty const & portalProperty, PortalProperty::DynamicRoomGraft const & graft)
	{
		if (!PortalProperty::isCustomSocketIndex(graft.hostPortalIndex))
			return;

		PortalProperty::CustomSocket customSocket;
		if (!portalProperty.findCustomSocket(graft.hostCellIndex, graft.hostPortalIndex, customSocket))
			return;

		addCustomSocketDoorVisual(building, portalProperty, customSocket);
	}

	void refreshDoorwaysFromPortalProperty(Object & building, PortalProperty const & portalProperty)
	{
		clearBuildingDoorways(building.getNetworkId());

		PortalProperty::CustomSocketList const & sockets = portalProperty.getCustomSockets();
		for (size_t i = 0; i < sockets.size(); ++i)
			addCustomSocketDoorVisual(building, portalProperty, sockets[i]);

		PortalProperty::DynamicRoomGraftList const & grafts = portalProperty.getDynamicRoomGrafts();
		for (size_t gi = 0; gi < grafts.size(); ++gi)
			addDoorwayVisual(building, portalProperty, grafts[gi]);
	}

	struct WallCutContext
	{
		enum Mode
		{
			ModeBox,
			ModePortalVolume
		};

		Mode mode;
		AxialBox const * cutBox;
		Transform const * doorTransform;
		Transform const * doorTransformCell;
		CellProperty * cell;
		float doorwayWidth;
		float doorwayHeight;
		PortalVolumeCut const * portalVolume;
		PortalVolumeCut const * portalFloorVolume;
		std::set<MeshAppearance *> * processedMeshes;
		bool usePreviewWallHit;
		Transform doorWorld;
		bool hasDoorWorld;
	};

	void forEachLoadedMeshAppearance(Appearance * appearance, void (*callback)(MeshAppearance &, void *), void * context);

	bool computePortalDoorWorld(
		PortalProperty const & portalProperty,
		int cellIndex,
		int portalIndex,
		Transform const & doorTransform_cell,
		Transform & outDoorWorld)
	{
		Object const & building = portalProperty.getOwner();
		Transform portalBuilding;
		if (portalIndex >= 0 && getPortalBuildingTransform(portalProperty, cellIndex, portalIndex, portalBuilding))
		{
			outDoorWorld.multiply(building.getTransform_o2w(), portalBuilding);
			return true;
		}

		CellProperty const * const cell = portalProperty.getCell(cellIndex);
		if (!cell)
			return false;

		outDoorWorld.multiply(cell->getOwner().getTransform_o2w(), doorTransform_cell);
		return true;
	}

	NetworkId findWallHitObjectAtPortal(
		PortalProperty const & portalProperty,
		int cellIndex,
		int materializedPortalIndex,
		Transform const & doorTransform_cell)
	{
		CellProperty const * const cell = portalProperty.getCell(cellIndex);
		if (!cell)
			return NetworkId();

		Transform doorWorld;
		if (!computePortalDoorWorld(portalProperty, cellIndex, materializedPortalIndex, doorTransform_cell, doorWorld))
			return NetworkId();

		Vector const start = doorWorld.getPosition_p() + doorWorld.getLocalFrameK_p() * 0.05f;
		Vector const end = start - doorWorld.getLocalFrameK_p() * 3.0f;

		CollideParameters collideParams;
		collideParams.setQuality(CollideParameters::Q_high);

		ClientWorld::CollisionInfoVector results;
		uint16 const collisionFlags =
			ClientWorld::CF_interiorObjects |
			ClientWorld::CF_interiorGeometry |
			ClientWorld::CF_childObjects;

		if (!ClientWorld::collide(cell, start, end, collideParams, results, collisionFlags, 0))
			return NetworkId();

		Object const * const building = &portalProperty.getOwner();
		for (size_t i = 0; i < results.size(); ++i)
		{
			Object const * const hitObject = results[i].getObject();
			if (!hitObject || hitObject == building)
				continue;

			for (Object const * current = hitObject; current; current = current->getAttachedTo())
			{
				if (current == building)
					return hitObject->getNetworkId();
			}
		}

		return NetworkId();
	}

	bool resolveMaterializedPortalDoorTransform_cell(
		PortalProperty const & portalProperty,
		int cellIndex,
		int portalIndex,
		Transform & outDoorTransform_cell)
	{
		CellProperty const * const cell = portalProperty.getCell(cellIndex);
		if (!cell)
			return false;

		Transform portalBuilding;
		if (!getPortalBuildingTransform(portalProperty, cellIndex, portalIndex, portalBuilding))
			return false;

		Transform buildingToCell;
		buildingToCell.invert(cell->getOwner().getTransform_o2p());
		outDoorTransform_cell.multiply(buildingToCell, portalBuilding);
		snapDoorTransformYToCellFloor(cell, outDoorTransform_cell);
		return true;
	}

	CellProperty * resolveCellForPlacement(PortalProperty & portalProperty, int cellIndex)
	{
		CellProperty * cell = portalProperty.getCell(cellIndex);
		if (cell)
			return cell;

		Object const * const player = Game::getPlayer();
		if (!player)
			return 0;

		CellProperty const * const playerCell = player->getParentCell();
		if (!playerCell || playerCell->isWorldCell() || playerCell->getCellIndex() != cellIndex)
			return 0;

		PortalProperty const * const playerPortal = playerCell->getPortalProperty();
		if (!playerPortal)
			return 0;

		NetworkId const buildingId = portalProperty.getOwner().getNetworkId();
		if (playerPortal->getOwner().getNetworkId() != buildingId)
			return 0;

		return const_cast<CellProperty *>(playerCell);
	}

	void forceLoadCellDetailMeshes(CellProperty * cell)
	{
		if (!cell)
			return;

		Object * const appearanceObject = cell->getAppearanceObject();
		if (!appearanceObject || !appearanceObject->getAppearance())
			return;

		DetailAppearance * const detailApp = appearanceObject->getAppearance()->asDetailAppearance();
		if (!detailApp)
			return;

		detailApp->forEachAllDetailMeshAppearances(
			[](MeshAppearance & meshApp, void * context)
			{
				UNREF(meshApp);
				UNREF(context);
			},
			0);
	}

	void syncCellAppearanceMeshTransforms(CellProperty * cell)
	{
		if (!cell)
			return;

		Object * const appearanceObject = cell->getAppearanceObject();
		if (!appearanceObject || !appearanceObject->getAppearance())
			return;

		IGNORE_RETURN(appearanceObject->alter(0.0f));

		DetailAppearance * const detailApp = appearanceObject->getAppearance()->asDetailAppearance();
		if (!detailApp)
			return;

		Transform const meshTransform = appearanceObject->getTransform_o2w();
		detailApp->forEachAllDetailMeshAppearances(
			[](MeshAppearance & meshApp, void * context)
			{
				Transform const * const transform = static_cast<Transform const *>(context);
				if (transform)
					meshApp.setTransform_w(*transform);
			},
			const_cast<Transform *>(&meshTransform));
	}

	void buildFloorPortalVolumeCut(PortalVolumeCut const & wallVolume, PortalVolumeCut & outFloorVolume)
	{
		outFloorVolume = wallVolume;
		outFloorVolume.minJ = -0.25f;
		outFloorVolume.maxJ = 0.15f;
		outFloorVolume.minI -= 0.08f;
		outFloorVolume.maxI += 0.08f;
		outFloorVolume.inwardDepth = std::max(wallVolume.inwardDepth, 1.25f);
		outFloorVolume.outwardDepth = std::max(wallVolume.outwardDepth, 0.85f);
	}

	struct MeshWallRaycastState
	{
		Segment3d segment;
		float bestDistance;
		Vector bestPoint;
		Vector bestNormal;
		bool found;

		MeshWallRaycastState(Vector const & worldStart, Vector const & worldEnd)
		: segment(worldStart, worldEnd),
		  bestDistance(0.0f),
		  bestPoint(),
		  bestNormal(),
		  found(false)
		{
		}
	};

	void raycastMeshAppearanceWallTriangles(MeshAppearance & meshApp, void * context)
	{
		MeshWallRaycastState * const state = static_cast<MeshWallRaycastState *>(context);
		if (!state)
			return;

		ShaderPrimitiveSet const * const shaderPrimitiveSet = meshApp.getShaderPrimitiveSet();
		if (!shaderPrimitiveSet)
			return;

		Object const * const meshOwner = meshApp.getOwner();
		Transform meshToWorld = meshApp.getTransform_w();
		if (meshOwner)
			meshToWorld = meshOwner->getTransform_o2w();
		meshApp.setTransform_w(meshToWorld);

		int const shaderCount = shaderPrimitiveSet->getNumberOfShaders();
		for (int shaderIndex = 0; shaderIndex < shaderCount; ++shaderIndex)
		{
			StaticVertexBuffer const * const vb = shaderPrimitiveSet->getVertexBuffer(shaderIndex);
			StaticIndexBuffer const * const ib = shaderPrimitiveSet->getIndexBuffer(shaderIndex);
			if (!vb || !ib)
				continue;

			int const indexCount = ib->getNumberOfIndices();
			if (indexCount < 3)
				continue;

			vb->lockReadOnly();
			ib->lockReadOnly();
			{
				VertexBufferReadIterator vit = vb->beginReadOnly();
				Index const * indices = ib->beginReadOnly();
				for (int tri = 0; tri < indexCount; tri += 3)
				{
					VertexBufferReadIterator it0 = vit;
					it0 += indices[tri];
					VertexBufferReadIterator it1 = vit;
					it1 += indices[tri + 1];
					VertexBufferReadIterator it2 = vit;
					it2 += indices[tri + 2];

					Vector const w0 = meshToWorld.rotateTranslate_l2p(it0.getPosition());
					Vector const w1 = meshToWorld.rotateTranslate_l2p(it1.getPosition());
					Vector const w2 = meshToWorld.rotateTranslate_l2p(it2.getPosition());
					Triangle3d const triangle(w0, w1, w2);

					Vector hitPoint;
					float hitParam = 0.0f;
					if (!Intersect3d::IntersectSegTriSided(state->segment, triangle, hitPoint, hitParam))
						continue;

					Vector const edge0 = w1 - w0;
					Vector const edge1 = w2 - w0;
					Vector normal = edge0.cross(edge1);
					if (normal.normalize() < 0.01f)
						continue;

					if (fabsf(normal.y) > 0.95f)
						continue;

					float const distance = (hitPoint - state->segment.getBegin()).magnitude();
					if (state->found && distance >= state->bestDistance)
						continue;

					state->found = true;
					state->bestDistance = distance;
					state->bestPoint = hitPoint;
					state->bestNormal = normal;
				}
			}
			ib->unlock();
			vb->unlock();
		}
	}

	void applyWallCutToMesh(MeshAppearance & meshApp, WallCutContext const & ctx, Transform const & doorWorld)
	{
		if (!ctx.portalVolume)
			return;

		Object const * const meshOwner = meshApp.getOwner();
		if (meshOwner)
			meshApp.setTransform_w(meshOwner->getTransform_o2w());

		Transform const meshToWorld = meshApp.getTransform_w();
		PortalVolumeCut combinedVolume;
		mergePortalVolumeCuts(*ctx.portalVolume, ctx.portalFloorVolume, combinedVolume);

		meshApp.clearPendingPortalCuts();
		meshApp.addPendingPortalCut(doorWorld, *ctx.portalVolume);
		if (ctx.portalFloorVolume)
			meshApp.addPendingPortalCut(doorWorld, *ctx.portalFloorVolume);

		IGNORE_RETURN(meshApp.hideShaderPrimitivesInPortalVolumeWorld(doorWorld, meshToWorld, combinedVolume));
		meshApp.applyPendingPortalCuts();
	}

	void forceApplyWallCutMeshCallback(MeshAppearance & meshApp, void * context)
	{
		UNREF(context);
		meshApp.applyPendingPortalCuts();
	}

	void applyWallCutMeshCallback(MeshAppearance & meshApp, void * context)
	{
		WallCutContext * const ctx = static_cast<WallCutContext *>(context);
		if (!ctx || !ctx->portalVolume)
			return;

		if (ctx->mode != WallCutContext::ModePortalVolume || !ctx->cell || !ctx->doorTransformCell)
			return;

		if (ctx->processedMeshes)
		{
			if (ctx->processedMeshes->find(&meshApp) != ctx->processedMeshes->end())
				return;
			ctx->processedMeshes->insert(&meshApp);
		}

		Object const & cellOwner = ctx->cell->getOwner();
		Transform doorWorld;
		if (ctx->hasDoorWorld)
			doorWorld = ctx->doorWorld;
		else
			doorWorld.multiply(cellOwner.getTransform_o2w(), *ctx->doorTransformCell);

		applyWallCutToMesh(meshApp, *ctx, doorWorld);
	}

	void forEachObjectMeshAppearances(Object & object, void (*callback)(MeshAppearance &, void *), void * context)
	{
		if (object.getAppearance())
			forEachLoadedMeshAppearance(object.getAppearance(), callback, context);

		int const childCount = object.getNumberOfChildObjects();
		for (int childIndex = 0; childIndex < childCount; ++childIndex)
		{
			Object * const childObject = object.getChildObject(childIndex);
			if (childObject)
				forEachObjectMeshAppearances(*childObject, callback, context);
		}
	}

	void applyWallCutToPeerAppearanceMeshes(Object const & hitObject, void (*callback)(MeshAppearance &, void *), void * context)
	{
		for (Object const * current = &hitObject; current; current = current->getAttachedTo())
		{
			Appearance * const appearance = const_cast<Appearance *>(current->getAppearance());
			if (!appearance)
				continue;

			ComponentAppearance * const componentApp = appearance->asComponentAppearance();
			if (componentApp)
			{
				ObjectList const & objectList = componentApp->getObjectList();
				for (int i = 0; i < objectList.getNumberOfObjects(); ++i)
				{
					Object * const childObject = const_cast<Object *>(objectList.getObject(i));
					if (childObject)
						forEachObjectMeshAppearances(*childObject, callback, context);
				}
				return;
			}

			DetailAppearance * const detailApp = appearance->asDetailAppearance();
			if (detailApp)
			{
				detailApp->forEachAllDetailMeshAppearances(callback, context);
				return;
			}
		}
	}

	void applyWallCutToObjectHierarchy(Object & object, void (*callback)(MeshAppearance &, void *), void * context)
	{
		forEachObjectMeshAppearances(object, callback, context);
		applyWallCutToPeerAppearanceMeshes(object, callback, context);

		for (Object * parent = object.getAttachedTo(); parent; parent = parent->getAttachedTo())
		{
			if (parent->getAppearance())
				forEachLoadedMeshAppearance(parent->getAppearance(), callback, context);
		}
	}

	void applyWallCutToHitObject(NetworkId const & hitObjectId, WallCutContext const & ctx)
	{
		if (!hitObjectId.isValid() || !ctx.cell || !ctx.doorTransformCell || !ctx.portalVolume)
			return;

		Object * const hitObject = NetworkIdManager::getObjectById(hitObjectId);
		if (!hitObject)
			return;

		applyWallCutToObjectHierarchy(*hitObject, applyWallCutMeshCallback, const_cast<WallCutContext *>(&ctx));
	}

	void forEachLoadedMeshAppearance(Appearance * appearance, void (*callback)(MeshAppearance &, void *), void * context)
	{
		if (!appearance || !callback)
			return;

		IGNORE_RETURN(appearance->alter(0.0f));

		if (MeshAppearance * const meshApp = dynamic_cast<MeshAppearance *>(appearance))
		{
			if (!meshApp->isLoaded())
				IGNORE_RETURN(appearance->alter(0.0f));
			if (meshApp->isLoaded())
				callback(*meshApp, context);
			return;
		}

		DetailAppearance * const detailApp = dynamic_cast<DetailAppearance *>(appearance);
		if (detailApp)
		{
			detailApp->forEachAllDetailMeshAppearances(callback, context);
			return;
		}

		ComponentAppearance * const componentApp = dynamic_cast<ComponentAppearance *>(appearance);
		if (componentApp)
		{
			ObjectList const & objectList = componentApp->getObjectList();
			for (int i = 0; i < objectList.getNumberOfObjects(); ++i)
			{
				Object const * const childObject = objectList.getObject(i);
				if (childObject && childObject->getAppearance())
					forEachLoadedMeshAppearance(const_cast<Appearance *>(childObject->getAppearance()), callback, context);
			}
		}
	}

	void forEachCellWallMeshAppearance(CellProperty * cell, void (*callback)(MeshAppearance &, void *), void * context)
	{
		if (!cell || !callback)
			return;

		Object & cellOwner = cell->getOwner();
		Object * const appearanceObject = cell->getAppearanceObject();

		if (appearanceObject)
			IGNORE_RETURN(appearanceObject->alter(0.0f));

		IGNORE_RETURN(cellOwner.alter(0.0f));

		if (cellOwner.getAppearance())
			forEachLoadedMeshAppearance(cellOwner.getAppearance(), callback, context);

		forEachObjectMeshAppearances(cellOwner, callback, context);

		if (appearanceObject)
			forEachObjectMeshAppearances(*appearanceObject, callback, context);
	}

	void forEachBuildingWallMeshAppearance(PortalProperty & portalProperty, void (*callback)(MeshAppearance &, void *), void * context)
	{
		Object & building = portalProperty.getOwner();
		IGNORE_RETURN(building.alter(0.0f));

		if (building.getAppearance())
			forEachLoadedMeshAppearance(building.getAppearance(), callback, context);

		int const cellCount = portalProperty.getNumberOfCells();
		for (int cellIndex = 1; cellIndex < cellCount; ++cellIndex)
		{
			CellProperty * const cell = portalProperty.getCell(cellIndex);
			if (cell)
				forEachCellWallMeshAppearance(cell, callback, context);
		}
	}

	void forceLoadTargetCellAppearance(CellProperty * cell)
	{
		if (!cell)
			return;

		Object & cellOwner = cell->getOwner();
		IGNORE_RETURN(cellOwner.alter(0.0f));

		Object * const appearanceObject = cell->getAppearanceObject();
		if (appearanceObject)
			IGNORE_RETURN(appearanceObject->alter(0.0f));

		forceLoadCellDetailMeshes(cell);
	}

	void forceLoadAllCellAppearances(PortalProperty & portalProperty)
	{
		int const cellCount = portalProperty.getNumberOfCells();
		for (int cellIndex = 1; cellIndex < cellCount; ++cellIndex)
		{
			CellProperty * const cell = portalProperty.getCell(cellIndex);
			if (!cell)
				continue;

			Object & cellOwner = cell->getOwner();
			IGNORE_RETURN(cellOwner.alter(0.0f));
			cellOwner.conclude();

			Object * const appearanceObject = cell->getAppearanceObject();
			if (appearanceObject)
			{
				IGNORE_RETURN(appearanceObject->alter(0.0f));
				appearanceObject->conclude();
			}
		}

		Object & building = portalProperty.getOwner();
		IGNORE_RETURN(building.alter(0.0f));
		building.conclude();
	}

	Appearance * getCellRenderAppearance(CellProperty * cell)
	{
		if (!cell)
			return 0;

		Object * const appearanceObject = cell->getAppearanceObject();
		if (appearanceObject && appearanceObject->getAppearance())
			return appearanceObject->getAppearance();

		return cell->getOwner().getAppearance();
	}

	void refreshCellRenderAppearance(CellProperty * cell)
	{
		if (!cell)
			return;

		Object * const appearanceObject = cell->getAppearanceObject();
		if (!appearanceObject)
			return;

		IGNORE_RETURN(appearanceObject->alter(0.0f));
		appearanceObject->conclude();
	}

	bool buildPortalVolumeCutFromGeometry(
		IndexedTriangleList const & geometry,
		Transform const & doorTransform_cell,
		PortalVolumeCut & outVolume,
		float maxWidth,
		float maxHeight)
	{
		if (geometry.getVertices().empty())
			return false;

		Transform invDoor;
		invDoor.invert(doorTransform_cell);

		bool first = true;
		for (size_t vi = 0; vi < geometry.getVertices().size(); ++vi)
		{
			Vector const local = invDoor.rotateTranslate_p2l(geometry.getVertices()[vi]);
			if (fabsf(local.z) > 0.30f)
				continue;

			if (first)
			{
				outVolume.minI = outVolume.maxI = local.x;
				outVolume.minJ = outVolume.maxJ = local.y;
				first = false;
			}
			else
			{
				outVolume.minI = std::min(outVolume.minI, local.x);
				outVolume.maxI = std::max(outVolume.maxI, local.x);
				outVolume.minJ = std::min(outVolume.minJ, local.y);
				outVolume.maxJ = std::max(outVolume.maxJ, local.y);
			}
		}

		if (first)
			return false;

		float const geomWidth = outVolume.maxI - outVolume.minI;
		float const geomHeight = outVolume.maxJ - outVolume.minJ;
		if (geomWidth > maxWidth * 2.0f || geomHeight > maxHeight * 2.0f || geomWidth < 0.05f || geomHeight < 0.05f)
			return false;

		float const centerI = (outVolume.minI + outVolume.maxI) * 0.5f;
		float const centerJ = outVolume.minJ;
		float const clampedWidth = std::min(geomWidth, maxWidth);
		float const clampedHeight = std::min(geomHeight, maxHeight);
		outVolume.minI = centerI - clampedWidth * 0.5f;
		outVolume.maxI = centerI + clampedWidth * 0.5f;
		outVolume.minJ = centerJ;
		outVolume.maxJ = centerJ + clampedHeight;

		float const pad = 0.08f;
		outVolume.minI -= pad;
		outVolume.maxI += pad;
		outVolume.minJ -= pad;
		outVolume.maxJ += pad;
		outVolume.inwardDepth = 0.50f;
		outVolume.outwardDepth = 1.00f;
		return true;
	}

	bool buildPortalVolumeCutFromMaterializedPortal(
		PortalProperty const & portalProperty,
		int cellIndex,
		int portalIndex,
		Transform const & doorTransform_cell,
		float maxWidth,
		float maxHeight,
		PortalVolumeCut & outVolume)
	{
		IndexedTriangleList const * const geometry = portalProperty.getCellPortalGeometry(cellIndex, portalIndex);
		if (!geometry || geometry->getVertices().empty())
			return false;

		return buildPortalVolumeCutFromGeometry(*geometry, doorTransform_cell, outVolume, maxWidth, maxHeight);
	}

	bool buildGraftPortalVolumeCut(
		PortalProperty const & portalProperty,
		PortalProperty::DynamicRoomGraft const & graft,
		float fallbackWidth,
		float fallbackHeight,
		PortalVolumeCut & outVolume)
	{
		UNREF(portalProperty);
		if (graft.donorPobName.empty())
			return DynamicBunkerClient::buildPortalVolumeCutFromDimensions(fallbackWidth, fallbackHeight, outVolume);

		PortalPropertyTemplate const * const tmpl = PortalPropertyTemplateList::fetch(CrcLowerString(graft.donorPobName.c_str()));
		if (!tmpl)
			return DynamicBunkerClient::buildPortalVolumeCutFromDimensions(fallbackWidth, fallbackHeight, outVolume);

		bool ok = false;
		int const donorCellIndex = graft.donorCellIndex >= 1 ? graft.donorCellIndex : 1;
		int const donorPortalIndex = graft.graftedPortalIndex >= 0 ? graft.graftedPortalIndex : 0;

		PortalPropertyTemplate::PortalOwnersList const & owners = tmpl->getPortalOwnersList();
		for (size_t portalGeometryIndex = 0; portalGeometryIndex < owners.size(); ++portalGeometryIndex)
		{
			PortalPropertyTemplate::PortalOwners const & owner = owners[portalGeometryIndex];
			if (owner.owners[0].cell != donorCellIndex || owner.owners[0].portal != donorPortalIndex)
				continue;

			IndexedTriangleList const * const geometry = tmpl->getPortalGeometry(
				static_cast<int>(portalGeometryIndex), donorCellIndex, donorPortalIndex, false);
			if (!geometry || geometry->getVertices().empty())
				break;

			if (donorCellIndex < 1 || donorCellIndex >= tmpl->getNumberOfCells())
				break;

			PortalPropertyTemplateCell const & donorCell = tmpl->getCell(donorCellIndex);
			PortalPropertyTemplateCell::PortalPropertyTemplateCellPortalList const * const portalList = donorCell.getPortalList();
			if (!portalList || donorPortalIndex < 0 || donorPortalIndex >= static_cast<int>(portalList->size()))
				break;

			Transform const doorTransform_cell = (*portalList)[static_cast<size_t>(donorPortalIndex)]->getDoorTransform(false);
			ok = buildPortalVolumeCutFromGeometry(*geometry, doorTransform_cell, outVolume, fallbackWidth, fallbackHeight);
			break;
		}

		tmpl->release();
		if (!ok)
			ok = DynamicBunkerClient::buildPortalVolumeCutFromDimensions(fallbackWidth, fallbackHeight, outVolume);
		return ok;
	}

	void clearWallCutMeshCallback(MeshAppearance & meshApp, void * context)
	{
		UNREF(context);
		meshApp.clearPendingPortalCuts();
	}

	void clearCellWallVisualCuts(CellProperty * cell)
	{
		forEachCellWallMeshAppearance(cell, clearWallCutMeshCallback, 0);
	}

	void applyWallCutForPortal(
		PortalProperty & portalProperty,
		int cellIndex,
		Transform doorTransform_o2p,
		float width,
		float height,
		PortalVolumeCut const * portalVolume = 0,
		bool usePreviewWallHit = false,
		int materializedPortalIndex = -1,
		int customSocketIndex = -1)
	{
		CellProperty * const cell = resolveCellForPlacement(portalProperty, cellIndex);
		if (!cell)
			return;

		forceLoadAllCellAppearances(portalProperty);
		forceLoadTargetCellAppearance(cell);
		ensureWallCutRenderHooks(portalProperty);
		syncCellAppearanceMeshTransforms(cell);

		snapDoorTransformYToCellFloor(cell, doorTransform_o2p);
		disableDetailWriteModelForCell(cell);

		PortalVolumeCut dimensionVolume;
		PortalVolumeCut const * volume = portalVolume;
		if (!volume)
		{
			DynamicBunkerClient::buildPortalVolumeCutFromDimensions(width, height, dimensionVolume);
			volume = &dimensionVolume;
		}

		PortalVolumeCut floorVolume;
		buildFloorPortalVolumeCut(*volume, floorVolume);

		WallCutContext ctx;
		ctx.mode = WallCutContext::ModePortalVolume;
		ctx.cutBox = 0;
		ctx.doorTransform = 0;
		ctx.doorTransformCell = &doorTransform_o2p;
		ctx.cell = cell;
		ctx.doorwayWidth = width;
		ctx.doorwayHeight = height;
		ctx.portalVolume = volume;
		ctx.portalFloorVolume = &floorVolume;
		ctx.usePreviewWallHit = usePreviewWallHit;
		ctx.hasDoorWorld = computePortalDoorWorld(
			portalProperty, cellIndex, materializedPortalIndex, doorTransform_o2p, ctx.doorWorld);
		std::set<MeshAppearance *> processedMeshes;
		ctx.processedMeshes = &processedMeshes;

		NetworkId const buildingId = portalProperty.getOwner().getNetworkId();
		NetworkId wallHitObjectId;
		if (usePreviewWallHit)
		{
			std::map<NetworkId, PreviewWallHit>::const_iterator previewIt = s_previewWallHits.find(buildingId);
			if (previewIt != s_previewWallHits.end() && previewIt->second.cellIndex == cellIndex)
				wallHitObjectId = previewIt->second.hitObjectId;
		}
		else if (customSocketIndex >= 0)
			wallHitObjectId = lookupSocketWallHitObject(buildingId, cellIndex, customSocketIndex);

		if (!wallHitObjectId.isValid())
		{
			wallHitObjectId = findWallHitObjectAtPortal(
				portalProperty, cellIndex, materializedPortalIndex, doorTransform_o2p);
			if (wallHitObjectId.isValid() && customSocketIndex >= 0)
			{
				SocketWallHitKey key;
				key.buildingId = buildingId;
				key.cellIndex = cellIndex;
				key.socketIndex = customSocketIndex;
				s_socketWallHits[key] = wallHitObjectId;
			}
		}

		if (wallHitObjectId.isValid())
			applyWallCutToHitObject(wallHitObjectId, ctx);

		Object & building = portalProperty.getOwner();
		IGNORE_RETURN(building.alter(0.0f));
		if (building.getAppearance())
			forEachLoadedMeshAppearance(building.getAppearance(), applyWallCutMeshCallback, &ctx);

		forEachCellWallMeshAppearance(cell, applyWallCutMeshCallback, &ctx);

		if (ctx.processedMeshes)
		{
			for (std::set<MeshAppearance *>::iterator meshIt = ctx.processedMeshes->begin();
				meshIt != ctx.processedMeshes->end(); ++meshIt)
			{
				if (*meshIt)
					(*meshIt)->applyPendingPortalCuts();
			}
		}

		int const cellCount = portalProperty.getNumberOfCells();
		for (int refreshCellIndex = 1; refreshCellIndex < cellCount; ++refreshCellIndex)
		{
			CellProperty * const refreshCell = portalProperty.getCell(refreshCellIndex);
			if (refreshCell)
				refreshCellRenderAppearance(refreshCell);
		}
	}

	void clearWallVisualCuts(PortalProperty & portalProperty)
	{
		MeshAppearance::clearBuildingCellPortalCuts(portalProperty.getOwner().getNetworkId());
		MeshAppearance::clearActivePortalCutBuildingId();
		forEachBuildingWallMeshAppearance(portalProperty, clearWallCutMeshCallback, 0);

		int const cellCount = portalProperty.getNumberOfCells();
		for (int cellIndex = 1; cellIndex < cellCount; ++cellIndex)
		{
			CellProperty * const cell = portalProperty.getCell(cellIndex);
			if (cell)
				restoreDetailWriteModelForCell(cell);
		}
	}

	void applyPreviewWallCutInternal(
		PortalProperty & portalProperty,
		int cellIndex,
		Transform const & doorTransform_cell,
		float width,
		float height,
		PortalVolumeCut const * portalVolume,
		NetworkId const & wallHitObjectId,
		bool storeAsActivePreview)
	{
		if (cellIndex < 1)
			return;

		CellProperty * const cell = resolveCellForPlacement(portalProperty, cellIndex);
		if (!cell)
			return;

		NetworkId const buildingId = portalProperty.getOwner().getNetworkId();
		if (storeAsActivePreview)
		{
			s_activePreviewWallCut.buildingId = buildingId;
			s_activePreviewWallCut.cellIndex = cellIndex;
			s_activePreviewWallCut.active = true;
		}

		forceLoadAllCellAppearances(portalProperty);
		forceLoadTargetCellAppearance(cell);
		ensureWallCutRenderHooks(portalProperty);
		syncCellAppearanceMeshTransforms(cell);
		disableDetailWriteModelForCell(cell);

		Transform snappedDoor = doorTransform_cell;
		snapDoorTransformYToCellFloor(cell, snappedDoor);

		PortalVolumeCut dimensionVolume;
		PortalVolumeCut const * volume = portalVolume;
		if (!volume)
		{
			DynamicBunkerClient::buildPortalVolumeCutFromDimensions(width, height, dimensionVolume);
			volume = &dimensionVolume;
		}

		if (wallHitObjectId.isValid())
		{
			PreviewWallHit hit;
			hit.cellIndex = cellIndex;
			hit.hitObjectId = wallHitObjectId;
			s_previewWallHits[buildingId] = hit;
		}

		RegisteredCellPortalCut previewCut;
		previewCut.doorTransform_cell = snappedDoor;
		previewCut.volume = *volume;
		std::vector<RegisteredCellPortalCut> previewCuts;
		previewCuts.push_back(previewCut);
		MeshAppearance::setBuildingCellPortalCuts(buildingId, cellIndex, previewCuts);
		MeshAppearance::setActivePortalCutBuildingId(buildingId);

		applyWallCutForPortal(
			portalProperty, cellIndex, snappedDoor, width, height, volume, true);

		if (storeAsActivePreview)
		{
			s_activePreviewWallCut.buildingId = buildingId;
			s_activePreviewWallCut.cellIndex = cellIndex;
			s_activePreviewWallCut.hitObjectId = wallHitObjectId;
			s_activePreviewWallCut.doorTransform_cell = snappedDoor;
			s_activePreviewWallCut.width = width;
			s_activePreviewWallCut.height = height;
			s_activePreviewWallCut.volume = *volume;
			s_activePreviewWallCut.hasVolume = true;
			s_activePreviewWallCut.active = true;
		}
	}

	void reapplyActivePreviewWallCut(PortalProperty & portalProperty)
	{
		if (!s_activePreviewWallCut.active)
			return;

		if (portalProperty.getOwner().getNetworkId() != s_activePreviewWallCut.buildingId)
			return;

		PortalVolumeCut const * volumePtr = 0;
		if (s_activePreviewWallCut.hasVolume)
			volumePtr = &s_activePreviewWallCut.volume;

		applyPreviewWallCutInternal(
			portalProperty,
			s_activePreviewWallCut.cellIndex,
			s_activePreviewWallCut.doorTransform_cell,
			s_activePreviewWallCut.width,
			s_activePreviewWallCut.height,
			volumePtr,
			s_activePreviewWallCut.hitObjectId,
			false);
	}

	void applyWallVisualCuts(PortalProperty & portalProperty)
	{
		clearWallVisualCuts(portalProperty);

		NetworkId const buildingId = portalProperty.getOwner().getNetworkId();
		std::map<int, std::vector<RegisteredCellPortalCut> > cellCuts;

		PortalProperty::CustomSocketList const & sockets = portalProperty.getCustomSockets();
		for (size_t i = 0; i < sockets.size(); ++i)
		{
			PortalProperty::CustomSocket const & socket = sockets[i];
			if (socket.materializedPortalIndex < 0)
				continue;

			float const width = socket.doorwayWidth > 0.01f ? socket.doorwayWidth : 1.0f;
			float const height = socket.doorwayHeight > 0.01f ? socket.doorwayHeight : 2.0f;

			CellProperty * const hostCell = portalProperty.getCell(socket.cellIndex);
			Transform hostDoorTransform_cell;
			if (!resolveMaterializedPortalDoorTransform_cell(
					portalProperty, socket.cellIndex, socket.materializedPortalIndex, hostDoorTransform_cell))
			{
				hostDoorTransform_cell = socket.doorTransform_o2p;
				snapDoorTransformYToCellFloor(hostCell, hostDoorTransform_cell);
			}
			disableDetailWriteModelForCell(hostCell);

			PortalProperty::DynamicRoomGraft graft;
			bool const hasGraft = portalProperty.findDynamicRoomGraftForSocket(socket.cellIndex, socket.socketIndex, graft);

			PortalVolumeCut hostVolume;
			PortalVolumeCut const * hostVolumePtr = 0;
			char const * const hostDoorStyle = Portal::pickDynamicBunkerDoorStyle(hostDoorTransform_cell);

			if (buildPortalVolumeCutFromMaterializedPortal(
					portalProperty, socket.cellIndex, socket.materializedPortalIndex, hostDoorTransform_cell, width, height, hostVolume))
				hostVolumePtr = &hostVolume;
			else if (hasGraft && buildGraftPortalVolumeCut(portalProperty, graft, width, height, hostVolume))
				hostVolumePtr = &hostVolume;
			else if (DynamicBunkerClient::buildPortalVolumeCutFromDoorStyle(hostDoorStyle, width, height, hostVolume))
				hostVolumePtr = &hostVolume;
			if (!hostVolumePtr)
			{
				DynamicBunkerClient::buildPortalVolumeCutFromDimensions(width, height, hostVolume);
				hostVolumePtr = &hostVolume;
			}

			RegisteredCellPortalCut hostCut;
			hostCut.doorTransform_cell = hostDoorTransform_cell;
			hostCut.volume = *hostVolumePtr;
			cellCuts[socket.cellIndex].push_back(hostCut);
			MeshAppearance::setBuildingCellPortalCuts(buildingId, socket.cellIndex, cellCuts[socket.cellIndex]);
			MeshAppearance::setActivePortalCutBuildingId(buildingId);

			applyWallCutForPortal(
				portalProperty, socket.cellIndex, hostDoorTransform_cell, width, height,
				hostVolumePtr, false, socket.materializedPortalIndex, socket.socketIndex);

			if (!hasGraft)
				continue;

			CellProperty * const graftCell = portalProperty.getCell(graft.graftedCellIndex);
			if (!graftCell)
				continue;

			int const graftPortalIndex = PortalProperty::resolveCellPortalIndex(graftCell, graft.graftedPortalIndex);
			if (graftPortalIndex < 0)
				continue;

			Transform graftPortalBuilding;
			if (!getPortalBuildingTransform(portalProperty, graft.graftedCellIndex, graftPortalIndex, graftPortalBuilding))
				continue;

			Transform graftDoorTransform_cell;
			if (!resolveMaterializedPortalDoorTransform_cell(
					portalProperty, graft.graftedCellIndex, graftPortalIndex, graftDoorTransform_cell))
			{
				Transform const & graftCellToBuilding = graftCell->getOwner().getTransform_o2p();
				Transform buildingToGraftCell;
				buildingToGraftCell.invert(graftCellToBuilding);
				graftDoorTransform_cell.multiply(buildingToGraftCell, graftPortalBuilding);
				snapDoorTransformYToCellFloor(graftCell, graftDoorTransform_cell);
			}
			disableDetailWriteModelForCell(graftCell);

			PortalVolumeCut graftVolume;
			if (buildPortalVolumeCutFromMaterializedPortal(
					portalProperty, graft.graftedCellIndex, graftPortalIndex, graftDoorTransform_cell, width, height, graftVolume))
			{
				// volume set from runtime portal geometry
			}
			else if (!buildGraftPortalVolumeCut(portalProperty, graft, width, height, graftVolume))
				IGNORE_RETURN(DynamicBunkerClient::buildPortalVolumeCutFromDoorStyle(hostDoorStyle, width, height, graftVolume));

			RegisteredCellPortalCut graftCut;
			graftCut.doorTransform_cell = graftDoorTransform_cell;
			graftCut.volume = graftVolume;
			cellCuts[graft.graftedCellIndex].push_back(graftCut);
			MeshAppearance::setBuildingCellPortalCuts(buildingId, graft.graftedCellIndex, cellCuts[graft.graftedCellIndex]);
			MeshAppearance::setActivePortalCutBuildingId(buildingId);

			applyWallCutForPortal(
				portalProperty, graft.graftedCellIndex, graftDoorTransform_cell, width, height,
				&graftVolume, false, graftPortalIndex);
		}

		for (std::map<int, std::vector<RegisteredCellPortalCut> >::const_iterator it = cellCuts.begin(); it != cellCuts.end(); ++it)
			MeshAppearance::setBuildingCellPortalCuts(buildingId, it->first, it->second);

		if (!cellCuts.empty())
			MeshAppearance::setActivePortalCutBuildingId(buildingId);

		reapplyActivePreviewWallCut(portalProperty);
	}

	void recordBridgeForGraft(Object & building, PortalProperty & portalProperty, PortalProperty::DynamicRoomGraft const & graft)
	{
		UNREF(building);
		Transform hostTransform;
		Transform graftTransform;
		if (!getPortalBuildingTransform(portalProperty, graft.hostCellIndex, graft.hostPortalIndex, hostTransform))
			return;

		CellProperty *const graftCell = portalProperty.getCell(graft.graftedCellIndex);
		int const resolvedGraftPortal = PortalProperty::resolveCellPortalIndex(graftCell, graft.graftedPortalIndex);
		if (resolvedGraftPortal < 0)
			return;
		if (!getPortalBuildingTransform(portalProperty, graft.graftedCellIndex, resolvedGraftPortal, graftTransform))
			return;

		float width = 2.5f;
		float height = 3.0f;
		if (PortalProperty::isCustomSocketIndex(graft.hostPortalIndex))
		{
			PortalProperty::CustomSocket customSocket;
			if (portalProperty.findCustomSocket(graft.hostCellIndex, graft.hostPortalIndex, customSocket))
			{
				width = std::max(0.5f, customSocket.doorwayWidth);
				height = std::max(0.5f, customSocket.doorwayHeight);
			}
		}

		Vector const delta = graftTransform.getPosition_p() - hostTransform.getPosition_p();
		float const gap = delta.magnitude();
		if (gap < 0.05f)
			return;

		PortalProperty::BridgeSegment segment;
		segment.hostCellIndex = graft.hostCellIndex;
		segment.hostPortalIndex = graft.hostPortalIndex;
		segment.graftedCellIndex = graft.graftedCellIndex;
		segment.graftedPortalIndex = resolvedGraftPortal;
		segment.transform_o2p.setPosition_p((hostTransform.getPosition_p() + graftTransform.getPosition_p()) * 0.5f);
		if (gap > 0.01f)
			segment.transform_o2p.yaw_l(atan2f(delta.x, delta.z));
		segment.length = std::max(0.5f, gap);
		segment.width = width;
		segment.height = height;
		portalProperty.recordBridgeSegment(segment);
	}



	void refreshBridgesFromPortalProperty(Object & building, PortalProperty & portalProperty)
	{
		clearBuildingBridges(building.getNetworkId());
		portalProperty.clearBridgeSegments();
	}

	bool isGraftAlreadyLinked(PortalProperty const &portalProperty, PortalProperty::DynamicRoomGraft const &graft)
	{
		if (!portalProperty.getCell(graft.hostCellIndex) || !portalProperty.getCell(graft.graftedCellIndex))
			return false;

		CellProperty const *const graftCell = portalProperty.getCell(graft.graftedCellIndex);
		int const resolvedGraftPortal = PortalProperty::resolveCellPortalIndex(graftCell, graft.graftedPortalIndex);
		if (resolvedGraftPortal < 0)
			return false;

		int linkedCell = -1;
		int linkedPortal = -1;

		if (PortalProperty::isCustomSocketIndex(graft.hostPortalIndex))
		{
			PortalProperty::CustomSocket customSocket;
			if (!portalProperty.findCustomSocket(graft.hostCellIndex, graft.hostPortalIndex, customSocket))
				return false;
			if (customSocket.open || customSocket.materializedPortalIndex < 0)
				return false;
			if (!portalProperty.getPortalNeighbor(graft.hostCellIndex, customSocket.materializedPortalIndex, linkedCell, linkedPortal))
				return false;
		}
		else if (!portalProperty.getPortalNeighbor(graft.hostCellIndex, graft.hostPortalIndex, linkedCell, linkedPortal))
		{
			return false;
		}

		return linkedCell == graft.graftedCellIndex && linkedPortal == resolvedGraftPortal;
	}

	bool linkGraftRecord(PortalProperty &portalProperty, PortalProperty::DynamicRoomGraft const &graft)
	{
		if (!portalProperty.getCell(graft.hostCellIndex) || !portalProperty.getCell(graft.graftedCellIndex))
			return false;

		if (isGraftAlreadyLinked(portalProperty, graft))
			return false;

		CellProperty *const graftCell = portalProperty.getCell(graft.graftedCellIndex);
		int const resolvedGraftPortal = PortalProperty::resolveCellPortalIndex(graftCell, graft.graftedPortalIndex);
		if (resolvedGraftPortal < 0)
			return false;

		if (PortalProperty::isCustomSocketIndex(graft.hostPortalIndex))
		{
			if (!portalProperty.linkCustomSocketGraft(graft.hostCellIndex, graft.hostPortalIndex, graft.graftedCellIndex, resolvedGraftPortal))
				return false;
			IGNORE_RETURN(portalProperty.markCustomSocketOpen(graft.hostCellIndex, graft.hostPortalIndex, false));
		}
		else
		{
			if (!portalProperty.linkCellPortals(graft.hostCellIndex, graft.hostPortalIndex, graft.graftedCellIndex, resolvedGraftPortal))
				return false;
		}

		return true;
	}

	void materializeAllCustomSockets(PortalProperty &portalProperty)
	{
		PortalProperty::CustomSocketList const &sockets = portalProperty.getCustomSockets();
		int const cellCount = portalProperty.getNumberOfCells();
		for (size_t i = 0; i < sockets.size(); ++i)
		{
			PortalProperty::CustomSocket const &socket = sockets[i];
			if (socket.materializedPortalIndex >= 0)
				continue;
			if (socket.cellIndex < 1 || socket.cellIndex >= cellCount)
				continue;

			CellProperty * const cell = portalProperty.getCell(socket.cellIndex);
			if (!cell)
				continue;

			IGNORE_RETURN(portalProperty.materializeCustomSocketPortal(socket.cellIndex, socket.socketIndex));
		}
	}

	void prepareGraftPortalLinking(PortalProperty &portalProperty)
	{
		materializeAllCustomSockets(portalProperty);
	}



	Listener::Listener()

	: MessageDispatch::Receiver()

	{

		connectToMessage(DynamicBunkerGraftMessage::MessageType);
		connectToMessage(DynamicBunkerGraftTransformMessage::MessageType);
		connectToMessage(DynamicBunkerUngraftMessage::MessageType);

		connectToMessage(DynamicBunkerCustomSocketSyncMessage::MessageType);

		connectToMessage(DynamicBunkerBuildingRevertedMessage::MessageType);

	}



	void Listener::receiveMessage(MessageDispatch::Emitter const &, MessageDispatch::MessageBase const & message)

	{

		GameNetworkMessage const *const gnm = dynamic_cast<GameNetworkMessage const *>(&message);

		if (!gnm)

			return;



		if (message.isType(DynamicBunkerGraftMessage::MessageType))

		{

			Archive::ReadIterator ri = gnm->getByteStream().begin();

			DynamicBunkerGraftMessage const msg(ri);

			DynamicBunkerClient::handleGraftMessage(msg);

			return;

		}

		if (message.isType(DynamicBunkerGraftTransformMessage::MessageType))
		{
			Archive::ReadIterator ri = gnm->getByteStream().begin();
			DynamicBunkerGraftTransformMessage const msg(ri);
			DynamicBunkerClient::handleGraftTransformMessage(msg);
			return;
		}



		if (message.isType(DynamicBunkerUngraftMessage::MessageType))

		{

			Archive::ReadIterator ri = gnm->getByteStream().begin();

			DynamicBunkerUngraftMessage const msg(ri);

			DynamicBunkerClient::handleUngraftMessage(msg);

			return;

		}



		if (message.isType(DynamicBunkerCustomSocketSyncMessage::MessageType))

		{

			Archive::ReadIterator ri = gnm->getByteStream().begin();

			DynamicBunkerCustomSocketSyncMessage const msg(ri);

			DynamicBunkerClient::handleCustomSocketSyncMessage(msg);

			return;

		}



		if (message.isType(DynamicBunkerBuildingRevertedMessage::MessageType))

		{

			Archive::ReadIterator ri = gnm->getByteStream().begin();

			DynamicBunkerBuildingRevertedMessage const msg(ri);

			DynamicBunkerClient::handleBuildingRevertedMessage(msg);

		}

	}

}



using namespace DynamicBunkerClientNamespace;



// ======================================================================



void DynamicBunkerClient::install()

{

	s_listener = new Listener;

	ExitChain::add(DynamicBunkerClient::remove, "DynamicBunkerClient::remove");

}



// ----------------------------------------------------------------------



void DynamicBunkerClient::remove()

{

	for (BuildingBridgeMap::iterator it = s_buildingBridges.begin(); it != s_buildingBridges.end(); ++it)

	{

		for (size_t i = 0; i < it->second.size(); ++i)

			delete it->second[i];

	}

	s_buildingBridges.clear();

	s_pendingGraftCellLoads.clear();



	delete s_listener;

	s_listener = 0;

}



// ----------------------------------------------------------------------



void DynamicBunkerClient::handleGraftMessage(DynamicBunkerGraftMessage const &message)

{

	Object *const buildingObject = NetworkIdManager::getObjectById(message.getBuildingId());

	if (!buildingObject)

	{

		WARNING(true, ("DynamicBunkerClient - building %s not found", message.getBuildingId().getValueString().c_str()));

		return;

	}



	PortalProperty *const portalProperty = buildingObject->getPortalProperty();

	if (!portalProperty)

	{

		WARNING(true, ("DynamicBunkerClient - building %s has no PortalProperty", message.getBuildingId().getValueString().c_str()));

		return;

	}



	if (!portalProperty->ensureGraftedCellSlot(message.getGraftedCellIndex(), message.getDonorPobName().c_str(), message.getDonorCellIndex()))

	{

		WARNING(true, ("DynamicBunkerClient - ensureGraftedCellSlot failed for %s", message.getDonorPobName().c_str()));

		return;

	}



	PortalProperty::DynamicRoomGraft graft;

	graft.graftedCellIndex = message.getGraftedCellIndex();

	graft.hostCellIndex = message.getHostCellIndex();

	graft.hostPortalIndex = message.getHostPortalIndex();

	graft.graftedPortalIndex = message.getGraftedPortalIndex();

	graft.donorCellIndex = message.getDonorCellIndex();

	graft.donorPobName = message.getDonorPobName();

	portalProperty->recordDynamicRoomGraft(graft);



	Object *const cellObject = NetworkIdManager::getObjectById(message.getCellId());

	if (cellObject)
	{
		if (!portalProperty->getCell(message.getGraftedCellIndex()))
			portalProperty->cellLoaded(message.getGraftedCellIndex(), *cellObject, true);

		Transform cellTransform = message.getCellTransform();
		DynamicBunkerClient::ensureRightHandedCellTransform(cellTransform);
		cellObject->setTransform_o2p(cellTransform);
	}
	else if (message.getCellId().isValid())
	{
		Transform cellTransform = message.getCellTransform();
		DynamicBunkerClient::ensureRightHandedCellTransform(cellTransform);
		queuePendingGraftCellLoad(message.getBuildingId(), message.getCellId(), message.getGraftedCellIndex(), cellTransform);
	}

	if (PortalProperty::isCustomSocketIndex(message.getHostPortalIndex())
		&& !customSocketAlreadyMaterialized(*portalProperty, message.getHostCellIndex(), message.getHostPortalIndex()))
	{
		IGNORE_RETURN(portalProperty->materializeCustomSocketPortal(message.getHostCellIndex(), message.getHostPortalIndex()));
	}

	tryLoadPendingGraftCells(*buildingObject, *portalProperty);
	prepareGraftPortalLinking(*portalProperty);
	DynamicBunkerClient::tryLinkAllPendingGrafts(*buildingObject, *portalProperty);
	finalizeLinkedCustomSocketGrafts(*portalProperty);
	refreshBridgesFromPortalProperty(*buildingObject, *portalProperty);
	finalizeBuildingPortalChanges(*buildingObject, *portalProperty);
}



// ----------------------------------------------------------------------



void DynamicBunkerClient::handleGraftTransformMessage(DynamicBunkerGraftTransformMessage const &message)
{
	Object *const buildingObject = NetworkIdManager::getObjectById(message.getBuildingId());
	if (!buildingObject)
		return;

	PortalProperty *const portalProperty = buildingObject->getPortalProperty();
	if (!portalProperty)
		return;

	CellProperty *const cell = portalProperty->getCell(message.getGraftedCellIndex());
	if (!cell)
		return;

	Object *const cellObject = &cell->getOwner();
	Transform cellTransform = message.getCellTransform_o2p();
	DynamicBunkerClient::ensureRightHandedCellTransform(cellTransform);
	cellObject->setTransform_o2p(cellTransform);
	finalizeBuildingPortalChanges(*buildingObject, *portalProperty);
}



// ----------------------------------------------------------------------



void DynamicBunkerClient::handleUngraftMessage(DynamicBunkerUngraftMessage const &message)

{

	Object *const buildingObject = NetworkIdManager::getObjectById(message.getBuildingId());

	if (!buildingObject)

	{

		WARNING(true, ("DynamicBunkerClient ungraft - building %s not found", message.getBuildingId().getValueString().c_str()));

		return;

	}



	PortalProperty *const portalProperty = buildingObject->getPortalProperty();

	if (!portalProperty)

	{

		WARNING(true, ("DynamicBunkerClient ungraft - building %s has no PortalProperty", message.getBuildingId().getValueString().c_str()));

		return;

	}



	if (PortalProperty::isCustomSocketIndex(message.getHostPortalIndex()))
	{
		IGNORE_RETURN(portalProperty->unlinkHostPortal(message.getHostCellIndex(), message.getHostPortalIndex()));
	}
	else
	{
		IGNORE_RETURN(portalProperty->unlinkCellPortal(message.getHostCellIndex(), message.getHostPortalIndex()));
	}

	Object *const graftedCellObject = message.getCellId().isValid()
		? NetworkIdManager::getObjectById(message.getCellId())
		: 0;
	if (graftedCellObject)
	{
		CellProperty *const graftedCell = graftedCellObject->getCellProperty();
		if (graftedCell)
			graftedCell->clearAllPortalNeighbors();
	}

	IGNORE_RETURN(portalProperty->clearLoadedCellSlot(message.getGraftedCellIndex()));

	IGNORE_RETURN(portalProperty->removeDynamicRoomGraft(message.getGraftedCellIndex()));

	IGNORE_RETURN(portalProperty->releaseGraftedCellSlot(message.getGraftedCellIndex()));



	if (PortalProperty::isCustomSocketIndex(message.getHostPortalIndex()))

		IGNORE_RETURN(portalProperty->markCustomSocketOpen(message.getHostCellIndex(), message.getHostPortalIndex(), true));



	refreshBridgesFromPortalProperty(*buildingObject, *portalProperty);

	finalizeBuildingPortalChanges(*buildingObject, *portalProperty);

}



// ----------------------------------------------------------------------



void DynamicBunkerClient::handleCustomSocketSyncMessage(DynamicBunkerCustomSocketSyncMessage const &message)

{

	Object *const buildingObject = NetworkIdManager::getObjectById(message.getBuildingId());

	if (!buildingObject)

		return;



	PortalProperty *const portalProperty = buildingObject->getPortalProperty();

	if (!portalProperty)

		return;



	PortalProperty::CustomSocket socket;

	socket.cellIndex = message.getCellIndex();

	socket.socketIndex = message.getSocketIndex();

	socket.label = message.getLabel();

	socket.doorTransform_o2p = message.getDoorTransform_o2p();

	socket.open = message.getOpen();

	socket.doorwayWidth = message.getDoorwayWidth();

	socket.doorwayHeight = message.getDoorwayHeight();

	bool const alreadyMaterialized = customSocketAlreadyMaterialized(*portalProperty, socket.cellIndex, socket.socketIndex);

	IGNORE_RETURN(portalProperty->addCustomSocket(socket));
	registerPendingSocketWallHit(message.getBuildingId(), message.getCellIndex(), message.getSocketIndex());

	if (!alreadyMaterialized)
		IGNORE_RETURN(portalProperty->materializeCustomSocketPortal(socket.cellIndex, socket.socketIndex));

	IGNORE_RETURN(portalProperty->finalizeCustomSocketPortalWalkthrough(socket.cellIndex, socket.socketIndex));
	tryLoadPendingGraftCells(*buildingObject, *portalProperty);
	tryLinkAllPendingGrafts(*buildingObject, *portalProperty);
	finalizeLinkedCustomSocketGrafts(*portalProperty);
	finalizeBuildingPortalChanges(*buildingObject, *portalProperty);
}



// ----------------------------------------------------------------------



void DynamicBunkerClient::handleBuildingRevertedMessage(DynamicBunkerBuildingRevertedMessage const &message)

{

	Object *const buildingObject = NetworkIdManager::getObjectById(message.getBuildingId());

	if (!buildingObject)

		return;



	PortalProperty *const portalProperty = buildingObject->getPortalProperty();

	if (!portalProperty)

		return;



	while (!portalProperty->getDynamicRoomGrafts().empty())

	{

		PortalProperty::DynamicRoomGraft const graft = portalProperty->getDynamicRoomGrafts().front();

		if (PortalProperty::isCustomSocketIndex(graft.hostPortalIndex))

			IGNORE_RETURN(portalProperty->unlinkHostPortal(graft.hostCellIndex, graft.hostPortalIndex));

		else

			IGNORE_RETURN(portalProperty->unlinkCellPortal(graft.hostCellIndex, graft.hostPortalIndex));



		CellProperty *const graftedCell = portalProperty->getCell(graft.graftedCellIndex);

		if (graftedCell)

			graftedCell->clearAllPortalNeighbors();



		IGNORE_RETURN(portalProperty->clearLoadedCellSlot(graft.graftedCellIndex));

		IGNORE_RETURN(portalProperty->removeDynamicRoomGraft(graft.graftedCellIndex));

		IGNORE_RETURN(portalProperty->releaseGraftedCellSlot(graft.graftedCellIndex));

	}



	portalProperty->dematerializeAllCustomSocketPortals();

	portalProperty->clearCustomSockets();

	portalProperty->clearBridgeSegments();

	clearBuildingBridges(message.getBuildingId());
	clearBuildingDoorways(message.getBuildingId());
	clearSocketWallHitsForBuilding(message.getBuildingId());

	finalizeBuildingPortalChanges(*buildingObject, *portalProperty);

}



// ----------------------------------------------------------------------



void DynamicBunkerClient::syncCustomSocketsFromOpenFloorplan(DynamicBunkerOpenFloorplanMessage const &message)

{

	Object *const buildingObject = NetworkIdManager::getObjectById(message.getBuildingId());

	if (!buildingObject)

		return;



	PortalProperty *const portalProperty = buildingObject->getPortalProperty();

	if (!portalProperty)

		return;



	DynamicBunkerOpenFloorplanMessage::CustomSocketList const &entries = message.getCustomSockets();
	std::set<int> serverSocketIndices;

	for (size_t i = 0; i < entries.size(); ++i)
	{
		DynamicBunkerOpenFloorplanMessage::CustomSocketEntry const &entry = entries[i];
		serverSocketIndices.insert(entry.socketIndex);

		PortalProperty::CustomSocket socket;
		socket.cellIndex = entry.cellIndex;
		socket.socketIndex = entry.socketIndex;
		socket.label = entry.label;
		socket.doorTransform_o2p = entry.doorTransform_o2p;
		socket.open = entry.open;
		socket.doorwayWidth = entry.doorwayWidth;
		socket.doorwayHeight = entry.doorwayHeight;
		IGNORE_RETURN(portalProperty->addCustomSocket(socket));
	}

	std::vector<int> staleSocketIndices;
	{
		PortalProperty::CustomSocketList const &localSockets = portalProperty->getCustomSockets();
		for (size_t i = 0; i < localSockets.size(); ++i)
		{
			if (serverSocketIndices.find(localSockets[i].socketIndex) == serverSocketIndices.end())
				staleSocketIndices.push_back(localSockets[i].socketIndex);
		}
	}
	for (size_t i = 0; i < staleSocketIndices.size(); ++i)
		IGNORE_RETURN(portalProperty->removeCustomSocket(staleSocketIndices[i]));

	prepareGraftPortalLinking(*portalProperty);
	tryLinkAllPendingGrafts(*buildingObject, *portalProperty);
	finalizeBuildingPortalChanges(*buildingObject, *portalProperty);
}



// ----------------------------------------------------------------------



void DynamicBunkerClient::tryLinkAllPendingGrafts(Object &building, PortalProperty &portalProperty)
{
	UNREF(building);
	PortalProperty::DynamicRoomGraftList const &grafts = portalProperty.getDynamicRoomGrafts();
	for (size_t i = 0; i < grafts.size(); ++i)
		DynamicBunkerClientNamespace::linkGraftRecord(portalProperty, grafts[i]);
}



// ----------------------------------------------------------------------



void DynamicBunkerClient::onCellLoaded(Object &building, PortalProperty &portalProperty, int cellIndex)
{
	tryLoadPendingGraftCells(building, portalProperty);
	prepareGraftPortalLinking(portalProperty);
	tryLinkAllPendingGrafts(building, portalProperty);
	finalizeLinkedCustomSocketGrafts(portalProperty);
	finalizeBuildingPortalChanges(building, portalProperty);

	if (cellIndex >= 1)
	{
		CellProperty * const cell = portalProperty.getCell(cellIndex);
		if (cell)
			refreshCellRenderAppearance(cell);
	}
}



// ----------------------------------------------------------------------



void DynamicBunkerClient::finalizeBuildingPortalChanges(Object &building, PortalProperty &portalProperty)

{

	tryLoadPendingGraftCells(building, portalProperty);
	s_wallCutRenderHookThrottle = 0;

	int const cellCount = portalProperty.getNumberOfCells();

	for (int cellIndex = 1; cellIndex < cellCount; ++cellIndex)
	{
		CellProperty *const cell = portalProperty.getCell(cellIndex);
		if (!cell)
			continue;

		Object &cellOwner = cell->getOwner();
		if (!cellOwner.isInitialized() || cellOwner.getCellProperty() != cell)
			continue;

		CellObject *const cellObject = CellObject::asCellObject(&cellOwner);

		if (cellObject)

			cellObject->buildRadarGeometry();

	}



	refreshBridgesFromPortalProperty(building, portalProperty);
	refreshDoorwaysFromPortalProperty(building, portalProperty);
	portalProperty.refreshAllCustomSocketWallCuts();
	forceLoadAllCellAppearances(portalProperty);
	applyWallVisualCuts(portalProperty);
	forceLoadAllCellAppearances(portalProperty);
	applyWallVisualCuts(portalProperty);
	ensureWallCutRenderHooks(portalProperty);
	finalizeLinkedCustomSocketGrafts(portalProperty);
	portalProperty.refreshDynamicGraftPortalDpvs();

	for (int cellIndex = 1; cellIndex < cellCount; ++cellIndex)
	{
		CellProperty * const cell = portalProperty.getCell(cellIndex);
		if (cell)
			refreshCellRenderAppearance(cell);
	}

	IGNORE_RETURN(building.alter(0.0f));
}

// ----------------------------------------------------------------------

void DynamicBunkerClient::applyPreviewWallCut(
	Object & building,
	int cellIndex,
	Transform const & doorTransform_cell,
	float width,
	float height,
	PortalVolumeCut const * portalVolume)
{
	PortalProperty * const portalProperty = building.getPortalProperty();
	if (!portalProperty || cellIndex < 1)
		return;

	NetworkId wallHitObjectId;
	std::map<NetworkId, PreviewWallHit>::const_iterator previewIt = s_previewWallHits.find(building.getNetworkId());
	if (previewIt != s_previewWallHits.end() && previewIt->second.cellIndex == cellIndex)
		wallHitObjectId = previewIt->second.hitObjectId;

	applyPreviewWallCutInternal(
		*portalProperty, cellIndex, doorTransform_cell, width, height,
		portalVolume, wallHitObjectId, true);
}

// ----------------------------------------------------------------------

void DynamicBunkerClient::applyPlacementWallCut(
	Object & building,
	int cellIndex,
	Transform const & doorTransform_cell,
	float width,
	float height,
	NetworkId const & wallHitObjectId)
{
	PortalProperty * const portalProperty = building.getPortalProperty();
	if (!portalProperty || cellIndex < 1)
		return;

	CellProperty * const cell = DynamicBunkerClientNamespace::resolveCellForPlacement(*portalProperty, cellIndex);
	if (!cell)
		return;

	Transform snappedDoor = doorTransform_cell;
	snapDoorTransformYToCellFloor(cell, snappedDoor);

	char const * const doorStyle = Portal::pickDynamicBunkerDoorStyle(snappedDoor);
	PortalVolumeCut volume;
	if (!DynamicBunkerClient::buildPortalVolumeCutFromDoorStyle(doorStyle, width, height, volume))
		DynamicBunkerClient::buildPortalVolumeCutFromDimensions(width, height, volume);

	NetworkId const buildingId = building.getNetworkId();
	if (wallHitObjectId.isValid())
		setPreviewWallHitObject(buildingId, cellIndex, wallHitObjectId);

	RegisteredCellPortalCut placementCut;
	placementCut.doorTransform_cell = snappedDoor;
	placementCut.volume = volume;
	std::vector<RegisteredCellPortalCut> placementCuts;
	placementCuts.push_back(placementCut);
	MeshAppearance::setBuildingCellPortalCuts(buildingId, cellIndex, placementCuts);
	MeshAppearance::setActivePortalCutBuildingId(buildingId);

	applyWallCutForPortal(
		*portalProperty, cellIndex, snappedDoor, width, height, &volume, true);
	ensureWallCutRenderHooks(*portalProperty);

	std::string doorAppearancePath;
	if (!Portal::lookupDoorStyleAppearance(doorStyle, doorAppearancePath))
		doorAppearancePath = s_doorFrameAppearance;

	Vector const frameScale(
		std::max(0.5f, width / 1.6f),
		std::max(0.5f, height / 2.4f),
		1.0f);
	addDoorVisualToCell(building, cell, snappedDoor, width, height, doorAppearancePath.c_str(), frameScale, Vector::zero);

	Vector const alcoveOffset = snappedDoor.getLocalFrameK_p() * -0.35f;
	Vector const alcoveScale(
		std::max(0.5f, width / 1.8f),
		std::max(0.5f, height / 2.6f),
		0.65f);
	addDoorVisualToCell(building, cell, snappedDoor, width, height, s_doorAlcoveAppearance, alcoveScale, alcoveOffset);
}

// ----------------------------------------------------------------------

void DynamicBunkerClient::setPreviewWallHitObject(NetworkId const & buildingId, int cellIndex, NetworkId const & wallHitObjectId)
{
	if (!buildingId.isValid() || cellIndex < 1)
	{
		s_previewWallHits.erase(buildingId);
		return;
	}

	PreviewWallHit hit;
	hit.cellIndex = cellIndex;
	hit.hitObjectId = wallHitObjectId;
	s_previewWallHits[buildingId] = hit;
}

// ----------------------------------------------------------------------

void DynamicBunkerClient::setPendingSocketWallHit(NetworkId const & buildingId, int cellIndex, NetworkId const & wallHitObjectId)
{
	if (!buildingId.isValid() || cellIndex < 1 || !wallHitObjectId.isValid())
	{
		s_pendingSocketWallHit = PendingSocketWallHit();
		return;
	}

	s_pendingSocketWallHit.buildingId = buildingId;
	s_pendingSocketWallHit.cellIndex = cellIndex;
	s_pendingSocketWallHit.hitObjectId = wallHitObjectId;
}

// ----------------------------------------------------------------------

void DynamicBunkerClient::snapDoorTransformYToCellFloor(CellProperty const * cell, Transform & doorTransform_cell)
{
	DynamicBunkerClientNamespace::snapDoorTransformYToCellFloor(cell, doorTransform_cell);
}

// ----------------------------------------------------------------------

bool DynamicBunkerClient::raycastCellWallForPlacement(
	PortalProperty const & portalProperty,
	int cellIndex,
	Vector const & worldStart,
	Vector const & worldEnd,
	Vector & outWorldPoint,
	Vector & outWorldNormal,
	NetworkId & outHitObjectId)
{
	outHitObjectId = NetworkId::cms_invalid;
	if (cellIndex < 1)
		return false;

	CellProperty * const cell = resolveCellForPlacement(const_cast<PortalProperty &>(portalProperty), cellIndex);
	if (!cell)
		return false;

	forceLoadAllCellAppearances(const_cast<PortalProperty &>(portalProperty));
	forceLoadTargetCellAppearance(cell);
	syncCellAppearanceMeshTransforms(cell);

	MeshWallRaycastState state(worldStart, worldEnd);
	forEachCellWallMeshAppearance(cell, raycastMeshAppearanceWallTriangles, &state);

	Object & building = const_cast<PortalProperty &>(portalProperty).getOwner();
	if (!state.found)
	{
		IGNORE_RETURN(building.alter(0.0f));
		if (building.getAppearance())
		{
			Appearance * const buildingAppearance = building.getAppearance();
			IGNORE_RETURN(buildingAppearance->alter(0.0f));
			if (buildingAppearance->isLoaded())
				forEachLoadedMeshAppearance(buildingAppearance, raycastMeshAppearanceWallTriangles, &state);
		}
	}

	if (!state.found)
		return false;

	Object * const appearanceObject = cell->getAppearanceObject();
	outHitObjectId = appearanceObject
		? appearanceObject->getNetworkId()
		: cell->getOwner().getNetworkId();
	outWorldPoint = state.bestPoint;
	outWorldNormal = state.bestNormal;
	return true;
}

// ----------------------------------------------------------------------

void DynamicBunkerClient::clearPreviewWallCut(Object & building)
{
	NetworkId const buildingId = building.getNetworkId();
	int previewCellIndex = -1;
	if (s_activePreviewWallCut.active && s_activePreviewWallCut.buildingId == buildingId)
		previewCellIndex = s_activePreviewWallCut.cellIndex;

	s_previewWallHits.erase(buildingId);
	s_activePreviewWallCut = ActivePreviewWallCut();

	PortalProperty * const portalProperty = building.getPortalProperty();
	if (!portalProperty)
		return;

	if (previewCellIndex >= 1)
	{
		std::vector<RegisteredCellPortalCut> emptyCuts;
		MeshAppearance::setBuildingCellPortalCuts(buildingId, previewCellIndex, emptyCuts);
	}

	bool hasMaterializedSocket = false;
	PortalProperty::CustomSocketList const & sockets = portalProperty->getCustomSockets();
	for (size_t i = 0; i < sockets.size(); ++i)
	{
		if (sockets[i].materializedPortalIndex >= 0)
		{
			hasMaterializedSocket = true;
			break;
		}
	}

	if (hasMaterializedSocket)
	{
		applyWallVisualCuts(*portalProperty);
		return;
	}

	CellProperty * const previewCell = previewCellIndex >= 1 ? portalProperty->getCell(previewCellIndex) : 0;
	if (previewCell)
		clearCellWallVisualCuts(previewCell);

	MeshAppearance::clearActivePortalCutBuildingId();
}

// ----------------------------------------------------------------------

bool DynamicBunkerClient::buildPortalVolumeCutFromDimensions(float width, float height, PortalVolumeCut & outVolume)
{
	float const halfW = std::max(0.25f, width * 0.5f);
	outVolume.minI = -halfW;
	outVolume.maxI = halfW;
	outVolume.minJ = 0.0f;
	outVolume.maxJ = std::max(0.5f, height);
	outVolume.minI -= 0.05f;
	outVolume.maxI += 0.05f;
	outVolume.minJ -= 0.05f;
	outVolume.maxJ += 0.05f;
	outVolume.inwardDepth = 6.0f;
	outVolume.outwardDepth = 6.0f;
	return true;
}

// ----------------------------------------------------------------------

bool DynamicBunkerClient::buildPortalVolumeCutFromDoorStyle(
	char const * doorStyle,
	float fallbackWidth,
	float fallbackHeight,
	PortalVolumeCut & outVolume)
{
	float width = fallbackWidth;
	float height = fallbackHeight;
	if (Portal::lookupDoorStyleDimensions(doorStyle, width, height))
		return buildPortalVolumeCutFromDimensions(width, height, outVolume);
	return buildPortalVolumeCutFromDimensions(fallbackWidth, fallbackHeight, outVolume);
}

// ----------------------------------------------------------------------

bool DynamicBunkerClient::buildDonorPortalVolumeCut(
	char const * donorPob,
	int donorCellIndex,
	int donorPortalIndex,
	PortalVolumeCut & outVolume)
{
	float width = 1.0f;
	float height = 2.0f;

	if (!donorPob || !donorPob[0])
		return buildPortalVolumeCutFromDimensions(width, height, outVolume);

	PortalPropertyTemplate const * const tmpl = PortalPropertyTemplateList::fetch(CrcLowerString(donorPob));
	if (!tmpl)
		return buildPortalVolumeCutFromDimensions(width, height, outVolume);

	if (donorCellIndex < 1)
		donorCellIndex = 1;
	if (donorPortalIndex < 0)
		donorPortalIndex = 0;

	bool ok = false;
	PortalPropertyTemplate::PortalOwnersList const & owners = tmpl->getPortalOwnersList();
	for (size_t portalGeometryIndex = 0; portalGeometryIndex < owners.size(); ++portalGeometryIndex)
	{
		PortalPropertyTemplate::PortalOwners const & owner = owners[portalGeometryIndex];
		if (owner.owners[0].cell != donorCellIndex || owner.owners[0].portal != donorPortalIndex)
			continue;

		IndexedTriangleList const * const geometry = tmpl->getPortalGeometry(
			static_cast<int>(portalGeometryIndex), donorCellIndex, donorPortalIndex, false);
		if (!geometry || geometry->getVertices().empty())
			break;

		if (donorCellIndex >= tmpl->getNumberOfCells())
			break;

		PortalPropertyTemplateCell const & donorCell = tmpl->getCell(donorCellIndex);
		PortalPropertyTemplateCell::PortalPropertyTemplateCellPortalList const * const portalList = donorCell.getPortalList();
		if (!portalList || donorPortalIndex >= static_cast<int>(portalList->size()))
			break;

		Transform const doorTransform_cell = (*portalList)[static_cast<size_t>(donorPortalIndex)]->getDoorTransform(false);
		ok = buildPortalVolumeCutFromGeometry(*geometry, doorTransform_cell, outVolume, width, height);
		break;
	}

	tmpl->release();
	if (!ok)
		ok = buildPortalVolumeCutFromDimensions(width, height, outVolume);
	return ok;
}

// ----------------------------------------------------------------------

void DynamicBunkerClient::ensureRightHandedCellTransform(Transform & cellTransform_o2p)
{
	Vector const I = cellTransform_o2p.getLocalFrameI_p();
	Vector const J = cellTransform_o2p.getLocalFrameJ_p();
	Vector const K = cellTransform_o2p.getLocalFrameK_p();
	if (I.cross(J).dot(K) >= 0.0f)
		return;

	Vector const pos = cellTransform_o2p.getPosition_p();
	cellTransform_o2p.setLocalFrameIJK_p(-I, J, K);
	cellTransform_o2p.setPosition_p(pos);
}

// ======================================================================

