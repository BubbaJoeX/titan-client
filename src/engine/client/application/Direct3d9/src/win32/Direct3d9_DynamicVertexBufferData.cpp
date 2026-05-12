// ======================================================================
//
// Direct3d9_DynamicVertexBufferData.cpp
// copyright 2001 Sony Online Entertainment
//
// ======================================================================

#include "FirstDirect3d9.h"
#include "Direct3d9_DynamicVertexBufferData.h"

#include "ConfigDirect3d9.h"
#include "Direct3d9.h"
#include "Direct3d9_Metrics.h"
#include "Direct3d9_VertexBufferDescriptorMap.h"
#include "Direct3d9_VertexDeclarationMap.h"

#include "clientGraphics/VertexBuffer.h"
#include "sharedFoundation/MemoryBlockManager.h"

// ======================================================================

bool                     Direct3d9_DynamicVertexBufferData::ms_newFrame;
int                      Direct3d9_DynamicVertexBufferData::ms_size;
int                      Direct3d9_DynamicVertexBufferData::ms_used[] = { 0, 0 };
int                      Direct3d9_DynamicVertexBufferData::ms_activeBufferIndex = 0;
IDirect3DVertexBuffer9 * Direct3d9_DynamicVertexBufferData::ms_d3dVertexBuffers[] = { NULL, NULL };
MemoryBlockManager *     Direct3d9_DynamicVertexBufferData::ms_memoryBlockManager;
int                      Direct3d9_DynamicVertexBufferData::ms_locksSinceBeginFrame;
int                      Direct3d9_DynamicVertexBufferData::ms_discardsSinceBeginFrame;
int                      Direct3d9_DynamicVertexBufferData::ms_locksSinceResourceCreation;
int                      Direct3d9_DynamicVertexBufferData::ms_discardsSinceResourceCreation;
int                      Direct3d9_DynamicVertexBufferData::ms_locksEver;
int                      Direct3d9_DynamicVertexBufferData::ms_discardsEver;

// ======================================================================

void Direct3d9_DynamicVertexBufferData::install()
{
	DEBUG_FATAL(ms_memoryBlockManager, ("Already installed"));
	ms_memoryBlockManager  = new MemoryBlockManager("Direct3d9_DynamicVertexBufferData", true, sizeof(Direct3d9_DynamicVertexBufferData), 0, 0, 0);

	bool ffp = !Direct3d9::supportsVertexShaders();
	if (ffp)
	{
		ms_size=256*1024;
	}
	else
	{
		int videoMemory = Direct3d9::getVideoMemoryInMegabytes();
		if (videoMemory<=16)
		{
			ms_size=256*1024;
		}
		else if (videoMemory<=32)
		{
			ms_size=512*1024;
		}
		else if (videoMemory<=64)
		{
			ms_size=1024*1024;
		}
		else
		{
			ms_size=2048*1024;
		}
	}
	//ms_size = ConfigDirect3d9::getDynamicVertexBufferSize() * 1024;

	ms_used[0] = 0;
	ms_used[1] = 0;
	ms_activeBufferIndex = 0;
	restoreDevice();
}

// ----------------------------------------------------------------------

void Direct3d9_DynamicVertexBufferData::remove()
{
	lostDevice();

	delete ms_memoryBlockManager;
	ms_memoryBlockManager = NULL;
}

// ----------------------------------------------------------------------

void Direct3d9_DynamicVertexBufferData::beginFrame()
{
	ms_newFrame = ConfigDirect3d9::getDiscardDynamicBuffersAtBeginningOfFrame();
	ms_locksSinceBeginFrame = 0;
	ms_discardsSinceBeginFrame = 0;

	// Ping-pong buffers so the GPU can finish consuming last frame's uploads while the CPU fills the other buffer.
	ms_activeBufferIndex = (ms_activeBufferIndex + 1) & 1;
	ms_used[ms_activeBufferIndex] = 0;
	ms_newFrame = true;
}

// ----------------------------------------------------------------------

void *Direct3d9_DynamicVertexBufferData::operator new(size_t size)
{
	UNREF(size);
	NOT_NULL(ms_memoryBlockManager);
	DEBUG_FATAL(size != sizeof(Direct3d9_DynamicVertexBufferData), ("wrong new called"));
	DEBUG_FATAL(size != static_cast<size_t> (ms_memoryBlockManager->getElementSize()), ("installed with bad size"));

	return ms_memoryBlockManager->allocate();
}

// ----------------------------------------------------------------------

void Direct3d9_DynamicVertexBufferData::operator delete(void *memory)
{
	NOT_NULL(ms_memoryBlockManager);
	ms_memoryBlockManager->free(memory);
}

// ----------------------------------------------------------------------

void Direct3d9_DynamicVertexBufferData::lostDevice()
{
	for (int b = 0; b < 2; ++b)
	{
		if (ms_d3dVertexBuffers[b])
		{
			IGNORE_RETURN(ms_d3dVertexBuffers[b]->Release());
			ms_d3dVertexBuffers[b] = NULL;
		}
	}
	ms_locksSinceResourceCreation = 0;
	ms_discardsSinceResourceCreation = 0;
}

// ----------------------------------------------------------------------

void Direct3d9_DynamicVertexBufferData::restoreDevice()
{
	ms_newFrame = ConfigDirect3d9::getDiscardDynamicBuffersAtBeginningOfFrame();
	IDirect3DDevice9 *device = Direct3d9::getDevice();
	for (int b = 0; b < 2; ++b)
	{
		HRESULT const hresult = device->CreateVertexBuffer(ms_size, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &ms_d3dVertexBuffers[b], NULL);
		FATAL_DX_HR("Could not create dynamic VB %s", hresult);
	}
	ms_used[0] = 0;
	ms_used[1] = 0;
	ms_activeBufferIndex = 0;
	ms_locksSinceResourceCreation = 0;
	ms_discardsSinceResourceCreation = 0;
}

// ======================================================================

Direct3d9_DynamicVertexBufferData::Direct3d9_DynamicVertexBufferData(const VertexBuffer &vertexBuffer)
:
	m_vertexBufferDescriptor(Direct3d9_VertexBufferDescriptorMap::getDescriptor(vertexBuffer.getFormat())),
	m_numberOfVertices(0),
	m_offset(0),
	m_vertexDeclaration(Direct3d9_VertexDeclarationMap::fetchVertexDeclaration(vertexBuffer.getFormat()))
{
}

// ----------------------------------------------------------------------

Direct3d9_DynamicVertexBufferData::~Direct3d9_DynamicVertexBufferData()
{
	m_vertexDeclaration->Release();
}

// ----------------------------------------------------------------------

const VertexBufferDescriptor &Direct3d9_DynamicVertexBufferData::getDescriptor() const
{
	return m_vertexBufferDescriptor;
}

// ----------------------------------------------------------------------

void Direct3d9_DynamicVertexBufferData::roundUpUsed() const
{
	const int vertexSize = getVertexSize();
	int & used = ms_used[ms_activeBufferIndex];
	used = ((used + vertexSize - 1) / vertexSize) * vertexSize;
}

// ----------------------------------------------------------------------

void *Direct3d9_DynamicVertexBufferData::lock(int numberOfVertices, bool forceDiscard)
{
	roundUpUsed();

	const int vertexSize = getVertexSize();
	const int length = numberOfVertices * vertexSize;
	int & used = ms_used[ms_activeBufferIndex];
	IDirect3DVertexBuffer9 *const activeBuffer = ms_d3dVertexBuffers[ms_activeBufferIndex];

	++ms_locksSinceBeginFrame;
	++ms_locksSinceResourceCreation;
	++ms_locksEver;

	// check for space
	DWORD lockFlag = D3DLOCK_NOOVERWRITE;
	int discard = 0;
	if (ms_newFrame || forceDiscard || used + length > ms_size)
	{
		ms_newFrame = false;
		++ms_discardsSinceBeginFrame;
		++ms_discardsSinceResourceCreation;
		++ms_discardsEver;
		discard = 1;

		// make sure this VB will fit even when the dynamic vb is empty
		DEBUG_FATAL(length > ms_size, ("Too many vertices %d/%d", numberOfVertices, ms_size / getVertexSize()));

#ifdef _DEBUG
		Direct3d9_Metrics::vertexBufferDiscards += 1;
#endif

		// lock with discard contents
		lockFlag = D3DLOCK_DISCARD;
		used = 0;
	}

	void *data = NULL;
	HRESULT const hresult = activeBuffer->Lock(used, length, &data, lockFlag);
	FATAL(FAILED(hresult), ("Could not lock dynamic %s %d=err %d=discard %d=offset %d=length %d/%d/%d=locks %d/%d/%d=discards", "vb", HRESULT_CODE(hresult), discard, used, length, ms_locksSinceBeginFrame, ms_locksSinceResourceCreation, ms_locksEver, ms_discardsSinceBeginFrame, ms_discardsSinceResourceCreation, ms_discardsEver));
	NOT_NULL(data);

	// handle bad NVidia drivers
	if (IsBadWritePtr(data, 1) != 0)
		data = Direct3d9::getTemporaryBuffer(length);

	// use up vertices from this dynamic vb
	m_numberOfVertices = numberOfVertices;
	m_offset = used / vertexSize;

	return data;
}

// ----------------------------------------------------------------------

void Direct3d9_DynamicVertexBufferData::unlock()
{
	Direct3d9_DynamicVertexBufferData::unlock(m_numberOfVertices);
}

// ----------------------------------------------------------------------

void Direct3d9_DynamicVertexBufferData::unlock(int numberOfVertices)
{
	IDirect3DVertexBuffer9 *const activeBuffer = ms_d3dVertexBuffers[ms_activeBufferIndex];
	const HRESULT hresult = activeBuffer->Unlock();
	FATAL_DX_HR("Could not unlock vb %s", hresult);

	m_numberOfVertices = numberOfVertices;
	const int vertexSize = getVertexSize();
	const int length = numberOfVertices * vertexSize;

#ifdef _DEBUG
	Direct3d9_Metrics::vertexBufferMemoryDynamic += length;
#endif

	ms_used[ms_activeBufferIndex] += length;
}

// ------------------------------------------------------------------	----

int Direct3d9_DynamicVertexBufferData::getNumberOfLockableDynamicVertices(bool withDiscard)
{
	roundUpUsed();

	int const used = ms_used[ms_activeBufferIndex];
	// with a discard, they can get all the vertices
	// without discard, they can only get access to the remaining ones
	return (ms_size - (withDiscard ? 0 : used)) / getVertexSize();
}

// ----------------------------------------------------------------------

int Direct3d9_DynamicVertexBufferData::getSortKey()
{
	return reinterpret_cast<int>(ms_d3dVertexBuffers[ms_activeBufferIndex]);
}

// ----------------------------------------------------------------------

int Direct3d9_DynamicVertexBufferData::getVertexSize() const
{
	return m_vertexBufferDescriptor.vertexSize;
}

// ----------------------------------------------------------------------

IDirect3DVertexBuffer9 *Direct3d9_DynamicVertexBufferData::getVertexBuffer() const
{
	return ms_d3dVertexBuffers[ms_activeBufferIndex];
}

// ======================================================================

