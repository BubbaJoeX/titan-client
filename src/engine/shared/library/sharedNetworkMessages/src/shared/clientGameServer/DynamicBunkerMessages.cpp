// ======================================================================

//

// DynamicBunkerMessages.cpp

// copyright 2026 Titan

//

// ======================================================================



#include "sharedNetworkMessages/FirstSharedNetworkMessages.h"

#include "sharedNetworkMessages/DynamicBunkerMessages.h"



#include "sharedFoundation/NetworkIdArchive.h"

#include "sharedMathArchive/TransformArchive.h"



// ======================================================================



char const * const DynamicBunkerGraftMessage::MessageType = "DynamicBunkerGraftMessage";

char const * const DynamicBunkerOpenFloorplanMessage::MessageType = "DynamicBunkerOpenFloorplanMessage";

char const * const DynamicBunkerAssignRoomMessage::MessageType = "DynamicBunkerAssignRoomMessage";

char const * const DynamicBunkerUnassignRoomMessage::MessageType = "DynamicBunkerUnassignRoomMessage";

char const * const DynamicBunkerUngraftMessage::MessageType = "DynamicBunkerUngraftMessage";

char const * const DynamicBunkerCreateCustomSocketMessage::MessageType = "DynamicBunkerCreateCustomSocketMessage";
char const * const DynamicBunkerCustomSocketSyncMessage::MessageType = "DynamicBunkerCustomSocketSyncMessage";



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

		get(source, target.linkedCellIndex);

		get(source, target.linkedPortalIndex);

		get(source, target.mapX);

		get(source, target.mapZ);

		get(source, target.custom);

	}



	void put(ByteStream &target, DynamicBunkerOpenFloorplanMessage::SocketEntry const &source)

	{

		put(target, source.cellIndex);

		put(target, source.portalIndex);

		put(target, source.label);

		put(target, source.open);

		put(target, source.linkedCellIndex);

		put(target, source.linkedPortalIndex);

		put(target, source.mapX);

		put(target, source.mapZ);

		put(target, source.custom);

	}



	void get(ReadIterator &source, DynamicBunkerOpenFloorplanMessage::BridgeEntry &target)

	{

		get(source, target.hostCellIndex);

		get(source, target.hostPortalIndex);

		get(source, target.graftedCellIndex);

		get(source, target.graftedPortalIndex);

		get(source, target.transform_o2p);

		get(source, target.length);

		get(source, target.width);

		get(source, target.height);

	}



	void put(ByteStream &target, DynamicBunkerOpenFloorplanMessage::BridgeEntry const &source)

	{

		put(target, source.hostCellIndex);

		put(target, source.hostPortalIndex);

		put(target, source.graftedCellIndex);

		put(target, source.graftedPortalIndex);

		put(target, source.transform_o2p);

		put(target, source.length);

		put(target, source.width);

		put(target, source.height);

	}



	void get(ReadIterator &source, DynamicBunkerOpenFloorplanMessage::CustomSocketEntry &target)

	{

		get(source, target.cellIndex);

		get(source, target.socketIndex);

		get(source, target.label);

		get(source, target.doorTransform_o2p);

		get(source, target.open);

		get(source, target.doorwayWidth);

		get(source, target.doorwayHeight);

	}



	void put(ByteStream &target, DynamicBunkerOpenFloorplanMessage::CustomSocketEntry const &source)

	{

		put(target, source.cellIndex);

		put(target, source.socketIndex);

		put(target, source.label);

		put(target, source.doorTransform_o2p);

		put(target, source.open);

		put(target, source.doorwayWidth);

		put(target, source.doorwayHeight);

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

	SocketList const &sockets,

	BridgeList const &bridges,

	CustomSocketList const &customSockets)

: GameNetworkMessage(MessageType),

	m_buildingId(buildingId),

	m_terminalId(terminalId),

	m_selectedCellIndex(selectedCellIndex),

	m_selectedPortalIndex(selectedPortalIndex),

	m_rooms(rooms),

	m_sockets(sockets),

	m_bridges(bridges),

	m_customSockets(customSockets)

{

	addVariable(m_buildingId);

	addVariable(m_terminalId);

	addVariable(m_selectedCellIndex);

	addVariable(m_selectedPortalIndex);

	addVariable(m_rooms);

	addVariable(m_sockets);

	addVariable(m_bridges);

	addVariable(m_customSockets);

}



DynamicBunkerOpenFloorplanMessage::DynamicBunkerOpenFloorplanMessage(Archive::ReadIterator &source)

: GameNetworkMessage(MessageType),

	m_buildingId(),

	m_terminalId(),

	m_selectedCellIndex(0),

	m_selectedPortalIndex(0),

	m_rooms(),

	m_sockets(),

	m_bridges(),

	m_customSockets()

{

	addVariable(m_buildingId);

	addVariable(m_terminalId);

	addVariable(m_selectedCellIndex);

	addVariable(m_selectedPortalIndex);

	addVariable(m_rooms);

	addVariable(m_sockets);

	addVariable(m_bridges);

	addVariable(m_customSockets);

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



DynamicBunkerUnassignRoomMessage::DynamicBunkerUnassignRoomMessage(

	NetworkId const &buildingId,

	NetworkId const &terminalId,

	int32 hostCellIndex,

	int32 hostPortalIndex)

: GameNetworkMessage(MessageType),

	m_buildingId(buildingId),

	m_terminalId(terminalId),

	m_hostCellIndex(hostCellIndex),

	m_hostPortalIndex(hostPortalIndex)

{

	addVariable(m_buildingId);

	addVariable(m_terminalId);

	addVariable(m_hostCellIndex);

	addVariable(m_hostPortalIndex);

}



DynamicBunkerUnassignRoomMessage::DynamicBunkerUnassignRoomMessage(Archive::ReadIterator &source)

: GameNetworkMessage(MessageType),

	m_buildingId(),

	m_terminalId(),

	m_hostCellIndex(0),

	m_hostPortalIndex(0)

{

	addVariable(m_buildingId);

	addVariable(m_terminalId);

	addVariable(m_hostCellIndex);

	addVariable(m_hostPortalIndex);

	unpack(source);

}



DynamicBunkerUnassignRoomMessage::~DynamicBunkerUnassignRoomMessage()

{

}



// ======================================================================



DynamicBunkerUngraftMessage::DynamicBunkerUngraftMessage(

	NetworkId const &buildingId,

	NetworkId const &cellId,

	int32 graftedCellIndex,

	int32 hostCellIndex,

	int32 hostPortalIndex)

: GameNetworkMessage(MessageType),

	m_buildingId(buildingId),

	m_cellId(cellId),

	m_graftedCellIndex(graftedCellIndex),

	m_hostCellIndex(hostCellIndex),

	m_hostPortalIndex(hostPortalIndex)

{

	addVariable(m_buildingId);

	addVariable(m_cellId);

	addVariable(m_graftedCellIndex);

	addVariable(m_hostCellIndex);

	addVariable(m_hostPortalIndex);

}



DynamicBunkerUngraftMessage::DynamicBunkerUngraftMessage(Archive::ReadIterator &source)

: GameNetworkMessage(MessageType),

	m_buildingId(),

	m_cellId(),

	m_graftedCellIndex(0),

	m_hostCellIndex(0),

	m_hostPortalIndex(0)

{

	addVariable(m_buildingId);

	addVariable(m_cellId);

	addVariable(m_graftedCellIndex);

	addVariable(m_hostCellIndex);

	addVariable(m_hostPortalIndex);

	unpack(source);

}



DynamicBunkerUngraftMessage::~DynamicBunkerUngraftMessage()

{

}



// ======================================================================



DynamicBunkerCreateCustomSocketMessage::DynamicBunkerCreateCustomSocketMessage(

	NetworkId const &buildingId,

	NetworkId const &terminalId,

	int32 cellIndex,

	Transform const &doorTransform_o2p,

	std::string const &label,

	float doorwayWidth,

	float doorwayHeight)

: GameNetworkMessage(MessageType),

	m_buildingId(buildingId),

	m_terminalId(terminalId),

	m_cellIndex(cellIndex),

	m_doorTransform_o2p(doorTransform_o2p),

	m_label(label),

	m_doorwayWidth(doorwayWidth),

	m_doorwayHeight(doorwayHeight)

{

	addVariable(m_buildingId);

	addVariable(m_terminalId);

	addVariable(m_cellIndex);

	addVariable(m_doorTransform_o2p);

	addVariable(m_label);

	addVariable(m_doorwayWidth);

	addVariable(m_doorwayHeight);

}



DynamicBunkerCreateCustomSocketMessage::DynamicBunkerCreateCustomSocketMessage(Archive::ReadIterator &source)

: GameNetworkMessage(MessageType),

	m_buildingId(),

	m_terminalId(),

	m_cellIndex(0),

	m_doorTransform_o2p(),

	m_label(),

	m_doorwayWidth(1.0f),

	m_doorwayHeight(2.0f)

{

	addVariable(m_buildingId);

	addVariable(m_terminalId);

	addVariable(m_cellIndex);

	addVariable(m_doorTransform_o2p);

	addVariable(m_label);

	addVariable(m_doorwayWidth);

	addVariable(m_doorwayHeight);

	unpack(source);

}



DynamicBunkerCreateCustomSocketMessage::~DynamicBunkerCreateCustomSocketMessage()

{

}



// ======================================================================



DynamicBunkerCustomSocketSyncMessage::DynamicBunkerCustomSocketSyncMessage(

	NetworkId const &buildingId,

	int32 cellIndex,

	int32 socketIndex,

	std::string const &label,

	Transform const &doorTransform_o2p,

	bool open,

	float doorwayWidth,

	float doorwayHeight)

: GameNetworkMessage(MessageType),

	m_buildingId(buildingId),

	m_cellIndex(cellIndex),

	m_socketIndex(socketIndex),

	m_label(label),

	m_doorTransform_o2p(doorTransform_o2p),

	m_open(open),

	m_doorwayWidth(doorwayWidth),

	m_doorwayHeight(doorwayHeight)

{

	addVariable(m_buildingId);

	addVariable(m_cellIndex);

	addVariable(m_socketIndex);

	addVariable(m_label);

	addVariable(m_doorTransform_o2p);

	addVariable(m_open);

	addVariable(m_doorwayWidth);

	addVariable(m_doorwayHeight);

}



DynamicBunkerCustomSocketSyncMessage::DynamicBunkerCustomSocketSyncMessage(Archive::ReadIterator &source)

: GameNetworkMessage(MessageType),

	m_buildingId(),

	m_cellIndex(0),

	m_socketIndex(0),

	m_label(),

	m_doorTransform_o2p(),

	m_open(true),

	m_doorwayWidth(1.0f),

	m_doorwayHeight(2.0f)

{

	addVariable(m_buildingId);

	addVariable(m_cellIndex);

	addVariable(m_socketIndex);

	addVariable(m_label);

	addVariable(m_doorTransform_o2p);

	addVariable(m_open);

	addVariable(m_doorwayWidth);

	addVariable(m_doorwayHeight);

	unpack(source);

}



DynamicBunkerCustomSocketSyncMessage::~DynamicBunkerCustomSocketSyncMessage()

{

}



// ======================================================================

