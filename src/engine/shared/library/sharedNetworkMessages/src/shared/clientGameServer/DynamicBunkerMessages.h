// ======================================================================
//
// DynamicBunkerMessages.h
// copyright 2026 Titan
//
// ======================================================================

#ifndef INCLUDED_DynamicBunkerMessages_H
#define INCLUDED_DynamicBunkerMessages_H

#include "sharedMath/Transform.h"
#include "sharedMathArchive/TransformArchive.h"
#include "sharedNetworkMessages/GameNetworkMessage.h"
#include "../../../../../../engine/shared/library/sharedFoundation/include/public/sharedFoundation/NetworkId.h"

#include <string>
#include <vector>

// ======================================================================

class DynamicBunkerGraftMessage : public GameNetworkMessage
{
public:

	static char const * const MessageType;

	DynamicBunkerGraftMessage(
		NetworkId const &buildingId,
		NetworkId const &cellId,
		int32 graftedCellIndex,
		int32 hostCellIndex,
		int32 hostPortalIndex,
		int32 graftedPortalIndex,
		int32 donorCellIndex,
		std::string const &donorPobName,
		Transform const &cellTransform);

	explicit DynamicBunkerGraftMessage(Archive::ReadIterator &source);
	~DynamicBunkerGraftMessage();

	NetworkId const &getBuildingId() const { return m_buildingId.get(); }
	NetworkId const &getCellId() const { return m_cellId.get(); }
	int32 getGraftedCellIndex() const { return m_graftedCellIndex.get(); }
	int32 getHostCellIndex() const { return m_hostCellIndex.get(); }
	int32 getHostPortalIndex() const { return m_hostPortalIndex.get(); }
	int32 getGraftedPortalIndex() const { return m_graftedPortalIndex.get(); }
	int32 getDonorCellIndex() const { return m_donorCellIndex.get(); }
	std::string const &getDonorPobName() const { return m_donorPobName.get(); }
	Transform const &getCellTransform() const { return m_cellTransform.get(); }

private:

	Archive::AutoVariable<NetworkId> m_buildingId;
	Archive::AutoVariable<NetworkId> m_cellId;
	Archive::AutoVariable<int32> m_graftedCellIndex;
	Archive::AutoVariable<int32> m_hostCellIndex;
	Archive::AutoVariable<int32> m_hostPortalIndex;
	Archive::AutoVariable<int32> m_graftedPortalIndex;
	Archive::AutoVariable<int32> m_donorCellIndex;
	Archive::AutoVariable<std::string> m_donorPobName;
	Archive::AutoVariable<Transform> m_cellTransform;
};

// ======================================================================
// Server -> client: open floorplan room picker (catalog + active socket)
// ======================================================================

class DynamicBunkerOpenFloorplanMessage : public GameNetworkMessage
{
public:

	static char const * const MessageType;

	struct RoomEntry
	{
		std::string roomId;
		std::string displayName;
		std::string donorPob;
		std::string appearanceHint;
		std::string socketType;
		int32 donorCellIndex;
		int32 donorPortalIndex;
	};

	struct SocketEntry
	{
		int32 cellIndex;
		int32 portalIndex;
		std::string label;
		bool open;
	};

	typedef std::vector<RoomEntry> RoomList;
	typedef std::vector<SocketEntry> SocketList;

	DynamicBunkerOpenFloorplanMessage(
		NetworkId const &buildingId,
		NetworkId const &terminalId,
		int32 selectedCellIndex,
		int32 selectedPortalIndex,
		RoomList const &rooms,
		SocketList const &sockets);

	explicit DynamicBunkerOpenFloorplanMessage(Archive::ReadIterator &source);
	~DynamicBunkerOpenFloorplanMessage();

	NetworkId const &getBuildingId() const { return m_buildingId.get(); }
	NetworkId const &getTerminalId() const { return m_terminalId.get(); }
	int32 getSelectedCellIndex() const { return m_selectedCellIndex.get(); }
	int32 getSelectedPortalIndex() const { return m_selectedPortalIndex.get(); }
	RoomList const &getRooms() const { return m_rooms.get(); }
	SocketList const &getSockets() const { return m_sockets.get(); }

private:

	Archive::AutoVariable<NetworkId> m_buildingId;
	Archive::AutoVariable<NetworkId> m_terminalId;
	Archive::AutoVariable<int32> m_selectedCellIndex;
	Archive::AutoVariable<int32> m_selectedPortalIndex;
	Archive::AutoVariable<RoomList> m_rooms;
	Archive::AutoVariable<SocketList> m_sockets;
};

// ======================================================================
// Client -> server: assign selected catalog room to a snap socket
// ======================================================================

class DynamicBunkerAssignRoomMessage : public GameNetworkMessage
{
public:

	static char const * const MessageType;

	DynamicBunkerAssignRoomMessage(
		NetworkId const &buildingId,
		NetworkId const &terminalId,
		int32 hostCellIndex,
		int32 hostPortalIndex,
		std::string const &roomId);

	explicit DynamicBunkerAssignRoomMessage(Archive::ReadIterator &source);
	~DynamicBunkerAssignRoomMessage();

	NetworkId const &getBuildingId() const { return m_buildingId.get(); }
	NetworkId const &getTerminalId() const { return m_terminalId.get(); }
	int32 getHostCellIndex() const { return m_hostCellIndex.get(); }
	int32 getHostPortalIndex() const { return m_hostPortalIndex.get(); }
	std::string const &getRoomId() const { return m_roomId.get(); }

private:

	Archive::AutoVariable<NetworkId> m_buildingId;
	Archive::AutoVariable<NetworkId> m_terminalId;
	Archive::AutoVariable<int32> m_hostCellIndex;
	Archive::AutoVariable<int32> m_hostPortalIndex;
	Archive::AutoVariable<std::string> m_roomId;
};

// ======================================================================

namespace Archive
{
	void get(ReadIterator &source, DynamicBunkerOpenFloorplanMessage::RoomEntry &target);
	void put(ByteStream &target, DynamicBunkerOpenFloorplanMessage::RoomEntry const &source);
	void get(ReadIterator &source, DynamicBunkerOpenFloorplanMessage::SocketEntry &target);
	void put(ByteStream &target, DynamicBunkerOpenFloorplanMessage::SocketEntry const &source);
}

// ======================================================================

#endif
