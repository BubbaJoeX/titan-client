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



	void addBridgeVisual(Object & building, Transform const & bridgeTransform, float /*length*/)
	{
		Appearance * const app = AppearanceTemplateList::createAppearance(s_bridgeAppearance);
		if (!app)
			return;

		Object * const bridgeObject = new Object;
		bridgeObject->setAppearance(app);
		bridgeObject->setTransform_o2w(bridgeTransform);
		IGNORE_RETURN(bridgeObject->alter(0.0f));
		bridgeObject->conclude();

		s_buildingBridges[building.getNetworkId()].push_back(bridgeObject);
	}



	void refreshBridgesFromPortalProperty(Object & building, PortalProperty & portalProperty)
	{

		clearBuildingBridges(building.getNetworkId());



		PortalProperty::BridgeSegmentList const & bridges = portalProperty.getBridgeSegments();

		for (size_t i = 0; i < bridges.size(); ++i)

			addBridgeVisual(building, bridges[i].transform_o2p, bridges[i].length);



		if (!bridges.empty())

			return;



		PortalProperty::DynamicRoomGraftList const & grafts = portalProperty.getDynamicRoomGrafts();

		for (size_t i = 0; i < grafts.size(); ++i)

		{

			PortalProperty::DynamicRoomGraft const & graft = grafts[i];

			Transform hostTransform;

			Transform graftTransform;

			if (!getPortalBuildingTransform(portalProperty, graft.hostCellIndex, graft.hostPortalIndex, hostTransform))

				continue;

			if (!getPortalBuildingTransform(portalProperty, graft.graftedCellIndex, graft.graftedPortalIndex, graftTransform))

				continue;



			Vector const delta = graftTransform.getPosition_p() - hostTransform.getPosition_p();

			float const gap = delta.magnitude();

			if (gap < 0.75f)

				continue;



			Transform bridgeTransform;

			bridgeTransform.setPosition_p((hostTransform.getPosition_p() + graftTransform.getPosition_p()) * 0.5f);

			if (gap > 0.01f)

				bridgeTransform.yaw_l(atan2f(delta.x, delta.z));

			addBridgeVisual(building, bridgeTransform, gap);

		}

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

	if (cellObject && !portalProperty->getCell(message.getGraftedCellIndex()))

	{

		portalProperty->cellLoaded(message.getGraftedCellIndex(), *cellObject, true);

		cellObject->setTransform_o2p(message.getCellTransform());

	}



	if (portalProperty->getCell(message.getHostCellIndex()) && portalProperty->getCell(message.getGraftedCellIndex()))

	{

		if (!portalProperty->linkCellPortals(

			message.getHostCellIndex(),

			message.getHostPortalIndex(),

			message.getGraftedCellIndex(),

			message.getGraftedPortalIndex()))

		{

			if (PortalProperty::isCustomSocketIndex(message.getHostPortalIndex()))

			{

				IGNORE_RETURN(portalProperty->linkCustomSocketGraft(

					message.getHostCellIndex(),

					message.getHostPortalIndex(),

					message.getGraftedCellIndex(),

					message.getGraftedPortalIndex()));

				IGNORE_RETURN(portalProperty->markCustomSocketOpen(message.getHostCellIndex(), message.getHostPortalIndex(), false));

			}

		}

	}



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



	IGNORE_RETURN(portalProperty->unlinkCellPortal(message.getHostCellIndex(), message.getHostPortalIndex()));

	portalProperty->unlinkAllCellPortals(message.getGraftedCellIndex());

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



		CellObject *const cellObject = CellObject::asCellObject(&cell->getOwner());

		if (cellObject)

			cellObject->buildRadarGeometry();

	}



	refreshBridgesFromPortalProperty(building, portalProperty);

}



// ======================================================================

