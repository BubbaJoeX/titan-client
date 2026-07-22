// ============================================================================
//
// RtCameraManager.h
// Copyright SWG: Titan
//
// Real-Time Camera System Manager
// Manages RT Camera feeds rendered to RT Screen objects
//
// ============================================================================

#ifndef INCLUDED_RtCameraManager_H
#define INCLUDED_RtCameraManager_H

#include "sharedFoundation/NetworkId.h"
#include "sharedMath/Transform.h"
#include "sharedMath/Vector.h"

class Camera;
class Object;
class Texture;

// ============================================================================

class RtCameraManager
{
public:
	static void install();
	static void remove();
	static void alter(float deltaTime);

	// Feed management
	struct RtCameraFeed
	{
		NetworkId   cameraObjectId;
		NetworkId   screenObjectId;
		Transform   cameraTransform;
		float       fov;
		int         resolution;
		float       updateTimer;
		float       updateRate;
		Texture*    renderTexture;
		bool        isActive;
		bool        needsUpdate;
	};

	static int const MAX_ACTIVE_CAMERAS = 4;
	static int const DEFAULT_RESOLUTION = 256;
	static float const DEFAULT_UPDATE_RATE;
	static float const MIN_UPDATE_RATE;

	// Create a new camera feed
	static bool registerFeed(NetworkId const& cameraId, NetworkId const& screenId, int resolution, float fov);

	// Remove a camera feed
	static bool unregisterFeed(NetworkId const& cameraId);
	static bool unregisterFeedByScreen(NetworkId const& screenId);

	// Update camera transform
	static bool updateCameraTransform(NetworkId const& cameraId, Transform const& transform);
	static bool updateCameraFov(NetworkId const& cameraId, float fov);

	// Get feed info
	static RtCameraFeed const* getFeed(NetworkId const& cameraId);
	static RtCameraFeed const* getFeedByScreen(NetworkId const& screenId);
	static int getActiveFeedCount();

	// Get render texture for a screen
	static Texture const* getScreenTexture(NetworkId const& screenId);

	// Check if system is available
	static bool isSystemAvailable();

	// Render all active camera feeds
	static void renderFeeds();

	// Handle device loss/reset (for alt-tab, etc.)
	static void lostDevice();
	static void restoreDevice();

	// Handle server message to start viewing feed
	static void handleRtCameraFeedMessage(NetworkId const& cameraId, NetworkId const& screenId,
		int resolution, float fov, float camX, float camY, float camZ);

private:
	RtCameraManager();
	~RtCameraManager();
	RtCameraManager(RtCameraManager const&);
	RtCameraManager& operator=(RtCameraManager const&);

	static void renderSingleFeed(RtCameraFeed& feed);
	static Texture* createRenderTexture(int resolution);
	static void releaseRenderTexture(Texture* texture);
	static bool isScreenVisible(NetworkId const& screenId);
	static bool isRtScreenObject(Object const* object);
};

// ============================================================================

#endif // INCLUDED_RtCameraManager_H

