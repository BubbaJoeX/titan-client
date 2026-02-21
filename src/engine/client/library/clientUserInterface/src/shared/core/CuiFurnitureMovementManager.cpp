//======================================================================
//
// CuiFurnitureMovementManager.cpp
// copyright (c) 2024
//
// Manager for interactive furniture movement mode with gimbal controls
//
//======================================================================

#include "clientUserInterface/FirstClientUserInterface.h"
#include "clientUserInterface/CuiFurnitureMovementManager.h"

#include "clientDirectInput/DirectInput.h"
#include "clientGame/ClientCommandQueue.h"
#include "clientGame/ClientObject.h"
#include "clientGame/ClientWorld.h"
#include "clientGame/CreatureObject.h"
#include "clientGame/FreeCamera.h"
#include "clientGame/Game.h"
#include "clientGame/GroundScene.h"
#include "clientGame/PlayerObject.h"
#include "clientGraphics/Camera.h"
#include "clientGraphics/DebugPrimitive.h"
#include "clientGraphics/Graphics.h"
#include "clientGraphics/ShaderTemplateList.h"
#include "clientObject/GameCamera.h"
#include "clientUserInterface/CuiActionManager.h"
#include "clientUserInterface/CuiActions.h"
#include "clientUserInterface/CuiManager.h"
#include "clientUserInterface/CuiPreferences.h"
#include "clientUserInterface/CuiSystemMessageManager.h"
#include "clientUserInterface/CuiTextManager.h"
#include "UITypes.h"
#include "sharedCollision/BoxExtent.h"
#include "sharedCollision/CollideParameters.h"
#include "sharedCollision/CollisionInfo.h"
#include "sharedCollision/Extent.h"
#include "sharedFoundation/Clock.h"
#include "sharedFoundation/Crc.h"
#include "sharedFoundation/FormattedString.h"
#include "sharedMath/PackedArgb.h"
#include "sharedMath/Quaternion.h"
#include "sharedMath/Sphere.h"
#include "sharedMath/Transform.h"
#include "sharedMath/Vector.h"
#include "sharedMath/VectorArgb.h"
#include "sharedObject/Appearance.h"
#include "sharedObject/CachedNetworkId.h"
#include "sharedObject/CellProperty.h"
#include "sharedObject/NetworkIdManager.h"
#include "sharedTerrain/TerrainObject.h"
#include "UnicodeUtils.h"

#include <cmath>
#include <cfloat>
#include <dinput.h>
#include <float.h>

#ifndef PI
#define PI 3.14159265359f
#endif

//======================================================================

namespace CuiFurnitureMovementManagerNamespace
{
	// Type aliases
	typedef CuiFurnitureMovementManager::GizmoComponent GizmoComponent;
	typedef CuiFurnitureMovementManager::GizmoMode GizmoMode;
	GizmoComponent const GC_None = CuiFurnitureMovementManager::GC_None;
	GizmoComponent const GC_AxisX = CuiFurnitureMovementManager::GC_AxisX;
	GizmoComponent const GC_AxisY = CuiFurnitureMovementManager::GC_AxisY;
	GizmoComponent const GC_AxisZ = CuiFurnitureMovementManager::GC_AxisZ;
	GizmoComponent const GC_PlaneXY = CuiFurnitureMovementManager::GC_PlaneXY;
	GizmoComponent const GC_PlaneXZ = CuiFurnitureMovementManager::GC_PlaneXZ;
	GizmoComponent const GC_PlaneYZ = CuiFurnitureMovementManager::GC_PlaneYZ;
	GizmoComponent const GC_RotateYaw = CuiFurnitureMovementManager::GC_RotateYaw;
	GizmoComponent const GC_RotatePitch = CuiFurnitureMovementManager::GC_RotatePitch;
	GizmoComponent const GC_RotateRoll = CuiFurnitureMovementManager::GC_RotateRoll;
	GizmoComponent const GC_Sphere = CuiFurnitureMovementManager::GC_Sphere;
	GizmoMode const GM_Translate = CuiFurnitureMovementManager::GM_Translate;
	GizmoMode const GM_Rotate = CuiFurnitureMovementManager::GM_Rotate;
	GizmoMode const GM_Scale = CuiFurnitureMovementManager::GM_Scale;

	bool s_installed = false;
	bool s_active = false;
	bool s_decoratorCameraActive = false;
	int s_previousCameraMode = -1;
	float s_previousHudOpacity = 1.0f;

	// Camera control keys (WASD)
	bool s_cameraKeyW = false;
	bool s_cameraKeyA = false;
	bool s_cameraKeyS = false;
	bool s_cameraKeyD = false;
	bool s_cameraKeySpace = false;  // Up
	bool s_cameraKeyCtrl = false;   // Down
	float s_cameraSpeed = 10.0f;

	// Hit test caching to reduce lag
	int s_lastHitTestX = -1;
	int s_lastHitTestY = -1;
	CuiFurnitureMovementManager::GizmoComponent s_cachedHitResult = CuiFurnitureMovementManager::GC_None;

	CachedNetworkId s_selectedFurniture;
	Transform s_originalTransform;
	
	Vector s_positionDelta;
	float s_yawDelta = 0.0f;
	float s_pitchDelta = 0.0f;
	float s_rollDelta = 0.0f;

	float s_movementSpeed = 0.5f;
	float s_rotationSpeed = 45.0f;
	bool s_fineMode = false;
	float s_fineModeFactor = 0.1f;

	GizmoMode s_gizmoMode = CuiFurnitureMovementManager::GM_Translate;
	GizmoComponent s_activeComponent = CuiFurnitureMovementManager::GC_None;
	GizmoComponent s_hoveredComponent = CuiFurnitureMovementManager::GC_None;
	float s_gizmoScale = 1.0f;
	float s_cachedObjectRadius = 1.0f;
	bool s_gizmoWorldSpace = true; // true = world axes, false = object (local) axes
	bool s_snapToGridHorizontal = false;  // default off
	bool s_snapToGridVertical = false;
	float s_snapGridMeters = 1.0f;  // 0.5 to 8
	int const s_snapGridOptionCount = 9;  // 0.5, 1, 2, 3, 4, 5, 6, 7, 8
	bool s_focusCameraOnSelection = false;  // default off
	bool s_useGimbalMovement = true;   // Mouse 1 gizmo drag
	bool s_useMouse4Drag = true;      // Mouse4 free drag
	bool s_dragOnTerrain = false;     // Y key toggles; when on, drag snaps to terrain
	bool s_mouse4Down = false;
	bool s_mouse4Dragging = false;
	bool s_tildeHeld = false;  // snap rotation to 45 degrees
	bool s_isDragging = false;
	int s_lastMouseX = 0;
	int s_lastMouseY = 0;
	Vector s_dragStartPosition;

	// Gizmo colors
	PackedArgb const s_colorRed(255, 255, 50, 50);
	PackedArgb const s_colorGreen(255, 50, 255, 50);
	PackedArgb const s_colorBlue(255, 50, 50, 255);
	PackedArgb const s_colorRedHighlight(255, 255, 128, 128);
	PackedArgb const s_colorGreenHighlight(255, 128, 255, 128);
	PackedArgb const s_colorBlueHighlight(255, 128, 128, 255);
	PackedArgb const s_colorWhite(255, 255, 255, 255);
	PackedArgb const s_colorYellow(255, 255, 255, 50);

	uint32 s_hashMoveFurniture = 0;
	uint32 s_hashRotateFurniture = 0;

	float const s_snapGridOptions[] = { 0.5f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f };

	// Round to nearest ten-thousandths to avoid malformed location strings
	inline float roundToTenThousandths(float v)
	{
		return std::floor(v * 10000.0f + 0.5f) / 10000.0f;
	}

	void resetDeltas()
	{
		s_positionDelta = Vector::zero;
		s_yawDelta = 0.0f;
		s_pitchDelta = 0.0f;
		s_rollDelta = 0.0f;
	}

	void resetGizmoState()
	{
		s_activeComponent = CuiFurnitureMovementManager::GC_None;
		s_hoveredComponent = CuiFurnitureMovementManager::GC_None;
		s_isDragging = false;
		s_mouse4Dragging = false;
		s_lastMouseX = 0;
		s_lastMouseY = 0;
	}

	// Returns NetworkId of first pickable object at screen position, or cms_invalid
	NetworkId getObjectAtScreenPosition(int x, int y, Object const * excludeObject)
	{
		Camera const * const camera = Game::getCamera();
		if (!camera) return NetworkId::cms_invalid;
		int const vx = x - camera->getViewportX0();
		int const vy = y - camera->getViewportY0();
		Vector const start = camera->getPosition_w();
		Vector const direction = camera->rotate_o2w(camera->reverseProjectInScreenSpace(vx, vy));
		Vector const end = start + direction * 512.0f;
		CollideParameters collideParams;
		collideParams.setQuality(CollideParameters::Q_high);
		ClientWorld::CollisionInfoVector results;
		CellProperty const * const startCell = camera->getParentCell();
		uint16 const pickFlags = ClientWorld::CF_tangible | ClientWorld::CF_tangibleNotTargetable | ClientWorld::CF_interiorGeometry | ClientWorld::CF_interiorObjects | ClientWorld::CF_childObjects;
		if (!ClientWorld::collide(startCell, start, end, collideParams, results, pickFlags, excludeObject))
			return NetworkId::cms_invalid;
		for (ClientWorld::CollisionInfoVector::const_iterator it = results.begin(); it != results.end(); ++it)
		{
			Object const * const hitObject = it->getObject();
			if (!hitObject) continue;
			ClientObject * const clientObj = const_cast<Object*>(hitObject)->asClientObject();
			if (!clientObj || !clientObj->getNetworkId().isValid()) continue;
			return clientObj->getNetworkId();
		}
		return NetworkId::cms_invalid;
	}

	// Ray from camera through screen (x,y); intersect with plane (point, normal). Returns true and outHit if hit.
	bool rayPlaneIntersection(Camera const * camera, int screenX, int screenY, Vector const & planePoint, Vector const & planeNormal, Vector & outHit)
	{
		Vector const origin = camera->getPosition_w();
		Vector dirCam = camera->reverseProjectInScreenSpace(screenX, screenY);
		Vector dirWorld = camera->rotate_o2w(dirCam);
		float const lenSq = dirWorld.magnitudeSquared();
		if (lenSq < 0.0001f) return false;
		dirWorld.approximateNormalize();
		float const denom = dirWorld.dot(planeNormal);
		if (std::fabs(denom) < 0.0001f) return false;
		Vector const diff = planePoint - origin;
		float const t = diff.dot(planeNormal) / denom;
		if (t < 0.0f) return false;
		outHit = origin + dirWorld * t;
		return true;
	}

	float getObjectRadius(Object const * obj)
	{
		if (!obj) return 1.0f;
		Appearance const * const appearance = obj->getAppearance();
		if (!appearance) return 1.0f;
		
		Extent const * const extent = appearance->getExtent();
		if (extent)
		{
			BoxExtent const * const boxExtent = dynamic_cast<BoxExtent const *>(extent);
			if (boxExtent)
			{
				float const h = boxExtent->getHeight();
				float const w = boxExtent->getWidth();
				float const l = boxExtent->getLength();
				return std::max(h, std::max(w, l)) * 0.5f;
			}
			return extent->getSphere().getRadius();
		}
		
		Sphere const & sphere = appearance->getSphere();
		return std::max(0.5f, sphere.getRadius());
	}
}

using namespace CuiFurnitureMovementManagerNamespace;

//======================================================================

void CuiFurnitureMovementManager::install()
{
	DEBUG_FATAL(s_installed, ("CuiFurnitureMovementManager already installed"));
	s_installed = true;
	s_active = false;
	s_hashMoveFurniture = Crc::normalizeAndCalculate("moveFurniture");
	s_hashRotateFurniture = Crc::normalizeAndCalculate("rotateFurniture");
	resetDeltas();
	resetGizmoState();
}

//----------------------------------------------------------------------

void CuiFurnitureMovementManager::remove()
{
	DEBUG_FATAL(!s_installed, ("CuiFurnitureMovementManager not installed"));
	if (s_active) exitMovementMode(false);
	s_installed = false;
}

//----------------------------------------------------------------------

void CuiFurnitureMovementManager::update(float const deltaTimeSecs)
{
	if (!s_installed) return;
	
	// If decorator camera is active but view was changed (e.g. Ctrl+Shift+F2 to chase cam), clean up
	// so pointer and input state are restored and chatbox is not locked
	if (s_decoratorCameraActive)
	{
		GroundScene * const gs = dynamic_cast<GroundScene *>(Game::getScene());
		if (gs && gs->getCurrentView() != GroundScene::CI_free)
		{
			disableDecoratorCamera();
			// If we had an object selected, exit movement mode without applying
			if (s_active)
				exitMovementMode(false);
		}
	}
	
	if (!s_active && !s_decoratorCameraActive) return;
	
	// When active with an object, validate it
	if (s_active)
	{
		Object * const obj = getSelectedObject();
		if (!obj)
		{
			exitMovementMode(false);
			return;
		}
	}
	
	// Handle WASD camera movement in decorator mode (works with or without object selected)
	if (s_decoratorCameraActive)
	{
		GroundScene * const gs = dynamic_cast<GroundScene *>(Game::getScene());
		if (gs)
		{
			FreeCamera * const freeCamera = dynamic_cast<FreeCamera *>(gs->getCurrentCamera());
			if (freeCamera)
			{
				float const speed = s_fineMode ? s_cameraSpeed * 0.2f : s_cameraSpeed;
				float const moveAmount = speed * deltaTimeSecs;
				
				// Get current camera info
				FreeCamera::Info info = freeCamera->getInfo();
				
				// Calculate movement in camera space
				Vector forward = freeCamera->getObjectFrameK_w();
				Vector right = freeCamera->getObjectFrameI_w();
				Vector movement = Vector::zero;
				
				// Forward/Back (W/S)
				if (s_cameraKeyW) movement += forward * moveAmount;
				if (s_cameraKeyS) movement -= forward * moveAmount;
				
				// Strafe Left/Right (A/D)
				if (s_cameraKeyA) movement -= right * moveAmount;
				if (s_cameraKeyD) movement += right * moveAmount;
				
				// Up/Down (Space/Ctrl)
				if (s_cameraKeySpace) movement.y += moveAmount;
				if (s_cameraKeyCtrl) movement.y -= moveAmount;
				
				if (movement != Vector::zero)
				{
					info.translate += movement;
					freeCamera->setInfo(info);
				}
			}
		}
	}
	
	// Update furniture transform if not dragging (only when we have an object selected)
	if (s_active && !s_isDragging)
	{
		updateFurnitureTransform();
	}
}

//----------------------------------------------------------------------

bool CuiFurnitureMovementManager::enterMovementMode(NetworkId const & furnitureId)
{
	if (!s_installed) return false;
	// When switching to a different object, only clear current selection and revert — do not disable decorator camera (keeps camera where user left it)
	if (s_active)
	{
		revertToOriginalPosition();
		s_selectedFurniture = NetworkId::cms_invalid;
		s_active = false;
		resetDeltas();
		resetGizmoState();
	}
	
	Object * const obj = NetworkIdManager::getObjectById(furnitureId);
	if (!obj)
	{
		CuiSystemMessageManager::sendFakeSystemMessage(Unicode::narrowToWide("Cannot find object."));
		return false;
	}
	
	ClientObject * const clientObj = obj->asClientObject();
	if (!clientObj)
	{
		CuiSystemMessageManager::sendFakeSystemMessage(Unicode::narrowToWide("Invalid object."));
		return false;
	}
	
	s_selectedFurniture = furnitureId;
	s_originalTransform = obj->getTransform_o2p();
	s_cachedObjectRadius = getObjectRadius(obj);
	
	resetDeltas();
	resetGizmoState();
	
	s_active = true;
	
	// Enable decorator camera only when first entering (not when switching objects), so camera is not reset
	if (!s_decoratorCameraActive)
		enableDecoratorCamera();
	
	// Only focus camera and zoom when option is on (otherwise leave camera and gimbal scale untouched)
	if (s_focusCameraOnSelection)
	{
		GroundScene * const gs = dynamic_cast<GroundScene *>(Game::getScene());
		if (gs && gs->getCurrentView() == GroundScene::CI_free)
		{
			FreeCamera * const freeCam = gs->getGodClientCamera();
			if (freeCam)
			{
				Vector const objPos = obj->getPosition_w();
				freeCam->setPivotPoint(objPos);
				FreeCamera::Info info = freeCam->getInfo();
				// Zoom: object origin to extent + 2m
				info.distance = std::max(2.0f, s_cachedObjectRadius + 2.0f);
				freeCam->setInfo(info);
				freeCam->setMode(FreeCamera::M_pivot);
			}
		}
	}
	// When focus is off, do not change camera; gimbal still uses s_cachedObjectRadius for current object
	
	return true;
}

//----------------------------------------------------------------------

void CuiFurnitureMovementManager::exitMovementMode(bool const applyChanges)
{
	if (!s_active) return;
	
	// Disable decorator camera first
	disableDecoratorCamera();
	
	if (applyChanges)
	{
		applyMovementToServer();
		CuiSystemMessageManager::sendFakeSystemMessage(Unicode::narrowToWide("Furniture placement confirmed."));
	}
	else
	{
		revertToOriginalPosition();
		CuiSystemMessageManager::sendFakeSystemMessage(Unicode::narrowToWide("Furniture placement cancelled."));
	}
	
	s_selectedFurniture = NetworkId::cms_invalid;
	s_active = false;
	resetDeltas();
	resetGizmoState();
}

//----------------------------------------------------------------------

bool CuiFurnitureMovementManager::isActive() { return s_active; }
NetworkId const & CuiFurnitureMovementManager::getSelectedFurniture() { return s_selectedFurniture; }

Object * CuiFurnitureMovementManager::getSelectedObject()
{
	if (!s_selectedFurniture.isValid()) return NULL;
	return s_selectedFurniture.getObject();
}

//----------------------------------------------------------------------

void CuiFurnitureMovementManager::moveX(float const amount) { s_positionDelta.x += amount; }
void CuiFurnitureMovementManager::moveY(float const amount) { s_positionDelta.y += amount; }
void CuiFurnitureMovementManager::moveZ(float const amount) { s_positionDelta.z += amount; }
void CuiFurnitureMovementManager::rotateYaw(float const degrees) { s_yawDelta += degrees; }
void CuiFurnitureMovementManager::rotatePitch(float const degrees) { s_pitchDelta += degrees; }
void CuiFurnitureMovementManager::rotateRoll(float const degrees) { s_rollDelta += degrees; }

float CuiFurnitureMovementManager::getMovementSpeed() { return s_movementSpeed; }
void CuiFurnitureMovementManager::setMovementSpeed(float const v) { s_movementSpeed = v; }
float CuiFurnitureMovementManager::getRotationSpeed() { return s_rotationSpeed; }
void CuiFurnitureMovementManager::setRotationSpeed(float const v) { s_rotationSpeed = v; }
bool CuiFurnitureMovementManager::isFineMode() { return s_fineMode; }
void CuiFurnitureMovementManager::setFineMode(bool const v) { s_fineMode = v; }

CuiFurnitureMovementManager::GizmoMode CuiFurnitureMovementManager::getGizmoMode() { return s_gizmoMode; }
void CuiFurnitureMovementManager::setGizmoMode(GizmoMode mode) { s_gizmoMode = mode; }
void CuiFurnitureMovementManager::cycleGizmoMode() 
{ 
	s_gizmoMode = (s_gizmoMode == GM_Translate) ? GM_Rotate : GM_Translate; 
}
float CuiFurnitureMovementManager::getGizmoScale() { return s_gizmoScale; }
void CuiFurnitureMovementManager::setGizmoScale(float scale) { s_gizmoScale = scale; }
bool CuiFurnitureMovementManager::getGizmoWorldSpace() { return s_gizmoWorldSpace; }
void CuiFurnitureMovementManager::setGizmoWorldSpace(bool worldSpace) { s_gizmoWorldSpace = worldSpace; }
bool CuiFurnitureMovementManager::getSnapToGridHorizontal() { return s_snapToGridHorizontal; }
void CuiFurnitureMovementManager::setSnapToGridHorizontal(bool snap) { s_snapToGridHorizontal = snap; }
bool CuiFurnitureMovementManager::getSnapToGridVertical() { return s_snapToGridVertical; }
void CuiFurnitureMovementManager::setSnapToGridVertical(bool snap) { s_snapToGridVertical = snap; }
bool CuiFurnitureMovementManager::getSnapToGrid() { return s_snapToGridHorizontal || s_snapToGridVertical; }
void CuiFurnitureMovementManager::setSnapToGrid(bool snap) { s_snapToGridHorizontal = snap; s_snapToGridVertical = false; }
float CuiFurnitureMovementManager::getSnapGridMeters() { return s_snapGridMeters; }
void CuiFurnitureMovementManager::setSnapGridMeters(float meters) { s_snapGridMeters = std::max(0.5f, std::min(8.0f, meters)); }
void CuiFurnitureMovementManager::cycleSnapGridMeters()
{
	for (int i = 0; i < s_snapGridOptionCount; ++i)
		if (s_snapGridOptions[i] >= s_snapGridMeters - 0.01f)
		{
			s_snapGridMeters = s_snapGridOptions[(i + 1) % s_snapGridOptionCount];
			return;
		}
	s_snapGridMeters = s_snapGridOptions[0];
}
void CuiFurnitureMovementManager::dropSelectionToTerrain()
{
	Object * const obj = getSelectedObject();
	if (!obj) return;
	TerrainObject const * const terrain = TerrainObject::getConstInstance();
	if (!terrain) return;
	Vector pos = obj->getPosition_w();
	float height = 0.0f;
	if (!terrain->getHeight(pos, height))
		return;
	pos.y = height;
	Transform t = obj->getTransform_o2p();
	t.setPosition_p(pos);
	obj->setTransform_o2p(t);
	s_originalTransform = t;
	resetDeltas();
	resetGizmoState();
}
void CuiFurnitureMovementManager::resetSelectionRotation()
{
	Object * const obj = getSelectedObject();
	if (!obj) return;
	Transform const current = obj->getTransform_o2p();
	Vector const pos = current.getPosition_p();
	Transform t;
	t.setPosition_p(pos);
	// Identity rotation = face North (Y axis up)
	obj->setTransform_o2p(t);
	s_originalTransform = t;
	s_yawDelta = 0.0f;
	s_pitchDelta = 0.0f;
	s_rollDelta = 0.0f;
	resetGizmoState();
}

bool CuiFurnitureMovementManager::getFocusCameraOnSelection() { return s_focusCameraOnSelection; }
void CuiFurnitureMovementManager::setFocusCameraOnSelection(bool focus) { s_focusCameraOnSelection = focus; }

void CuiFurnitureMovementManager::focusCameraOnSelection()
{
	if (!s_active || !s_selectedFurniture.isValid()) return;
	Object * const obj = getSelectedObject();
	if (!obj) return;
	GroundScene * const gs = dynamic_cast<GroundScene *>(Game::getScene());
	if (!gs || gs->getCurrentView() != GroundScene::CI_free) return;
	FreeCamera * const freeCam = gs->getGodClientCamera();
	if (!freeCam) return;
	Vector const objPos = obj->getPosition_w();
	freeCam->setPivotPoint(objPos);
	FreeCamera::Info info = freeCam->getInfo();
	info.distance = std::max(2.0f, s_cachedObjectRadius + 2.0f);
	freeCam->setInfo(info);
	freeCam->setMode(FreeCamera::M_pivot);
}

bool CuiFurnitureMovementManager::getUseGimbalMovement() { return s_useGimbalMovement; }
void CuiFurnitureMovementManager::setUseGimbalMovement(bool use) { s_useGimbalMovement = use; }
bool CuiFurnitureMovementManager::getUseMouse4Drag() { return s_useMouse4Drag; }
void CuiFurnitureMovementManager::setUseMouse4Drag(bool use) { s_useMouse4Drag = use; }
bool CuiFurnitureMovementManager::getDragOnTerrain() { return s_dragOnTerrain; }
void CuiFurnitureMovementManager::setDragOnTerrain(bool on) { s_dragOnTerrain = on; }
void CuiFurnitureMovementManager::setMouse4Down(bool down) { s_mouse4Down = down; }

void CuiFurnitureMovementManager::sendControlsHelp()
{
	CuiSystemMessageManager::sendFakeSystemMessage(Unicode::narrowToWide("--- Decorator camera controls ---"));
	CuiSystemMessageManager::sendFakeSystemMessage(Unicode::narrowToWide("WASD: Move camera"));
	CuiSystemMessageManager::sendFakeSystemMessage(Unicode::narrowToWide("Mouse 1: Gimbal drag | Mouse 4: Free drag"));
	CuiSystemMessageManager::sendFakeSystemMessage(Unicode::narrowToWide("Mouse 2: Pan | Y: Terrain | F: Focus | ~: Snap 45 deg"));
	CuiSystemMessageManager::sendFakeSystemMessage(Unicode::narrowToWide("TAB: Cycle gizmo (move/rotate/scale)"));
	CuiSystemMessageManager::sendFakeSystemMessage(Unicode::narrowToWide("R: Apply changes"));
	CuiSystemMessageManager::sendFakeSystemMessage(Unicode::narrowToWide("Q / E: Scale gizmo"));
	CuiSystemMessageManager::sendFakeSystemMessage(Unicode::narrowToWide("T: Local/World orientation"));
	CuiSystemMessageManager::sendFakeSystemMessage(Unicode::narrowToWide("J: Snap horizontal (XZ) | H: Snap vertical (Y) | G: Grid size (0.5-8m)"));
	CuiSystemMessageManager::sendFakeSystemMessage(Unicode::narrowToWide("Shift: Fine movement"));
	CuiSystemMessageManager::sendFakeSystemMessage(Unicode::narrowToWide("Esc: Exit decorator camera"));
}

Vector const & CuiFurnitureMovementManager::getPositionDelta() { return s_positionDelta; }
float CuiFurnitureMovementManager::getYawDelta() { return s_yawDelta; }
float CuiFurnitureMovementManager::getPitchDelta() { return s_pitchDelta; }
float CuiFurnitureMovementManager::getRollDelta() { return s_rollDelta; }

//----------------------------------------------------------------------

bool CuiFurnitureMovementManager::processKeyDown(int const keystroke)
{
	// Allow WASD/camera keys when decorator camera is active (with or without object selected)
	bool const decoratorOnly = s_decoratorCameraActive && !s_active;
	
	// In decorator mode, block Alt so pointer stays in mouse mode
	if (s_decoratorCameraActive && (keystroke == DIK_LMENU || keystroke == DIK_RMENU))
		return true;
	
	switch (keystroke)
	{
	// Gizmo scale controls (only when object selected)
	case DIK_Q:
		if (!decoratorOnly) { s_gizmoScale = std::max(0.25f, s_gizmoScale - 0.25f); return true; }
		break;
	case DIK_E:
		if (!decoratorOnly) { s_gizmoScale = std::min(4.0f, s_gizmoScale + 0.25f); return true; }
		break;
		
	// Apply changes to server but stay in decorator mode (commit position, reset deltas)
	case DIK_R:
		if (!decoratorOnly)
		{
			Object * const obj = getSelectedObject();
			applyMovementToServer();
			if (obj)
			{
				Vector const pos = obj->getTransform_o2p().getPosition_p();
				char buf[128];
				snprintf(buf, sizeof(buf), "[setLocation]: %.4f %.4f %.4f", roundToTenThousandths(pos.x), roundToTenThousandths(pos.y), roundToTenThousandths(pos.z));
				CuiSystemMessageManager::sendFakeSystemMessage(Unicode::narrowToWide(buf), true);
				s_originalTransform = obj->getTransform_o2p();
				resetDeltas();
				resetGizmoState();
			}
			return true;
		}
		break;
		
	// Cycle gizmo mode (only when object selected)
	case DIK_TAB:
		if (!decoratorOnly) { cycleGizmoMode(); return true; }
		break;

	// Toggle local vs world orientation (only when object selected)
	case DIK_T:
		if (!decoratorOnly)
		{
			s_gizmoWorldSpace = !s_gizmoWorldSpace;
			return true;
		}
		break;

	// G: cycle grid size only (0.5 -> 8 -> 0.5)
	case DIK_G:
		if (s_decoratorCameraActive) { cycleSnapGridMeters(); return true; }
		break;
	// J: toggle horizontal (XZ) snap
	case DIK_J:
		if (s_decoratorCameraActive) { s_snapToGridHorizontal = !s_snapToGridHorizontal; return true; }
		break;
	// H: toggle vertical (Y) snap
	case DIK_H:
		if (s_decoratorCameraActive) { s_snapToGridVertical = !s_snapToGridVertical; return true; }
		break;
	// Y: toggle drag on terrain (for mouse4 and gimbal)
	case DIK_Y:
		if (s_decoratorCameraActive) { s_dragOnTerrain = !s_dragOnTerrain; return true; }
		break;
	// F: focus camera on selection
	case DIK_F:
		if (s_decoratorCameraActive && s_active) { focusCameraOnSelection(); return true; }
		break;
		
	// Tilde: snap rotation to 45 degrees
	case DIK_GRAVE:
		s_tildeHeld = true;
		return true;
	// Fine mode
	case DIK_LSHIFT:
	case DIK_RSHIFT:
		s_fineMode = true;
		return true;
		
	// Cancel and exit: ESC always exits decorator mode (with or without object) to avoid soft lock
	case DIK_ESCAPE:
		if (s_active)
			exitMovementMode(false);
		else if (s_decoratorCameraActive)
			disableDecoratorCamera();
		return true;
		
	// Camera movement (WASD + Space/Ctrl) - always when decorator camera active
	case DIK_W:
		s_cameraKeyW = true;
		return true;
	case DIK_A:
		s_cameraKeyA = true;
		return true;
	case DIK_S:
		s_cameraKeyS = true;
		return true;
	case DIK_D:
		s_cameraKeyD = true;
		return true;
	case DIK_SPACE:
		s_cameraKeySpace = true;
		return true;
	case DIK_LCONTROL:
	case DIK_RCONTROL:
		s_cameraKeyCtrl = true;
		return true;
		
	default:
		break;
	}
	return false;
}

//----------------------------------------------------------------------

bool CuiFurnitureMovementManager::processKeyUp(int const keystroke)
{
	// Allow WASD/camera key release when decorator camera is active (with or without object selected)
	if (!s_active && !s_decoratorCameraActive) return false;
	
	// In decorator mode, block Alt so pointer stays in mouse mode
	if (s_decoratorCameraActive && (keystroke == DIK_LMENU || keystroke == DIK_RMENU))
		return true;
	
	switch (keystroke)
	{
	case DIK_LSHIFT:
	case DIK_RSHIFT:
		s_fineMode = false;
		return true;
	case DIK_GRAVE:
		s_tildeHeld = false;
		return true;
		
	// Camera movement release
	case DIK_W:
		s_cameraKeyW = false;
		return true;
	case DIK_A:
		s_cameraKeyA = false;
		return true;
	case DIK_S:
		s_cameraKeyS = false;
		return true;
	case DIK_D:
		s_cameraKeyD = false;
		return true;
	case DIK_SPACE:
		s_cameraKeySpace = false;
		return true;
	case DIK_LCONTROL:
	case DIK_RCONTROL:
		s_cameraKeyCtrl = false;
		return true;
		
	default:
		break;
	}
	return false;
}

//----------------------------------------------------------------------

bool CuiFurnitureMovementManager::processMouseWheel(float const delta)
{
	if (!s_active) return false;
	
	// Mouse wheel adjusts gizmo scale
	float const scaleDelta = delta * 0.1f;
	s_gizmoScale = std::max(0.5f, std::min(3.0f, s_gizmoScale + scaleDelta));
	return true;
}

//----------------------------------------------------------------------

bool CuiFurnitureMovementManager::processMouseInput(int x, int y, bool leftButton, bool rightButton)
{
	// Convert to viewport-relative coordinates for correct behavior in windowed mode
	Camera const * const viewportCamera = Game::getCamera();
	if (viewportCamera)
	{
		x -= viewportCamera->getViewportX0();
		y -= viewportCamera->getViewportY0();
	}
	
	// Mouse4 up: release drag
	if (s_mouse4Dragging && !s_mouse4Down)
	{
		s_mouse4Dragging = false;
		s_activeComponent = GC_None;
	}
	
	// Mouse4 free drag: grab object and drag on XZ (or terrain)
	if (s_decoratorCameraActive && s_useMouse4Drag && s_mouse4Down)
	{
		if (s_mouse4Dragging && s_active)
		{
			int const deltaX = x - s_lastMouseX;
			int const deltaY = y - s_lastMouseY;
			if (deltaX != 0 || deltaY != 0)
				processMouseDrag(x, y, deltaX, deltaY);
			s_lastMouseX = x;
			s_lastMouseY = y;
			return true;
		}
		// Mouse4 down: try to grab object at cursor
		NetworkId hitId = getObjectAtScreenPosition(x, y, NULL);
		if (hitId.isValid())
		{
			Object * const currentObj = getSelectedObject();
			if (!currentObj || currentObj->getNetworkId() != hitId)
			{
				if (s_active)
					applyMovementToServer();
				enterMovementMode(hitId);
			}
			if (s_active)
			{
				s_mouse4Dragging = true;
				s_activeComponent = GC_Mouse4FreeDrag;
				s_dragStartPosition = getSelectedObject() ? getSelectedObject()->getPosition_w() : Vector::zero;
				s_lastMouseX = x;
				s_lastMouseY = y;
				return true;
			}
		}
	}
	
	// Right-click pan: allow when decorator camera is active (with or without object selected)
	if (s_decoratorCameraActive && rightButton)
	{
		// If we have an object and were dragging gizmo or mouse4, cancel the drag
		if (s_active && (s_isDragging || s_mouse4Dragging))
		{
			s_isDragging = false;
			s_mouse4Dragging = false;
			s_activeComponent = GC_None;
		}
		
		// Pan camera with right-click drag
		if (s_decoratorCameraActive)
		{
			int const deltaX = x - s_lastMouseX;
			int const deltaY = y - s_lastMouseY;
			
			if (deltaX != 0 || deltaY != 0)
			{
				GroundScene * const gs = dynamic_cast<GroundScene *>(Game::getScene());
				if (gs)
				{
					FreeCamera * const freeCamera = dynamic_cast<FreeCamera *>(gs->getCurrentCamera());
					if (freeCamera)
					{
						float const panSensitivity = 0.005f;
						FreeCamera::Info info = freeCamera->getInfo();
						// Inverted panning controls
						info.yaw += static_cast<float>(deltaX) * panSensitivity;
						info.pitch += static_cast<float>(deltaY) * panSensitivity;
						
						// Clamp pitch
						if (info.pitch > PI * 0.49f) info.pitch = PI * 0.49f;
						if (info.pitch < -PI * 0.49f) info.pitch = -PI * 0.49f;
						
						freeCamera->setInfo(info);
					}
				}
			}
		}
		
		s_lastMouseX = x;
		s_lastMouseY = y;
		return true;
	}
	
	// Rest of input (gizmo, selection) only when we have an object selected
	if (!s_active) return false;
	
	Object * const obj = getSelectedObject();
	if (!obj) return false;
	
	Camera const * const camera = Game::getCamera();
	if (!camera) return false;
	
	Vector const objectPosition = obj->getPosition_w();
	float const gizmoSize = s_cachedObjectRadius * s_gizmoScale * 2.0f;
	Transform gizmoTransform = obj->getTransform_o2w();
	if (s_gizmoWorldSpace)
	{
		gizmoTransform.resetRotateTranslate_l2p();
		gizmoTransform.setPosition_p(objectPosition);
	}
	
	// Handle gizmo dragging with left button (only when gimbal mode enabled)
	if (s_useGimbalMovement && s_isDragging && leftButton)
	{
		int const deltaX = x - s_lastMouseX;
		int const deltaY = y - s_lastMouseY;
		
		if (deltaX != 0 || deltaY != 0)
		{
			processMouseDrag(x, y, deltaX, deltaY);
		}
		
		s_lastMouseX = x;
		s_lastMouseY = y;
		return true;
	}
	
	// Handle mouse down - start gimbal drag or switch focus (only when gimbal mode enabled)
	if (s_useGimbalMovement && leftButton && !s_isDragging && !s_mouse4Dragging)
	{
		GizmoComponent const hit = hitTestGizmo(x, y, camera, gizmoTransform, gizmoSize);
		if (hit != GC_None)
		{
			s_isDragging = true;
			s_activeComponent = hit;
			s_lastMouseX = x;
			s_lastMouseY = y;
			s_dragStartPosition = objectPosition;
			return true;
		}
		
		// Click didn't hit gizmo - check if we clicked another object to switch focus
		if (selectObjectAtScreenPosition(x, y))
		{
			return true;
		}
	}
	
	// Handle mouse up (gimbal)
	if (!leftButton && s_isDragging)
	{
		s_isDragging = false;
		s_activeComponent = GC_None;
	}
	
	// Update hover state (with caching to reduce lag)
	if (x != s_lastHitTestX || y != s_lastHitTestY)
	{
		s_cachedHitResult = hitTestGizmo(x, y, camera, gizmoTransform, gizmoSize);
		s_lastHitTestX = x;
		s_lastHitTestY = y;
	}
	s_hoveredComponent = s_cachedHitResult;
	s_lastMouseX = x;
	s_lastMouseY = y;
	
	return s_hoveredComponent != GC_None || s_isDragging || s_mouse4Dragging;
}

//----------------------------------------------------------------------

bool CuiFurnitureMovementManager::processMouseDown(int x, int y, bool leftButton, bool rightButton)
{
	return processMouseInput(x, y, leftButton, rightButton);
}

//----------------------------------------------------------------------

bool CuiFurnitureMovementManager::processMouseUp(int x, int y, bool leftButton, bool rightButton)
{
	UNREF(leftButton);
	UNREF(rightButton);
	
	if (!s_active) return false;
	
	if (s_isDragging)
	{
		s_isDragging = false;
		s_activeComponent = GC_None;
		s_lastMouseX = x;
		s_lastMouseY = y;
		return true;
	}
	return false;
}

//----------------------------------------------------------------------

bool CuiFurnitureMovementManager::processMouseDrag(int x, int y, int deltaX, int deltaY)
{
	if (!s_active) return false;
	bool const isMouse4Drag = (s_mouse4Dragging && s_activeComponent == GC_Mouse4FreeDrag);
	if (!s_isDragging && !isMouse4Drag) return false;
	
	Camera const * const camera = Game::getCamera();
	if (!camera) return false;
	
	float const sensitivity = s_fineMode ? 0.005f : 0.05f;
	Vector const cameraRight = camera->getObjectFrameI_w();
	Vector const cameraForward = camera->getObjectFrameK_w();
	Vector const originalPos = s_originalTransform.getPosition_p();
	Vector const start = s_dragStartPosition;
	
	// Use ray-plane intersection for translation when in world space so the object stays under the cursor
	bool useRayPlane = s_gizmoWorldSpace;
	Vector hit;
	bool hitValid = false;
	if (useRayPlane)
	{
		Vector const planeNormalY(0.0f, 1.0f, 0.0f);
		Vector const planeNormalX(1.0f, 0.0f, 0.0f);
		switch (s_activeComponent)
		{
		case GC_AxisX:
			hitValid = rayPlaneIntersection(camera, x, y, start, planeNormalY, hit);
			if (hitValid) hit = Vector(hit.x, start.y, start.z);
			break;
		case GC_AxisY:
			hitValid = rayPlaneIntersection(camera, x, y, start, planeNormalX, hit);
			if (hitValid) hit = Vector(start.x, hit.y, start.z);
			break;
		case GC_AxisZ:
			hitValid = rayPlaneIntersection(camera, x, y, start, planeNormalY, hit);
			if (hitValid) hit = Vector(start.x, start.y, hit.z);
			break;
		case GC_PlaneXY:
			hitValid = rayPlaneIntersection(camera, x, y, start, Vector(0.0f, 0.0f, 1.0f), hit);
			if (hitValid) hit = Vector(hit.x, hit.y, start.z);
			break;
		case GC_PlaneXZ:
		case GC_Sphere:
			hitValid = rayPlaneIntersection(camera, x, y, start, planeNormalY, hit);
			break;
		case GC_PlaneYZ:
			hitValid = rayPlaneIntersection(camera, x, y, start, planeNormalX, hit);
			if (hitValid) hit = Vector(start.x, hit.y, hit.z);
			break;
		default:
			break;
		}
	}
	
	if (useRayPlane && hitValid)
	{
		if (s_dragOnTerrain)
		{
			TerrainObject const * const terrain = TerrainObject::getConstInstance();
			float terrainHeight = 0.0f;
			if (terrain && terrain->getHeight(hit, terrainHeight))
				hit.y = terrainHeight;
		}
		Vector const fullDelta = hit - originalPos;
		float const scale = s_fineMode ? s_fineModeFactor : 1.0f;
		s_positionDelta = fullDelta * scale;
		updateFurnitureTransform();
		return true;
	}
	
	// Delta-based fallback (local space or rotation)
	switch (s_activeComponent)
	{
	case GC_AxisX:
		{
			float const dot = cameraRight.x;
			float const sign = (dot >= 0.0f) ? 1.0f : -1.0f;
			moveX(static_cast<float>(deltaX) * sensitivity * sign);
		}
		break;
	case GC_AxisY:
		moveY(static_cast<float>(-deltaY) * sensitivity);
		break;
	case GC_AxisZ:
		{
			float const dot = cameraRight.z;
			float const sign = (dot >= 0.0f) ? 1.0f : -1.0f;
			moveZ(static_cast<float>(deltaX) * sensitivity * sign);
		}
		break;
	case GC_PlaneXY:
		moveX(static_cast<float>(deltaX) * sensitivity);
		moveY(static_cast<float>(-deltaY) * sensitivity);
		break;
	case GC_PlaneXZ:
		{
			Vector screenMove = cameraRight * static_cast<float>(deltaX) * sensitivity;
			screenMove -= cameraForward * static_cast<float>(deltaY) * sensitivity;
			moveX(screenMove.x);
			moveZ(screenMove.z);
		}
		break;
	case GC_PlaneYZ:
		moveY(static_cast<float>(-deltaY) * sensitivity);
		moveZ(static_cast<float>(deltaX) * sensitivity);
		break;
	case GC_RotateYaw:
		rotateYaw(static_cast<float>(deltaX) * sensitivity * 20.0f);
		if (s_tildeHeld)
			s_yawDelta = std::floor(s_yawDelta / 45.0f + 0.5f) * 45.0f;
		break;
	case GC_RotatePitch:
		rotatePitch(static_cast<float>(deltaY) * sensitivity * 20.0f);
		if (s_tildeHeld)
			s_pitchDelta = std::floor(s_pitchDelta / 45.0f + 0.5f) * 45.0f;
		break;
	case GC_RotateRoll:
		rotateRoll(static_cast<float>(deltaX) * sensitivity * 20.0f);
		if (s_tildeHeld)
			s_rollDelta = std::floor(s_rollDelta / 45.0f + 0.5f) * 45.0f;
		break;
	case GC_Sphere:
		{
			Vector screenMove = cameraRight * static_cast<float>(deltaX) * sensitivity;
			screenMove -= cameraForward * static_cast<float>(deltaY) * sensitivity;
			moveX(screenMove.x);
			moveZ(screenMove.z);
		}
		break;
	case GC_Mouse4FreeDrag:
		{
			// Free drag on XZ plane; optionally snap to terrain
			Object * const obj = getSelectedObject();
			if (!obj) return false;
			Vector const planePoint = s_dragStartPosition;
			Vector hit;
			if (rayPlaneIntersection(camera, x, y, planePoint, Vector::unitY, hit))
			{
				if (s_dragOnTerrain)
				{
					TerrainObject const * const terrain = TerrainObject::getConstInstance();
					float terrainHeight = 0.0f;
					if (terrain && terrain->getHeight(hit, terrainHeight))
						hit.y = terrainHeight;
				}
				Vector const origPos = s_originalTransform.getPosition_p();
				s_positionDelta = hit - origPos;
				updateFurnitureTransform();
			}
		}
		break;
	default:
		return false;
	}
	
	updateFurnitureTransform();
	return true;
}

//----------------------------------------------------------------------

void CuiFurnitureMovementManager::render()
{
	if (!s_active || !s_selectedFurniture.isValid()) return;
	
	Object * const obj = getSelectedObject();
	if (!obj) return;
	
	Camera const * const camera = Game::getCamera();
	if (!camera) return;
	
	// Use 3D vertex color shader without Z/depth so gizmo renders through terrain (always visible)
	Graphics::setStaticShader(ShaderTemplateList::get3dVertexColorStaticShader());
	
	// Gizmo transform: world orientation = position only (identity rotation); local = full object transform
	Transform const & objTransform = obj->getTransform_o2w();
	Transform gizmoTransform = objTransform;
	if (s_gizmoWorldSpace)
	{
		gizmoTransform.resetRotateTranslate_l2p();
		gizmoTransform.setPosition_p(objTransform.getPosition_p());
	}
	float const gizmoSize = s_cachedObjectRadius * s_gizmoScale * 2.0f;
	float const lineThickness = gizmoSize * 0.02f; // For thicker lines
	
	// Set up transform for 3D rendering at object location
	Graphics::setObjectToWorldTransformAndScale(gizmoTransform, obj->getScale());
	
	// Render text overlay
	renderTextOverlay();
	
	// Helper lambda to draw thicker lines (draw main line plus offset lines)
	auto drawThickLine = [&](Vector const & start, Vector const & end, VectorArgb const & color) {
		Graphics::drawLine(start, end, color);
		// Draw additional offset lines for thickness
		Vector dir = end - start;
		dir.normalize();
		Vector perp1 = dir.cross(Vector::unitY);
		if (perp1.magnitudeSquared() < 0.01f)
			perp1 = dir.cross(Vector::unitX);
		perp1.normalize();
		Vector perp2 = dir.cross(perp1);
		perp2.normalize();
		
		Graphics::drawLine(start + perp1 * lineThickness, end + perp1 * lineThickness, color);
		Graphics::drawLine(start - perp1 * lineThickness, end - perp1 * lineThickness, color);
		Graphics::drawLine(start + perp2 * lineThickness, end + perp2 * lineThickness, color);
		Graphics::drawLine(start - perp2 * lineThickness, end - perp2 * lineThickness, color);
	};
	
	// Draw gizmo based on mode
	if (s_gizmoMode == GM_Translate)
	{
		// Draw axis lines with thickness
		bool const xH = (s_hoveredComponent == GC_AxisX || s_activeComponent == GC_AxisX);
		bool const yH = (s_hoveredComponent == GC_AxisY || s_activeComponent == GC_AxisY);
		bool const zH = (s_hoveredComponent == GC_AxisZ || s_activeComponent == GC_AxisZ);
		
		VectorArgb const xColor = xH ? VectorArgb(1.0f, 1.0f, 0.4f, 0.4f) : VectorArgb(1.0f, 0.8f, 0.1f, 0.1f);
		VectorArgb const yColor = yH ? VectorArgb(1.0f, 0.4f, 1.0f, 0.4f) : VectorArgb(1.0f, 0.1f, 0.8f, 0.1f);
		VectorArgb const zColor = zH ? VectorArgb(1.0f, 0.4f, 0.4f, 1.0f) : VectorArgb(1.0f, 0.1f, 0.1f, 0.8f);
		
		// Axis shafts: thick lines
		drawThickLine(Vector::zero, Vector(gizmoSize, 0.0f, 0.0f), xColor);
		drawThickLine(Vector::zero, Vector(0.0f, gizmoSize, 0.0f), yColor);
		drawThickLine(Vector::zero, Vector(0.0f, 0.0f, gizmoSize), zColor);
		
		// Axis handles: solid cylinder (shaft) + solid octahedron (head) per axis
		float const shaftLen = gizmoSize * 0.88f;
		float const shaftRadius = gizmoSize * 0.04f;
		float const headSize = gizmoSize * 0.12f;
		int const cylSegments = 12;
		// Solid cylinder/octahedron are along +Y (base to base+Vector(0,height,0))
		// X axis: rotate so Y -> X (roll -90 around Z)
		Transform axisX = gizmoTransform;
		axisX.roll_l(-PI / 2.0f);
		Graphics::setObjectToWorldTransformAndScale(axisX, obj->getScale());
		Graphics::drawSolidCylinder(Vector::zero, shaftRadius, shaftLen, cylSegments, xColor);
		Graphics::drawSolidOctahedron(Vector(0.0f, shaftLen, 0.0f), headSize, xColor);
		// Y axis: already along Y
		Graphics::setObjectToWorldTransformAndScale(gizmoTransform, obj->getScale());
		Graphics::drawSolidCylinder(Vector::zero, shaftRadius, shaftLen, cylSegments, yColor);
		Graphics::drawSolidOctahedron(Vector(0.0f, shaftLen, 0.0f), headSize, yColor);
		// Z axis: rotate so Y -> Z (pitch +90 around X)
		Transform axisZ = gizmoTransform;
		axisZ.pitch_l(PI / 2.0f);
		Graphics::setObjectToWorldTransformAndScale(axisZ, obj->getScale());
		Graphics::drawSolidCylinder(Vector::zero, shaftRadius, shaftLen, cylSegments, zColor);
		Graphics::drawSolidOctahedron(Vector(0.0f, shaftLen, 0.0f), headSize, zColor);
		// Restore for center
		Graphics::setObjectToWorldTransformAndScale(gizmoTransform, obj->getScale());
		
		// Draw center indicator for translate mode (solid)
		bool const centerH = (s_hoveredComponent == GC_Sphere || s_activeComponent == GC_Sphere);
		VectorArgb const centerColor = centerH ? VectorArgb(1.0f, 1.0f, 1.0f, 1.0f) : VectorArgb(1.0f, 0.9f, 0.9f, 0.5f);
		Graphics::drawSolidOctahedron(Vector::zero, gizmoSize * 0.15f, centerColor);
	}
	else if (s_gizmoMode == GM_Rotate)
	{
		// Rotate mode: axis lines only (drawExtentDiscsYPR/drawDisc causes 0xc0000090 in gl07_r.dll)
		float const r = gizmoSize * 1.2f;
		bool const yawH = (s_hoveredComponent == GC_RotateYaw || s_activeComponent == GC_RotateYaw);
		bool const pitchH = (s_hoveredComponent == GC_RotatePitch || s_activeComponent == GC_RotatePitch);
		bool const rollH = (s_hoveredComponent == GC_RotateRoll || s_activeComponent == GC_RotateRoll);
		VectorArgb const yawColor   = yawH   ? VectorArgb(1.0f, 0.5f, 0.5f, 1.0f) : VectorArgb(1.0f, 0.2f, 0.2f, 0.9f);
		VectorArgb const pitchColor = pitchH ? VectorArgb(1.0f, 0.5f, 1.0f, 0.5f) : VectorArgb(1.0f, 0.2f, 0.9f, 0.2f);
		VectorArgb const rollColor  = rollH  ? VectorArgb(1.0f, 1.0f, 0.5f, 0.5f) : VectorArgb(1.0f, 0.9f, 0.2f, 0.2f);
		Graphics::drawLine(Vector::zero, Vector(0.0f, r, 0.0f), yawColor);   // Y axis = yaw
		Graphics::drawLine(Vector::zero, Vector(r, 0.0f, 0.0f), pitchColor); // X axis = pitch
		Graphics::drawLine(Vector::zero, Vector(0.0f, 0.0f, r), rollColor);   // Z axis = roll
	}
}

//----------------------------------------------------------------------

void CuiFurnitureMovementManager::renderTranslationGizmo(Vector const & position, float scale)
{
	UNREF(position);
	UNREF(scale);
	// Handled in render()
}

//----------------------------------------------------------------------

void CuiFurnitureMovementManager::renderRotationGizmo(Vector const & position, float scale)
{
	UNREF(position);
	UNREF(scale);
	// Handled in render()
}

//----------------------------------------------------------------------

void CuiFurnitureMovementManager::renderAxisLine(Vector const & start, Vector const & end, unsigned int color, bool highlighted)
{
	UNREF(color);
	UNREF(highlighted);
	Graphics::drawLine(start, end, VectorArgb::solidWhite);
}

//----------------------------------------------------------------------

void CuiFurnitureMovementManager::renderCircle(Vector const & center, Vector const & normal, float radius, unsigned int color, bool highlighted, int segments)
{
	UNREF(color);
	UNREF(highlighted);
	Graphics::drawCircle(center, normal, radius, segments, VectorArgb::solidWhite);
}

//----------------------------------------------------------------------

CuiFurnitureMovementManager::GizmoComponent CuiFurnitureMovementManager::hitTestGizmo(int screenX, int screenY, Camera const * camera, Transform const & gizmoTransform, float gizmoSize)
{
	if (!camera) return GC_None;
	
	Vector const gizmoPosition = gizmoTransform.getPosition_p();
	
	// Bigger hit threshold for easier clicking
	float const threshold = 30.0f; // pixels
	float const handleThreshold = 40.0f; // even bigger for handles
	
	if (s_gizmoMode == GM_Translate)
	{
		// Test axis endpoint handles first (bigger hit area); endpoints in world space
		Vector xEnd = gizmoTransform.rotateTranslate_l2p(Vector(gizmoSize, 0.0f, 0.0f));
		Vector yEnd = gizmoTransform.rotateTranslate_l2p(Vector(0.0f, gizmoSize, 0.0f));
		Vector zEnd = gizmoTransform.rotateTranslate_l2p(Vector(0.0f, 0.0f, gizmoSize));
		
		Vector screenX_end, screenY_end, screenZ_end;
		if (camera->projectInWorldSpace(xEnd, &screenX_end.x, &screenX_end.y, &screenX_end.z, false))
		{
			float dx = static_cast<float>(screenX) - screenX_end.x;
			float dy = static_cast<float>(screenY) - screenX_end.y;
			if (std::sqrt(dx * dx + dy * dy) < handleThreshold)
				return GC_AxisX;
		}
		if (camera->projectInWorldSpace(yEnd, &screenY_end.x, &screenY_end.y, &screenY_end.z, false))
		{
			float dx = static_cast<float>(screenX) - screenY_end.x;
			float dy = static_cast<float>(screenY) - screenY_end.y;
			if (std::sqrt(dx * dx + dy * dy) < handleThreshold)
				return GC_AxisY;
		}
		if (camera->projectInWorldSpace(zEnd, &screenZ_end.x, &screenZ_end.y, &screenZ_end.z, false))
		{
			float dx = static_cast<float>(screenX) - screenZ_end.x;
			float dy = static_cast<float>(screenY) - screenZ_end.y;
			if (std::sqrt(dx * dx + dy * dy) < handleThreshold)
				return GC_AxisZ;
		}
		
		// Test axis lines
		if (hitTestAxis(screenX, screenY, camera, gizmoPosition, xEnd, threshold))
			return GC_AxisX;
		if (hitTestAxis(screenX, screenY, camera, gizmoPosition, yEnd, threshold))
			return GC_AxisY;
		if (hitTestAxis(screenX, screenY, camera, gizmoPosition, zEnd, threshold))
			return GC_AxisZ;
	}
	else if (s_gizmoMode == GM_Rotate)
	{
		// Test hollow discs - use outer radius for hit testing; normals in world space
		float const outerRadius = gizmoSize * 1.2f;
		Vector const yawNormal = gizmoTransform.rotate_l2p(Vector::unitY);
		Vector const pitchNormal = gizmoTransform.rotate_l2p(Vector::unitX);
		Vector const rollNormal = gizmoTransform.rotate_l2p(Vector::unitZ);
		if (hitTestCircle(screenX, screenY, camera, gizmoPosition, yawNormal, outerRadius, threshold))
			return GC_RotateYaw;
		if (hitTestCircle(screenX, screenY, camera, gizmoPosition, pitchNormal, outerRadius * 0.95f, threshold))
			return GC_RotatePitch;
		if (hitTestCircle(screenX, screenY, camera, gizmoPosition, rollNormal, outerRadius * 0.9f, threshold))
			return GC_RotateRoll;
	}
	
	// Test center sphere (bigger hit area)
	Vector screenPos;
	if (camera->projectInWorldSpace(gizmoPosition, &screenPos.x, &screenPos.y, &screenPos.z, false))
	{
		float const dx = static_cast<float>(screenX) - screenPos.x;
		float const dy = static_cast<float>(screenY) - screenPos.y;
		if (std::sqrt(dx * dx + dy * dy) < handleThreshold)
			return GC_Sphere;
	}
	
	return GC_None;
}

//----------------------------------------------------------------------

bool CuiFurnitureMovementManager::hitTestAxis(int screenX, int screenY, Camera const * camera, Vector const & start, Vector const & end, float threshold)
{
	if (!camera) return false;
	
	Vector screenStart, screenEnd;
	if (!camera->projectInWorldSpace(start, &screenStart.x, &screenStart.y, &screenStart.z, false))
		return false;
	if (!camera->projectInWorldSpace(end, &screenEnd.x, &screenEnd.y, &screenEnd.z, false))
		return false;
	
	// Point to line segment distance in 2D
	float const ax = screenEnd.x - screenStart.x;
	float const ay = screenEnd.y - screenStart.y;
	float const bx = static_cast<float>(screenX) - screenStart.x;
	float const by = static_cast<float>(screenY) - screenStart.y;
	
	float const lenSq = ax * ax + ay * ay;
	if (lenSq < 0.001f) return false;
	
	float t = (ax * bx + ay * by) / lenSq;
	t = std::max(0.0f, std::min(1.0f, t));
	
	float const projX = screenStart.x + t * ax;
	float const projY = screenStart.y + t * ay;
	
	float const dx = static_cast<float>(screenX) - projX;
	float const dy = static_cast<float>(screenY) - projY;
	float const dist = std::sqrt(dx * dx + dy * dy);
	
	return dist < threshold;
}

//----------------------------------------------------------------------

bool CuiFurnitureMovementManager::hitTestCircle(int screenX, int screenY, Camera const * camera, Vector const & center, Vector const & normal, float radius, float threshold)
{
	if (!camera) return false;
	
	// Sample points on the circle and test distance
	int const numSamples = 16;
	Vector tangent, bitangent;
	
	// Create orthonormal basis
	if (std::fabs(normal.y) < 0.99f)
		tangent = normal.cross(Vector::unitY);
	else
		tangent = normal.cross(Vector::unitX);
	tangent.normalize();
	bitangent = normal.cross(tangent);
	
	for (int i = 0; i < numSamples; ++i)
	{
		float const angle = (static_cast<float>(i) / static_cast<float>(numSamples)) * 2.0f * PI;
		Vector const worldPoint = center + tangent * (std::cos(angle) * radius) + bitangent * (std::sin(angle) * radius);
		
		Vector screenPoint;
		if (camera->projectInWorldSpace(worldPoint, &screenPoint.x, &screenPoint.y, &screenPoint.z, false))
		{
			float const dx = static_cast<float>(screenX) - screenPoint.x;
			float const dy = static_cast<float>(screenY) - screenPoint.y;
			if (std::sqrt(dx * dx + dy * dy) < threshold)
				return true;
		}
	}
	
	return false;
}

//----------------------------------------------------------------------

void CuiFurnitureMovementManager::applyMovementToServer()
{
	if (!s_selectedFurniture.isValid()) return;
	
	// Build final transform (same logic as updateFurnitureTransform) and send one BYPASS move + one BYPASS rotate.
	Transform finalTransform = s_originalTransform;
	Vector position;
	if (s_gizmoWorldSpace)
		position = finalTransform.getPosition_p() + s_positionDelta;
	else
		position = finalTransform.getPosition_p() + finalTransform.rotate_l2p(s_positionDelta);
	if (s_snapGridMeters > 0.0f)
	{
		float const g = s_snapGridMeters;
		if (s_snapToGridHorizontal)
		{
			position.x = std::floor(position.x / g + 0.5f) * g;
			position.z = std::floor(position.z / g + 0.5f) * g;
		}
		if (s_snapToGridVertical)
			position.y = std::floor(position.y / g + 0.5f) * g;
	}
	finalTransform.setPosition_p(position);
	float const yawRad = s_yawDelta * PI / 180.0f;
	float const pitchRad = s_pitchDelta * PI / 180.0f;
	float const rollRad = s_rollDelta * PI / 180.0f;
	finalTransform.yaw_l(yawRad);
	finalTransform.pitch_l(pitchRad);
	finalTransform.roll_l(rollRad);
	
	// Validate before sending - bad data causes 0xc0000090 crash when client restarts
	float const rx = roundToTenThousandths(position.x);
	float const ry = roundToTenThousandths(position.y);
	float const rz = roundToTenThousandths(position.z);
	bool posValid = (_finite(static_cast<double>(rx)) != 0 && _finite(static_cast<double>(ry)) != 0 && _finite(static_cast<double>(rz)) != 0);
	if (!posValid || rx < -100000.0f || rx > 100000.0f || rz < -100000.0f || rz > 100000.0f || ry < -10000.0f || ry > 10000.0f)
	{
		revertToOriginalPosition();
		resetDeltas();
		resetGizmoState();
		return;
	}
	
	NetworkId const & targetId = s_selectedFurniture;
	char buffer[256];
	snprintf(buffer, sizeof(buffer), "BYPASS %.4f %.4f %.4f", rx, ry, rz);
	ClientCommandQueue::enqueueCommand(s_hashMoveFurniture, targetId, Unicode::narrowToWide(buffer));
	
	Quaternion q(finalTransform);
	float magSq = q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z;
	if (magSq < 0.0001f || magSq > 100.0f || _finite(static_cast<double>(q.w)) == 0 || _finite(static_cast<double>(q.x)) == 0 || _finite(static_cast<double>(q.y)) == 0 || _finite(static_cast<double>(q.z)) == 0)
	{
		revertToOriginalPosition();
		resetDeltas();
		resetGizmoState();
		return;
	}
	if (std::fabs(magSq - 1.0f) > 0.01f)
	{
		float const mag = std::sqrt(magSq);
		q.w /= mag;
		q.x /= mag;
		q.y /= mag;
		q.z /= mag;
	}
	float const rw = roundToTenThousandths(q.w);
	float const rqx = roundToTenThousandths(q.x);
	float const rqy = roundToTenThousandths(q.y);
	float const rqz = roundToTenThousandths(q.z);
	snprintf(buffer, sizeof(buffer), "BYPASS %.4f %.4f %.4f %.4f", rw, rqx, rqy, rqz);
	ClientCommandQueue::enqueueCommand(s_hashRotateFurniture, targetId, Unicode::narrowToWide(buffer));
}

//----------------------------------------------------------------------

void CuiFurnitureMovementManager::revertToOriginalPosition()
{
	Object * const obj = getSelectedObject();
	if (obj)
	{
		obj->setTransform_o2p(s_originalTransform);
	}
}

//----------------------------------------------------------------------

void CuiFurnitureMovementManager::updateFurnitureTransform()
{
	Object * const obj = getSelectedObject();
	if (!obj) return;
	
	Transform newTransform = s_originalTransform;
	
	Vector position;
	if (s_gizmoWorldSpace)
		position = newTransform.getPosition_p() + s_positionDelta;
	else
		position = newTransform.getPosition_p() + newTransform.rotate_l2p(s_positionDelta);
	if (s_snapGridMeters > 0.0f)
	{
		float const g = s_snapGridMeters;
		if (s_snapToGridHorizontal)
		{
			position.x = std::floor(position.x / g + 0.5f) * g;
			position.z = std::floor(position.z / g + 0.5f) * g;
		}
		if (s_snapToGridVertical)
			position.y = std::floor(position.y / g + 0.5f) * g;
	}
	newTransform.setPosition_p(position);
	
	float const yawRad = s_yawDelta * PI / 180.0f;
	float const pitchRad = s_pitchDelta * PI / 180.0f;
	float const rollRad = s_rollDelta * PI / 180.0f;
	
	newTransform.yaw_l(yawRad);
	newTransform.pitch_l(pitchRad);
	newTransform.roll_l(rollRad);
	
	obj->setTransform_o2p(newTransform);
}

//----------------------------------------------------------------------

void CuiFurnitureMovementManager::enableDecoratorCamera()
{
	if (s_decoratorCameraActive) return;
	
	GroundScene * const gs = dynamic_cast<GroundScene *>(Game::getScene());
	if (!gs) return;
	
	// Save current camera mode
	s_previousCameraMode = gs->getCurrentView();
	
	// Save current HUD opacity and hide UI
	//s_previousHudOpacity = CuiPreferences::getHudOpacity();
	//CuiPreferences::setHudOpacity(0.0f);
	
	// Switch to free camera (fixed setup to avoid 0xc0000090 in chase-cam init path)
	gs->setView(GroundScene::CI_free);
	Object * const player = Game::getPlayer();
	FreeCamera * const freeCam = gs->getGodClientCamera();
	if (player && freeCam)
	{
		Vector pivotPos = player->getPosition_w();
		TerrainObject const * const terrain = TerrainObject::getConstInstance();
		if (terrain)
		{
			float groundHeight = 0.0f;
			if (terrain->getHeight(pivotPos, groundHeight))
				pivotPos.y = groundHeight;
		}
		freeCam->setMode(FreeCamera::M_pivot);
		freeCam->setPivotPoint(pivotPos);
		FreeCamera::Info info = freeCam->getInfo();
		info.distance = 20.0f;
		info.pitch = -PI / 4.0f;
		info.yaw = 0.0f;
		freeCam->setInfo(info);
	}
	
	// Enable pointer input so mouse cursor is visible
	CuiManager::setPointerToggledOn(true);
	
	s_decoratorCameraActive = true;
}

//----------------------------------------------------------------------

void CuiFurnitureMovementManager::disableDecoratorCamera()
{
	if (!s_decoratorCameraActive) return;
	
	GroundScene * const gs = dynamic_cast<GroundScene *>(Game::getScene());
	if (gs && s_previousCameraMode >= 0)
	{
		// Restore previous camera mode
		gs->setView(s_previousCameraMode);
	}
	
	// Restore HUD opacity
	//CuiPreferences::setHudOpacity(s_previousHudOpacity);

	
	// Disable pointer toggle
	CuiManager::setPointerToggledOn(false);
	
	s_decoratorCameraActive = false;
	s_previousCameraMode = -1;
}

//----------------------------------------------------------------------

bool CuiFurnitureMovementManager::isDecoratorCameraActive()
{
	return s_decoratorCameraActive;
}

//----------------------------------------------------------------------

void CuiFurnitureMovementManager::openSpawnUI()
{
	// Open the decorator spawn UI
	CuiActionManager::performAction(CuiActions::decoratorSpawn, Unicode::emptyString);
}

//----------------------------------------------------------------------

void CuiFurnitureMovementManager::sendSetPositionToServer()
{
	/*
	//get location (cell too) and transform/rotation of the furniture 
	if (!s_selectedFurniture.isValid()) return;
	Object* const obj = getSelectedObject();
	if (!obj) return;
	Transform const& t = obj->getTransform_o2p();

	char buffer[256];
	
	*/
}

//----------------------------------------------------------------------

bool CuiFurnitureMovementManager::selectObjectAtScreenPosition(int x, int y)
{
	// Only allow selection when decorator camera is active
	if (!s_decoratorCameraActive) return false;
	
	Camera const * const camera = Game::getCamera();
	if (!camera) return false;
	
	// Viewport-relative coordinates for correct picking in windowed mode
	int const vx = x - camera->getViewportX0();
	int const vy = y - camera->getViewportY0();
	
	Object * const currentObj = getSelectedObject();
	
	// Use proper collision detection for mesh-accurate targeting
	Vector const start = camera->getPosition_w();
	Vector const direction = camera->rotate_o2w(camera->reverseProjectInScreenSpace(vx, vy));
	Vector const end = start + direction * 512.0f;
	
	// Use ClientWorld::collide for accurate mesh collision (bitwise OR to combine flags)
	CollideParameters collideParams;
	collideParams.setQuality(CollideParameters::Q_high);
	
	ClientWorld::CollisionInfoVector results;
	// Use camera's cell so picking works in interiors (portals respected)
	CellProperty const * const startCell = camera->getParentCell();
	
	uint16 const pickFlags = ClientWorld::CF_tangible | ClientWorld::CF_tangibleNotTargetable | ClientWorld::CF_interiorGeometry | ClientWorld::CF_interiorObjects | ClientWorld::CF_childObjects;
	if (ClientWorld::collide(startCell, start, end, collideParams, results, pickFlags, currentObj))
	{
		// Results are sorted front to back. If we already have a selection and the first hit is it,
		// don't switch (user clicked on current object, e.g. missed the gizmo while dragging).
		for (ClientWorld::CollisionInfoVector::const_iterator it = results.begin(); it != results.end(); ++it)
		{
			Object const * const hitObject = it->getObject();
			if (!hitObject) continue;
			ClientObject * const clientObj = const_cast<Object*>(hitObject)->asClientObject();
			if (!clientObj || !clientObj->getNetworkId().isValid()) continue;
			// First valid hit: if it's our current object, don't switch selection
			if (s_active && currentObj && hitObject == currentObj)
				return false;
			// First valid hit is another object (or we have no selection) - switch to it
			if (hitObject != currentObj)
			{
				if (s_active)
					applyMovementToServer();
				enterMovementMode(clientObj->getNetworkId());
				return true;
			}
		}
	}
	
	return false;
}

//----------------------------------------------------------------------

void CuiFurnitureMovementManager::renderTextOverlay()
{
	// Overlay text is now shown in the decorator camera UI panel.
}

std::string CuiFurnitureMovementManager::getSelectionOverlayPositionLine()
{
	if (!s_active || !s_selectedFurniture.isValid()) return std::string();
	Object * const obj = getSelectedObject();
	if (!obj) return std::string();
	Vector const position = obj->getPosition_w();
	return FormattedString<128>().sprintf("X: %.2f  Z: %.2f  Y: %.2f", position.x, position.z, position.y);
}

std::string CuiFurnitureMovementManager::getSelectionOverlaySelectionLine()
{
	if (!s_active || !s_selectedFurniture.isValid()) return std::string();
	Object * const obj = getSelectedObject();
	if (!obj) return std::string();
	ClientObject * const clientObj = obj->asClientObject();
	std::string name;
	if (clientObj)
		name = Unicode::wideToNarrow(clientObj->getLocalizedName());
	else
		name = "Unknown Object";
	return "Selection: " + name + " (" + s_selectedFurniture.getValueString() + ")";
}

//----------------------------------------------------------------------