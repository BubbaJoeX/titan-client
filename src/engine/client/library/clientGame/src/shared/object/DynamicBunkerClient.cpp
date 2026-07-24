// ======================================================================

//

// DynamicBunkerClient.cpp

// copyright 2026 Titan

//

// ======================================================================



#include "clientGame/FirstClientGame.h"

#include "clientGame/DynamicBunkerClient.h"

#include "clientGame/CellObject.h"



#include "sharedFoundation/ExitChain.h"

#include "sharedMessageDispatch/Message.h"

#include "sharedMessageDispatch/Receiver.h"

#include "sharedNetworkMessages/DynamicBunkerMessages.h"

#include "sharedNetworkMessages/GameNetworkMessage.h"

#include "sharedObject/Appearance.h"

#include "sharedObject/AppearanceTemplateList.h"

#include "sharedObject/CellProperty.h"

#include "sharedObject/NetworkIdManager.h"

#include "sharedObject/Object.h"

#include "sharedObject/Portal.h"

#include "sharedObject/PortalProperty.h"



#include <cmath>

#include <map>

#include <vector>



// ======================================================================



namespace DynamicBunkerClientNamespace

{

	char const * const s_bridgeAppearance = "appearance/godclient_space_waypoint_corridor.apt";



	typedef std::vector<Object *> BridgeObjectList;

	typedef std::map<NetworkId, BridgeObjectList> BuildingBridgeMap;



	BuildingBridgeMap s_buildingBridges;



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
		PortalProperty::DynamicRoomGraftList const & grafts = portalProperty.getDynamicRoomGrafts();
		for (size_t i = 0; i < grafts.size(); ++i)
			recordBridgeForGraft(building, portalProperty, grafts[i]);

		PortalProperty::BridgeSegmentList const & bridges = portalProperty.getBridgeSegments();
		for (size_t i = 0; i < bridges.size(); ++i)
			addBridgeVisual(building, bridges[i]);
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
			return true;
		}

		return portalProperty.linkCellPortals(graft.hostCellIndex, graft.hostPortalIndex, graft.graftedCellIndex, resolvedGraftPortal);
	}

	void materializeAllCustomSockets(PortalProperty &portalProperty)
	{
		PortalProperty::CustomSocketList const &sockets = portalProperty.getCustomSockets();
		for (size_t i = 0; i < sockets.size(); ++i)
		{
			PortalProperty::CustomSocket const &socket = sockets[i];
			CellProperty * const cell = portalProperty.getCell(socket.cellIndex);
			if (!cell)
				continue;

			if (socket.materializedPortalIndex >= 0)
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

		connectToMessage(DynamicBunkerUngraftMessage::MessageType);

		connectToMessage(DynamicBunkerCustomSocketSyncMessage::MessageType);

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
		cellObject->setTransform_o2p(message.getCellTransform());
	}

	prepareGraftPortalLinking(*portalProperty);
	DynamicBunkerClient::tryLinkAllPendingGrafts(*buildingObject, *portalProperty);
	refreshBridgesFromPortalProperty(*buildingObject, *portalProperty);
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

	IGNORE_RETURN(portalProperty->addCustomSocket(socket));
	IGNORE_RETURN(portalProperty->materializeCustomSocketPortal(socket.cellIndex, socket.socketIndex));
	tryLinkAllPendingGrafts(*buildingObject, *portalProperty);
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



	portalProperty->clearCustomSockets();



	DynamicBunkerOpenFloorplanMessage::CustomSocketList const &entries = message.getCustomSockets();

	for (size_t i = 0; i < entries.size(); ++i)

	{

		DynamicBunkerOpenFloorplanMessage::CustomSocketEntry const &entry = entries[i];

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
	UNREF(cellIndex);
	prepareGraftPortalLinking(portalProperty);
	tryLinkAllPendingGrafts(building, portalProperty);
	finalizeBuildingPortalChanges(building, portalProperty);
}



// ----------------------------------------------------------------------



void DynamicBunkerClient::finalizeBuildingPortalChanges(Object &building, PortalProperty &portalProperty)

{

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

	IGNORE_RETURN(building.alter(0.0f));
}



// ======================================================================

