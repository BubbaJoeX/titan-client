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
#include "sharedCollision/BoxExtent.h"
#include "sharedCollision/CollideParameters.h"
#include "sharedCollision/CollisionInfo.h"
#include "sharedCollision/Extent.h"
#include "sharedFoundation/Clock.h"
#include "sharedFoundation/Crc.h"
#include "sharedMath/PackedArgb.h"
#include "sharedMath/Sphere.h"
#include "sharedMath/Transform.h"
#include "sharedMath/Vector.h"
#include "sharedMath/VectorArgb.h"
#include "sharedObject/Appearance.h"
#include "sharedObject/CachedNetworkId.h"
#include "sharedObject/CellProperty.h"
#include "sharedObject/NetworkIdManager.h"
#include "UnicodeUtils.h"

#include <cmath>
#include <dinput.h>

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
		s_lastMouseX = 0;
		s_lastMouseY = 0;
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
	if (!s_installed || !s_active) return;
	
	Object * const obj = getSelectedObject();
	if (!obj)
	{
		exitMovementMode(false);
		return;
	}
	
	// Handle WASD camera movement in decorator mode
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
	
	// Update furniture transform if not dragging
	if (!s_isDragging)
	{
		updateFurnitureTransform();
	}
}

//----------------------------------------------------------------------

bool CuiFurnitureMovementManager::enterMovementMode(NetworkId const & furnitureId)
{
	if (!s_installed) return false;
	if (s_active) exitMovementMode(false);
	
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
	
	// Enable decorator camera for gizmo interaction
	enableDecoratorCamera();
	
	CuiSystemMessageManager::sendFakeSystemMessage(Unicode::narrowToWide("== Decorator Mode == "));
	CuiSystemMessageManager::sendFakeSystemMessage(Unicode::narrowToWide("TAB: Cycle Gizmo"));
	CuiSystemMessageManager::sendFakeSystemMessage(Unicode::narrowToWide("WASD: Movement"));
	CuiSystemMessageManager::sendFakeSystemMessage(Unicode::narrowToWide("Mouse 1: Drag or select an object"));
	CuiSystemMessageManager::sendFakeSystemMessage(Unicode::narrowToWide("Mouse 2: Drag to pan"));
	CuiSystemMessageManager::sendFakeSystemMessage(Unicode::narrowToWide("R: Apply Changes"));
	CuiSystemMessageManager::sendFakeSystemMessage(Unicode::narrowToWide("Q or E: Scale Gizmo:"));
	CuiSystemMessageManager::sendFakeSystemMessage(Unicode::narrowToWide("T: Object Tree Viewer"));
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

Vector const & CuiFurnitureMovementManager::getPositionDelta() { return s_positionDelta; }
float CuiFurnitureMovementManager::getYawDelta() { return s_yawDelta; }
float CuiFurnitureMovementManager::getPitchDelta() { return s_pitchDelta; }
float CuiFurnitureMovementManager::getRollDelta() { return s_rollDelta; }

//----------------------------------------------------------------------

bool CuiFurnitureMovementManager::processKeyDown(int const keystroke)
{
	if (!s_active) return false;
	
	switch (keystroke)
	{
	// Gizmo scale controls
	case DIK_Q:
		s_gizmoScale = std::max(0.25f, s_gizmoScale - 0.25f);
		return true;
	case DIK_E:
		s_gizmoScale = std::min(4.0f, s_gizmoScale + 0.25f);
		return true;
		
	// Apply and exit
	case DIK_R:
		exitMovementMode(true);
		return true;
		
	// Cycle gizmo mode
	case DIK_TAB:
		cycleGizmoMode();
		return true;
		
	// Fine mode
	case DIK_LSHIFT:
	case DIK_RSHIFT:
		s_fineMode = true;
		return true;
		
	// Cancel and exit
	case DIK_ESCAPE:
		exitMovementMode(false);
		return true;
		
	// Open object spawn UI
	case DIK_T:
		if (s_decoratorCameraActive)
		{
			openSpawnUI();
		}
		return true;
		
	// Camera movement (WASD + Space/Ctrl)
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
	if (!s_active) return false;
	
	switch (keystroke)
	{
	case DIK_LSHIFT:
	case DIK_RSHIFT:
		s_fineMode = false;
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
	if (!s_active) return false;
	
	Object * const obj = getSelectedObject();
	if (!obj) return false;
	
	Camera const * const camera = Game::getCamera();
	if (!camera) return false;
	
	Vector const objectPosition = obj->getPosition_w();
	float const gizmoSize = s_cachedObjectRadius * s_gizmoScale * 2.0f;
	
	// Right-click to pan camera
	if (rightButton)
	{
		// If dragging gizmo, cancel the drag
		if (s_isDragging)
		{
			s_isDragging = false;
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
	
	// Handle gizmo dragging with left button
	if (s_isDragging && leftButton)
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
	
	// Handle mouse down - start drag or switch focus
	if (leftButton && !s_isDragging)
	{
		GizmoComponent const hit = hitTestGizmo(x, y, camera, objectPosition, gizmoSize);
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
		if (s_decoratorCameraActive)
		{
			// Use proper collision detection for mesh-accurate targeting
			Vector const start = camera->getPosition_w();
			Vector const direction = camera->rotate_o2w(camera->reverseProjectInScreenSpace(x, y));
			Vector const end = start + direction * 512.0f;
			
			// Use ClientWorld::collide for accurate mesh collision
			CollideParameters collideParams;
			collideParams.setQuality(CollideParameters::Q_high);
			
			ClientWorld::CollisionInfoVector results;
			CellProperty const * const startCell = CellProperty::getWorldCellProperty();
			
			if (ClientWorld::collide(startCell, start, end, collideParams, results, ClientWorld::CF_tangible || ClientWorld::CF_tangibleNotTargetable || ClientWorld::CF_interiorGeometry, obj))
			{
				// Results are sorted front to back, take the first valid one
				for (ClientWorld::CollisionInfoVector::const_iterator it = results.begin(); it != results.end(); ++it)
				{
					Object const * const hitObject = it->getObject();
					if (!hitObject || hitObject == obj) continue;
					
					ClientObject * const clientObj = const_cast<Object*>(hitObject)->asClientObject();
					if (!clientObj) continue;
					
					NetworkId const & newId = clientObj->getNetworkId();
					if (newId.isValid())
					{
						// Apply current changes first
						applyMovementToServer();
						
						// Switch to new object
						s_selectedFurniture = newId;
						s_originalTransform = clientObj->getTransform_o2p();
						s_cachedObjectRadius = getObjectRadius(clientObj);
						resetDeltas();
						resetGizmoState();
						
						CuiSystemMessageManager::sendFakeSystemMessage(Unicode::narrowToWide("New object selected."));
						return true;
					}
				}
			}
		}
	}
	
	// Handle mouse up
	if (!leftButton && s_isDragging)
	{
		s_isDragging = false;
		s_activeComponent = GC_None;
	}
	
	// Update hover state (with caching to reduce lag)
	if (x != s_lastHitTestX || y != s_lastHitTestY)
	{
		s_cachedHitResult = hitTestGizmo(x, y, camera, objectPosition, gizmoSize);
		s_lastHitTestX = x;
		s_lastHitTestY = y;
	}
	s_hoveredComponent = s_cachedHitResult;
	s_lastMouseX = x;
	s_lastMouseY = y;
	
	return s_hoveredComponent != GC_None || s_isDragging;
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
	UNREF(x);
	UNREF(y);
	
	if (!s_active || !s_isDragging || s_activeComponent == GC_None) return false;
	
	Camera const * const camera = Game::getCamera();
	if (!camera) return false;
	
	float const sensitivity = s_fineMode ? 0.005f : 0.05f;
	
	// Get camera right and up vectors for screen-space movement
	Vector const cameraRight = camera->getObjectFrameI_w();
	Vector const cameraUp = camera->getObjectFrameJ_w();
	Vector const cameraForward = camera->getObjectFrameK_w();
	
	switch (s_activeComponent)
	{
	case GC_AxisX:
		{
			// Project camera right onto X axis to determine drag direction
			float const dot = cameraRight.x;
			float const sign = (dot >= 0.0f) ? 1.0f : -1.0f;
			moveX(static_cast<float>(deltaX) * sensitivity * sign);
		}
		break;
	case GC_AxisY:
		// Y is always vertical, so use screen Y
		moveY(static_cast<float>(-deltaY) * sensitivity);
		break;
	case GC_AxisZ:
		{
			// Project camera right onto Z axis to determine drag direction
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
			// Move along XZ plane based on screen movement
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
		break;
	case GC_RotatePitch:
		rotatePitch(static_cast<float>(deltaY) * sensitivity * 20.0f);
		break;
	case GC_RotateRoll:
		rotateRoll(static_cast<float>(deltaX) * sensitivity * 20.0f);
		break;
	case GC_Sphere:
		{
			// Free drag on ground plane (XZ)
			Vector screenMove = cameraRight * static_cast<float>(deltaX) * sensitivity;
			screenMove -= cameraForward * static_cast<float>(deltaY) * sensitivity;
			moveX(screenMove.x);
			moveZ(screenMove.z);
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
	
	// Set up 3D rendering shader (like GodClient does)
	Graphics::setStaticShader(ShaderTemplateList::get3dVertexColorZStaticShader());
	
	// Get world transform and calculate gizmo size
	Transform const & objTransform = obj->getTransform_o2w();
	float const gizmoSize = s_cachedObjectRadius * s_gizmoScale * 2.0f;
	float const lineThickness = gizmoSize * 0.02f; // For thicker lines
	
	// Set up transform for 3D rendering at object location
	Graphics::setObjectToWorldTransformAndScale(objTransform, obj->getScale());
	
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
		
		// X axis (Red)
		drawThickLine(Vector::zero, Vector(gizmoSize, 0.0f, 0.0f), xColor);
		// Y axis (Green)
		drawThickLine(Vector::zero, Vector(0.0f, gizmoSize, 0.0f), yColor);
		// Z axis (Blue)
		drawThickLine(Vector::zero, Vector(0.0f, 0.0f, gizmoSize), zColor);
		
		// Draw larger axis endpoint cubes (bigger handles)
		float const cubeSize = gizmoSize * 0.18f;
		Graphics::drawCube(Vector(gizmoSize, 0.0f, 0.0f), cubeSize, xColor);
		Graphics::drawCube(Vector(0.0f, gizmoSize, 0.0f), cubeSize, yColor);
		Graphics::drawCube(Vector(0.0f, 0.0f, gizmoSize), cubeSize, zColor);
		
		// Draw center indicator for translate mode (bigger)
		bool const centerH = (s_hoveredComponent == GC_Sphere || s_activeComponent == GC_Sphere);
		VectorArgb const centerColor = centerH ? VectorArgb(1.0f, 1.0f, 1.0f, 1.0f) : VectorArgb(1.0f, 0.9f, 0.9f, 0.5f);
		Graphics::drawOctahedron(Vector::zero, gizmoSize * 0.15f, centerColor);
	}
	else if (s_gizmoMode == GM_Rotate)
	{
		// Draw 3 rotation circles (like a sphere/gimbal)
		bool const yawH = (s_hoveredComponent == GC_RotateYaw || s_activeComponent == GC_RotateYaw);
		bool const pitchH = (s_hoveredComponent == GC_RotatePitch || s_activeComponent == GC_RotatePitch);
		bool const rollH = (s_hoveredComponent == GC_RotateRoll || s_activeComponent == GC_RotateRoll);
		
		float const radius = gizmoSize * 1.2f;
		int const segments = 64;
		
		// Colors - brighter when hovered
		VectorArgb const yawColor = yawH ? VectorArgb(1.0f, 0.6f, 1.0f, 0.6f) : VectorArgb(1.0f, 0.3f, 0.9f, 0.3f);
		VectorArgb const pitchColor = pitchH ? VectorArgb(1.0f, 1.0f, 0.6f, 0.6f) : VectorArgb(1.0f, 0.9f, 0.3f, 0.3f);
		VectorArgb const rollColor = rollH ? VectorArgb(1.0f, 0.6f, 0.6f, 1.0f) : VectorArgb(1.0f, 0.3f, 0.3f, 0.9f);
		
		// Draw 3 circles forming a sphere-like gimbal
		// Each circle is drawn multiple times with slight offsets for thickness
		float const thickness = gizmoSize * 0.015f;
		
		// Yaw circle (Green - horizontal plane, around Y axis)
		for (float offset = -thickness; offset <= thickness; offset += thickness)
		{
			Graphics::drawCircle(Vector(0.0f, offset, 0.0f), Vector::unitY, radius, segments, yawColor);
		}
		
		// Pitch circle (Red - vertical plane front/back, around X axis)
		for (float offset = -thickness; offset <= thickness; offset += thickness)
		{
			Graphics::drawCircle(Vector(offset, 0.0f, 0.0f), Vector::unitX, radius, segments, pitchColor);
		}
		
		// Roll circle (Blue - vertical plane left/right, around Z axis)
		for (float offset = -thickness; offset <= thickness; offset += thickness)
		{
			Graphics::drawCircle(Vector(0.0f, 0.0f, offset), Vector::unitZ, radius, segments, rollColor);
		}
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

CuiFurnitureMovementManager::GizmoComponent CuiFurnitureMovementManager::hitTestGizmo(int screenX, int screenY, Camera const * camera, Vector const & gizmoPosition, float gizmoSize)
{
	if (!camera) return GC_None;
	
	// Bigger hit threshold for easier clicking
	float const threshold = 30.0f; // pixels
	float const handleThreshold = 40.0f; // even bigger for handles
	
	if (s_gizmoMode == GM_Translate)
	{
		// Test axis endpoint handles first (bigger hit area)
		Vector xEnd = gizmoPosition + Vector(gizmoSize, 0.0f, 0.0f);
		Vector yEnd = gizmoPosition + Vector(0.0f, gizmoSize, 0.0f);
		Vector zEnd = gizmoPosition + Vector(0.0f, 0.0f, gizmoSize);
		
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
		// Test hollow discs - use outer radius for hit testing
		float const outerRadius = gizmoSize * 1.2f;
		
		if (hitTestCircle(screenX, screenY, camera, gizmoPosition, Vector::unitY, outerRadius, threshold))
			return GC_RotateYaw;
		if (hitTestCircle(screenX, screenY, camera, gizmoPosition, Vector::unitX, outerRadius * 0.95f, threshold))
			return GC_RotatePitch;
		if (hitTestCircle(screenX, screenY, camera, gizmoPosition, Vector::unitZ, outerRadius * 0.9f, threshold))
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
	
	NetworkId const & targetId = s_selectedFurniture;
	char buffer[256];
	float const epsilon = 0.001f;
	
	if (std::fabs(s_positionDelta.z) > epsilon)
	{
		if (s_positionDelta.z > 0)
			snprintf(buffer, sizeof(buffer), "forward %f", s_positionDelta.z);
		else
			snprintf(buffer, sizeof(buffer), "back %f", -s_positionDelta.z);
		ClientCommandQueue::enqueueCommand(s_hashMoveFurniture, targetId, Unicode::narrowToWide(buffer));
	}
	
	if (std::fabs(s_positionDelta.x) > epsilon)
	{
		if (s_positionDelta.x > 0)
			snprintf(buffer, sizeof(buffer), "right %f", s_positionDelta.x);
		else
			snprintf(buffer, sizeof(buffer), "left %f", -s_positionDelta.x);
		ClientCommandQueue::enqueueCommand(s_hashMoveFurniture, targetId, Unicode::narrowToWide(buffer));
	}
	
	if (std::fabs(s_positionDelta.y) > epsilon)
	{
		if (s_positionDelta.y > 0)
			snprintf(buffer, sizeof(buffer), "up %f", s_positionDelta.y);
		else
			snprintf(buffer, sizeof(buffer), "down %f", -s_positionDelta.y);
		ClientCommandQueue::enqueueCommand(s_hashMoveFurniture, targetId, Unicode::narrowToWide(buffer));
	}
	
	if (std::fabs(s_yawDelta) > epsilon)
	{
		snprintf(buffer, sizeof(buffer), "yaw %f", s_yawDelta);
		ClientCommandQueue::enqueueCommand(s_hashRotateFurniture, targetId, Unicode::narrowToWide(buffer));
	}
	
	if (std::fabs(s_pitchDelta) > epsilon)
	{
		snprintf(buffer, sizeof(buffer), "pitch %f", s_pitchDelta);
		ClientCommandQueue::enqueueCommand(s_hashRotateFurniture, targetId, Unicode::narrowToWide(buffer));
	}
	
	if (std::fabs(s_rollDelta) > epsilon)
	{
		snprintf(buffer, sizeof(buffer), "roll %f", s_rollDelta);
		ClientCommandQueue::enqueueCommand(s_hashRotateFurniture, targetId, Unicode::narrowToWide(buffer));
	}
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
	
	Vector position = newTransform.getPosition_p();
	position += s_positionDelta;
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
	s_previousHudOpacity = CuiPreferences::getHudOpacity();
	CuiPreferences::setHudOpacity(0.0f);
	
	// Switch to free camera mode
	gs->setView(GroundScene::CI_free);
	
	// Enable pointer input so mouse cursor is visible
	CuiManager::setPointerToggledOn(true);
	
	s_decoratorCameraActive = true;
	
	CuiSystemMessageManager::sendFakeSystemMessage(Unicode::narrowToWide("Decorator camera enabled. WASD to move, Right-click to pan, R to apply, Esc to exit."));
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
	CuiPreferences::setHudOpacity(s_previousHudOpacity);
	
	// Disable pointer toggle
	CuiManager::setPointerToggledOn(false);
	
	s_decoratorCameraActive = false;
	s_previousCameraMode = -1;
	
	CuiSystemMessageManager::sendFakeSystemMessage(Unicode::narrowToWide("Decorator camera disabled."));
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