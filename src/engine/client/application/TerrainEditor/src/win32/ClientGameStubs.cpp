//
// ClientGameStubs.cpp
//
// Minimal game-runtime symbols required by GroundEnvironment in editor builds.
//

#include "FirstTerrainEditor.h"

#include <string>

class PlayerObject;

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
	static PlayerObject * getPlayerObject();

private:
	static std::string ms_sceneId;
};

PlayerObject * Game::getPlayerObject()
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
