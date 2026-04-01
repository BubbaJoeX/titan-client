// ======================================================================
//
// Floor.cpp
// copyright (c) 2001 Sony Online Entertainment
//
// ======================================================================

#include "sharedCollision/FirstSharedCollision.h"
#include "sharedCollision/Floor.h"

#include "sharedCollision/CollisionUtils.h"
#include "sharedCollision/ConfigSharedCollision.h"
#include "sharedCollision/Extent.h"
#include "sharedCollision/FloorLocator.h"
#include "sharedCollision/FloorManager.h"
#include "sharedCollision/FloorMesh.h"
#include "sharedCollision/FloorTri.h"
#include "sharedCollision/Footprint.h"
#include "sharedCollision/Intersect3d.h"
#include "sharedMath/DebugShapeRenderer.h"
#include "sharedMath/Line3d.h"
#include "sharedMath/Plane3d.h"
#include "sharedMath/Ray3d.h"
#include "sharedMath/Segment3d.h"
#include "sharedMath/ShapeUtils.h"
#include "sharedMath/Sphere.h"
#include "sharedMath/Transform.h"
#include "sharedMath/Triangle3d.h"
#include "sharedMath/Vector.h"
#include "sharedObject/Appearance.h"
#include "sharedObject/CellProperty.h"
#include "sharedObject/Object.h"

#include <vector>
#include <set>
#include <algorithm>
#include <cmath>

namespace
{
	inline float maxAbsScale3( Vector const & s )
	{
		real const ax = std::fabs( s.x );
		real const ay = std::fabs( s.y );
		real const az = std::fabs( s.z );
		return static_cast<float>( std::max( ax, std::max( ay, az ) ) );
	}

	inline bool scaleVecChanged( Vector const & a, Vector const & b )
	{
		return std::fabs( a.x - b.x ) > 1.0e-5f || std::fabs( a.y - b.y ) > 1.0e-5f || std::fabs( a.z - b.z ) > 1.0e-5f;
	}
}

// Need to disable this warning to make MultiListHandle work

#pragma warning(disable : 4355) // 'this' used in base member initializer list

// ======================================================================

Floor::Floor( FloorMesh const * pMesh, Object const * owner, Appearance const * appearance, bool objectFloor )
: BaseClass(),
  CollisionSurface(),
  m_owner(owner),
  m_appearance(appearance),
  m_objectFloor(objectFloor),
  m_mesh(pMesh),
  m_footList(this,1),
  m_databaseList(this,1),
  m_spatialSubdivisionHandle(NULL),
  m_extent(NULL),
  m_extentDirty(true),
  m_hasLastScaleVec(false),
  m_lastScaleVec(),
  m_sphere_l(),
  m_sphere_w()
{
    NOT_NULL(pMesh);

    const_cast<FloorMesh*>(m_mesh)->addReference();

	const_cast<FloorMesh*>(m_mesh)->setObjectFloor(objectFloor);

	const_cast<FloorMesh*>(m_mesh)->setAppearance(appearance);

	// Only build box trees for object floors

	if( ConfigSharedCollision::getBuildBoxTrees() && m_objectFloor && !m_mesh->hasBoxTree() )
	{
		const_cast<FloorMesh*>(m_mesh)->buildBoxTree();
	}
}

// ----------

Floor::~Floor()
{
	if(isCellFloor())
	{
		if(!m_footList.isEmpty())
		{

			DEBUG_WARNING(true,("Floor::~Floor - Deleting a cell floor that still has footprints on it - this is probably an error, and is probably caused by the parent building getting deleted without being removed from the world\n"));

			// gotta do this to prevent erasing-while-iterating bug

			static std::vector<Footprint*> tempList;

			tempList.clear();

			for(MultiListIterator<Footprint> it(m_footList); it; ++it)
			{
				tempList.push_back(*it);
			}

			int footCount = tempList.size();

			for(int i = 0; i < footCount; i++)
			{
				tempList[i]->snapToObject();
			}

		}
	}

    const_cast<FloorMesh*>(m_mesh)->releaseReference();
    m_mesh = NULL;

	m_owner = NULL;
	m_appearance = NULL;

    m_spatialSubdivisionHandle = NULL;

	delete m_extent;
	m_extent = NULL;
}

// ----------------------------------------------------------------------

Transform const & Floor::getTransform_o2p ( void ) const
{
	if(isCellFloor())
	{
		return Transform::identity;
	}
	else
	{
		Object const * pOwner = getOwner();

		if(pOwner)
		{
			return pOwner->getTransform_o2p();
		}
		else
		{
			return Transform::identity;
		}
	}
}

// ----------

Transform const & Floor::getTransform_o2w ( void ) const
{
	Object const * pOwner = getOwner();

	if(pOwner)
	{
		return pOwner->getTransform_o2w();
	}
	else
	{
		return Transform::identity;
	}
}

// ----------

float Floor::getScale ( void ) const
{
	return maxAbsScale3( getScaleVector() );
}

// ----------

Vector Floor::getScaleVector ( void ) const
{
	if(isCellFloor())
		return Vector::xyz111;

	Object const * const pOwner = getOwner();
	if(pOwner)
		return pOwner->getScale();

	return Vector::xyz111;
}

// ----------

Vector Floor::getLocalDropDirection ( void ) const
{
	// Parent-space "down" (-Y) expressed in floor/mesh local axes (rotation only); used for drop tests and snapping on rotated object floors.
	return rotate_p2l( Vector::negativeUnitY );
}

// ----------------------------------------------------------------------

BaseExtent const * Floor::getExtent_l ( void ) const
{
    return getFloorMesh()->getExtent_l();
}

// ----------

BaseExtent const * Floor::getExtent_p ( void ) const
{
	if(getExtentDirty())
	{
		updateExtent();
	}

	return m_extent;
}

// ----------

bool Floor::getExtentDirty ( void ) const
{
	Vector const s = getScaleVector();
	if (m_hasLastScaleVec && scaleVecChanged( s, m_lastScaleVec ))
		m_extentDirty = true;
	return m_extentDirty;
}

// ----------

void Floor::invalidateExtent ( void )
{
	m_extentDirty = true;
}

// ----------

void Floor::updateExtent ( void ) const
{
	BaseExtent const * localExtent = getExtent_l();
	
	if(!m_extent)
	{
		if(localExtent)
		{
			m_extent = localExtent->clone();
		}
	}
	
	if(m_extent && localExtent)
	{
		Vector const s = getScaleVector();
		float const uniformScale = maxAbsScale3( s );
		m_extent->transform( localExtent, getTransform_o2p(), uniformScale );
		m_lastScaleVec = s;
		m_hasLastScaleVec = true;
		m_extentDirty = false;
	}

	// ----------

	m_sphere_l = getExtent_l()->getBoundingSphere();
	Vector const s = getScaleVector();
	Vector const scaledCenter( m_sphere_l.getCenter().x * s.x, m_sphere_l.getCenter().y * s.y, m_sphere_l.getCenter().z * s.z );
	float const rScale = maxAbsScale3( s );
	m_sphere_w = Sphere( getTransform_o2w().rotateTranslate_l2p( scaledCenter ), m_sphere_l.getRadius() * rScale );
}

// ----------------------------------------------------------------------

bool Floor::dropTestBounds ( Vector const & position_p ) const
{
	Vector dropDir = getLocalDropDirection();
	IGNORE_RETURN( dropDir.normalize() );
	Line3d line_l( transform_p2l( position_p ), dropDir );

	return getFloorMesh()->testBounds(line_l);
}

// ----------------------------------------------------------------------

bool Floor::segTestBounds ( Vector const & begin, Vector const & end ) const
{
	Segment3d seg(transform_p2l(begin), transform_p2l(end));

	return getFloorMesh()->testBounds(seg);
}

// ----------------------------------------------------------------------

Object const * Floor::getOwner ( void ) const
{
	if(m_owner != NULL)
	{
		return m_owner;
	}
	else if(m_appearance)
	{
		return m_appearance->getOwner();
	}
	else
	{
		return NULL;
	}
}

// ----------

Appearance const * Floor::getAppearance ( void ) const
{
	if(m_appearance != NULL)
	{
		return m_appearance;
	}
	else if(m_owner)
	{
		return m_owner->getAppearance();
	}
	else
	{
		return NULL;
	}
}

// ----------

CellProperty const * Floor::getCell ( void ) const
{
	if(m_owner)
	{
		CellProperty const * cell = m_owner->getCellProperty();
		if (cell)
			return cell;
		else
			return m_owner->getParentCell();
	}
	else
	{
		return NULL;
	}
}

// ----------------------------------------------------------------------

bool Floor::findFloorTri ( Vector const & position_p, bool bAllowJump, FloorLocator & outLoc ) const
{
	Vector position_l = transform_p2l(position_p);

	FloorLocator testLoc(this, position_l);

	bool found = getFloorMesh()->findFloorTri(testLoc,bAllowJump,outLoc);

	outLoc.setFloor(this);

	return found;
}

// ----------------------------------------------------------------------

bool Floor::findFloorTri ( Vector const & position_p, float heightTolerance, bool bAllowJump, FloorLocator & outLoc ) const
{
	Vector position_l = transform_p2l(position_p);

	FloorLocator testLoc( this, position_l );

	bool found = getFloorMesh()->findFloorTri(testLoc,heightTolerance,bAllowJump,outLoc);

	outLoc.setFloor(this);

	return found;
}

// ----------------------------------------------------------------------

bool Floor::dropTest ( Vector const & position_p, FloorLocator & outLoc ) const
{
	return dropTest(position_p,ConfigSharedCollision::getHopHeight(),outLoc);
}

// ----------

bool Floor::dropTest ( Vector const & position_p, float hopHeight, FloorLocator & outLoc ) const
{
	Vector position_l = transform_p2l(position_p);

	FloorLocator testLoc( this, position_l );

	bool dropOk = getFloorMesh()->dropTest(testLoc,hopHeight,outLoc);

	if(dropOk) outLoc.setFloor(this);

	return dropOk;
}

// ----------

bool Floor::dropTest ( Vector const & position_p, int triId, FloorLocator & outLoc ) const
{
	Vector position_l = transform_p2l(position_p);

	FloorLocator testLoc( this, position_l );

	bool dropOk = getFloorMesh()->dropTest(testLoc,triId,outLoc);

	if(dropOk) outLoc.setFloor(this);

	return dropOk;
}

// ----------------------------------------------------------------------

Vector Floor::transform_p2l ( Vector const & point ) const
{
	Vector const s = getScaleVector();
	Vector const local = getTransform_o2p().rotateTranslate_p2l(point);
	return Vector( local.x / s.x, local.y / s.y, local.z / s.z );
}

Vector Floor::transform_l2p ( Vector const & point ) const
{
	Vector const s = getScaleVector();
	Vector const scaledLocal( point.x * s.x, point.y * s.y, point.z * s.z );
	return getTransform_o2p().rotateTranslate_l2p( scaledLocal );
}

Vector Floor::rotate_p2l ( Vector const & dir ) const
{
	return getTransform_o2p().rotate_p2l(dir);
}

Vector Floor::rotate_l2p ( Vector const & dir ) const
{
	return getTransform_o2p().rotate_l2p(dir);
}

// ----------------------------------------------------------------------

PathWalkResult Floor::canMove ( FloorLocator const & testLoc, 
                                Vector const & worldDelta,
								int ignoreTriId,
								int ignoreEdge,
                                FloorLocator & result ) const
{
	Vector delta = rotate_p2l(worldDelta);

	PathWalkResult walkResult = getFloorMesh()->pathWalkCircle(testLoc,delta,ignoreTriId,ignoreEdge,result);
	
	result.setFloor(this);

	result.setContactNormal( rotate_l2p(result.getContactNormal()) );

	return walkResult;
}

// ----------------------------------------------------------------------

PathWalkResult Floor::moveLocator ( FloorLocator const & startLoc, Vector const & worldGoal, int ignoreId, int ignoreEdge, FloorLocator & outLoc ) const
{
	UNREF(ignoreId);
	UNREF(ignoreEdge);

	Vector goal = transform_p2l(worldGoal);

	Vector startPoint = startLoc.getPosition_l();

	Vector delta = goal - startPoint;

	delta.y = 0.0f;

	int hitTriId = -1;
	int hitEdge = -1;
	float hitTime = 0.0f;

	PathWalkResult result = getFloorMesh()->pathWalkPoint(startLoc,delta,hitTriId,hitEdge,hitTime);

	if((result == PWR_WalkOk) || (result == PWR_InContact))
	{
		outLoc = FloorLocator(this,startPoint+delta,hitTriId,0.0f,startLoc.getRadius());
	}
	else
	{
		outLoc = FloorLocator(this,startPoint+delta,-1,0.0f,startLoc.getRadius());
	}

	if(outLoc.getId() != -1)
	{
		Plane3d plane = getFloorMesh()->getTriangle( outLoc.getId() ).getPlane();

		Vector snapPoint;
		float dummy;

		Vector snapDir = getLocalDropDirection();
		IGNORE_RETURN( snapDir.normalize() );

		if( Intersect3d::IntersectLinePlaneUnsided( Line3d( outLoc.getPosition_l(), snapDir ), plane, snapPoint, dummy ) )
		{
			outLoc.setPosition_l(snapPoint);
		}
	}

	{
		Floor const * floor = outLoc.getFloor();

		FloorLocator temp;

		if((outLoc.getId() != -1) && floor && !floor->dropTest(outLoc.getPosition_p(),outLoc.getId(),temp))
		{
			outLoc.detach();
		}
	}


	return result;
}

// ----------------------------------------------------------------------

bool Floor::findClosestLocation ( Vector const & position_p, FloorLocator & outLoc ) const
{
	Vector position_l = transform_p2l(position_p);

	FloorLocator testLoc(this,position_l);
	
	bool findOK = getFloorMesh()->findClosestLocation(testLoc,outLoc);
	
	outLoc.setFloor(this);
	
	return findOK;
}

// ----------------------------------------------------------------------

bool    Floor::findElevatorNeighbors( FloorLocator const & testLoc,
                                      FloorLocator & outClosestAbove,
                                      FloorLocator & outClosestBelow ) const
{
	if(testLoc.getFloor() != this)
	{
	    FLOOR_WARNING("Floor::findElevatorNeighbors - Test location isn't on this floor\n");
	}
	
	Vector dropDir = getLocalDropDirection();
	IGNORE_RETURN( dropDir.normalize() );
	Line3d line( testLoc.getPosition_l(), dropDir );
	int ignoreId = testLoc.getId();
	
	bool found = getFloorMesh()->findClosestPair(line,ignoreId,outClosestBelow,outClosestAbove);
	
	outClosestAbove.setFloor(this);
	outClosestBelow.setFloor(this);
	
	return found;
}

// ----------------------------------------------------------------------

void Floor::drawDebugShapes ( DebugShapeRenderer * renderer, bool drawExtent ) const
{
	UNREF(renderer);
	UNREF(drawExtent);
#ifdef _DEBUG

	getFloorMesh()->drawDebugShapes(renderer,drawExtent);

#endif
}

// ----------------------------------------------------------------------

Sphere Floor::getBoundingSphere_l ( void ) const
{
	if(getExtentDirty()) updateExtent();

#ifdef _DEBUG

	if(ConfigSharedCollision::getEnableCollisionValidate())
	{
		Sphere temp = getExtent_l()->getBoundingSphere();

		if(!CollisionUtils::epsilonEqual(temp,m_sphere_l,0.001f))
		{
			DEBUG_WARNING(true,("Floor::getBoundingSphere_l - Current sphere != cached sphere"));
		}
	}

#endif
	
	return m_sphere_l;
}

// ----------

Sphere Floor::getBoundingSphere_w ( void ) const
{
	if(getExtentDirty()) updateExtent();

#ifdef _DEBUG

	if(ConfigSharedCollision::getEnableCollisionValidate())
	{
		Vector worldCenter = getTransform_o2w().rotateTranslate_l2p(m_sphere_l.getCenter());

		Sphere temp = Sphere(worldCenter,m_sphere_l.getRadius());

		if(!CollisionUtils::epsilonEqual(temp,m_sphere_w,0.001f))
		{
			DEBUG_WARNING(true,("Floor::getBoundingSphere_w - Current sphere != cached sphere"));
		}
	}

#endif

	return m_sphere_w;
}

// ----------------------------------------------------------------------

bool Floor::findEntryPoint ( FloorLocator const & inStartLoc, Vector const & delta, bool useRadius, FloorLocator & outLoc ) const
{
	FloorLocator startLoc = inStartLoc;

	startLoc.setFloor(this);

	Vector localDelta = rotate_p2l(delta);

	bool result = getFloorMesh()->findEntrance(startLoc,localDelta,useRadius,outLoc);

	outLoc.setFloor(this);

	return result;
}

// ----------------------------------------------------------------------

bool Floor::intersectClosest ( Ray3d const & ray, FloorLocator & outLoc ) const
{
	Vector localPoint = transform_p2l(ray.getPoint());
	Vector localNormal = rotate_p2l(ray.getNormal());

	bool result = getFloorMesh()->intersectClosest( Ray3d(localPoint,localNormal), outLoc );

	outLoc.setFloor(this);

	return result;
}

bool Floor::intersectClosest ( Segment3d const & seg, FloorLocator & outLoc ) const
{
	Vector localBegin = transform_p2l(seg.getBegin());
	Vector localEnd = transform_p2l(seg.getEnd());

	bool result = getFloorMesh()->intersectClosest( Segment3d(localBegin,localEnd), outLoc );

	outLoc.setFloor(this);

	return result;
}

bool Floor::intersectClosest ( Line3d const & line, FloorLocator & outLoc ) const
{
	Vector localPoint = transform_p2l(line.getPoint());
	Vector localNormal = rotate_p2l(line.getNormal());

	bool result = getFloorMesh()->intersectClosest( Line3d(localPoint,localNormal), outLoc );

	outLoc.setFloor(this);

	return result;
}

// ----------------------------------------------------------------------

bool Floor::getGoodLocation ( float radius, Vector & outLoc ) const
{
	if(getFloorMesh()->getGoodLocation(radius,outLoc))
	{
		outLoc = transform_l2p(outLoc);

		return true;
	}
	else
	{
		return false;
	}
}

// ----------------------------------------------------------------------

bool Floor::getDistanceUncrossable2d ( Vector const & position_w, float maxDistance, float & outDistance, FloorEdgeId & outEdgeId ) const
{
	Vector position_p = CollisionUtils::transformFromWorld(position_w,getCell());
	Vector position_l = transform_p2l(position_p);

	return getFloorMesh()->getDistanceUncrossable2d(position_l,maxDistance,outDistance,outEdgeId);
}

// ----------------------------------------------------------------------

NetworkId const & Floor::getId() const
{
	CellProperty const * const cellProperty = getCell();

	return (cellProperty != NULL) ? cellProperty->getOwner().getNetworkId() : NetworkId::cms_invalid;
}

// ======================================================================
