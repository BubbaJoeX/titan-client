//
// ClientGameStubs.cpp
//
// Minimal game-runtime symbols required by GroundEnvironment and clientTerrain
// water code in editor builds. Keep in sync with symbols referenced from those TUs.
//

#include "FirstTerrainEditor.h"

#include <string>

class PlayerObject;
class ShipObject;

class ClientRegionEffectManager
{
public:
	static bool isCurrentRegionPermanentDay(long regionFlags);
	static bool isCurrentRegionPermanentNight(long regionFlags);
};

bool ClientRegionEffectManager::isCurrentRegionPermanentDay(long)
{
	return false;
}

bool ClientRegionEffectManager::isCurrentRegionPermanentNight(long)
{
	return false;
}

class Game
{
public:
	static PlayerObject *    getPlayerObject();
	static bool                isSpace();
	static bool                isShipScene();
	static ShipObject *        getPlayerContainingShip();

private:
	static std::string ms_sceneId;
};

PlayerObject * Game::getPlayerObject()
{
	return 0;
}

bool Game::isSpace()
{
	return false;
}

bool Game::isShipScene()
{
	return false;
}

ShipObject * Game::getPlayerContainingShip()
{
	return 0;
}

std::string Game::ms_sceneId;

class GuildObject
{
public:
	static GuildObject const * getGuildObject();
};

GuildObject const * GuildObject::getGuildObject()
{
	return 0;
}

// ---------------------------------------------------------------------------
// clientTerrain (ClientGlobalWaterManager2) — config and water sampling
// ---------------------------------------------------------------------------

class ConfigClientGame
{
public:
	static bool  getWaterEnvironmentPoleShaderConstants();
	static bool  getWaterEnvironmentPoleEnabled();
	static float getWaterEnvironmentPole0X();
	static float getWaterEnvironmentPole0Z();
	static float getWaterEnvironmentPole0Radius();
	static float getWaterEnvironmentPole0Amplitude();
	static float getWaterEnvironmentPole0Speed();
	static float getWaterEnvironmentPole0Phase();
	static float getWaterEnvironmentPole1X();
	static float getWaterEnvironmentPole1Z();
	static float getWaterEnvironmentPole1Radius();
	static float getWaterEnvironmentPole1Amplitude();
	static float getWaterEnvironmentPole1Speed();
	static float getWaterEnvironmentPole1Phase();
	static bool  getWaterEnvironmentPoleCpuMeshDisplacement();
	static bool  getWaterEnvironmentFlowFieldEnabled();
};

bool ConfigClientGame::getWaterEnvironmentPoleShaderConstants()
{
	return false;
}

bool ConfigClientGame::getWaterEnvironmentPoleEnabled()
{
	return false;
}

float ConfigClientGame::getWaterEnvironmentPole0X()
{
	return 0.f;
}

float ConfigClientGame::getWaterEnvironmentPole0Z()
{
	return 0.f;
}

float ConfigClientGame::getWaterEnvironmentPole0Radius()
{
	return 0.f;
}

float ConfigClientGame::getWaterEnvironmentPole0Amplitude()
{
	return 0.f;
}

float ConfigClientGame::getWaterEnvironmentPole0Speed()
{
	return 0.f;
}

float ConfigClientGame::getWaterEnvironmentPole0Phase()
{
	return 0.f;
}

float ConfigClientGame::getWaterEnvironmentPole1X()
{
	return 0.f;
}

float ConfigClientGame::getWaterEnvironmentPole1Z()
{
	return 0.f;
}

float ConfigClientGame::getWaterEnvironmentPole1Radius()
{
	return 0.f;
}

float ConfigClientGame::getWaterEnvironmentPole1Amplitude()
{
	return 0.f;
}

float ConfigClientGame::getWaterEnvironmentPole1Speed()
{
	return 0.f;
}

float ConfigClientGame::getWaterEnvironmentPole1Phase()
{
	return 0.f;
}

bool ConfigClientGame::getWaterEnvironmentPoleCpuMeshDisplacement()
{
	return false;
}

bool ConfigClientGame::getWaterEnvironmentFlowFieldEnabled()
{
	return false;
}

class DeveloperWaterEnvironmentOverride
{
public:
	static bool isActive();
};

bool DeveloperWaterEnvironmentOverride::isActive()
{
	return false;
}

namespace WaterEnvironmentFlow
{
	float sampleWaveDisplacementYWorld(float, float, double)
	{
		return 0.f;
	}
}
