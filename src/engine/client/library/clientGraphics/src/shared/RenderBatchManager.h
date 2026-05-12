// ======================================================================
//
// RenderBatchManager.h
// Copyright 2026 Titan Development Team
// All Rights Reserved.
//
// Manages batching of render primitives to minimize draw calls
// and state changes for improved rendering performance.
//
// ======================================================================

#ifndef INCLUDED_RenderBatchManager_H
#define INCLUDED_RenderBatchManager_H

// ======================================================================

#include "sharedMath/Transform.h"
#include "sharedMath/Vector.h"
#include "sharedMath/VectorRgba.h"

#include <vector>

// ======================================================================

class StaticShader;
class StaticVertexBuffer;
class StaticIndexBuffer;
class Texture;
class ShaderPrimitive;

// ======================================================================

/**
 * RenderBatchManager optimizes rendering by:
 *
 * 1. Sorting primitives by state (shader, texture, vertex format)
 * 2. Batching identical meshes using instancing
 * 3. Merging compatible draw calls
 * 4. Tracking state changes to minimize redundant API calls
 *
 * Usage:
 *   beginFrame();
 *   submitPrimitive(primitive, transform);  // Called many times
 *   endFrame();  // Sorts, batches, and renders
 */
class RenderBatchManager
{
public:
	// Batch key for sorting (lower = renders first)
	struct BatchKey
	{
		uint32 shaderHash;        // Shader identifier
		uint32 textureHash;       // Primary texture identifier
		uint32 vertexFormatHash;  // Vertex format identifier
		uint32 lightHash;         // Active lights bitmask (phase integration) or 0
		uint32 blendState;        // Blend mode (opaque first, then alpha)
		float  depth;             // Distance from camera (for alpha sorting)

		bool operator<(BatchKey const & other) const;
	};

	// Render item
	struct RenderItem
	{
		ShaderPrimitive const * primitive;
		Transform               transform;
		Vector                  objectScale;
		VectorRgba              color;
		float                   depth;
		BatchKey                key;
		bool                    replayLightsFromMask;
		uint32                  lightMask;
	};

	// Batch statistics
	struct Statistics
	{
		int totalPrimitives;       // Total primitives submitted
		int totalBatches;          // Number of batches created
		int totalDrawCalls;        // Actual draw calls issued
		int stateChanges;          // Shader/texture changes
		int instancedDrawCalls;    // Draw calls using instancing
		int instancedPrimitives;   // Primitives drawn via instancing
		float sortTime;            // Time spent sorting (ms)
		float renderTime;          // Time spent rendering (ms)

		void reset();
	};

	// Sort modes
	enum SortMode
	{
		SM_frontToBack,    // Opaque: minimize overdraw
		SM_backToFront,    // Transparent: correct blending
		SM_byState,        // Minimize state changes
		SM_hybrid          // Opaque front-to-back, transparent back-to-front
	};

public:
	static void install();
	static void remove();

	// Frame management
	static void beginFrame();
	static void endFrame();

	// Submit primitives for batched rendering
	static void submitPrimitive(ShaderPrimitive const * primitive, Transform const & transform);
	static void submitPrimitive(ShaderPrimitive const * primitive, Transform const & transform, VectorRgba const & color);
	static void submitPrimitiveImmediate(ShaderPrimitive const * primitive, Transform const & transform);  // Bypass batching

	// ShaderPrimitiveSorter: preserves phase sort order and optional light replay (DebugFlags ClientGraphics/usePhaseOrderedBatching)
	static bool isInstalled();
	static bool getUsePhaseOrderedBatching();

	static void beginPhaseOrderedBatch(Vector const & depthSortOrigin);
	static void flushPhaseOrderedBatch();
	static void beginPhaseOrderedBatchResume();
	static void endPhaseOrderedBatchIntegration();
	static void submitPhaseOrderedPrimitive(ShaderPrimitive const * primitive, Transform const & transform, Vector const & objectScale, uint32 lightMask);

	// Configuration
	static void setSortMode(SortMode mode);
	static SortMode getSortMode();
	static void setInstancingEnabled(bool enabled);
	static bool isInstancingEnabled();
	static void setMaxBatchSize(int maxSize);
	static void setDepthSortOrigin(Vector const & origin);

	// Statistics
	static Statistics const & getStatistics();
	static void resetStatistics();

	// Debug
	static void setDebugShowBatches(bool enabled);
	static void setDebugShowInstancing(bool enabled);

private:
	RenderBatchManager();
	~RenderBatchManager();
	RenderBatchManager(RenderBatchManager const &);
	RenderBatchManager & operator=(RenderBatchManager const &);

	// Internal methods
	static void sortItems();
	static void buildBatches();
	static void renderBatches();
	static void renderSingleItem(RenderItem const & item);
	static void renderInstancedBatch(std::vector<RenderItem> const & items, int start, int count);
	static void renderItemsInSubmissionOrder();

	static BatchKey computeBatchKey(ShaderPrimitive const * primitive, float depth, uint32 lightMask);
	static uint32 computeShaderHash(StaticShader const * shader);
	static uint32 computeTextureHash(Texture const * texture);
	static uint32 computeVertexFormatHash(StaticVertexBuffer const * vb);
};

// ======================================================================

#endif

