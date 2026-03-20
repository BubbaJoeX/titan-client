// ======================================================================
//
// VertexBufferIterator.cpp
// Copyright 2000-01, Sony Online Entertainment Inc.
// All Rights Reserved.
//
// ======================================================================

#include "VertexBufferIterator.h"

// ======================================================================

VertexBufferBaseIterator::~VertexBufferBaseIterator()
{
    m_vertexBuffer = nullptr;
    m_descriptor = nullptr;
    m_data = nullptr;
}

// ----------------------------------------------------------------------

VertexBufferReadIterator::~VertexBufferReadIterator()
{
}

// ----------------------------------------------------------------------

VertexBufferWriteIterator::~VertexBufferWriteIterator()
{
}

// ----------------------------------------------------------------------

VertexBufferReadWriteIterator::~VertexBufferReadWriteIterator()
{
}

// ======================================================================
