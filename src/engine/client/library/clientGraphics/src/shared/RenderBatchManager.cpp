// ======================================================================
//
// RenderBatchManager.cpp
// Copyright 2026 Titan Development Team
// All Rights Reserved.
//
// Render batch management implementation
//
// ======================================================================

#include "clientGraphics/FirstClientGraphics.h"
#include "clientGraphics/RenderBatchManager.h"

#include "clientGraphics/Graphics.h"
#include "clientGraphics/ShaderPrimitive.h"
#include "clientGraphics/StaticShader.h"
#include "clientGraphics/StaticVertexBuffer.h"
#include "clientGraphics/Texture.h"

#include "sharedDebug/DebugFlags.h"
#include "sharedDebug/PerformanceTimer.h"
#include "sharedFoundation/ExitChain.h"
#include "sharedMath/Vector.h"

#include <algorithm>

// ======================================================================

namespace RenderBatchManagerNamespace
{
	// Configuration
	int const cs_defaultMaxBatchSize = 256;
	int const cs_instancingThreshold = 4;  // Min items to trigger instancing

	// State
	bool ms_installed = false;
	bool ms_inFrame = false;

	// Settings
	RenderBatchManager::SortMode ms_sortMode = RenderBatchManager::SM_hybrid;
	bool ms_instancingEnabled = true;
	int ms_maxBatchSize = cs_defaultMaxBatchSize;
	Vector ms_depthSortOrigin;

	// Render items
	std::vector<RenderBatchManager::RenderItem> ms_items;
	std::vector<RenderBatchManager::RenderItem> ms_opaqueItems;
	std::vector<RenderBatchManager::RenderItem> ms_transparentItems;

	// Statistics
	RenderBatchManager::Statistics ms_stats;

	// Debug
	bool ms_debugShowBatches = false;
	bool ms_debugShowInstancing = false;

	// Hash function (FNV-1a)
	uint32 fnv1aHash(void const * data, size_t size, uint32 hash = 2166136261u)
	{
		uint8 const * bytes = static_cast<uint8 const *>(data);
		for (size_t i = 0; i < size; ++i)
		{
			hash ^= bytes[i];
			hash *= 16777619u;
		}
		return hash;
	}
}

using namespace RenderBatchManagerNamespace;

// ======================================================================

bool RenderBatchManager::BatchKey::operator<(BatchKey const & other) const
{
	// Primary sort: blend state (opaque first)
	if (blendState != other.blendState)
		return blendState < other.blendState;

	// Secondary sort for opaque: state-based (minimize changes)
	if (blendState == 0)  // Opaque
	{
		if (shaderHash != other.shaderHash)
			return shaderHash < other.shaderHash;

		if (textureHash != other.textureHash)
			return textureHash < other.textureHash;

		if (vertexFormatHash != other.vertexFormatHash)
			return vertexFormatHash < other.vertexFormatHash;

		// Same state: sort front-to-back
		return depth < other.depth;
	}
	else  // Transparent
	{
		// Sort back-to-front for correct blending
		return depth > other.depth;
	}
}

// ----------------------------------------------------------------------

void RenderBatchManager::Statistics::reset()
{
	totalPrimitives = 0;
	totalBatches = 0;
	totalDrawCalls = 0;
	stateChanges = 0;
	instancedDrawCalls = 0;
	instancedPrimitives = 0;
	sortTime = 0.0f;
	renderTime = 0.0f;
}

// ======================================================================

void RenderBatchManager::install()
{
	DEBUG_FATAL(ms_installed, ("RenderBatchManager already installed"));

	ms_items.reserve(4096);
	ms_opaqueItems.reserve(4096);
	ms_transparentItems.reserve(1024);

	ms_stats.reset();
	ms_installed = true;

	ExitChain::add(remove, "RenderBatchManager::remove");

	DEBUG_REPORT_LOG(true, ("RenderBatchManager installed: instancing=%s, maxBatch=%d\n",
		ms_instancingEnabled ? "enabled" : "disabled", ms_maxBatchSize));
}

// ----------------------------------------------------------------------

void RenderBatchManager::remove()
{
	DEBUG_FATAL(!ms_installed, ("RenderBatchManager not installed"));

	ms_items.clear();
	ms_opaqueItems.clear();
	ms_transparentItems.clear();
	ms_installed = false;
}

// ----------------------------------------------------------------------

void RenderBatchManager::beginFrame()
{
	DEBUG_FATAL(!ms_installed, ("RenderBatchManager not installed"));
	DEBUG_FATAL(ms_inFrame, ("RenderBatchManager::beginFrame called while already in frame"));

	ms_items.clear();
	ms_opaqueItems.clear();
	ms_transparentItems.clear();
	ms_stats.reset();
	ms_inFrame = true;
}

// ----------------------------------------------------------------------

void RenderBatchManager::endFrame()
{
	DEBUG_FATAL(!ms_inFrame, ("RenderBatchManager::endFrame called without beginFrame"));

	// Sort items
	sortItems();

	// Build and render batches
	buildBatches();
	renderBatches();

	ms_inFrame = false;
}

// ----------------------------------------------------------------------

void RenderBatchManager::submitPrimitive(ShaderPrimitive const * primitive, Transform const & transform)
{
	submitPrimitive(primitive, transform, VectorRgba::solidWhite);
}

// ----------------------------------------------------------------------

void RenderBatchManager::submitPrimitive(ShaderPrimitive const * primitive,
                                          Transform const & transform,
                                          VectorRgba const & color)
{
	if (!primitive || !ms_inFrame)
		return;

	// Calculate depth from camera
	Vector const position = transform.getPosition_p();
	float const depth = (position - ms_depthSortOrigin).magnitudeSquared();

	RenderItem item;
	item.primitive = primitive;
	item.transform = transform;
	item.color = color;
	item.depth = depth;
	item.key = computeBatchKey(primitive, depth);

	ms_items.push_back(item);
	++ms_stats.totalPrimitives;
}

// ----------------------------------------------------------------------

void RenderBatchManager::submitPrimitiveImmediate(ShaderPrimitive const * primitive,
                                                   Transform const & transform)
{
	if (!primitive)
		return;

	// Render immediately, bypassing batch system
	RenderItem item;
	item.primitive = primitive;
	item.transform = transform;
	item.color = VectorRgba::solidWhite;
	item.depth = 0.0f;

	renderSingleItem(item);
	++ms_stats.totalDrawCalls;
}

// ----------------------------------------------------------------------

void RenderBatchManager::setSortMode(SortMode mode)
{
	ms_sortMode = mode;
}

// ----------------------------------------------------------------------

RenderBatchManager::SortMode RenderBatchManager::getSortMode()
{
	return ms_sortMode;
}

// ----------------------------------------------------------------------

void RenderBatchManager::setInstancingEnabled(bool enabled)
{
	ms_instancingEnabled = enabled;
}

// ----------------------------------------------------------------------

bool RenderBatchManager::isInstancingEnabled()
{
	return ms_instancingEnabled;
}

// ----------------------------------------------------------------------

void RenderBatchManager::setMaxBatchSize(int maxSize)
{
	ms_maxBatchSize = std::max(1, maxSize);
}

// ----------------------------------------------------------------------

void RenderBatchManager::setDepthSortOrigin(Vector const & origin)
{
	ms_depthSortOrigin = origin;
}

// ----------------------------------------------------------------------

RenderBatchManager::Statistics const & RenderBatchManager::getStatistics()
{
	return ms_stats;
}

// ----------------------------------------------------------------------

void RenderBatchManager::resetStatistics()
{
	ms_stats.reset();
}

// ----------------------------------------------------------------------

void RenderBatchManager::setDebugShowBatches(bool enabled)
{
	ms_debugShowBatches = enabled;
}

// ----------------------------------------------------------------------

void RenderBatchManager::setDebugShowInstancing(bool enabled)
{
	ms_debugShowInstancing = enabled;
}

// ----------------------------------------------------------------------

void RenderBatchManager::sortItems()
{
	PerformanceTimer timer;
	timer.start();

	// Separate opaque and transparent items
	for (size_t i = 0; i < ms_items.size(); ++i)
	{
		RenderItem const & item = ms_items[i];

		if (item.key.blendState == 0)
			ms_opaqueItems.push_back(item);
		else
			ms_transparentItems.push_back(item);
	}

	// Sort based on mode
	switch (ms_sortMode)
	{
	case SM_frontToBack:
		std::sort(ms_opaqueItems.begin(), ms_opaqueItems.end(),
			[](RenderItem const & a, RenderItem const & b) { return a.depth < b.depth; });
		std::sort(ms_transparentItems.begin(), ms_transparentItems.end(),
			[](RenderItem const & a, RenderItem const & b) { return a.depth < b.depth; });
		break;

	case SM_backToFront:
		std::sort(ms_opaqueItems.begin(), ms_opaqueItems.end(),
			[](RenderItem const & a, RenderItem const & b) { return a.depth > b.depth; });
		std::sort(ms_transparentItems.begin(), ms_transparentItems.end(),
			[](RenderItem const & a, RenderItem const & b) { return a.depth > b.depth; });
		break;

	case SM_byState:
		std::sort(ms_opaqueItems.begin(), ms_opaqueItems.end(),
			[](RenderItem const & a, RenderItem const & b) { return a.key < b.key; });
		std::sort(ms_transparentItems.begin(), ms_transparentItems.end(),
			[](RenderItem const & a, RenderItem const & b) { return a.key < b.key; });
		break;

	case SM_hybrid:
	default:
		// Opaque: sort by state, then front-to-back within same state
		std::sort(ms_opaqueItems.begin(), ms_opaqueItems.end(),
			[](RenderItem const & a, RenderItem const & b) { return a.key < b.key; });

		// Transparent: sort back-to-front
		std::sort(ms_transparentItems.begin(), ms_transparentItems.end(),
			[](RenderItem const & a, RenderItem const & b) { return a.depth > b.depth; });
		break;
	}

	timer.stop();
	ms_stats.sortTime = timer.getElapsedTime() * 1000.0f;
}

// ----------------------------------------------------------------------

void RenderBatchManager::buildBatches()
{
	// Batches are built during rendering to avoid extra memory allocation
}

// ----------------------------------------------------------------------

void RenderBatchManager::renderBatches()
{
	PerformanceTimer timer;
	timer.start();

	uint32 currentShaderHash = 0;
	uint32 currentTextureHash = 0;

	// Render opaque items
	for (size_t i = 0; i < ms_opaqueItems.size(); )
	{
		RenderItem const & item = ms_opaqueItems[i];

		// Track state changes
		if (item.key.shaderHash != currentShaderHash)
		{
			++ms_stats.stateChanges;
			currentShaderHash = item.key.shaderHash;
		}
		if (item.key.textureHash != currentTextureHash)
		{
			++ms_stats.stateChanges;
			currentTextureHash = item.key.textureHash;
		}

		// Check for instancing opportunity
		if (ms_instancingEnabled)
		{
			// Count identical items
			int batchCount = 1;
			while (i + batchCount < ms_opaqueItems.size() &&
			       batchCount < ms_maxBatchSize &&
			       ms_opaqueItems[i + batchCount].key.shaderHash == item.key.shaderHash &&
			       ms_opaqueItems[i + batchCount].key.textureHash == item.key.textureHash &&
			       ms_opaqueItems[i + batchCount].key.vertexFormatHash == item.key.vertexFormatHash)
			{
				++batchCount;
			}

			if (batchCount >= cs_instancingThreshold)
			{
				// Use instancing
				renderInstancedBatch(ms_opaqueItems, static_cast<int>(i), batchCount);
				++ms_stats.instancedDrawCalls;
				ms_stats.instancedPrimitives += batchCount;
				++ms_stats.totalBatches;
				i += batchCount;
				continue;
			}
		}

		// Single item render
		renderSingleItem(item);
		++ms_stats.totalDrawCalls;
		++i;
	}

	// Render transparent items (no instancing, preserve order)
	for (size_t i = 0; i < ms_transparentItems.size(); ++i)
	{
		RenderItem const & item = ms_transparentItems[i];

		if (item.key.shaderHash != currentShaderHash)
		{
			++ms_stats.stateChanges;
			currentShaderHash = item.key.shaderHash;
		}

		renderSingleItem(item);
		++ms_stats.totalDrawCalls;
	}

	timer.stop();
	ms_stats.renderTime = timer.getElapsedTime() * 1000.0f;
}

// ----------------------------------------------------------------------

void RenderBatchManager::renderSingleItem(RenderItem const & item)
{
	// Set transform
	Graphics::setObjectToWorldTransformAndScale(item.transform, Vector::xyz111);

	// Render the primitive
	if (item.primitive)
	{
		item.primitive->prepareToDraw();
		item.primitive->draw();
	}
}

// ----------------------------------------------------------------------

void RenderBatchManager::renderInstancedBatch(std::vector<RenderItem> const & items,
                                               int start, int count)
{
	// This would use Direct3d9_Instancing for actual batched rendering
	// For now, fall back to sequential rendering

	for (int i = 0; i < count; ++i)
	{
		renderSingleItem(items[start + i]);
	}
}

// ----------------------------------------------------------------------

RenderBatchManager::BatchKey RenderBatchManager::computeBatchKey(ShaderPrimitive const * primitive,
                                                                   float depth)
{
	BatchKey key;
	key.depth = depth;
	key.blendState = 0;

	if (primitive)
	{
		// Get shader for hashing - prepareToView returns a reference
		StaticShader const & shader = primitive->prepareToView();
		key.shaderHash = computeShaderHash(&shader);

		// Use the number of passes as a simple transparency heuristic:
		// transparent shaders typically have more passes or are in alpha-blend phase
		int const numPasses = shader.getNumberOfPasses();
		if (numPasses > 1)
			key.blendState = 1;  // Likely transparent/blended

		// Texture hash - would need access to bound textures
		key.textureHash = 0;

		// Vertex format hash
		key.vertexFormatHash = 0;
	}
	else
	{
		key.shaderHash = 0;
		key.textureHash = 0;
		key.vertexFormatHash = 0;
	}

	return key;
}

// ----------------------------------------------------------------------

uint32 RenderBatchManager::computeShaderHash(StaticShader const * shader)
{
	if (!shader)
		return 0;

	// Use shader pointer as simple hash
	return static_cast<uint32>(reinterpret_cast<uintptr_t>(shader));
}

// ----------------------------------------------------------------------

uint32 RenderBatchManager::computeTextureHash(Texture const * texture)
{
	if (!texture)
		return 0;

	return static_cast<uint32>(reinterpret_cast<uintptr_t>(texture));
}

// ----------------------------------------------------------------------

uint32 RenderBatchManager::computeVertexFormatHash(StaticVertexBuffer const * vb)
{
	if (!vb)
		return 0;

	// Hash based on vertex size as a simple format discriminator
	int const vertexSize = vb->getVertexSize();
	return fnv1aHash(&vertexSize, sizeof(vertexSize));
}

// ======================================================================

