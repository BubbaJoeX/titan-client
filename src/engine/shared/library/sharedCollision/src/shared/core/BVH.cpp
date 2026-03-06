// ======================================================================
//
// BVH.cpp
// Copyright 2026 Titan Development Team
// All Rights Reserved.
//
// Bounding Volume Hierarchy implementation
//
// ======================================================================

#include "sharedCollision/FirstSharedCollision.h"
#include "sharedCollision/BVH.h"

#include "sharedCollision/Intersect3d.h"
#include "sharedObject/Object.h"
#include "sharedObject/Appearance.h"
#include "sharedDebug/DebugFlags.h"

#include <algorithm>
#include <limits>
#include <stack>

// ======================================================================

namespace BVHNamespace
{
	// Comparator for sorting QueryResults by distance
	struct QueryResultDistanceCompare
	{
		bool operator()(BVH::QueryResult const & a, BVH::QueryResult const & b) const
		{
			return a.distance < b.distance;
		}
	};

	// SAH constants
	float const cs_traversalCost = 1.0f;      // Cost of traversing a node
	float const cs_intersectionCost = 1.0f;   // Cost of intersection test
	int   const cs_maxLeafObjects = 4;        // Max objects per leaf before splitting
	int   const cs_sahBins = 12;              // Number of bins for SAH sweep

	// Compute surface area of AABB
	float surfaceArea(AxialBox const & box)
	{
		Vector const extent = box.getMax() - box.getMin();
		return 2.0f * (extent.x * extent.y + extent.y * extent.z + extent.z * extent.x);
	}

	// Merge two AABBs
	AxialBox merge(AxialBox const & a, AxialBox const & b)
	{
		AxialBox result;
		result.setMin(Vector(
			std::min(a.getMin().x, b.getMin().x),
			std::min(a.getMin().y, b.getMin().y),
			std::min(a.getMin().z, b.getMin().z)
		));
		result.setMax(Vector(
			std::max(a.getMax().x, b.getMax().x),
			std::max(a.getMax().y, b.getMax().y),
			std::max(a.getMax().z, b.getMax().z)
		));
		return result;
	}

	// Ray-AABB intersection
	bool rayIntersectsAABB(Ray3d const & ray, AxialBox const & box, float & tMin, float & tMax)
	{
		Vector const & origin = ray.getPoint();
		Vector const & dir = ray.getNormal();
		Vector const & boxMin = box.getMin();
		Vector const & boxMax = box.getMax();

		tMin = 0.0f;
		tMax = std::numeric_limits<float>::max();

		for (int i = 0; i < 3; ++i)
		{
			float origin_i = (i == 0) ? origin.x : (i == 1) ? origin.y : origin.z;
			float dir_i = (i == 0) ? dir.x : (i == 1) ? dir.y : dir.z;
			float min_i = (i == 0) ? boxMin.x : (i == 1) ? boxMin.y : boxMin.z;
			float max_i = (i == 0) ? boxMax.x : (i == 1) ? boxMax.y : boxMax.z;

			if (fabs(dir_i) < 1e-8f)
			{
				// Ray is parallel to slab
				if (origin_i < min_i || origin_i > max_i)
					return false;
			}
			else
			{
				float invDir = 1.0f / dir_i;
				float t1 = (min_i - origin_i) * invDir;
				float t2 = (max_i - origin_i) * invDir;

				if (t1 > t2)
					std::swap(t1, t2);

				tMin = std::max(tMin, t1);
				tMax = std::min(tMax, t2);

				if (tMin > tMax)
					return false;
			}
		}

		return true;
	}
}

using namespace BVHNamespace;

// ======================================================================

BVH::BVH()
	: m_root(-1)
	, m_leafCount(0)
	, m_maxDepth(0)
{
	// Reserve initial capacity
	m_nodes.reserve(1024);
	m_objectNodeMap.reserve(512);
}

// ----------------------------------------------------------------------

BVH::~BVH()
{
	clear();
}

// ----------------------------------------------------------------------

void BVH::clear()
{
	m_nodes.clear();
	m_freeNodes.clear();
	m_objectNodeMap.clear();
	m_root = -1;
	m_leafCount = 0;
	m_maxDepth = 0;
}

// ----------------------------------------------------------------------

void BVH::build(std::vector<Object *> const & objects)
{
	clear();

	if (objects.empty())
		return;

	// Create initial bounds and indices
	std::vector<int> objectIndices;
	objectIndices.reserve(objects.size());

	m_objectNodeMap.clear();
	m_objectNodeMap.reserve(objects.size());

	for (size_t i = 0; i < objects.size(); ++i)
	{
		Object * const obj = objects[i];
		if (!obj)
			continue;

		// Allocate leaf node for object
		int const nodeIndex = allocateNode();
		Node & node = m_nodes[nodeIndex];
		node.object = obj;
		node.type = NT_leaf;

		// Get object bounds from appearance sphere if available
		Vector const pos = obj->getPosition_w();
		float radius = 1.0f;
		Appearance const * const appearance = obj->getAppearance();
		if (appearance)
		{
			Sphere const & sphere = appearance->getSphere();
			float const sphereRadius = sphere.getRadius();
			if (sphereRadius > 0.0f)
				radius = sphereRadius;
		}

		node.bounds.setMin(pos - Vector(radius, radius, radius));
		node.bounds.setMax(pos + Vector(radius, radius, radius));

		objectIndices.push_back(nodeIndex);
		m_objectNodeMap.push_back(ObjectNodeEntry(obj, nodeIndex));
	}

	// Build tree using SAH
	m_root = buildSAH(objectIndices, 0, static_cast<int>(objectIndices.size()), 0);
	m_leafCount = static_cast<int>(objectIndices.size());
}

// ----------------------------------------------------------------------

int BVH::buildSAH(std::vector<int> & objectIndices, int start, int end, int depth)
{
	int const count = end - start;

	if (count == 0)
		return -1;

	m_maxDepth = std::max(m_maxDepth, depth);

	// Calculate combined bounds
	AxialBox bounds = m_nodes[objectIndices[start]].bounds;
	for (int i = start + 1; i < end; ++i)
	{
		bounds = merge(bounds, m_nodes[objectIndices[i]].bounds);
	}

	// If only one object, return leaf
	if (count == 1)
	{
		m_nodes[objectIndices[start]].depth = depth;
		return objectIndices[start];
	}

	// If few objects, create simple split
	if (count <= cs_maxLeafObjects)
	{
		// Create internal node
		int const nodeIndex = allocateNode();
		Node & node = m_nodes[nodeIndex];
		node.bounds = bounds;
		node.type = NT_internal;
		node.depth = depth;

		int const mid = start + count / 2;
		node.left = buildSAH(objectIndices, start, mid, depth + 1);
		node.right = buildSAH(objectIndices, mid, end, depth + 1);

		if (node.left >= 0) m_nodes[node.left].parent = nodeIndex;
		if (node.right >= 0) m_nodes[node.right].parent = nodeIndex;

		return nodeIndex;
	}

	// SAH partitioning
	int splitAxis;
	int const splitIndex = partitionSAH(objectIndices, start, end, splitAxis);

	if (splitIndex == start || splitIndex == end)
	{
		// SAH failed, use median split
		int const mid = start + count / 2;

		int const nodeIndex = allocateNode();
		Node & node = m_nodes[nodeIndex];
		node.bounds = bounds;
		node.type = NT_internal;
		node.depth = depth;

		node.left = buildSAH(objectIndices, start, mid, depth + 1);
		node.right = buildSAH(objectIndices, mid, end, depth + 1);

		if (node.left >= 0) m_nodes[node.left].parent = nodeIndex;
		if (node.right >= 0) m_nodes[node.right].parent = nodeIndex;

		return nodeIndex;
	}

	// Create internal node
	int const nodeIndex = allocateNode();
	Node & node = m_nodes[nodeIndex];
	node.bounds = bounds;
	node.type = NT_internal;
	node.depth = depth;

	node.left = buildSAH(objectIndices, start, splitIndex, depth + 1);
	node.right = buildSAH(objectIndices, splitIndex, end, depth + 1);

	if (node.left >= 0) m_nodes[node.left].parent = nodeIndex;
	if (node.right >= 0) m_nodes[node.right].parent = nodeIndex;

	return nodeIndex;
}

// ----------------------------------------------------------------------

int BVH::partitionSAH(std::vector<int> & objectIndices, int start, int end, int & outAxis)
{
	int const count = end - start;
	if (count <= 1)
	{
		outAxis = 0;
		return start;
	}

	// Calculate centroid bounds
	AxialBox centroidBounds;
	centroidBounds.setMin(m_nodes[objectIndices[start]].bounds.getCenter());
	centroidBounds.setMax(centroidBounds.getMin());

	for (int i = start + 1; i < end; ++i)
	{
		Vector const center = m_nodes[objectIndices[i]].bounds.getCenter();
		centroidBounds.setMin(Vector(
			std::min(centroidBounds.getMin().x, center.x),
			std::min(centroidBounds.getMin().y, center.y),
			std::min(centroidBounds.getMin().z, center.z)
		));
		centroidBounds.setMax(Vector(
			std::max(centroidBounds.getMax().x, center.x),
			std::max(centroidBounds.getMax().y, center.y),
			std::max(centroidBounds.getMax().z, center.z)
		));
	}

	// Find best axis to split
	Vector const extent = centroidBounds.getMax() - centroidBounds.getMin();
	outAxis = 0;
	if (extent.y > extent.x && extent.y > extent.z) outAxis = 1;
	else if (extent.z > extent.x && extent.z > extent.y) outAxis = 2;

	float const axisExtent = (outAxis == 0) ? extent.x : (outAxis == 1) ? extent.y : extent.z;

	// If all centroids are at the same point, use median
	if (axisExtent < 1e-6f)
		return start + count / 2;

	// SAH binning
	struct Bin { AxialBox bounds; int count; };
	Bin bins[cs_sahBins];
	for (int i = 0; i < cs_sahBins; ++i)
	{
		bins[i].count = 0;
	}

	float const axisMin = (outAxis == 0) ? centroidBounds.getMin().x :
		(outAxis == 1) ? centroidBounds.getMin().y : centroidBounds.getMin().z;

	for (int i = start; i < end; ++i)
	{
		Vector const center = m_nodes[objectIndices[i]].bounds.getCenter();
		float const centerCoord = (outAxis == 0) ? center.x : (outAxis == 1) ? center.y : center.z;

		int bin = static_cast<int>(cs_sahBins * (centerCoord - axisMin) / axisExtent);
		bin = std::max(0, std::min(cs_sahBins - 1, bin));

		if (bins[bin].count == 0)
			bins[bin].bounds = m_nodes[objectIndices[i]].bounds;
		else
			bins[bin].bounds = merge(bins[bin].bounds, m_nodes[objectIndices[i]].bounds);

		++bins[bin].count;
	}

	// Compute costs for each split position
	float bestCost = std::numeric_limits<float>::max();
	int bestSplit = -1;

	for (int i = 0; i < cs_sahBins - 1; ++i)
	{
		AxialBox leftBounds, rightBounds;
		int leftCount = 0, rightCount = 0;

		// Left side
		for (int j = 0; j <= i; ++j)
		{
			if (bins[j].count > 0)
			{
				if (leftCount == 0)
					leftBounds = bins[j].bounds;
				else
					leftBounds = merge(leftBounds, bins[j].bounds);
				leftCount += bins[j].count;
			}
		}

		// Right side
		for (int j = i + 1; j < cs_sahBins; ++j)
		{
			if (bins[j].count > 0)
			{
				if (rightCount == 0)
					rightBounds = bins[j].bounds;
				else
					rightBounds = merge(rightBounds, bins[j].bounds);
				rightCount += bins[j].count;
			}
		}

		if (leftCount == 0 || rightCount == 0)
			continue;

		// SAH cost
		float const cost = cs_traversalCost + cs_intersectionCost *
			(leftCount * surfaceArea(leftBounds) + rightCount * surfaceArea(rightBounds));

		if (cost < bestCost)
		{
			bestCost = cost;
			bestSplit = i;
		}
	}

	if (bestSplit < 0)
		return start + count / 2;

	// Partition objects
	float const splitCoord = axisMin + (bestSplit + 1) * axisExtent / cs_sahBins;

	// Manual partition - move elements less than splitCoord to the left
	int left = start;
	int right = end - 1;
	while (left <= right)
	{
		Vector const center = m_nodes[objectIndices[left]].bounds.getCenter();
		float const coord = (outAxis == 0) ? center.x : (outAxis == 1) ? center.y : center.z;
		if (coord < splitCoord)
		{
			++left;
		}
		else
		{
			int const temp = objectIndices[left];
			objectIndices[left] = objectIndices[right];
			objectIndices[right] = temp;
			--right;
		}
	}

	return left;
}

// ----------------------------------------------------------------------

int BVH::insert(Object * object, AxialBox const & bounds)
{
	// Allocate leaf node
	int const leafIndex = allocateNode();
	Node & leaf = m_nodes[leafIndex];
	leaf.bounds = bounds;
	leaf.object = object;
	leaf.type = NT_leaf;

	m_objectNodeMap.push_back(ObjectNodeEntry(object, leafIndex));
	++m_leafCount;

	// If tree is empty, this is the root
	if (m_root < 0)
	{
		m_root = leafIndex;
		return leafIndex;
	}

	// Find best sibling
	int const sibling = findBestSibling(bounds);

	// Create new parent
	int const oldParent = m_nodes[sibling].parent;
	int const newParent = allocateNode();
	Node & parentNode = m_nodes[newParent];

	parentNode.parent = oldParent;
	parentNode.bounds = merge(bounds, m_nodes[sibling].bounds);
	parentNode.type = NT_internal;
	parentNode.left = sibling;
	parentNode.right = leafIndex;

	m_nodes[sibling].parent = newParent;
	leaf.parent = newParent;

	if (oldParent >= 0)
	{
		if (m_nodes[oldParent].left == sibling)
			m_nodes[oldParent].left = newParent;
		else
			m_nodes[oldParent].right = newParent;
	}
	else
	{
		m_root = newParent;
	}

	// Refit ancestors
	refitAncestors(newParent);

	return leafIndex;
}

// ----------------------------------------------------------------------

void BVH::remove(int nodeIndex)
{
	if (nodeIndex < 0 || nodeIndex >= static_cast<int>(m_nodes.size()))
		return;

	Node & node = m_nodes[nodeIndex];
	if (!node.isLeaf())
		return;

	// Remove from object map
	for (size_t i = 0; i < m_objectNodeMap.size(); ++i)
	{
		if (m_objectNodeMap[i].nodeIndex == nodeIndex)
		{
			m_objectNodeMap.erase(m_objectNodeMap.begin() + i);
			break;
		}
	}

	--m_leafCount;

	int const parent = node.parent;

	if (parent < 0)
	{
		// This is the root
		m_root = -1;
		freeNode(nodeIndex);
		return;
	}

	// Get sibling
	Node & parentNode = m_nodes[parent];
	int const sibling = (parentNode.left == nodeIndex) ? parentNode.right : parentNode.left;

	int const grandparent = parentNode.parent;

	if (grandparent >= 0)
	{
		// Replace parent with sibling
		Node & gpNode = m_nodes[grandparent];
		if (gpNode.left == parent)
			gpNode.left = sibling;
		else
			gpNode.right = sibling;

		m_nodes[sibling].parent = grandparent;
		refitAncestors(grandparent);
	}
	else
	{
		// Parent was root, sibling becomes root
		m_root = sibling;
		m_nodes[sibling].parent = -1;
	}

	freeNode(parent);
	freeNode(nodeIndex);
}

// ----------------------------------------------------------------------

void BVH::remove(Object * object)
{
	for (size_t i = 0; i < m_objectNodeMap.size(); ++i)
	{
		if (m_objectNodeMap[i].object == object)
		{
			remove(m_objectNodeMap[i].nodeIndex);
			return;
		}
	}
}

// ----------------------------------------------------------------------

void BVH::update(int nodeIndex, AxialBox const & newBounds)
{
	if (nodeIndex < 0 || nodeIndex >= static_cast<int>(m_nodes.size()))
		return;

	Node & node = m_nodes[nodeIndex];
	if (!node.isLeaf())
		return;

	// Check if new bounds fit within current bounds (with some margin)
	AxialBox const & oldBounds = node.bounds;
	float const margin = 0.1f;

	bool const contained =
		newBounds.getMin().x >= oldBounds.getMin().x - margin &&
		newBounds.getMin().y >= oldBounds.getMin().y - margin &&
		newBounds.getMin().z >= oldBounds.getMin().z - margin &&
		newBounds.getMax().x <= oldBounds.getMax().x + margin &&
		newBounds.getMax().y <= oldBounds.getMax().y + margin &&
		newBounds.getMax().z <= oldBounds.getMax().z + margin;

	if (contained)
	{
		// Just update bounds, no need to restructure
		node.bounds = newBounds;
		return;
	}

	// Bounds changed significantly, remove and reinsert
	Object * const obj = node.object;
	remove(nodeIndex);
	insert(obj, newBounds);
}

// ----------------------------------------------------------------------

void BVH::update(Object * object, AxialBox const & newBounds)
{
	for (size_t i = 0; i < m_objectNodeMap.size(); ++i)
	{
		if (m_objectNodeMap[i].object == object)
		{
			update(m_objectNodeMap[i].nodeIndex, newBounds);
			return;
		}
	}
}

// ----------------------------------------------------------------------

void BVH::queryRay(Ray3d const & ray, std::vector<QueryResult> & results, float maxDistance) const
{
	results.clear();

	if (m_root < 0)
		return;

	std::stack<int> stack;
	stack.push(m_root);

	while (!stack.empty())
	{
		int const nodeIndex = stack.top();
		stack.pop();

		if (nodeIndex < 0)
			continue;

		Node const & node = m_nodes[nodeIndex];

		float tMin, tMax;
		if (!rayIntersectsAABB(ray, node.bounds, tMin, tMax))
			continue;

		if (tMin > maxDistance)
			continue;

		if (node.isLeaf())
		{
			if (node.object)
			{
				QueryResult result;
				result.object = node.object;
				result.distance = tMin;
				result.hitPoint = ray.getPoint() + ray.getNormal() * tMin;
				results.push_back(result);
			}
		}
		else
		{
			stack.push(node.left);
			stack.push(node.right);
		}
	}

	// Sort by distance
	std::sort(results.begin(), results.end(), BVHNamespace::QueryResultDistanceCompare());
}

// ----------------------------------------------------------------------

void BVH::querySphere(Sphere const & sphere, std::vector<Object *> & results) const
{
	results.clear();

	if (m_root < 0)
		return;

	// Convert sphere to AABB for initial test
	AxialBox sphereBox;
	Vector const center = sphere.getCenter();
	float const radius = sphere.getRadius();
	sphereBox.setMin(center - Vector(radius, radius, radius));
	sphereBox.setMax(center + Vector(radius, radius, radius));

	std::stack<int> stack;
	stack.push(m_root);

	while (!stack.empty())
	{
		int const nodeIndex = stack.top();
		stack.pop();

		if (nodeIndex < 0)
			continue;

		Node const & node = m_nodes[nodeIndex];

		if (!intersectBoxNode(sphereBox, nodeIndex))
			continue;

		if (node.isLeaf())
		{
			if (node.object)
				results.push_back(node.object);
		}
		else
		{
			stack.push(node.left);
			stack.push(node.right);
		}
	}
}

// ----------------------------------------------------------------------

void BVH::queryBox(AxialBox const & box, std::vector<Object *> & results) const
{
	results.clear();

	if (m_root < 0)
		return;

	std::stack<int> stack;
	stack.push(m_root);

	while (!stack.empty())
	{
		int const nodeIndex = stack.top();
		stack.pop();

		if (nodeIndex < 0)
			continue;

		Node const & node = m_nodes[nodeIndex];

		if (!intersectBoxNode(box, nodeIndex))
			continue;

		if (node.isLeaf())
		{
			if (node.object)
				results.push_back(node.object);
		}
		else
		{
			stack.push(node.left);
			stack.push(node.right);
		}
	}
}

// ----------------------------------------------------------------------

bool BVH::queryRay(Ray3d const & ray, QueryCallback callback, float maxDistance) const
{
	if (!callback || m_root < 0)
		return false;

	std::stack<int> stack;
	stack.push(m_root);

	while (!stack.empty())
	{
		int const nodeIndex = stack.top();
		stack.pop();

		if (nodeIndex < 0)
			continue;

		Node const & node = m_nodes[nodeIndex];

		float tMin, tMax;
		if (!rayIntersectsAABB(ray, node.bounds, tMin, tMax))
			continue;

		if (tMin > maxDistance)
			continue;

		if (node.isLeaf())
		{
			if (node.object)
			{
				if (!callback(node.object, tMin))
					return true;  // Callback requested early-out
			}
		}
		else
		{
			stack.push(node.left);
			stack.push(node.right);
		}
	}

	return false;
}

// ----------------------------------------------------------------------

bool BVH::querySphere(Sphere const & sphere, QueryCallback callback) const
{
	if (!callback || m_root < 0)
		return false;

	AxialBox sphereBox;
	Vector const center = sphere.getCenter();
	float const radius = sphere.getRadius();
	sphereBox.setMin(center - Vector(radius, radius, radius));
	sphereBox.setMax(center + Vector(radius, radius, radius));

	std::stack<int> stack;
	stack.push(m_root);

	while (!stack.empty())
	{
		int const nodeIndex = stack.top();
		stack.pop();

		if (nodeIndex < 0)
			continue;

		Node const & node = m_nodes[nodeIndex];

		if (!intersectBoxNode(sphereBox, nodeIndex))
			continue;

		if (node.isLeaf())
		{
			if (node.object)
			{
				float const dist = (node.bounds.getCenter() - center).magnitude();
				if (!callback(node.object, dist))
					return true;
			}
		}
		else
		{
			stack.push(node.left);
			stack.push(node.right);
		}
	}

	return false;
}

// ----------------------------------------------------------------------

bool BVH::queryBox(AxialBox const & box, QueryCallback callback) const
{
	if (!callback || m_root < 0)
		return false;

	std::stack<int> stack;
	stack.push(m_root);

	while (!stack.empty())
	{
		int const nodeIndex = stack.top();
		stack.pop();

		if (nodeIndex < 0)
			continue;

		Node const & node = m_nodes[nodeIndex];

		if (!intersectBoxNode(box, nodeIndex))
			continue;

		if (node.isLeaf())
		{
			if (node.object)
			{
				if (!callback(node.object, 0.0f))
					return true;
			}
		}
		else
		{
			stack.push(node.left);
			stack.push(node.right);
		}
	}

	return false;
}

// ----------------------------------------------------------------------

Object * BVH::queryRayFirst(Ray3d const & ray, float & outDistance, float maxDistance) const
{
	Object * closestObject = nullptr;
	outDistance = maxDistance;

	if (m_root < 0)
		return nullptr;

	std::stack<int> stack;
	stack.push(m_root);

	while (!stack.empty())
	{
		int const nodeIndex = stack.top();
		stack.pop();

		if (nodeIndex < 0)
			continue;

		Node const & node = m_nodes[nodeIndex];

		float tMin, tMax;
		if (!rayIntersectsAABB(ray, node.bounds, tMin, tMax))
			continue;

		if (tMin > outDistance)
			continue;

		if (node.isLeaf())
		{
			if (node.object && tMin < outDistance)
			{
				outDistance = tMin;
				closestObject = node.object;
			}
		}
		else
		{
			// Push children, closer one last (processed first)
			float tMinL, tMaxL, tMinR, tMaxR;
			bool hitL = node.left >= 0 && rayIntersectsAABB(ray, m_nodes[node.left].bounds, tMinL, tMaxL);
			bool hitR = node.right >= 0 && rayIntersectsAABB(ray, m_nodes[node.right].bounds, tMinR, tMaxR);

			if (hitL && hitR)
			{
				if (tMinL < tMinR)
				{
					stack.push(node.right);
					stack.push(node.left);
				}
				else
				{
					stack.push(node.left);
					stack.push(node.right);
				}
			}
			else if (hitL)
			{
				stack.push(node.left);
			}
			else if (hitR)
			{
				stack.push(node.right);
			}
		}
	}

	return closestObject;
}

// ----------------------------------------------------------------------

void BVH::queryAllPairs(std::vector<ObjectPair> & pairs) const
{
	pairs.clear();

	// Collect all leaves
	std::vector<int> leaves;
	for (size_t i = 0; i < m_objectNodeMap.size(); ++i)
	{
		leaves.push_back(m_objectNodeMap[i].nodeIndex);
	}

	// Check each pair (can be optimized with better tree traversal)
	for (size_t i = 0; i < leaves.size(); ++i)
	{
		Node const & nodeA = m_nodes[leaves[i]];

		for (size_t j = i + 1; j < leaves.size(); ++j)
		{
			Node const & nodeB = m_nodes[leaves[j]];

			// AABB overlap test
			if (nodeA.bounds.getMax().x < nodeB.bounds.getMin().x) continue;
			if (nodeA.bounds.getMin().x > nodeB.bounds.getMax().x) continue;
			if (nodeA.bounds.getMax().y < nodeB.bounds.getMin().y) continue;
			if (nodeA.bounds.getMin().y > nodeB.bounds.getMax().y) continue;
			if (nodeA.bounds.getMax().z < nodeB.bounds.getMin().z) continue;
			if (nodeA.bounds.getMin().z > nodeB.bounds.getMax().z) continue;

			if (nodeA.object && nodeB.object)
				pairs.push_back(ObjectPair(nodeA.object, nodeB.object));
		}
	}
}

// ----------------------------------------------------------------------

Object * BVH::queryPointFirst(Vector const & point) const
{
	if (m_root < 0)
		return 0;

	std::stack<int> stack;
	stack.push(m_root);

	while (!stack.empty())
	{
		int const nodeIndex = stack.top();
		stack.pop();

		if (nodeIndex < 0)
			continue;

		Node const & node = m_nodes[nodeIndex];

		if (!node.bounds.contains(point))
			continue;

		if (node.isLeaf())
		{
			if (node.object)
				return node.object;
		}
		else
		{
			stack.push(node.left);
			stack.push(node.right);
		}
	}

	return 0;
}

// ----------------------------------------------------------------------

void BVH::queryPairsFor(Object * object, std::vector<Object *> & results) const
{
	results.clear();

	if (!object || m_root < 0)
		return;

	// Find the node for this object
	int objectNodeIndex = -1;
	for (size_t i = 0; i < m_objectNodeMap.size(); ++i)
	{
		if (m_objectNodeMap[i].object == object)
		{
			objectNodeIndex = m_objectNodeMap[i].nodeIndex;
			break;
		}
	}

	if (objectNodeIndex < 0)
		return;

	AxialBox const & objectBounds = m_nodes[objectNodeIndex].bounds;

	// Query tree for overlapping objects
	std::stack<int> stack;
	stack.push(m_root);

	while (!stack.empty())
	{
		int const nodeIndex = stack.top();
		stack.pop();

		if (nodeIndex < 0)
			continue;

		Node const & node = m_nodes[nodeIndex];

		if (!objectBounds.intersects(node.bounds))
			continue;

		if (node.isLeaf())
		{
			if (node.object && node.object != object)
				results.push_back(node.object);
		}
		else
		{
			stack.push(node.left);
			stack.push(node.right);
		}
	}
}

// ----------------------------------------------------------------------

int BVH::allocateNode()
{
	if (!m_freeNodes.empty())
	{
		int const index = m_freeNodes.back();
		m_freeNodes.pop_back();
		m_nodes[index] = Node();
		return index;
	}

	int const index = static_cast<int>(m_nodes.size());
	m_nodes.push_back(Node());
	return index;
}

// ----------------------------------------------------------------------

void BVH::freeNode(int index)
{
	if (index >= 0 && index < static_cast<int>(m_nodes.size()))
	{
		m_nodes[index] = Node();
		m_freeNodes.push_back(index);
	}
}

// ----------------------------------------------------------------------

void BVH::refitNode(int index)
{
	if (index < 0 || index >= static_cast<int>(m_nodes.size()))
		return;

	Node & node = m_nodes[index];
	if (node.isLeaf())
		return;

	if (node.left >= 0 && node.right >= 0)
	{
		node.bounds = merge(m_nodes[node.left].bounds, m_nodes[node.right].bounds);
	}
	else if (node.left >= 0)
	{
		node.bounds = m_nodes[node.left].bounds;
	}
	else if (node.right >= 0)
	{
		node.bounds = m_nodes[node.right].bounds;
	}
}

// ----------------------------------------------------------------------

void BVH::refitAncestors(int index)
{
	int current = index;
	while (current >= 0)
	{
		refitNode(current);
		current = m_nodes[current].parent;
	}
}

// ----------------------------------------------------------------------

int BVH::findBestSibling(AxialBox const & bounds) const
{
	if (m_root < 0)
		return -1;

	int bestSibling = m_root;
	float bestCost = std::numeric_limits<float>::max();

	std::stack<int> stack;
	stack.push(m_root);

	while (!stack.empty())
	{
		int const nodeIndex = stack.top();
		stack.pop();

		if (nodeIndex < 0)
			continue;

		Node const & node = m_nodes[nodeIndex];

		// Cost of inserting here
		AxialBox const combined = merge(bounds, node.bounds);
		float const cost = surfaceArea(combined);

		if (cost < bestCost)
		{
			bestCost = cost;
			bestSibling = nodeIndex;
		}

		// Lower bound on cost of inserting in subtree
		if (!node.isLeaf())
		{
			float const inheritedCost = surfaceArea(combined) - surfaceArea(node.bounds);
			float const lowerBound = surfaceArea(bounds) + inheritedCost;

			if (lowerBound < bestCost)
			{
				stack.push(node.left);
				stack.push(node.right);
			}
		}
	}

	return bestSibling;
}

// ----------------------------------------------------------------------

bool BVH::intersectBoxNode(AxialBox const & box, int nodeIndex) const
{
	if (nodeIndex < 0 || nodeIndex >= static_cast<int>(m_nodes.size()))
		return false;

	AxialBox const & nodeBox = m_nodes[nodeIndex].bounds;

	return !(
		box.getMax().x < nodeBox.getMin().x ||
		box.getMin().x > nodeBox.getMax().x ||
		box.getMax().y < nodeBox.getMin().y ||
		box.getMin().y > nodeBox.getMax().y ||
		box.getMax().z < nodeBox.getMin().z ||
		box.getMin().z > nodeBox.getMax().z
	);
}

// ----------------------------------------------------------------------

BVH::Node const * BVH::getNode(int index) const
{
	if (index < 0 || index >= static_cast<int>(m_nodes.size()))
		return nullptr;
	return &m_nodes[index];
}

// ----------------------------------------------------------------------

BVH::Node const * BVH::getRoot() const
{
	return getNode(m_root);
}

// ======================================================================

