// ======================================================================
//
// Direct3d9_OcclusionQuery.cpp
// Copyright 2026 Titan Development Team
// All Rights Reserved.
//
// Hardware occlusion query support for Direct3D 9
//
// ======================================================================

#include "FirstDirect3d9.h"
#include "Direct3d9_OcclusionQuery.h"

#include "ConfigDirect3d9.h"
#include "Direct3d9.h"
#include "Direct3d9_StateCache.h"

#include "sharedDebug/DebugFlags.h"
#include "sharedMath/Vector.h"
#include "sharedMath/Sphere.h"

#include <d3d9.h>
#include <vector>

// ======================================================================

namespace Direct3d9_OcclusionQueryNamespace
{
	// Configuration
	int const cs_defaultMaxQueries = 256;
	int const cs_defaultQueryLatency = 2;  // Frames to wait before using results

	// Internal query structure
	struct QueryData
	{
		IDirect3DQuery9 * query;
		Direct3d9_OcclusionQuery::QueryState state;
		bool visible;
		DWORD pixelsDrawn;
		int frameIssued;
		bool inUse;

		QueryData()
			: query(nullptr)
			, state(Direct3d9_OcclusionQuery::QS_idle)
			, visible(true)  // Default to visible
			, pixelsDrawn(0)
			, frameIssued(-1)
			, inUse(false)
		{}
	};

	// State
	bool ms_installed = false;
	bool ms_supported = false;
	int  ms_maxQueries = 0;
	int  ms_queryLatency = cs_defaultQueryLatency;
	int  ms_currentFrame = 0;

	// Query pool
	std::vector<QueryData> ms_queries;
	std::vector<int> ms_freeQueries;
	std::vector<int> ms_pendingQueries;

	// Bounding box vertex buffer for query rendering
	IDirect3DVertexBuffer9 * ms_boxVertexBuffer = nullptr;
	IDirect3DIndexBuffer9 * ms_boxIndexBuffer = nullptr;

	// Statistics
	int ms_queriesIssued = 0;
	int ms_queriesSucceeded = 0;
	int ms_objectsCulled = 0;

	// Helper functions
	bool createQueryPool(int maxQueries);
	void destroyQueryPool();
	bool createBoxBuffers();
	void destroyBoxBuffers();
}

using namespace Direct3d9_OcclusionQueryNamespace;

// ======================================================================

void Direct3d9_OcclusionQuery::install()
{
	DEBUG_FATAL(ms_installed, ("Direct3d9_OcclusionQuery already installed"));

	// Check if occlusion queries are enabled in config
	if (!ConfigDirect3d9::getEnableOcclusionQueries())
	{
		ms_supported = false;
		ms_installed = true;
		DEBUG_REPORT_LOG(true, ("Direct3d9_OcclusionQuery: disabled by config\n"));
		return;
	}

	// Get query latency from config
	ms_queryLatency = ConfigDirect3d9::getOcclusionQueryLatency();
	if (ms_queryLatency < 1)
		ms_queryLatency = cs_defaultQueryLatency;

	IDirect3DDevice9 * const device = Direct3d9::getDevice();
	if (!device)
	{
		ms_supported = false;
		ms_installed = true;
		return;
	}

	// Check if occlusion queries are supported
	IDirect3DQuery9 * testQuery = nullptr;
	HRESULT hr = device->CreateQuery(D3DQUERYTYPE_OCCLUSION, &testQuery);

	if (SUCCEEDED(hr) && testQuery)
	{
		ms_supported = true;
		testQuery->Release();

		// Create query pool
		if (!createQueryPool(cs_defaultMaxQueries))
		{
			ms_supported = false;
		}

		// Create box geometry for bounding volume queries
		if (ms_supported && !createBoxBuffers())
		{
			DEBUG_WARNING(true, ("Failed to create occlusion query box buffers"));
		}
	}
	else
	{
		ms_supported = false;
	}

	ms_maxQueries = ms_supported ? cs_defaultMaxQueries : 0;
	ms_installed = true;


	DEBUG_REPORT_LOG(true, ("Direct3d9_OcclusionQuery: supported=%s, maxQueries=%d\n",
		ms_supported ? "yes" : "no", ms_maxQueries));
}

// ----------------------------------------------------------------------

void Direct3d9_OcclusionQuery::remove()
{
	DEBUG_FATAL(!ms_installed, ("Direct3d9_OcclusionQuery not installed"));

	destroyBoxBuffers();
	destroyQueryPool();

	ms_freeQueries.clear();
	ms_pendingQueries.clear();
	ms_installed = false;
}

// ----------------------------------------------------------------------

void Direct3d9_OcclusionQuery::lostDevice()
{
	if (!ms_installed)
		return;

	// Release D3DPOOL_DEFAULT resources before device reset
	ms_pendingQueries.clear();
	destroyBoxBuffers();
	destroyQueryPool();
}

// ----------------------------------------------------------------------

void Direct3d9_OcclusionQuery::restoreDevice()
{
	if (!ms_installed || !ms_supported)
		return;

	// Recreate D3DPOOL_DEFAULT resources after device reset
	createQueryPool(cs_defaultMaxQueries);
	createBoxBuffers();
}

// ----------------------------------------------------------------------

bool Direct3d9_OcclusionQuery::isSupported()
{
	return ms_supported;
}

// ----------------------------------------------------------------------

int Direct3d9_OcclusionQuery::getMaxQueries()
{
	return ms_maxQueries;
}

// ----------------------------------------------------------------------

void Direct3d9_OcclusionQuery::beginFrame()
{
	++ms_currentFrame;
	collectResults();
}

// ----------------------------------------------------------------------

void Direct3d9_OcclusionQuery::endFrame()
{
	// Nothing needed here currently
}

// ----------------------------------------------------------------------

Direct3d9_OcclusionQuery::QueryHandle Direct3d9_OcclusionQuery::allocateQuery()
{
	if (!ms_supported || ms_freeQueries.empty())
		return InvalidQuery;

	int const handle = ms_freeQueries.back();
	ms_freeQueries.pop_back();

	QueryData & data = ms_queries[handle];
	data.inUse = true;
	data.state = QS_idle;
	data.visible = true;  // Default to visible
	data.pixelsDrawn = 0;
	data.frameIssued = -1;

	return handle;
}

// ----------------------------------------------------------------------

void Direct3d9_OcclusionQuery::releaseQuery(QueryHandle handle)
{
	if (handle < 0 || handle >= static_cast<int>(ms_queries.size()))
		return;

	QueryData & data = ms_queries[handle];
	if (!data.inUse)
		return;

	data.inUse = false;
	data.state = QS_idle;

	ms_freeQueries.push_back(handle);

	// Remove from pending list if present
	for (size_t i = 0; i < ms_pendingQueries.size(); ++i)
	{
		if (ms_pendingQueries[i] == handle)
		{
			ms_pendingQueries.erase(ms_pendingQueries.begin() + i);
			break;
		}
	}
}

// ----------------------------------------------------------------------

void Direct3d9_OcclusionQuery::releaseAllQueries()
{
	ms_freeQueries.clear();
	ms_pendingQueries.clear();

	for (size_t i = 0; i < ms_queries.size(); ++i)
	{
		ms_queries[i].inUse = false;
		ms_queries[i].state = QS_idle;
		ms_freeQueries.push_back(static_cast<int>(i));
	}
}

// ----------------------------------------------------------------------

bool Direct3d9_OcclusionQuery::beginQuery(QueryHandle handle)
{
	if (handle < 0 || handle >= static_cast<int>(ms_queries.size()))
		return false;

	QueryData & data = ms_queries[handle];
	if (!data.inUse || !data.query)
		return false;

	HRESULT hr = data.query->Issue(D3DISSUE_BEGIN);
	if (FAILED(hr))
	{
		data.state = QS_error;
		return false;
	}

	data.state = QS_pending;
	data.frameIssued = ms_currentFrame;

	return true;
}

// ----------------------------------------------------------------------

bool Direct3d9_OcclusionQuery::endQuery(QueryHandle handle)
{
	if (handle < 0 || handle >= static_cast<int>(ms_queries.size()))
		return false;

	QueryData & data = ms_queries[handle];
	if (!data.inUse || !data.query || data.state != QS_pending)
		return false;

	HRESULT hr = data.query->Issue(D3DISSUE_END);
	if (FAILED(hr))
	{
		data.state = QS_error;
		return false;
	}

	// Add to pending list for result collection
	ms_pendingQueries.push_back(handle);
	++ms_queriesIssued;

	return true;
}

// ----------------------------------------------------------------------

Direct3d9_OcclusionQuery::QueryState Direct3d9_OcclusionQuery::getQueryState(QueryHandle handle)
{
	if (handle < 0 || handle >= static_cast<int>(ms_queries.size()))
		return QS_error;

	return ms_queries[handle].state;
}

// ----------------------------------------------------------------------

bool Direct3d9_OcclusionQuery::getQueryResult(QueryHandle handle, QueryResult & outResult)
{
	if (handle < 0 || handle >= static_cast<int>(ms_queries.size()))
		return false;

	QueryData const & data = ms_queries[handle];

	outResult.handle = handle;
	outResult.state = data.state;
	outResult.visible = data.visible;
	outResult.pixelsDrawn = data.pixelsDrawn;
	outResult.frameIssued = data.frameIssued;

	return true;
}

// ----------------------------------------------------------------------

bool Direct3d9_OcclusionQuery::isVisible(QueryHandle handle)
{
	if (handle < 0 || handle >= static_cast<int>(ms_queries.size()))
		return true;  // Default to visible if invalid

	QueryData const & data = ms_queries[handle];

	// If query is still pending or too recent, assume visible
	if (data.state == QS_pending)
	{
		int const framesSinceIssue = ms_currentFrame - data.frameIssued;
		if (framesSinceIssue < ms_queryLatency)
			return true;  // Not enough frames have passed
	}

	return data.visible;
}

// ----------------------------------------------------------------------

bool Direct3d9_OcclusionQuery::renderBoundingBoxQuery(QueryHandle handle, AxialBox const & box)
{
	if (!beginQuery(handle))
		return false;

	renderBox(box);

	return endQuery(handle);
}

// ----------------------------------------------------------------------

bool Direct3d9_OcclusionQuery::renderBoundingSphereQuery(QueryHandle handle, Sphere const & sphere)
{
	// Render sphere as bounding box for simplicity
	// Construct box vertices directly to avoid AxialBox dependency
	Vector const center = sphere.getCenter();
	float  const r      = sphere.getRadius();
	Vector const minP(center.x - r, center.y - r, center.z - r);
	Vector const maxP(center.x + r, center.y + r, center.z + r);

	// Inline the renderBoundingBoxQuery logic with our min/max
	if (!ms_supported || !ms_installed)
		return false;

	if (handle < 0 || handle >= static_cast<int>(ms_queries.size()))
		return false;

	QueryData & data = ms_queries[handle];
	if (!data.query || data.state == QS_pending)
		return false;

	IDirect3DDevice9 * const device = Direct3d9::getDevice();
	if (!device || !ms_boxVertexBuffer || !ms_boxIndexBuffer)
		return false;

	// Begin query
	data.query->Issue(D3DISSUE_BEGIN);
	data.state = QS_pending;

	struct BoxVertex { float x, y, z; };

	BoxVertex vertices[8] =
	{
		{ minP.x, minP.y, minP.z },
		{ maxP.x, minP.y, minP.z },
		{ maxP.x, maxP.y, minP.z },
		{ minP.x, maxP.y, minP.z },
		{ minP.x, minP.y, maxP.z },
		{ maxP.x, minP.y, maxP.z },
		{ maxP.x, maxP.y, maxP.z },
		{ minP.x, maxP.y, maxP.z }
	};

	void * vertexData = 0;
	if (SUCCEEDED(ms_boxVertexBuffer->Lock(0, sizeof(vertices), &vertexData, D3DLOCK_DISCARD)))
	{
		memcpy(vertexData, vertices, sizeof(vertices));
		ms_boxVertexBuffer->Unlock();
	}

	DWORD oldColorWriteEnable, oldZWriteEnable;
	device->GetRenderState(D3DRS_COLORWRITEENABLE, &oldColorWriteEnable);
	device->GetRenderState(D3DRS_ZWRITEENABLE, &oldZWriteEnable);

	device->SetRenderState(D3DRS_COLORWRITEENABLE, 0);
	device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);

	device->SetStreamSource(0, ms_boxVertexBuffer, 0, sizeof(BoxVertex));
	device->SetIndices(ms_boxIndexBuffer);
	device->SetFVF(D3DFVF_XYZ);
	device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 8, 0, 12);

	device->SetRenderState(D3DRS_COLORWRITEENABLE, oldColorWriteEnable);
	device->SetRenderState(D3DRS_ZWRITEENABLE, oldZWriteEnable);

	// End query
	data.query->Issue(D3DISSUE_END);
	ms_pendingQueries.push_back(handle);
	++ms_queriesIssued;

	return true;
}

// ----------------------------------------------------------------------

void Direct3d9_OcclusionQuery::collectResults()
{
	// Collect results from pending queries
	for (size_t i = 0; i < ms_pendingQueries.size(); )
	{
		int const handle = ms_pendingQueries[i];
		QueryData & data = ms_queries[handle];

		if (!data.query)
		{
			ms_pendingQueries.erase(ms_pendingQueries.begin() + i);
			continue;
		}

		// Check if result is available (non-blocking)
		DWORD pixelsDrawn = 0;
		HRESULT hr = data.query->GetData(&pixelsDrawn, sizeof(DWORD), D3DGETDATA_FLUSH);

		if (hr == S_OK)
		{
			// Result ready
			data.pixelsDrawn = pixelsDrawn;
			data.visible = (pixelsDrawn > 0);
			data.state = QS_ready;

			++ms_queriesSucceeded;
			if (!data.visible)
				++ms_objectsCulled;

			ms_pendingQueries.erase(ms_pendingQueries.begin() + i);
		}
		else if (hr == S_FALSE)
		{
			// Result not ready yet
			++i;
		}
		else
		{
			// Error
			data.state = QS_error;
			data.visible = true;  // Default to visible on error
			ms_pendingQueries.erase(ms_pendingQueries.begin() + i);
		}
	}
}

// ----------------------------------------------------------------------

void Direct3d9_OcclusionQuery::renderBox(AxialBox const & box)
{
	IDirect3DDevice9 * const device = Direct3d9::getDevice();
	if (!device || !ms_boxVertexBuffer || !ms_boxIndexBuffer)
		return;

	// Update box vertex positions
	Vector const & minP = box.getMin();
	Vector const & maxP = box.getMax();

	struct BoxVertex { float x, y, z; };

	BoxVertex vertices[8] =
	{
		{ minP.x, minP.y, minP.z },  // 0: min corner
		{ maxP.x, minP.y, minP.z },  // 1
		{ maxP.x, maxP.y, minP.z },  // 2
		{ minP.x, maxP.y, minP.z },  // 3
		{ minP.x, minP.y, maxP.z },  // 4
		{ maxP.x, minP.y, maxP.z },  // 5
		{ maxP.x, maxP.y, maxP.z },  // 6: max corner
		{ minP.x, maxP.y, maxP.z }   // 7
	};

	void * data = nullptr;
	if (SUCCEEDED(ms_boxVertexBuffer->Lock(0, sizeof(vertices), &data, D3DLOCK_DISCARD)))
	{
		memcpy(data, vertices, sizeof(vertices));
		ms_boxVertexBuffer->Unlock();
	}

	// Disable color writes and z-writes for occlusion query
	// We only care about depth testing
	DWORD oldColorWriteEnable, oldZWriteEnable;
	device->GetRenderState(D3DRS_COLORWRITEENABLE, &oldColorWriteEnable);
	device->GetRenderState(D3DRS_ZWRITEENABLE, &oldZWriteEnable);

	device->SetRenderState(D3DRS_COLORWRITEENABLE, 0);
	device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);

	// Draw the box
	device->SetStreamSource(0, ms_boxVertexBuffer, 0, sizeof(BoxVertex));
	device->SetIndices(ms_boxIndexBuffer);
	device->SetFVF(D3DFVF_XYZ);
	device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 8, 0, 12);

	// Restore render states
	device->SetRenderState(D3DRS_COLORWRITEENABLE, oldColorWriteEnable);
	device->SetRenderState(D3DRS_ZWRITEENABLE, oldZWriteEnable);
}

// ----------------------------------------------------------------------

int Direct3d9_OcclusionQuery::getQueriesIssued()
{
	return ms_queriesIssued;
}

// ----------------------------------------------------------------------

int Direct3d9_OcclusionQuery::getQueriesSucceeded()
{
	return ms_queriesSucceeded;
}

// ----------------------------------------------------------------------

int Direct3d9_OcclusionQuery::getObjectsCulled()
{
	return ms_objectsCulled;
}

// ----------------------------------------------------------------------

void Direct3d9_OcclusionQuery::resetStatistics()
{
	ms_queriesIssued = 0;
	ms_queriesSucceeded = 0;
	ms_objectsCulled = 0;
}

// ----------------------------------------------------------------------

void Direct3d9_OcclusionQuery::setQueryLatency(int frames)
{
	ms_queryLatency = (frames < 1) ? 1 : frames;
}

// ----------------------------------------------------------------------

int Direct3d9_OcclusionQuery::getQueryLatency()
{
	return ms_queryLatency;
}

// ----------------------------------------------------------------------

namespace Direct3d9_OcclusionQueryNamespace
{
	bool createQueryPool(int maxQueries)
	{
		IDirect3DDevice9 * const device = Direct3d9::getDevice();
		if (!device)
			return false;

		ms_queries.resize(maxQueries);
		ms_freeQueries.clear();
		ms_freeQueries.reserve(maxQueries);

		for (int i = 0; i < maxQueries; ++i)
		{
			HRESULT hr = device->CreateQuery(D3DQUERYTYPE_OCCLUSION, &ms_queries[i].query);
			if (FAILED(hr))
			{
				DEBUG_WARNING(true, ("Failed to create occlusion query %d: 0x%08X", i, hr));
				ms_queries[i].query = nullptr;
			}
			else
			{
				ms_freeQueries.push_back(i);
			}
		}

		return !ms_freeQueries.empty();
	}

	void destroyQueryPool()
	{
		for (size_t i = 0; i < ms_queries.size(); ++i)
		{
			if (ms_queries[i].query)
			{
				ms_queries[i].query->Release();
				ms_queries[i].query = nullptr;
			}
		}
		ms_queries.clear();
	}

	bool createBoxBuffers()
	{
		IDirect3DDevice9 * const device = Direct3d9::getDevice();
		if (!device)
			return false;

		// Create vertex buffer for 8 box corners
		HRESULT hr = device->CreateVertexBuffer(
			8 * sizeof(float) * 3,
			D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY,
			D3DFVF_XYZ,
			D3DPOOL_DEFAULT,
			&ms_boxVertexBuffer,
			nullptr
		);

		if (FAILED(hr))
			return false;

		// Create index buffer for box faces (12 triangles = 36 indices)
		hr = device->CreateIndexBuffer(
			36 * sizeof(WORD),
			D3DUSAGE_WRITEONLY,
			D3DFMT_INDEX16,
			D3DPOOL_DEFAULT,
			&ms_boxIndexBuffer,
			nullptr
		);

		if (FAILED(hr))
		{
			ms_boxVertexBuffer->Release();
			ms_boxVertexBuffer = nullptr;
			return false;
		}

		// Fill index buffer
		WORD indices[36] =
		{
			// Front face
			0, 1, 2, 0, 2, 3,
			// Back face
			4, 6, 5, 4, 7, 6,
			// Left face
			0, 3, 7, 0, 7, 4,
			// Right face
			1, 5, 6, 1, 6, 2,
			// Top face
			3, 2, 6, 3, 6, 7,
			// Bottom face
			0, 4, 5, 0, 5, 1
		};

		void * data = nullptr;
		if (SUCCEEDED(ms_boxIndexBuffer->Lock(0, sizeof(indices), &data, 0)))
		{
			memcpy(data, indices, sizeof(indices));
			ms_boxIndexBuffer->Unlock();
		}

		return true;
	}

	void destroyBoxBuffers()
	{
		if (ms_boxVertexBuffer)
		{
			ms_boxVertexBuffer->Release();
			ms_boxVertexBuffer = nullptr;
		}

		if (ms_boxIndexBuffer)
		{
			ms_boxIndexBuffer->Release();
			ms_boxIndexBuffer = nullptr;
		}
	}
}

// ======================================================================



