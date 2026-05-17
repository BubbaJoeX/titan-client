// ======================================================================
//
// ClientClaimFootprintManager.cpp
//
// ======================================================================

#include "clientGame/FirstClientGame.h"
#include "clientGame/ClientClaimFootprintManager.h"

#include "clientGame/ClientBattlefieldMarkerOutlineObject.h"
#include "clientGame/ClientClaimManipulateState.h"
#include "clientGame/ClientWorld.h"
#include "clientGame/Game.h"
#include "clientGraphics/RenderWorld.h"
#include "sharedDebug/InstallTimer.h"
#include "sharedFoundation/ExitChain.h"
#include "sharedMath/VectorArgb.h"
#include "sharedMessageDispatch/Message.h"
#include "sharedMessageDispatch/Receiver.h"
#include "sharedNetworkMessages/ClaimClientMessageDispatch.h"
#include "sharedNetworkMessages/GameNetworkMessage.h"
#include "sharedFoundation/NetworkId.h"
#include "sharedObject/NetworkIdManager.h"
#include "sharedObject/Object.h"

#include <map>
#include <string>
#include <vector>

// ======================================================================

namespace ClientClaimFootprintManagerNamespace
{
	std::string s_lastSceneId;

	class Listener : public MessageDispatch::Receiver
	{
	public:
		Listener()
		{
			connectToMessage(Game::Messages::SCENE_CHANGED);
			connectToMessage(ClaimClientMessageDispatch::cms_claimFootprintSync);
			connectToMessage(ClaimClientMessageDispatch::cms_claimManipulateState);
		}

		~Listener()
		{
			disconnectFromMessage(Game::Messages::SCENE_CHANGED);
			disconnectFromMessage(ClaimClientMessageDispatch::cms_claimFootprintSync);
			disconnectFromMessage(ClaimClientMessageDispatch::cms_claimManipulateState);
		}

		void receiveMessage(MessageDispatch::Emitter const &, MessageDispatch::MessageBase const & message)
		{
			if (message.isType(Game::Messages::SCENE_CHANGED))
			{
				std::string const & sceneId = Game::getSceneId();
				if (sceneId.empty())
				{
					ClientClaimFootprintManager::clearAll();
					s_lastSceneId.clear();
					return;
				}

				if (!s_lastSceneId.empty() && s_lastSceneId != sceneId)
					ClientClaimFootprintManager::clearAll();

				s_lastSceneId = sceneId;
				return;
			}

			if (message.isType(ClaimClientMessageDispatch::cms_claimManipulateState))
			{
				GameNetworkMessage const * const gnm = safe_cast<GameNetworkMessage const *>(&message);
				Archive::ReadIterator ri = gnm->getByteStream().begin();
				bool const canManipulate = ClaimClientMessageDispatch::decodeManipulateState(ri);
				ClientClaimFootprintManager::handleManipulateState(canManipulate);
				return;
			}

			if (message.isType(ClaimClientMessageDispatch::cms_claimFootprintSync))
			{
				GameNetworkMessage const * const gnm = safe_cast<GameNetworkMessage const *>(&message);
				Archive::ReadIterator ri = gnm->getByteStream().begin();
				NetworkId markerId;
				float radiusMeters = 0.f;
				bool active = false;
				ClaimClientMessageDispatch::decodeFootprintSync(ri, markerId, radiusMeters, active);
				ClientClaimFootprintManager::handleSync(markerId, radiusMeters, active);
			}
		}
	};

	struct Entry
	{
		float                               m_radiusMeters;
		ClientBattlefieldMarkerOutlineObject *m_outline;
		bool                                m_inWorld;

		Entry() :
			m_radiusMeters(0.f),
			m_outline(0),
			m_inWorld(false)
		{
		}
	};

	typedef std::map<NetworkId, Entry> EntryMap;

	Listener *  s_listener = 0;
	EntryMap    s_entries;
	float       s_updateTimer = 0.f;

	void destroyEntry(Entry & entry)
	{
		if (!entry.m_outline)
		{
			entry.m_inWorld = false;
			return;
		}

		ClientBattlefieldMarkerOutlineObject * const outline = entry.m_outline;
		entry.m_outline = 0;
		entry.m_inWorld = false;

		if (outline->isInWorld())
			outline->removeFromWorld();

		delete outline;
	}

	/** World-space outline (same approach as claim placement preview), centered on the marker. */
	void syncEntryToMarker(NetworkId const & markerId, Entry & entry)
	{
		if (!entry.m_outline)
			return;

		Object * const marker = NetworkIdManager::getObjectById(markerId);
		if (!marker || !marker->isInWorld())
			return;

		Vector const center_w = marker->getPosition_w();

		if (!entry.m_inWorld)
		{
			entry.m_outline->addToWorld();
			entry.m_inWorld = true;
		}

		entry.m_outline->setPosition_w(center_w);
		entry.m_outline->resetBoundary();
	}
}

using namespace ClientClaimFootprintManagerNamespace;

// ======================================================================

void ClientClaimFootprintManager::install()
{
	InstallTimer const installTimer("ClientClaimFootprintManager::install");
	if (!s_listener)
	{
		s_listener = new Listener();
		ExitChain::add(&remove, "ClientClaimFootprintManager::remove");
	}
}

// ----------------------------------------------------------------------

void ClientClaimFootprintManager::remove()
{
	clearAll();
	delete s_listener;
	s_listener = 0;
}

// ----------------------------------------------------------------------

void ClientClaimFootprintManager::clearAll()
{
	for (EntryMap::iterator i = s_entries.begin(); i != s_entries.end(); ++i)
		destroyEntry(i->second);
	s_entries.clear();
	s_lastSceneId.clear();
	ClientClaimManipulateState::notify(false);
}

// ----------------------------------------------------------------------

void ClientClaimFootprintManager::handleManipulateState(bool const canManipulate)
{
	ClientClaimManipulateState::notify(canManipulate);
}

// ----------------------------------------------------------------------

void ClientClaimFootprintManager::handleSync(NetworkId const & markerId, float const radiusMeters, bool const active)
{
	if (!markerId.isValid())
		return;

	EntryMap::iterator existing = s_entries.find(markerId);
	if (!active)
	{
		if (existing != s_entries.end())
		{
			destroyEntry(existing->second);
			s_entries.erase(existing);
		}
		return;
	}

	if (radiusMeters <= 0.f)
		return;

	if (existing != s_entries.end())
	{
		if (existing->second.m_radiusMeters == radiusMeters && existing->second.m_outline)
		{
			syncEntryToMarker(markerId, existing->second);
			return;
		}
		destroyEntry(existing->second);
		s_entries.erase(existing);
	}

	Entry entry;
	entry.m_radiusMeters = radiusMeters;
	int const poleCount = ClientBattlefieldMarkerOutlineObject::calculatePoleCountForRadius(radiusMeters);
	entry.m_outline = new ClientBattlefieldMarkerOutlineObject(poleCount, radiusMeters);
	VectorArgb const ribbonColor(0.5f, VectorArgb::solidGreen.r, VectorArgb::solidGreen.g, VectorArgb::solidGreen.b);
	entry.m_outline->setRibbonColor(ribbonColor);
	entry.m_outline->addNotification(ClientWorld::getIntangibleNotification());
	RenderWorld::addObjectNotifications(*entry.m_outline);

	s_entries[markerId] = entry;
	syncEntryToMarker(markerId, s_entries[markerId]);
}

// ----------------------------------------------------------------------

void ClientClaimFootprintManager::update(float const elapsedTime)
{
	if (s_entries.empty())
		return;

	s_updateTimer += elapsedTime;
	if (s_updateTimer < 0.25f)
		return;
	s_updateTimer = 0.f;

	for (EntryMap::iterator i = s_entries.begin(); i != s_entries.end(); ++i)
		syncEntryToMarker(i->first, i->second);
}

// ======================================================================
