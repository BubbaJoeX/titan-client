//
// ClientGameStubs.cpp
//
// Provides stub implementations for 5 game-runtime symbols referenced
// transitively by GroundEnvironment (in clientTerrain.lib). These symbols
// live in clientGame.lib, which cannot be linked here because it pulls in
// the entire game client (~400+ cascading unresolved externals).
//
// GroundEnvironment::alter() references ClientRegionEffectManager and
// Game::getPlayerObject() for region-based day/night effects.
// GroundEnvironment::updateFactionCelestials() references GuildObject
// and Game::ms_sceneId for faction-specific sky rendering.
//
// None of these have meaning in an offline terrain editor.
//

#include "FirstTerrainEditor.h"

#include <string>

class PlayerObject;

// =====================================================================
// ClientRegionEffectManager - region-based permanent day/night checks
// =====================================================================

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

// =====================================================================
// Game - player object access and scene identification
// =====================================================================

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

// =====================================================================
// GuildObject - guild faction data for celestial rendering
// =====================================================================

class GuildObject
{
public:
	static GuildObject const * getGuildObject();
};

GuildObject const * GuildObject::getGuildObject()
{
	return 0;
}
