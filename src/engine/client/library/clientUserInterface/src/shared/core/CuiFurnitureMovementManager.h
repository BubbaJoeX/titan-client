//======================================================================
//
// CuiFurnitureMovementManager.h
// copyright (c) 2024
//
// Manager for interactive furniture movement mode with gimbal controls
//
//======================================================================

#ifndef INCLUDED_CuiFurnitureMovementManager_H
#define INCLUDED_CuiFurnitureMovementManager_H

//======================================================================

class NetworkId;
class Object;
class Vector;
class Transform;
class Camera;

//----------------------------------------------------------------------

class CuiFurnitureMovementManager
{
public:
	// Gizmo component types for mouse interaction
	enum GizmoComponent
	{
		GC_None = 0,
		GC_AxisX,
		GC_AxisY,
		GC_AxisZ,
		GC_PlaneXY,
		GC_PlaneXZ,
		GC_PlaneYZ,
		GC_RotateYaw,
		GC_RotatePitch,
		GC_RotateRoll,
		GC_Sphere
	};

	// Gizmo modes
	enum GizmoMode
	{
		GM_Translate = 0,
		GM_Rotate,
		GM_Scale
	};

	static void install();
	static void remove();
	static void update(float deltaTimeSecs);

	// Enter/exit movement mode
	static bool enterMovementMode(NetworkId const & furnitureId);
	static void exitMovementMode(bool applyChanges);
	static bool isActive();
	
	// Decorator camera mode (free camera with mouse cursor visible)
	static void enableDecoratorCamera();
	static void disableDecoratorCamera();
	static bool isDecoratorCameraActive();

	void sendSetPositionToServer();

	// Get selected furniture
	static NetworkId const & getSelectedFurniture();
	static Object * getSelectedObject();

	// Mouse/Gizmo interaction methods
	static bool processMouseInput(int x, int y, bool leftButton, bool rightButton);
	static bool processMouseDown(int x, int y, bool leftButton, bool rightButton);
	static bool processMouseUp(int x, int y, bool leftButton, bool rightButton);
	static bool processMouseDrag(int x, int y, int deltaX, int deltaY);
	static void render();

	// Axis movement controls (world-relative)
	static void moveX(float amount);
	static void moveY(float amount);
	static void moveZ(float amount);

	// Gimbal rotation controls (degrees)
	static void rotateYaw(float degrees);
	static void rotatePitch(float degrees);
	static void rotateRoll(float degrees);

	// Settings
	static float getMovementSpeed();
	static void setMovementSpeed(float metersPerSecond);
	static float getRotationSpeed();
	static void setRotationSpeed(float degreesPerSecond);
	static bool isFineMode();
	static void setFineMode(bool fine);

	// Gizmo settings
	static GizmoMode getGizmoMode();
	static void setGizmoMode(GizmoMode mode);
	static void cycleGizmoMode();
	static float getGizmoScale();
	static void setGizmoScale(float scale);

	// Get current accumulated deltas (for UI display)
	static Vector const & getPositionDelta();
	static float getYawDelta();
	static float getPitchDelta();
	static float getRollDelta();

	// Input handling - returns true if input was consumed
	static bool processKeyDown(int keystroke);
	static bool processKeyUp(int keystroke);
	static bool processMouseWheel(float delta);

private:
	CuiFurnitureMovementManager();
	~CuiFurnitureMovementManager();
	CuiFurnitureMovementManager(CuiFurnitureMovementManager const &);
	CuiFurnitureMovementManager & operator=(CuiFurnitureMovementManager const &);

	static void applyMovementToServer();
	static void revertToOriginalPosition();
	static void updateFurnitureTransform();
	static void openSpawnUI();

	// Gizmo rendering helpers
	static void renderTranslationGizmo(Vector const & position, float scale);
	static void renderRotationGizmo(Vector const & position, float scale);
	static void renderAxisLine(Vector const & start, Vector const & end, unsigned int color, bool highlighted);
	static void renderCircle(Vector const & center, Vector const & normal, float radius, unsigned int color, bool highlighted, int segments);

	// Gizmo hit testing
	static GizmoComponent hitTestGizmo(int screenX, int screenY, Camera const * camera, Vector const & gizmoPosition, float gizmoScale);
	static bool hitTestAxis(int screenX, int screenY, Camera const * camera, Vector const & start, Vector const & end, float threshold);
	static bool hitTestCircle(int screenX, int screenY, Camera const * camera, Vector const & center, Vector const & normal, float radius, float threshold);
};

//======================================================================

#endif
