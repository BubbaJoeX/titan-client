// ======================================================================
//
// BVH.h
// Copyright 2026 Titan Development Team
// All Rights Reserved.
//
// Bounding Volume Hierarchy for efficient spatial queries
// Uses Surface Area Heuristic (SAH) for optimal tree construction
//
// ======================================================================

#ifndef INCLUDED_BVH_H
#define INCLUDED_BVH_H

// ======================================================================

#include "sharedMath/AxialBox.h"
#include "sharedMath/Ray3d.h"
#include "sharedMath/Sphere.h"
#include "sharedMath/Vector.h"

#include <vector>

// ======================================================================

class Object;

// ======================================================================

/**
 * BVH (Bounding Volume Hierarchy) - A spatial acceleration structure
 * for efficient collision detection and visibility queries.
 *
 * Features:
 * - SAH (Surface Area Heuristic) construction for optimal tree balance
 * - Support for both AABB (AxialBox) and sphere bounds
 * - Incremental updates for dynamic objects
 * - Thread-safe read access
 * - Query types: ray cast, sphere overlap, box overlap, frustum cull
 *
 * Complexity:
 * - Build: O(n log n)
 * - Query: O(log n) average case
 * - Update: O(log n) for single object
 */
class BVH
{
public:
	// Node type
	enum NodeType
	{
		NT_internal,    // Has children, no object
		NT_leaf         // Has object, no children
	};

	// BVH Node
	struct Node
	{
		AxialBox bounds;           // Bounding volume
		int      parent;           // Parent node index (-1 for root)
		int      left;             // Left child index (-1 for leaf)
		int      right;            // Right child index (-1 for leaf)
		Object * object;           // Object pointer (only for leaves)
		NodeType type;
		int      depth;            // Tree depth (for debugging)

		Node()
			: parent(-1)
			, left(-1)
			, right(-1)
			, object(nullptr)
			, type(NT_leaf)
			, depth(0)
		{}

		bool isLeaf() const { return type == NT_leaf; }
	};

	// Query result
	struct QueryResult
	{
		Object * object;
		float    distance;         // For ray queries
		Vector   hitPoint;         // For ray queries

		QueryResult() : object(nullptr), distance(0.0f) {}
		QueryResult(Object * obj, float dist = 0.0f) : object(obj), distance(dist) {}
	};

	// Query callback type - returns true to continue query, false to stop
	typedef bool (*QueryCallback)(Object * object, float distance);

public:
	BVH();
	~BVH();

	// Construction
	void clear();
	void build(std::vector<Object *> const & objects);
	void buildIncremental();  // Rebuild tree maintaining balance

	// Object management
	int  insert(Object * object, AxialBox const & bounds);
	void remove(int nodeIndex);
	void remove(Object * object);
	void update(int nodeIndex, AxialBox const & newBounds);
	void update(Object * object, AxialBox const & newBounds);

	// Queries - return all results
	void queryRay(Ray3d const & ray, std::vector<QueryResult> & results, float maxDistance = 1e10f) const;
	void querySphere(Sphere const & sphere, std::vector<Object *> & results) const;
	void queryBox(AxialBox const & box, std::vector<Object *> & results) const;
	void queryFrustum(Vector const * frustumPlanes, int planeCount, std::vector<Object *> & results) const;

	// Queries with callback (early-out capable)
	bool queryRay(Ray3d const & ray, QueryCallback callback, float maxDistance = 1e10f) const;
	bool querySphere(Sphere const & sphere, QueryCallback callback) const;
	bool queryBox(AxialBox const & box, QueryCallback callback) const;

	// Queries - return first/closest
	Object * queryRayFirst(Ray3d const & ray, float & outDistance, float maxDistance = 1e10f) const;
	Object * queryPointFirst(Vector const & point) const;

	// Pair queries for collision detection
	struct ObjectPair
	{
		Object * first;
		Object * second;
		ObjectPair() : first(0), second(0) {}
		ObjectPair(Object * a, Object * b) : first(a), second(b) {}
	};
	void queryAllPairs(std::vector<ObjectPair> & pairs) const;
	void queryPairsFor(Object * object, std::vector<Object *> & results) const;

	// Tree info
	int  getNodeCount() const { return static_cast<int>(m_nodes.size()); }
	int  getLeafCount() const { return m_leafCount; }
	int  getMaxDepth() const { return m_maxDepth; }
	float getAverageCost() const;  // SAH cost metric

	// Node access
	Node const * getNode(int index) const;
	Node const * getRoot() const;

	// Debugging
	void validate() const;
	void getTreeStatistics(int & nodeCount, int & leafCount, int & maxDepth, float & avgLeafDepth) const;

private:
	// SAH construction helpers
	int  buildSAH(std::vector<int> & objectIndices, int start, int end, int depth);
	int  partitionSAH(std::vector<int> & objectIndices, int start, int end, int & outAxis);
	float computeSAHCost(AxialBox const & bounds, int objectCount) const;

	// Tree maintenance
	int  allocateNode();
	void freeNode(int index);
	void refitNode(int index);
	void refitAncestors(int index);
	int  findBestSibling(AxialBox const & bounds) const;
	void rotateNode(int index);  // Tree rotation for balance

	// Query helpers
	bool intersectRayNode(Ray3d const & ray, int nodeIndex, float & tMin, float & tMax) const;
	bool intersectSphereNode(Sphere const & sphere, int nodeIndex) const;
	bool intersectBoxNode(AxialBox const & box, int nodeIndex) const;

	// Data
	std::vector<Node> m_nodes;
	std::vector<int>  m_freeNodes;
	int               m_root;
	int               m_leafCount;
	int               m_maxDepth;

	// Object to node mapping for updates
	struct ObjectNodeEntry
	{
		Object * object;
		int      nodeIndex;
		ObjectNodeEntry() : object(0), nodeIndex(-1) {}
		ObjectNodeEntry(Object * o, int n) : object(o), nodeIndex(n) {}
	};
	std::vector<ObjectNodeEntry> m_objectNodeMap;
};

// ======================================================================

#endif

