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
#include "sharedFoundation/NetworkId.h"
#include "sharedFoundation/NetworkIdArchive.h"
#include "sharedNetworkMessages/GameNetworkMessage.h"

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
		int32 linkedCellIndex;
		int32 linkedPortalIndex;
		float mapX;
		float mapZ;
		bool custom;
	};

	struct BridgeEntry
	{
		int32 hostCellIndex;
		int32 hostPortalIndex;
		int32 graftedCellIndex;
		int32 graftedPortalIndex;
		Transform transform_o2p;
		float length;
		float width;
		float height;
	};

	struct CustomSocketEntry
	{
		int32 cellIndex;
		int32 socketIndex;
		std::string label;
		Transform doorTransform_o2p;
		bool open;
		float doorwayWidth;
		float doorwayHeight;
	};

	typedef std::vector<RoomEntry> RoomList;
	typedef std::vector<SocketEntry> SocketList;
	typedef std::vector<BridgeEntry> BridgeList;
	typedef std::vector<CustomSocketEntry> CustomSocketList;

	DynamicBunkerOpenFloorplanMessage(
		NetworkId const &buildingId,
		NetworkId const &terminalId,
		int32 selectedCellIndex,
		int32 selectedPortalIndex,
		RoomList const &rooms,
		SocketList const &sockets,
		BridgeList const &bridges,
		CustomSocketList const &customSockets);

	explicit DynamicBunkerOpenFloorplanMessage(Archive::ReadIterator &source);
	~DynamicBunkerOpenFloorplanMessage();

	NetworkId const &getBuildingId() const { return m_buildingId.get(); }
	NetworkId const &getTerminalId() const { return m_terminalId.get(); }
	int32 getSelectedCellIndex() const { return m_selectedCellIndex.get(); }
	int32 getSelectedPortalIndex() const { return m_selectedPortalIndex.get(); }
	RoomList const &getRooms() const { return m_rooms.get(); }
	SocketList const &getSockets() const { return m_sockets.get(); }
	BridgeList const &getBridges() const { return m_bridges.get(); }
	CustomSocketList const &getCustomSockets() const { return m_customSockets.get(); }

private:

	Archive::AutoVariable<NetworkId> m_buildingId;
	Archive::AutoVariable<NetworkId> m_terminalId;
	Archive::AutoVariable<int32> m_selectedCellIndex;
	Archive::AutoVariable<int32> m_selectedPortalIndex;
	Archive::AutoVariable<RoomList> m_rooms;
	Archive::AutoVariable<SocketList> m_sockets;
	Archive::AutoVariable<BridgeList> m_bridges;
	Archive::AutoVariable<CustomSocketList> m_customSockets;
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
// Client -> server: unassign / detach graft from a snap socket
// ======================================================================

class DynamicBunkerUnassignRoomMessage : public GameNetworkMessage
{
public:

	static char const * const MessageType;

	DynamicBunkerUnassignRoomMessage(
		NetworkId const &buildingId,
		NetworkId const &terminalId,
		int32 hostCellIndex,
		int32 hostPortalIndex);

	explicit DynamicBunkerUnassignRoomMessage(Archive::ReadIterator &source);
	~DynamicBunkerUnassignRoomMessage();

	NetworkId const &getBuildingId() const { return m_buildingId.get(); }
	NetworkId const &getTerminalId() const { return m_terminalId.get(); }
	int32 getHostCellIndex() const { return m_hostCellIndex.get(); }
	int32 getHostPortalIndex() const { return m_hostPortalIndex.get(); }

private:

	Archive::AutoVariable<NetworkId> m_buildingId;
	Archive::AutoVariable<NetworkId> m_terminalId;
	Archive::AutoVariable<int32> m_hostCellIndex;
	Archive::AutoVariable<int32> m_hostPortalIndex;
};

// ======================================================================
// Server -> client: remove a previously grafted room
// ======================================================================

class DynamicBunkerUngraftMessage : public GameNetworkMessage
{
public:

	static char const * const MessageType;

	DynamicBunkerUngraftMessage(
		NetworkId const &buildingId,
		NetworkId const &cellId,
		int32 graftedCellIndex,
		int32 hostCellIndex,
		int32 hostPortalIndex);

	explicit DynamicBunkerUngraftMessage(Archive::ReadIterator &source);
	~DynamicBunkerUngraftMessage();

	NetworkId const &getBuildingId() const { return m_buildingId.get(); }
	NetworkId const &getCellId() const { return m_cellId.get(); }
	int32 getGraftedCellIndex() const { return m_graftedCellIndex.get(); }
	int32 getHostCellIndex() const { return m_hostCellIndex.get(); }
	int32 getHostPortalIndex() const { return m_hostPortalIndex.get(); }

private:

	Archive::AutoVariable<NetworkId> m_buildingId;
	Archive::AutoVariable<NetworkId> m_cellId;
	Archive::AutoVariable<int32> m_graftedCellIndex;
	Archive::AutoVariable<int32> m_hostCellIndex;
	Archive::AutoVariable<int32> m_hostPortalIndex;
};

// ======================================================================
// Server -> client: sync a custom snap point onto the local PortalProperty
// ======================================================================

class DynamicBunkerCustomSocketSyncMessage : public GameNetworkMessage
{
public:

	static char const * const MessageType;

	DynamicBunkerCustomSocketSyncMessage(
		NetworkId const &buildingId,
		int32 cellIndex,
		int32 socketIndex,
		std::string const &label,
		Transform const &doorTransform_o2p,
		bool open,
		float doorwayWidth,
		float doorwayHeight);

	explicit DynamicBunkerCustomSocketSyncMessage(Archive::ReadIterator &source);
	~DynamicBunkerCustomSocketSyncMessage();

	NetworkId const &getBuildingId() const { return m_buildingId.get(); }
	int32 getCellIndex() const { return m_cellIndex.get(); }
	int32 getSocketIndex() const { return m_socketIndex.get(); }
	std::string const &getLabel() const { return m_label.get(); }
	Transform const &getDoorTransform_o2p() const { return m_doorTransform_o2p.get(); }
	bool getOpen() const { return m_open.get(); }
	float getDoorwayWidth() const { return m_doorwayWidth.get(); }
	float getDoorwayHeight() const { return m_doorwayHeight.get(); }

private:

	Archive::AutoVariable<NetworkId> m_buildingId;
	Archive::AutoVariable<int32> m_cellIndex;
	Archive::AutoVariable<int32> m_socketIndex;
	Archive::AutoVariable<std::string> m_label;
	Archive::AutoVariable<Transform> m_doorTransform_o2p;
	Archive::AutoVariable<bool> m_open;
	Archive::AutoVariable<float> m_doorwayWidth;
	Archive::AutoVariable<float> m_doorwayHeight;
};

// ======================================================================
// Client -> server: create a custom snap point on a cell wall
// ======================================================================

class DynamicBunkerCreateCustomSocketMessage : public GameNetworkMessage
{
public:

	static char const * const MessageType;

	DynamicBunkerCreateCustomSocketMessage(
		NetworkId const &buildingId,
		NetworkId const &terminalId,
		int32 cellIndex,
		Transform const &doorTransform_o2p,
		std::string const &label,
		float doorwayWidth,
		float doorwayHeight);

	explicit DynamicBunkerCreateCustomSocketMessage(Archive::ReadIterator &source);
	~DynamicBunkerCreateCustomSocketMessage();

	NetworkId const &getBuildingId() const { return m_buildingId.get(); }
	NetworkId const &getTerminalId() const { return m_terminalId.get(); }
	int32 getCellIndex() const { return m_cellIndex.get(); }
	Transform const &getDoorTransform_o2p() const { return m_doorTransform_o2p.get(); }
	std::string const &getLabel() const { return m_label.get(); }
	float getDoorwayWidth() const { return m_doorwayWidth.get(); }
	float getDoorwayHeight() const { return m_doorwayHeight.get(); }

private:

	Archive::AutoVariable<NetworkId> m_buildingId;
	Archive::AutoVariable<NetworkId> m_terminalId;
	Archive::AutoVariable<int32> m_cellIndex;
	Archive::AutoVariable<Transform> m_doorTransform_o2p;
	Archive::AutoVariable<std::string> m_label;
	Archive::AutoVariable<float> m_doorwayWidth;
	Archive::AutoVariable<float> m_doorwayHeight;
};

// ======================================================================
// Client -> server: revert building to original POB cells only
// ======================================================================

class DynamicBunkerRevertBuildingMessage : public GameNetworkMessage
{
public:

	static char const * const MessageType;

	DynamicBunkerRevertBuildingMessage(
		NetworkId const &buildingId,
		NetworkId const &terminalId);

	explicit DynamicBunkerRevertBuildingMessage(Archive::ReadIterator &source);
	~DynamicBunkerRevertBuildingMessage();

	NetworkId const &getBuildingId() const { return m_buildingId.get(); }
	NetworkId const &getTerminalId() const { return m_terminalId.get(); }

private:

	Archive::AutoVariable<NetworkId> m_buildingId;
	Archive::AutoVariable<NetworkId> m_terminalId;
};

// ======================================================================
// Server -> client: building dynamic layout was cleared
// ======================================================================

class DynamicBunkerBuildingRevertedMessage : public GameNetworkMessage
{
public:

	static char const * const MessageType;

	explicit DynamicBunkerBuildingRevertedMessage(NetworkId const &buildingId);
	explicit DynamicBunkerBuildingRevertedMessage(Archive::ReadIterator &source);
	~DynamicBunkerBuildingRevertedMessage();

	NetworkId const &getBuildingId() const { return m_buildingId.get(); }

private:

	Archive::AutoVariable<NetworkId> m_buildingId;
};

// ======================================================================
// Client -> server: nudge/adjust a grafted room transform
// ======================================================================

class DynamicBunkerAdjustGraftMessage : public GameNetworkMessage
{
public:

	static char const * const MessageType;

	DynamicBunkerAdjustGraftMessage(
		NetworkId const &buildingId,
		NetworkId const &terminalId,
		int32 graftedCellIndex,
		Transform const &cellTransform_o2p);

	explicit DynamicBunkerAdjustGraftMessage(Archive::ReadIterator &source);
	~DynamicBunkerAdjustGraftMessage();

	NetworkId const &getBuildingId() const { return m_buildingId.get(); }
	NetworkId const &getTerminalId() const { return m_terminalId.get(); }
	int32 getGraftedCellIndex() const { return m_graftedCellIndex.get(); }
	Transform const &getCellTransform_o2p() const { return m_cellTransform_o2p.get(); }

private:

	Archive::AutoVariable<NetworkId> m_buildingId;
	Archive::AutoVariable<NetworkId> m_terminalId;
	Archive::AutoVariable<int32> m_graftedCellIndex;
	Archive::AutoVariable<Transform> m_cellTransform_o2p;
};

// ======================================================================
// Server -> client: authoritative graft cell transform update
// ======================================================================

class DynamicBunkerGraftTransformMessage : public GameNetworkMessage
{
public:

	static char const * const MessageType;

	DynamicBunkerGraftTransformMessage(
		NetworkId const &buildingId,
		int32 graftedCellIndex,
		Transform const &cellTransform_o2p);

	explicit DynamicBunkerGraftTransformMessage(Archive::ReadIterator &source);
	~DynamicBunkerGraftTransformMessage();

	NetworkId const &getBuildingId() const { return m_buildingId.get(); }
	int32 getGraftedCellIndex() const { return m_graftedCellIndex.get(); }
	Transform const &getCellTransform_o2p() const { return m_cellTransform_o2p.get(); }

private:

	Archive::AutoVariable<NetworkId> m_buildingId;
	Archive::AutoVariable<int32> m_graftedCellIndex;
	Archive::AutoVariable<Transform> m_cellTransform_o2p;
};

// ======================================================================

namespace Archive
{
	void get(ReadIterator &source, DynamicBunkerOpenFloorplanMessage::RoomEntry &target);
	void put(ByteStream &target, DynamicBunkerOpenFloorplanMessage::RoomEntry const &source);
	void get(ReadIterator &source, DynamicBunkerOpenFloorplanMessage::SocketEntry &target);
	void put(ByteStream &target, DynamicBunkerOpenFloorplanMessage::SocketEntry const &source);
	void get(ReadIterator &source, DynamicBunkerOpenFloorplanMessage::BridgeEntry &target);
	void put(ByteStream &target, DynamicBunkerOpenFloorplanMessage::BridgeEntry const &source);
	void get(ReadIterator &source, DynamicBunkerOpenFloorplanMessage::CustomSocketEntry &target);
	void put(ByteStream &target, DynamicBunkerOpenFloorplanMessage::CustomSocketEntry const &source);
}

// ======================================================================

#endif
