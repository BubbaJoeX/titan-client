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

#include <string>

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
		GC_Sphere,
		GC_Mouse4FreeDrag
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
	// Overlay text is now shown in the decorator camera UI panel; this is a no-op.
	static void renderTextOverlay();
	// Position and selection line for the UI panel (empty when no selection). Narrow strings.
	static std::string getSelectionOverlayPositionLine();
	static std::string getSelectionOverlaySelectionLine();
	
	// Object selection (works when decorator camera is active)
	static bool selectObjectAtScreenPosition(int x, int y);

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
	// Local vs world orientation: world = axes aligned to world; local = axes aligned to object
	static bool getGizmoWorldSpace();
	static void setGizmoWorldSpace(bool worldSpace);

	// Snap to grid: horizontal (XZ) and vertical (Y). G cycles grid size (0.5-8m), J toggles horizontal, H toggles vertical.
	static bool getSnapToGridHorizontal();
	static void setSnapToGridHorizontal(bool snap);
	static bool getSnapToGridVertical();
	static void setSnapToGridVertical(bool snap);
	static bool getSnapToGrid();  // true if either horizontal or vertical is on
	static void setSnapToGrid(bool snap);  // sets both horizontal=snap, vertical=false
	static float getSnapGridMeters();
	static void setSnapGridMeters(float meters);
	static void cycleSnapGridMeters();  // 0.5 -> 1 -> 2 -> ... -> 8 -> 0.5
	// Drop selection to terrain; reset selection to face North and zero rotation
	static void dropSelectionToTerrain();
	static void resetSelectionRotation();
	// Send control help lines to the system message area (for Help button)
	static void sendControlsHelp();
	// Focus camera on selection when entering movement mode or selecting an object
	static bool getFocusCameraOnSelection();
	static void setFocusCameraOnSelection(bool focus);
	// Focus camera on current selection (F key)
	static void focusCameraOnSelection();

	// Gimbal (Mouse 1) and Mouse4 free-drag toggles
	static bool getUseGimbalMovement();
	static void setUseGimbalMovement(bool use);
	static bool getUseMouse4Drag();
	static void setUseMouse4Drag(bool use);
	// Drag on terrain: when enabled, dragging snaps object to terrain height (Y key)
	static bool getDragOnTerrain();
	static void setDragOnTerrain(bool on);
	static void setMouse4Down(bool down);
	// Open the decorator spawn UI (item spawner)
	static void openSpawnUI();

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

	// Gizmo rendering helpers
	static void renderTranslationGizmo(Vector const & position, float scale);
	static void renderRotationGizmo(Vector const & position, float scale);
	static void renderAxisLine(Vector const & start, Vector const & end, unsigned int color, bool highlighted);
	static void renderCircle(Vector const & center, Vector const & normal, float radius, unsigned int color, bool highlighted, int segments);

	// Gizmo hit testing (gizmoTransform is the transform used to draw the gizmo; determines axis orientation)
	static GizmoComponent hitTestGizmo(int screenX, int screenY, Camera const * camera, Transform const & gizmoTransform, float gizmoScale);
	static bool hitTestAxis(int screenX, int screenY, Camera const * camera, Vector const & start, Vector const & end, float threshold);
	static bool hitTestCircle(int screenX, int screenY, Camera const * camera, Vector const & center, Vector const & normal, float radius, float threshold);
};

//======================================================================

#endif
