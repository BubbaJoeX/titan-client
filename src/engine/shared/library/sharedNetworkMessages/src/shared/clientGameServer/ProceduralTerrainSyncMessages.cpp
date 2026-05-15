// ======================================================================
//
// ProceduralTerrainSyncMessages.cpp
// copyright 2026 Titan
// ======================================================================

#include "sharedNetworkMessages/FirstSharedNetworkMessages.h"
#include "sharedNetworkMessages/ProceduralTerrainSyncMessages.h"

// ----------------------------------------------------------------------

const char * const ProceduralTerrainSyncChunkMessage::MessageType = "ProceduralTerrainSyncChunkMessage";

ProceduralTerrainSyncChunkMessage::ProceduralTerrainSyncChunkMessage(
	std::string const & terrainTreeFileName,
	uint32 totalSize,
	uint32 crc32,
	uint32 byteOffset,
	std::vector<unsigned char> const & chunkBytes) :
	GameNetworkMessage(MessageType),
	m_terrainTreeFileName(terrainTreeFileName),
	m_totalSize(totalSize),
	m_crc32(crc32),
	m_byteOffset(byteOffset),
	m_chunkBytes()
{
	m_chunkBytes.set(chunkBytes);
	addVariable(m_terrainTreeFileName);
	addVariable(m_totalSize);
	addVariable(m_crc32);
	addVariable(m_byteOffset);
	addVariable(m_chunkBytes);
}

// ----------------------------------------------------------------------

ProceduralTerrainSyncChunkMessage::ProceduralTerrainSyncChunkMessage(Archive::ReadIterator & source) :
	GameNetworkMessage(MessageType),
	m_terrainTreeFileName(),
	m_totalSize(),
	m_crc32(),
	m_byteOffset(),
	m_chunkBytes()
{
	addVariable(m_terrainTreeFileName);
	addVariable(m_totalSize);
	addVariable(m_crc32);
	addVariable(m_byteOffset);
	addVariable(m_chunkBytes);
	unpack(source);
}

// ----------------------------------------------------------------------

ProceduralTerrainSyncChunkMessage::~ProceduralTerrainSyncChunkMessage()
{
}
