// ======================================================================
//
// SwgCameraMapCapture.cpp
//
// ======================================================================

#include "clientGame/FirstClientGame.h"
#include "clientGame/SwgCameraMapCapture.h"

#include "clientGame/ClientObject.h"
#include "clientGame/FreeCamera.h"
#include "clientGame/Game.h"
#include "clientGame/GroundScene.h"
#include "clientGraphics/Camera.h"
#include "clientGraphics/Graphics.h"
#include "clientObject/GameCamera.h"
#include "clientParticle/ParticleManager.h"
#include "clientTerrain/ClientProceduralTerrainAppearance.h"
#include "clientTerrain/GroundEnvironment.h"
#include "clientTerrain/WeatherManager.h"
#include "clientUserInterface/CuiManager.h"
#include "sharedTerrain/TerrainObject.h"
#include "sharedFoundation/ConfigFile.h"
#include "sharedFoundation/ExitChain.h"
#include "sharedFoundation/FloatMath.h"
#include "sharedGame/GameObjectTypes.h"
#include "sharedGame/SharedObjectTemplate.h"
#include "sharedFoundation/Os.h"
#include "sharedMath/Vector.h"
#include "sharedObject/ObjectList.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

// ======================================================================

namespace SwgCameraMapCaptureNamespace
{
	bool  s_installed = false;

	bool  s_mapCaptureEnabled = false;
	float s_tileWorldSize = 512.f;
	float s_cameraHeight = 700.f;
	int   s_gridRadius = 0;
	int   s_settleFrames = 2;
	char  s_outputDir[Os::MAX_PATH_LENGTH] = "mapcapture";

	bool  s_filterCreatures = true;
	bool  s_filterLairs = true;
	bool  s_filterNpcLike = true;

	bool  s_suppressFog = true;
	bool  s_suppressParticles = true;
	bool  s_suppressWeather = true;

	bool  s_useFixedWorldBounds = true;
	float s_worldMinX = -8000.f;
	float s_worldMaxX = 8000.f;
	float s_worldMinZ = -8000.f;
	float s_worldMaxZ = 8000.f;

	int   s_numCols = 1;
	int   s_numRows = 1;
	int   s_totalTiles = 1;

	bool  s_batchActive = false;
	bool  s_waitingForStart = false;

	int   s_savedView = GroundScene::CI_freeChase;
	bool  s_savedFog = true;
	bool  s_savedParticles = true;
	bool  s_savedEnvPaused = false;

	real  s_savedNear = 1.f;
	real  s_savedFar = 5000.f;
	real  s_savedHorizontalFov = PI / 3.f;

	int   s_originTileX = 0;
	int   s_originTileZ = 0;

	int   s_tileLinearIndex = 0;
	int   s_tileSide = 1;
	int   s_settleRemaining = 0;
	bool  s_pendingShot = false;

	bool  s_appliedProjection = false;

	bool  s_externalRequestPending = false;
	bool  s_externalRequestActive = false;
	bool  s_batchCompleted = false;
	SwgCameraMapCapture::MapCaptureRequest s_pendingRequest;

	int   s_savedViewportWidth = 0;
	int   s_savedViewportHeight = 0;
	bool  s_savedViewport = false;
	int   s_savedScreenShotFormat = 0;

	float s_savedLodThreshold = 512.f;
	float s_savedHighLodThreshold = 512.f;
	bool  s_savedTerrainLod = false;

	int   s_lastNumCols = 0;
	int   s_lastNumRows = 0;
	int   s_lastTotalTiles = 0;
	char  s_lastOutputDir[Os::MAX_PATH_LENGTH] = "";

	Vector s_savedPlayerPosition_w;
	bool   s_savedPlayerPosition = false;
	bool   s_savedForceRenderEntireMap = false;

	void getTileCenterXZ(int linearIndex, float &centerX, float &centerZ);

	Object *getCapturePlayer(GroundScene *groundScene)
	{
		if (groundScene)
		{
			Object *const scenePlayer = groundScene->getPlayer();
			if (scenePlayer)
				return scenePlayer;
		}

		return Game::getPlayer();
	}

	bool isCapturePlayerReady(GroundScene *groundScene)
	{
		Object *const player = getCapturePlayer(groundScene);
		if (!player)
			return false;

		return Game::isGodClient() || Game::getPlayerObject() != nullptr || player->isInWorld();
	}

	//----------------------------------------------------------------------

	void repositionPlayerForTileCapture()
	{
		if (!s_externalRequestActive)
			return;

		GroundScene *const groundScene = dynamic_cast<GroundScene *>(Game::getScene());
		Object *const player = getCapturePlayer(groundScene);
		if (!player)
			return;

		float centerX = 0.f;
		float centerZ = 0.f;
		getTileCenterXZ(s_tileLinearIndex, centerX, centerZ);

		Vector pos(centerX, player->getPosition_w().y, centerZ);
		if (TerrainObject *const terrain = TerrainObject::getInstance())
		{
			float height = 0.f;
			if (terrain->getHeightForceChunkCreation(pos, height))
				pos.y = height + 2.f;
		}

		player->setPosition_w(pos);
	}

	void restorePlayerPosition()
	{
		if (!s_savedPlayerPosition)
			return;

		GroundScene *const groundScene = dynamic_cast<GroundScene *>(Game::getScene());
		Object *const player = getCapturePlayer(groundScene);
		if (player)
			player->setPosition_w(s_savedPlayerPosition_w);

		s_savedPlayerPosition = false;
	}

	void beginExternalCaptureEnvironment()
	{
		GroundScene *const groundScene = dynamic_cast<GroundScene *>(Game::getScene());
		Object *const player = getCapturePlayer(groundScene);
		if (player && !s_savedPlayerPosition)
		{
			s_savedPlayerPosition_w = player->getPosition_w();
			s_savedPlayerPosition = true;
		}
	}

	void endExternalCaptureEnvironment()
	{
		Graphics::setForceScreenShotBackBuffer(false, true);
		ClientProceduralTerrainAppearance::setForceRenderEntireMapEnabled(s_savedForceRenderEntireMap);
		restorePlayerPosition();
	}

	bool isTerrainPrewarmComplete()
	{
		if (!s_externalRequestPending && !s_externalRequestActive)
			return true;

		return ClientProceduralTerrainAppearance::isForceRenderEntireMapComplete();
	}

	//----------------------------------------------------------------------

	void applyRequestSettings(SwgCameraMapCapture::MapCaptureRequest const &request)
	{
		if (request.outputDir[0])
			IGNORE_RETURN(strncpy(s_outputDir, request.outputDir, sizeof(s_outputDir)));
		else
			IGNORE_RETURN(strncpy(s_outputDir, "mapcapture", sizeof(s_outputDir)));
		s_outputDir[sizeof(s_outputDir) - 1] = '\0';

		s_tileWorldSize = request.tileWorldSize;
		s_cameraHeight = request.cameraHeight;
		s_settleFrames = request.settleFrames;
		s_filterCreatures = request.filterCreatures;
		s_filterLairs = request.filterLairs;
		s_filterNpcLike = request.filterNpcLike;
		s_suppressFog = request.suppressFog;
		s_suppressParticles = request.suppressParticles;
		s_suppressWeather = request.suppressWeather;
		s_useFixedWorldBounds = request.useFixedWorldBounds;
		s_worldMinX = request.worldMinX;
		s_worldMaxX = request.worldMaxX;
		s_worldMinZ = request.worldMinZ;
		s_worldMaxZ = request.worldMaxZ;

		if (s_tileWorldSize < 1.f)
			s_tileWorldSize = 1.f;
		if (s_cameraHeight < 1.f)
			s_cameraHeight = 1.f;
		if (s_settleFrames < 0)
			s_settleFrames = 0;

		if (s_worldMinX > s_worldMaxX)
		{
			float const t = s_worldMinX;
			s_worldMinX = s_worldMaxX;
			s_worldMaxX = t;
		}
		if (s_worldMinZ > s_worldMaxZ)
		{
			float const t = s_worldMinZ;
			s_worldMinZ = s_worldMaxZ;
			s_worldMaxZ = t;
		}

		if (request.viewportWidth > 0 && request.viewportHeight > 0)
		{
			s_savedViewportWidth = Graphics::getCurrentRenderTargetWidth();
			s_savedViewportHeight = Graphics::getCurrentRenderTargetHeight();
			if (s_savedViewportWidth != request.viewportWidth || s_savedViewportHeight != request.viewportHeight)
			{
				Graphics::resize(request.viewportWidth, request.viewportHeight);
				s_savedViewport = true;
			}
			else
				s_savedViewport = false;
		}
		else
			s_savedViewport = false;

		s_savedScreenShotFormat = static_cast<int>(Graphics::getScreenShotFormat());
		Graphics::setScreenShotFormat(GSSF_tga);
	}

	void restoreRequestSettings()
	{
		if (s_savedViewport)
		{
			Graphics::resize(s_savedViewportWidth, s_savedViewportHeight);
			s_savedViewport = false;
		}

		Graphics::setScreenShotFormat(static_cast<GlScreenShotFormat>(s_savedScreenShotFormat));
	}

	void resetExternalCaptureRequest()
	{
		s_externalRequestPending = false;
		s_externalRequestActive = false;
		s_waitingForStart = false;
		s_batchCompleted = false;
		endExternalCaptureEnvironment();
		restoreRequestSettings();
	}

	bool isCaptureEnabled()
	{
		return s_mapCaptureEnabled || s_externalRequestPending || s_externalRequestActive;
	}

	//----------------------------------------------------------------------

	void loadConfig()
	{
		// Use Section::getKey* only — ConfigFile::getKeyBool("SwgCameraClient", ...) creates a lazy
		// section+false key when the section is missing (e.g. .include failed / wrong cwd), which then
		// blocks real values from ever being read. Also allow [ClientGame] mapCaptureEnabled as a switch.
		ConfigFile::Section *const swg = ConfigFile::getSection("SwgCameraClient");
		ConfigFile::Section *const cg = ConfigFile::getSection("ClientGame");

		bool const enableSwg = swg ? swg->getKeyBool("mapCaptureEnabled", 0, false) : false;
		bool const enableCg = cg ? cg->getKeyBool("mapCaptureEnabled", 0, false) : false;
		s_mapCaptureEnabled = enableSwg || enableCg;

		if (swg)
		{
			s_tileWorldSize = swg->getKeyFloat("mapCaptureTileWorldSize", 0, 512.f);
			s_cameraHeight = swg->getKeyFloat("mapCaptureCameraHeight", 0, 700.f);
			s_gridRadius = swg->getKeyInt("mapCaptureGridRadius", 0, 0);
			s_settleFrames = swg->getKeyInt("mapCaptureSettleFrames", 0, 2);

			char const *const dir = swg->getKeyString("mapCaptureOutputDirectory", 0, "mapcapture");
			if (dir && *dir)
				IGNORE_RETURN(strncpy(s_outputDir, dir, sizeof(s_outputDir)));
			else
				IGNORE_RETURN(strncpy(s_outputDir, "mapcapture", sizeof(s_outputDir)));
			s_outputDir[sizeof(s_outputDir) - 1] = '\0';

			s_filterCreatures = swg->getKeyBool("mapCaptureFilterCreatures", 0, true);
			s_filterLairs = swg->getKeyBool("mapCaptureFilterLairs", 0, true);
			s_filterNpcLike = swg->getKeyBool("mapCaptureFilterNpcLike", 0, true);

			s_suppressFog = swg->getKeyBool("mapCaptureSuppressFog", 0, true);
			s_suppressParticles = swg->getKeyBool("mapCaptureSuppressParticles", 0, true);
			s_suppressWeather = swg->getKeyBool("mapCaptureSuppressWeather", 0, true);

			s_useFixedWorldBounds = swg->getKeyBool("mapCaptureUseFixedWorldBounds", 0, true);
			s_worldMinX = swg->getKeyFloat("mapCaptureWorldMinX", 0, -8000.f);
			s_worldMaxX = swg->getKeyFloat("mapCaptureWorldMaxX", 0, 8000.f);
			s_worldMinZ = swg->getKeyFloat("mapCaptureWorldMinZ", 0, -8000.f);
			s_worldMaxZ = swg->getKeyFloat("mapCaptureWorldMaxZ", 0, 8000.f);
		}
		else
		{
			s_tileWorldSize = 512.f;
			s_cameraHeight = 700.f;
			s_gridRadius = 0;
			s_settleFrames = 2;
			IGNORE_RETURN(strncpy(s_outputDir, "mapcapture", sizeof(s_outputDir)));
			s_outputDir[sizeof(s_outputDir) - 1] = '\0';
			s_filterCreatures = true;
			s_filterLairs = true;
			s_filterNpcLike = true;
			s_suppressFog = true;
			s_suppressParticles = true;
			s_suppressWeather = true;
			s_useFixedWorldBounds = true;
			s_worldMinX = -8000.f;
			s_worldMaxX = 8000.f;
			s_worldMinZ = -8000.f;
			s_worldMaxZ = 8000.f;
		}

		if (s_tileWorldSize < 1.f)
			s_tileWorldSize = 1.f;
		if (s_cameraHeight < 1.f)
			s_cameraHeight = 1.f;
		if (s_gridRadius < 0)
			s_gridRadius = 0;
		if (s_settleFrames < 0)
			s_settleFrames = 0;

		if (s_worldMinX > s_worldMaxX)
		{
			float const t = s_worldMinX;
			s_worldMinX = s_worldMaxX;
			s_worldMaxX = t;
		}
		if (s_worldMinZ > s_worldMaxZ)
		{
			float const t = s_worldMinZ;
			s_worldMinZ = s_worldMaxZ;
			s_worldMaxZ = t;
		}
	}

	//----------------------------------------------------------------------

	bool objectRenderSkipPredicate(Object const *object)
	{
		if (!object)
			return false;

		ClientObject const *const co = object->asClientObject();
		if (!co)
			return false;

		int const got = co->getGameObjectType();

		if (s_filterLairs && got == SharedObjectTemplate::GOT_lair)
			return true;

		if (s_filterNpcLike && got == SharedObjectTemplate::GOT_vendor)
			return true;

		if (s_filterNpcLike && got == SharedObjectTemplate::GOT_creature_character)
			return true;

		if (s_filterCreatures && GameObjectTypes::getMaskedType(got) == SharedObjectTemplate::GOT_creature && got != SharedObjectTemplate::GOT_creature_character)
			return true;

		return false;
	}

	//----------------------------------------------------------------------

	void restoreEnvironmentOnly()
	{
		if (!s_batchActive)
			return;

		if (s_suppressFog)
			GroundEnvironment::getInstance().setEnableFog(s_savedFog);

		if (s_suppressParticles)
			ParticleManager::setParticlesEnabled(s_savedParticles);

		if (s_suppressWeather)
		{
			WeatherManager::setWindScale(1.f);
			WeatherManager::setNormalizedWindVelocity_w(Vector(1.f, 0.f, 0.f));
			GroundEnvironment::getInstance().setPaused(s_savedEnvPaused);
		}

		ObjectList::clearObjectRenderSkipPredicate();
		s_batchActive = false;
	}

	//----------------------------------------------------------------------

	void getTileCenterXZ(int const linearIndex, float &centerX, float &centerZ)
	{
		if (s_useFixedWorldBounds)
		{
			int const row = linearIndex / s_numCols;
			int const inner = linearIndex % s_numCols;
			int const col = (row % 2 == 0) ? inner : (s_numCols - 1 - inner);
			centerX = s_worldMinX + (static_cast<float>(col) + 0.5f) * s_tileWorldSize;
			centerZ = s_worldMaxZ - (static_cast<float>(row) + 0.5f) * s_tileWorldSize;
		}
		else
		{
			int const side = s_tileSide;
			int const ix = linearIndex % side - s_gridRadius;
			int const iz = linearIndex / side - s_gridRadius;
			centerX = (static_cast<float>(s_originTileX) + 0.5f + static_cast<float>(ix)) * s_tileWorldSize;
			centerZ = (static_cast<float>(s_originTileZ) + 0.5f + static_cast<float>(iz)) * s_tileWorldSize;
		}
	}
}

using namespace SwgCameraMapCaptureNamespace;

// ======================================================================

void SwgCameraMapCapture::install()
{
	if (s_installed)
		return;

	s_installed = true;
	loadConfig();
	if (!ConfigFile::getSection("SwgCameraClient"))
		REPORT_LOG(true, ("SwgCameraMapCapture: [SwgCameraClient] section missing from ConfigFile (client.cfg .include failed or wrong cwd). Map keys ignored; set mapCaptureEnabled in [ClientGame] or fix paths.\n"));
	else
		REPORT_LOG(s_mapCaptureEnabled, ("SwgCameraMapCapture: map capture is enabled (see batch started / screenShot messages in this log).\n"));
	ExitChain::add(&remove, "SwgCameraMapCapture::remove");
}

// ----------------------------------------------------------------------

void SwgCameraMapCapture::remove()
{
	if (!s_installed)
		return;

	GroundScene *const gs = dynamic_cast<GroundScene *>(Game::getScene());
	endBatch(gs);

	s_installed = false;
}

// ----------------------------------------------------------------------

void SwgCameraMapCapture::advanceTileIndex()
{
	++s_tileLinearIndex;
	if (s_tileLinearIndex >= s_totalTiles)
	{
		if (s_externalRequestActive)
			s_batchCompleted = true;

		GroundScene *const gs = dynamic_cast<GroundScene *>(Game::getScene());
		endBatch(gs);
		return;
	}

	s_settleRemaining = s_settleFrames;
	s_pendingShot = false;
}

// ----------------------------------------------------------------------

void SwgCameraMapCapture::beginBatch(GroundScene *groundScene)
{
	if (s_batchActive || !groundScene || groundScene->isLoading())
		return;

	if (!isCapturePlayerReady(groundScene))
		return;

	if (s_externalRequestPending)
	{
		applyRequestSettings(s_pendingRequest);
		s_externalRequestPending = false;
		s_externalRequestActive = true;
		beginExternalCaptureEnvironment();
	}
	else
		loadConfig();

	if (!isCaptureEnabled())
		return;

	s_savedView = groundScene->getCurrentView();

	groundScene->setView(GroundScene::CI_free);

	if (s_useFixedWorldBounds)
	{
		float const width = s_worldMaxX - s_worldMinX;
		float const depth = s_worldMaxZ - s_worldMinZ;
		s_numCols = std::max(1, static_cast<int>(ceilf(width / s_tileWorldSize)));
		s_numRows = std::max(1, static_cast<int>(ceilf(depth / s_tileWorldSize)));
		s_totalTiles = s_numCols * s_numRows;
		s_tileSide = s_numCols;
		s_originTileX = s_numCols;
		s_originTileZ = s_numRows;
	}
	else
	{
		Object *const player = getCapturePlayer(groundScene);
		Vector const p = player ? player->getPosition_w() : Vector::zero;
		s_originTileX = static_cast<int>(floorf(p.x / s_tileWorldSize));
		s_originTileZ = static_cast<int>(floorf(p.z / s_tileWorldSize));
		s_tileSide = std::max(1, 2 * s_gridRadius + 1);
		s_numCols = s_tileSide;
		s_numRows = s_tileSide;
		s_totalTiles = s_tileSide * s_tileSide;
	}

	s_tileLinearIndex = 0;
	s_settleRemaining = s_settleFrames;
	s_pendingShot = false;

	s_lastNumCols = s_numCols;
	s_lastNumRows = s_numRows;
	s_lastTotalTiles = s_totalTiles;
	IGNORE_RETURN(strncpy(s_lastOutputDir, s_outputDir, sizeof(s_lastOutputDir)));
	s_lastOutputDir[sizeof(s_lastOutputDir) - 1] = '\0';

	if (s_externalRequestActive)
	{
		s_savedLodThreshold = TerrainObject::getLevelOfDetailThreshold();
		s_savedHighLodThreshold = TerrainObject::getHighLevelOfDetailThreshold();
		TerrainObject::setLevelOfDetailThreshold(8192.f);
		TerrainObject::setHighLevelOfDetailThreshold(8192.f);
		s_savedTerrainLod = true;
	}

	if (s_suppressFog)
	{
		s_savedFog = GroundEnvironment::getInstance().getEnableFog();
		GroundEnvironment::getInstance().setEnableFog(false);
	}

	if (s_suppressParticles)
	{
		s_savedParticles = ParticleManager::isParticlesEnabled();
		ParticleManager::setParticlesEnabled(false);
	}

	if (s_suppressWeather)
	{
		s_savedEnvPaused = GroundEnvironment::getInstance().getPaused();
		WeatherManager::setWindScale(0.f);
		WeatherManager::setNormalizedWindVelocity_w(Vector(1.f, 0.f, 0.f));
		GroundEnvironment::getInstance().setPaused(true);
	}

	ObjectList::setObjectRenderSkipPredicate(&objectRenderSkipPredicate);

	s_batchActive = true;
	s_waitingForStart = false;

	IGNORE_RETURN(Os::createDirectories(s_outputDir));
	if (CuiManager::getInstalled())
		CuiManager::setRenderSuppressed(true);

	REPORT_LOG(true, ("SwgCameraMapCapture: batch started: %d tile(s) (%s), output directory \"%s\"\n",
		s_totalTiles,
		s_useFixedWorldBounds ? "fixed world zigzag" : "player-centered grid",
		s_outputDir));
}

// ----------------------------------------------------------------------

void SwgCameraMapCapture::endBatch(GroundScene *groundScene)
{
	GroundScene *gs = groundScene;
	if (!gs)
		gs = dynamic_cast<GroundScene *>(Game::getScene());

	if (!s_batchActive)
	{
		if (s_externalRequestActive)
			resetExternalCaptureRequest();
		return;
	}

	if (s_externalRequestActive)
	{
		endExternalCaptureEnvironment();
		s_externalRequestActive = false;
		restoreRequestSettings();
	}

	if (s_savedTerrainLod)
	{
		TerrainObject::setLevelOfDetailThreshold(s_savedLodThreshold);
		TerrainObject::setHighLevelOfDetailThreshold(s_savedHighLodThreshold);
		s_savedTerrainLod = false;
	}

	if (gs)
	{
		FreeCamera *const freeCam = safe_cast<FreeCamera *>(gs->getCamera(GroundScene::CI_free));
		if (freeCam)
		{
			GameCamera *const gc = freeCam;
			if (s_appliedProjection)
			{
				gc->setPerspectiveProjection();
				gc->setNearPlane(s_savedNear);
				gc->setFarPlane(s_savedFar);
				gc->setHorizontalFieldOfView(s_savedHorizontalFov);
				s_appliedProjection = false;
			}
		}
	}

	restoreEnvironmentOnly();

	if (CuiManager::getInstalled())
		CuiManager::setRenderSuppressed(false);

	if (gs && s_savedView >= 0 && s_savedView < GroundScene::CI_COUNT)
		gs->setView(s_savedView);
}

// ----------------------------------------------------------------------

void SwgCameraMapCapture::update(float elapsedTime, GroundScene *groundScene)
{
	UNREF(elapsedTime);

	if (!s_installed || !groundScene)
		return;

	if (!s_externalRequestActive)
		loadConfig();

	if (!isCaptureEnabled())
	{
		if (s_batchActive)
			endBatch(groundScene);
		return;
	}

	if (groundScene->isLoading())
		return;

	if (!s_batchActive)
	{
		if (!s_waitingForStart)
			s_waitingForStart = true;

		if (s_waitingForStart && isCapturePlayerReady(groundScene))
			beginBatch(groundScene);

		return;
	}

	if (s_settleRemaining > 0)
	{
		--s_settleRemaining;
		if (s_settleRemaining == 0)
			s_pendingShot = true;
	}
}

// ----------------------------------------------------------------------

void SwgCameraMapCapture::prepareDraw(GroundScene *groundScene)
{
	if (!s_batchActive || !groundScene || groundScene->isLoading())
		return;

	FreeCamera *const freeCam = safe_cast<FreeCamera *>(groundScene->getCamera(GroundScene::CI_free));
	if (!freeCam)
		return;

	if (groundScene->getCurrentView() != GroundScene::CI_free)
		groundScene->setView(GroundScene::CI_free);

	float centerX = 0.f;
	float centerZ = 0.f;
	getTileCenterXZ(s_tileLinearIndex, centerX, centerZ);

	repositionPlayerForTileCapture();

	freeCam->setMode(FreeCamera::M_fly);
	freeCam->setInterpolating(false);
	freeCam->setPivotPoint(Vector(centerX, s_cameraHeight, centerZ));
	freeCam->setYaw(0.f);
	// Slightly below PI/2 avoids edge cases with FreeCamera pitch clamp vs. straight-down ortho.
	freeCam->setPitch(PI_OVER_2 - 0.02f);

	GameCamera *const gameCamera = freeCam;
	s_savedNear = gameCamera->getNearPlane();
	s_savedFar = gameCamera->getFarPlane();
	s_savedHorizontalFov = gameCamera->getHorizontalFieldOfView();

	float const vw = static_cast<float>(std::max(1, Graphics::getCurrentRenderTargetWidth()));
	float const vh = static_cast<float>(std::max(1, Graphics::getCurrentRenderTargetHeight()));

	float const halfW = s_tileWorldSize * 0.5f;
	float const halfD = (s_tileWorldSize * (vh / vw)) * 0.5f;

	gameCamera->setNearPlane(0.25f);
	gameCamera->setFarPlane(std::max(2048.f, s_cameraHeight + 8192.f));
	gameCamera->setParallelProjection(-halfW, halfD, halfW, -halfD);
	s_appliedProjection = true;
}

// ----------------------------------------------------------------------

void SwgCameraMapCapture::finishDraw(GroundScene *groundScene)
{
	if (!s_batchActive || !groundScene)
		return;

	FreeCamera *const freeCam = safe_cast<FreeCamera *>(groundScene->getCamera(GroundScene::CI_free));
	if (freeCam)
	{
		GameCamera *const gameCamera = freeCam;
		if (s_appliedProjection)
		{
			gameCamera->setPerspectiveProjection();
			gameCamera->setNearPlane(s_savedNear);
			gameCamera->setFarPlane(s_savedFar);
			gameCamera->setHorizontalFieldOfView(s_savedHorizontalFov);
			s_appliedProjection = false;
		}
	}

	if (!s_pendingShot)
		return;

	char path[Os::MAX_PATH_LENGTH];
	if (s_useFixedWorldBounds)
	{
		int const row = s_tileLinearIndex / s_numCols;
		int const inner = s_tileLinearIndex % s_numCols;
		int const col = (row % 2 == 0) ? inner : (s_numCols - 1 - inner);
		sprintf(path, "%s/tile_%05d_r%03d_c%03d", s_outputDir, s_tileLinearIndex, row, col);
	}
	else
	{
		int const side = s_tileSide;
		int const tx = s_tileLinearIndex % side - s_gridRadius;
		int const tz = s_tileLinearIndex / side - s_gridRadius;
		sprintf(path, "%s/tile_%d_%d_%d_%d", s_outputDir, s_originTileX, s_originTileZ, tx, tz);
	}
	if (!Graphics::screenShot(path))
		REPORT_LOG(true, ("SwgCameraMapCapture: Graphics::screenShot failed for base path \"%s\" (engine adds .jpg/.tga; check windowed/desktop and cwd)\n", path));

	s_pendingShot = false;
	advanceTileIndex();
}

// ----------------------------------------------------------------------

bool SwgCameraMapCapture::isBatchActive()
{
	return SwgCameraMapCaptureNamespace::s_batchActive;
}

// ----------------------------------------------------------------------

bool SwgCameraMapCapture::isExternalCapturePending()
{
	return SwgCameraMapCaptureNamespace::s_externalRequestPending
		|| SwgCameraMapCaptureNamespace::s_externalRequestActive;
}

// ----------------------------------------------------------------------

SwgCameraMapCapture::MapCaptureRequest::MapCaptureRequest()
: worldMinX(-8000.f),
  worldMaxX(8000.f),
  worldMinZ(-8000.f),
  worldMaxZ(8000.f),
  tileWorldSize(512.f),
  cameraHeight(700.f),
  settleFrames(2),
  viewportWidth(512),
  viewportHeight(512),
  filterCreatures(true),
  filterLairs(true),
  filterNpcLike(true),
  suppressFog(true),
  suppressParticles(true),
  suppressWeather(true),
  useFixedWorldBounds(true)
{
	outputDir[0] = '\0';
}

// ----------------------------------------------------------------------

void SwgCameraMapCapture::requestBatch(MapCaptureRequest const &request)
{
	if (s_batchActive)
		return;

	if (s_externalRequestPending || s_externalRequestActive)
		resetExternalCaptureRequest();

	s_pendingRequest = request;
	s_externalRequestPending = true;
	s_batchCompleted = false;
	s_waitingForStart = false;

	s_savedForceRenderEntireMap = ClientProceduralTerrainAppearance::getForceRenderEntireMapEnabled();
	ClientProceduralTerrainAppearance::setForceRenderEntireMapEnabled(true);
	Graphics::setForceScreenShotBackBuffer(true, true);
}

// ----------------------------------------------------------------------

void SwgCameraMapCapture::cancelBatch()
{
	if (s_externalRequestPending || (s_externalRequestActive && !s_batchActive))
	{
		resetExternalCaptureRequest();
		return;
	}

	GroundScene *const gs = dynamic_cast<GroundScene *>(Game::getScene());
	endBatch(gs);
}

// ----------------------------------------------------------------------

void SwgCameraMapCapture::getBatchProgress(int &currentTile, int &totalTiles)
{
	if (!s_batchActive)
	{
		currentTile = 0;
		totalTiles = 0;
		return;
	}

	currentTile = s_tileLinearIndex;
	totalTiles = s_totalTiles;
}

// ----------------------------------------------------------------------

bool SwgCameraMapCapture::consumeBatchCompleted()
{
	if (!s_batchCompleted)
		return false;

	s_batchCompleted = false;
	return true;
}

// ----------------------------------------------------------------------

void SwgCameraMapCapture::getBatchGrid(int &numCols, int &numRows, int &totalTiles)
{
	numCols = s_lastNumCols;
	numRows = s_lastNumRows;
	totalTiles = s_lastTotalTiles;
}

// ----------------------------------------------------------------------

void SwgCameraMapCapture::getTileFileBasePath(int linearIndex, char *path, size_t pathSize)
{
	if (!path || pathSize == 0)
		return;

	path[0] = '\0';

	if (linearIndex < 0 || linearIndex >= s_lastTotalTiles || s_lastOutputDir[0] == '\0')
		return;

	if (s_lastNumCols > 0)
	{
		int const row = linearIndex / s_lastNumCols;
		int const inner = linearIndex % s_lastNumCols;
		int const col = (row % 2 == 0) ? inner : (s_lastNumCols - 1 - inner);
		sprintf(path, "%s/tile_%05d_r%03d_c%03d", s_lastOutputDir, linearIndex, row, col);
	}
	else
	{
		sprintf(path, "%s/tile_%05d", s_lastOutputDir, linearIndex);
	}

	if (strlen(path) >= pathSize)
		path[pathSize - 1] = '\0';
}
