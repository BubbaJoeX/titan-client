// ======================================================================
//
// ProceduralTerrainSyncMessages.h
// copyright 2026 Titan
//
// Chunked sync of procedural terrain (.trn) between God Client, game server,
// and all connected game clients.
// ======================================================================

#ifndef INCLUDED_ProceduralTerrainSyncMessages_H
#define INCLUDED_ProceduralTerrainSyncMessages_H

#include "sharedNetworkMessages/GameNetworkMessage.h"
#include "Archive/AutoDeltaByteStream.h"

// ======================================================================
// ProceduralTerrainSyncChunkMessage
// Reliable, ordered chunks: first chunk has byteOffset 0 and initializes
// a transfer; last chunk has byteOffset + chunkSize == totalSize.
// ======================================================================

class ProceduralTerrainSyncChunkMessage : public GameNetworkMessage
{
public:
	static const char * const MessageType;

	ProceduralTerrainSyncChunkMessage(
		std::string const & terrainTreeFileName,
		uint32 totalSize,
		uint32 crc32,
		uint32 byteOffset,
		std::vector<unsigned char> const & chunkBytes);

	explicit ProceduralTerrainSyncChunkMessage(Archive::ReadIterator & source);
	~ProceduralTerrainSyncChunkMessage();

	std::string const & getTerrainTreeFileName() const { return m_terrainTreeFileName.get(); }
	uint32 getTotalSize() const { return m_totalSize.get(); }
	uint32 getCrc32() const { return m_crc32.get(); }
	uint32 getByteOffset() const { return m_byteOffset.get(); }
	std::vector<unsigned char> const & getChunkBytes() const { return m_chunkBytes.get(); }

private:
	Archive::AutoVariable<std::string> m_terrainTreeFileName;
	Archive::AutoVariable<uint32> m_totalSize;
	Archive::AutoVariable<uint32> m_crc32;
	Archive::AutoVariable<uint32> m_byteOffset;
	Archive::AutoArray<unsigned char> m_chunkBytes;

	ProceduralTerrainSyncChunkMessage(ProceduralTerrainSyncChunkMessage const &);
	ProceduralTerrainSyncChunkMessage & operator=(ProceduralTerrainSyncChunkMessage const &);
};

#endif // INCLUDED_ProceduralTerrainSyncMessages_H
