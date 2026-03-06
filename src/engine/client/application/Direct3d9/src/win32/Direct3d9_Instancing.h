// ======================================================================
//
// Direct3d9_Instancing.h
// Copyright 2026 Titan Development Team
// All Rights Reserved.
//
// Hardware instancing support for Direct3D 9 (SM3.0)
// Provides efficient batching of identical meshes with per-instance transforms
//
// ======================================================================

#ifndef INCLUDED_Direct3d9_Instancing_H
#define INCLUDED_Direct3d9_Instancing_H

// ======================================================================

#include "sharedMath/Transform.h"
#include "sharedMath/Vector.h"
#include "sharedMath/VectorRgba.h"

#include <d3d9.h>
#include <vector>

// ======================================================================

class StaticVertexBuffer;
class StaticIndexBuffer;
class StaticShader;

// ======================================================================

/**
 * Manages hardware instancing for Direct3D 9.
 *
 * Supports two modes:
 * 1. Hardware Instancing (SM3.0+): Uses SetStreamSourceFreq() for true GPU instancing
 * 2. Pseudo-Instancing (SM2.0): Batches up to 8 instances using shader constants
 *
 * Usage:
 *   1. Call beginBatch() with the mesh to instance
 *   2. Call addInstance() for each instance with its transform
 *   3. Call endBatch() to flush all instances in a single draw call
 */
class Direct3d9_Instancing
{
public:
	// Instance data structure (matches shader input)
	struct InstanceData
	{
		float worldMatrix[12];     // World matrix (3x4, row-major)
		float color[4];            // Per-instance color/tint
		float userData[4];         // Custom per-instance data
	};

	// Instancing capabilities
	enum InstancingMode
	{
		IM_none,                   // No instancing support
		IM_pseudoInstancing,       // SM2.0 shader constants (up to 8 instances)
		IM_hardwareInstancing      // SM3.0 SetStreamSourceFreq
	};

public:
	static void install();
	static void remove();
	static void lostDevice();
	static void restoreDevice();

	// Capability queries
	static bool              isInstancingSupported();
	static InstancingMode    getInstancingMode();
	static int               getMaxInstancesPerBatch();

	// Batch management
	static bool  beginBatch(StaticVertexBuffer const * vertexBuffer,
	                        StaticIndexBuffer const * indexBuffer,
	                        StaticShader const * shader);
	static bool  addInstance(Transform const & transform,
	                         VectorRgba const & color = VectorRgba::solidWhite,
	                         float const * userData = nullptr);
	static int   endBatch();  // Returns number of instances drawn

	// Manual instance buffer management
	static bool  createInstanceBuffer(int maxInstances);
	static void  destroyInstanceBuffer();

	// Statistics
	static int   getDrawCallsSaved();
	static int   getTotalInstancesDrawn();
	static void  resetStatistics();

private:
	Direct3d9_Instancing();
	~Direct3d9_Instancing();
	Direct3d9_Instancing(Direct3d9_Instancing const &);
	Direct3d9_Instancing & operator=(Direct3d9_Instancing const &);

	static void  flushBatch();
	static void  drawHardwareInstanced(int instanceCount);
	static void  drawPseudoInstanced(int instanceCount);
	static bool  updateInstanceBuffer();
};

// ======================================================================

#endif

