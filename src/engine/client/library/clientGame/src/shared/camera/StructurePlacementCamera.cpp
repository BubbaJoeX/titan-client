//===================================================================
//
// StructurePlacementCamera.cpp
// asommers 
//
// copyright 2002, sont online entertainment
//
//===================================================================

#include "clientGame/FirstClientGame.h"
#include "clientGame/StructurePlacementCamera.h"
#include "clientGame/StructurePlacementVisualState.h"

#include "clientGame/ClientWorld.h"
#include "clientGame/CreatureObject.h"
#include "clientGraphics/Graphics.h"
#include "clientGraphics/Light.h"
#include "clientGraphics/Shader.h"
#include "clientGraphics/ShaderPrimitiveSorter.h"
#include "clientGraphics/ShaderTemplateList.h"
#include "clientObject/ShadowManager.h"
#include "clientTerrain/ClientProceduralTerrainAppearance.h"
#include "sharedCollision/CollisionInfo.h"
#include "sharedDebug/DebugFlags.h"
#include "sharedFoundation/GameControllerMessage.h"
#include "sharedFoundation/MessageQueue.h"
#include "sharedObject/AlterResult.h"
#include "sharedObject/CellProperty.h"
#include "sharedObject/LotManager.h"
#include "sharedObject/StructureFootprint.h"
#include "sharedMath/VectorArgb.h"
#include "sharedTerrain/TerrainObject.h"

#include <algorithm>
#include <cmath>

//===================================================================
// StructurePlacementCameraNamespace
//===================================================================

namespace StructurePlacementCameraNamespace
{
	//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

	const float ks_defaultPlacementViewDistance = 50.f;
	float const ks_minPlacementViewDistance     = 22.f;
	float const ks_maxPlacementViewDistance    = 175.f;
	float const ks_mouseWheelToViewDistance    = 1.f / 5200.f;
	const float ms_glassWallRadius             = 32.f;
	bool        ms_renderLotManager = true;
	bool        ms_alwaysAllowStructurePlacement;

	//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

	void drawLot (const bool canPlace, const int x, const int z, const float chunkWidthInMeters, const StructureFootprint* const structureFootprint, const int sx, const int sz, const RotationType rotation, const Shader* const allowedFootprintShader, const Shader* const disallowedFootprintShader, const bool showDirection) 
	{
		TerrainObject* const terrainObject = TerrainObject::getInstance ();
		NOT_NULL (terrainObject);

		const float x0 = (x + 0) * chunkWidthInMeters + 0.05f;
		const float x1 = (x + 1) * chunkWidthInMeters + 0.05f;
		const float z0 = (z + 0) * chunkWidthInMeters + 0.05f;
		const float z1 = (z + 1) * chunkWidthInMeters + 0.05f;

		Vector corners [4];
		
		corners [0].set (x0, 0.f, z0);
		IGNORE_RETURN (terrainObject->getHeight (corners [0], corners [0].y));

		corners [1].set (x0, 0.f, z1);
		IGNORE_RETURN (terrainObject->getHeight (corners [1], corners [1].y));
		
		corners [2].set (x1, 0.f, z1);
		IGNORE_RETURN (terrainObject->getHeight (corners [2], corners [2].y));
		
		corners [3].set (x1, 0.f, z0);
		IGNORE_RETURN (terrainObject->getHeight (corners [3], corners [3].y));

		//-- draw footprint 
		{
			const Shader* shader = 0;

			switch (structureFootprint->getLotType (sx, sz))
			{
			default:
			case LT_nothing:
				break;

			case LT_hard:
				shader = canPlace ? allowedFootprintShader : disallowedFootprintShader;
				break;

			case LT_structure:
				shader = canPlace ? allowedFootprintShader : disallowedFootprintShader;
				break;
			}

			if (shader)
			{
				ClientProceduralTerrainAppearance* const cpta = dynamic_cast<ClientProceduralTerrainAppearance*> (terrainObject->getAppearance ());
				if (cpta)
					cpta->setChunkLotShader (x, z, shader);
			}
		}

		//-- draw border
		{
			bool top    = false;
			bool bottom = false;
			bool left   = false;
			bool right  = false;
			structureFootprint->getBorder (sx, sz, top, bottom, left, right);

			Graphics::setStaticShader (ShaderTemplateList::get3dVertexColorStaticShader ());

			switch (rotation)
			{
			case RT_0:
				{
					if (top)
						Graphics::drawLine (corners [1], corners [2], VectorArgb::solidWhite);

					if (bottom)
						Graphics::drawLine (corners [0], corners [3], VectorArgb::solidWhite);

					if (left)
						Graphics::drawLine (corners [1], corners [0], VectorArgb::solidWhite);

					if (right)
						Graphics::drawLine (corners [2], corners [3], VectorArgb::solidWhite);
				}
				break;

			case RT_90:
				{
					if (top)
						Graphics::drawLine (corners [2], corners [3], VectorArgb::solidWhite);

					if (bottom)
						Graphics::drawLine (corners [1], corners [0], VectorArgb::solidWhite);

					if (left)
						Graphics::drawLine (corners [2], corners [1], VectorArgb::solidWhite);

					if (right)
						Graphics::drawLine (corners [3], corners [0], VectorArgb::solidWhite);
				}
				break;

			case RT_180:
				{
					if (top)
						Graphics::drawLine (corners [3], corners [0], VectorArgb::solidWhite);

					if (bottom)
						Graphics::drawLine (corners [2], corners [1], VectorArgb::solidWhite);

					if (left)
						Graphics::drawLine (corners [3], corners [2], VectorArgb::solidWhite);

					if (right)
						Graphics::drawLine (corners [0], corners [1], VectorArgb::solidWhite);
				}
				break;

			case RT_270:
				{
					if (top)
						Graphics::drawLine (corners [0], corners [1], VectorArgb::solidWhite);

					if (bottom)
						Graphics::drawLine (corners [3], corners [2], VectorArgb::solidWhite);

					if (left)
						Graphics::drawLine (corners [0], corners [3], VectorArgb::solidWhite);

					if (right)
						Graphics::drawLine (corners [1], corners [2], VectorArgb::solidWhite);
				}
				break;
			}
		}

		//-- Indicate FRONT toward model north (+local Z for RT_0) - aligns with doorway / playable facing.
		{
			const float x_0   =  static_cast<float> (x)         * chunkWidthInMeters;
			const float x_1_2 = (static_cast<float> (x) + 0.5f) * chunkWidthInMeters;
			const float x_1   = (static_cast<float> (x) + 1.f)  * chunkWidthInMeters;
			const float z_0   =  static_cast<float> (z)         * chunkWidthInMeters;
			const float z_1_2 = (static_cast<float> (z) + 0.5f) * chunkWidthInMeters;
			const float z_1   = (static_cast<float> (z) + 1.f)  * chunkWidthInMeters;

			Vector direction [2];
			direction [0].set (x_1_2, Vector::linearInterpolate (corners [0], corners [2], 0.5f).y, z_1_2);

			switch (rotation)
			{
			case RT_0:
				direction [1].set (x_1_2, Vector::linearInterpolate (corners [1], corners [2], 0.5f).y, z_1);
				break;

			case RT_90:
				direction [1].set (x_1, Vector::linearInterpolate (corners [2], corners [3], 0.5f).y, z_1_2);
				break;

			case RT_180:
				direction [1].set (x_1_2, Vector::linearInterpolate (corners [0], corners [3], 0.5f).y, z_0);
				break;

			case RT_270:
				direction [1].set (x_0, Vector::linearInterpolate (corners [0], corners [1], 0.5f).y, z_1_2);
				break;
			}

			VectorArgb const arrowColor =
				showDirection ? VectorArgb (1.f, 1.f, 0.92f, 0.09f)
				              : VectorArgb::solidWhite;
			float const head = chunkWidthInMeters * (showDirection ? 0.12f : 0.08f);

			Vector const mid = Vector::linearInterpolate (direction [0], direction [1], showDirection ? 0.92f : 0.82f);

			Vector perpFlat (direction [1]);
			perpFlat -= direction [0];
			perpFlat.y = 0.f;
			perpFlat.normalize ();
			if (perpFlat.magnitudeSquared () < CONST_REAL (0.001))
				perpFlat.set (1.f, 0.f, 0.f);

			Vector side = Vector::unitY.cross (perpFlat);
			side.normalize ();
			Vector headA = mid + side * head;
			Vector headB = mid - side * head;

			Graphics::drawLine (direction [0], direction [1], arrowColor);
			Graphics::drawLine (direction [1], headA, arrowColor);
			Graphics::drawLine (direction [1], headB, arrowColor);
		}
	}

	//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
}

using namespace StructurePlacementCameraNamespace;

//===================================================================
// PUBLIC StructurePlacementCamera
//===================================================================

StructurePlacementCamera::StructurePlacementCamera () : 
	GameCamera (),
	m_queue (0),
	m_target (),
	m_pivot (),
	m_placementViewDistance (ks_defaultPlacementViewDistance),
	m_mouseX (Graphics::getCurrentRenderTargetWidth () >> 1),
	m_mouseY (Graphics::getCurrentRenderTargetHeight () >> 1),
	m_structureFootprint (0),
	m_structureObject (0),
	m_createLocation (),
	m_rotationType (RT_0),
	m_lodThreshold (0.f),
	m_highLodThreshold (0.f),
	m_fov (PI_OVER_2),
	m_light (new Light (Light::T_ambient, VectorArgb::solidWhite)),
	m_lotOccupiedShader (ShaderTemplateList::fetchShader ("shader/placement_yellow.sht")),
	m_allowedFootprintShader (ShaderTemplateList::fetchShader ("shader/placement_green.sht")),
	m_disallowedFootprintShader (ShaderTemplateList::fetchShader ("shader/placement_red.sht"))
{
	DebugFlags::registerFlag (ms_renderLotManager, "ClientGame", "renderLotManager");
	DebugFlags::registerFlag (ms_alwaysAllowStructurePlacement, "ClientGame", "alwaysAllowStructurePlacement");

	int i;
	for (i = 0; i < K_COUNT; ++i)
		m_keys [i] = false;
}

//-------------------------------------------------------------------

StructurePlacementCamera::~StructurePlacementCamera ()
{
	m_queue  = 0;
	m_target = 0;
	m_structureFootprint = 0;
	m_structureObject = 0;

	delete m_light;

	if (m_lotOccupiedShader)
		m_lotOccupiedShader->release ();
	if (m_allowedFootprintShader)
		m_allowedFootprintShader->release ();
	if (m_disallowedFootprintShader)
		m_disallowedFootprintShader->release ();

	DebugFlags::unregisterFlag (ms_renderLotManager);
	DebugFlags::unregisterFlag (ms_alwaysAllowStructurePlacement);
}

//-------------------------------------------------------------------

void StructurePlacementCamera::setActive (bool active)
{
	//-- activating
	if (active && !isActive ())
	{
		NOT_NULL (m_target);
		m_pivot = m_target->getPosition_w ();

		TerrainObject* const terrainObject = TerrainObject::getInstance ();
		if (terrainObject)
		{
			m_lodThreshold = terrainObject->getLevelOfDetailThreshold ();
			terrainObject->setLevelOfDetailThreshold (1.f);

			m_highLodThreshold = terrainObject->getHighLevelOfDetailThreshold ();
			terrainObject->setHighLevelOfDetailThreshold (12.f);
		}

		m_rotationType = RT_0;

		m_placementViewDistance = ks_defaultPlacementViewDistance;

		m_fov = getHorizontalFieldOfView ();
		setHorizontalFieldOfView (PI_OVER_2);
	}
	else
		//-- deactivating
		if (!active && isActive ())
		{
			TerrainObject* const terrainObject = TerrainObject::getInstance ();
			if (terrainObject)
			{
				ClientProceduralTerrainAppearance* const cpta = dynamic_cast<ClientProceduralTerrainAppearance*> (terrainObject->getAppearance ());
				if (cpta)
					cpta->clearLotShaders ();

				if (m_lodThreshold > 0.f)
					terrainObject->setLevelOfDetailThreshold (m_lodThreshold);

				if (m_highLodThreshold > 0.f)
					terrainObject->setHighLevelOfDetailThreshold (m_highLodThreshold);
			}

			setHorizontalFieldOfView (m_fov);

			Graphics::setFillMode (GFM_solid);
		}

	GameCamera::setActive (active);
}

//-------------------------------------------------------------------

float StructurePlacementCamera::alter (float elapsedTime)
{
	if (!isActive ())
	{
		// @todo consider returning no update, changing setActive to add to alter scheduler.
		return AlterResult::cms_alterNextFrame;
	}

	if (!m_target)
		return AlterResult::cms_alterNextFrame;

	NOT_NULL (m_queue);

	int i;
	for (i = 0; i < K_COUNT; ++i)
		m_keys [i] = false;

	//-- handle camera specific messages
	float mouseWheelAccum = 0.f;
	for (i = 0; i < m_queue->getNumberOfMessages (); i++)
	{
		int  message;
		float value;

		m_queue->getMessage (i, &message, &value);

		switch (message)
		{
		case CM_walk:         m_keys [K_up]     = true;  break;
		case CM_down:         m_keys [K_down]   = true;  break;
		case CM_left:         m_keys [K_left]   = true;  break;
		case CM_right:        m_keys [K_right]  = true;  break;
		case CM_mouseWheel:
		case CM_cameraPivotZoom:
			mouseWheelAccum += value;
			break;
		default:
			break;
		}
	}

	if (mouseWheelAccum != CONST_REAL (0))
	{
		float const scale = std::sqrt (std::max (CONST_REAL (10), m_placementViewDistance));
		m_placementViewDistance += mouseWheelAccum * ks_mouseWheelToViewDistance * scale;
		m_placementViewDistance  = std::max (ks_minPlacementViewDistance, std::min (ks_maxPlacementViewDistance, m_placementViewDistance));
	}
	
	const Vector x = Vector::unitX * 2.f;
	const Vector z = Vector::unitZ * 2.f;
	const Vector targetPosition = m_target->getPosition_w ();

	if ((m_pivot.z + z.z < targetPosition.z + ms_glassWallRadius) && (m_keys [K_up] || m_mouseY < 1))
		m_pivot += z;
	
	if ((m_pivot.z - z.z > targetPosition.z - ms_glassWallRadius) && (m_keys [K_down] || m_mouseY > Graphics::getCurrentRenderTargetHeight () - 3))
		m_pivot -= z;
		
	if ((m_pivot.x - x.x > targetPosition.x - ms_glassWallRadius) && (m_keys [K_left] || m_mouseX < 1))
		m_pivot -= x;
	
	if ((m_pivot.x + x.x < targetPosition.x + ms_glassWallRadius) && (m_keys [K_right] || m_mouseX > Graphics::getCurrentRenderTargetWidth () - 3))
		m_pivot += x;

	//-- collide with terrain
	const TerrainObject* const terrain = TerrainObject::getConstInstance ();
	NOT_NULL (terrain);

	IGNORE_RETURN (terrain->getHeight (m_pivot, m_pivot.y));
			
	Transform t;
	t.setPosition_p (m_pivot);
	t.pitch_l (PI_OVER_2 - (PI / 32.f));
	t.move_l (Vector (0.f, 0.f, -m_placementViewDistance));

	setTransform_o2p (t);

	StructurePlacementVisualState::pollChordEachFrame ();

	return GameCamera::alter (elapsedTime);
}

//-------------------------------------------------------------------

void StructurePlacementCamera::setMessageQueue (const MessageQueue* newQueue)
{
	m_queue = newQueue;
}

//-------------------------------------------------------------------

void StructurePlacementCamera::setTarget (const Object* target)
{
	NOT_NULL (target);

	m_target = target;
}

//-------------------------------------------------------------------

void StructurePlacementCamera::setMouseCoordinates (int x, int y)
{
	m_mouseX = x;
	m_mouseY = y;
}

//-------------------------------------------------------------------

void StructurePlacementCamera::setStructureFootprint (const StructureFootprint* structureFootprint)
{
	NOT_NULL (structureFootprint);
	m_structureFootprint = structureFootprint;
}

//-------------------------------------------------------------------

void StructurePlacementCamera::setStructureObject (Object* structureObject)
{
	m_structureObject = structureObject;
}

//-------------------------------------------------------------------

void StructurePlacementCamera::setRotation (RotationType rotationType)
{
	m_rotationType = rotationType;

	//-- rotate structure object
	float rotation = 0.f;
	switch (m_rotationType)
	{
	default:
	case RT_0:    rotation =  0.f;        break;
	case RT_90:   rotation =  PI_OVER_2;  break;
	case RT_180:  rotation =  PI;         break;
	case RT_270:  rotation = -PI_OVER_2;  break;
	}

	NOT_NULL (m_structureObject);
	m_structureObject->resetRotate_o2p ();
	m_structureObject->yaw_o (rotation);
}

//-------------------------------------------------------------------

const Vector& StructurePlacementCamera::getCreateLocation () const
{
	return m_createLocation;
}

//===================================================================
// PROTECTED StructurePlacementCamera
//===================================================================

void StructurePlacementCamera::drawScene () const
{
	GameCamera::drawScene ();

	//-- draw the object
	NOT_NULL (m_structureObject);
	const GlFillMode fillMode = Graphics::getFillMode ();
	Graphics::setFillMode (GFM_wire);
	const bool shadowsEnabled = ShadowManager::getEnabled ();
	ShadowManager::setEnabled (false);
	ShaderPrimitiveSorter::pushCell (*CellProperty::getWorldCellProperty());
	ShaderPrimitiveSorter::enableLight (*m_light);
	Appearance *structureAppearance = m_structureObject->getAppearance();
	structureAppearance->setTransform_w(m_structureObject->getTransform_o2w());
	structureAppearance->objectListCameraRender();
	ShaderPrimitiveSorter::disableLight (*m_light);
	ShaderPrimitiveSorter::popCell ();
	Graphics::setFillMode (fillMode);
	ShadowManager::setEnabled (shadowsEnabled);

	//-- draw a square around the mouse coordinates
	TerrainObject* const terrainObject = TerrainObject::getInstance ();
	NOT_NULL (terrainObject);

	const float  chunkWidthInMeters = terrainObject->getChunkWidthInMeters ();
	DEBUG_FATAL (chunkWidthInMeters == 0, ("chunkWidthInMeters == 0"));

	const Vector start_w = getPosition_w ();
	const Vector end_w   = ((rotateTranslate_o2w (reverseProjectInScreenSpace (m_mouseX, m_mouseY)) - start_w) * 10000.f) + start_w;

	CollisionInfo result;
	if (terrainObject->collide (start_w, end_w, result))
	{
		NOT_NULL (m_structureFootprint);

		const int x = terrainObject->calculateChunkX (result.getPoint());
		const int z = terrainObject->calculateChunkZ (result.getPoint());

		const LotManager* const lotManager = ClientWorld::getConstLotManager ();
		NOT_NULL (lotManager);

		float height_w = 0.f;
		const bool canPlace = ms_alwaysAllowStructurePlacement || lotManager->canPlace (m_structureFootprint, x, z, m_rotationType, height_w, false);

		if (ms_renderLotManager)
		{
			ClientProceduralTerrainAppearance* const cpta = dynamic_cast<ClientProceduralTerrainAppearance*> (terrainObject->getAppearance ());
			if (cpta)
			{
				int i;
				int j;
				for (j = z - 15; j < z + 15; ++j)
					for (i = x - 15; i < x + 15; ++i)
						if (m_lotOccupiedShader && (lotManager->getLotType (i, j) == LT_illegal || terrainObject->getWater (i, j)))
							cpta->setChunkLotShader (i, j, m_lotOccupiedShader);
			}
		}

		{
			Graphics::setObjectToWorldTransformAndScale (Transform::identity, Vector::xyz111);

			//-- which way are we rotated?
			switch (m_rotationType)
			{
			case RT_0:
				{
					const int pivotX = m_structureFootprint->getPivotX ();
					const int pivotZ = m_structureFootprint->getPivotZ ();

					int sj = m_structureFootprint->getHeight () - 1;
					int j;
					for (j = 0; j < m_structureFootprint->getHeight (); ++j)
					{
						int si = 0;
						int i;
						for (i = 0; i < m_structureFootprint->getWidth (); ++i)
						{
							drawLot (canPlace, (x - pivotX) + i, (z - pivotZ) + j, chunkWidthInMeters, m_structureFootprint, si, sj, m_rotationType, m_allowedFootprintShader, m_disallowedFootprintShader, pivotX == i && pivotZ == j);
							++si;
						}

						--sj;
					}
				}
				break;

			case RT_90:
				{
					const int pivotX = m_structureFootprint->getPivotZ ();
					const int pivotZ = m_structureFootprint->getPivotX ();

					int sj = m_structureFootprint->getWidth () - 1;
					int j;
					for (j = 0; j < m_structureFootprint->getWidth (); ++j)
					{
						int si = m_structureFootprint->getHeight () - 1;
						int i;
						for (i = 0; i < m_structureFootprint->getHeight (); ++i)
						{
							drawLot (canPlace, (x - pivotX) + i, (z - (m_structureFootprint->getWidth () - 1 - pivotZ)) + j, chunkWidthInMeters, m_structureFootprint, sj, si, m_rotationType, m_allowedFootprintShader, m_disallowedFootprintShader, pivotX == i && (m_structureFootprint->getWidth () - 1 - pivotZ) == j);
							--si;
						}

						--sj;
					}
				}
				break;

			case RT_180:
				{
					const int pivotX = m_structureFootprint->getPivotX ();
					const int pivotZ = (m_structureFootprint->getHeight () - 1) - m_structureFootprint->getPivotZ ();

					int sj = 0;
					int j;
					for (j = 0; j < m_structureFootprint->getHeight (); ++j)
					{
						int si = m_structureFootprint->getWidth () - 1;
						int i;
						for (i = 0; i < m_structureFootprint->getWidth (); ++i)
						{
							drawLot (canPlace, (x - (m_structureFootprint->getWidth () - 1 - pivotX)) + i, (z - pivotZ) + j, chunkWidthInMeters, m_structureFootprint, si, sj, m_rotationType, m_allowedFootprintShader, m_disallowedFootprintShader, (m_structureFootprint->getWidth () - 1 - pivotX) == i && pivotZ == j);
							--si;
						}

						++sj;
					}
				}
				break;

			case RT_270:
				{
					const int pivotX = (m_structureFootprint->getHeight () - 1) - m_structureFootprint->getPivotZ ();
					const int pivotZ = m_structureFootprint->getPivotX ();

					int sj = 0;
					int j;
					for (j = 0; j < m_structureFootprint->getWidth (); ++j)
					{
						int si = 0;
						int i;
						for (i = 0; i < m_structureFootprint->getHeight (); ++i)
						{
							drawLot (canPlace, (x - pivotX) + i, (z - pivotZ) + j, chunkWidthInMeters, m_structureFootprint, sj, si, m_rotationType, m_allowedFootprintShader, m_disallowedFootprintShader, pivotX == i && pivotZ == j);
							++si;
						}

						++sj;
					}
				}
				break;
			}
		}

		//-- Ground-plane world compass (chunk X,Z); matches UI copy for N/E/S/W.
		{
			Graphics::setStaticShader (ShaderTemplateList::get3dVertexColorStaticShader ());
			Vector pivot = result.getPoint ();
			IGNORE_RETURN (terrainObject->getHeight (pivot, pivot.y));
			pivot.y += chunkWidthInMeters * 0.08f;

			float const limb = chunkWidthInMeters * 0.24f;

			Vector nTip = pivot + Vector (0.f, 0.f, limb);

			Vector sTip = pivot + Vector (0.f, 0.f, -limb);

			Vector eTip = pivot + Vector (limb, 0.f, 0.f);

			Vector wTip = pivot + Vector (-limb, 0.f, 0.f);

			Graphics::drawLine (pivot, nTip, VectorArgb::solidCyan);
			Graphics::drawLine (pivot, eTip, VectorArgb::solidMagenta);
			Graphics::drawLine (pivot, sTip, VectorArgb::solidRed);
			Graphics::drawLine (pivot, wTip, VectorArgb::solidYellow);
		}

		//-- update createLocation (we use 0.495 because 0.5 causes us to miss collisions)
		const float x0 = (x * chunkWidthInMeters) + (chunkWidthInMeters * 0.51f);
		const float z0 = (z * chunkWidthInMeters) + (chunkWidthInMeters * 0.49f);

		m_createLocation.set (x0, 0.f, z0);
		IGNORE_RETURN (terrainObject->getHeight (m_createLocation, m_createLocation.y));

		if (canPlace)
			m_createLocation.y = height_w;

		NOT_NULL (m_structureObject);
		m_structureObject->setPosition_w (m_createLocation);
	}
}

//===================================================================
