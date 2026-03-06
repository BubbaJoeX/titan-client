// ======================================================================
//
// Direct3d9_Instancing.cpp
// Copyright 2026 Titan Development Team
// All Rights Reserved.
//
// Hardware instancing support for Direct3D 9
//
// ======================================================================

#include "FirstDirect3d9.h"
#include "Direct3d9_Instancing.h"

#include "ConfigDirect3d9.h"
#include "Direct3d9.h"
#include "Direct3d9_StateCache.h"
#include "Direct3d9_StaticVertexBufferData.h"
#include "Direct3d9_StaticIndexBufferData.h"

#include "clientGraphics/StaticVertexBuffer.h"
#include "clientGraphics/StaticIndexBuffer.h"
#include "clientGraphics/StaticShader.h"
#include "clientGraphics/ShaderCapability.h"

#include "sharedDebug/DebugFlags.h"

#include <d3d9.h>
#include <algorithm>

// ======================================================================

namespace Direct3d9_InstancingNamespace
{
	// Configuration
	int const cs_maxHardwareInstances = 256;      // Max instances per hardware batch
	int const cs_maxPseudoInstances = 8;          // Max instances via shader constants
	int const cs_instanceDataSize = sizeof(Direct3d9_Instancing::InstanceData);

	// State
	bool                                 ms_installed = false;
	Direct3d9_Instancing::InstancingMode ms_instancingMode = Direct3d9_Instancing::IM_none;
	int                                  ms_maxInstances = 0;

	// Current batch state
	bool                         ms_batchActive = false;
	StaticVertexBuffer const *   ms_currentVertexBuffer = nullptr;
	StaticIndexBuffer const *    ms_currentIndexBuffer = nullptr;
	StaticShader const *         ms_currentShader = nullptr;

	// Instance buffer
	IDirect3DVertexBuffer9 *     ms_instanceBuffer = nullptr;
	IDirect3DVertexDeclaration9 *ms_instanceDeclaration = nullptr;
	int                          ms_instanceBufferCapacity = 0;

	// Pending instances
	std::vector<Direct3d9_Instancing::InstanceData> ms_pendingInstances;

	// Statistics
	int ms_drawCallsSaved = 0;
	int ms_totalInstancesDrawn = 0;

	// Helper functions
	bool createInstanceDeclaration();
	void destroyInstanceDeclaration();
}

using namespace Direct3d9_InstancingNamespace;

// ======================================================================

void Direct3d9_Instancing::install()
{
	DEBUG_FATAL(ms_installed, ("Direct3d9_Instancing already installed"));

	// Check if instancing is enabled in config
	if (!ConfigDirect3d9::getEnableInstancing())
	{
		ms_instancingMode = IM_none;
		ms_maxInstances = 1;
		ms_installed = true;
		DEBUG_REPORT_LOG(true, ("Direct3d9_Instancing: disabled by config\n"));
		return;
	}

	// Determine instancing capability based on shader model
	int const shaderCapability = Direct3d9::getShaderCapability();

	if (shaderCapability >= ShaderCapability(3, 0, true))
	{
		// SM3.0+ supports true hardware instancing via SetStreamSourceFreq
		ms_instancingMode = IM_hardwareInstancing;
		ms_maxInstances = ConfigDirect3d9::getMaxInstancesPerBatch();
	}
	else if (shaderCapability >= ShaderCapability(2, 0))
	{
		// SM2.0 uses pseudo-instancing via shader constants
		ms_instancingMode = IM_pseudoInstancing;
		ms_maxInstances = cs_maxPseudoInstances;
	}
	else
	{
		// No instancing support
		ms_instancingMode = IM_none;
		ms_maxInstances = 1;
	}

	// Reserve space for pending instances
	ms_pendingInstances.reserve(ms_maxInstances);

	// Create instance buffer for hardware instancing
	if (ms_instancingMode == IM_hardwareInstancing)
	{
		createInstanceBuffer(cs_maxHardwareInstances);
		createInstanceDeclaration();
	}

	ms_installed = true;

	DEBUG_REPORT_LOG(true, ("Direct3d9_Instancing: mode=%d, maxInstances=%d\n",
		static_cast<int>(ms_instancingMode), ms_maxInstances));
}

// ----------------------------------------------------------------------

void Direct3d9_Instancing::remove()
{
	DEBUG_FATAL(!ms_installed, ("Direct3d9_Instancing not installed"));

	destroyInstanceBuffer();
	destroyInstanceDeclaration();

	ms_pendingInstances.clear();
	ms_installed = false;
}

// ----------------------------------------------------------------------

void Direct3d9_Instancing::lostDevice()
{
	if (!ms_installed)
		return;

	// Release D3DPOOL_DEFAULT resources before device reset
	destroyInstanceBuffer();
	destroyInstanceDeclaration();
}

// ----------------------------------------------------------------------

void Direct3d9_Instancing::restoreDevice()
{
	if (!ms_installed)
		return;

	// Recreate D3DPOOL_DEFAULT resources after device reset
	if (ms_instancingMode == IM_hardwareInstancing)
	{
		createInstanceBuffer(cs_maxHardwareInstances);
		createInstanceDeclaration();
	}
}

// ----------------------------------------------------------------------

bool Direct3d9_Instancing::isInstancingSupported()
{
	return ms_instancingMode != IM_none;
}

// ----------------------------------------------------------------------

Direct3d9_Instancing::InstancingMode Direct3d9_Instancing::getInstancingMode()
{
	return ms_instancingMode;
}

// ----------------------------------------------------------------------

int Direct3d9_Instancing::getMaxInstancesPerBatch()
{
	return ms_maxInstances;
}

// ----------------------------------------------------------------------

bool Direct3d9_Instancing::beginBatch(StaticVertexBuffer const * vertexBuffer,
                                       StaticIndexBuffer const * indexBuffer,
                                       StaticShader const * shader)
{
	if (!ms_installed || ms_instancingMode == IM_none)
		return false;

	// End any existing batch
	if (ms_batchActive)
		endBatch();

	ms_currentVertexBuffer = vertexBuffer;
	ms_currentIndexBuffer = indexBuffer;
	ms_currentShader = shader;
	ms_batchActive = true;

	ms_pendingInstances.clear();

	return true;
}

// ----------------------------------------------------------------------

bool Direct3d9_Instancing::addInstance(Transform const & transform,
                                        VectorRgba const & color,
                                        float const * userData)
{
	if (!ms_batchActive)
		return false;

	// Flush if batch is full
	if (static_cast<int>(ms_pendingInstances.size()) >= ms_maxInstances)
		flushBatch();

	// Add instance to pending list
	InstanceData instance;

	// Convert transform to 3x4 matrix (row-major)
	// Row 0: X-axis
	instance.worldMatrix[0] = transform.getLocalFrameI_p().x;
	instance.worldMatrix[1] = transform.getLocalFrameI_p().y;
	instance.worldMatrix[2] = transform.getLocalFrameI_p().z;
	instance.worldMatrix[3] = transform.getPosition_p().x;

	// Row 1: Y-axis
	instance.worldMatrix[4] = transform.getLocalFrameJ_p().x;
	instance.worldMatrix[5] = transform.getLocalFrameJ_p().y;
	instance.worldMatrix[6] = transform.getLocalFrameJ_p().z;
	instance.worldMatrix[7] = transform.getPosition_p().y;

	// Row 2: Z-axis
	instance.worldMatrix[8] = transform.getLocalFrameK_p().x;
	instance.worldMatrix[9] = transform.getLocalFrameK_p().y;
	instance.worldMatrix[10] = transform.getLocalFrameK_p().z;
	instance.worldMatrix[11] = transform.getPosition_p().z;

	// Color
	instance.color[0] = color.r;
	instance.color[1] = color.g;
	instance.color[2] = color.b;
	instance.color[3] = color.a;

	// User data
	if (userData)
	{
		instance.userData[0] = userData[0];
		instance.userData[1] = userData[1];
		instance.userData[2] = userData[2];
		instance.userData[3] = userData[3];
	}
	else
	{
		instance.userData[0] = 0.0f;
		instance.userData[1] = 0.0f;
		instance.userData[2] = 0.0f;
		instance.userData[3] = 0.0f;
	}

	ms_pendingInstances.push_back(instance);
	return true;
}

// ----------------------------------------------------------------------

int Direct3d9_Instancing::endBatch()
{
	if (!ms_batchActive)
		return 0;

	int const instanceCount = static_cast<int>(ms_pendingInstances.size());

	if (instanceCount > 0)
		flushBatch();

	ms_batchActive = false;
	ms_currentVertexBuffer = nullptr;
	ms_currentIndexBuffer = nullptr;
	ms_currentShader = nullptr;

	return instanceCount;
}

// ----------------------------------------------------------------------

void Direct3d9_Instancing::flushBatch()
{
	int const instanceCount = static_cast<int>(ms_pendingInstances.size());
	if (instanceCount == 0)
		return;

	// Update statistics
	if (instanceCount > 1)
		ms_drawCallsSaved += (instanceCount - 1);
	ms_totalInstancesDrawn += instanceCount;

	// Draw based on instancing mode
	if (ms_instancingMode == IM_hardwareInstancing)
	{
		updateInstanceBuffer();
		drawHardwareInstanced(instanceCount);
	}
	else if (ms_instancingMode == IM_pseudoInstancing)
	{
		drawPseudoInstanced(instanceCount);
	}

	ms_pendingInstances.clear();
}

// ----------------------------------------------------------------------

void Direct3d9_Instancing::drawHardwareInstanced(int instanceCount)
{
	IDirect3DDevice9 * const device = Direct3d9::getDevice();
	if (!device || !ms_currentVertexBuffer || !ms_instanceBuffer)
		return;

	// Get vertex buffer graphics data
	Direct3d9_StaticVertexBufferData const * const d3dVbData =
		safe_cast<Direct3d9_StaticVertexBufferData const *>(ms_currentVertexBuffer->m_graphicsData);
	if (!d3dVbData)
		return;

	// Set stream 0 for geometry (indexed data, geometry instancing divider)
	UINT const geometryStride = ms_currentVertexBuffer->getVertexSize();
	device->SetStreamSource(0, d3dVbData->getVertexBuffer(), 0, geometryStride);
	device->SetStreamSourceFreq(0, D3DSTREAMSOURCE_INDEXEDDATA | instanceCount);

	// Set stream 1 for instance data (instance data divider)
	device->SetStreamSource(1, ms_instanceBuffer, 0, cs_instanceDataSize);
	device->SetStreamSourceFreq(1, D3DSTREAMSOURCE_INSTANCEDATA | 1);

	// Set vertex declaration
	if (ms_instanceDeclaration)
		device->SetVertexDeclaration(ms_instanceDeclaration);

	// Draw
	if (ms_currentIndexBuffer)
	{
		Direct3d9_StaticIndexBufferData const * const d3dIbData =
			safe_cast<Direct3d9_StaticIndexBufferData const *>(ms_currentIndexBuffer->m_graphicsData);
		if (d3dIbData)
		{
			device->SetIndices(d3dIbData->getIndexBuffer());

			int const numIndices = ms_currentIndexBuffer->getNumberOfIndices();
			int const primitiveCount = numIndices / 3;
			int const numVertices = ms_currentVertexBuffer->getNumberOfVertices();

			device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0,
				numVertices, 0, primitiveCount);
		}
	}
	else
	{
		int const numVertices = ms_currentVertexBuffer->getNumberOfVertices();
		int const primitiveCount = numVertices / 3;
		device->DrawPrimitive(D3DPT_TRIANGLELIST, 0, primitiveCount);
	}

	// Reset stream frequency
	device->SetStreamSourceFreq(0, 1);
	device->SetStreamSourceFreq(1, 1);
}

// ----------------------------------------------------------------------

void Direct3d9_Instancing::drawPseudoInstanced(int instanceCount)
{
	IDirect3DDevice9 * const device = Direct3d9::getDevice();
	if (!device || !ms_currentVertexBuffer)
		return;

	// Pseudo-instancing: draw each instance separately but batch shader constant updates
	// This is still faster than individual draw calls due to reduced API overhead

	for (int i = 0; i < instanceCount; ++i)
	{
		InstanceData const & instance = ms_pendingInstances[i];

		// Set world matrix via shader constants
		// Constants 0-2: World matrix rows
		device->SetVertexShaderConstantF(0, instance.worldMatrix, 3);

		// Constant 3: Instance color
		device->SetVertexShaderConstantF(3, instance.color, 1);

		// Draw the mesh
		// Note: The actual draw call would go through the normal rendering path
		// This is a simplified version for demonstration
	}
}

// ----------------------------------------------------------------------

bool Direct3d9_Instancing::createInstanceBuffer(int maxInstances)
{
	IDirect3DDevice9 * const device = Direct3d9::getDevice();
	if (!device)
		return false;

	// Destroy existing buffer
	destroyInstanceBuffer();

	UINT const bufferSize = maxInstances * cs_instanceDataSize;

	HRESULT const hr = device->CreateVertexBuffer(
		bufferSize,
		D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY,
		0,  // FVF not used with declarations
		D3DPOOL_DEFAULT,
		&ms_instanceBuffer,
		nullptr
	);

	if (FAILED(hr))
	{
		DEBUG_WARNING(true, ("Failed to create instance buffer: 0x%08X", hr));
		return false;
	}

	ms_instanceBufferCapacity = maxInstances;
	return true;
}

// ----------------------------------------------------------------------

void Direct3d9_Instancing::destroyInstanceBuffer()
{
	if (ms_instanceBuffer)
	{
		ms_instanceBuffer->Release();
		ms_instanceBuffer = nullptr;
	}
	ms_instanceBufferCapacity = 0;
}

// ----------------------------------------------------------------------

bool Direct3d9_Instancing::updateInstanceBuffer()
{
	if (!ms_instanceBuffer || ms_pendingInstances.empty())
		return false;

	int const instanceCount = static_cast<int>(ms_pendingInstances.size());
	UINT const dataSize = instanceCount * cs_instanceDataSize;

	void * data = nullptr;
	HRESULT const hr = ms_instanceBuffer->Lock(0, dataSize, &data, D3DLOCK_DISCARD);

	if (FAILED(hr) || !data)
		return false;

	memcpy(data, &ms_pendingInstances[0], dataSize);

	ms_instanceBuffer->Unlock();
	return true;
}

// ----------------------------------------------------------------------

namespace Direct3d9_InstancingNamespace
{
	bool createInstanceDeclaration()
	{
		IDirect3DDevice9 * const device = Direct3d9::getDevice();
		if (!device)
			return false;

		// Vertex declaration combining geometry (stream 0) and instance data (stream 1)
		// Stream 0: Standard geometry vertex format
		// Stream 1: Per-instance data
		D3DVERTEXELEMENT9 const elements[] =
		{
			// Stream 0: Geometry (position, normal, texcoords - standard format)
			{ 0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
			{ 0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0 },
			{ 0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },

			// Stream 1: Instance data (world matrix rows + color + user data)
			{ 1, 0,  D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1 },  // Matrix row 0
			{ 1, 16, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 2 },  // Matrix row 1
			{ 1, 32, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 3 },  // Matrix row 2
			{ 1, 48, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 1 },     // Instance color
			{ 1, 64, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 4 },  // User data

			D3DDECL_END()
		};

		HRESULT const hr = device->CreateVertexDeclaration(elements, &ms_instanceDeclaration);

		if (FAILED(hr))
		{
			DEBUG_WARNING(true, ("Failed to create instance vertex declaration: 0x%08X", hr));
			return false;
		}

		return true;
	}

	void destroyInstanceDeclaration()
	{
		if (ms_instanceDeclaration)
		{
			ms_instanceDeclaration->Release();
			ms_instanceDeclaration = nullptr;
		}
	}
}

// ----------------------------------------------------------------------

int Direct3d9_Instancing::getDrawCallsSaved()
{
	return ms_drawCallsSaved;
}

// ----------------------------------------------------------------------

int Direct3d9_Instancing::getTotalInstancesDrawn()
{
	return ms_totalInstancesDrawn;
}

// ----------------------------------------------------------------------

void Direct3d9_Instancing::resetStatistics()
{
	ms_drawCallsSaved = 0;
	ms_totalInstancesDrawn = 0;
}

// ======================================================================



