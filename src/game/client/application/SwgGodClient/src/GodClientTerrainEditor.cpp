// ======================================================================
//
// GodClientTerrainEditor.cpp
// copyright (c) 2001-2026 Sony Online Entertainment
//
// Real-time terrain editing system for the God Client
//
// ======================================================================

#include "SwgGodClient/FirstSwgGodClient.h"
#include "GodClientTerrainEditor.h"

#include <cstdio>

#include "clientGame/Game.h"
#include "clientGame/FreeCamera.h"
#include "clientGame/GroundScene.h"
#include "clientGraphics/Camera.h"
#include "clientGraphics/DebugPrimitive.h"
#include "clientTerrain/ClientProceduralTerrainAppearance.h"
#include "clientTerrain/CityTerrainLayerManager.h"

#include "sharedFile/Iff.h"
#include "sharedFoundation/Clock.h"
#include "sharedFoundation/ExitChain.h"
#include "sharedImage/Image.h"
#include "sharedMath/Rectangle2d.h"
#include "sharedMath/Transform.h"
#include "sharedMath/VectorArgb.h"
#include "sharedObject/Appearance.h"
#include "sharedObject/AppearanceTemplate.h"
#include "sharedRandom/Random.h"
#include "sharedTerrain/TerrainObject.h"
#include "sharedTerrain/TerrainGenerator.h"
#include "sharedTerrain/ProceduralTerrainAppearanceTemplate.h"
#include "sharedTerrain/AffectorRibbon.h"
#include "sharedTerrain/AffectorRoad.h"
#include "sharedTerrain/AffectorShader.h"
#include "sharedTerrain/AffectorPassable.h"
#include "sharedTerrain/AffectorFloraStatic.h"
#include "sharedTerrain/AffectorEnvironment.h"
#include "sharedTerrain/Boundary.h"
#include "sharedTerrain/ShaderGroup.h"
#include "sharedTerrain/FloraGroup.h"
#include "sharedTerrain/RadialGroup.h"
#include "sharedTerrain/EnvironmentGroup.h"
#include "sharedTerrain/BitmapGroup.h"

#include "MainFrame.h"

#include <cmath>
#include <algorithm>
#include <fstream>

namespace GodClientTerrainEditorNamespace
{
	void addTerrainHeightDebugLineStrip(
		TerrainObject const & terrain,
		Camera const & camera,
		float const x0,
		float const z0,
		float const x1,
		float const z1,
		VectorArgb const & color)
	{
		float const dx = x1 - x0;
		float const dz = z1 - z0;
		float const len = std::sqrt(dx * dx + dz * dz);
		if (len < 0.01f)
			return;

		int const steps = std::max(2, std::min(96, static_cast<int>(std::ceil(len / 6.f))));

		Vector prev(x0, 0.f, z0);
		if (!terrain.getHeight(prev, prev.y))
			return;

		for (int s = 1; s <= steps; ++s)
		{
			float const t = static_cast<float>(s) / static_cast<float>(steps);
			Vector cur(x0 + dx * t, 0.f, z0 + dz * t);
			if (!terrain.getHeight(cur, cur.y))
				continue;
			camera.addDebugPrimitive(
				new Line3dDebugPrimitive(Line3dDebugPrimitive::S_none, Transform::identity, prev, cur, color));
			prev = cur;
		}
	}

	VectorArgb heightBandColor(float const h, float const h0, float const h1)
	{
		float t = 0.5f;
		if (h1 > h0 + 0.001f)
			t = (h - h0) / (h1 - h0);
		else if (h < h0)
			t = 0.f;
		else if (h > h1)
			t = 1.f;
		t = std::max(0.0f, std::min(1.0f, t));
		return VectorArgb(1.0f, t, 0.35f + 0.65f * t, 1.0f - t * 0.85f);
	}
}

using namespace GodClientTerrainEditorNamespace;

// ======================================================================

GodClientTerrainEditor* GodClientTerrainEditor::ms_instance = 0;
const float GodClientTerrainEditor::MIN_MODIFICATION_INTERVAL = 0.012f; // slight headroom above 60Hz
const float GodClientTerrainEditor::REALTIME_INVALIDATION_INTERVAL = 0.025f; // ~40Hz coalesced chunk invalidation while stroking heights

// ======================================================================

void GodClientTerrainEditor::install()
{
	DEBUG_FATAL(ms_instance != 0, ("GodClientTerrainEditor already installed"));
	ms_instance = new GodClientTerrainEditor();
	ExitChain::add(GodClientTerrainEditor::remove, "GodClientTerrainEditor::remove");

	// Register the height modifier callback with the terrain system
	CityTerrainLayerManager::setExternalHeightModifierCallback(&GodClientTerrainEditor::getModifiedHeight);

	// Register shader and flora modifier callbacks
	CityTerrainLayerManager::setExternalShaderModifierCallback(&GodClientTerrainEditor::getModifiedShader);
	CityTerrainLayerManager::setExternalFloraModifierCallback(&GodClientTerrainEditor::getModifiedFlora);
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::remove()
{
	DEBUG_FATAL(ms_instance == 0, ("GodClientTerrainEditor not installed"));

	// Unregister the height modifier callback
	CityTerrainLayerManager::clearExternalHeightModifierCallback();

	// Unregister shader and flora modifier callbacks
	CityTerrainLayerManager::clearExternalShaderModifierCallback();
	CityTerrainLayerManager::clearExternalFloraModifierCallback();

	delete ms_instance;
	ms_instance = 0;
}

// ----------------------------------------------------------------------

GodClientTerrainEditor& GodClientTerrainEditor::getInstance()
{
	DEBUG_FATAL(ms_instance == 0, ("GodClientTerrainEditor not installed"));
	return *ms_instance;
}

// ----------------------------------------------------------------------

bool GodClientTerrainEditor::isInstalled()
{
	return ms_instance != 0;
}

// ----------------------------------------------------------------------

GodClientTerrainEditor::GodClientTerrainEditor() :
	m_toolMode(TM_None),
	m_brushShape(BS_Circle),
	m_falloffType(FT_Smooth),
	m_brushFeather(1.0f),
	m_brushSize(32.0f),
	m_brushStrength(0.5f),
	m_targetHeight(0.0f),
	m_noiseAmplitude(1.0f),
	m_noiseFrequency(0.1f),
	m_selectedShaderFamily(0),
	m_selectedFloraFamily(0),
	m_floraCollidable(false),
	m_floraDensity(1.0f),
	m_brushPreviewEnabled(true),
	m_cursorWorldPosition(Vector::zero),
	m_cursorPositionValid(false),
	m_brushStrokeActive(false),
	m_currentStroke(),
	m_lastStrokeX(0.0f),
	m_lastStrokeZ(0.0f),
	m_heightModifications(),
	m_shaderModifications(),
	m_floraModifications(),
	m_undoStack(),
	m_redoStack(),
	m_hasRegionSelection(false),
	m_regionMinX(0.0f),
	m_regionMinZ(0.0f),
	m_regionMaxX(0.0f),
	m_regionMaxZ(0.0f),
	m_lastModificationTime(-1.0e9f),
	m_lastInvalidationTime(0.0f),
	m_dirtyRegionMinX(0.0f),
	m_dirtyRegionMinZ(0.0f),
	m_dirtyRegionMaxX(0.0f),
	m_dirtyRegionMaxZ(0.0f),
	m_hasDirtyRegion(false),
	m_polylineEditMode(PEM_None),
	m_activePolyline(),
	m_selectedPolylinePoint(-1),
	m_polylineExtent(),
	m_activeEnvironmentZone(),
	m_environmentZoneActive(false),
	m_environmentFamilyId(0),
	m_bitmapStamp(),
	m_bitmapHeightData(),
	m_bitmapShaderData(),
	m_bitmapWidth(0),
	m_bitmapHeight(0),
	m_modifiedBounds(),
	m_hasModifiedBounds(false),
	m_createdLayers()
{
	m_activePolyline.width = 8.0f;
	m_activePolyline.shaderFamilyId = 0;
	m_activePolyline.featherDistance = 4.0f;
	m_activePolyline.hasFixedHeights = false;
	m_activePolyline.isRibbon = false;
	m_activePolyline.name = "New Road";

	m_bitmapStamp.rotation = 0.0f;
	m_bitmapStamp.scale = 1.0f;
	m_bitmapStamp.affectsHeight = true;
	m_bitmapStamp.affectsShader = false;
	m_bitmapStamp.heightScale = 1.0f;
	m_bitmapStamp.shaderFamilyId = 0;
}

// ----------------------------------------------------------------------

GodClientTerrainEditor::~GodClientTerrainEditor()
{
	clearHistory();
	m_heightModifications.clear();
	m_shaderModifications.clear();
	m_floraModifications.clear();
	m_activePolyline.controlPoints.clear();
	m_activeEnvironmentZone.boundaryPoints.clear();
	m_bitmapHeightData.clear();
	m_bitmapShaderData.clear();
	m_createdLayers.clear();
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::setToolMode(ToolMode mode)
{
	m_toolMode = mode;
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::setBrushSize(float size)
{
	m_brushSize = std::max(1.0f, std::min(256.0f, size));
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::setBrushStrength(float strength)
{
	m_brushStrength = std::max(0.01f, std::min(1.0f, strength));
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::setBrushShape(BrushShape shape)
{
	m_brushShape = shape;
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::setFalloffType(FalloffType type)
{
	m_falloffType = type;
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::setBrushFeather(float feather)
{
	m_brushFeather = std::max(0.05f, std::min(1.0f, feather));
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::setTargetHeight(float height)
{
	m_targetHeight = height;
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::setNoiseAmplitude(float amplitude)
{
	m_noiseAmplitude = amplitude;
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::setNoiseFrequency(float frequency)
{
	m_noiseFrequency = frequency;
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::setSelectedShaderFamily(int familyIndex)
{
	m_selectedShaderFamily = familyIndex;
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::setSelectedFloraFamily(int familyIndex)
{
	m_selectedFloraFamily = familyIndex;
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::setFloraCollidable(bool collidable)
{
	m_floraCollidable = collidable;
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::setFloraDensity(float density)
{
	m_floraDensity = std::max(0.0f, std::min(1.0f, density));
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::setBrushPreviewEnabled(bool enabled)
{
	m_brushPreviewEnabled = enabled;
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::setCursorWorldPosition(const Vector& position)
{
	m_cursorWorldPosition = position;
	m_cursorPositionValid = true;
}

// ----------------------------------------------------------------------

bool GodClientTerrainEditor::beginBrushStroke(float worldX, float worldZ)
{
	if (m_toolMode == TM_None)
		return false;

	// Commit any in-progress stroke (e.g. missed mouse-release) before starting anew.
	if (m_brushStrokeActive)
		endBrushStroke();

	m_brushStrokeActive = true;
	m_lastStrokeX = worldX;
	m_lastStrokeZ = worldZ;

	// Reset dirty region tracking for new stroke
	m_hasDirtyRegion = false;
	const float ft = Clock::frameTime();
	// Allow immediate realtime invalidation on the first sample of each stroke (otherwise
	// lastInvalidationTime == frameTime from below suppresses invalidate until REALTIME_* elapses).
	m_lastInvalidationTime = ft - REALTIME_INVALIDATION_INTERVAL - 1.0e-3f;

	// Initialize current stroke
	m_currentStroke.centerX = worldX;
	m_currentStroke.centerZ = worldZ;
	m_currentStroke.radius = m_brushSize;
	m_currentStroke.strength = m_brushStrength;
	m_currentStroke.tool = m_toolMode;

	// Flatten blends toward height at stroke start; absolute target comes from Set Height / UI
	if (m_toolMode == TM_Flatten)
	{
		TerrainObject* const terrainObject = TerrainObject::getInstance();
		if (terrainObject)
		{
			float h = 0.0f;
			Vector const p(worldX, 0.0f, worldZ);
			if (terrainObject->getHeight(p, h))
				m_targetHeight = h;
		}
	}
	m_currentStroke.targetHeight = m_targetHeight;
	m_currentStroke.modifications.clear();

	// Sample initial state for undo
	sampleBrushArea(worldX, worldZ, m_brushSize * 0.5f, m_currentStroke.modifications);

	// Apply the first brush application
	applyBrushAtPoint(worldX, worldZ);

	return true;
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::continueBrushStroke(float worldX, float worldZ)
{
	if (!m_brushStrokeActive)
		return;

	// Check if we've moved enough to warrant a new application
	const float dx = worldX - m_lastStrokeX;
	const float dz = worldZ - m_lastStrokeZ;
	const float distSq = dx * dx + dz * dz;
	
	// Minimum travel before re-sampling (smaller = smoother live feedback while dragging).
	// Height tools use a coarser step to limit CPU; shader/flora/water repaint much cheaper per cell.
	float minFrac = 0.1f;
	if (m_toolMode == TM_PaintShader || m_toolMode == TM_PaintFlora || m_toolMode == TM_PlaceWater)
		minFrac = 0.004f;
	else if (m_toolMode == TM_Raise || m_toolMode == TM_Lower || m_toolMode == TM_Flatten ||
	         m_toolMode == TM_Smooth || m_toolMode == TM_Noise || m_toolMode == TM_SetHeight)
		minFrac = 0.03f;
	const float minDist = std::max(0.05f, m_brushSize * minFrac);
	const float minDistSq = minDist * minDist;
	
	if (distSq >= minDistSq)
	{
		applyBrushAtPoint(worldX, worldZ);
		m_lastStrokeX = worldX;
		m_lastStrokeZ = worldZ;
	}
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::endBrushStroke()
{
	if (!m_brushStrokeActive)
		return;

	m_brushStrokeActive = false;

	// Push stroke to undo stack
	if (!m_currentStroke.modifications.empty())
	{
		m_undoStack.push_back(m_currentStroke);
		
		// Limit undo stack size
		while (static_cast<int>(m_undoStack.size()) > MAX_UNDO_STROKES)
		{
			m_undoStack.erase(m_undoStack.begin());
		}

		// Clear redo stack on new modification
		m_redoStack.clear();
	}

	// Final invalidation of the full dirty region for this stroke
	if (m_hasDirtyRegion)
	{
		TerrainObject* const terrainObject = TerrainObject::getInstance();
		if (terrainObject)
		{
			Rectangle2d extent2d(
				m_dirtyRegionMinX,
				m_dirtyRegionMinZ,
				m_dirtyRegionMaxX,
				m_dirtyRegionMaxZ
			);
			terrainObject->invalidateRegion(extent2d);
		}
		
		// Clear dirty region tracking
		m_hasDirtyRegion = false;
		m_dirtyRegionMinX = 0.0f;
		m_dirtyRegionMinZ = 0.0f;
		m_dirtyRegionMaxX = 0.0f;
		m_dirtyRegionMaxZ = 0.0f;
	}

	// Flush any remaining terrain changes
	flushTerrainChanges();
	nudgeGodClientCameraToRefreshDpvs();
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::applyBrushAtPoint(float worldX, float worldZ)
{
	if (m_toolMode == TM_None)
		return;

	const float currentTime = Clock::frameTime();

	// While dragging a stroke, apply on every sample (no time throttle) so the mesh can track the brush.
	if (!m_brushStrokeActive)
	{
		if (currentTime - m_lastModificationTime < MIN_MODIFICATION_INTERVAL)
			return;
	}
	m_lastModificationTime = currentTime;

	// Apply based on tool mode
	switch (m_toolMode)
	{
		case TM_Raise:
			modifyHeightRaise(worldX, worldZ, m_brushStrength);
			break;

		case TM_Lower:
			modifyHeightLower(worldX, worldZ, m_brushStrength);
			break;

		case TM_Flatten:
			modifyHeightFlatten(worldX, worldZ, m_targetHeight, m_brushStrength);
			break;

		case TM_Smooth:
			modifyHeightSmooth(worldX, worldZ, m_brushStrength);
			break;

		case TM_Noise:
			modifyHeightNoise(worldX, worldZ, m_noiseAmplitude, m_noiseFrequency);
			break;

		case TM_SetHeight:
			modifyHeightSet(worldX, worldZ, m_targetHeight);
			break;

		case TM_PaintShader:
			modifyShaderPaint(worldX, worldZ, m_selectedShaderFamily, m_brushStrength);
			break;

		case TM_PaintFlora:
			modifyFloraPaint(worldX, worldZ, m_selectedFloraFamily, m_floraDensity, m_floraCollidable);
			break;

		case TM_StampBitmap:
			applyBitmapStamp(worldX, worldZ);
			break;

		default:
			break;
	}

	// Track modified region
	expandModifiedBounds(worldX, worldZ, m_brushSize * 0.5f);

	// Accumulate dirty region for deferred invalidation
	const float halfBrush = m_brushSize * 0.5f;
	const float margin = 16.0f;
	const float regionRadius = halfBrush + margin;

	if (!m_hasDirtyRegion)
	{
		m_dirtyRegionMinX = worldX - regionRadius;
		m_dirtyRegionMinZ = worldZ - regionRadius;
		m_dirtyRegionMaxX = worldX + regionRadius;
		m_dirtyRegionMaxZ = worldZ + regionRadius;
		m_hasDirtyRegion = true;
	}
	else
	{
		// Expand dirty region to include new brush area
		if (worldX - regionRadius < m_dirtyRegionMinX)
			m_dirtyRegionMinX = worldX - regionRadius;
		if (worldZ - regionRadius < m_dirtyRegionMinZ)
			m_dirtyRegionMinZ = worldZ - regionRadius;
		if (worldX + regionRadius > m_dirtyRegionMaxX)
			m_dirtyRegionMaxX = worldX + regionRadius;
		if (worldZ + regionRadius > m_dirtyRegionMaxZ)
			m_dirtyRegionMaxZ = worldZ + regionRadius;
	}

	// Height brushing: coalesce chunk invalidations. The procedural generator clears pending requests per
	// invalidate — flooding invalidateRegion() makes distant geometry/streaming drop until rebuild.
	const bool heightStrokeTool =
		m_brushStrokeActive &&
		(m_toolMode == TM_Raise || m_toolMode == TM_Lower || m_toolMode == TM_Flatten ||
		 m_toolMode == TM_Smooth || m_toolMode == TM_Noise || m_toolMode == TM_SetHeight);

	if (heightStrokeTool)
	{
		if (currentTime - m_lastInvalidationTime >= REALTIME_INVALIDATION_INTERVAL && m_hasDirtyRegion)
		{
			TerrainObject* const terrainObject = TerrainObject::getInstance();
			if (terrainObject)
			{
				Rectangle2d const extent2d(
					m_dirtyRegionMinX,
					m_dirtyRegionMinZ,
					m_dirtyRegionMaxX,
					m_dirtyRegionMaxZ);
				terrainObject->invalidateRegion(extent2d);
			}
			m_lastInvalidationTime = currentTime;
		}
	}
	else
	{
		invalidateTerrainRegion(worldX, worldZ, regionRadius);
		m_lastInvalidationTime = currentTime;
	}
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::modifyHeightRaise(float worldX, float worldZ, float strength)
{
	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (!terrainObject)
		return;

	const float halfBrush = m_brushSize * 0.5f;
	const float heightDelta = strength * 0.5f;

	// Iterate at integer grid points for proper interpolation support
	const int minX = static_cast<int>(std::floor(worldX - halfBrush));
	const int maxX = static_cast<int>(std::ceil(worldX + halfBrush));
	const int minZ = static_cast<int>(std::floor(worldZ - halfBrush));
	const int maxZ = static_cast<int>(std::ceil(worldZ + halfBrush));

	for (int iz = minZ; iz <= maxZ; ++iz)
	{
		for (int ix = minX; ix <= maxX; ++ix)
		{
			const float x = static_cast<float>(ix);
			const float z = static_cast<float>(iz);
			const float localX = x - worldX;
			const float localZ = z - worldZ;
			const float effect = calculateBrushEffect(localX, localZ);

			if (effect > 0.0f)
			{
				float originalHeight = 0.0f;
				Vector pos(x, 0.0f, z);
				if (terrainObject->getHeight(pos, originalHeight))
				{
					const float delta = heightDelta * effect;
					
					// Create position key using integer grid coordinates
					const uint64 key = (static_cast<uint64>(ix + 32768) << 32) |
					                   static_cast<uint64>(iz + 32768);

					HeightModification mod;
					mod.worldX = x;
					mod.worldZ = z;
					mod.originalHeight = originalHeight;
					
					// Check if we already have a modification for this point
					HeightModificationMap::iterator it = m_heightModifications.find(key);
					if (it != m_heightModifications.end())
					{
						mod.originalHeight = it->second.originalHeight;
						mod.modifiedHeight = it->second.modifiedHeight + delta;
					}
					else
					{
						mod.modifiedHeight = originalHeight + delta;
					}
					
					mod.timestamp = Clock::frameTime();
					m_heightModifications[key] = mod;
				}
			}
		}
	}
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::modifyHeightLower(float worldX, float worldZ, float strength)
{
	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (!terrainObject)
		return;

	const float halfBrush = m_brushSize * 0.5f;
	const float heightDelta = -strength * 0.5f;

	const int minX = static_cast<int>(std::floor(worldX - halfBrush));
	const int maxX = static_cast<int>(std::ceil(worldX + halfBrush));
	const int minZ = static_cast<int>(std::floor(worldZ - halfBrush));
	const int maxZ = static_cast<int>(std::ceil(worldZ + halfBrush));

	for (int iz = minZ; iz <= maxZ; ++iz)
	{
		for (int ix = minX; ix <= maxX; ++ix)
		{
			const float x = static_cast<float>(ix);
			const float z = static_cast<float>(iz);
			const float localX = x - worldX;
			const float localZ = z - worldZ;
			const float effect = calculateBrushEffect(localX, localZ);

			if (effect > 0.0f)
			{
				float originalHeight = 0.0f;
				Vector pos(x, 0.0f, z);
				if (terrainObject->getHeight(pos, originalHeight))
				{
					const float delta = heightDelta * effect;
					
					const uint64 key = (static_cast<uint64>(ix + 32768) << 32) |
					                   static_cast<uint64>(iz + 32768);

					HeightModification mod;
					mod.worldX = x;
					mod.worldZ = z;
					mod.originalHeight = originalHeight;
					
					HeightModificationMap::iterator it = m_heightModifications.find(key);
					if (it != m_heightModifications.end())
					{
						mod.originalHeight = it->second.originalHeight;
						mod.modifiedHeight = it->second.modifiedHeight + delta;
					}
					else
					{
						mod.modifiedHeight = originalHeight + delta;
					}
					
					mod.timestamp = Clock::frameTime();
					m_heightModifications[key] = mod;
				}
			}
		}
	}
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::modifyHeightFlatten(float worldX, float worldZ, float targetHeight, float strength)
{
	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (!terrainObject)
		return;

	const float halfBrush = m_brushSize * 0.5f;

	const int minX = static_cast<int>(std::floor(worldX - halfBrush));
	const int maxX = static_cast<int>(std::ceil(worldX + halfBrush));
	const int minZ = static_cast<int>(std::floor(worldZ - halfBrush));
	const int maxZ = static_cast<int>(std::ceil(worldZ + halfBrush));

	for (int iz = minZ; iz <= maxZ; ++iz)
	{
		for (int ix = minX; ix <= maxX; ++ix)
		{
			const float x = static_cast<float>(ix);
			const float z = static_cast<float>(iz);
			const float localX = x - worldX;
			const float localZ = z - worldZ;
			const float effect = calculateBrushEffect(localX, localZ);

			if (effect > 0.0f)
			{
				float originalHeight = 0.0f;
				Vector pos(x, 0.0f, z);
				if (terrainObject->getHeight(pos, originalHeight))
				{
					const float blendedHeight = originalHeight + (targetHeight - originalHeight) * effect * strength;
					
					const uint64 key = (static_cast<uint64>(ix + 32768) << 32) |
					                   static_cast<uint64>(iz + 32768);

					HeightModification mod;
					mod.worldX = x;
					mod.worldZ = z;
					
					HeightModificationMap::iterator it = m_heightModifications.find(key);
					if (it != m_heightModifications.end())
					{
						mod.originalHeight = it->second.originalHeight;
						float currentModified = it->second.modifiedHeight;
						mod.modifiedHeight = currentModified + (targetHeight - currentModified) * effect * strength;
					}
					else
					{
						mod.originalHeight = originalHeight;
						mod.modifiedHeight = blendedHeight;
					}
					
					mod.timestamp = Clock::frameTime();
					m_heightModifications[key] = mod;
				}
			}
		}
	}
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::modifyHeightSmooth(float worldX, float worldZ, float strength)
{
	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (!terrainObject)
		return;

	// First pass: calculate average height in the brush area
	const float avgHeight = getAverageHeight(worldX, worldZ, m_brushSize * 0.5f);

	// Second pass: blend towards average
	const float halfBrush = m_brushSize * 0.5f;

	const int minX = static_cast<int>(std::floor(worldX - halfBrush));
	const int maxX = static_cast<int>(std::ceil(worldX + halfBrush));
	const int minZ = static_cast<int>(std::floor(worldZ - halfBrush));
	const int maxZ = static_cast<int>(std::ceil(worldZ + halfBrush));

	for (int iz = minZ; iz <= maxZ; ++iz)
	{
		for (int ix = minX; ix <= maxX; ++ix)
		{
			const float x = static_cast<float>(ix);
			const float z = static_cast<float>(iz);
			const float localX = x - worldX;
			const float localZ = z - worldZ;
			const float effect = calculateBrushEffect(localX, localZ);

			if (effect > 0.0f)
			{
				float originalHeight = 0.0f;
				Vector pos(x, 0.0f, z);
				if (terrainObject->getHeight(pos, originalHeight))
				{
					float currentHeight = originalHeight;
					
					const uint64 key = (static_cast<uint64>(ix + 32768) << 32) |
					                   static_cast<uint64>(iz + 32768);
					
					HeightModificationMap::iterator it = m_heightModifications.find(key);
					if (it != m_heightModifications.end())
					{
						currentHeight = it->second.modifiedHeight;
					}

					const float smoothedHeight = currentHeight + (avgHeight - currentHeight) * effect * strength * 0.3f;
					
					HeightModification mod;
					mod.worldX = x;
					mod.worldZ = z;
					mod.originalHeight = (it != m_heightModifications.end()) ? it->second.originalHeight : originalHeight;
					mod.modifiedHeight = smoothedHeight;
					mod.timestamp = Clock::frameTime();
					
					m_heightModifications[key] = mod;
				}
			}
		}
	}
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::modifyHeightNoise(float worldX, float worldZ, float amplitude, float frequency)
{
	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (!terrainObject)
		return;

	const float halfBrush = m_brushSize * 0.5f;

	const int minX = static_cast<int>(std::floor(worldX - halfBrush));
	const int maxX = static_cast<int>(std::ceil(worldX + halfBrush));
	const int minZ = static_cast<int>(std::floor(worldZ - halfBrush));
	const int maxZ = static_cast<int>(std::ceil(worldZ + halfBrush));

	for (int iz = minZ; iz <= maxZ; ++iz)
	{
		for (int ix = minX; ix <= maxX; ++ix)
		{
			const float x = static_cast<float>(ix);
			const float z = static_cast<float>(iz);
			const float localX = x - worldX;
			const float localZ = z - worldZ;
			const float effect = calculateBrushEffect(localX, localZ);

			if (effect > 0.0f)
			{
				float originalHeight = 0.0f;
				Vector pos(x, 0.0f, z);
				if (terrainObject->getHeight(pos, originalHeight))
				{
					const float noise = generateNoise(x, z, frequency) * amplitude * effect * m_brushStrength;
					
					const uint64 key = (static_cast<uint64>(ix + 32768) << 32) |
					                   static_cast<uint64>(iz + 32768);

					HeightModification mod;
					mod.worldX = x;
					mod.worldZ = z;
					
					HeightModificationMap::iterator it = m_heightModifications.find(key);
					if (it != m_heightModifications.end())
					{
						mod.originalHeight = it->second.originalHeight;
						mod.modifiedHeight = it->second.modifiedHeight + noise;
					}
					else
					{
						mod.originalHeight = originalHeight;
						mod.modifiedHeight = originalHeight + noise;
					}
					
					mod.timestamp = Clock::frameTime();
					m_heightModifications[key] = mod;
				}
			}
		}
	}
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::modifyHeightSet(float worldX, float worldZ, float targetHeight)
{
	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (!terrainObject)
		return;

	const float halfBrush = m_brushSize * 0.5f;

	const int minX = static_cast<int>(std::floor(worldX - halfBrush));
	const int maxX = static_cast<int>(std::ceil(worldX + halfBrush));
	const int minZ = static_cast<int>(std::floor(worldZ - halfBrush));
	const int maxZ = static_cast<int>(std::ceil(worldZ + halfBrush));

	for (int iz = minZ; iz <= maxZ; ++iz)
	{
		for (int ix = minX; ix <= maxX; ++ix)
		{
			const float x = static_cast<float>(ix);
			const float z = static_cast<float>(iz);
			const float localX = x - worldX;
			const float localZ = z - worldZ;
			const float effect = calculateBrushEffect(localX, localZ);

			if (effect > 0.0f)
			{
				float originalHeight = 0.0f;
				Vector pos(x, 0.0f, z);
				if (terrainObject->getHeight(pos, originalHeight))
				{
					const uint64 key = (static_cast<uint64>(ix + 32768) << 32) |
					                   static_cast<uint64>(iz + 32768);

					HeightModification mod;
					mod.worldX = x;
					mod.worldZ = z;
					
					HeightModificationMap::iterator it = m_heightModifications.find(key);
					if (it != m_heightModifications.end())
					{
						mod.originalHeight = it->second.originalHeight;
					}
					else
					{
						mod.originalHeight = originalHeight;
					}
					
					mod.modifiedHeight = targetHeight;
					mod.timestamp = Clock::frameTime();
					
					m_heightModifications[key] = mod;
				}
			}
		}
	}
}

// ----------------------------------------------------------------------

float GodClientTerrainEditor::calculateFalloff(float distance, float radius) const
{
	if (distance >= radius)
		return 0.0f;

	const float normalizedDistance = distance / radius;

	switch (m_falloffType)
	{
		case FT_Linear:
			return 1.0f - normalizedDistance;

		case FT_Smooth:
			{
				const float t = 1.0f - normalizedDistance;
				return t * t * (3.0f - 2.0f * t);
			}

		case FT_Sharp:
			{
				const float t = 1.0f - normalizedDistance;
				return t * t;
			}

		case FT_Flat:
			return 1.0f;

		default:
			return 1.0f - normalizedDistance;
	}
}

// ----------------------------------------------------------------------

float GodClientTerrainEditor::calculateBrushEffect(float localX, float localZ) const
{
	float distance = 0.0f;
	const float radius = m_brushSize * 0.5f;

	switch (m_brushShape)
	{
		case BS_Circle:
			distance = std::sqrt(localX * localX + localZ * localZ);
			break;

		case BS_Square:
			distance = std::max(std::fabs(localX), std::fabs(localZ));
			break;

		default:
			distance = std::sqrt(localX * localX + localZ * localZ);
			break;
	}

	const float base = calculateFalloff(distance, radius);
	const float exponent = 1.0f / std::max(0.08f, m_brushFeather);
	return std::pow(std::max(0.0f, std::min(1.0f, base)), exponent);
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::invalidateTerrainRegion(float centerX, float centerZ, float radius)
{
	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (!terrainObject)
		return;

	Rectangle2d extent2d(
		centerX - radius,
		centerZ - radius,
		centerX + radius,
		centerZ + radius
	);

	terrainObject->invalidateRegion(extent2d);
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::sampleBrushArea(float centerX, float centerZ, float radius, std::vector<HeightModification>& outSamples) const
{
	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (!terrainObject)
		return;

	const float step = std::max(1.0f, radius / 16.0f);

	for (float z = centerZ - radius; z <= centerZ + radius; z += step)
	{
		for (float x = centerX - radius; x <= centerX + radius; x += step)
		{
			float height = 0.0f;
			Vector pos(x, 0.0f, z);
			if (terrainObject->getHeight(pos, height))
			{
				HeightModification sample;
				sample.worldX = x;
				sample.worldZ = z;
				sample.originalHeight = height;
				sample.modifiedHeight = height;
				sample.timestamp = Clock::frameTime();
				outSamples.push_back(sample);
			}
		}
	}
}

// ----------------------------------------------------------------------

float GodClientTerrainEditor::getAverageHeight(float centerX, float centerZ, float radius) const
{
	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (!terrainObject)
		return 0.0f;

	float totalHeight = 0.0f;
	int sampleCount = 0;
	const float step = std::max(1.0f, radius / 8.0f);

	for (float z = centerZ - radius; z <= centerZ + radius; z += step)
	{
		for (float x = centerX - radius; x <= centerX + radius; x += step)
		{
			float height = 0.0f;
			Vector pos(x, 0.0f, z);
			if (terrainObject->getHeight(pos, height))
			{
				// Check if we have a modification for this point
				const uint64 key = (static_cast<uint64>(static_cast<int>(x) + 32768) << 32) |
				                   (static_cast<uint64>(static_cast<int>(z) + 32768));
				
				HeightModificationMap::const_iterator it = m_heightModifications.find(key);
				if (it != m_heightModifications.end())
				{
					height = it->second.modifiedHeight;
				}
				
				totalHeight += height;
				++sampleCount;
			}
		}
	}

	return (sampleCount > 0) ? (totalHeight / static_cast<float>(sampleCount)) : 0.0f;
}

// ----------------------------------------------------------------------

float GodClientTerrainEditor::generateNoise(float x, float z, float frequency) const
{
	// Simple Perlin-like noise approximation
	const float nx = x * frequency;
	const float nz = z * frequency;

	const int ix = static_cast<int>(std::floor(nx));
	const int iz = static_cast<int>(std::floor(nz));

	const float fx = nx - static_cast<float>(ix);
	const float fz = nz - static_cast<float>(iz);

	// Hash function for pseudo-random values
	auto hash = [](int x, int z) -> float {
		int n = x + z * 57;
		n = (n << 13) ^ n;
		return 1.0f - static_cast<float>((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f;
	};

	// Bilinear interpolation
	const float v00 = hash(ix, iz);
	const float v10 = hash(ix + 1, iz);
	const float v01 = hash(ix, iz + 1);
	const float v11 = hash(ix + 1, iz + 1);

	const float sx = fx * fx * (3.0f - 2.0f * fx);
	const float sz = fz * fz * (3.0f - 2.0f * fz);

	const float v0 = v00 + sx * (v10 - v00);
	const float v1 = v01 + sx * (v11 - v01);

	return v0 + sz * (v1 - v0);
}

// ----------------------------------------------------------------------

bool GodClientTerrainEditor::getModifiedHeight(float x, float z, float originalHeight, float& outHeight)
{
	if (!ms_instance)
	{
		outHeight = originalHeight;
		return false;
	}
	return ms_instance->getModifiedHeightInternal(x, z, originalHeight, outHeight);
}

// ----------------------------------------------------------------------

bool GodClientTerrainEditor::getModifiedHeightInternal(float x, float z, float originalHeight, float& outHeight) const
{
	if (m_heightModifications.empty())
	{
		outHeight = originalHeight;
		return false;
	}

	// Use bilinear interpolation between stored integer grid points
	const int ix = static_cast<int>(std::floor(x));
	const int iz = static_cast<int>(std::floor(z));
	const float fx = x - static_cast<float>(ix);
	const float fz = z - static_cast<float>(iz);

	// Look up the four surrounding grid points
	float h00 = originalHeight, h10 = originalHeight, h01 = originalHeight, h11 = originalHeight;
	bool found00 = false, found10 = false, found01 = false, found11 = false;

	const uint64 key00 = (static_cast<uint64>(ix + 32768) << 32) | static_cast<uint64>(iz + 32768);
	const uint64 key10 = (static_cast<uint64>(ix + 1 + 32768) << 32) | static_cast<uint64>(iz + 32768);
	const uint64 key01 = (static_cast<uint64>(ix + 32768) << 32) | static_cast<uint64>(iz + 1 + 32768);
	const uint64 key11 = (static_cast<uint64>(ix + 1 + 32768) << 32) | static_cast<uint64>(iz + 1 + 32768);

	HeightModificationMap::const_iterator it;

	it = m_heightModifications.find(key00);
	if (it != m_heightModifications.end()) { h00 = it->second.modifiedHeight; found00 = true; }

	it = m_heightModifications.find(key10);
	if (it != m_heightModifications.end()) { h10 = it->second.modifiedHeight; found10 = true; }

	it = m_heightModifications.find(key01);
	if (it != m_heightModifications.end()) { h01 = it->second.modifiedHeight; found01 = true; }

	it = m_heightModifications.find(key11);
	if (it != m_heightModifications.end()) { h11 = it->second.modifiedHeight; found11 = true; }

	// If none found, no modification
	if (!found00 && !found10 && !found01 && !found11)
	{
		outHeight = originalHeight;
		return false;
	}

	// Bilinear interpolation
	const float h0 = h00 + fx * (h10 - h00);
	const float h1 = h01 + fx * (h11 - h01);
	outHeight = h0 + fz * (h1 - h0);

	return true;
}

// ----------------------------------------------------------------------

bool GodClientTerrainEditor::canUndo() const
{
	return !m_undoStack.empty();
}

// ----------------------------------------------------------------------

bool GodClientTerrainEditor::canRedo() const
{
	return !m_redoStack.empty();
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::undo()
{
	if (m_undoStack.empty())
		return;

	BrushStroke stroke = m_undoStack.back();
	m_undoStack.pop_back();

	// Revert the modifications from this stroke
	for (size_t i = 0; i < stroke.modifications.size(); ++i)
	{
		const HeightModification& mod = stroke.modifications[i];
		const int ix = static_cast<int>(std::floor(mod.worldX));
		const int iz = static_cast<int>(std::floor(mod.worldZ));
		const uint64 key = (static_cast<uint64>(ix + 32768) << 32) |
		                   static_cast<uint64>(iz + 32768);

		HeightModificationMap::iterator it = m_heightModifications.find(key);
		if (it != m_heightModifications.end())
		{
			it->second.modifiedHeight = mod.originalHeight;
			if (std::fabs(it->second.modifiedHeight - it->second.originalHeight) < 0.01f)
			{
				m_heightModifications.erase(it);
			}
		}
	}

	m_redoStack.push_back(stroke);

	// Invalidate terrain
	invalidateTerrainRegion(stroke.centerX, stroke.centerZ, stroke.radius + 32.0f);
	nudgeGodClientCameraToRefreshDpvs();

	MainFrame::getInstance().textToConsole("Undo: Terrain modification reverted");
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::redo()
{
	if (m_redoStack.empty())
		return;

	BrushStroke stroke = m_redoStack.back();
	m_redoStack.pop_back();

	// Reapply the modifications
	for (size_t i = 0; i < stroke.modifications.size(); ++i)
	{
		const HeightModification& mod = stroke.modifications[i];
		const int ix = static_cast<int>(std::floor(mod.worldX));
		const int iz = static_cast<int>(std::floor(mod.worldZ));
		const uint64 key = (static_cast<uint64>(ix + 32768) << 32) |
		                   static_cast<uint64>(iz + 32768);

		HeightModificationMap::iterator it = m_heightModifications.find(key);
		if (it != m_heightModifications.end())
		{
			it->second.modifiedHeight = mod.modifiedHeight;
		}
		else
		{
			m_heightModifications[key] = mod;
		}
	}

	m_undoStack.push_back(stroke);

	// Invalidate terrain
	invalidateTerrainRegion(stroke.centerX, stroke.centerZ, stroke.radius + 32.0f);
	nudgeGodClientCameraToRefreshDpvs();

	MainFrame::getInstance().textToConsole("Redo: Terrain modification reapplied");
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::clearHistory()
{
	m_undoStack.clear();
	m_redoStack.clear();
	m_heightModifications.clear();
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::flushTerrainChanges()
{
	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (!terrainObject)
		return;

	float minX = 1e9f, maxX = -1e9f;
	float minZ = 1e9f, maxZ = -1e9f;
	bool hasBounds = false;

	for (HeightModificationMap::const_iterator it = m_heightModifications.begin(); it != m_heightModifications.end(); ++it)
	{
		HeightModification const& mod = it->second;
		hasBounds = true;
		if (mod.worldX < minX) minX = mod.worldX;
		if (mod.worldX > maxX) maxX = mod.worldX;
		if (mod.worldZ < minZ) minZ = mod.worldZ;
		if (mod.worldZ > maxZ) maxZ = mod.worldZ;
	}

	for (ShaderModificationMap::const_iterator it = m_shaderModifications.begin(); it != m_shaderModifications.end(); ++it)
	{
		ShaderModification const& mod = it->second;
		hasBounds = true;
		if (mod.worldX < minX) minX = mod.worldX;
		if (mod.worldX > maxX) maxX = mod.worldX;
		if (mod.worldZ < minZ) minZ = mod.worldZ;
		if (mod.worldZ > maxZ) maxZ = mod.worldZ;
	}

	for (FloraModificationMap::const_iterator it = m_floraModifications.begin(); it != m_floraModifications.end(); ++it)
	{
		FloraModification const& mod = it->second;
		hasBounds = true;
		if (mod.worldX < minX) minX = mod.worldX;
		if (mod.worldX > maxX) maxX = mod.worldX;
		if (mod.worldZ < minZ) minZ = mod.worldZ;
		if (mod.worldZ > maxZ) maxZ = mod.worldZ;
	}

	if (!hasBounds)
		return;

	const float margin = 32.0f;
	Rectangle2d extent2d(minX - margin, minZ - margin, maxX + margin, maxZ + margin);
	terrainObject->invalidateRegion(extent2d);
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::nudgeGodClientCameraToRefreshDpvs()
{
	GroundScene* const gs = dynamic_cast<GroundScene*>(Game::getScene());
	if (!gs)
		return;

	FreeCamera* const cam = gs->getGodClientCamera();
	if (!cam)
		return;

	const bool wasInterpolating = cam->getInterpolating();
	cam->setInterpolating(false);

	// Small translate nudge so streaming / visibility uses a fresh camera pose; 0.002f was
	// too subtle for some DPVS paths after procedural chunk rebuilds.
	static float const nudge = 0.05f;
	FreeCamera::Info info(cam->getInfo());
	info.translate.x += nudge;
	cam->setInfo(info);
	info.translate.x -= nudge;
	cam->setInfo(info);

	cam->setInterpolating(wasInterpolating);
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::renderBrushPreview(const Camera& camera) const
{
	if (!m_brushPreviewEnabled)
		return;

	if (m_toolMode == TM_None || m_toolMode == TM_Select)
		return;

	if (!m_cursorPositionValid)
		return;

	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (!terrainObject)
		return;

	const float worldX = m_cursorWorldPosition.x;
	const float worldZ = m_cursorWorldPosition.z;
	const float radius = m_brushSize * 0.5f;
	const int segments = 32;

	// Determine brush color based on tool mode
	VectorArgb brushColor(1.0f, 0.0f, 1.0f, 0.0f); // Default green

	switch (m_toolMode)
	{
		case TM_Raise:
			brushColor = VectorArgb(1.0f, 0.0f, 0.8f, 0.0f);
			break;
		case TM_Lower:
			brushColor = VectorArgb(1.0f, 0.8f, 0.0f, 0.0f);
			break;
		case TM_Flatten:
		case TM_SetHeight:
			brushColor = VectorArgb(1.0f, 0.0f, 0.5f, 1.0f);
			break;
		case TM_Smooth:
			brushColor = VectorArgb(1.0f, 0.5f, 0.5f, 1.0f);
			break;
		case TM_Noise:
			brushColor = VectorArgb(1.0f, 1.0f, 0.5f, 0.0f);
			break;
		case TM_PaintShader:
			brushColor = VectorArgb(1.0f, 1.0f, 1.0f, 0.0f);
			break;
		case TM_PaintFlora:
			brushColor = VectorArgb(1.0f, 0.0f, 1.0f, 0.5f);
			break;
		case TM_PlaceWater:
			brushColor = VectorArgb(1.0f, 0.0f, 0.3f, 1.0f);
			break;
		case TM_StampBitmap:
			brushColor = VectorArgb(1.0f, 1.0f, 0.55f, 0.1f);
			break;
		case TM_PlaceRadial:
			brushColor = VectorArgb(1.0f, 0.85f, 0.2f, 1.0f);
			break;
		case TM_PlaceRibbon:
		case TM_PlaceRoad:
			brushColor = VectorArgb(1.0f, 0.4f, 0.9f, 1.0f);
			break;
		case TM_PlaceEnvironment:
			brushColor = VectorArgb(1.0f, 0.2f, 1.0f, 0.6f);
			break;
		default:
			break;
	}

	// Draw brush circle/square preview on terrain
	if (m_brushShape == BS_Circle)
	{
		for (int i = 0; i < segments; ++i)
		{
			const float angle1 = (static_cast<float>(i) / static_cast<float>(segments)) * 2.0f * 3.14159265f;
			const float angle2 = (static_cast<float>(i + 1) / static_cast<float>(segments)) * 2.0f * 3.14159265f;

			const float x1 = worldX + radius * std::cos(angle1);
			const float z1 = worldZ + radius * std::sin(angle1);
			const float x2 = worldX + radius * std::cos(angle2);
			const float z2 = worldZ + radius * std::sin(angle2);

			float y1 = 0.0f, y2 = 0.0f;
			Vector pos1(x1, 0.0f, z1);
			Vector pos2(x2, 0.0f, z2);

			if (terrainObject->getHeight(pos1, y1) && terrainObject->getHeight(pos2, y2))
			{
				const Vector start(x1, y1 + 0.35f, z1);
				const Vector end(x2, y2 + 0.35f, z2);

				camera.addDebugPrimitive(new Line3dDebugPrimitive(
					Line3dDebugPrimitive::S_none,
					Transform::identity,
					start,
					end,
					brushColor
				));
			}
		}
	}
	else // BS_Square
	{
		const float halfSize = radius;

		Vector corners[4];
		corners[0] = Vector(worldX - halfSize, 0.0f, worldZ - halfSize);
		corners[1] = Vector(worldX + halfSize, 0.0f, worldZ - halfSize);
		corners[2] = Vector(worldX + halfSize, 0.0f, worldZ + halfSize);
		corners[3] = Vector(worldX - halfSize, 0.0f, worldZ + halfSize);

		for (int i = 0; i < 4; ++i)
		{
			terrainObject->getHeight(corners[i], corners[i].y);
			corners[i].y += 0.35f;
		}

		for (int i = 0; i < 4; ++i)
		{
			camera.addDebugPrimitive(new Line3dDebugPrimitive(
				Line3dDebugPrimitive::S_none,
				Transform::identity,
				corners[i],
				corners[(i + 1) % 4],
				brushColor
			));
		}
	}
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::setRegionSelection(float minX, float minZ, float maxX, float maxZ)
{
	m_hasRegionSelection = true;
	m_regionMinX = minX;
	m_regionMinZ = minZ;
	m_regionMaxX = maxX;
	m_regionMaxZ = maxZ;
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::clearRegionSelection()
{
	m_hasRegionSelection = false;
}

// ----------------------------------------------------------------------

bool GodClientTerrainEditor::applyRectangularHeightSamples(
	float const minX,
	float const minZ,
	float const maxX,
	float const maxZ,
	int const nx,
	int const nz,
	float const* heightsRowMajor)
{
	if (!heightsRowMajor || nx < 2 || nz < 2)
		return false;

	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (!terrainObject)
		return false;

	BrushStroke stroke;
	stroke.centerX = (minX + maxX) * 0.5f;
	stroke.centerZ = (minZ + maxZ) * 0.5f;
	stroke.radius = std::max(maxX - minX, maxZ - minZ) * 0.5f;
	stroke.strength = 1.0f;
	stroke.tool = TM_SetHeight;
	stroke.targetHeight = 0.0f;

	float const dx = (nx > 1) ? ((maxX - minX) / static_cast<float>(nx - 1)) : 0.0f;
	float const dz = (nz > 1) ? ((maxZ - minZ) / static_cast<float>(nz - 1)) : 0.0f;

	for (int iz = 0; iz < nz; ++iz)
	{
		for (int ix = 0; ix < nx; ++ix)
		{
			float const wx = minX + dx * static_cast<float>(ix);
			float const wz = minZ + dz * static_cast<float>(iz);
			int const gix = static_cast<int>(std::floor(wx));
			int const giz = static_cast<int>(std::floor(wz));
			float const newH = heightsRowMajor[iz * nx + ix];

			float baseH = 0.0f;
			Vector pos(static_cast<float>(gix), 0.0f, static_cast<float>(giz));
			if (!terrainObject->getHeight(pos, baseH))
				continue;

			uint64 const key = (static_cast<uint64>(gix + 32768) << 32) | static_cast<uint64>(giz + 32768);

			float beforeH = baseH;
			HeightModificationMap::iterator hit = m_heightModifications.find(key);
			if (hit != m_heightModifications.end())
				beforeH = hit->second.modifiedHeight;

			float const baseline = (hit != m_heightModifications.end()) ? hit->second.originalHeight : baseH;

			HeightModification snapshot;
			snapshot.worldX = static_cast<float>(gix);
			snapshot.worldZ = static_cast<float>(giz);
			snapshot.originalHeight = beforeH;
			snapshot.modifiedHeight = newH;
			snapshot.timestamp = Clock::frameTime();
			stroke.modifications.push_back(snapshot);

			HeightModification stored;
			stored.worldX = snapshot.worldX;
			stored.worldZ = snapshot.worldZ;
			stored.originalHeight = baseline;
			stored.modifiedHeight = newH;
			stored.timestamp = snapshot.timestamp;
			m_heightModifications[key] = stored;
		}
	}

	if (!stroke.modifications.empty())
	{
		m_undoStack.push_back(stroke);
		while (static_cast<int>(m_undoStack.size()) > MAX_UNDO_STROKES)
			m_undoStack.erase(m_undoStack.begin());
		m_redoStack.clear();
	}

	float const margin = 32.0f;
	Rectangle2d const extent2d(minX - margin, minZ - margin, maxX + margin, maxZ + margin);
	terrainObject->invalidateRegion(extent2d);
	flushTerrainChanges();
	nudgeGodClientCameraToRefreshDpvs();
	return true;
}

// ----------------------------------------------------------------------

bool GodClientTerrainEditor::applyRectangleExcludeAndNonPassable(float minX, float minZ, float maxX, float maxZ)
{
	float const ax0 = std::min(minX, maxX);
	float const az0 = std::min(minZ, maxZ);
	float const ax1 = std::max(minX, maxX);
	float const az1 = std::max(minZ, maxZ);

	static const float kMinExtent = 0.5f;
	if ((ax1 - ax0) < kMinExtent || (az1 - az0) < kMinExtent)
		return false;

	TerrainGenerator* const generator = getTerrainGenerator();
	if (!generator)
		return false;

	Rectangle2d const rect(ax0, az0, ax1, az1);

	TerrainGenerator::Layer* const layer = new TerrainGenerator::Layer();

	char layerName[128];
	static int s_godClientCutLayerSerial = 0;
	snprintf(layerName, sizeof(layerName), "GodClientTerrainCut_%d", s_godClientCutLayerSerial++);

	layer->setName(layerName);
	layer->setActive(true);

	BoundaryRectangle* const boundary = new BoundaryRectangle();
	boundary->setName("Terrain cut (exclude/non-passable)");
	boundary->setRectangle(rect);
	boundary->setFeatherDistance(0.0f);
	layer->addBoundary(boundary);

	AffectorExclude* const ex = new AffectorExclude();
	ex->setName("Exclude Flora");
	layer->addAffector(ex);

	AffectorPassable* const pass = new AffectorPassable();
	pass->setName("Non-passable");
	pass->setPassable(false);
	pass->setFeatherThreshold(0.0f);
	layer->addAffector(pass);

	generator->addLayer(layer);
	m_createdLayers.push_back(layerName);

	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (terrainObject)
	{
		float const margin = 32.0f;
		Rectangle2d extent2d(ax0 - margin, az0 - margin, ax1 + margin, az1 + margin);
		terrainObject->invalidateRegion(extent2d);
	}

	flushTerrainChanges();
	nudgeGodClientCameraToRefreshDpvs();

	return true;
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::renderTerrainDebugOverlays(
	Camera const & camera,
	bool const wireframeGrid,
	bool const heightColorGrid,
	bool const chunkBoundsGrid) const
{
	if (!wireframeGrid && !heightColorGrid && !chunkBoundsGrid)
		return;

	TerrainObject const* const terrainObject = TerrainObject::getConstInstance();
	if (!terrainObject)
		return;

	Appearance const* const appearance = terrainObject->getAppearance();
	ProceduralTerrainAppearanceTemplate const* const terrainTemplate = appearance
		? dynamic_cast<ProceduralTerrainAppearanceTemplate const*>(appearance->getAppearanceTemplate())
		: 0;

	float tileW = 8.0f;
	float chunkW = 32.0f;
	if (terrainTemplate)
	{
		float const tw = terrainTemplate->getTileWidthInMeters();
		float const cw = terrainTemplate->getChunkWidthInMeters();
		if (tw > 0.01f)
			tileW = tw;
		if (cw > 0.01f)
			chunkW = cw;
	}

	Vector const centerW = camera.getPosition_w();
	float const cx = centerW.x;
	float const cz = centerW.z;
	float halfSpan = 112.0f;
	float const maxHalf = tileW * 56.0f;
	if (halfSpan > maxHalf)
		halfSpan = maxHalf;

	float const x0 = cx - halfSpan;
	float const x1 = cx + halfSpan;
	float const z0 = cz - halfSpan;
	float const z1 = cz + halfSpan;

	float hMin = 1e9f;
	float hMax = -1e9f;
	for (int c = 0; c < 4; ++c)
	{
		float const tx = (c & 1) ? x1 : x0;
		float const tz = (c & 2) ? z1 : z0;
		float h = 0.0f;
		Vector const p(tx, 0.0f, tz);
		if (!terrainObject->getHeight(p, h))
			continue;
		if (h < hMin)
			hMin = h;
		if (h > hMax)
			hMax = h;
	}
	if (!(hMax > hMin + 0.001f))
	{
		hMin -= 2.0f;
		hMax += 2.0f;
	}

	if (wireframeGrid && !heightColorGrid)
	{
		VectorArgb const lineColor(1.0f, 0.95f, 0.95f, 0.95f);
		int const iMin = static_cast<int>(std::floor(x0 / tileW));
		int const iMax = static_cast<int>(std::ceil(x1 / tileW));
		int const kMin = static_cast<int>(std::floor(z0 / tileW));
		int const kMax = static_cast<int>(std::ceil(z1 / tileW));
		for (int i = iMin; i <= iMax; ++i)
		{
			float const x = static_cast<float>(i) * tileW;
			addTerrainHeightDebugLineStrip(*terrainObject, camera, x, z0, x, z1, lineColor);
		}
		for (int k = kMin; k <= kMax; ++k)
		{
			float const z = static_cast<float>(k) * tileW;
			addTerrainHeightDebugLineStrip(*terrainObject, camera, x0, z, x1, z, lineColor);
		}
	}

	if (heightColorGrid)
	{
		int const iMin = static_cast<int>(std::floor(x0 / tileW));
		int const iMax = static_cast<int>(std::ceil(x1 / tileW));
		int const kMin = static_cast<int>(std::floor(z0 / tileW));
		int const kMax = static_cast<int>(std::ceil(z1 / tileW));
		for (int i = iMin; i <= iMax; ++i)
		{
			float const x = static_cast<float>(i) * tileW;
			float hm = 0.5f * (hMin + hMax);
			Vector pm(x, 0.0f, 0.5f * (z0 + z1));
			IGNORE_RETURN(terrainObject->getHeight(pm, hm));
			addTerrainHeightDebugLineStrip(
				*terrainObject, camera, x, z0, x, z1,
				heightBandColor(hm, hMin, hMax));
		}
		for (int k = kMin; k <= kMax; ++k)
		{
			float const z = static_cast<float>(k) * tileW;
			float hm = 0.5f * (hMin + hMax);
			Vector pm(0.5f * (x0 + x1), 0.0f, z);
			IGNORE_RETURN(terrainObject->getHeight(pm, hm));
			addTerrainHeightDebugLineStrip(
				*terrainObject, camera, x0, z, x1, z,
				heightBandColor(hm, hMin, hMax));
		}
	}

	if (chunkBoundsGrid)
	{
		VectorArgb const chunkColor(1.0f, 0.2f, 0.85f, 1.0f);
		int const iMin = static_cast<int>(std::floor(x0 / chunkW));
		int const iMax = static_cast<int>(std::ceil(x1 / chunkW));
		int const kMin = static_cast<int>(std::floor(z0 / chunkW));
		int const kMax = static_cast<int>(std::ceil(z1 / chunkW));
		for (int i = iMin; i <= iMax; ++i)
		{
			float const x = static_cast<float>(i) * chunkW;
			addTerrainHeightDebugLineStrip(*terrainObject, camera, x, z0, x, z1, chunkColor);
		}
		for (int k = kMin; k <= kMax; ++k)
		{
			float const z = static_cast<float>(k) * chunkW;
			addTerrainHeightDebugLineStrip(*terrainObject, camera, x0, z, x1, z, chunkColor);
		}
	}
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::renderRegionSelectionOverlay(
	Camera const & camera,
	float const minX,
	float const minZ,
	float const maxX,
	float const maxZ) const
{
	TerrainObject const* const terrainObject = TerrainObject::getConstInstance();
	if (!terrainObject)
		return;

	VectorArgb const color(1.0f, 1.0f, 0.85f, 0.1f);
	addTerrainHeightDebugLineStrip(*terrainObject, camera, minX, minZ, maxX, minZ, color);
	addTerrainHeightDebugLineStrip(*terrainObject, camera, maxX, minZ, maxX, maxZ, color);
	addTerrainHeightDebugLineStrip(*terrainObject, camera, maxX, maxZ, minX, maxZ, color);
	addTerrainHeightDebugLineStrip(*terrainObject, camera, minX, maxZ, minX, minZ, color);
}

// ======================================================================
// Shader Painting
// ======================================================================

void GodClientTerrainEditor::modifyShaderPaint(float worldX, float worldZ, int shaderFamilyId, float strength)
{
	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (!terrainObject)
		return;

	const float halfBrush = m_brushSize * 0.5f;

	const int minX = static_cast<int>(std::floor(worldX - halfBrush));
	const int maxX = static_cast<int>(std::ceil(worldX + halfBrush));
	const int minZ = static_cast<int>(std::floor(worldZ - halfBrush));
	const int maxZ = static_cast<int>(std::ceil(worldZ + halfBrush));

	for (int iz = minZ; iz <= maxZ; ++iz)
	{
		for (int ix = minX; ix <= maxX; ++ix)
		{
			const float x = static_cast<float>(ix);
			const float z = static_cast<float>(iz);
			const float localX = x - worldX;
			const float localZ = z - worldZ;
			const float effect = calculateBrushEffect(localX, localZ);

			if (effect > 0.0f)
			{
				const uint64 key = (static_cast<uint64>(ix + 32768) << 32) |
				                   static_cast<uint64>(iz + 32768);

				float const feather01 = std::max(0.0f, std::min(1.0f, effect * strength));

				ShaderModification mod;
				mod.worldX = x;
				mod.worldZ = z;
				mod.modifiedFamilyId = shaderFamilyId;

				ShaderModificationMap::iterator it = m_shaderModifications.find(key);
				if (it != m_shaderModifications.end())
				{
					mod.originalFamilyId = it->second.originalFamilyId;
					if (shaderFamilyId == it->second.modifiedFamilyId)
						mod.featherAmount = std::max(feather01, it->second.featherAmount);
					else
						mod.featherAmount = feather01;
				}
				else
				{
					mod.originalFamilyId = -1;
					mod.featherAmount = feather01;
				}

				m_shaderModifications[key] = mod;
			}
		}
	}
}

// ----------------------------------------------------------------------

bool GodClientTerrainEditor::getModifiedShader(float x, float z, int originalFamilyId, int& outFamilyId, float& outFeather)
{
	if (!ms_instance)
	{
		outFamilyId = originalFamilyId;
		outFeather = 0.0f;
		return false;
	}
	return ms_instance->getModifiedShaderInternal(x, z, originalFamilyId, outFamilyId, outFeather);
}

// ----------------------------------------------------------------------

bool GodClientTerrainEditor::getModifiedShaderInternal(float x, float z, int originalFamilyId, int& outFamilyId, float& outFeather) const
{
	if (m_shaderModifications.empty())
	{
		outFamilyId = originalFamilyId;
		outFeather = 0.0f;
		return false;
	}

	// Sample positions from procedural chunk shading do not land on exact 1m integer grid points
	// used when painting; search a small neighborhood so painted edits actually show up.
	const int cx = static_cast<int>(floorf(x));
	const int cz = static_cast<int>(floorf(z));

	int bestFamily = originalFamilyId;
	float bestFeather = -1.0f;
	bool found = false;

	for (int dz = -2; dz <= 2; ++dz)
	{
		for (int dx = -2; dx <= 2; ++dx)
		{
			const int ix = cx + dx;
			const int iz = cz + dz;
			const uint64 key = (static_cast<uint64>(ix + 32768) << 32) | static_cast<uint64>(iz + 32768);
			ShaderModificationMap::const_iterator const it = m_shaderModifications.find(key);
			if (it == m_shaderModifications.end())
				continue;
			const float f = it->second.featherAmount;
			if (f > bestFeather)
			{
				bestFeather = f;
				bestFamily = it->second.modifiedFamilyId;
				found = true;
			}
		}
	}

	if (!found || bestFeather <= 0.0f)
	{
		outFamilyId = originalFamilyId;
		outFeather = 0.0f;
		return false;
	}

	outFamilyId = bestFamily;
	outFeather = bestFeather;
	return true;
}

// ======================================================================
// Flora Painting
// ======================================================================

void GodClientTerrainEditor::modifyFloraPaint(float worldX, float worldZ, int floraFamilyId, float density, bool collidable)
{
	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (!terrainObject)
		return;

	const float halfBrush = m_brushSize * 0.5f;

	const int minX = static_cast<int>(std::floor(worldX - halfBrush));
	const int maxX = static_cast<int>(std::ceil(worldX + halfBrush));
	const int minZ = static_cast<int>(std::floor(worldZ - halfBrush));
	const int maxZ = static_cast<int>(std::ceil(worldZ + halfBrush));

	for (int iz = minZ; iz <= maxZ; ++iz)
	{
		for (int ix = minX; ix <= maxX; ++ix)
		{
			const float x = static_cast<float>(ix);
			const float z = static_cast<float>(iz);
			const float localX = x - worldX;
			const float localZ = z - worldZ;
			const float effect = calculateBrushEffect(localX, localZ);

			if (effect > 0.0f)
			{
				const uint64 key = (static_cast<uint64>(ix + 32768) << 32) |
				                   static_cast<uint64>(iz + 32768);

				FloraModification mod;
				mod.worldX = x;
				mod.worldZ = z;
				mod.modifiedFamilyId = floraFamilyId;
				mod.density = density * effect * m_brushStrength;
				mod.collidable = collidable;

				FloraModificationMap::iterator it = m_floraModifications.find(key);
				if (it != m_floraModifications.end())
				{
					mod.originalFamilyId = it->second.originalFamilyId;
					if (mod.density > it->second.density)
					{
						m_floraModifications[key] = mod;
					}
				}
				else
				{
					mod.originalFamilyId = -1;
					m_floraModifications[key] = mod;
				}
			}
		}
	}
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::modifyFloraRemove(float worldX, float worldZ, float strength)
{
	const float halfBrush = m_brushSize * 0.5f;

	const int minX = static_cast<int>(std::floor(worldX - halfBrush));
	const int maxX = static_cast<int>(std::ceil(worldX + halfBrush));
	const int minZ = static_cast<int>(std::floor(worldZ - halfBrush));
	const int maxZ = static_cast<int>(std::ceil(worldZ + halfBrush));

	for (int iz = minZ; iz <= maxZ; ++iz)
	{
		for (int ix = minX; ix <= maxX; ++ix)
		{
			const float x = static_cast<float>(ix);
			const float z = static_cast<float>(iz);
			const float localX = x - worldX;
			const float localZ = z - worldZ;
			const float effect = calculateBrushEffect(localX, localZ);

			if (effect > 0.0f && effect * strength > 0.5f)
			{
				const uint64 key = (static_cast<uint64>(ix + 32768) << 32) |
				                   static_cast<uint64>(iz + 32768);

				m_floraModifications.erase(key);
			}
		}
	}
}

// ----------------------------------------------------------------------

bool GodClientTerrainEditor::getModifiedFlora(float x, float z, int originalFamilyId, int& outFamilyId, float& outDensity)
{
	if (!ms_instance)
	{
		outFamilyId = originalFamilyId;
		outDensity = 0.0f;
		return false;
	}
	return ms_instance->getModifiedFloraInternal(x, z, originalFamilyId, outFamilyId, outDensity);
}

// ----------------------------------------------------------------------

bool GodClientTerrainEditor::getModifiedFloraInternal(float x, float z, int originalFamilyId, int& outFamilyId, float& outDensity) const
{
	if (m_floraModifications.empty())
	{
		outFamilyId = originalFamilyId;
		outDensity = 0.0f;
		return false;
	}

	const int cx = static_cast<int>(floorf(x));
	const int cz = static_cast<int>(floorf(z));
	int bestFamily = originalFamilyId;
	float bestDensity = -1.0f;
	bool found = false;

	for (int dz = -2; dz <= 2; ++dz)
	{
		for (int dx = -2; dx <= 2; ++dx)
		{
			const int ix = cx + dx;
			const int iz = cz + dz;
			const uint64 key = (static_cast<uint64>(ix + 32768) << 32) | static_cast<uint64>(iz + 32768);
			FloraModificationMap::const_iterator const it = m_floraModifications.find(key);
			if (it == m_floraModifications.end())
				continue;
			const float d = it->second.density;
			if (d > bestDensity)
			{
				bestDensity = d;
				bestFamily = it->second.modifiedFamilyId;
				found = true;
			}
		}
	}

	if (!found || bestDensity < 0.0f)
	{
		outFamilyId = originalFamilyId;
		outDensity = 0.0f;
		return false;
	}

	outFamilyId = bestFamily;
	outDensity = bestDensity;
	return true;
}

// ======================================================================
// Polyline Editing (Roads/Ribbons)
// ======================================================================

void GodClientTerrainEditor::setPolylineEditMode(PolylineEditMode mode)
{
	m_polylineEditMode = mode;
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::beginPolyline(bool isRibbon)
{
	m_activePolyline.controlPoints.clear();
	m_activePolyline.isRibbon = isRibbon;
	m_activePolyline.name = isRibbon ? "New Ribbon" : "New Road";
	m_polylineEditMode = PEM_AddPoints;
	m_selectedPolylinePoint = -1;

	MainFrame::getInstance().textToConsole(isRibbon ? 
		"Started new ribbon - click to add control points" :
		"Started new road - click to add control points");
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::addPolylinePoint(float worldX, float worldZ, float height)
{
	ControlPoint point;
	point.position = Vector2d(worldX, worldZ);
	
	if (height == 0.0f && !m_activePolyline.hasFixedHeights)
	{
		point.height = getTerrainHeightAtPoint(worldX, worldZ);
	}
	else
	{
		point.height = height;
	}
	point.width = m_activePolyline.width;

	m_activePolyline.controlPoints.push_back(point);
	m_selectedPolylinePoint = static_cast<int>(m_activePolyline.controlPoints.size()) - 1;

	recalculatePolylineExtent();

	char buffer[256];
	snprintf(buffer, sizeof(buffer), "Added control point %d at (%.1f, %.1f, %.1f)",
		m_selectedPolylinePoint, worldX, worldZ, point.height);
	MainFrame::getInstance().textToConsole(buffer);
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::movePolylinePoint(int pointIndex, float worldX, float worldZ, float height)
{
	if (pointIndex < 0 || pointIndex >= static_cast<int>(m_activePolyline.controlPoints.size()))
		return;

	ControlPoint& point = m_activePolyline.controlPoints[static_cast<size_t>(pointIndex)];
	point.position = Vector2d(worldX, worldZ);
	point.height = height;

	recalculatePolylineExtent();
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::deletePolylinePoint(int pointIndex)
{
	if (pointIndex < 0 || pointIndex >= static_cast<int>(m_activePolyline.controlPoints.size()))
		return;

	m_activePolyline.controlPoints.erase(
		m_activePolyline.controlPoints.begin() + pointIndex);

	if (m_selectedPolylinePoint >= static_cast<int>(m_activePolyline.controlPoints.size()))
	{
		m_selectedPolylinePoint = static_cast<int>(m_activePolyline.controlPoints.size()) - 1;
	}

	recalculatePolylineExtent();
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::insertPolylinePoint(int afterIndex, float worldX, float worldZ, float height)
{
	if (afterIndex < -1 || afterIndex >= static_cast<int>(m_activePolyline.controlPoints.size()))
		return;

	ControlPoint point;
	point.position = Vector2d(worldX, worldZ);
	point.height = (height == 0.0f) ? getTerrainHeightAtPoint(worldX, worldZ) : height;
	point.width = m_activePolyline.width;

	m_activePolyline.controlPoints.insert(
		m_activePolyline.controlPoints.begin() + afterIndex + 1, point);

	m_selectedPolylinePoint = afterIndex + 1;

	recalculatePolylineExtent();
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::finalizePolyline()
{
	if (m_activePolyline.controlPoints.size() < 2)
	{
		MainFrame::getInstance().textToConsole("Need at least 2 control points to create road/ribbon");
		return;
	}

	bool success = false;
	if (m_activePolyline.isRibbon)
	{
		success = createRibbonFromPolyline(m_activePolyline.name.c_str());
	}
	else
	{
		success = createRoadFromPolyline(m_activePolyline.name.c_str());
	}

	if (success)
	{
		char buffer[256];
		snprintf(buffer, sizeof(buffer), "%s '%s' created with %d control points",
			m_activePolyline.isRibbon ? "Ribbon" : "Road",
			m_activePolyline.name.c_str(),
			static_cast<int>(m_activePolyline.controlPoints.size()));
		MainFrame::getInstance().textToConsole(buffer);

		m_activePolyline.controlPoints.clear();
		m_polylineEditMode = PEM_None;
		m_selectedPolylinePoint = -1;
	}
	else
	{
		MainFrame::getInstance().textToConsole("Failed to create road/ribbon - check terrain generator access");
	}
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::cancelPolyline()
{
	m_activePolyline.controlPoints.clear();
	m_polylineEditMode = PEM_None;
	m_selectedPolylinePoint = -1;

	MainFrame::getInstance().textToConsole("Polyline editing cancelled");
}

// ----------------------------------------------------------------------

const GodClientTerrainEditor::ControlPoint* GodClientTerrainEditor::getPolylinePoint(int index) const
{
	if (index < 0 || index >= static_cast<int>(m_activePolyline.controlPoints.size()))
		return 0;

	return &m_activePolyline.controlPoints[static_cast<size_t>(index)];
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::setPolylineWidth(float width)
{
	m_activePolyline.width = std::max(1.0f, std::min(128.0f, width));

	for (size_t i = 0; i < m_activePolyline.controlPoints.size(); ++i)
	{
		m_activePolyline.controlPoints[i].width = m_activePolyline.width;
	}
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::setPolylineShaderFamily(int familyId)
{
	m_activePolyline.shaderFamilyId = familyId;
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::setPolylineFeatherDistance(float distance)
{
	m_activePolyline.featherDistance = std::max(0.0f, distance);
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::setPolylineUseFixedHeights(bool useFixed)
{
	m_activePolyline.hasFixedHeights = useFixed;
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::setPolylineName(const char* name)
{
	if (name)
	{
		m_activePolyline.name = name;
	}
}

// ----------------------------------------------------------------------

const char* GodClientTerrainEditor::getPolylineName() const
{
	return m_activePolyline.name.c_str();
}

// ----------------------------------------------------------------------

int GodClientTerrainEditor::findNearestPolylinePoint(float worldX, float worldZ, float maxDistance) const
{
	int nearestIndex = -1;
	float nearestDistSq = maxDistance * maxDistance;

	for (size_t i = 0; i < m_activePolyline.controlPoints.size(); ++i)
	{
		const ControlPoint& point = m_activePolyline.controlPoints[i];
		const float dx = static_cast<float>(point.position.x) - worldX;
		const float dz = static_cast<float>(point.position.y) - worldZ;
		const float distSq = dx * dx + dz * dz;

		if (distSq < nearestDistSq)
		{
			nearestDistSq = distSq;
			nearestIndex = static_cast<int>(i);
		}
	}

	return nearestIndex;
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::setSelectedPolylinePoint(int index)
{
	if (index < -1)
		index = -1;
	if (index >= static_cast<int>(m_activePolyline.controlPoints.size()))
		index = static_cast<int>(m_activePolyline.controlPoints.size()) - 1;

	m_selectedPolylinePoint = index;
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::recalculatePolylineExtent()
{
	if (m_activePolyline.controlPoints.empty())
	{
		m_polylineExtent = Rectangle2d();
		return;
	}

	float minX = static_cast<float>(m_activePolyline.controlPoints[0].position.x);
	float minZ = static_cast<float>(m_activePolyline.controlPoints[0].position.y);
	float maxX = minX;
	float maxZ = minZ;

	for (size_t i = 1; i < m_activePolyline.controlPoints.size(); ++i)
	{
		const float x = static_cast<float>(m_activePolyline.controlPoints[i].position.x);
		const float z = static_cast<float>(m_activePolyline.controlPoints[i].position.y);

		if (x < minX) minX = x;
		if (x > maxX) maxX = x;
		if (z < minZ) minZ = z;
		if (z > maxZ) maxZ = z;
	}

	const float margin = m_activePolyline.width + m_activePolyline.featherDistance;
	m_polylineExtent = Rectangle2d(minX - margin, minZ - margin, maxX + margin, maxZ + margin);
}

// ----------------------------------------------------------------------

float GodClientTerrainEditor::getTerrainHeightAtPoint(float worldX, float worldZ) const
{
	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (!terrainObject)
		return 0.0f;

	float height = 0.0f;
	Vector pos(worldX, 0.0f, worldZ);
	if (terrainObject->getHeight(pos, height))
	{
		return height;
	}

	return 0.0f;
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::renderPolylinePreview(const Camera& camera) const
{
	if (m_activePolyline.controlPoints.size() < 1)
		return;

	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (!terrainObject)
		return;

	const VectorArgb pointColor(1.0f, 1.0f, 1.0f, 0.0f);
	const VectorArgb selectedPointColor(1.0f, 0.0f, 1.0f, 0.0f);
	const VectorArgb lineColor = m_activePolyline.isRibbon ? 
		VectorArgb(1.0f, 0.0f, 0.5f, 1.0f) : 
		VectorArgb(1.0f, 0.8f, 0.4f, 0.0f);

	for (size_t i = 0; i < m_activePolyline.controlPoints.size(); ++i)
	{
		const ControlPoint& point = m_activePolyline.controlPoints[i];
		const float x = static_cast<float>(point.position.x);
		const float z = static_cast<float>(point.position.y);
		float y = point.height;

		const bool isSelected = (static_cast<int>(i) == m_selectedPolylinePoint);
		const VectorArgb& color = isSelected ? selectedPointColor : pointColor;
		const float size = isSelected ? 2.0f : 1.0f;

		const Vector center(x, y + 0.5f, z);
		const Vector top(x, y + size + 0.5f, z);
		const Vector right(x + size, y + 0.5f, z);
		const Vector front(x, y + 0.5f, z + size);

		camera.addDebugPrimitive(new Line3dDebugPrimitive(
			Line3dDebugPrimitive::S_none, Transform::identity, center, top, color));
		camera.addDebugPrimitive(new Line3dDebugPrimitive(
			Line3dDebugPrimitive::S_none, Transform::identity, 
			Vector(center.x - size, center.y, center.z), right, color));
		camera.addDebugPrimitive(new Line3dDebugPrimitive(
			Line3dDebugPrimitive::S_none, Transform::identity,
			Vector(center.x, center.y, center.z - size), front, color));
	}

	for (size_t i = 1; i < m_activePolyline.controlPoints.size(); ++i)
	{
		const ControlPoint& p1 = m_activePolyline.controlPoints[i - 1];
		const ControlPoint& p2 = m_activePolyline.controlPoints[i];

		const Vector start(static_cast<float>(p1.position.x), p1.height + 0.5f, static_cast<float>(p1.position.y));
		const Vector end(static_cast<float>(p2.position.x), p2.height + 0.5f, static_cast<float>(p2.position.y));

		camera.addDebugPrimitive(new Line3dDebugPrimitive(
			Line3dDebugPrimitive::S_none, Transform::identity, start, end, lineColor));
	}
}

// ======================================================================
// Environment Zones
// ======================================================================

void GodClientTerrainEditor::beginEnvironmentZone()
{
	m_activeEnvironmentZone.boundaryPoints.clear();
	m_environmentZoneActive = true;

	MainFrame::getInstance().textToConsole("Started new environment zone - click to add boundary points");
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::addEnvironmentZonePoint(float worldX, float worldZ)
{
	m_activeEnvironmentZone.boundaryPoints.push_back(Vector2d(worldX, worldZ));

	char buffer[128];
	snprintf(buffer, sizeof(buffer), "Added environment zone point at (%.1f, %.1f)", worldX, worldZ);
	MainFrame::getInstance().textToConsole(buffer);
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::finalizeEnvironmentZone()
{
	if (m_activeEnvironmentZone.boundaryPoints.size() < 3)
	{
		MainFrame::getInstance().textToConsole("Need at least 3 points to create environment zone");
		return;
	}

	bool success = createEnvironmentZoneAffector(m_activeEnvironmentZone.name.c_str());

	if (success)
	{
		char buffer[256];
		snprintf(buffer, sizeof(buffer), "Environment zone '%s' created with %d boundary points",
			m_activeEnvironmentZone.name.c_str(),
			static_cast<int>(m_activeEnvironmentZone.boundaryPoints.size()));
		MainFrame::getInstance().textToConsole(buffer);

		m_activeEnvironmentZone.boundaryPoints.clear();
		m_environmentZoneActive = false;
	}
	else
	{
		MainFrame::getInstance().textToConsole("Failed to create environment zone");
	}
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::cancelEnvironmentZone()
{
	m_activeEnvironmentZone.boundaryPoints.clear();
	m_environmentZoneActive = false;

	MainFrame::getInstance().textToConsole("Environment zone editing cancelled");
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::setEnvironmentFamily(int familyId)
{
	m_environmentFamilyId = familyId;
	m_activeEnvironmentZone.environmentFamilyId = familyId;
}

// ======================================================================
// Bitmap Stamps
// ======================================================================

void GodClientTerrainEditor::setBitmapStamp(const char* bitmapName)
{
	if (bitmapName)
	{
		m_bitmapStamp.bitmapName = bitmapName;
		loadBitmapStampData(bitmapName);
	}
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::setBitmapStampRotation(float rotation)
{
	m_bitmapStamp.rotation = rotation;
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::setBitmapStampScale(float scale)
{
	m_bitmapStamp.scale = std::max(0.1f, std::min(10.0f, scale));
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::setBitmapAffectsHeight(bool affects)
{
	m_bitmapStamp.affectsHeight = affects;
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::setBitmapAffectsShader(bool affects)
{
	m_bitmapStamp.affectsShader = affects;
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::setBitmapHeightScale(float scale)
{
	m_bitmapStamp.heightScale = scale;
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::setBitmapShaderFamily(int familyId)
{
	m_bitmapStamp.shaderFamilyId = familyId;
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::loadBitmapStampData(const char* filename)
{
	UNREF(filename);

	m_bitmapHeightData.clear();
	m_bitmapShaderData.clear();
	m_bitmapWidth = 0;
	m_bitmapHeight = 0;
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::loadBitmapStampDataFromImage(Image const* image)
{
	m_bitmapHeightData.clear();
	m_bitmapShaderData.clear();
	m_bitmapWidth = 0;
	m_bitmapHeight = 0;

	if (!image)
		return;

	int const w = image->getWidth();
	int const h = image->getHeight();
	if (w <= 0 || h <= 0)
		return;

	uint8 const* base = image->lockReadOnly(true);
	if (!base)
		return;

	Image::UnlockGuard const guard(image);

	m_bitmapWidth = w;
	m_bitmapHeight = h;
	m_bitmapHeightData.resize(static_cast<size_t>(w * h));

	Image::PixelFormat const pf = image->getPixelFormat();
	int const bpp = image->getBytesPerPixel();
	int const stride = image->getStride();

	for (int z = 0; z < h; ++z)
	{
		uint8 const* row = base + z * stride;
		uint8 const* p = row;
		for (int x = 0; x < w; ++x)
		{
			uint8 r = 0;
			uint8 g = 0;
			uint8 b = 0;
			uint8 a = 255;
			Image::getPixel(r, g, b, a, p, pf);
			float const lum = (0.299f * static_cast<float>(r) + 0.587f * static_cast<float>(g) + 0.114f * static_cast<float>(b)) / 255.0f;
			m_bitmapHeightData[static_cast<size_t>(z * w + x)] = lum;
			p += bpp;
		}
	}
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::reloadBitmapStampFromTerrainFamily(int familyId)
{
	TerrainGenerator* const generator = getTerrainGenerator();
	if (!generator || familyId < 0)
	{
		loadBitmapStampData(0);
		return;
	}

	BitmapGroup const& bg = generator->getBitmapGroup();
	if (!bg.hasFamily(familyId))
	{
		loadBitmapStampData(0);
		MainFrame::getInstance().textToConsole("Bitmap stamp family not in scene terrain.");
		return;
	}

	Image const* const image = bg.getFamilyBitmap(familyId);
	loadBitmapStampDataFromImage(image);

	char const* const n = bg.getFamilyName(familyId);
	if (n && *n)
		m_bitmapStamp.bitmapName = n;
	else
	{
		char buf[64];
		snprintf(buf, sizeof(buf), "bitmap_family_%d", familyId);
		m_bitmapStamp.bitmapName = buf;
	}
}

// ----------------------------------------------------------------------

float GodClientTerrainEditor::sampleBitmapHeight(float normalizedX, float normalizedZ) const
{
	if (m_bitmapHeightData.empty() || m_bitmapWidth == 0 || m_bitmapHeight == 0)
		return 0.0f;

	const int ix = static_cast<int>(normalizedX * static_cast<float>(m_bitmapWidth - 1));
	const int iz = static_cast<int>(normalizedZ * static_cast<float>(m_bitmapHeight - 1));

	if (ix < 0 || ix >= m_bitmapWidth || iz < 0 || iz >= m_bitmapHeight)
		return 0.0f;

	return m_bitmapHeightData[static_cast<size_t>(iz * m_bitmapWidth + ix)];
}

// ----------------------------------------------------------------------

int GodClientTerrainEditor::sampleBitmapShader(float normalizedX, float normalizedZ) const
{
	if (m_bitmapShaderData.empty() || m_bitmapWidth == 0 || m_bitmapHeight == 0)
		return 0;

	const int ix = static_cast<int>(normalizedX * static_cast<float>(m_bitmapWidth - 1));
	const int iz = static_cast<int>(normalizedZ * static_cast<float>(m_bitmapHeight - 1));

	if (ix < 0 || ix >= m_bitmapWidth || iz < 0 || iz >= m_bitmapHeight)
		return 0;

	return m_bitmapShaderData[static_cast<size_t>(iz * m_bitmapWidth + ix)];
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::applyBitmapStamp(float worldX, float worldZ)
{
	if (m_bitmapHeightData.empty() && m_bitmapShaderData.empty())
	{
		MainFrame::getInstance().textToConsole("No bitmap stamp loaded — choose a stamp family in the Terrain dock.");
		return;
	}

	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (!terrainObject)
		return;

	const float halfSize = m_brushSize * 0.5f * m_bitmapStamp.scale;
	const float cosR = std::cos(m_bitmapStamp.rotation);
	const float sinR = std::sin(m_bitmapStamp.rotation);

	const int minX = static_cast<int>(std::floor(worldX - halfSize));
	const int maxX = static_cast<int>(std::ceil(worldX + halfSize));
	const int minZ = static_cast<int>(std::floor(worldZ - halfSize));
	const int maxZ = static_cast<int>(std::ceil(worldZ + halfSize));

	for (int iz = minZ; iz <= maxZ; ++iz)
	{
		for (int ix = minX; ix <= maxX; ++ix)
		{
			const float x = static_cast<float>(ix);
			const float z = static_cast<float>(iz);

			const float localX = x - worldX;
			const float localZ = z - worldZ;
			const float rotX = localX * cosR - localZ * sinR;
			const float rotZ = localX * sinR + localZ * cosR;

			const float normalizedX = (rotX / halfSize + 1.0f) * 0.5f;
			const float normalizedZ = (rotZ / halfSize + 1.0f) * 0.5f;

			if (normalizedX < 0.0f || normalizedX > 1.0f || normalizedZ < 0.0f || normalizedZ > 1.0f)
				continue;

			const uint64 key = (static_cast<uint64>(ix + 32768) << 32) |
			                   static_cast<uint64>(iz + 32768);

			if (m_bitmapStamp.affectsHeight && !m_bitmapHeightData.empty())
			{
				float originalHeight = 0.0f;
				Vector pos(x, 0.0f, z);
				if (terrainObject->getHeight(pos, originalHeight))
				{
					const float bitmapHeight = sampleBitmapHeight(normalizedX, normalizedZ);
					const float delta = bitmapHeight * m_bitmapStamp.heightScale;

					HeightModification mod;
					mod.worldX = x;
					mod.worldZ = z;

					HeightModificationMap::iterator it = m_heightModifications.find(key);
					if (it != m_heightModifications.end())
					{
						mod.originalHeight = it->second.originalHeight;
						mod.modifiedHeight = it->second.modifiedHeight + delta;
					}
					else
					{
						mod.originalHeight = originalHeight;
						mod.modifiedHeight = originalHeight + delta;
					}

					mod.timestamp = Clock::frameTime();
					m_heightModifications[key] = mod;
				}
			}

			if (m_bitmapStamp.affectsShader)
			{
				int shaderFamilyToApply = m_bitmapStamp.shaderFamilyId;
				if (shaderFamilyToApply == 0)
					shaderFamilyToApply = m_selectedShaderFamily;

				if (!m_bitmapShaderData.empty())
				{
					const int shaderId = sampleBitmapShader(normalizedX, normalizedZ);
					if (shaderId < 0)
						continue;
					shaderFamilyToApply = shaderId;
				}
				else
				{
					const float maskAmt = m_bitmapHeightData.empty() ? 1.0f : sampleBitmapHeight(normalizedX, normalizedZ);
					if (maskAmt <= 0.001f)
						continue;
				}

				ShaderModification mod;
				mod.worldX = x;
				mod.worldZ = z;
				mod.modifiedFamilyId = shaderFamilyToApply;
				mod.featherAmount = 1.0f;
				mod.originalFamilyId = -1;

				m_shaderModifications[key] = mod;
			}
		}
	}

	invalidateTerrainRegion(worldX, worldZ, halfSize + 16.0f);
	expandModifiedBounds(worldX, worldZ, halfSize);

	MainFrame::getInstance().textToConsole("Bitmap stamp applied");
}

// ======================================================================
// TerrainGenerator Integration
// ======================================================================

TerrainGenerator* GodClientTerrainEditor::getTerrainGenerator() const
{
	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (!terrainObject)
		return 0;

	Appearance* const appearance = terrainObject->getAppearance();
	if (!appearance)
		return 0;

	ClientProceduralTerrainAppearance* const clientTerrain = 
		dynamic_cast<ClientProceduralTerrainAppearance*>(appearance);
	if (!clientTerrain)
		return 0;

	const AppearanceTemplate* const appearanceTemplate = clientTerrain->getAppearanceTemplate();
	if (!appearanceTemplate)
		return 0;

	const ProceduralTerrainAppearanceTemplate* const terrainTemplate = 
		dynamic_cast<const ProceduralTerrainAppearanceTemplate*>(appearanceTemplate);
	if (!terrainTemplate)
		return 0;

	return const_cast<TerrainGenerator*>(terrainTemplate->getTerrainGenerator());
}

// ----------------------------------------------------------------------

bool GodClientTerrainEditor::createHeightAffectorLayer(const char* layerName)
{
	if (m_heightModifications.empty())
		return false;

	TerrainGenerator* const generator = getTerrainGenerator();
	if (!generator)
		return false;

	TerrainGenerator::Layer* const layer = new TerrainGenerator::Layer();
	layer->setName(layerName);
	layer->setActive(true);

	BoundaryRectangle* const boundary = new BoundaryRectangle();
	boundary->setName("Height Edit Boundary");
	boundary->setRectangle(m_modifiedBounds);
	boundary->setFeatherDistance(8.0f);
	layer->addBoundary(boundary);

	generator->addLayer(layer);
	m_createdLayers.push_back(layerName);

	char buffer[256];
	snprintf(buffer, sizeof(buffer), "Created height affector layer '%s' with %d modifications",
		layerName, static_cast<int>(m_heightModifications.size()));
	MainFrame::getInstance().textToConsole(buffer);

	return true;
}

// ----------------------------------------------------------------------

bool GodClientTerrainEditor::createShaderAffectorLayer(const char* layerName)
{
	if (m_shaderModifications.empty())
		return false;

	TerrainGenerator* const generator = getTerrainGenerator();
	if (!generator)
		return false;

	TerrainGenerator::Layer* const layer = new TerrainGenerator::Layer();
	layer->setName(layerName);
	layer->setActive(true);

	BoundaryRectangle* const boundary = new BoundaryRectangle();
	boundary->setName("Shader Edit Boundary");
	boundary->setRectangle(m_modifiedBounds);
	boundary->setFeatherDistance(4.0f);
	layer->addBoundary(boundary);

	AffectorShaderConstant* const affector = new AffectorShaderConstant();
	affector->setName("Shader Paint");
	affector->setFamilyId(m_selectedShaderFamily);
	layer->addAffector(affector);

	generator->addLayer(layer);
	m_createdLayers.push_back(layerName);

	char buffer[256];
	snprintf(buffer, sizeof(buffer), "Created shader affector layer '%s'", layerName);
	MainFrame::getInstance().textToConsole(buffer);

	return true;
}

// ----------------------------------------------------------------------

bool GodClientTerrainEditor::createFloraAffectorLayer(const char* layerName)
{
	if (m_floraModifications.empty())
		return false;

	TerrainGenerator* const generator = getTerrainGenerator();
	if (!generator)
		return false;

	TerrainGenerator::Layer* const layer = new TerrainGenerator::Layer();
	layer->setName(layerName);
	layer->setActive(true);

	BoundaryRectangle* const boundary = new BoundaryRectangle();
	boundary->setName("Flora Edit Boundary");
	boundary->setRectangle(m_modifiedBounds);
	boundary->setFeatherDistance(4.0f);
	layer->addBoundary(boundary);

	AffectorFloraStaticCollidableConstant* const affector = new AffectorFloraStaticCollidableConstant();
	affector->setName("Flora Paint");
	affector->setFamilyId(m_selectedFloraFamily);
	affector->setDensityOverride(true);
	affector->setDensityOverrideDensity(m_floraDensity);
	layer->addAffector(affector);

	generator->addLayer(layer);
	m_createdLayers.push_back(layerName);

	char buffer[256];
	snprintf(buffer, sizeof(buffer), "Created flora affector layer '%s'", layerName);
	MainFrame::getInstance().textToConsole(buffer);

	return true;
}

// ----------------------------------------------------------------------

bool GodClientTerrainEditor::createRoadFromPolyline(const char* name)
{
	if (m_activePolyline.controlPoints.size() < 2)
		return false;

	TerrainGenerator* const generator = getTerrainGenerator();
	if (!generator)
		return false;

	TerrainGenerator::Layer* const layer = new TerrainGenerator::Layer();
	layer->setName(name);
	layer->setActive(true);

	AffectorRoad* const road = new AffectorRoad();
	road->setName(name);
	road->setWidth(m_activePolyline.width);
	road->setFamilyId(m_activePolyline.shaderFamilyId);
	road->setFeatherDistance(m_activePolyline.featherDistance);
	road->setFeatherDistanceShader(m_activePolyline.featherDistance);
	road->setHasFixedHeights(m_activePolyline.hasFixedHeights);

	road->clearPointList();
	for (size_t i = 0; i < m_activePolyline.controlPoints.size(); ++i)
	{
		const ControlPoint& cp = m_activePolyline.controlPoints[i];
		road->addPoint(cp.position);
	}

	if (m_activePolyline.hasFixedHeights)
	{
		road->createInitialHeightList();
		ArrayList<float> heightList;
		for (size_t i = 0; i < m_activePolyline.controlPoints.size(); ++i)
		{
			heightList.add(m_activePolyline.controlPoints[i].height);
		}
		road->copyHeightList(heightList);
	}

	road->createHeightData();
	layer->addAffector(road);

	generator->addLayer(layer);
	m_createdLayers.push_back(name);

	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (terrainObject)
	{
		terrainObject->invalidateRegion(m_polylineExtent);
	}

	return true;
}

// ----------------------------------------------------------------------

bool GodClientTerrainEditor::createRibbonFromPolyline(const char* name)
{
	if (m_activePolyline.controlPoints.size() < 2)
		return false;

	TerrainGenerator* const generator = getTerrainGenerator();
	if (!generator)
		return false;

	TerrainGenerator::Layer* const layer = new TerrainGenerator::Layer();
	layer->setName(name);
	layer->setActive(true);

	AffectorRibbon* const ribbon = new AffectorRibbon();
	ribbon->setName(name);
	ribbon->setWidth(m_activePolyline.width);
	ribbon->setTerrainShaderFamilyId(m_activePolyline.shaderFamilyId);
	ribbon->setFeatherDistance(m_activePolyline.featherDistance);
	ribbon->setFeatherDistanceTerrainShader(m_activePolyline.featherDistance);

	ribbon->clearPointList();
	for (size_t i = 0; i < m_activePolyline.controlPoints.size(); ++i)
	{
		const ControlPoint& cp = m_activePolyline.controlPoints[i];
		ribbon->addPoint(cp.position);
	}

	ribbon->createInitialHeightList();
	ArrayList<float> heightList;
	for (size_t i = 0; i < m_activePolyline.controlPoints.size(); ++i)
	{
		heightList.add(m_activePolyline.controlPoints[i].height);
	}
	ribbon->copyHeightList(heightList);

	layer->addAffector(ribbon);

	generator->addLayer(layer);
	m_createdLayers.push_back(name);

	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (terrainObject)
	{
		terrainObject->invalidateRegion(m_polylineExtent);
	}

	return true;
}

// ----------------------------------------------------------------------

bool GodClientTerrainEditor::createEnvironmentZoneAffector(const char* name)
{
	if (m_activeEnvironmentZone.boundaryPoints.size() < 3)
		return false;

	TerrainGenerator* const generator = getTerrainGenerator();
	if (!generator)
		return false;

	TerrainGenerator::Layer* const layer = new TerrainGenerator::Layer();
	layer->setName(name);
	layer->setActive(true);

	BoundaryPolygon* const boundary = new BoundaryPolygon();
	boundary->setName("Environment Zone Boundary");
	boundary->clearPointList();
	for (size_t i = 0; i < m_activeEnvironmentZone.boundaryPoints.size(); ++i)
	{
		boundary->addPoint(m_activeEnvironmentZone.boundaryPoints[i]);
	}
	boundary->setFeatherDistance(m_activeEnvironmentZone.featherDistance);
	layer->addBoundary(boundary);

	generator->addLayer(layer);
	m_createdLayers.push_back(name);

	return true;
}

// ----------------------------------------------------------------------

bool GodClientTerrainEditor::exportModificationsToLayer(const char* layerName)
{
	TerrainGenerator* const generator = getTerrainGenerator();
	if (!generator)
		return false;

	bool success = false;

	if (!m_heightModifications.empty())
	{
		char heightLayerName[256];
		snprintf(heightLayerName, sizeof(heightLayerName), "%s_Height", layerName);
		success |= createHeightAffectorLayer(heightLayerName);
	}

	if (!m_shaderModifications.empty())
	{
		char shaderLayerName[256];
		snprintf(shaderLayerName, sizeof(shaderLayerName), "%s_Shader", layerName);
		success |= createShaderAffectorLayer(shaderLayerName);
	}

	if (!m_floraModifications.empty())
	{
		char floraLayerName[256];
		snprintf(floraLayerName, sizeof(floraLayerName), "%s_Flora", layerName);
		success |= createFloraAffectorLayer(floraLayerName);
	}

	if (success)
	{
		char buffer[256];
		snprintf(buffer, sizeof(buffer), "Exported modifications to layer group '%s'", layerName);
		MainFrame::getInstance().textToConsole(buffer);
	}

	return success;
}

// ----------------------------------------------------------------------

bool GodClientTerrainEditor::exportPolylineToFile(const char* filename) const
{
	if (m_activePolyline.controlPoints.empty())
		return false;

	std::ofstream file(filename);
	if (!file.is_open())
		return false;

	file << "POLYLINE_V1\n";
	file << "TYPE " << (m_activePolyline.isRibbon ? "RIBBON" : "ROAD") << "\n";
	file << "NAME " << m_activePolyline.name << "\n";
	file << "WIDTH " << m_activePolyline.width << "\n";
	file << "SHADER " << m_activePolyline.shaderFamilyId << "\n";
	file << "FEATHER " << m_activePolyline.featherDistance << "\n";
	file << "FIXEDHEIGHTS " << (m_activePolyline.hasFixedHeights ? 1 : 0) << "\n";
	file << "POINTS " << m_activePolyline.controlPoints.size() << "\n";

	for (size_t i = 0; i < m_activePolyline.controlPoints.size(); ++i)
	{
		const ControlPoint& cp = m_activePolyline.controlPoints[i];
		file << cp.position.x << " " << cp.position.y << " " << cp.height << " " << cp.width << "\n";
	}

	file.close();
	return true;
}

// ----------------------------------------------------------------------

bool GodClientTerrainEditor::importPolylineFromFile(const char* filename)
{
	std::ifstream file(filename);
	if (!file.is_open())
		return false;

	std::string header;
	file >> header;
	if (header != "POLYLINE_V1")
	{
		file.close();
		return false;
	}

	m_activePolyline.controlPoints.clear();

	std::string token;
	while (file >> token)
	{
		if (token == "TYPE")
		{
			std::string typeStr;
			file >> typeStr;
			m_activePolyline.isRibbon = (typeStr == "RIBBON");
		}
		else if (token == "NAME")
		{
			std::getline(file, m_activePolyline.name);
			while (!m_activePolyline.name.empty() && m_activePolyline.name[0] == ' ')
				m_activePolyline.name.erase(0, 1);
		}
		else if (token == "WIDTH")
		{
			file >> m_activePolyline.width;
		}
		else if (token == "SHADER")
		{
			file >> m_activePolyline.shaderFamilyId;
		}
		else if (token == "FEATHER")
		{
			file >> m_activePolyline.featherDistance;
		}
		else if (token == "FIXEDHEIGHTS")
		{
			int val;
			file >> val;
			m_activePolyline.hasFixedHeights = (val != 0);
		}
		else if (token == "POINTS")
		{
			int count;
			file >> count;
			for (int i = 0; i < count; ++i)
			{
				ControlPoint cp;
				double px, pz;
				file >> px >> pz >> cp.height >> cp.width;
				cp.position = Vector2d(px, pz);
				m_activePolyline.controlPoints.push_back(cp);
			}
		}
	}

	file.close();
	recalculatePolylineExtent();
	m_polylineEditMode = PEM_AddPoints;

	return true;
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::expandModifiedBounds(float worldX, float worldZ, float radius)
{
	if (!m_hasModifiedBounds)
	{
		m_modifiedBounds = Rectangle2d(
			worldX - radius, worldZ - radius,
			worldX + radius, worldZ + radius);
		m_hasModifiedBounds = true;
	}
	else
	{
		if (worldX - radius < m_modifiedBounds.x0)
			m_modifiedBounds.x0 = worldX - radius;
		if (worldZ - radius < m_modifiedBounds.y0)
			m_modifiedBounds.y0 = worldZ - radius;
		if (worldX + radius > m_modifiedBounds.x1)
			m_modifiedBounds.x1 = worldX + radius;
		if (worldZ + radius > m_modifiedBounds.y1)
			m_modifiedBounds.y1 = worldZ + radius;
	}
}

// ======================================================================
