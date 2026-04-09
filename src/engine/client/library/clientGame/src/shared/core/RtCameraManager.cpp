// ============================================================================
//
// RtCameraManager.cpp
// Copyright SWG: Titan
//
// Real-Time Camera System Manager Implementation
// Implements full scene rendering with recursion protection
//
// ============================================================================

#include "clientGame/FirstClientGame.h"
#include "clientGame/RtCameraManager.h"

#include "clientGame/ClientWorld.h"
#include "clientGame/Game.h"
#include "clientGraphics/Camera.h"
#include "clientGraphics/Graphics.h"
#include "clientGraphics/RenderWorld.h"
#include "clientGraphics/RenderWorldCamera.h"
#include "clientGraphics/ShaderPrimitiveSorter.h"
#include "clientGraphics/Texture.h"
#include "clientGraphics/TextureList.h"
#include "clientObject/GameCamera.h"
#include "clientTerrain/ClientProceduralTerrainAppearance.h"
#include "clientTerrain/GroundEnvironment.h"
#include "sharedTerrain/FloraManager.h"
#include "sharedDebug/DebugFlags.h"
#include "sharedDebug/InstallTimer.h"
#include "sharedFoundation/ExitChain.h"
#include "sharedFoundation/NetworkId.h"
#include "sharedMath/Transform.h"
#include "sharedObject/Appearance.h"
#include "sharedObject/CellProperty.h"
#include "sharedObject/NetworkIdManager.h"
#include "sharedObject/Object.h"

#include <map>
#include <vector>
#include <cmath>
#include <cstring>
#include <cfloat>

// ============================================================================

namespace RtCameraManagerNamespace
{
	typedef std::map<NetworkId, RtCameraManager::RtCameraFeed> FeedMap;
	typedef std::map<NetworkId, NetworkId> ScreenToCameraMap;
	typedef std::vector<Object const*> ExcludedObjectList;

	// Store feed info for recreation after device restore
	struct FeedRestoreInfo
	{
		NetworkId cameraId;
		NetworkId screenId;
		int resolution;
		float fov;
		bool isActive;
	};

	FeedMap*            s_feeds = nullptr;
	ScreenToCameraMap*  s_screenToCamera = nullptr;
	std::vector<FeedRestoreInfo>* s_feedRestoreInfo = nullptr;
	bool                s_installed = false;
	bool                s_systemAvailable = true;
	bool                s_isRenderingRtFeed = false;  // Recursion guard

	// Render camera for RT feeds - uses RenderWorldCamera for proper scene rendering
	RenderWorldCamera*  s_rtCamera = nullptr;

	// List of RT screen objects to exclude from RT camera renders (recursion protection)
	ExcludedObjectList  s_rtScreenObjects;

	// Render target texture format
	TextureFormat const s_renderTargetFormats[] = { TF_ARGB_8888 };
	int const           s_renderTargetFormatCount = 1;

	// Debug flags
	bool s_debugRtCamera = false;

	// Helper to collect all RT screen objects for exclusion
	void updateRtScreenExclusionList();

	// Remove RT texture override from a screen appearance.
	void clearScreenTextureOverride(NetworkId const & screenId);
}

using namespace RtCameraManagerNamespace;

// ============================================================================

float const RtCameraManager::DEFAULT_UPDATE_RATE = 0.05f;  // 50ms = 20 FPS
float const RtCameraManager::MIN_UPDATE_RATE = 0.016f;     // ~60 FPS max

// ============================================================================

void RtCameraManager::install()
{
	InstallTimer const installTimer("RtCameraManager::install");

	DEBUG_FATAL(s_installed, ("RtCameraManager::install() - Already installed."));

	s_installed = true;
	s_feeds = new FeedMap();
	s_screenToCamera = new ScreenToCameraMap();

	// Create the render world camera for RT feeds
	s_rtCamera = new RenderWorldCamera();

	// Add the camera to the world (required for proper rendering)
	s_rtCamera->addToWorld();

	// Register for device lost/restored callbacks
	Graphics::addDeviceLostCallback(RtCameraManager::lostDevice);
	Graphics::addDeviceRestoredCallback(RtCameraManager::restoreDevice);

	DebugFlags::registerFlag(s_debugRtCamera, "ClientGame", "debugRtCamera");

	ExitChain::add(RtCameraManager::remove, "RtCameraManager::remove", 0, false);

	DEBUG_REPORT_LOG(true, ("RtCameraManager: installed with full scene rendering\n"));
}

// -----------------------------------------------------------------------------

void RtCameraManager::remove()
{
	DEBUG_FATAL(!s_installed, ("RtCameraManager::remove() - Not installed."));

	// Unregister device lost/restored callbacks
	Graphics::removeDeviceLostCallback(RtCameraManager::lostDevice);
	Graphics::removeDeviceRestoredCallback(RtCameraManager::restoreDevice);

	// Clean up all feeds
	if (s_feeds)
	{
		for (FeedMap::iterator it = s_feeds->begin(); it != s_feeds->end(); ++it)
		{
			if (it->second.renderTexture)
			{
				releaseRenderTexture(it->second.renderTexture);
			}
		}
		delete s_feeds;
		s_feeds = nullptr;
	}

	if (s_screenToCamera)
	{
		delete s_screenToCamera;
		s_screenToCamera = nullptr;
	}

	// Clean up render camera
	if (s_rtCamera)
	{
		if (s_rtCamera->isInWorld())
		{
			s_rtCamera->removeFromWorld();
		}
		delete s_rtCamera;
		s_rtCamera = nullptr;
	}

	// Clean up restore info
	if (s_feedRestoreInfo)
	{
		delete s_feedRestoreInfo;
		s_feedRestoreInfo = nullptr;
	}

	s_rtScreenObjects.clear();
	s_installed = false;
}

// -----------------------------------------------------------------------------

void RtCameraManager::alter(float deltaTime)
{
	if (!s_installed || !s_feeds)
		return;

	// Check for and cleanup any stale feeds (objects that no longer exist)
	std::vector<NetworkId> staleFeeds;
	for (FeedMap::const_iterator it = s_feeds->begin(); it != s_feeds->end(); ++it)
	{
		Object* cameraObject = NetworkIdManager::getObjectById(it->second.cameraObjectId);
		Object* screenObject = NetworkIdManager::getObjectById(it->second.screenObjectId);

		if (!cameraObject || !screenObject)
		{
			staleFeeds.push_back(it->first);
		}
	}

	// Remove stale feeds
	for (std::vector<NetworkId>::const_iterator it = staleFeeds.begin(); it != staleFeeds.end(); ++it)
	{
		DEBUG_REPORT_LOG(s_debugRtCamera, ("RtCameraManager: Cleaning up stale feed for camera=%s\n", it->getValueString().c_str()));
		unregisterFeed(*it);
	}

	// Update timers for all active feeds
	for (FeedMap::iterator it = s_feeds->begin(); it != s_feeds->end(); ++it)
	{
		RtCameraFeed& feed = it->second;
		if (feed.isActive)
		{
			feed.updateTimer -= deltaTime;
			if (feed.updateTimer <= 0.0f)
			{
				feed.needsUpdate = true;
				feed.updateTimer = feed.updateRate;
			}
		}
	}
}

// -----------------------------------------------------------------------------

bool RtCameraManager::registerFeed(NetworkId const& cameraId, NetworkId const& screenId, int resolution, float fov)
{
	if (!s_installed || !s_feeds || !s_screenToCamera || !s_systemAvailable)
		return false;

	// Check max cameras
	if (static_cast<int>(s_feeds->size()) >= MAX_ACTIVE_CAMERAS)
	{
		DEBUG_REPORT_LOG(s_debugRtCamera, ("RtCameraManager: Max cameras reached (%d)\n", MAX_ACTIVE_CAMERAS));
		return false;
	}

	// Check if camera already registered
	if (s_feeds->find(cameraId) != s_feeds->end())
	{
		DEBUG_REPORT_LOG(s_debugRtCamera, ("RtCameraManager: Camera already registered\n"));
		return false;
	}

	// Check if screen already registered (prevent duplicate textures)
	if (s_screenToCamera->find(screenId) != s_screenToCamera->end())
	{
		DEBUG_REPORT_LOG(s_debugRtCamera, ("RtCameraManager: Screen already registered\n"));
		return false;
	}

	// Clamp resolution to safe values
	if (resolution < 128)
		resolution = 128;
	if (resolution > 512)
		resolution = 512;

	// Create render texture
	Texture* renderTexture = createRenderTexture(resolution);
	if (!renderTexture)
	{
		DEBUG_REPORT_LOG(s_debugRtCamera, ("RtCameraManager: Failed to create render texture\n"));
		return false;
	}

	// Create feed
	RtCameraFeed feed;
	feed.cameraObjectId = cameraId;
	feed.screenObjectId = screenId;
	feed.fov = fov;
	feed.resolution = resolution;
	feed.updateTimer = 0.0f;
	feed.updateRate = DEFAULT_UPDATE_RATE;
	feed.renderTexture = renderTexture;
	feed.isActive = true;
	feed.needsUpdate = true;

	// Get initial camera transform from camera object
	Object* cameraObject = NetworkIdManager::getObjectById(cameraId);
	if (cameraObject)
	{
		feed.cameraTransform = cameraObject->getTransform_o2w();
	}

	s_feeds->insert(std::make_pair(cameraId, feed));
	s_screenToCamera->insert(std::make_pair(screenId, cameraId));

	// Update exclusion list for recursion protection
	updateRtScreenExclusionList();

	DEBUG_REPORT_LOG(s_debugRtCamera, ("RtCameraManager: Registered feed camera=%s screen=%s res=%d fov=%.1f\n",
		cameraId.getValueString().c_str(), screenId.getValueString().c_str(), resolution, fov));

	return true;
}

// -----------------------------------------------------------------------------

bool RtCameraManager::unregisterFeed(NetworkId const& cameraId)
{
	if (!s_installed || !s_feeds || !s_screenToCamera)
		return false;

	FeedMap::iterator it = s_feeds->find(cameraId);
	if (it == s_feeds->end())
		return false;

	// Release any screen-side reference to this feed texture before destroying it.
	clearScreenTextureOverride(it->second.screenObjectId);

	// Release render texture
	if (it->second.renderTexture)
	{
		releaseRenderTexture(it->second.renderTexture);
	}

	// Remove from screen map
	s_screenToCamera->erase(it->second.screenObjectId);

	// Remove feed
	s_feeds->erase(it);

	// Update exclusion list
	updateRtScreenExclusionList();

	DEBUG_REPORT_LOG(s_debugRtCamera, ("RtCameraManager: Unregistered feed camera=%s\n", cameraId.getValueString().c_str()));

	return true;
}

// -----------------------------------------------------------------------------

bool RtCameraManager::unregisterFeedByScreen(NetworkId const& screenId)
{
	if (!s_installed || !s_screenToCamera)
		return false;

	ScreenToCameraMap::iterator it = s_screenToCamera->find(screenId);
	if (it == s_screenToCamera->end())
		return false;

	return unregisterFeed(it->second);
}

// -----------------------------------------------------------------------------

bool RtCameraManager::updateCameraTransform(NetworkId const& cameraId, Transform const& transform)
{
	if (!s_installed || !s_feeds)
		return false;

	FeedMap::iterator it = s_feeds->find(cameraId);
	if (it == s_feeds->end())
		return false;

	it->second.cameraTransform = transform;
	it->second.needsUpdate = true;

	return true;
}

// -----------------------------------------------------------------------------

bool RtCameraManager::updateCameraFov(NetworkId const& cameraId, float fov)
{
	if (!s_installed || !s_feeds)
		return false;

	FeedMap::iterator it = s_feeds->find(cameraId);
	if (it == s_feeds->end())
		return false;

	it->second.fov = fov;
	it->second.needsUpdate = true;

	return true;
}

// -----------------------------------------------------------------------------

RtCameraManager::RtCameraFeed const* RtCameraManager::getFeed(NetworkId const& cameraId)
{
	if (!s_installed || !s_feeds)
		return nullptr;

	FeedMap::const_iterator it = s_feeds->find(cameraId);
	if (it == s_feeds->end())
		return nullptr;

	return &it->second;
}

// -----------------------------------------------------------------------------

RtCameraManager::RtCameraFeed const* RtCameraManager::getFeedByScreen(NetworkId const& screenId)
{
	if (!s_installed || !s_screenToCamera || !s_feeds)
		return nullptr;

	ScreenToCameraMap::const_iterator screenIt = s_screenToCamera->find(screenId);
	if (screenIt == s_screenToCamera->end())
		return nullptr;

	return getFeed(screenIt->second);
}

// -----------------------------------------------------------------------------

int RtCameraManager::getActiveFeedCount()
{
	if (!s_installed || !s_feeds)
		return 0;

	return static_cast<int>(s_feeds->size());
}

// -----------------------------------------------------------------------------

Texture const* RtCameraManager::getScreenTexture(NetworkId const& screenId)
{
	RtCameraFeed const* feed = getFeedByScreen(screenId);
	if (!feed)
		return nullptr;

	return feed->renderTexture;
}

// -----------------------------------------------------------------------------

bool RtCameraManager::isSystemAvailable()
{
	return s_installed && s_systemAvailable;
}

// -----------------------------------------------------------------------------

void RtCameraManager::renderFeeds()
{
	if (!s_installed || !s_feeds || !s_rtCamera)
		return;

	// Don't render if device is lost
	if (!s_systemAvailable)
		return;

	// Prevent recursive rendering
	if (s_isRenderingRtFeed)
		return;

	// Find the closest visible screen to render (only render ONE at a time to conserve GPU memory)
	RtCameraFeed* closestFeed = nullptr;
	float closestDistanceSquared = FLT_MAX;

	Camera const* playerCamera = Game::getCamera();
	Vector cameraPos;
	if (playerCamera)
	{
		cameraPos = playerCamera->getPosition_w();
	}

	for (FeedMap::iterator it = s_feeds->begin(); it != s_feeds->end(); ++it)
	{
		RtCameraFeed& feed = it->second;

		if (!feed.isActive || !feed.needsUpdate)
			continue;

		// Skip if no render texture (may have been released due to device lost)
		if (!feed.renderTexture)
			continue;

		// Get screen object position
		Object* screenObject = NetworkIdManager::getObjectById(feed.screenObjectId);
		if (!screenObject)
			continue;

		// Calculate distance to screen
		Vector const screenPos = screenObject->getPosition_w();
		float const distanceSquared = screenPos.magnitudeBetweenSquared(cameraPos);

		// Only consider screens within view distance (100m)
		float const maxViewDistanceSquared = 100.0f * 100.0f;
		if (distanceSquared > maxViewDistanceSquared)
			continue;

		// Track closest screen
		if (distanceSquared < closestDistanceSquared)
		{
			closestDistanceSquared = distanceSquared;
			closestFeed = &feed;
		}
	}

	// Only render the closest visible screen
	if (closestFeed)
	{
		renderSingleFeed(*closestFeed);
		closestFeed->needsUpdate = false;
	}
}

// -----------------------------------------------------------------------------

void RtCameraManager::renderSingleFeed(RtCameraFeed& feed)
{
	if (!feed.renderTexture || !s_rtCamera)
		return;

	// Set recursion guard
	s_isRenderingRtFeed = true;

	// Get the camera object to update transform
	Object* cameraObject = NetworkIdManager::getObjectById(feed.cameraObjectId);
	if (cameraObject)
	{
		feed.cameraTransform = cameraObject->getTransform_o2w();
	}

	// Skip if camera object doesn't exist
	if (!cameraObject)
	{
		DEBUG_REPORT_LOG(s_debugRtCamera, ("RtCameraManager: Camera object not found\n"));
		s_isRenderingRtFeed = false;
		return;
	}

	// Set the RT camera as active (required for renderScene to work)
	s_rtCamera->setActive(true);

	// Put the render camera in the same parent cell as the RT camera object.
	// For interior cells, transforms must be in object-to-parent(cell) space (player-style behavior).
	CellProperty* cameraCell = cameraObject->getParentCell();
	if (!cameraCell)
		cameraCell = CellProperty::getWorldCellProperty();
	s_rtCamera->setParentCell(cameraCell);

	Transform const cameraTransform = (cameraCell == CellProperty::getWorldCellProperty())
		? cameraObject->getTransform_o2w()
		: cameraObject->getTransform_o2p();
	s_rtCamera->setTransform_o2p(cameraTransform);

	// Set camera parameters
	float const aspectRatio = 1.0f;  // Square texture
	float const nearPlane = 0.5f;
	float const farPlane = 16000.0f;
	float const fovRadians = feed.fov * 3.14159265f / 180.0f;

	s_rtCamera->setNearPlane(nearPlane);
	s_rtCamera->setFarPlane(farPlane);
	s_rtCamera->setHorizontalFieldOfView(fovRadians * aspectRatio);
	s_rtCamera->setViewport(0, 0, feed.resolution, feed.resolution);

	// Clear excluded objects and add all RT screens for recursion protection
	s_rtCamera->clearExcludedObjects();

	// Exclude all RT screen objects to prevent infinite recursion
	for (ExcludedObjectList::const_iterator it = s_rtScreenObjects.begin();
		 it != s_rtScreenObjects.end(); ++it)
	{
		if (*it)
		{
			s_rtCamera->addExcludedObject(*it);
		}
	}

	// Also exclude the camera object itself (don't render the camera from itself)
	s_rtCamera->addExcludedObject(cameraObject);

	// Set the RT camera as the reference for terrain rendering
	// The terrain system needs both camera and object references
	ClientProceduralTerrainAppearance::setReferenceCamera(s_rtCamera);
	GroundEnvironment::getInstance().setReferenceCamera(s_rtCamera);
	GroundEnvironment::getInstance().setReferenceObject(cameraObject);
	FloraManager::setReferenceObject(cameraObject);

	// Set render target to our texture
	Graphics::setRenderTarget(feed.renderTexture, CF_none, 0);

	// Clear the render target with a dark blue (sky-like) color
	Graphics::clearViewport(true, 0xFF1a1a2e, true, 1.0f, true, 0);

	// Set viewport to match texture size
	Graphics::setViewport(0, 0, feed.resolution, feed.resolution);

	// Begin scene rendering
	Graphics::beginScene();

	// Add terrain render hooks (required for terrain to render)
	ClientWorld::addRenderHookFunctions(s_rtCamera);

	// Use RenderWorldCamera to properly render the scene
	// This handles DPVS visibility, lighting, and all proper render passes
	s_rtCamera->renderScene();

	// Remove terrain render hooks
	ClientWorld::removeRenderHookFunctions();

	Graphics::endScene();

	// Restore back buffer as render target
	Graphics::setRenderTarget(nullptr, CF_none, 0);

	// Clear recursion guard
	s_isRenderingRtFeed = false;

	DEBUG_REPORT_LOG(s_debugRtCamera, ("RtCameraManager: Rendered full scene for camera %s\n",
		feed.cameraObjectId.getValueString().c_str()));
}

// -----------------------------------------------------------------------------

Texture* RtCameraManager::createRenderTexture(int resolution)
{
	// Verify system is available
	if (!s_systemAvailable)
	{
		DEBUG_REPORT_LOG(s_debugRtCamera, ("RtCameraManager: Cannot create texture - system unavailable\n"));
		return nullptr;
	}

	// Clamp resolution to safe values
	if (resolution < 128)
		resolution = 128;
	if (resolution > 512)
		resolution = 512;

	// Create a render target texture
	int const textureCreationFlags = TCF_renderTarget;

	Texture* texture = TextureList::fetch(
		textureCreationFlags,
		resolution,
		resolution,
		1,  // mipmap levels
		s_renderTargetFormats,
		s_renderTargetFormatCount
	);

	if (!texture)
	{
		DEBUG_REPORT_LOG(true, ("RtCameraManager: Failed to create %dx%d render texture - out of memory\n", resolution, resolution));
	}

	return texture;
}

// -----------------------------------------------------------------------------

void RtCameraManager::releaseRenderTexture(Texture* texture)
{
	if (texture)
	{
		texture->release();
	}
}

// -----------------------------------------------------------------------------

bool RtCameraManager::isScreenVisible(NetworkId const& screenId)
{
	Object* screenObject = NetworkIdManager::getObjectById(screenId);
	if (!screenObject)
		return false;

	// Get the player's camera to check visibility
	Camera const* playerCamera = Game::getCamera();
	if (!playerCamera)
		return true;  // If no camera, assume visible

	// Check if the screen object is within reasonable view distance
	Vector const screenPos = screenObject->getPosition_w();
	Vector const cameraPos = playerCamera->getPosition_w();
	float const distanceSquared = screenPos.magnitudeBetweenSquared(cameraPos);

	// Only render if screen is within 100 meters
	float const maxViewDistanceSquared = 100.0f * 100.0f;

	return distanceSquared <= maxViewDistanceSquared;
}

// -----------------------------------------------------------------------------

bool RtCameraManager::isRtScreenObject(Object const* object)
{
	if (!object)
		return false;

	// Check object template name for rt_screen
	// This is used for recursion protection
	char const* templateName = object->getObjectTemplateName();
	if (templateName)
	{
		return strstr(templateName, "rt_screen") != nullptr;
	}

	return false;
}

// -----------------------------------------------------------------------------

void RtCameraManager::handleRtCameraFeedMessage(NetworkId const& cameraId, NetworkId const& screenId,
	int resolution, float fov, float camX, float camY, float camZ)
{
	DEBUG_REPORT_LOG(s_debugRtCamera, ("RtCameraManager: Received feed message camera=%s screen=%s\n",
		cameraId.getValueString().c_str(), screenId.getValueString().c_str()));

	// Register the feed
	if (!registerFeed(cameraId, screenId, resolution, fov))
	{
		DEBUG_REPORT_LOG(s_debugRtCamera, ("RtCameraManager: Failed to register feed\n"));
		return;
	}

	// Update initial position if provided
	if (camX != 0.0f || camY != 0.0f || camZ != 0.0f)
	{
		Transform t;
		t.setPosition_p(Vector(camX, camY, camZ));
		updateCameraTransform(cameraId, t);
	}
}

// -----------------------------------------------------------------------------

namespace RtCameraManagerNamespace
{
	void clearScreenTextureOverride(NetworkId const & screenId)
	{
		Object * const screenObject = NetworkIdManager::getObjectById(screenId);
		if (!screenObject)
			return;

		Appearance * const appearance = screenObject->getAppearance();
		if (!appearance)
			return;

		Tag const TAG_MAIN = TAG(M,A,I,N);
		Texture const * const defaultTexture = TextureList::fetchDefaultTexture();
		if (!defaultTexture)
			return;

		appearance->setTexture(TAG_MAIN, *defaultTexture);
		defaultTexture->release();
	}

	void updateRtScreenExclusionList()
	{
		s_rtScreenObjects.clear();

		if (!s_screenToCamera)
			return;

		// Add all registered screen objects to exclusion list
		for (ScreenToCameraMap::const_iterator it = s_screenToCamera->begin();
			 it != s_screenToCamera->end(); ++it)
		{
			Object* screenObject = NetworkIdManager::getObjectById(it->first);
			if (screenObject)
			{
				s_rtScreenObjects.push_back(screenObject);
			}
		}

		DEBUG_REPORT_LOG(s_debugRtCamera, ("RtCameraManager: Updated exclusion list with %d RT screens\n",
			static_cast<int>(s_rtScreenObjects.size())));
	}
}

// -----------------------------------------------------------------------------

void RtCameraManager::lostDevice()
{
	if (!s_installed || !s_feeds)
		return;

	// Immediately mark system as unavailable to prevent any rendering
	s_systemAvailable = false;

	// Clear the rendering flag in case we were in the middle of rendering
	s_isRenderingRtFeed = false;

	DEBUG_REPORT_LOG(true, ("RtCameraManager: lostDevice - releasing render targets\n"));

	// Reset render target to primary/backbuffer before releasing our textures
	// This ensures the GPU is not using our render targets when we release them
	Graphics::setRenderTarget(nullptr, CF_none, 0);

	// Save feed info for restoration
	if (!s_feedRestoreInfo)
		s_feedRestoreInfo = new std::vector<FeedRestoreInfo>();
	s_feedRestoreInfo->clear();

	// Release all render textures (required before device reset)
	for (FeedMap::iterator it = s_feeds->begin(); it != s_feeds->end(); ++it)
	{
		RtCameraFeed& feed = it->second;

		// Ensure no appearance/shader still references this render target during Reset().
		clearScreenTextureOverride(feed.screenObjectId);

		// Save info for restore
		FeedRestoreInfo info;
		info.cameraId = feed.cameraObjectId;
		info.screenId = feed.screenObjectId;
		info.resolution = feed.resolution;
		info.fov = feed.fov;
		info.isActive = feed.isActive;
		s_feedRestoreInfo->push_back(info);

		// Release the render texture
		if (feed.renderTexture)
		{
			releaseRenderTexture(feed.renderTexture);
			feed.renderTexture = nullptr;
		}
	}
}

// -----------------------------------------------------------------------------

void RtCameraManager::restoreDevice()
{
	if (!s_installed || !s_feeds)
		return;

	DEBUG_REPORT_LOG(true, ("RtCameraManager: restoreDevice - recreating render targets\n"));

	// Recreate render textures for all feeds and force immediate refresh
	for (FeedMap::iterator it = s_feeds->begin(); it != s_feeds->end(); ++it)
	{
		RtCameraFeed& feed = it->second;

		if (!feed.renderTexture)
		{
			feed.renderTexture = createRenderTexture(feed.resolution);
		}

		// Force immediate refresh on next frame
		feed.needsUpdate = true;
		feed.updateTimer = 0.0f;

		DEBUG_REPORT_LOG(s_debugRtCamera, ("RtCameraManager: Recreated render texture for camera %s\n",
			feed.cameraObjectId.getValueString().c_str()));
	}

	s_systemAvailable = true;
}

// ============================================================================

