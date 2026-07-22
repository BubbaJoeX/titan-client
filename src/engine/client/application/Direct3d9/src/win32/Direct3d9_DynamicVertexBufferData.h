// ======================================================================
//
// Direct3d9_DynamicVertexBufferData.h
// Copyright 2001 Sony Online Entertainment Inc.
// All Rights Reserved.
//
// ======================================================================

#ifndef INCLUDED_Direct3d9_DynamicVertexBufferData_H
#define INCLUDED_Direct3d9_DynamicVertexBufferData_H

// ======================================================================

struct IDirect3DVertexBuffer9;
struct IDirect3DVertexDeclaration9;
class  MemoryBlockManager;

#include "clientGraphics/DynamicVertexBuffer.h"
#include "Direct3d9_VertexBufferDescriptorMap.h"

// ======================================================================

class Direct3d9_DynamicVertexBufferData: public DynamicVertexBufferGraphicsData
{
public:

	static void install();
	static void remove();
	static void beginFrame();
	static void lostDevice();
	static void restoreDevice();

	static void *operator new(size_t size);
	static void  operator delete(void *memory);

public:

	explicit Direct3d9_DynamicVertexBufferData(const VertexBuffer &vertexBuffer);
	virtual ~Direct3d9_DynamicVertexBufferData();

	virtual void                            *lock(int numberOfVertices, bool forceDiscard);
	virtual void                             unlock();
	virtual void                             unlock(int numberOfVertices);
	virtual const VertexBufferDescriptor    &getDescriptor() const;
	virtual int                              getNumberOfLockableDynamicVertices(bool withDiscard);
	virtual int                              getSortKey();

	int                          getNumberOfVertices() const;
	IDirect3DVertexBuffer9      *getVertexBuffer() const;
	int                          getVertexSize() const;
	IDirect3DVertexDeclaration9 *getVertexDeclaration() const;
	int                          getOffset() const;

private:

	void roundUpUsed() const;

private:

	// Disabled.
	Direct3d9_DynamicVertexBufferData(void);
	Direct3d9_DynamicVertexBufferData(const Direct3d9_DynamicVertexBufferData &);
	Direct3d9_DynamicVertexBufferData &operator =(const Direct3d9_DynamicVertexBufferData &);

private:

	enum Ring
	{
		R_world    = DynamicVertexBuffer::DR_world,
		R_ui       = DynamicVertexBuffer::DR_ui,
		R_skeletal = DynamicVertexBuffer::DR_skeletal,
		R_count    = DynamicVertexBuffer::DR_count,
		B_count    = 2
	};

private:

	static bool                    ms_newFrame[R_count];
	static int                     ms_size;
	static int                     ms_activeBufferIndex[R_count];
	static int                     ms_used[R_count][B_count];
	static IDirect3DVertexBuffer9 *ms_d3dVertexBuffer[R_count][B_count];
	static MemoryBlockManager     *ms_memoryBlockManager;

	static int                     ms_locksSinceBeginFrame;
	static int                     ms_discardsSinceBeginFrame;
	static int                     ms_locksSinceResourceCreation;
	static int                     ms_discardsSinceResourceCreation;
	static int                     ms_locksEver;
	static int                     ms_discardsEver;

private:

	const VertexBufferDescriptor  &m_vertexBufferDescriptor;
	int                            m_numberOfVertices;
	int                            m_offset;
	int                            m_ringIndex;
	int                            m_bufferIndex;
	IDirect3DVertexDeclaration9   *m_vertexDeclaration;
};

// ======================================================================

inline int Direct3d9_DynamicVertexBufferData::getNumberOfVertices() const
{
	return m_numberOfVertices;
}

// ----------------------------------------------------------------------

inline int Direct3d9_DynamicVertexBufferData::getOffset() const
{
	return m_offset;
}

// ----------------------------------------------------------------------

inline IDirect3DVertexDeclaration9 *Direct3d9_DynamicVertexBufferData::getVertexDeclaration() const
{
	return m_vertexDeclaration;
}

// ======================================================================

#endif
