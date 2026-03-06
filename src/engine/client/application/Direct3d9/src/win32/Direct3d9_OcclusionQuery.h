// ======================================================================
//
// Direct3d9_OcclusionQuery.h
// Copyright 2026 Titan Development Team
// All Rights Reserved.
//
// Hardware occlusion query support for Direct3D 9
// Provides frame-delayed visibility testing to avoid GPU stalls
//
// ======================================================================

#ifndef INCLUDED_Direct3d9_OcclusionQuery_H
#define INCLUDED_Direct3d9_OcclusionQuery_H

// ======================================================================

#include "sharedMath/AxialBox.h"
#include "sharedMath/Sphere.h"

#include <d3d9.h>

// ======================================================================

class Object;

// ======================================================================

/**
 * Manages hardware occlusion queries for visibility culling.
 *
 * Uses a query pool with frame-delayed result retrieval to avoid
 * stalling the GPU. Results from frame N are used for visibility
 * decisions in frame N+1 or N+2.
 *
 * Usage:
 *   1. Call beginQuery() before rendering the bounding volume
 *   2. Render a simplified representation (box/sphere)
 *   3. Call endQuery() after rendering
 *   4. Check isVisible() in subsequent frames
 */
class Direct3d9_OcclusionQuery
{
public:
	// Query handle (opaque)
	typedef int QueryHandle;
	static QueryHandle const InvalidQuery = -1;

	// Query state
	enum QueryState
	{
		QS_idle,           // Query not in use
		QS_pending,        // Query issued, waiting for results
		QS_ready,          // Results available
		QS_error           // Query failed
	};

	// Query result
	struct QueryResult
	{
		QueryHandle handle;
		QueryState  state;
		bool        visible;       // True if any pixels passed
		DWORD       pixelsDrawn;   // Number of pixels that passed depth test
		int         frameIssued;   // Frame number when query was issued
	};

public:
	static void install();
	static void remove();
	static void lostDevice();
	static void restoreDevice();

	// Capability queries
	static bool isSupported();
	static int  getMaxQueries();

	// Frame management
	static void beginFrame();
	static void endFrame();

	// Query management
	static QueryHandle allocateQuery();
	static void        releaseQuery(QueryHandle handle);
	static void        releaseAllQueries();

	// Query execution
	static bool beginQuery(QueryHandle handle);
	static bool endQuery(QueryHandle handle);

	// Result retrieval (non-blocking)
	static QueryState  getQueryState(QueryHandle handle);
	static bool        getQueryResult(QueryHandle handle, QueryResult & outResult);
	static bool        isVisible(QueryHandle handle);  // Returns last known visibility

	// Convenience: render bounding volume and query
	static bool renderBoundingBoxQuery(QueryHandle handle, AxialBox const & box);
	static bool renderBoundingSphereQuery(QueryHandle handle, Sphere const & sphere);

	// Statistics
	static int  getQueriesIssued();
	static int  getQueriesSucceeded();
	static int  getObjectsCulled();
	static void resetStatistics();

	// Configuration
	static void setQueryLatency(int frames);  // Frames to wait before using results
	static int  getQueryLatency();

private:
	Direct3d9_OcclusionQuery();
	~Direct3d9_OcclusionQuery();
	Direct3d9_OcclusionQuery(Direct3d9_OcclusionQuery const &);
	Direct3d9_OcclusionQuery & operator=(Direct3d9_OcclusionQuery const &);

	static void collectResults();
	static void renderBox(AxialBox const & box);
};

// ======================================================================

#endif

