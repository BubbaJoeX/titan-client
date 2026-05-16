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
#include "sharedImage/ImageFormatList.h"
#include "sharedMath/Rectangle2d.h"
#include "sharedMath/PackedRgb.h"
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
#include "sharedTerrain/Affector.h"
#include "sharedTerrain/AffectorHeight.h"
#include "sharedTerrain/Boundary.h"

#include <cmath>
#include <cfloat>
#include <cstring>
#include <algorithm>
#include <set>

#include "TerrainDock.h"

namespace
{
	Vector godClientTerrainObjectXZHorizontal(TerrainObject const* const terrainObject, float const worldX, float const worldZ)
	{
		if (!terrainObject)
			return Vector(worldX, 0.f, worldZ);
		return terrainObject->rotateTranslate_w2o(Vector(worldX, 0.f, worldZ));
	}

	Vector godClientTerrainObjectSampled_w2o(TerrainObject const* const terrainObject, float const worldX, float const worldZ)
	{
		if (!terrainObject)
			return Vector(worldX, 0.f, worldZ);
		Vector pos_w(worldX, 0.f, worldZ);
		float hy = 0.f;
		if (!terrainObject->getHeight(pos_w, hy))
			hy = 0.f;
		pos_w.y = hy;
		return terrainObject->rotateTranslate_w2o(pos_w);
	}

	uint64 godClientTerrainPaintCellKey(float const objectSpaceX, float const objectSpaceZ)
	{
		int const kx = static_cast<int>(std::floor(objectSpaceX));
		int const kz = static_cast<int>(std::floor(objectSpaceZ));
		return (static_cast<uint64>(kx + 32768) << 32) |
			static_cast<uint64>(kz + 32768);
	}

	inline void godClientTerrainPaintDecodeKey(uint64 const key, int& objectCellX, int& objectCellZ)
	{
		objectCellX = static_cast<int>((key >> 32) & 0xffffffffULL) - 32768;
		objectCellZ = static_cast<int>(key & 0xffffffffULL) - 32768;
	}

	inline uint64 godClientTerrainPaintKeyFromWorld(TerrainObject const* const terrainObject, float const worldX, float const worldZ)
	{
		Vector const objectPos = godClientTerrainObjectSampled_w2o(terrainObject, worldX, worldZ);
		return godClientTerrainPaintCellKey(objectPos.x, objectPos.z);
	}

	inline Rectangle2d godClientNormalizeRect2d(Rectangle2d const& r)
	{
		return Rectangle2d(
			std::min(r.x0, r.x1), std::min(r.y0, r.y1),
			std::max(r.x0, r.x1), std::max(r.y0, r.y1));
	}

	inline Rectangle2d godClientUnionRects(Rectangle2d const& aRaw, Rectangle2d const& bRaw)
	{
		Rectangle2d const a = godClientNormalizeRect2d(aRaw);
		Rectangle2d const b = godClientNormalizeRect2d(bRaw);
		return Rectangle2d(
			std::min(a.x0, b.x0), std::min(a.y0, b.y0),
			std::max(a.x1, b.x1), std::max(a.y1, b.y1));
	}

	inline bool gdIsFiniteTerrainScalar(float const v)
	{
		return v == v && std::fabs(static_cast<double>(v)) < 1.0e24;
	}

	inline bool gdIsValidWorldFootprint(Rectangle2d const& n)
	{
		return gdIsFiniteTerrainScalar(n.x0) && gdIsFiniteTerrainScalar(n.x1) && gdIsFiniteTerrainScalar(n.y0) &&
			gdIsFiniteTerrainScalar(n.y1) && n.x0 <= n.x1 && n.y0 <= n.y1;
	}

	/// Clamp live-edit footprints to sane world XZ; NaNs / inversions corrupt BoundaryRectangle union and procedural rebuild.
	inline Rectangle2d gdSanitizeTerrainWorldFootprint(TerrainObject const* const toeConst, Rectangle2d const& rawExtent)
	{
		Rectangle2d const normalized = godClientNormalizeRect2d(rawExtent);
		float halfClamp = 16384.f;
		if (toeConst)
			halfClamp = std::max(halfClamp, toeConst->getMapWidthInMeters() * 0.5f + 8192.f);

		if (!gdIsValidWorldFootprint(normalized))
			return Rectangle2d(-halfClamp, -halfClamp, halfClamp, halfClamp);

		float x0 = std::max(-halfClamp, normalized.x0);
		float x1 = std::min(halfClamp, normalized.x1);
		float z0 = std::max(-halfClamp, normalized.y0);
		float z1 = std::min(halfClamp, normalized.y1);
		if (!(x0 < x1 && z0 < z1))
			return Rectangle2d(-halfClamp, -halfClamp, halfClamp, halfClamp);

		return Rectangle2d(x0, z0, x1, z1);
	}

	int s_godWaterLayerSerial = 0;
	int s_godPolygonLayerSerial = 0;
	int s_godProceduralAuthoringSerial = 0;

	Rectangle2d godClientBoundsFromPoints(std::vector<Vector2d> const& pts, float pad)
	{
		if (pts.empty())
			return Rectangle2d();
		float minX = static_cast<float>(pts[0].x);
		float minZ = static_cast<float>(pts[0].y);
		float maxX = minX;
		float maxZ = minZ;
		for (size_t i = 1; i < pts.size(); ++i)
		{
			float const x = static_cast<float>(pts[i].x);
			float const z = static_cast<float>(pts[i].y);
			if (x < minX) minX = x;
			if (x > maxX) maxX = x;
			if (z < minZ) minZ = z;
			if (z > maxZ) maxZ = z;
		}
		return Rectangle2d(minX - pad, minZ - pad, maxX + pad, maxZ + pad);
	}

	inline double gdClamp01d(double const t)
	{
		if (t < 0.0)
			return 0.0;
		if (t > 1.0)
			return 1.0;
		return t;
	}

	inline bool gdBuildGrey01RowMajorFromImage(Image const& image, std::vector<float>& outGrey01, int& outW, int& outH)
	{
		outGrey01.clear();
		outW = 0;
		outH = 0;
		int const w = image.getWidth();
		int const h = image.getHeight();
		if (w < 2 || h < 2)
			return false;

		uint8 const* base = image.lockReadOnly(true);
		if (!base)
			return false;
		Image const* const imagePtr = &image;
		Image::UnlockGuard const guard(imagePtr);

		Image::PixelFormat const pf = image.getPixelFormat();
		int const bpp = image.getBytesPerPixel();
		int const stride = image.getStride();
		outGrey01.resize(static_cast<size_t>(w * h));
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
				double const lum = (0.299 * static_cast<double>(r) + 0.587 * static_cast<double>(g) + 0.114 * static_cast<double>(b)) / 255.0;
				outGrey01[static_cast<size_t>(z * w + x)] = static_cast<float>(lum);
				p += bpp;
			}
		}
		outW = w;
		outH = h;
		return true;
	}

	inline float gdBilinearGrey01(std::vector<float> const& g01, int const w, int const h, double const colWest01, double const rowNorthTop01)
	{
		if (w < 2 || h < 2 || g01.size() < static_cast<size_t>(w * h))
			return 0.f;

		double const cx = gdClamp01d(colWest01) * static_cast<double>(w - 1);
		double const rowT = gdClamp01d(rowNorthTop01);
		double const ry = rowT * static_cast<double>(h - 1);

		int const x0 = static_cast<int>(std::floor(cx));
		int const y0 = static_cast<int>(std::floor(ry));
		int const x1 = std::min(x0 + 1, w - 1);
		int const y1 = std::min(y0 + 1, h - 1);

		double const fx = cx - std::floor(cx);
		double const fy = ry - std::floor(ry);

		double const s00 = static_cast<double>(g01[static_cast<size_t>(y0 * w + x0)]);
		double const s10 = static_cast<double>(g01[static_cast<size_t>(y0 * w + x1)]);
		double const s01 = static_cast<double>(g01[static_cast<size_t>(y1 * w + x0)]);
		double const s11 = static_cast<double>(g01[static_cast<size_t>(y1 * w + x1)]);

		double const v = s00 * (1. - fx) * (1. - fy) +
			s10 * fx * (1. - fy) +
			s01 * (1. - fx) * fy +
			s11 * fx * fy;

		return static_cast<float>(v);
	}
}

#include "sharedTerrain/ShaderGroup.h"
#include "sharedTerrain/FloraGroup.h"
#include "sharedTerrain/RadialGroup.h"
#include "sharedTerrain/EnvironmentGroup.h"
#include "sharedTerrain/BitmapGroup.h"

#include "MainFrame.h"

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

	void addTerrainHeightDebugCircle(
		TerrainObject const& terrain,
		Camera const& camera,
		float const cx,
		float const cz,
		float const r,
		VectorArgb const& color,
		int const segments)
	{
		if (r < 0.01f)
			return;

		int const seg = std::max(12, std::min(128, segments));
		static float const kTwoPi = 6.28318530717958647692f;

		for (int i = 0; i < seg; ++i)
		{
			float const a0 = (static_cast<float>(i) / static_cast<float>(seg)) * kTwoPi;
			float const a1 = (static_cast<float>(i + 1) / static_cast<float>(seg)) * kTwoPi;
			float const x0 = cx + std::cos(a0) * r;
			float const z0 = cz + std::sin(a0) * r;
			float const x1 = cx + std::cos(a1) * r;
			float const z1 = cz + std::sin(a1) * r;
			addTerrainHeightDebugLineStrip(terrain, camera, x0, z0, x1, z1, color);
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
// ~40Hz coalesced chunk invalidation while stroking (height + overlay tools). Lower = snappier live mesh, more rebuild work.
const float GodClientTerrainEditor::REALTIME_INVALIDATION_INTERVAL = 0.025f;

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
	CityTerrainLayerManager::setExternalVertexColorModifierCallback(&GodClientTerrainEditor::getModifiedVertexColor);
	CityTerrainLayerManager::setExternalFloraModifierCallback(&GodClientTerrainEditor::getModifiedFlora);
	CityTerrainLayerManager::setExternalRadialModifierCallback(&GodClientTerrainEditor::getModifiedRadial);
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::remove()
{
	DEBUG_FATAL(ms_instance == 0, ("GodClientTerrainEditor not installed"));

	// Unregister the height modifier callback
	CityTerrainLayerManager::clearExternalHeightModifierCallback();

	// Unregister shader and flora modifier callbacks
	CityTerrainLayerManager::clearExternalShaderModifierCallback();
	CityTerrainLayerManager::clearExternalVertexColorModifierCallback();
	CityTerrainLayerManager::clearExternalFloraModifierCallback();
	CityTerrainLayerManager::clearExternalRadialModifierCallback();

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

GodClientTerrainEditor* GodClientTerrainEditor::getInstanceNullable()
{
	return ms_instance;
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
	m_raiseLowerSpeed(0.25f),
	m_raiseLowerBias(0.f),
	m_raiseLowerClickRate(1.f),
	m_raiseLowerJitter(0.f),
	m_targetHeight(0.0f),
	m_noiseAmplitude(1.0f),
	m_noiseFrequency(0.1f),
	m_selectedShaderFamily(0),
	m_selectedFloraFamily(0),
	m_selectedRadialFamily(0),
	m_floraCollidable(false),
	m_floraDensity(1.0f),
	m_shaderPaintTintMode(false),
	m_shaderPaintTintR(255),
	m_shaderPaintTintG(255),
	m_shaderPaintTintB(255),
	m_brushPreviewEnabled(true),
	m_cursorWorldPosition(Vector::zero),
	m_cursorPositionValid(false),
	m_brushStrokeActive(false),
	m_currentStroke(),
	m_lastStrokeX(0.0f),
	m_lastStrokeZ(0.0f),
	m_heightModifications(),
	m_shaderModificationMutex(),
	m_shaderModifications(),
	m_vertexColorModifications(),
	m_floraModifications(),
	m_radialModifications(),
	m_undoStack(),
	m_redoStack(),
	m_shaderUndoBatch(0),
	m_shaderStrokePending(),
	m_shaderStrokePendingKeys(),
	m_vertexColorStrokePending(),
	m_vertexColorStrokePendingKeys(),
	m_hasRegionSelection(false),
	m_regionMinX(0.0f),
	m_regionMinZ(0.0f),
	m_regionMaxX(0.0f),
	m_regionMaxZ(0.0f),
	m_regionSelectionCircular(false),
	m_regionCircleCenterX(0.0f),
	m_regionCircleCenterZ(0.0f),
	m_regionCircleRadius(0.0f),
	m_lastModificationTime(-1.0e9f),
	m_lastInvalidationTime(0.0f),
	m_dirtyRegionMinX(0.0f),
	m_dirtyRegionMinZ(0.0f),
	m_dirtyRegionMaxX(0.0f),
	m_dirtyRegionMaxZ(0.0f),
	m_hasDirtyRegion(false),
	m_liveStrokeInvalidateHasPrior(false),
	m_liveStrokeInvalidatePriorX(0.0f),
	m_liveStrokeInvalidatePriorZ(0.0f),
	m_polylineEditMode(PEM_None),
	m_activePolyline(),
	m_selectedPolylinePoint(-1),
	m_polylineExtent(),
	m_activeEnvironmentZone(),
	m_polygonDrawPurpose(PDP_None),
	m_environmentFamilyId(0),
	m_bitmapStamp(),
	m_bitmapHeightData(),
	m_bitmapShaderData(),
	m_bitmapWidth(0),
	m_bitmapHeight(0),
	m_modifiedBounds(),
	m_hasModifiedBounds(false),
	m_createdLayers(),
	m_waterPlacementHeight(0.0f),
	m_waterPlacementShaderTemplate(),
	m_ribbonWaterShaderTemplate(),
	m_lastWaterDabTime(-1.0e9f)
{
	m_activePolyline.width = 8.0f;
	m_activePolyline.shaderFamilyId = 0;
	m_activePolyline.featherDistance = 4.0f;
	m_activePolyline.hasFixedHeights = false;
	m_activePolyline.isRibbon = false;
	m_activePolyline.commitKind = PCK_RoadRibbon;
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
	m_activePolyline.controlPoints.clear();
	m_activeEnvironmentZone.boundaryPoints.clear();
	m_bitmapHeightData.clear();
	m_bitmapShaderData.clear();
	m_createdLayers.clear();
}

// ----------------------------------------------------------------------

GodClientTerrainEditor::ShaderMapLock::ShaderMapLock(GodClientTerrainEditor& editor) :
	m_mutex(editor.m_shaderModificationMutex)
{
	m_mutex.enter();
}

GodClientTerrainEditor::ShaderMapLock::~ShaderMapLock()
{
	m_mutex.leave();
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

void GodClientTerrainEditor::setRaiseLowerSpeed(float metersPerDab)
{
	m_raiseLowerSpeed = std::max(0.005f, std::min(5.0f, metersPerDab));
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::setRaiseLowerBias(float bias)
{
	m_raiseLowerBias = std::max(-1.0f, std::min(1.0f, bias));
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::setRaiseLowerClickRate(float multiplier)
{
	m_raiseLowerClickRate = std::max(0.25f, std::min(4.0f, multiplier));
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::setRaiseLowerJitter(float jitter)
{
	m_raiseLowerJitter = std::max(0.f, std::min(1.0f, jitter));
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
	m_brushFeather = std::max(0.f, std::min(1.0f, feather));
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

void GodClientTerrainEditor::setSelectedShaderFamily(int familyId)
{
	m_selectedShaderFamily = familyId;
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::setShaderPaintTintMode(bool const enabled)
{
	m_shaderPaintTintMode = enabled;
}

// ----------------------------------------------------------------------

bool GodClientTerrainEditor::getShaderPaintTintMode() const
{
	return m_shaderPaintTintMode;
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::setShaderPaintTintRgb(uint8 const r, uint8 const g, uint8 const b)
{
	m_shaderPaintTintR = r;
	m_shaderPaintTintG = g;
	m_shaderPaintTintB = b;
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::setSelectedFloraFamily(int familyId)
{
	m_selectedFloraFamily = familyId;
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::setSelectedRadialFamily(int familyId)
{
	m_selectedRadialFamily = familyId;
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

	m_shaderStrokePending.clear();
	m_shaderStrokePendingKeys.clear();
	m_vertexColorStrokePending.clear();
	m_vertexColorStrokePendingKeys.clear();

	// Reset dirty region tracking for new stroke
	m_hasDirtyRegion = false;
	m_liveStrokeInvalidateHasPrior = false;

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
	m_currentStroke.shaderStrokeRecords.clear();
	m_currentStroke.vertexColorStrokeRecords.clear();

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

	// Apply on every drag sample so behavior matches repeated clicks along the path. Mesh invalidation
	// is still coalesced in invalidateTerrainMeshesForLiveBrushSample (REALTIME interval + rolling union).
	applyBrushAtPoint(worldX, worldZ);
	m_lastStrokeX = worldX;
	m_lastStrokeZ = worldZ;
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::endBrushStroke()
{
	if (!m_brushStrokeActive)
		return;

	m_brushStrokeActive = false;

	sealShaderStrokeRecords(m_currentStroke);
	sealVertexColorStrokeRecords(m_currentStroke);

	// Push stroke to undo stack
	if (!m_currentStroke.modifications.empty() || !m_currentStroke.shaderStrokeRecords.empty() ||
		!m_currentStroke.vertexColorStrokeRecords.empty())
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

	m_liveStrokeInvalidateHasPrior = false;

	// Flush any remaining terrain changes
	flushTerrainChanges();
	nudgeGodClientCameraToRefreshDpvs();
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::applyShaderPaintDab(float worldX, float worldZ, int shaderFamilyId, float strength)
{
	modifyShaderPaint(worldX, worldZ, shaderFamilyId, strength);
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::applyVertexColorPaintDab(float worldX, float worldZ, PackedRgb const& rgb, float strength)
{
	modifyVertexColorPaint(worldX, worldZ, rgb, strength);
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::applyBrushAtPoint(float worldX, float worldZ)
{
	if (m_toolMode == TM_None)
		return;

	const float currentTime = Clock::frameTime();

	// Height tools are throttled between clicks; overlay painting (shader/flora/water/radial/stamp)
	// is cheap and needs per-click application even if a stroke was not started yet.
	bool const overlayTool =
		m_toolMode == TM_PaintShader ||
		m_toolMode == TM_PaintFlora ||
		m_toolMode == TM_PlaceWater ||
		m_toolMode == TM_PlaceRadial ||
		m_toolMode == TM_StampBitmap;

	bool const raiseOrLower = (m_toolMode == TM_Raise || m_toolMode == TM_Lower);

	// While dragging a stroke, apply on every sample (no time throttle) so the mesh can track the brush.
	if (!m_brushStrokeActive && !overlayTool)
	{
		float minInterval = MIN_MODIFICATION_INTERVAL;
		if (raiseOrLower)
			minInterval /= m_raiseLowerClickRate;
		if (currentTime - m_lastModificationTime < minInterval)
			return;
	}
	m_lastModificationTime = currentTime;

	// Apply based on tool mode
	switch (m_toolMode)
	{
		case TM_Raise:
			modifyHeightRaise(worldX, worldZ);
			break;

		case TM_Lower:
			modifyHeightLower(worldX, worldZ);
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
			if (m_shaderPaintTintMode)
				modifyVertexColorPaint(worldX, worldZ, PackedRgb(m_shaderPaintTintR, m_shaderPaintTintG, m_shaderPaintTintB), m_brushStrength);
			else
				modifyShaderPaint(worldX, worldZ, m_selectedShaderFamily, m_brushStrength);
			break;

		case TM_PlaceWater:
			placeWaterBrushDab(worldX, worldZ, currentTime);
			break;

		case TM_PaintFlora:
			modifyFloraPaint(worldX, worldZ, m_selectedFloraFamily, m_floraDensity, m_floraCollidable);
			break;

		case TM_PlaceRadial:
			modifyRadialPaint(worldX, worldZ, m_selectedRadialFamily, m_floraDensity);
			break;

		case TM_StampBitmap:
			applyBitmapStamp(worldX, worldZ);
			break;

		default:
			break;
	}

	// Track modified region (export bounds)
	expandModifiedBounds(worldX, worldZ, m_brushSize * 0.5f);

	const float halfBrush = m_brushSize * 0.5f;
	const float margin = 16.0f;
	const float regionRadius = halfBrush + margin;

	accumulateStrokeFinalizeDirtyRect(worldX, worldZ, regionRadius);

	// Live stroke: throttled rolling invalidates + AOI sync while dragging; endBrushStroke() also invalidates full stroke AABB.
	invalidateTerrainMeshesForLiveBrushSample(worldX, worldZ, regionRadius, currentTime);
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::modifyHeightRaise(float worldX, float worldZ)
{
	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (!terrainObject)
		return;

	const float halfBrush = m_brushSize * 0.5f;

	float jitterMul = 1.f;
	if (m_raiseLowerJitter > 1e-5f)
	{
		float const jitter = (Random::randomReal() * 2.f - 1.f) * m_raiseLowerJitter;
		jitterMul = std::max(0.1f, 1.f + jitter);
	}

	const float avgHeight = getAverageHeight(worldX, worldZ, halfBrush);
	const float spread = std::max(0.5f, m_brushSize * 0.12f);

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
					float t = (originalHeight - avgHeight) / spread;
					t = std::max(-1.f, std::min(1.f, t));
					float biasWeight = 1.f + m_raiseLowerBias * t * 0.9f;
					biasWeight = std::max(0.05f, biasWeight);

					const float delta = m_raiseLowerSpeed * effect * biasWeight * jitterMul;

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

void GodClientTerrainEditor::modifyHeightLower(float worldX, float worldZ)
{
	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (!terrainObject)
		return;

	const float halfBrush = m_brushSize * 0.5f;

	float jitterMul = 1.f;
	if (m_raiseLowerJitter > 1e-5f)
	{
		float const jitter = (Random::randomReal() * 2.f - 1.f) * m_raiseLowerJitter;
		jitterMul = std::max(0.1f, 1.f + jitter);
	}

	const float avgHeight = getAverageHeight(worldX, worldZ, halfBrush);
	const float spread = std::max(0.5f, m_brushSize * 0.12f);

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
					float t = (originalHeight - avgHeight) / spread;
					t = std::max(-1.f, std::min(1.f, t));
					float biasWeight = 1.f + m_raiseLowerBias * t * 0.9f;
					biasWeight = std::max(0.05f, biasWeight);

					const float delta = -m_raiseLowerSpeed * effect * biasWeight * jitterMul;

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
	const float radius = m_brushSize * 0.5f;

	// Feather at 0%: crisp brush footprint (no additional edge softening / exponent curve).
	if (m_brushFeather <= 1.e-7f && radius > 1.e-6f)
	{
		switch (m_brushShape)
		{
		case BS_Square:
			return (std::fabs(localX) <= radius && std::fabs(localZ) <= radius) ? 1.0f : 0.0f;

		case BS_Circle:
		default:
		{
			float const dsq = localX * localX + localZ * localZ;
			return (dsq <= radius * radius + 1.e-6f) ? 1.0f : 0.0f;
		}
		}
	}

	float distance = 0.0f;

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

void GodClientTerrainEditor::accumulateStrokeFinalizeDirtyRect(float worldX, float worldZ, float regionRadius)
{
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
		if (worldX - regionRadius < m_dirtyRegionMinX)
			m_dirtyRegionMinX = worldX - regionRadius;
		if (worldZ - regionRadius < m_dirtyRegionMinZ)
			m_dirtyRegionMinZ = worldZ - regionRadius;
		if (worldX + regionRadius > m_dirtyRegionMaxX)
			m_dirtyRegionMaxX = worldX + regionRadius;
		if (worldZ + regionRadius > m_dirtyRegionMaxZ)
			m_dirtyRegionMaxZ = worldZ + regionRadius;
	}
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::invalidateTerrainMeshesForLiveBrushSample(float worldX, float worldZ, float regionRadius, float currentTime)
{
	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (!terrainObject)
		return;

	const bool heightStrokeTool =
		m_brushStrokeActive &&
		(m_toolMode == TM_Raise || m_toolMode == TM_Lower || m_toolMode == TM_Flatten ||
		 m_toolMode == TM_Smooth || m_toolMode == TM_Noise || m_toolMode == TM_SetHeight);

	const bool overlayPaintStroke =
		m_brushStrokeActive &&
		(m_toolMode == TM_PaintShader || m_toolMode == TM_PaintFlora || m_toolMode == TM_PlaceWater ||
		 m_toolMode == TM_PlaceRadial || m_toolMode == TM_StampBitmap);

	const bool useRollingLivePolicy = m_brushStrokeActive && (heightStrokeTool || overlayPaintStroke);

	if (useRollingLivePolicy)
	{
		// No time throttle here: coalescing was starving live God edits (most drag frames skipped invalidate;
		// mesh only caught up on mouse-up). Rolling union below still bounds each invalidate to the stroke segment.
		Rectangle2d invalidateRect;
		if (m_liveStrokeInvalidateHasPrior)
		{
			float const ax = m_liveStrokeInvalidatePriorX;
			float const az = m_liveStrokeInvalidatePriorZ;
			float const bx = worldX;
			float const bz = worldZ;
			float const xmin = std::min(ax, bx) - regionRadius;
			float const xmax = std::max(ax, bx) + regionRadius;
			float const zmin = std::min(az, bz) - regionRadius;
			float const zmax = std::max(az, bz) + regionRadius;
			invalidateRect = Rectangle2d(xmin, zmin, xmax, zmax);
		}
		else
		{
			invalidateRect = Rectangle2d(
				worldX - regionRadius,
				worldZ - regionRadius,
				worldX + regionRadius,
				worldZ + regionRadius);
		}

		godClientSyncLiveStagingAoiLayer(invalidateRect);
		terrainObject->invalidateRegion(invalidateRect);

		m_liveStrokeInvalidatePriorX = worldX;
		m_liveStrokeInvalidatePriorZ = worldZ;
		m_liveStrokeInvalidateHasPrior = true;
		m_lastInvalidationTime = currentTime;
		GodClientTerrainEditor::nudgeGodClientCameraToRefreshDpvs ();
		return;
	}

	invalidateTerrainRegion(worldX, worldZ, regionRadius);
	m_lastInvalidationTime = currentTime;
	nudgeGodClientCameraToRefreshDpvs();
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::setWaterPlacementHeight(float height)
{
	m_waterPlacementHeight = height;
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::setWaterPlacementShaderTemplate(char const* shaderTemplateName)
{
	if (shaderTemplateName && *shaderTemplateName)
		m_waterPlacementShaderTemplate = shaderTemplateName;
	else
		m_waterPlacementShaderTemplate.clear();
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::setRibbonWaterShaderTemplate(char const* shaderTemplateName)
{
	if (shaderTemplateName && *shaderTemplateName)
		m_ribbonWaterShaderTemplate = shaderTemplateName;
	else
		m_ribbonWaterShaderTemplate.clear();
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::installLocalWaterTableAxisAligned(float centerWorldX, float centerWorldZ, float halfExtentSquare, float tableHeight)
{
	TerrainGenerator* const generator = getTerrainGenerator();
	if (!generator)
	{
		MainFrame::getInstance().textToConsole("installLocalWaterTableAxisAligned: No terrain generator available.");
		return;
	}

	float const hs = std::max(1.0f, halfExtentSquare);
	char layerBuf[128];
	snprintf(layerBuf, sizeof(layerBuf), "GodWater_%06d", s_godWaterLayerSerial++);
	TerrainGenerator::Layer* const layer = new TerrainGenerator::Layer();
	layer->setName(layerBuf);
	layer->setActive(true);

	char boundaryBuf[128];
	snprintf(boundaryBuf, sizeof(boundaryBuf), "%s_bc", layerBuf);

	BoundaryRectangle* const boundary = new BoundaryRectangle();
	boundary->setName(boundaryBuf);
	boundary->setActive(true);
	boundary->setRectangle(Rectangle2d(centerWorldX - hs, centerWorldZ - hs, centerWorldX + hs, centerWorldZ + hs));
	boundary->setLocalWaterTable(true);
	boundary->setLocalGlobalWaterTable(false);
	boundary->setLocalWaterTableHeight(tableHeight);
	boundary->setLocalWaterTableShaderSize(8.f);

	std::string const shaderName = (!m_waterPlacementShaderTemplate.empty() ? m_waterPlacementShaderTemplate : std::string("wter_ocean_water"));
	boundary->setLocalWaterTableShaderTemplateName(shaderName.c_str());

	layer->addBoundary(boundary);
	generator->addLayer(layer);
	generator->prepare();

	TerrainObject* const terrainObject = TerrainObject::getInstance();
	Appearance* const appearance = (terrainObject ? terrainObject->getAppearance() : 0);
	ClientProceduralTerrainAppearance* const proceduralAppearance = dynamic_cast<ClientProceduralTerrainAppearance*>(appearance);
	if (proceduralAppearance)
		proceduralAppearance->rebuildLocalWaterTablesFromTerrainGenerator();

	if (terrainObject)
	{
		float const invalidatePad = hs + 128.f;
		terrainObject->invalidateRegion(
			Rectangle2d(centerWorldX - invalidatePad,
			             centerWorldZ - invalidatePad,
			             centerWorldX + invalidatePad,
			             centerWorldZ + invalidatePad));
	}

	nudgeGodClientCameraToRefreshDpvs();
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::placeWaterBrushDab(float worldX, float worldZ, float currentFrameTime)
{
	static float const minIntervalSeconds = 0.11f;
	if ((currentFrameTime - m_lastWaterDabTime < minIntervalSeconds) && m_brushStrokeActive)
		return;

	m_lastWaterDabTime = currentFrameTime;
	float const halfBrush = std::max(1.5f, m_brushSize * 0.5f);
	installLocalWaterTableAxisAligned(worldX, worldZ, halfBrush, m_waterPlacementHeight);
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

	restoreShaderModificationsFromStrokeRecords(stroke.shaderStrokeRecords, true);
	restoreVertexColorModificationsFromStrokeRecords(stroke.vertexColorStrokeRecords, true);

	m_redoStack.push_back(stroke);

	flushTerrainChanges();
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

	restoreShaderModificationsFromStrokeRecords(stroke.shaderStrokeRecords, false);
	restoreVertexColorModificationsFromStrokeRecords(stroke.vertexColorStrokeRecords, false);

	m_undoStack.push_back(stroke);

	flushTerrainChanges();
	nudgeGodClientCameraToRefreshDpvs();

	MainFrame::getInstance().textToConsole("Redo: Terrain modification reapplied");
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::clearHistory()
{
	ShaderMapLock shaderLock(*this);
	m_undoStack.clear();
	m_redoStack.clear();
	m_heightModifications.clear();
	m_shaderModifications.clear();
	m_vertexColorModifications.clear();
	m_floraModifications.clear();
	m_radialModifications.clear();
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::flushTerrainChanges()
{
	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (!terrainObject)
		return;
	if (!terrainObject->getAppearance())
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

	// Shader / flora / radial keys use terrain object-space XZ (see modify*Paint). Invalidate in world space.
	typedef std::vector<std::pair<uint64, ShaderModification> > ShaderSnapshotVector;
	ShaderSnapshotVector shaderSnapshot;
	{
		ShaderMapLock shaderLock(*this);
		shaderSnapshot.assign(m_shaderModifications.begin(), m_shaderModifications.end());
	}
	for (ShaderSnapshotVector::const_iterator it = shaderSnapshot.begin(); it != shaderSnapshot.end(); ++it)
	{
		ShaderModification const& mod = it->second;
		Vector const w = terrainObject->rotateTranslate_o2w(Vector(mod.worldX, 0.f, mod.worldZ));
		hasBounds = true;
		if (w.x < minX) minX = w.x;
		if (w.x > maxX) maxX = w.x;
		if (w.z < minZ) minZ = w.z;
		if (w.z > maxZ) maxZ = w.z;
	}

	for (FloraModificationMap::const_iterator it = m_floraModifications.begin(); it != m_floraModifications.end(); ++it)
	{
		FloraModification const& mod = it->second;
		Vector const w = terrainObject->rotateTranslate_o2w(Vector(mod.worldX, 0.f, mod.worldZ));
		hasBounds = true;
		if (w.x < minX) minX = w.x;
		if (w.x > maxX) maxX = w.x;
		if (w.z < minZ) minZ = w.z;
		if (w.z > maxZ) maxZ = w.z;
	}

	for (RadialModificationMap::const_iterator it = m_radialModifications.begin(); it != m_radialModifications.end(); ++it)
	{
		RadialModification const& mod = it->second;
		Vector const w = terrainObject->rotateTranslate_o2w(Vector(mod.worldX, 0.f, mod.worldZ));
		hasBounds = true;
		if (w.x < minX) minX = w.x;
		if (w.x > maxX) maxX = w.x;
		if (w.z < minZ) minZ = w.z;
		if (w.z > maxZ) maxZ = w.z;
	}

	if (!hasBounds)
		return;

	const float margin = 32.0f;
	Rectangle2d const extentRaw(minX - margin, minZ - margin, maxX + margin, maxZ + margin);
	TerrainObject const* const toeConst = TerrainObject::getConstInstance();
	Rectangle2d const extent2d = gdSanitizeTerrainWorldFootprint(toeConst, extentRaw);

	godClientSyncLiveStagingAoiLayer(extent2d);

	terrainObject->invalidateRegion(extent2d);
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::godClientSyncLiveStagingAoiLayer(Rectangle2d const& worldExtentFootprintXZ)
{
	static char const kLayerName[] = "God Client Live Edit";
	TerrainGenerator* const gen = getTerrainGenerator();
	if (!gen)
		return;

	TerrainObject const* const toeConstSync = TerrainObject::getConstInstance();
	Rectangle2d const roi = gdSanitizeTerrainWorldFootprint(toeConstSync, worldExtentFootprintXZ);

	TerrainGenerator::Layer* found = 0;
	for (int i = 0; i < gen->getNumberOfLayers(); ++i)
	{
		TerrainGenerator::Layer* const L = gen->getLayer(i);
		if (!L)
			continue;
		char const* nm = L->getName();
		if (nm && std::strcmp(nm, kLayerName) == 0)
		{
			found = L;
			break;
		}
	}

	if (!found)
	{
		TerrainGenerator::Layer* const layer = new TerrainGenerator::Layer();
		layer->setName(kLayerName);
		layer->setActive(true);
		BoundaryRectangle* const boundary = new BoundaryRectangle();
		boundary->setName("Live edit AOI");
		boundary->setRectangle(roi);
		boundary->setFeatherDistance(0.f);
		layer->addBoundary(boundary);
		gen->addLayer(layer);
	}
	else if (found->getNumberOfBoundaries() > 0)
	{
		TerrainGenerator::Boundary* const b = found->getBoundary(0);
		if (b && b->getType() == TGBT_rectangle)
		{
			BoundaryRectangle* const br = static_cast<BoundaryRectangle*>(b);
			br->setRectangle(godClientUnionRects(br->getRectangle(), roi));
			br->setFeatherDistance(0.f);
		}
	}

	if (MainFrame* const mf = MainFrame::getInstanceNullable())
		if (TerrainDock* const dock = mf->getTerrainDock())
			dock->refreshTerrainLayerListFromGenerator();
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

	// Larger nudge keeps DPVS / streaming coherent after procedural invalidate storms.
	static float const nudgeXYZ = 0.12f;
	static float const nudgeYaw = 0.006f;

	FreeCamera::Info info(cam->getInfo());

	info.translate.x += nudgeXYZ;
	info.translate.y += nudgeXYZ * 0.25f;
	info.yaw += nudgeYaw;
	cam->setInfo(info);

	info.translate.x -= nudgeXYZ;
	info.translate.y -= nudgeXYZ * 0.25f;
	info.yaw -= nudgeYaw;
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

	if (isPolygonDrawActive())
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
		case TM_PlaceExcludeTerrain:
		case TM_PlaceBoundaryPolygon:
			brushColor = VectorArgb(1.0f, 0.2f, 1.0f, 0.6f);
			break;
		case TM_PlaceBoundaryPolyline:
		case TM_PlaceBoundaryPolyRoad:
			brushColor = VectorArgb(1.0f, 0.5f, 0.95f, 0.35f);
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

void GodClientTerrainEditor::setRegionSelection(
	float const minX,
	float const minZ,
	float const maxX,
	float const maxZ,
	bool const circularSelection,
	float const circleCenterX,
	float const circleCenterZ,
	float const circleRadius)
{
	m_hasRegionSelection = true;
	m_regionMinX = minX;
	m_regionMinZ = minZ;
	m_regionMaxX = maxX;
	m_regionMaxZ = maxZ;
	m_regionSelectionCircular = circularSelection;
	m_regionCircleCenterX = circleCenterX;
	m_regionCircleCenterZ = circleCenterZ;
	m_regionCircleRadius = circleRadius;
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::clearRegionSelection()
{
	m_hasRegionSelection = false;
	m_regionSelectionCircular = false;
	m_regionCircleRadius = 0.0f;
}

// ----------------------------------------------------------------------

bool GodClientTerrainEditor::applyRectangularHeightSamples(
	float const minX,
	float const minZ,
	float const maxX,
	float const maxZ,
	int const nx,
	int const nz,
	float const* heightsRowMajor,
	unsigned char const* cellMaskRowMajor)
{
	if (!heightsRowMajor || nx < 1 || nz < 1)
		return false;

	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (!terrainObject)
		return false;

	BrushStroke stroke;
	stroke.centerX = (minX + maxX) * 0.5f;
	stroke.centerZ = (minZ + maxZ) * 0.5f;
	stroke.radius = std::max(std::fabs(maxX - minX), std::fabs(maxZ - minZ)) * 0.5f + 1.0f;
	stroke.strength = 1.0f;
	stroke.tool = TM_SetHeight;
	stroke.targetHeight = 0.0f;

	int const baseX = static_cast<int>(std::floor(std::min(minX, maxX) + 1e-4f));
	int const baseZ = static_cast<int>(std::floor(std::min(minZ, maxZ) + 1e-4f));

	for (int iz = 0; iz < nz; ++iz)
	{
		for (int ix = 0; ix < nx; ++ix)
		{
			if (cellMaskRowMajor)
			{
				unsigned char const m = cellMaskRowMajor[iz * nx + ix];
				if (m == 0)
					continue;
			}

			int const gix = baseX + ix;
			int const giz = baseZ + iz;
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
	float const minPaintX = static_cast<float>(baseX);
	float const minPaintZ = static_cast<float>(baseZ);
	float const maxPaintX = static_cast<float>(baseX + nx - 1);
	float const maxPaintZ = static_cast<float>(baseZ + nz - 1);
	Rectangle2d const extent2d(minPaintX - margin, minPaintZ - margin, maxPaintX + margin, maxPaintZ + margin);
	terrainObject->invalidateRegion(extent2d);
	flushTerrainChanges();
	nudgeGodClientCameraToRefreshDpvs();
	return true;
}

// ----------------------------------------------------------------------

bool GodClientTerrainEditor::applyRectangularShaderPaint(
	float const minX,
	float const minZ,
	float const maxX,
	float const maxZ,
	int const shaderFamilyId,
	float const strength,
	bool const circularClip,
	float const circleCenterX,
	float const circleCenterZ,
	float const circleRadius)
{
	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (!terrainObject)
		return false;

	float const ax0 = std::min(minX, maxX);
	float const az0 = std::min(minZ, maxZ);
	float const ax1 = std::max(minX, maxX);
	float const az1 = std::max(minZ, maxZ);

	static float const kMinExtent = 0.5f;
	if ((ax1 - ax0) < kMinExtent || (az1 - az0) < kMinExtent)
		return false;

	int const familyIdClamped = std::max(0, std::min(255, shaderFamilyId));
	float const feather01 = std::max(0.0f, std::min(1.0f, strength));

	const int minIx = static_cast<int>(std::floor(ax0));
	const int maxIx = static_cast<int>(std::ceil(ax1));
	const int minIz = static_cast<int>(std::floor(az0));
	const int maxIz = static_cast<int>(std::ceil(az1));

	int cellsWritten = 0;

	float const rSq = circleRadius * circleRadius;
	bool const useCircle = circularClip && circleRadius > 0.25f;

	beginShaderUndoBatch();

	{
		ShaderMapLock shaderLock(*this);
		for (int iz = minIz; iz <= maxIz; ++iz)
		{
			for (int ix = minIx; ix <= maxIx; ++ix)
			{
				float const xw = static_cast<float>(ix);
				float const zw = static_cast<float>(iz);
				if (xw < ax0 || xw > ax1 || zw < az0 || zw > az1)
					continue;

				if (!isWorldPositionInActiveRegion(xw, zw))
					continue;

				if (useCircle)
				{
					float const dx = xw - circleCenterX;
					float const dz = zw - circleCenterZ;
					if (dx * dx + dz * dz > rSq + 1e-3f)
						continue;
				}

				Vector const objectPos = godClientTerrainObjectSampled_w2o(terrainObject, xw, zw);
				float const lx = objectPos.x;
				float const lz = objectPos.z;

				const uint64 key = godClientTerrainPaintCellKey(lx, lz);

				ShaderModification mod;
				mod.worldX = lx;
				mod.worldZ = lz;
				mod.modifiedFamilyId = familyIdClamped;
				mod.featherAmount = feather01;

				ShaderModificationMap::iterator it = m_shaderModifications.find(key);
				if (it != m_shaderModifications.end())
				{
					mod.originalFamilyId = it->second.originalFamilyId;
					if (familyIdClamped == it->second.modifiedFamilyId)
						mod.featherAmount = std::max(feather01, it->second.featherAmount);
				}
				else
				{
					mod.originalFamilyId = -1;
				}

				recordShaderStrokePending(key);
				m_shaderModifications[key] = mod;
				++cellsWritten;
			}
		}
	}

	endShaderUndoBatch();

	if (cellsWritten <= 0)
		return false;

	expandModifiedBounds((ax0 + ax1) * 0.5f, (az0 + az1) * 0.5f, std::max(ax1 - ax0, az1 - az0) * 0.5f + 32.0f);

	return true;
}

// ----------------------------------------------------------------------

bool GodClientTerrainEditor::applyRectangularVertexColorPaint(
	float const minX,
	float const minZ,
	float const maxX,
	float const maxZ,
	PackedRgb const& rgb,
	float const strength,
	bool const circularClip,
	float const circleCenterX,
	float const circleCenterZ,
	float const circleRadius)
{
	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (!terrainObject)
		return false;

	float const ax0 = std::min(minX, maxX);
	float const az0 = std::min(minZ, maxZ);
	float const ax1 = std::max(minX, maxX);
	float const az1 = std::max(minZ, maxZ);

	static float const kMinExtent = 0.5f;
	if ((ax1 - ax0) < kMinExtent || (az1 - az0) < kMinExtent)
		return false;

	float const feather01 = std::max(0.0f, std::min(1.0f, strength));

	int const minIx = static_cast<int>(std::floor(ax0));
	int const maxIx = static_cast<int>(std::ceil(ax1));
	int const minIz = static_cast<int>(std::floor(az0));
	int const maxIz = static_cast<int>(std::ceil(az1));

	int cellsWritten = 0;

	float const rSq = circleRadius * circleRadius;
	bool const useCircle = circularClip && circleRadius > 0.25f;

	beginShaderUndoBatch();

	{
		ShaderMapLock shaderLock(*this);
		for (int iz = minIz; iz <= maxIz; ++iz)
		{
			for (int ix = minIx; ix <= maxIx; ++ix)
			{
				float const xw = static_cast<float>(ix);
				float const zw = static_cast<float>(iz);
				if (xw < ax0 || xw > ax1 || zw < az0 || zw > az1)
					continue;

				if (!isWorldPositionInActiveRegion(xw, zw))
					continue;

				if (useCircle)
				{
					float const dx = xw - circleCenterX;
					float const dz = zw - circleCenterZ;
					if (dx * dx + dz * dz > rSq + 1e-3f)
						continue;
				}

				Vector const objectPos = godClientTerrainObjectSampled_w2o(terrainObject, xw, zw);
				float const lx = objectPos.x;
				float const lz = objectPos.z;

				uint64 const key = godClientTerrainPaintCellKey(lx, lz);

				VertexColorModification mod;
				mod.worldX = lx;
				mod.worldZ = lz;
				mod.color = rgb;
				mod.blendAmount = feather01;

				VertexColorModificationMap::iterator it = m_vertexColorModifications.find(key);
				if (it != m_vertexColorModifications.end())
				{
					if (it->second.color == rgb)
						mod.blendAmount = std::max(feather01, it->second.blendAmount);
				}

				recordVertexColorStrokePending(key);
				m_vertexColorModifications[key] = mod;
				++cellsWritten;
			}
		}
	}

	endShaderUndoBatch();

	if (cellsWritten <= 0)
		return false;

	expandModifiedBounds((ax0 + ax1) * 0.5f, (az0 + az1) * 0.5f, std::max(ax1 - ax0, az1 - az0) * 0.5f + 32.0f);

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
	float const maxZ,
	bool const circularSelection,
	float const circleCenterX,
	float const circleCenterZ,
	float const circleRadius) const
{
	TerrainObject const* const terrainObject = TerrainObject::getConstInstance();
	if (!terrainObject)
		return;

	VectorArgb const color(1.0f, 1.0f, 0.85f, 0.1f);
	if (circularSelection && circleRadius > 0.01f)
		addTerrainHeightDebugCircle(*terrainObject, camera, circleCenterX, circleCenterZ, circleRadius, color, 72);
	else
	{
		addTerrainHeightDebugLineStrip(*terrainObject, camera, minX, minZ, maxX, minZ, color);
		addTerrainHeightDebugLineStrip(*terrainObject, camera, maxX, minZ, maxX, maxZ, color);
		addTerrainHeightDebugLineStrip(*terrainObject, camera, maxX, maxZ, minX, maxZ, color);
		addTerrainHeightDebugLineStrip(*terrainObject, camera, minX, maxZ, minX, minZ, color);
	}
}

// ======================================================================

bool GodClientTerrainEditor::shouldRecordShaderUndo() const
{
	return m_brushStrokeActive || m_shaderUndoBatch > 0;
}

void GodClientTerrainEditor::recordShaderStrokePending(uint64 const key)
{
	if (!shouldRecordShaderUndo())
		return;
	if (!m_shaderStrokePendingKeys.insert(key).second)
		return;
	std::pair<bool, ShaderModification> priorSlot(false, ShaderModification());
	ShaderModificationMap::const_iterator const pit = m_shaderModifications.find(key);
	if (pit != m_shaderModifications.end())
	{
		priorSlot.first = true;
		priorSlot.second = pit->second;
	}
	m_shaderStrokePending.push_back(std::make_pair(key, priorSlot));
}

void GodClientTerrainEditor::sealShaderStrokeRecords(BrushStroke& stroke)
{
	ShaderMapLock shaderLock(*this);
	stroke.shaderStrokeRecords.clear();
	for (ShaderStrokePendingVector::const_iterator pit = m_shaderStrokePending.begin(); pit != m_shaderStrokePending.end(); ++pit)
	{
		uint64 const key = pit->first;
		bool const hadPrior = pit->second.first;
		ShaderModification const& prior = pit->second.second;

		ShaderStrokeRecord rec;
		rec.key = key;
		rec.hadPrior = hadPrior;
		rec.prior = prior;

		ShaderModificationMap::const_iterator const it = m_shaderModifications.find(key);
		if (it != m_shaderModifications.end())
			rec.after = it->second;
		else
		{
			int cellX = 0;
			int cellZ = 0;
			godClientTerrainPaintDecodeKey(key, cellX, cellZ);
			rec.after.worldX = static_cast<float>(cellX);
			rec.after.worldZ = static_cast<float>(cellZ);
			rec.after.originalFamilyId = -1;
			rec.after.modifiedFamilyId = 0;
			rec.after.featherAmount = 0.f;
		}
		stroke.shaderStrokeRecords.push_back(rec);
	}
	m_shaderStrokePending.clear();
	m_shaderStrokePendingKeys.clear();
}

void GodClientTerrainEditor::restoreShaderModificationsFromStrokeRecords(std::vector<ShaderStrokeRecord> const& recs, bool const usePriorState)
{
	ShaderMapLock shaderLock(*this);
	for (size_t i = 0; i < recs.size(); ++i)
	{
		ShaderStrokeRecord const& r = recs[i];
		if (usePriorState)
		{
			if (r.hadPrior)
				m_shaderModifications[r.key] = r.prior;
			else
				m_shaderModifications.erase(r.key);
		}
		else
		{
			if (r.after.featherAmount <= 0.f)
				m_shaderModifications.erase(r.key);
			else
				m_shaderModifications[r.key] = r.after;
		}
	}
}

void GodClientTerrainEditor::recordVertexColorStrokePending(uint64 const key)
{
	if (!shouldRecordShaderUndo())
		return;
	if (!m_vertexColorStrokePendingKeys.insert(key).second)
		return;
	std::pair<bool, VertexColorModification> priorSlot(false, VertexColorModification());
	VertexColorModificationMap::const_iterator const pit = m_vertexColorModifications.find(key);
	if (pit != m_vertexColorModifications.end())
	{
		priorSlot.first = true;
		priorSlot.second = pit->second;
	}
	m_vertexColorStrokePending.push_back(std::make_pair(key, priorSlot));
}

void GodClientTerrainEditor::sealVertexColorStrokeRecords(BrushStroke& stroke)
{
	ShaderMapLock shaderLock(*this);
	stroke.vertexColorStrokeRecords.clear();
	for (VertexColorStrokePendingVector::const_iterator pit = m_vertexColorStrokePending.begin(); pit != m_vertexColorStrokePending.end(); ++pit)
	{
		uint64 const key = pit->first;
		bool const hadPrior = pit->second.first;
		VertexColorModification const& prior = pit->second.second;

		VertexColorStrokeRecord rec;
		rec.key = key;
		rec.hadPrior = hadPrior;
		rec.prior = prior;

		VertexColorModificationMap::const_iterator const it = m_vertexColorModifications.find(key);
		if (it != m_vertexColorModifications.end())
			rec.after = it->second;
		else
		{
			int cellX = 0;
			int cellZ = 0;
			godClientTerrainPaintDecodeKey(key, cellX, cellZ);
			rec.after.worldX = static_cast<float>(cellX);
			rec.after.worldZ = static_cast<float>(cellZ);
			rec.after.color = PackedRgb::solidBlack;
			rec.after.blendAmount = 0.f;
		}
		stroke.vertexColorStrokeRecords.push_back(rec);
	}
	m_vertexColorStrokePending.clear();
	m_vertexColorStrokePendingKeys.clear();
}

void GodClientTerrainEditor::restoreVertexColorModificationsFromStrokeRecords(std::vector<VertexColorStrokeRecord> const& recs, bool const usePriorState)
{
	ShaderMapLock shaderLock(*this);
	for (size_t i = 0; i < recs.size(); ++i)
	{
		VertexColorStrokeRecord const& r = recs[i];
		if (usePriorState)
		{
			if (r.hadPrior)
				m_vertexColorModifications[r.key] = r.prior;
			else
				m_vertexColorModifications.erase(r.key);
		}
		else
		{
			if (r.after.blendAmount <= 0.f)
				m_vertexColorModifications.erase(r.key);
			else
				m_vertexColorModifications[r.key] = r.after;
		}
	}
}

void GodClientTerrainEditor::beginShaderUndoBatch()
{
	++m_shaderUndoBatch;
}

void GodClientTerrainEditor::endShaderUndoBatch()
{
	if (--m_shaderUndoBatch > 0)
		return;

	BrushStroke stroke;
	stroke.centerX = stroke.centerZ = 0.f;
	stroke.radius = m_brushSize * 0.5f;
	stroke.strength = m_brushStrength;
	stroke.tool = TM_PaintShader;
	stroke.targetHeight = 0.f;
	sealShaderStrokeRecords(stroke);
	sealVertexColorStrokeRecords(stroke);
	if (!stroke.shaderStrokeRecords.empty() || !stroke.vertexColorStrokeRecords.empty())
	{
		m_undoStack.push_back(stroke);
		while (static_cast<int>(m_undoStack.size()) > MAX_UNDO_STROKES)
			m_undoStack.erase(m_undoStack.begin());
		m_redoStack.clear();

		float minWx = 1e9f;
		float maxWx = -1e9f;
		float minWz = 1e9f;
		float maxWz = -1e9f;
		TerrainObject const* const terrainObjectConst = TerrainObject::getConstInstance();
		if (terrainObjectConst)
		{
			for (size_t i = 0; i < stroke.shaderStrokeRecords.size(); ++i)
			{
				ShaderStrokeRecord const& sr = stroke.shaderStrokeRecords[i];
				Vector const w = terrainObjectConst->rotateTranslate_o2w(Vector(sr.after.worldX, 0.f, sr.after.worldZ));
				if (w.x < minWx)
					minWx = w.x;
				if (w.x > maxWx)
					maxWx = w.x;
				if (w.z < minWz)
					minWz = w.z;
				if (w.z > maxWz)
					maxWz = w.z;
			}
			for (size_t i = 0; i < stroke.vertexColorStrokeRecords.size(); ++i)
			{
				VertexColorStrokeRecord const& vr = stroke.vertexColorStrokeRecords[i];
				Vector const w = terrainObjectConst->rotateTranslate_o2w(Vector(vr.after.worldX, 0.f, vr.after.worldZ));
				if (w.x < minWx)
					minWx = w.x;
				if (w.x > maxWx)
					maxWx = w.x;
				if (w.z < minWz)
					minWz = w.z;
				if (w.z > maxWz)
					maxWz = w.z;
			}
		}

		float const pad = m_brushSize + 48.f;
		BrushStroke& pushed = m_undoStack.back();
		if (minWx <= maxWx && minWz <= maxWz)
		{
			pushed.centerX = 0.5f * (minWx + maxWx);
			pushed.centerZ = 0.5f * (minWz + maxWz);
			pushed.radius = 0.5f * std::max(maxWx - minWx, maxWz - minWz) + pad;
		}

		TerrainObject* const terrainObject = TerrainObject::getInstance();
		if (terrainObject && minWx <= maxWx && minWz <= maxWz)
			terrainObject->invalidateRegion(Rectangle2d(minWx - pad, minWz - pad, maxWx + pad, maxWz + pad));
		flushTerrainChanges();
		nudgeGodClientCameraToRefreshDpvs();
	}
}

bool GodClientTerrainEditor::isWorldPositionInActiveRegion(float const worldX, float const worldZ) const
{
	if (!m_hasRegionSelection)
		return true;

	float const rx0 = std::min(m_regionMinX, m_regionMaxX);
	float const rx1 = std::max(m_regionMinX, m_regionMaxX);
	float const rz0 = std::min(m_regionMinZ, m_regionMaxZ);
	float const rz1 = std::max(m_regionMinZ, m_regionMaxZ);

	if (worldX < rx0 || worldX > rx1 || worldZ < rz0 || worldZ > rz1)
		return false;

	if (m_regionSelectionCircular && m_regionCircleRadius > 0.01f)
	{
		float const dx = worldX - m_regionCircleCenterX;
		float const dz = worldZ - m_regionCircleCenterZ;
		if (dx * dx + dz * dz > m_regionCircleRadius * m_regionCircleRadius + 1e-3f)
			return false;
	}
	return true;
}

Rectangle2d GodClientTerrainEditor::clipBoundaryRectangleToActiveRegion(Rectangle2d const& worldRect) const
{
	if (!m_hasRegionSelection)
		return worldRect;

	float const wx0 = std::min(worldRect.x0, worldRect.x1);
	float const wx1 = std::max(worldRect.x0, worldRect.x1);
	float const wz0 = std::min(worldRect.y0, worldRect.y1);
	float const wz1 = std::max(worldRect.y0, worldRect.y1);

	float ix0 = wx0;
	float iz0 = wz0;
	float ix1 = wx1;
	float iz1 = wz1;

	float const rx0 = std::min(m_regionMinX, m_regionMaxX);
	float const rx1 = std::max(m_regionMinX, m_regionMaxX);
	float const rz0 = std::min(m_regionMinZ, m_regionMaxZ);
	float const rz1 = std::max(m_regionMinZ, m_regionMaxZ);

	ix0 = std::max(ix0, rx0);
	iz0 = std::max(iz0, rz0);
	ix1 = std::min(ix1, rx1);
	iz1 = std::min(iz1, rz1);

	if (m_regionSelectionCircular && m_regionCircleRadius > 0.01f)
	{
		float const cx0 = m_regionCircleCenterX - m_regionCircleRadius;
		float const cz0 = m_regionCircleCenterZ - m_regionCircleRadius;
		float const cx1 = m_regionCircleCenterX + m_regionCircleRadius;
		float const cz1 = m_regionCircleCenterZ + m_regionCircleRadius;
		ix0 = std::max(ix0, cx0);
		iz0 = std::max(iz0, cz0);
		ix1 = std::min(ix1, cx1);
		iz1 = std::min(iz1, cz1);
	}

	if (ix0 >= ix1 || iz0 >= iz1)
		return Rectangle2d(0.f, 0.f, 0.f, 0.f);

	return Rectangle2d(ix0, iz0, ix1, iz1);
}

bool GodClientTerrainEditor::applyRegionBrushFillHeightTools()
{
	if (!m_hasRegionSelection)
		return false;

	switch (m_toolMode)
	{
	case TM_Raise:
	case TM_Lower:
	case TM_Flatten:
	case TM_Smooth:
	case TM_Noise:
	case TM_SetHeight:
		break;
	default:
		return false;
	}

	float const rz0 = std::min(m_regionMinZ, m_regionMaxZ);
	float const rz1 = std::max(m_regionMinZ, m_regionMaxZ);
	float const rx0 = std::min(m_regionMinX, m_regionMaxX);
	float const rx1 = std::max(m_regionMinX, m_regionMaxX);

	float const step = std::max(m_brushSize * 0.12f, 2.f);

	float firstX = 0.f;
	float firstZ = 0.f;
	bool foundStart = false;

	for (float z = rz0; z <= rz1 + 1e-3f && !foundStart; z += step)
	{
		for (float x = rx0; x <= rx1 + 1e-3f && !foundStart; x += step)
		{
			if (!isWorldPositionInActiveRegion(x, z))
				continue;
			firstX = x;
			firstZ = z;
			foundStart = true;
		}
	}

	if (!foundStart)
		return false;

	if (!beginBrushStroke(firstX, firstZ))
		return false;

	for (float z = rz0; z <= rz1 + 1e-3f; z += step)
	{
		for (float x = rx0; x <= rx1 + 1e-3f; x += step)
		{
			if (!isWorldPositionInActiveRegion(x, z))
				continue;
			continueBrushStroke(x, z);
		}
	}

	endBrushStroke();
	return true;
}

// ======================================================================
// Shader Painting
// ======================================================================

void GodClientTerrainEditor::modifyShaderPaint(float worldX, float worldZ, int shaderFamilyId, float strength)
{
	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (!terrainObject)
		return;

	int const familyIdClamped = std::max(0, std::min(255, shaderFamilyId));

	TerrainGenerator* const generator = getTerrainGenerator();
	if (generator && !generator->getShaderGroup().hasFamily(familyIdClamped))
		return;

	ShaderMapLock shaderLock(*this);

	const float halfBrush = m_brushSize * 0.5f;

	const int minX = static_cast<int>(std::floor(worldX - halfBrush));
	const int maxX = static_cast<int>(std::ceil(worldX + halfBrush));
	const int minZ = static_cast<int>(std::floor(worldZ - halfBrush));
	const int maxZ = static_cast<int>(std::ceil(worldZ + halfBrush));

	for (int iz = minZ; iz <= maxZ; ++iz)
	{
		for (int ix = minX; ix <= maxX; ++ix)
		{
			const float xw = static_cast<float>(ix);
			const float zw = static_cast<float>(iz);
			const float brushLocalX = xw - worldX;
			const float brushLocalZ = zw - worldZ;
			const float effect = calculateBrushEffect(brushLocalX, brushLocalZ);

			if (effect > 0.0f)
			{
				if (!isWorldPositionInActiveRegion(xw, zw))
					continue;

				Vector const objectPos = godClientTerrainObjectSampled_w2o(terrainObject, xw, zw);
				float const lx = objectPos.x;
				float const lz = objectPos.z;

				const uint64 key = godClientTerrainPaintCellKey(lx, lz);

				float const feather01 = std::max(0.0f, std::min(1.0f, effect * strength));

				ShaderModification mod;
				mod.worldX = lx;
				mod.worldZ = lz;
				mod.modifiedFamilyId = familyIdClamped;

				ShaderModificationMap::iterator it = m_shaderModifications.find(key);
				if (it != m_shaderModifications.end())
				{
					mod.originalFamilyId = it->second.originalFamilyId;
					if (familyIdClamped == it->second.modifiedFamilyId)
						mod.featherAmount = std::max(feather01, it->second.featherAmount);
					else
						mod.featherAmount = feather01;
				}
				else
				{
					mod.originalFamilyId = -1;
					mod.featherAmount = feather01;
				}

				recordShaderStrokePending(key);
				m_shaderModifications[key] = mod;
			}
		}
	}
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::modifyVertexColorPaint(float worldX, float worldZ, PackedRgb const& color, float strength)
{
	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (!terrainObject)
		return;

	ShaderMapLock shaderLock(*this);

	float const halfBrush = m_brushSize * 0.5f;

	int const minX = static_cast<int>(std::floor(worldX - halfBrush));
	int const maxX = static_cast<int>(std::ceil(worldX + halfBrush));
	int const minZ = static_cast<int>(std::floor(worldZ - halfBrush));
	int const maxZ = static_cast<int>(std::ceil(worldZ + halfBrush));

	for (int iz = minZ; iz <= maxZ; ++iz)
	{
		for (int ix = minX; ix <= maxX; ++ix)
		{
			float const xw = static_cast<float>(ix);
			float const zw = static_cast<float>(iz);
			float const brushLocalX = xw - worldX;
			float const brushLocalZ = zw - worldZ;
			float const effect = calculateBrushEffect(brushLocalX, brushLocalZ);

			if (effect > 0.0f)
			{
				if (!isWorldPositionInActiveRegion(xw, zw))
					continue;

				Vector const objectPos = godClientTerrainObjectSampled_w2o(terrainObject, xw, zw);
				float const lx = objectPos.x;
				float const lz = objectPos.z;

				uint64 const key = godClientTerrainPaintCellKey(lx, lz);

				float const feather01 = std::max(0.0f, std::min(1.0f, effect * strength));

				VertexColorModification mod;
				mod.worldX = lx;
				mod.worldZ = lz;
				mod.color = color;
				mod.blendAmount = feather01;

				VertexColorModificationMap::iterator it = m_vertexColorModifications.find(key);
				if (it != m_vertexColorModifications.end())
				{
					if (it->second.color == color)
						mod.blendAmount = std::max(feather01, it->second.blendAmount);
				}

				recordVertexColorStrokePending(key);
				m_vertexColorModifications[key] = mod;
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
	TerrainObject const* const terrainObject = TerrainObject::getConstInstance();
	if (!terrainObject)
	{
		outFamilyId = originalFamilyId;
		outFeather = 0.0f;
		return false;
	}

	static int const s_neighborCells = 1;

	ShaderMapLock shaderLock(const_cast<GodClientTerrainEditor&>(*this));

	if (m_shaderModifications.empty())
	{
		outFamilyId = originalFamilyId;
		outFeather = 0.0f;
		return false;
	}

	uint64 const baseKey = godClientTerrainPaintKeyFromWorld(terrainObject, x, z);

	int bx = 0;
	int bz = 0;
	godClientTerrainPaintDecodeKey(baseKey, bx, bz);

	Vector const oq = godClientTerrainObjectSampled_w2o(terrainObject, x, z);

	ShaderModificationMap::const_iterator best = m_shaderModifications.end();
	float bestDist2 = 0.f;
	float bestFeather = 0.f;

	for (int dz = -s_neighborCells; dz <= s_neighborCells; ++dz)
	{
		for (int dx = -s_neighborCells; dx <= s_neighborCells; ++dx)
		{
			uint64 const nk = godClientTerrainPaintCellKey(static_cast<float>(bx + dx), static_cast<float>(bz + dz));
			ShaderModificationMap::const_iterator const it = m_shaderModifications.find(nk);
			if (it == m_shaderModifications.end() || it->second.featherAmount <= 0.f)
				continue;

			float const ddx = oq.x - it->second.worldX;
			float const ddz = oq.z - it->second.worldZ;
			float const dist2 = ddx * ddx + ddz * ddz;

			if (best == m_shaderModifications.end()
				|| dist2 < bestDist2 - 1e-6f
				|| (std::fabs(dist2 - bestDist2) <= 1e-6f && it->second.featherAmount > bestFeather))
			{
				best = it;
				bestDist2 = dist2;
				bestFeather = it->second.featherAmount;
			}
		}
	}

	if (best == m_shaderModifications.end())
	{
		outFamilyId = originalFamilyId;
		outFeather = 0.0f;
		return false;
	}

	int const modifiedId = best->second.modifiedFamilyId;
	TerrainGenerator* const generator = getTerrainGenerator();
	if (generator && !generator->getShaderGroup().hasFamily(modifiedId))
	{
		outFamilyId = originalFamilyId;
		outFeather = 0.0f;
		return false;
	}

	outFamilyId = modifiedId;
	outFeather = best->second.featherAmount;
	return true;
}

// ----------------------------------------------------------------------

bool GodClientTerrainEditor::getModifiedVertexColor(float x, float z, PackedRgb const& original, PackedRgb& out)
{
	if (!ms_instance)
	{
		out = original;
		return false;
	}
	return ms_instance->getModifiedVertexColorInternal(x, z, original, out);
}

// ----------------------------------------------------------------------

bool GodClientTerrainEditor::getModifiedVertexColorInternal(float x, float z, PackedRgb const& original, PackedRgb& out) const
{
	TerrainObject const* const terrainObject = TerrainObject::getConstInstance();
	if (!terrainObject)
	{
		out = original;
		return false;
	}

	static int const s_neighborCells = 1;

	ShaderMapLock shaderLock(const_cast<GodClientTerrainEditor&>(*this));

	if (m_vertexColorModifications.empty())
	{
		out = original;
		return false;
	}

	uint64 const baseKey = godClientTerrainPaintKeyFromWorld(terrainObject, x, z);

	int bx = 0;
	int bz = 0;
	godClientTerrainPaintDecodeKey(baseKey, bx, bz);

	Vector const oq = godClientTerrainObjectSampled_w2o(terrainObject, x, z);

	VertexColorModificationMap::const_iterator best = m_vertexColorModifications.end();
	float bestDist2 = 0.f;
	float bestBlend = 0.f;

	for (int dz = -s_neighborCells; dz <= s_neighborCells; ++dz)
	{
		for (int dx = -s_neighborCells; dx <= s_neighborCells; ++dx)
		{
			uint64 const nk = godClientTerrainPaintCellKey(static_cast<float>(bx + dx), static_cast<float>(bz + dz));
			VertexColorModificationMap::const_iterator const it = m_vertexColorModifications.find(nk);
			if (it == m_vertexColorModifications.end() || it->second.blendAmount <= 0.f)
				continue;

			float const ddx = oq.x - it->second.worldX;
			float const ddz = oq.z - it->second.worldZ;
			float const dist2 = ddx * ddx + ddz * ddz;

			if (best == m_vertexColorModifications.end()
				|| dist2 < bestDist2 - 1e-6f
				|| (std::fabs(dist2 - bestDist2) <= 1e-6f && it->second.blendAmount > bestBlend))
			{
				best = it;
				bestDist2 = dist2;
				bestBlend = it->second.blendAmount;
			}
		}
	}

	if (best == m_vertexColorModifications.end())
	{
		out = original;
		return false;
	}

	float const t = std::max(0.f, std::min(1.f, best->second.blendAmount));
	out = PackedRgb::linearInterpolate(original, best->second.color, t);
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

	int const familyIdClamped = std::max (0, std::min (255, floraFamilyId));

	TerrainGenerator* const generator = getTerrainGenerator();
	if (generator && !generator->getFloraGroup ().hasFamily (familyIdClamped))
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
			const float xw = static_cast<float>(ix);
			const float zw = static_cast<float>(iz);
			const float localX = xw - worldX;
			const float localZ = zw - worldZ;
			const float effect = calculateBrushEffect(localX, localZ);

			if (effect > 0.0f)
			{
				if (!isWorldPositionInActiveRegion(xw, zw))
					continue;

				Vector const objectPos = godClientTerrainObjectSampled_w2o(terrainObject, xw, zw);
				float const lx = objectPos.x;
				float const lz = objectPos.z;
				const uint64 key = godClientTerrainPaintCellKey(lx, lz);

				FloraModification mod;
				mod.worldX = lx;
				mod.worldZ = lz;
				mod.modifiedFamilyId = familyIdClamped;
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
			const float xw = static_cast<float>(ix);
			const float zw = static_cast<float>(iz);
			const float localX = xw - worldX;
			const float localZ = zw - worldZ;
			const float effect = calculateBrushEffect(localX, localZ);

			if (effect > 0.0f && effect * strength > 0.5f)
			{
				if (!isWorldPositionInActiveRegion(xw, zw))
					continue;

				Vector const objectPos = godClientTerrainObjectSampled_w2o(terrainObject, xw, zw);
				const uint64 key = godClientTerrainPaintCellKey(objectPos.x, objectPos.z);

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
	TerrainObject const* const terrainObject = TerrainObject::getConstInstance();
	if (!terrainObject || m_floraModifications.empty())
	{
		outFamilyId = originalFamilyId;
		outDensity = 0.0f;
		return false;
	}

	uint64 const key = godClientTerrainPaintKeyFromWorld(terrainObject, x, z);
	FloraModificationMap::const_iterator const it = m_floraModifications.find(key);
	if (it == m_floraModifications.end() || it->second.density <= 0.f)
	{
		outFamilyId = originalFamilyId;
		outDensity = 0.0f;
		return false;
	}

	outFamilyId = it->second.modifiedFamilyId;
	outDensity = it->second.density;
	return true;
}

// ======================================================================
// Dynamic radial flora (RadialGroup) paint
// ======================================================================

void GodClientTerrainEditor::modifyRadialPaint(float worldX, float worldZ, int radialFamilyId, float childChoiceStrength)
{
	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (!terrainObject)
		return;

	if (!radialFamilyId)
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
			const float xw = static_cast<float>(ix);
			const float zw = static_cast<float>(iz);
			const float localX = xw - worldX;
			const float localZ = zw - worldZ;
			const float effect = calculateBrushEffect(localX, localZ);

			if (effect > 0.0f)
			{
				if (!isWorldPositionInActiveRegion(xw, zw))
					continue;

				Vector const objectPos = godClientTerrainObjectSampled_w2o(terrainObject, xw, zw);
				float const lx = objectPos.x;
				float const lz = objectPos.z;
				const uint64 key = godClientTerrainPaintCellKey(lx, lz);

				RadialModification mod;
				mod.worldX = lx;
				mod.worldZ = lz;
				mod.modifiedFamilyId = radialFamilyId;
				mod.childChoice = childChoiceStrength * effect * m_brushStrength;

				RadialModificationMap::iterator it = m_radialModifications.find(key);
				if (it != m_radialModifications.end())
				{
					mod.originalFamilyId = it->second.originalFamilyId;
					if (mod.childChoice > it->second.childChoice)
					{
						m_radialModifications[key] = mod;
					}
				}
				else
				{
					mod.originalFamilyId = -1;
					m_radialModifications[key] = mod;
				}
			}
		}
	}
}

// ----------------------------------------------------------------------

bool GodClientTerrainEditor::getModifiedRadial(float x, float z, int originalFamilyId, int& outFamilyId, float& outChildChoice)
{
	if (!ms_instance)
	{
		outFamilyId = originalFamilyId;
		outChildChoice = 0.0f;
		return false;
	}
	return ms_instance->getModifiedRadialInternal(x, z, originalFamilyId, outFamilyId, outChildChoice);
}

// ----------------------------------------------------------------------

bool GodClientTerrainEditor::getModifiedRadialInternal(float x, float z, int originalFamilyId, int& outFamilyId, float& outChildChoice) const
{
	TerrainObject const* const terrainObject = TerrainObject::getConstInstance();
	if (!terrainObject || m_radialModifications.empty())
	{
		outFamilyId = originalFamilyId;
		outChildChoice = 0.0f;
		return false;
	}

	uint64 const key = godClientTerrainPaintKeyFromWorld(terrainObject, x, z);
	RadialModificationMap::const_iterator const it = m_radialModifications.find(key);
	if (it == m_radialModifications.end() || it->second.childChoice <= 0.f)
	{
		outFamilyId = originalFamilyId;
		outChildChoice = 0.0f;
		return false;
	}

	outFamilyId = it->second.modifiedFamilyId;
	outChildChoice = it->second.childChoice;
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

void GodClientTerrainEditor::beginPolyline(bool isRibbon, PolylineCommitKind commitKind)
{
	m_activePolyline.controlPoints.clear();
	m_activePolyline.isRibbon = isRibbon;
	m_activePolyline.commitKind = commitKind;
	if (commitKind == PCK_BoundaryPolyline)
	{
		m_activePolyline.name = "Boundary Polyline";
		m_activePolyline.width = std::max(2.f, m_activePolyline.width);
	}
	else if (commitKind == PCK_BoundaryPolyRoad)
	{
		m_activePolyline.name = "Boundary Poly Road";
		m_activePolyline.width = std::max(8.f, m_activePolyline.width);
	}
	else
		m_activePolyline.name = isRibbon ? "New Ribbon" : "New Road";
	m_polylineEditMode = PEM_AddPoints;
	m_selectedPolylinePoint = -1;

	if (commitKind == PCK_BoundaryPolyline || commitKind == PCK_BoundaryPolyRoad)
	{
		MainFrame::getInstance().textToConsole("Started boundary polyline — click to add vertices (min width enforced on commit).");
	}
	else
	{
		MainFrame::getInstance().textToConsole(isRibbon ?
			"Started new ribbon - click to add control points" :
			"Started new road - click to add control points");
	}
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
		MainFrame::getInstance().textToConsole("Need at least 2 control points for this polyline tool.");
		return;
	}

	bool success = false;
	switch (m_activePolyline.commitKind)
	{
	case PCK_BoundaryPolyline:
		{
			char layerBuf[128];
			snprintf(layerBuf, sizeof(layerBuf), "GodBdryLn_%06d", s_godPolygonLayerSerial++);
			success = createBoundaryPolylineLayer(layerBuf, std::max(2.f, m_activePolyline.width));
		}
		break;
	case PCK_BoundaryPolyRoad:
		{
			char layerBuf[128];
			snprintf(layerBuf, sizeof(layerBuf), "GodBdryRd_%06d", s_godPolygonLayerSerial++);
			success = createBoundaryPolylineLayer(layerBuf, std::max(2.f, m_activePolyline.width));
		}
		break;
	case PCK_RoadRibbon:
	default:
		if (m_activePolyline.isRibbon)
			success = createRibbonFromPolyline(m_activePolyline.name.c_str());
		else
			success = createRoadFromPolyline(m_activePolyline.name.c_str());
		break;
	}

	if (success)
	{
		char buffer[256];
		switch (m_activePolyline.commitKind)
		{
		case PCK_BoundaryPolyline:
		case PCK_BoundaryPolyRoad:
			snprintf(buffer, sizeof(buffer), "Boundary polyline layer created (%d vertices, corridor %.1f).",
				static_cast<int>(m_activePolyline.controlPoints.size()), m_activePolyline.width);
			break;
		default:
			snprintf(buffer, sizeof(buffer), "%s '%s' created with %d control points",
				m_activePolyline.isRibbon ? "Ribbon" : "Road",
				m_activePolyline.name.c_str(),
				static_cast<int>(m_activePolyline.controlPoints.size()));
			break;
		}
		MainFrame::getInstance().textToConsole(buffer);

		m_activePolyline.controlPoints.clear();
		m_polylineEditMode = PEM_None;
		m_selectedPolylinePoint = -1;
	}
	else
	{
		MainFrame::getInstance().textToConsole("Failed to commit polyline (no terrain generator, clipped empty, or invalid data).");
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
	float capPad = 0.f;
	if (m_activePolyline.isRibbon)
		capPad = std::max(64.f, m_activePolyline.width * 4.f);
	m_polylineExtent = Rectangle2d(minX - margin - capPad, minZ - margin - capPad, maxX + margin + capPad, maxZ + margin + capPad);
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
// Environment / exclude / boundary polygon placement (shared point list)
// ======================================================================

void GodClientTerrainEditor::beginPolygonDraw(PolygonDrawPurpose const purpose)
{
	m_polygonDrawPurpose = purpose;
	m_activeEnvironmentZone.boundaryPoints.clear();

	switch (purpose)
	{
	case PDP_EnvironmentZone:
		m_activeEnvironmentZone.featherDistance = 8.f;
		MainFrame::getInstance().textToConsole("Started new environment zone — click to add boundary points (Finish in dock).");
		break;
	case PDP_ExcludeTerrain:
		MainFrame::getInstance().textToConsole("Exclude terrain — click vertices of a closed polygon (min 3). Interior tiles will not generate procedural mesh.");
		break;
	case PDP_BoundaryPolygon:
		MainFrame::getInstance().textToConsole("Boundary polygon — click vertices (min 3). Creates a BoundaryPolygon layer for masking child affectors.");
		break;
	default:
		break;
	}
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::finalizePolygonDraw()
{
	if (m_polygonDrawPurpose == PDP_None)
		return;

	if (m_activeEnvironmentZone.boundaryPoints.size() < 3)
	{
		MainFrame::getInstance().textToConsole("Need at least 3 points for this polygon tool.");
		return;
	}

	char layerBuf[128];
	snprintf(layerBuf, sizeof(layerBuf), "GodPoly_%06d", s_godPolygonLayerSerial++);
	std::string const baseName = m_activeEnvironmentZone.name.empty() ? std::string(layerBuf) : m_activeEnvironmentZone.name;

	bool success = false;
	switch (m_polygonDrawPurpose)
	{
	case PDP_EnvironmentZone:
		success = createEnvironmentZoneAffector(baseName.c_str());
		break;
	case PDP_ExcludeTerrain:
		success = createTerrainExcludeFromPolygon(baseName.c_str());
		break;
	case PDP_BoundaryPolygon:
		success = createBoundaryPolygonLayer(baseName.c_str());
		break;
	default:
		break;
	}

	if (success)
	{
		char buffer[256];
		snprintf(buffer, sizeof(buffer), "Polygon tool committed (%d points).", static_cast<int>(m_activeEnvironmentZone.boundaryPoints.size()));
		MainFrame::getInstance().textToConsole(buffer);
		m_activeEnvironmentZone.boundaryPoints.clear();
		m_polygonDrawPurpose = PDP_None;
	}
	else
	{
		MainFrame::getInstance().textToConsole("Polygon commit failed (no terrain generator or invalid layer).");
	}
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::cancelPolygonDraw()
{
	m_activeEnvironmentZone.boundaryPoints.clear();
	m_polygonDrawPurpose = PDP_None;
	MainFrame::getInstance().textToConsole("Polygon drawing cancelled.");
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::beginEnvironmentZone()
{
	beginPolygonDraw(PDP_EnvironmentZone);
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::addEnvironmentZonePoint(float worldX, float worldZ)
{
	m_activeEnvironmentZone.boundaryPoints.push_back(Vector2d(worldX, worldZ));

	char buffer[128];
	snprintf(buffer, sizeof(buffer), "Added polygon point at (%.1f, %.1f)", worldX, worldZ);
	MainFrame::getInstance().textToConsole(buffer);
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::finalizeEnvironmentZone()
{
	finalizePolygonDraw();
}

// ----------------------------------------------------------------------

void GodClientTerrainEditor::cancelEnvironmentZone()
{
	cancelPolygonDraw();
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

	const bool lockShaderMaps = m_bitmapStamp.affectsShader;
	if (lockShaderMaps)
		m_shaderModificationMutex.enter();

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
				if (!isWorldPositionInActiveRegion(x, z))
					continue;

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

				Vector const objectPosShader = godClientTerrainObjectSampled_w2o(terrainObject, x, z);
				uint64 const shaderKey = godClientTerrainPaintCellKey(objectPosShader.x, objectPosShader.z);

				ShaderModification mod;
				mod.worldX = objectPosShader.x;
				mod.worldZ = objectPosShader.z;
				mod.modifiedFamilyId = shaderFamilyToApply;
				mod.featherAmount = 1.0f;
				mod.originalFamilyId = -1;

				m_shaderModifications[shaderKey] = mod;
			}
		}
	}

	if (lockShaderMaps)
		m_shaderModificationMutex.leave();

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
	boundary->setFeatherDistance(0.0f);
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
	m_shaderModificationMutex.enter();
	bool const emptyMods = m_shaderModifications.empty();
	m_shaderModificationMutex.leave();
	if (emptyMods)
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
	boundary->setFeatherDistance(0.0f);
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
	boundary->setFeatherDistance(0.0f);
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
	// Feather at 1.0 makes inner shader width (1 - feather) * width/2 == 0, so no shader is ever written.
	float const featherClamped = std::max(0.f, std::min(0.98f, m_activePolyline.featherDistance));
	road->setFeatherDistance(featherClamped);
	road->setFeatherDistanceShader(featherClamped);
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

	// HeightData segments are required for AffectorBoundaryPoly::find(); God-authored roads skipped this and roads/ribbons failed to affect.
	road->clearHeightData();
	for (size_t i = 0; i + 1 < m_activePolyline.controlPoints.size(); ++i)
	{
		road->addSegmentHeightData();
		ControlPoint const& a = m_activePolyline.controlPoints[i];
		ControlPoint const& b = m_activePolyline.controlPoints[i + 1];
		road->addPointHeightData(Vector(static_cast<float>(a.position.x), a.height, static_cast<float>(a.position.y)));
		road->addPointHeightData(Vector(static_cast<float>(b.position.x), b.height, static_cast<float>(b.position.y)));
	}

	road->createHeightData();

	// Clip the layer to a padded axis box so generator/layer extent matches the feature (matches authored .trn layers).
	Rectangle2d const e = road->getExtent();
	float const pad = std::max(256.f, m_activePolyline.width * 4.f + 128.f);
	Rectangle2d const paddedRoad(e.x0 - pad, e.y0 - pad, e.x1 + pad, e.y1 + pad);
	Rectangle2d clippedRoad = clipBoundaryRectangleToActiveRegion(paddedRoad);
	if (m_hasRegionSelection && (clippedRoad.x1 <= clippedRoad.x0 || clippedRoad.y1 <= clippedRoad.y0))
	{
		MainFrame::getInstance().textToConsole("Road layer does not intersect the active terrain region.");
		return false;
	}
	BoundaryRectangle* const roadClip = new BoundaryRectangle();
	roadClip->setName("Road layer bounds");
	roadClip->setRectangle(clippedRoad);
	roadClip->setFeatherDistance(0.f);
	layer->addBoundary(roadClip);
	layer->addAffector(road);

	generator->addLayer(layer);
	m_createdLayers.push_back(name);
	generator->prepare();

	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (terrainObject)
	{
		terrainObject->invalidateRegion(m_polylineExtent);
	}

	nudgeGodClientCameraToRefreshDpvs();

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
	ShaderGroup const& shaderGroup = generator->getShaderGroup();
	int familyId = m_activePolyline.shaderFamilyId;
	if (!shaderGroup.hasFamily(familyId) && shaderGroup.getNumberOfFamilies() > 0)
	{
		familyId = shaderGroup.getFamilyId(0);
		MainFrame::getInstance().textToConsole("Ribbon: shader family not in scene; using first terrain shader family.");
	}
	ribbon->setTerrainShaderFamilyId(familyId);
	float const featherClamped = std::max(0.f, std::min(0.98f, m_activePolyline.featherDistance));
	ribbon->setFeatherDistance(featherClamped);
	ribbon->setFeatherDistanceTerrainShader(featherClamped);

	std::string waterTpl = m_ribbonWaterShaderTemplate;
	if (waterTpl.empty())
		waterTpl = m_waterPlacementShaderTemplate;
	if (waterTpl.empty())
		waterTpl = "wter_ocean_water";
	ribbon->setRibbonWaterShaderTemplateName(waterTpl.c_str());

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

	ribbon->generateEndCapPointList();
	ribbon->updateExtentAfterEndCaps();

	Rectangle2d const re = ribbon->getExtent();
	float const rpad = std::max(256.f, m_activePolyline.width * 4.f + 128.f);
	Rectangle2d const paddedRibbon(re.x0 - rpad, re.y0 - rpad, re.x1 + rpad, re.y1 + rpad);
	Rectangle2d clippedRibbon = clipBoundaryRectangleToActiveRegion(paddedRibbon);
	if (m_hasRegionSelection && (clippedRibbon.x1 <= clippedRibbon.x0 || clippedRibbon.y1 <= clippedRibbon.y0))
	{
		MainFrame::getInstance().textToConsole("Ribbon layer does not intersect the active terrain region.");
		return false;
	}
	BoundaryRectangle* const ribbonClip = new BoundaryRectangle();
	ribbonClip->setName("Ribbon layer bounds");
	ribbonClip->setRectangle(clippedRibbon);
	ribbonClip->setFeatherDistance(0.f);
	layer->addBoundary(ribbonClip);
	layer->addAffector(ribbon);

	generator->addLayer(layer);
	m_createdLayers.push_back(name);
	generator->prepare();

	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (terrainObject)
	{
		terrainObject->invalidateRegion(m_polylineExtent);
	}

	nudgeGodClientCameraToRefreshDpvs();

	return true;
}

// ----------------------------------------------------------------------

bool GodClientTerrainEditor::createTerrainExcludeFromPolygon(const char* name)
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
	boundary->setName("Terrain exclude boundary");
	boundary->clearPointList();
	for (size_t i = 0; i < m_activeEnvironmentZone.boundaryPoints.size(); ++i)
		boundary->addPoint(m_activeEnvironmentZone.boundaryPoints[i]);
	boundary->setFeatherDistance(0.f);
	layer->addBoundary(boundary);

	AffectorExclude* const exclude = new AffectorExclude();
	exclude->setName("Terrain exclude");
	layer->addAffector(exclude);

	generator->addLayer(layer);
	m_createdLayers.push_back(name);
	generator->prepare();

	Rectangle2d const inv = godClientBoundsFromPoints(m_activeEnvironmentZone.boundaryPoints, 384.f);
	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (terrainObject)
		terrainObject->invalidateRegion(inv);
	nudgeGodClientCameraToRefreshDpvs();
	return true;
}

// ----------------------------------------------------------------------

bool GodClientTerrainEditor::createBoundaryPolygonLayer(const char* name)
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
	boundary->setName("Boundary polygon");
	boundary->clearPointList();
	for (size_t i = 0; i < m_activeEnvironmentZone.boundaryPoints.size(); ++i)
		boundary->addPoint(m_activeEnvironmentZone.boundaryPoints[i]);
	boundary->setFeatherDistance(m_activeEnvironmentZone.featherDistance);
	layer->addBoundary(boundary);

	generator->addLayer(layer);
	m_createdLayers.push_back(name);
	generator->prepare();

	Rectangle2d const inv = godClientBoundsFromPoints(m_activeEnvironmentZone.boundaryPoints, 384.f);
	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (terrainObject)
		terrainObject->invalidateRegion(inv);
	nudgeGodClientCameraToRefreshDpvs();
	return true;
}

// ----------------------------------------------------------------------

bool GodClientTerrainEditor::createBoundaryPolylineLayer(const char* name, float const corridorWidth)
{
	if (m_activePolyline.controlPoints.size() < 2)
		return false;

	TerrainGenerator* const generator = getTerrainGenerator();
	if (!generator)
		return false;

	std::vector<Vector2d> pts;
	pts.reserve(m_activePolyline.controlPoints.size());
	for (size_t i = 0; i < m_activePolyline.controlPoints.size(); ++i)
		pts.push_back(m_activePolyline.controlPoints[i].position);

	float const w = std::max(2.f, corridorWidth);

	TerrainGenerator::Layer* const layer = new TerrainGenerator::Layer();
	layer->setName(name);
	layer->setActive(true);

	BoundaryPolyline* const boundary = new BoundaryPolyline();
	boundary->setName("Boundary polyline");
	boundary->clearPointList();
	for (size_t i = 0; i < pts.size(); ++i)
		boundary->addPoint(pts[i]);
	boundary->setWidth(w);
	boundary->setFeatherDistance(0.05f);
	layer->addBoundary(boundary);

	generator->addLayer(layer);
	m_createdLayers.push_back(name);
	generator->prepare();

	Rectangle2d const inv = godClientBoundsFromPoints(pts, w + 256.f);
	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (terrainObject)
		terrainObject->invalidateRegion(godClientUnionRects(inv, m_polylineExtent));
	nudgeGodClientCameraToRefreshDpvs();
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

	int const envFamilyId = m_activeEnvironmentZone.environmentFamilyId;
	if (!generator->getEnvironmentGroup().hasFamily(envFamilyId))
	{
		delete layer;
		char buf[192];
		snprintf(buf, sizeof(buf), "Environment zone commit failed: invalid environment family id %d.", envFamilyId);
		MainFrame::getInstance().textToConsole(buf);
		return false;
	}

	AffectorEnvironment* const aff = new AffectorEnvironment();
	aff->setName("Environment");
	aff->setFamilyId(envFamilyId);
	aff->setUseFeatherClampOverride(false);
	layer->addAffector(aff);

	generator->addLayer(layer);
	m_createdLayers.push_back(name);
	generator->prepare();

	Rectangle2d const inv = godClientBoundsFromPoints(m_activeEnvironmentZone.boundaryPoints, 384.f);
	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (terrainObject)
		terrainObject->invalidateRegion(inv);
	nudgeGodClientCameraToRefreshDpvs();

	return true;
}

// ----------------------------------------------------------------------

bool GodClientTerrainEditor::addEnvironmentAffectorForCurrentRegionSelection(int familyId, float featherDistance)
{
	if (!m_hasRegionSelection)
		return false;

	TerrainGenerator* const generator = getTerrainGenerator();
	if (!generator)
		return false;

	if (!generator->getEnvironmentGroup().hasFamily(familyId))
		return false;

	featherDistance = std::max(0.f, featherDistance);

	float const ax0 = std::min(m_regionMinX, m_regionMaxX);
	float const az0 = std::min(m_regionMinZ, m_regionMaxZ);
	float const ax1 = std::max(m_regionMinX, m_regionMaxX);
	float const az1 = std::max(m_regionMinZ, m_regionMaxZ);

	static float const kMinExtent = 0.5f;

	char layerName[128];
	snprintf(layerName, sizeof(layerName), "GodEnvRegion_%d", s_godProceduralAuthoringSerial++);

	TerrainGenerator::Layer* const layer = new TerrainGenerator::Layer();
	layer->setName(layerName);
	layer->setActive(true);

	if (m_regionSelectionCircular && m_regionCircleRadius > 0.01f)
	{
		if (m_regionCircleRadius * 2.f < kMinExtent)
			return false;

		BoundaryCircle* const boundary = new BoundaryCircle();
		boundary->setName("Environment region (circle)");
		boundary->setCircle(m_regionCircleCenterX, m_regionCircleCenterZ, m_regionCircleRadius);
		boundary->setFeatherDistance(featherDistance);
		layer->addBoundary(boundary);
	}
	else
	{
		if ((ax1 - ax0) < kMinExtent || (az1 - az0) < kMinExtent)
			return false;

		Rectangle2d const rect(ax0, az0, ax1, az1);
		BoundaryRectangle* const boundary = new BoundaryRectangle();
		boundary->setName("Environment region (rectangle)");
		boundary->setRectangle(rect);
		boundary->setFeatherDistance(featherDistance);
		layer->addBoundary(boundary);
	}

	AffectorEnvironment* const aff = new AffectorEnvironment();
	aff->setName("Environment");
	aff->setFamilyId(familyId);
	aff->setUseFeatherClampOverride(false);
	layer->addAffector(aff);

	generator->addLayer(layer);
	m_createdLayers.push_back(layerName);
	generator->prepare();

	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (terrainObject)
	{
		float const margin = std::max(64.f, featherDistance + 32.f);
		if (m_regionSelectionCircular && m_regionCircleRadius > 0.01f)
		{
			float const cx0 = m_regionCircleCenterX - m_regionCircleRadius;
			float const cz0 = m_regionCircleCenterZ - m_regionCircleRadius;
			float const cx1 = m_regionCircleCenterX + m_regionCircleRadius;
			float const cz1 = m_regionCircleCenterZ + m_regionCircleRadius;
			terrainObject->invalidateRegion(Rectangle2d(cx0 - margin, cz0 - margin, cx1 + margin, cz1 + margin));
		}
		else
		{
			terrainObject->invalidateRegion(Rectangle2d(ax0 - margin, az0 - margin, ax1 + margin, az1 + margin));
		}
	}

	flushTerrainChanges();
	nudgeGodClientCameraToRefreshDpvs();

	char buf[160];
	snprintf(buf, sizeof(buf), "Added environment region layer '%s' (family %d).", layerName, familyId);
	MainFrame::getInstance().textToConsole(buf);

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

	m_shaderModificationMutex.enter();
	bool const hasShaderMods = !m_shaderModifications.empty();
	m_shaderModificationMutex.leave();
	if (hasShaderMods)
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

bool GodClientTerrainEditor::addFullMapHeightConstantLayer(float height, float featherDistance, char const* optionalLayerNameBase)
{
	TerrainGenerator* const generator = getTerrainGenerator();
	if (!generator)
		return false;

	TerrainObject const* const terrainObject = TerrainObject::getConstInstance();
	if (!terrainObject)
		return false;

	ProceduralTerrainAppearanceTemplate const* const tpl = dynamic_cast<ProceduralTerrainAppearanceTemplate const*>(terrainObject->getAppearance()->getAppearanceTemplate());
	if (!tpl)
		return false;

	float const half = tpl->getMapWidthInMeters() * 0.5f;
	if (half < 1.f)
		return false;

	Rectangle2d const rect(-half, -half, half, half);

	char layerName[128];
	if (optionalLayerNameBase && optionalLayerNameBase[0])
		snprintf(layerName, sizeof(layerName), "%s_%d", optionalLayerNameBase, s_godProceduralAuthoringSerial++);
	else
		snprintf(layerName, sizeof(layerName), "GodHeightConst_%d", s_godProceduralAuthoringSerial++);

	TerrainGenerator::Layer* const layer = new TerrainGenerator::Layer();
	layer->setName(layerName);
	layer->setActive(true);

	BoundaryRectangle* const boundary = new BoundaryRectangle();
	boundary->setName("Full map (height constant)");
	boundary->setRectangle(rect);
	boundary->setFeatherDistance(featherDistance);
	layer->addBoundary(boundary);

	AffectorHeightConstant* const aff = new AffectorHeightConstant();
	aff->setName("Height constant");
	aff->setHeight(height);
	aff->setOperation(TGO_replace);
	layer->addAffector(aff);

	generator->addLayer(layer);
	m_createdLayers.push_back(layerName);
	generator->prepare();

	TerrainObject* const toMut = TerrainObject::getInstance();
	if (toMut)
	{
		float const pad = std::max(64.f, featherDistance + 32.f);
		float const rx0 = std::min(rect.x0, rect.x1);
		float const ry0 = std::min(rect.y0, rect.y1);
		float const rx1 = std::max(rect.x0, rect.x1);
		float const ry1 = std::max(rect.y0, rect.y1);
		toMut->invalidateRegion(Rectangle2d(rx0 - pad, ry0 - pad, rx1 + pad, ry1 + pad));
	}

	flushTerrainChanges();
	nudgeGodClientCameraToRefreshDpvs();
	return true;
}

// ----------------------------------------------------------------------

bool GodClientTerrainEditor::applyImportedHeightRasterFromImageFile(
	char const* const localFilesystemPath,
	int elevationMinMeters,
	int elevationMaxMeters,
	int latticePointsPerEdge,
	bool invertLuminance)
{
	if (!localFilesystemPath || !localFilesystemPath[0])
		return false;

	TerrainObject const* const toeConst = TerrainObject::getConstInstance();
	if (!toeConst)
		return false;

	Appearance const* const appearance = toeConst->getAppearance();
	if (!appearance)
		return false;

	ProceduralTerrainAppearanceTemplate const* const tpl =
		dynamic_cast<ProceduralTerrainAppearanceTemplate const*>(appearance->getAppearanceTemplate());
	if (!tpl)
		return false;

	float const half = tpl->getMapWidthInMeters() * 0.5f;
	if (!(half >= 1.f))
		return false;

	if (!(elevationMaxMeters > elevationMinMeters))
		return false;

	Image* loaded = ImageFormatList::loadImage(localFilesystemPath, true);
	if (!loaded)
	{
		if (MainFrame::getInstanceNullable())
			MainFrame::getInstanceNullable()->textToConsole("Height raster: could not load image (unsupported format?).");
		return false;
	}

	std::vector<float> grey01;
	int iw = 0;
	int ih = 0;
	bool const sampled = gdBuildGrey01RowMajorFromImage(*loaded, grey01, iw, ih);
	delete loaded;

	if (!sampled || iw < 2 || ih < 2 || grey01.size() != static_cast<size_t>(iw * ih))
	{
		if (MainFrame::getInstanceNullable())
			MainFrame::getInstanceNullable()->textToConsole("Height raster: image too small or empty after decode.");
		return false;
	}

	int maxCoordInt = std::max(1, static_cast<int>(std::floor(static_cast<double>(half) + 1e-4)));
	int const latticeKeyClamp = 32767;
	if (maxCoordInt > latticeKeyClamp)
	{
		maxCoordInt = latticeKeyClamp;
		if (MainFrame::getInstanceNullable())
			MainFrame::getInstanceNullable()->textToConsole(
				"Height raster: keyed live edits clamp to +/-32767 m; clipping lattice extent.");
	}

	int const minGx = -maxCoordInt;
	int const maxGx = maxCoordInt;

	int spanInclusive = maxGx - minGx;
	if (spanInclusive <= 0)
		return false;

	int edgeN = std::max(2, latticePointsPerEdge);
	if (edgeN > 1025)
		edgeN = 1025;

	std::set<int> gxcoords;
	std::set<int> gzcoords;
	for (int i = 0; i < edgeN; ++i)
	{
		int gx = minGx + (i * spanInclusive) / (edgeN - 1);
		gxcoords.insert(gx);
		gzcoords.insert(gx); // symmetrical Z axis
	}

	long long totalCellsEst = static_cast<long long>(gxcoords.size()) * static_cast<long long>(gzcoords.size());
	if (totalCellsEst <= 0)
		return false;

	bool const granularUndo = (totalCellsEst <= static_cast<long long>(256 * 256));

	double const denom = std::max(1e-6, static_cast<double>(tpl->getMapWidthInMeters()));
	float const elevMin = static_cast<float>(elevationMinMeters);
	float const elevSpan = static_cast<float>(elevationMaxMeters - elevationMinMeters);

	BrushStroke stroke;
	stroke.centerX = 0.0f;
	stroke.centerZ = 0.0f;
	stroke.radius = half + 1.0f;
	stroke.strength = 1.0f;
	stroke.tool = TM_SetHeight;
	stroke.targetHeight = 0.0f;

	size_t modsWritten = 0;

	typedef std::set<int>::const_iterator IntSetIterator;
	for (IntSetIterator zig = gxcoords.begin(); zig != gxcoords.end(); ++zig)
	{
		for (IntSetIterator ixg = gzcoords.begin(); ixg != gzcoords.end(); ++ixg)
		{
			int const gxWorld = (*zig); // lattice along X axis
			int const gzWorld = (*ixg);

			double const westToEast01 = static_cast<double>(gxWorld + static_cast<double>(half)) / denom;
			double const northTop01 = static_cast<double>(static_cast<double>(half) - static_cast<double>(gzWorld)) / denom;

			float lum = gdBilinearGrey01(grey01, iw, ih, westToEast01, northTop01);
			if (invertLuminance)
				lum = 1.f - lum;
			lum = std::max(0.f, std::min(1.f, lum));

			float const newH = elevMin + lum * elevSpan;

			int const gxBiasPacked = gxWorld + 32768;
			int const gzBiasPacked = gzWorld + 32768;
			if (gxBiasPacked < 0 || gxBiasPacked > 65535 || gzBiasPacked < 0 || gzBiasPacked > 65535)
				continue;

			uint64 const key = (static_cast<uint64>(static_cast<uint32>(gxBiasPacked)) << 32) |
				static_cast<uint64>(static_cast<uint32>(gzBiasPacked));

			HeightModificationMap::iterator hit = m_heightModifications.find(key);

			// IMPORTANT: Avoid TerrainObject::getHeight(...) here — it walks client chunks/collision asynchronously and has
			// repeatedly faulted during large bulk applies. Cells already keyed use prior edits as the "before"; cold
			// cells approximate pre-import mesh using the UI min elevation (undo for first-time vertices restores that floor).
			float beforeH = elevMin;
			float baselineForUndo = elevMin;
			if (hit != m_heightModifications.end())
			{
				beforeH = hit->second.modifiedHeight;
				baselineForUndo = hit->second.originalHeight;
			}

			HeightModification snapshot;
			snapshot.worldX = static_cast<float>(gxWorld);
			snapshot.worldZ = static_cast<float>(gzWorld);
			snapshot.originalHeight = beforeH;
			snapshot.modifiedHeight = newH;
			snapshot.timestamp = Clock::frameTime();

			if (granularUndo)
				stroke.modifications.push_back(snapshot);

			HeightModification stored;
			stored.worldX = snapshot.worldX;
			stored.worldZ = snapshot.worldZ;
			stored.originalHeight = baselineForUndo;
			stored.modifiedHeight = newH;
			stored.timestamp = snapshot.timestamp;
			m_heightModifications[key] = stored;
			++modsWritten;
		}
	}

	if (modsWritten == 0)
	{
		if (MainFrame::getInstanceNullable())
			MainFrame::getInstanceNullable()->textToConsole("Height raster: no lattice points written (coordinate range?).");
		return false;
	}

	if (granularUndo && !stroke.modifications.empty())
	{
		m_undoStack.push_back(stroke);
		while (static_cast<int>(m_undoStack.size()) > MAX_UNDO_STROKES)
			m_undoStack.erase(m_undoStack.begin());
		m_redoStack.clear();
	}

	flushTerrainChanges();
	nudgeGodClientCameraToRefreshDpvs();

	if (MainFrame::getInstanceNullable())
	{
		char summary[320];
		snprintf(
			summary,
			sizeof(summary),
			"Height raster: wrote %zu lattice heights (granular_undo=%s); cold cells baseline=min elevation.",
			static_cast<size_t>(modsWritten),
			granularUndo ? "yes" : "no");
		MainFrame::getInstanceNullable()->textToConsole(summary);
	}

	return true;
}

// ----------------------------------------------------------------------

bool GodClientTerrainEditor::addFullMapShaderConstantLayer(int shaderFamilyId, float featherDistance, char const* optionalLayerNameBase)
{
	if (!shaderFamilyId)
		return false;

	TerrainGenerator* const generator = getTerrainGenerator();
	if (!generator)
		return false;

	TerrainObject const* const terrainObject = TerrainObject::getConstInstance();
	if (!terrainObject)
		return false;

	ProceduralTerrainAppearanceTemplate const* const tpl = dynamic_cast<ProceduralTerrainAppearanceTemplate const*>(terrainObject->getAppearance()->getAppearanceTemplate());
	if (!tpl)
		return false;

	float const half = tpl->getMapWidthInMeters() * 0.5f;
	if (half < 1.f)
		return false;

	Rectangle2d const rect(-half, -half, half, half);

	char layerName[128];
	if (optionalLayerNameBase && optionalLayerNameBase[0])
		snprintf(layerName, sizeof(layerName), "%s_%d", optionalLayerNameBase, s_godProceduralAuthoringSerial++);
	else
		snprintf(layerName, sizeof(layerName), "GodShaderConst_%d", s_godProceduralAuthoringSerial++);

	TerrainGenerator::Layer* const layer = new TerrainGenerator::Layer();
	layer->setName(layerName);
	layer->setActive(true);

	BoundaryRectangle* const boundary = new BoundaryRectangle();
	boundary->setName("Full map (shader constant)");
	boundary->setRectangle(rect);
	boundary->setFeatherDistance(featherDistance);
	layer->addBoundary(boundary);

	AffectorShaderConstant* const aff = new AffectorShaderConstant();
	aff->setName("Shader constant");
	aff->setFamilyId(shaderFamilyId);
	layer->addAffector(aff);

	generator->addLayer(layer);
	m_createdLayers.push_back(layerName);
	generator->prepare();

	TerrainObject* const toMut = TerrainObject::getInstance();
	if (toMut)
	{
		float const pad = std::max(256.f, featherDistance + 64.f);
		float const rx0 = std::min(rect.x0, rect.x1);
		float const ry0 = std::min(rect.y0, rect.y1);
		float const rx1 = std::max(rect.x0, rect.x1);
		float const ry1 = std::max(rect.y0, rect.y1);
		toMut->invalidateRegion(Rectangle2d(rx0 - pad, ry0 - pad, rx1 + pad, ry1 + pad));
	}

	flushTerrainChanges();
	nudgeGodClientCameraToRefreshDpvs();
	return true;
}

// ----------------------------------------------------------------------

bool GodClientTerrainEditor::addExcludeLayerForRectangle(Rectangle2d const& rectXZ, float featherDistance, char const* optionalLayerNameBase)
{
	TerrainGenerator* const generator = getTerrainGenerator();
	if (!generator)
		return false;

	float const ax0 = std::min(rectXZ.x0, rectXZ.x1);
	float const az0 = std::min(rectXZ.y0, rectXZ.y1);
	float const ax1 = std::max(rectXZ.x0, rectXZ.x1);
	float const az1 = std::max(rectXZ.y0, rectXZ.y1);
	Rectangle2d const rect(ax0, az0, ax1, az1);

	static float const kMinExtent = 0.5f;
	if ((ax1 - ax0) < kMinExtent || (az1 - az0) < kMinExtent)
		return false;

	char layerName[128];
	if (optionalLayerNameBase && optionalLayerNameBase[0])
		snprintf(layerName, sizeof(layerName), "%s_%d", optionalLayerNameBase, s_godProceduralAuthoringSerial++);
	else
		snprintf(layerName, sizeof(layerName), "GodExclude_%d", s_godProceduralAuthoringSerial++);

	TerrainGenerator::Layer* const layer = new TerrainGenerator::Layer();
	layer->setName(layerName);
	layer->setActive(true);

	BoundaryRectangle* const boundary = new BoundaryRectangle();
	boundary->setName("Exclude boundary");
	boundary->setRectangle(rect);
	boundary->setFeatherDistance(featherDistance);
	layer->addBoundary(boundary);

	AffectorExclude* const ex = new AffectorExclude();
	ex->setName("Exclude");
	layer->addAffector(ex);

	generator->addLayer(layer);
	m_createdLayers.push_back(layerName);
	generator->prepare();

	TerrainObject* const terrainObject = TerrainObject::getInstance();
	if (terrainObject)
	{
		float const margin = std::max(64.f, featherDistance + 32.f);
		terrainObject->invalidateRegion(Rectangle2d(ax0 - margin, az0 - margin, ax1 + margin, az1 + margin));
	}

	flushTerrainChanges();
	nudgeGodClientCameraToRefreshDpvs();
	return true;
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
