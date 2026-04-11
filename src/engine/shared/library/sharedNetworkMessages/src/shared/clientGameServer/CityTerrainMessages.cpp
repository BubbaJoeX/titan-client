// ======================================================================
//
// CityTerrainMessages.cpp
// copyright 2026 Titan
//
// Network messages for city terrain modification system
// ======================================================================

#include "sharedNetworkMessages/FirstSharedNetworkMessages.h"
#include "sharedNetworkMessages/CityTerrainMessages.h"

// ======================================================================
// CityTerrainPaintRequestMessage
// ======================================================================

const char * const CityTerrainPaintRequestMessage::MessageType = "CityTerrainPaintRequestMessage";

CityTerrainPaintRequestMessage::CityTerrainPaintRequestMessage(
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
) :
	GameNetworkMessage(MessageType),
	m_cityId(cityId),
	m_modificationType(modificationType),
	m_shaderTemplate(shaderTemplate),
	m_centerX(centerX),
	m_centerZ(centerZ),
	m_radius(radius),
	m_endX(endX),
	m_endZ(endZ),
	m_width(width),
	m_height(height),
	m_blendDistance(blendDistance)
{
	addVariable(m_cityId);
	addVariable(m_modificationType);
	addVariable(m_shaderTemplate);
	addVariable(m_centerX);
	addVariable(m_centerZ);
	addVariable(m_radius);
	addVariable(m_endX);
	addVariable(m_endZ);
	addVariable(m_width);
	addVariable(m_height);
	addVariable(m_blendDistance);
}

CityTerrainPaintRequestMessage::CityTerrainPaintRequestMessage(Archive::ReadIterator & source) :
	GameNetworkMessage(MessageType),
	m_cityId(),
	m_modificationType(),
	m_shaderTemplate(),
	m_centerX(),
	m_centerZ(),
	m_radius(),
	m_endX(),
	m_endZ(),
	m_width(),
	m_height(),
	m_blendDistance()
{
	addVariable(m_cityId);
	addVariable(m_modificationType);
	addVariable(m_shaderTemplate);
	addVariable(m_centerX);
	addVariable(m_centerZ);
	addVariable(m_radius);
	addVariable(m_endX);
	addVariable(m_endZ);
	addVariable(m_width);
	addVariable(m_height);
	addVariable(m_blendDistance);
	unpack(source);
}

CityTerrainPaintRequestMessage::~CityTerrainPaintRequestMessage()
{
}

// ======================================================================
// CityTerrainPaintResponseMessage
// ======================================================================

const char * const CityTerrainPaintResponseMessage::MessageType = "CityTerrainPaintResponseMessage";

CityTerrainPaintResponseMessage::CityTerrainPaintResponseMessage(bool success, const std::string & regionId, const std::string & errorMessage) :
	GameNetworkMessage(MessageType),
	m_success(success),
	m_regionId(regionId),
	m_errorMessage(errorMessage)
{
	addVariable(m_success);
	addVariable(m_regionId);
	addVariable(m_errorMessage);
}

CityTerrainPaintResponseMessage::CityTerrainPaintResponseMessage(Archive::ReadIterator & source) :
	GameNetworkMessage(MessageType),
	m_success(),
	m_regionId(),
	m_errorMessage()
{
	addVariable(m_success);
	addVariable(m_regionId);
	addVariable(m_errorMessage);
	unpack(source);
}

CityTerrainPaintResponseMessage::~CityTerrainPaintResponseMessage()
{
}

// ======================================================================
// CityTerrainModifyMessage
// ======================================================================

const char * const CityTerrainModifyMessage::MessageType = "CityTerrainModifyMessage";

CityTerrainModifyMessage::CityTerrainModifyMessage(
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
	bool regionActive
) :
	GameNetworkMessage(MessageType),
	m_cityId(cityId),
	m_modificationType(modificationType),
	m_regionId(regionId),
	m_shaderTemplate(shaderTemplate),
	m_centerX(centerX),
	m_centerZ(centerZ),
	m_radius(radius),
	m_endX(endX),
	m_endZ(endZ),
	m_width(width),
	m_height(height),
	m_blendDistance(blendDistance),
	m_regionActive(regionActive)
{
	addVariable(m_cityId);
	addVariable(m_modificationType);
	addVariable(m_regionId);
	addVariable(m_shaderTemplate);
	addVariable(m_centerX);
	addVariable(m_centerZ);
	addVariable(m_radius);
	addVariable(m_endX);
	addVariable(m_endZ);
	addVariable(m_width);
	addVariable(m_height);
	addVariable(m_blendDistance);
	addVariable(m_regionActive);
}

CityTerrainModifyMessage::CityTerrainModifyMessage(Archive::ReadIterator & source) :
	GameNetworkMessage(MessageType),
	m_cityId(),
	m_modificationType(),
	m_regionId(),
	m_shaderTemplate(),
	m_centerX(),
	m_centerZ(),
	m_radius(),
	m_endX(),
	m_endZ(),
	m_width(),
	m_height(),
	m_blendDistance(),
	m_regionActive()
{
	addVariable(m_cityId);
	addVariable(m_modificationType);
	addVariable(m_regionId);
	addVariable(m_shaderTemplate);
	addVariable(m_centerX);
	addVariable(m_centerZ);
	addVariable(m_radius);
	addVariable(m_endX);
	addVariable(m_endZ);
	addVariable(m_width);
	addVariable(m_height);
	addVariable(m_blendDistance);
	addVariable(m_regionActive);
	unpack(source);
}

CityTerrainModifyMessage::~CityTerrainModifyMessage()
{
}

// ======================================================================
// CityTerrainRemoveRequestMessage
// ======================================================================

const char * const CityTerrainRemoveRequestMessage::MessageType = "CityTerrainRemoveRequestMessage";

CityTerrainRemoveRequestMessage::CityTerrainRemoveRequestMessage(int32 cityId, const std::string & regionId) :
	GameNetworkMessage(MessageType),
	m_cityId(cityId),
	m_regionId(regionId)
{
	addVariable(m_cityId);
	addVariable(m_regionId);
}

CityTerrainRemoveRequestMessage::CityTerrainRemoveRequestMessage(Archive::ReadIterator & source) :
	GameNetworkMessage(MessageType),
	m_cityId(),
	m_regionId()
{
	addVariable(m_cityId);
	addVariable(m_regionId);
	unpack(source);
}

CityTerrainRemoveRequestMessage::~CityTerrainRemoveRequestMessage()
{
}

// ======================================================================
// CityTerrainSyncRequestMessage
// ======================================================================

const char * const CityTerrainSyncRequestMessage::MessageType = "CityTerrainSyncRequestMessage";

CityTerrainSyncRequestMessage::CityTerrainSyncRequestMessage(int32 cityId) :
	GameNetworkMessage(MessageType),
	m_cityId(cityId)
{
	addVariable(m_cityId);
}

CityTerrainSyncRequestMessage::CityTerrainSyncRequestMessage(Archive::ReadIterator & source) :
	GameNetworkMessage(MessageType),
	m_cityId()
{
	addVariable(m_cityId);
	unpack(source);
}

CityTerrainSyncRequestMessage::~CityTerrainSyncRequestMessage()
{
}

// ======================================================================
