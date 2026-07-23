// ======================================================================
//
// DynamicBunkerMessages.cpp
// copyright 2026 Titan
//
// ======================================================================

#include "sharedNetworkMessages/FirstSharedNetworkMessages.h"
#include "sharedNetworkMessages/DynamicBunkerMessages.h"

// ======================================================================

char const * const DynamicBunkerGraftMessage::MessageType = "DynamicBunkerGraftMessage";
char const * const DynamicBunkerOpenFloorplanMessage::MessageType = "DynamicBunkerOpenFloorplanMessage";
char const * const DynamicBunkerAssignRoomMessage::MessageType = "DynamicBunkerAssignRoomMessage";

// ----------------------------------------------------------------------

namespace Archive
{
	void get(ReadIterator &source, DynamicBunkerOpenFloorplanMessage::RoomEntry &target)
	{
		get(source, target.roomId);
		get(source, target.displayName);
		get(source, target.donorPob);
		get(source, target.appearanceHint);
		get(source, target.socketType);
		get(source, target.donorCellIndex);
		get(source, target.donorPortalIndex);
	}

	void put(ByteStream &target, DynamicBunkerOpenFloorplanMessage::RoomEntry const &source)
	{
		put(target, source.roomId);
		put(target, source.displayName);
		put(target, source.donorPob);
		put(target, source.appearanceHint);
		put(target, source.socketType);
		put(target, source.donorCellIndex);
		put(target, source.donorPortalIndex);
	}

	void get(ReadIterator &source, DynamicBunkerOpenFloorplanMessage::SocketEntry &target)
	{
		get(source, target.cellIndex);
		get(source, target.portalIndex);
		get(source, target.label);
		get(source, target.open);
	}

	void put(ByteStream &target, DynamicBunkerOpenFloorplanMessage::SocketEntry const &source)
	{
		put(target, source.cellIndex);
		put(target, source.portalIndex);
		put(target, source.label);
		put(target, source.open);
	}
}

// ======================================================================

DynamicBunkerGraftMessage::DynamicBunkerGraftMessage(
	NetworkId const &buildingId,
	NetworkId const &cellId,
	int32 graftedCellIndex,
	int32 hostCellIndex,
	int32 hostPortalIndex,
	int32 graftedPortalIndex,
	int32 donorCellIndex,
	std::string const &donorPobName,
	Transform const &cellTransform)
: GameNetworkMessage(MessageType),
	m_buildingId(buildingId),
	m_cellId(cellId),
	m_graftedCellIndex(graftedCellIndex),
	m_hostCellIndex(hostCellIndex),
	m_hostPortalIndex(hostPortalIndex),
	m_graftedPortalIndex(graftedPortalIndex),
	m_donorCellIndex(donorCellIndex),
	m_donorPobName(donorPobName),
	m_cellTransform(cellTransform)
{
	addVariable(m_buildingId);
	addVariable(m_cellId);
	addVariable(m_graftedCellIndex);
	addVariable(m_hostCellIndex);
	addVariable(m_hostPortalIndex);
	addVariable(m_graftedPortalIndex);
	addVariable(m_donorCellIndex);
	addVariable(m_donorPobName);
	addVariable(m_cellTransform);
}

DynamicBunkerGraftMessage::DynamicBunkerGraftMessage(Archive::ReadIterator &source)
: GameNetworkMessage(MessageType),
	m_buildingId(),
	m_cellId(),
	m_graftedCellIndex(0),
	m_hostCellIndex(0),
	m_hostPortalIndex(0),
	m_graftedPortalIndex(0),
	m_donorCellIndex(0),
	m_donorPobName(),
	m_cellTransform()
{
	addVariable(m_buildingId);
	addVariable(m_cellId);
	addVariable(m_graftedCellIndex);
	addVariable(m_hostCellIndex);
	addVariable(m_hostPortalIndex);
	addVariable(m_graftedPortalIndex);
	addVariable(m_donorCellIndex);
	addVariable(m_donorPobName);
	addVariable(m_cellTransform);
	unpack(source);
}

DynamicBunkerGraftMessage::~DynamicBunkerGraftMessage()
{
}

// ======================================================================

DynamicBunkerOpenFloorplanMessage::DynamicBunkerOpenFloorplanMessage(
	NetworkId const &buildingId,
	NetworkId const &terminalId,
	int32 selectedCellIndex,
	int32 selectedPortalIndex,
	RoomList const &rooms,
	SocketList const &sockets)
: GameNetworkMessage(MessageType),
	m_buildingId(buildingId),
	m_terminalId(terminalId),
	m_selectedCellIndex(selectedCellIndex),
	m_selectedPortalIndex(selectedPortalIndex),
	m_rooms(rooms),
	m_sockets(sockets)
{
	addVariable(m_buildingId);
	addVariable(m_terminalId);
	addVariable(m_selectedCellIndex);
	addVariable(m_selectedPortalIndex);
	addVariable(m_rooms);
	addVariable(m_sockets);
}

DynamicBunkerOpenFloorplanMessage::DynamicBunkerOpenFloorplanMessage(Archive::ReadIterator &source)
: GameNetworkMessage(MessageType),
	m_buildingId(),
	m_terminalId(),
	m_selectedCellIndex(0),
	m_selectedPortalIndex(0),
	m_rooms(),
	m_sockets()
{
	addVariable(m_buildingId);
	addVariable(m_terminalId);
	addVariable(m_selectedCellIndex);
	addVariable(m_selectedPortalIndex);
	addVariable(m_rooms);
	addVariable(m_sockets);
	unpack(source);
}

DynamicBunkerOpenFloorplanMessage::~DynamicBunkerOpenFloorplanMessage()
{
}

// ======================================================================

DynamicBunkerAssignRoomMessage::DynamicBunkerAssignRoomMessage(
	NetworkId const &buildingId,
	NetworkId const &terminalId,
	int32 hostCellIndex,
	int32 hostPortalIndex,
	std::string const &roomId)
: GameNetworkMessage(MessageType),
	m_buildingId(buildingId),
	m_terminalId(terminalId),
	m_hostCellIndex(hostCellIndex),
	m_hostPortalIndex(hostPortalIndex),
	m_roomId(roomId)
{
	addVariable(m_buildingId);
	addVariable(m_terminalId);
	addVariable(m_hostCellIndex);
	addVariable(m_hostPortalIndex);
	addVariable(m_roomId);
}

DynamicBunkerAssignRoomMessage::DynamicBunkerAssignRoomMessage(Archive::ReadIterator &source)
: GameNetworkMessage(MessageType),
	m_buildingId(),
	m_terminalId(),
	m_hostCellIndex(0),
	m_hostPortalIndex(0),
	m_roomId()
{
	addVariable(m_buildingId);
	addVariable(m_terminalId);
	addVariable(m_hostCellIndex);
	addVariable(m_hostPortalIndex);
	addVariable(m_roomId);
	unpack(source);
}

DynamicBunkerAssignRoomMessage::~DynamicBunkerAssignRoomMessage()
{
}

// ======================================================================
