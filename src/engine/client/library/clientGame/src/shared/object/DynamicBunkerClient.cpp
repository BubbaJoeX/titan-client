// ======================================================================
//
// DynamicBunkerClient.cpp
// copyright 2026 Titan
//
// ======================================================================

#include "clientGame/FirstClientGame.h"
#include "clientGame/DynamicBunkerClient.h"

#include "sharedFoundation/ExitChain.h"
#include "sharedMessageDispatch/Message.h"
#include "sharedMessageDispatch/Receiver.h"
#include "sharedNetworkMessages/DynamicBunkerMessages.h"
#include "sharedNetworkMessages/GameNetworkMessage.h"
#include "sharedObject/NetworkIdManager.h"
#include "sharedObject/Object.h"
#include "sharedObject/PortalProperty.h"

// ======================================================================

namespace DynamicBunkerClientNamespace
{
	class Listener : public MessageDispatch::Receiver
	{
	public:
		Listener();
		virtual void receiveMessage(MessageDispatch::Emitter const & source, MessageDispatch::MessageBase const & message);
	};

	Listener *s_listener = 0;

	Listener::Listener()
	: MessageDispatch::Receiver()
	{
		connectToMessage(DynamicBunkerGraftMessage::MessageType);
	}

	void Listener::receiveMessage(MessageDispatch::Emitter const &, MessageDispatch::MessageBase const & message)
	{
		GameNetworkMessage const *const gnm = dynamic_cast<GameNetworkMessage const *>(&message);
		if (!gnm || !message.isType(DynamicBunkerGraftMessage::MessageType))
			return;

		Archive::ReadIterator ri = gnm->getByteStream().begin();
		DynamicBunkerGraftMessage const msg(ri);
		DynamicBunkerClient::handleGraftMessage(msg);
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
		IGNORE_RETURN(portalProperty->linkCellPortals(
			message.getHostCellIndex(),
			message.getHostPortalIndex(),
			message.getGraftedCellIndex(),
			message.getGraftedPortalIndex()));
	}
}

// ======================================================================
