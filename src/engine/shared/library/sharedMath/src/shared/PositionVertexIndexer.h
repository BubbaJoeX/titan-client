// ======================================================================
//
// PositionVertexIndexer.h
// Copyright 2004, Sony Online Entertainment
//
// ======================================================================

#ifndef INCLUDED_PositionVertexIndexer_H
#define INCLUDED_PositionVertexIndexer_H

// ======================================================================

#include "sharedMath/Vector.h"

#ifdef _WIN64
#include <unordered_map>
#else
#include <hash_map>
#endif

// ======================================================================

class PositionVertexIndexer
{
public:
	typedef stdvector<Vector>::fwd VectorVector;

	PositionVertexIndexer();
	~PositionVertexIndexer();

	void clear();
	void reserve(int numberOfVertices);
	int addVertex(Vector const & vertex);
	int getNumberOfVertices() const;

	Vector const & getVertex(int index) const;
	VectorVector const & getVertices() const;

	Vector & getVertex(int index);
	VectorVector & getVertices();

private:

	PositionVertexIndexer(PositionVertexIndexer const &);
	PositionVertexIndexer & operator=(PositionVertexIndexer const &);

private:

#ifdef _WIN64
	typedef std::unordered_multimap<uint32 /*crc*/, int /*index*/> VertexIndexMap;
#else
	typedef std::hash_multimap<uint32 /*crc*/, int /*index*/> VertexIndexMap;
#endif

	VectorVector * m_vertices;
	VertexIndexMap * m_indexMap;
};

// ======================================================================

#endif
