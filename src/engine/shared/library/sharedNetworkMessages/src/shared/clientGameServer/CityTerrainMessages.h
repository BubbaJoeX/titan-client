// ======================================================================
//
// CityTerrainMessages.h
// copyright 2026 Titan
//
// Network messages for city terrain modification system
// ======================================================================

#ifndef INCLUDED_CityTerrainMessages_H
#define INCLUDED_CityTerrainMessages_H

#include "sharedNetworkMessages/GameNetworkMessage.h"
#include "Archive/AutoDeltaByteStream.h"

// ======================================================================
// Terrain modification types
// ======================================================================

namespace CityTerrainModificationType
{
	enum Type
	{
		MT_SHADER_CIRCLE = 0,
		MT_SHADER_LINE = 1,
		MT_FLATTEN = 2,
		MT_REMOVE = 3,
		MT_CLEAR_ALL = 4,
		MT_SUSPEND = 5,
		MT_RESUME = 6
	};
}

// ======================================================================
// CityTerrainPaintRequestMessage - sent from CLIENT to SERVER to request terrain paint
// ======================================================================

class CityTerrainPaintRequestMessage : public GameNetworkMessage
{
public:
	static const char * const MessageType;

	CityTerrainPaintRequestMessage(
		int32 cityId,
		int32 modificationType,
		const std::string & shaderTemplate,
		float centerX,
		float centerZ,
		float radius,
		float endX,
		float endZ,
		float width,
		float height,
		float blendDistance
	);

	explicit CityTerrainPaintRequestMessage(Archive::ReadIterator & source);
	~CityTerrainPaintRequestMessage();

	int32 getCityId() const { return m_cityId.get(); }
	int32 getModificationType() const { return m_modificationType.get(); }
	const std::string & getShaderTemplate() const { return m_shaderTemplate.get(); }
	float getCenterX() const { return m_centerX.get(); }
	float getCenterZ() const { return m_centerZ.get(); }
	float getRadius() const { return m_radius.get(); }
	float getEndX() const { return m_endX.get(); }
	float getEndZ() const { return m_endZ.get(); }
	float getWidth() const { return m_width.get(); }
	float getHeight() const { return m_height.get(); }
	float getBlendDistance() const { return m_blendDistance.get(); }

private:
	Archive::AutoVariable<int32> m_cityId;
	Archive::AutoVariable<int32> m_modificationType;
	Archive::AutoVariable<std::string> m_shaderTemplate;
	Archive::AutoVariable<float> m_centerX;
	Archive::AutoVariable<float> m_centerZ;
	Archive::AutoVariable<float> m_radius;
	Archive::AutoVariable<float> m_endX;
	Archive::AutoVariable<float> m_endZ;
	Archive::AutoVariable<float> m_width;
	Archive::AutoVariable<float> m_height;
	Archive::AutoVariable<float> m_blendDistance;

	CityTerrainPaintRequestMessage(const CityTerrainPaintRequestMessage &);
	CityTerrainPaintRequestMessage & operator=(const CityTerrainPaintRequestMessage &);
};

// ======================================================================
// CityTerrainPaintResponseMessage - sent from SERVER to CLIENT with result
// ======================================================================

class CityTerrainPaintResponseMessage : public GameNetworkMessage
{
public:
	static const char * const MessageType;

	CityTerrainPaintResponseMessage(bool success, const std::string & regionId, const std::string & errorMessage);
	explicit CityTerrainPaintResponseMessage(Archive::ReadIterator & source);
	~CityTerrainPaintResponseMessage();

	bool getSuccess() const { return m_success.get(); }
	const std::string & getRegionId() const { return m_regionId.get(); }
	const std::string & getErrorMessage() const { return m_errorMessage.get(); }

private:
	Archive::AutoVariable<bool> m_success;
	Archive::AutoVariable<std::string> m_regionId;
	Archive::AutoVariable<std::string> m_errorMessage;

	CityTerrainPaintResponseMessage(const CityTerrainPaintResponseMessage &);
	CityTerrainPaintResponseMessage & operator=(const CityTerrainPaintResponseMessage &);
};

// ======================================================================
// CityTerrainModifyMessage - sent from server to client to apply terrain modification
// ======================================================================

class CityTerrainModifyMessage : public GameNetworkMessage
{
public:
	static const char * const MessageType;

	CityTerrainModifyMessage(
		int32 cityId,
		int32 modificationType,
		const std::string & regionId,
		const std::string & shaderTemplate,
		float centerX,
		float centerZ,
		float radius,
		float endX,
		float endZ,
		float width,
		float height,
		float blendDistance,
		bool regionActive = true
	);

	explicit CityTerrainModifyMessage(Archive::ReadIterator & source);
	~CityTerrainModifyMessage();

	int32 getCityId() const { return m_cityId.get(); }
	int32 getModificationType() const { return m_modificationType.get(); }
	const std::string & getRegionId() const { return m_regionId.get(); }
	const std::string & getShaderTemplate() const { return m_shaderTemplate.get(); }
	float getCenterX() const { return m_centerX.get(); }
	float getCenterZ() const { return m_centerZ.get(); }
	float getRadius() const { return m_radius.get(); }
	float getEndX() const { return m_endX.get(); }
	float getEndZ() const { return m_endZ.get(); }
	float getWidth() const { return m_width.get(); }
	float getHeight() const { return m_height.get(); }
	float getBlendDistance() const { return m_blendDistance.get(); }
	bool getRegionActive() const { return m_regionActive.get(); }

private:
	Archive::AutoVariable<int32> m_cityId;
	Archive::AutoVariable<int32> m_modificationType;
	Archive::AutoVariable<std::string> m_regionId;
	Archive::AutoVariable<std::string> m_shaderTemplate;
	Archive::AutoVariable<float> m_centerX;
	Archive::AutoVariable<float> m_centerZ;
	Archive::AutoVariable<float> m_radius;
	Archive::AutoVariable<float> m_endX;
	Archive::AutoVariable<float> m_endZ;
	Archive::AutoVariable<float> m_width;
	Archive::AutoVariable<float> m_height;
	Archive::AutoVariable<float> m_blendDistance;
	Archive::AutoVariable<bool> m_regionActive;

	CityTerrainModifyMessage(const CityTerrainModifyMessage &);
	CityTerrainModifyMessage & operator=(const CityTerrainModifyMessage &);
};

// ======================================================================
// CityTerrainRemoveRequestMessage - client requests to remove a region
// ======================================================================

class CityTerrainRemoveRequestMessage : public GameNetworkMessage
{
public:
	static const char * const MessageType;

	CityTerrainRemoveRequestMessage(int32 cityId, const std::string & regionId);
	explicit CityTerrainRemoveRequestMessage(Archive::ReadIterator & source);
	~CityTerrainRemoveRequestMessage();

	int32 getCityId() const { return m_cityId.get(); }
	const std::string & getRegionId() const { return m_regionId.get(); }

private:
	Archive::AutoVariable<int32> m_cityId;
	Archive::AutoVariable<std::string> m_regionId;

	CityTerrainRemoveRequestMessage(const CityTerrainRemoveRequestMessage &);
	CityTerrainRemoveRequestMessage & operator=(const CityTerrainRemoveRequestMessage &);
};

// ======================================================================
// CityTerrainSyncRequestMessage - client requests terrain sync for a city
// ======================================================================

class CityTerrainSyncRequestMessage : public GameNetworkMessage
{
public:
	static const char * const MessageType;

	CityTerrainSyncRequestMessage(int32 cityId);
	explicit CityTerrainSyncRequestMessage(Archive::ReadIterator & source);
	~CityTerrainSyncRequestMessage();

	int32 getCityId() const { return m_cityId.get(); }

private:
	Archive::AutoVariable<int32> m_cityId;

	CityTerrainSyncRequestMessage(const CityTerrainSyncRequestMessage &);
	CityTerrainSyncRequestMessage & operator=(const CityTerrainSyncRequestMessage &);
};

// ======================================================================

#endif // INCLUDED_CityTerrainMessages_H

