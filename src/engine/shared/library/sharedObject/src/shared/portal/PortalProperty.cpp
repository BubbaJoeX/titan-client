// ======================================================================
//
// PortalProperty.cpp
// Copyright 2001 Sony Online Entertainment
// All Rights Reserved.
//
// ======================================================================

#include "sharedObject/FirstSharedObject.h"
#include "sharedObject/PortalProperty.h"

#include "sharedCollision/DoorObject.h"
#include "sharedCollision/BoxExtent.h"
#include "sharedCollision/ComponentExtent.h"
#include "sharedCollision/DetailExtent.h"
#include "sharedCollision/Distance3d.h"
#include "sharedCollision/ExtentList.h"
#include "sharedCollision/Floor.h"
#include "sharedCollision/FloorManager.h"
#include "sharedCollision/FloorLocator.h"
#include "sharedCollision/FloorMesh.h"
#include "sharedCollision/FloorTri.h"
#include "sharedDebug/DataLint.h"
#include "sharedFile/Iff.h"
#include "sharedObject/Appearance.h"
#include "sharedObject/AppearanceTemplateList.h"
#include "sharedObject/CellProperty.h"
#include "sharedObject/ConfigSharedObject.h"
#include "sharedObject/ContainedByProperty.h"
#include "sharedObject/Portal.h"
#include "sharedObject/PortalPropertyTemplate.h"
#include "sharedObject/PortalPropertyTemplateList.h"
#include "sharedFoundation/Crc.h"
#include "sharedMath/AxialBox.h"
#include "sharedMath/IndexedTriangleList.h"
#include "sharedMath/Plane3d.h"
#include "sharedMath/Segment3d.h"
#include "sharedMath/Triangle3d.h"

#include <algorithm>
#include <limits>
#include <cstdio>
#include <set>

// ======================================================================

const Tag TAG_PRTP = TAG(P,R,T,P);

// ======================================================================

PortalProperty::BeginCreateObjectFunction  PortalProperty::ms_beginCreateObjectFunction;
PortalProperty::EndCreateObjectFunction    PortalProperty::ms_endCreateObjectFunction;

// ======================================================================

void PortalProperty::install(BeginCreateObjectFunction beginCreateObjectFunction, EndCreateObjectFunction endCreateObjectFunction)
{
	ms_beginCreateObjectFunction = beginCreateObjectFunction;
	ms_endCreateObjectFunction = endCreateObjectFunction;
	Portal::install();
}

// ----------------------------------------------------------------------

PropertyId PortalProperty::getClassPropertyId()
{
	return PROPERTY_HASH(PortalObjectProperty, 0x939616B9);
}

// ======================================================================

PortalProperty::PortalProperty(Object &owner, const char *fileName)
: Container(getClassPropertyId(), owner),
	m_template(nullptr),
	m_cellList(new CellList),
	m_fixupList(nullptr),
	m_hasPassablePortalToParentCell(false),
	m_graftedCellMap(new GraftedCellMap),
	m_dynamicRoomGrafts(new DynamicRoomGraftList),
	m_customSockets(new CustomSocketList),
	m_bridgeSegments(new BridgeSegmentList),
	m_runtimePortalTemplates(0)
{
#ifdef _DEBUG
	DataLint::pushAsset(fileName);
#endif // _DEBUG

	m_template = PortalPropertyTemplateList::fetch(CrcLowerString(fileName));
	m_cellList->resize(static_cast<CellList::size_type>(m_template->getNumberOfCells()), nullptr);

#ifdef _DEBUG
	DataLint::popAsset();
#endif // _DEBUG
}

// ----------------------------------------------------------------------

PortalProperty::~PortalProperty()
{
	if (m_graftedCellMap)
	{
		for (GraftedCellMap::iterator i = m_graftedCellMap->begin(); i != m_graftedCellMap->end(); ++i)
		{
			if (i->second.donorTemplate)
				i->second.donorTemplate->release();
		}
		delete m_graftedCellMap;
		m_graftedCellMap = 0;
	}
	delete m_dynamicRoomGrafts;
	m_dynamicRoomGrafts = 0;
	delete m_customSockets;
	m_customSockets = 0;
	delete m_bridgeSegments;
	m_bridgeSegments = 0;
	if (m_runtimePortalTemplates)
	{
		for (size_t i = 0; i < m_runtimePortalTemplates->size(); ++i)
			delete (*m_runtimePortalTemplates)[i];
		delete m_runtimePortalTemplates;
		m_runtimePortalTemplates = 0;
	}

	delete m_cellList;
	delete m_fixupList;
	if (m_template)
	{
		m_template->release();
		m_template = 0;
	}

	m_fixupList = 0;
}

// ----------------------------------------------------------------------

void PortalProperty::initializeFirstTimeObject()
{
	// the first cell is the parent cell, so don't actually create an object/cell pair for it
	int numberOfCells = m_template->getNumberOfCells();
	for (int i = 1; i < numberOfCells; ++i)
	{
		ContainerErrorCode tmp = CEC_Success;
		Object *object = ms_beginCreateObjectFunction(i);
		IGNORE_RETURN(addToContents(*object, tmp));
		cellLoaded(i, *object, false);
		if (ms_endCreateObjectFunction)
			ms_endCreateObjectFunction(object);
	} //lint !e429 // Custodial pointer not freed or returned
}

// ----------------------------------------------------------------------

void PortalProperty::clientSinglePlayerInitializeFirstTimeObject()
{
	// the first cell is the parent cell, so don't actually create an object/cell pair for it
	int numberOfCells = m_template->getNumberOfCells();
	for (int i = 1; i < numberOfCells; ++i)
	{
		ContainerErrorCode tmp = CEC_Success;
		Object *object = ms_beginCreateObjectFunction(i);
		IGNORE_RETURN(addToContents(*object, tmp));
		if (ms_endCreateObjectFunction)
			ms_endCreateObjectFunction(object);
	} //lint !e429 // Custodial pointer not freed or returned
}

// ----------------------------------------------------------------------

const PortalPropertyTemplate &PortalProperty::getPortalPropertyTemplate() const
{
	NOT_NULL(m_template);
	return *m_template;
}

// ----------------------------------------------------------------------

void PortalProperty::addToWorld()
{
#ifdef _DEBUG
	{
		int unloaded = 0;
		int const numberOfCells = static_cast<int>(m_cellList->size());
		for (int i = 1; i < numberOfCells; ++i)
			if ((*m_cellList)[static_cast<CellList::size_type>(i)] == nullptr)
			{
				WARNING(true, ("cell %d/%d not loaded", i, numberOfCells));
				++unloaded;
			}
		DEBUG_FATAL(unloaded , ("%d/%d cells not loaded", unloaded, numberOfCells-1));
	}
#endif


	CellProperty *parentCell = getOwner().getParentCell();

	// create the portals that lead into this portal property
	PortalList *portalList = m_template->createExteriorPortalList(parentCell, &getOwner());

	// now that all the portals have been created, let every portal know who its neighbor is
	PortalPropertyTemplate::PortalOwnersList const &portalOwnersList = m_template->getPortalOwnersList();
	PortalPropertyTemplate::PortalOwnersList::const_iterator const iEnd = portalOwnersList.end();
	for (PortalPropertyTemplate::PortalOwnersList::const_iterator i = portalOwnersList.begin(); i != iEnd; ++i)
	{
		CellProperty *cell[2];
		Portal *portal[2];

		for (int j = 0; j < 2; ++j)
		{
			const int cellIndex = i->owners[j].cell;
			const int portalIndex = i->owners[j].portal;

			if (cellIndex == 0)
			{
				cell[j] = parentCell;
				portal[j] = (*portalList)[static_cast<PortalList::size_type>(portalIndex)];
				if (portal[j]->isPassable())
					m_hasPassablePortalToParentCell = true;
			}
			else
			{
				cell[j] = (*m_cellList)[static_cast<CellList::size_type>(cellIndex)];
				portal[j] = cell[j]->getPortal(portalIndex);
			}
		}

		portal[0]->setNeighbor(portal[1]);
		portal[1]->setNeighbor(portal[0]);
	}

	parentCell->attach(*this, portalList);
	Container::addToWorld();
}

// ----------------------------------------------------------------------

void PortalProperty::removeFromWorld()
{
	Container::removeFromWorld();
	CellProperty *parentCell = getOwner().getParentCell();
	parentCell->detach(*this);
}

// ----------------------------------------------------------------------
	
bool PortalProperty::serverEndBaselines(int serverObjectCrc, std::vector<Object*> &unfixables, bool authoritative)
{
	bool retval = false;
	ContainerErrorCode tmp = CEC_Success;

	// check if the CRC has changed cells
	const int crc = m_template->getCrc();
	if (authoritative && (crc == 0 || serverObjectCrc != crc))
	{
		retval = true;

		// create any missing cells
		uint const numberOfCells = m_cellList->size();
		for (uint i = 1; i < numberOfCells; ++i)
		{
			if ((*m_cellList)[i] == nullptr)
			{
				Object *object = ms_beginCreateObjectFunction(static_cast<int>(i));
				IGNORE_RETURN(addToContents(*object, tmp));
				cellLoaded(static_cast<int>(i), *object, false);
				if (ms_endCreateObjectFunction)
					ms_endCreateObjectFunction(object);
			} //lint !e429 // Custodial pointer not freed or returned
		}

		//-- jww: this fixup code is broken
		//-- fixupObject() causes the fixedup object to get added to the world before its parent has been
		//-- for now, we won't queue anything for fixup

#if 0	//TODO: see above
		// go through the cells and remove objects 
		for (size_t cell=1 ; cell< numberOfCells; ++cell)
		{
			for (ContainerIterator cellContents=(*m_cellList)[cell]->begin(); cellContents != (*m_cellList)[cell]->end(); ++cellContents)
			{
				Object *obj = (*cellContents).getObject();
				if (obj)
				{
					queueObjectForFixup(*obj);
					IGNORE_RETURN((*m_cellList)[cell]->remove(cellContents, tmp)); // does not invalidate the iterator
				}
			}
		}

#endif

	}
#ifdef _DEBUG
	else
	{
		if (ConfigSharedObject::getValidateCellContentsAttached())
		{
			// make sure the contents of every cell is attached to the cell
			uint const numberOfCells = m_cellList->size();
			for (size_t cellIndex = 1 ; cellIndex < numberOfCells; ++cellIndex)
			{
				CellProperty *cell = (*m_cellList)[cellIndex];
				DEBUG_WARNING(! cell, ("Expected to get a cell, none was returned"));
				if(cell)
				{
					Object const * const cellObject = &cell->getOwner();
					for (ContainerIterator cellContents=cell->begin(); cellContents != cell->end(); ++cellContents)
					{
						Object *obj = (*cellContents).getObject();
						DEBUG_FATAL(obj && obj->getAttachedTo() != cellObject, ("Cell content not attached to cell"));
						if (obj && obj->getAttachedTo() != cellObject)
						{
							WARNING(true, ("CellProblem Cell content not attached to cell"));
						}
					}
				}
			}
		}
	}
#endif

	// For each object, find the appropriate cell and place the object in it
	// Note that objects can be put in m_fixupList from other places, not just the above code)
	if (m_fixupList)
	{
		for (FixupList::iterator fixRec=(*m_fixupList).begin(); fixRec!=(*m_fixupList).end(); ++fixRec)
		{
			// put object into appropriate cell
			if (!fixupObject(*((*fixRec).m_obj), (*fixRec).m_transform))
				unfixables.push_back((*fixRec).m_obj);
		}

		delete m_fixupList;
		m_fixupList = 0;
	}
	return retval;
}

// ----------------------------------------------------------------------

/**
 * Given an object, attempt to put it in the POB in the best cell.
 * This function is used when the POB has changed or for player logins.
 * @param object            The object to place.
 * @param intendedTransform Where the object should be placed, relative to the POB.
 *                          May adjust the postion in the transform to make sure
 *                          the object is on a floor, but will preserve the rotation.
 * @return true on success, false if the object cannot be placed.
 * @see PortalProperty::serverEndBaselines
 */
bool PortalProperty::fixupObject (Object &object, Transform intendedTransform)
{
	Object *cell;
	Vector newPosition;
	if (findContainingCell(intendedTransform.getPosition_p(),newPosition,cell))
	{
		CellProperty *cellProperty = cell->getCellProperty();
		if (cellProperty)
		{
			//FIX HERE.  Set the transform before calling add object_w which adds it to the world.
			cellProperty->addObject_w(object);
			intendedTransform.setPosition_p(newPosition);
			object.setTransform_o2p(intendedTransform);
			if (!object.isInWorld())
				object.addToWorld();
			
			object.pobFixupComplete();
			return true;
		}
		else
		{
			WARNING_STRICT_FATAL(true,("findContainingCell returned object %s, which that lacks a cellProperty.\n",cell->getNetworkId().getValueString().c_str()));
		}
	}
	
	return false;
}

// ----------------------------------------------------------------------

/**
 * @param buildingPos - Position inside the building we're trying to find a cell for
 *
 * @param outPos - Position on a floor in the building that's close to the test position
 * @param outCell - Cell object in the building the test position is in
 *
 * @return - True if we found a position, false if not.
 */
bool PortalProperty::findContainingCell(Vector const &buildingPos, Vector &outPos, Object *&outCell) 
{
	Object &buildingObject = getOwner();
	
    // ----------
    // Object is a portallized object. 
    // Search through its cells to find the floor closest to the test point

    int minChild = -1;
    Vector minPos = Vector::zero;
    float minDist = REAL_MAX;

    for(int i = 0; i < buildingObject.getNumberOfChildObjects(); i++)
    {
        // ----------
        // Skip all child objects that aren't cells or that don't have a floor

        Object const * const childObject = buildingObject.getChildObject(i);
        if (!childObject) 
			continue;

        CellProperty const * const cellProperty = childObject->getCellProperty();
        if (!cellProperty) 
			continue;

        Floor const * const floor = cellProperty->getFloor();
        if (!floor) 
			continue;

        // ----------
        // Found a cell with a floor

        FloorLocator loc;

        if(floor->findClosestLocation(buildingPos,loc))
        {
            Vector floorPos = loc.getPosition_p();
        
            float dist = (floorPos - buildingPos).magnitude();

            if(dist < minDist)
            {
                minChild = i;
                minPos = floorPos;
                minDist = dist;
            }
        }
    }

    if(minChild != -1)
    {
        outPos.x = minPos.x;
		outPos.z = minPos.z;
		outPos.y = std::max(minPos.y,buildingPos.y);

        outCell = buildingObject.getChildObject(minChild);

        return true;
    }
    else
    {
        return false;
    }
}


// ----------------------------------------------------------------------

bool PortalProperty::mayAdd(const Object &, ContainerErrorCode& error) const
{
	error = CEC_Success;
	return true;
}

// ----------------------------------------------------------------------

bool PortalProperty::remove(Object &item, ContainerErrorCode& error)
{
	return Container::remove(item, error);
}

// ----------------------------------------------------------------------

bool PortalProperty::remove(ContainerIterator &pos, ContainerErrorCode& error)
{
	return Container::remove(pos, error);
}

// ----------------------------------------------------------------------

int  PortalProperty::getTypeId() const
{
	return static_cast<int>(TAG_PRTP);
}

// ----------------------------------------------------------------------

void PortalProperty::debugPrint(std::string &buffer) const
{
	char tempBuffer[1024];

	buffer += "====[BEGIN: portal property]====\n";

		sprintf(tempBuffer, "container id [%s].\n", getOwner().getNetworkId().getValueString().c_str());
		buffer += tempBuffer;

		buffer += "embedding container contents now:\n";
		Container::debugPrint(buffer);

	buffer += "====[END:   portal property]====\n";
}

// ----------------------------------------------------------------------

void PortalProperty::createAppearance()
{
	Appearance * const appearance = AppearanceTemplateList::createAppearance(m_template->getExteriorAppearanceName());
	if (appearance != nullptr) {
		appearance->setShadowBlobAllowed();
		getOwner().setAppearance(appearance);
	} else {
		DEBUG_WARNING(true, ("FIX ME: Appearance template for PortalProperty::createAppearance missing"));
	}
}

// ----------------------------------------------------------------------

char const *PortalProperty::getExteriorFloorName() const
{
	return m_template->getExteriorFloorName();
}

// ----------------------------------------------------------------------

namespace PortalPropertyNamespace
{
	Vector computeCellFloorCenter(CellProperty const & cell)
	{
		Floor const * const floor = cell.getFloor();
		if (!floor)
			return Vector::zero;

		FloorMesh const * const floorMesh = floor->getFloorMesh();
		if (!floorMesh)
			return Vector::zero;

		AxialBox box;
		for (int tri = 0; tri < floorMesh->getTriCount(); ++tri)
		{
			Triangle3d const T = floorMesh->getTriangle(tri);
			for (int c = 0; c < 3; ++c)
				box.add(T.getCorner(c));
		}
		return box.getCenter();
	}

	void ensureTransformRightHanded(Transform & transform_o2p)
	{
		Vector const I = transform_o2p.getLocalFrameI_p();
		Vector const J = transform_o2p.getLocalFrameJ_p();
		Vector const K = transform_o2p.getLocalFrameK_p();
		if (I.cross(J).dot(K) >= 0.0f)
			return;

		Vector const pos = transform_o2p.getPosition_p();
		transform_o2p.setLocalFrameIJK_p(-I, J, K);
		transform_o2p.setPosition_p(pos);
	}

	void ensurePortalTransformPointsOutward(CellProperty const & cell, Transform & portalTransform_o2p)
	{
		Vector const cellCenter_cell = computeCellFloorCenter(cell);
		Transform const cell_o2p = cell.getOwner().getTransform_o2p();
		Vector const center_p = cell_o2p.rotateTranslate_l2p(cellCenter_cell);
		Vector const portalPos_p = portalTransform_o2p.getPosition_p();
		Vector toPortal = portalPos_p - center_p;
		toPortal.y = 0.0f;

		Vector axisI = portalTransform_o2p.getLocalFrameI_p();
		Vector axisK = portalTransform_o2p.getLocalFrameK_p();
		axisK.y = 0.0f;
		if (toPortal.normalize() > 0.01f && axisK.normalize() > 0.01f && axisK.dot(toPortal) < 0.0f)
		{
			axisI = -axisI;
			axisK = -axisK;
			portalTransform_o2p.setLocalFrameIJK_p(axisI, Vector(0.0f, 1.0f, 0.0f), axisK);
		}
	}

	int findBestDonorTemplatePortal(
		PortalPropertyTemplateCell const &donorCell,
		Transform const &desiredPortal_building,
		Transform const &hostPortal_building,
		int preferredIndex)
	{
		PortalPropertyTemplateCell::PortalPropertyTemplateCellPortalList const *const portalList = donorCell.getPortalList();
		if (!portalList || portalList->empty())
			return -1;

		Vector const hostNormal = hostPortal_building.getLocalFrameK_p();
		Vector const hostPos = hostPortal_building.getPosition_p();

		int bestIndex = -1;
		float bestScore = -std::numeric_limits<float>::max();

		for (size_t portalIndex = 0; portalIndex < portalList->size(); ++portalIndex)
		{
			PortalPropertyTemplateCellPortal const *const portalTemplate = (*portalList)[portalIndex];
			if (!portalTemplate || !portalTemplate->isPassable())
				continue;

			Transform const donorPortal_cell = portalTemplate->getDoorTransform(false);
			Transform invDonorPortal;
			invDonorPortal.invert(donorPortal_cell);

			Transform graftTransform;
			graftTransform.multiply(desiredPortal_building, invDonorPortal);

			Vector const toGraft = graftTransform.getPosition_p() - hostPos;
			float score = toGraft.dot(hostNormal);
			if (static_cast<int>(portalIndex) == preferredIndex)
				score += 0.25f;

			if (score > bestScore)
			{
				bestScore = score;
				bestIndex = static_cast<int>(portalIndex);
			}
		}

		if (bestIndex >= 0)
			return bestIndex;

		if (preferredIndex >= 0 && preferredIndex < static_cast<int>(portalList->size()))
			return preferredIndex;

		return 0;
	}

	int resolveTemplateCellPortalIndex(PortalPropertyTemplateCell const &donorCell, int preferredIndex)
	{
		PortalPropertyTemplateCell::PortalPropertyTemplateCellPortalList const *const portalList = donorCell.getPortalList();
		if (!portalList || portalList->empty())
			return -1;

		if (preferredIndex >= 0 && preferredIndex < static_cast<int>(portalList->size()))
			return preferredIndex;

		return 0;
	}
}

using namespace PortalPropertyNamespace;

// ----------------------------------------------------------------------

int PortalProperty::resolveCellPortalIndex(CellProperty const *cell, int preferredIndex)
{
	if (!cell)
		return -1;

	if (preferredIndex >= 0 && const_cast<CellProperty *>(cell)->getPortal(preferredIndex))
		return preferredIndex;

	int const portalCount = cell->getPortalCount();
	for (int portalIndex = 0; portalIndex < portalCount; ++portalIndex)
	{
		Portal *const portal = const_cast<CellProperty *>(cell)->getPortal(portalIndex);
		if (portal && portal->isPassable())
			return portalIndex;
	}

	return -1;
}

// ----------------------------------------------------------------------

void PortalProperty::cellLoaded(int cellIndex, Object &cellObject, bool shouldCreateAppearance)
{
	DEBUG_FATAL(cellIndex < 1 || cellIndex >= static_cast<int>(m_cellList->size()), ("invalid cell index %d/%d", cellIndex, static_cast<int>(m_cellList->size())));

	CellProperty *const existingCell = (*m_cellList)[static_cast<CellList::size_type>(cellIndex)];
	if (existingCell)
	{
		if (&existingCell->getOwner() == &cellObject)
			return;

		WARNING(true, ("CellProblem for portal %s cell index %d already loaded", getOwner().getNetworkId().getValueString().c_str(), cellIndex));
		return;
	}

	// attach the cell as a child of the portal object
	cellObject.attachToObject_p(&getOwner(), false);

	// get the cell property from the cell object
	CellProperty *cell = cellObject.getCellProperty();

	// store that
	(*m_cellList)[static_cast<CellList::size_type>(cellIndex)] = cell;

	// let the cell know who its portal object is, and what its portal data is
	cell->initialize(*this, cellIndex, shouldCreateAppearance);
}

// ----------------------------------------------------------------------

int PortalProperty::getNumberOfCells() const
{
	return static_cast<int>(m_cellList->size());
}

// ----------------------------------------------------------------------

int PortalProperty::getBaseTemplateCellCount() const
{
	return m_template ? m_template->getNumberOfCells() : 0;
}

// ----------------------------------------------------------------------

namespace PortalPropertyCellNamespace
{
	bool isCellOwnerValid(CellProperty * cell)
	{
		if (!cell)
			return false;

#if defined(PLATFORM_WIN32)
		__try
		{
			Object &cellOwner = cell->getOwner();
			return cellOwner.isInitialized() && cellOwner.getCellProperty() == cell;
		}
		__except(EXCEPTION_EXECUTE_HANDLER)
		{
			return false;
		}
#else
		Object &cellOwner = cell->getOwner();
		return cellOwner.isInitialized() && cellOwner.getCellProperty() == cell;
#endif
	}

	bool isCellOwnerValid(CellProperty const * cell)
	{
		return isCellOwnerValid(const_cast<CellProperty *>(cell));
	}
}

// ----------------------------------------------------------------------

CellProperty *PortalProperty::getCell(int index)
{
	if (!m_cellList || index < 1 || index >= static_cast<int>(m_cellList->size()))
		return 0;

	CellProperty *cell = (*m_cellList)[static_cast<CellList::size_type>(index)];
	if (!cell)
		return 0;

	if (!PortalPropertyCellNamespace::isCellOwnerValid(cell))
	{
		(*m_cellList)[static_cast<CellList::size_type>(index)] = 0;
		return 0;
	}

	return cell;
}

// ----------------------------------------------------------------------

const CellProperty *PortalProperty::getCell(int index) const
{
	if (!m_cellList || index < 1 || index >= static_cast<int>(m_cellList->size()))
		return 0;

	CellProperty const *const cell = (*m_cellList)[static_cast<CellList::size_type>(index)];
	if (!cell)
		return 0;

	if (!PortalPropertyCellNamespace::isCellOwnerValid(cell))
		return 0;

	return cell;
}

// ----------------------------------------------------------------------

const PortalProperty::CellNameList &PortalProperty::getCellNames() const
{
	return m_template->getCellNames();
}

// ----------------------------------------------------------------------

CellProperty *PortalProperty::getCell(const char *desiredCellName)
{
	const int numberOfCells = getNumberOfCells();
	for (int i = 1; i < numberOfCells; ++i)
	{
		PortalPropertyTemplateCell const &cell = getCellTemplate(i);
		char const * const cellName = cell.getName();

		if (cellName && strcmp(desiredCellName, cellName) == 0)
			return (*m_cellList)[static_cast<CellList::size_type>(i)];
	}

	return nullptr;
}

// ----------------------------------------------------------------------

const CellProperty *PortalProperty::getCell(const char *cellName) const
{
	return const_cast<PortalProperty*>(this)->getCell(cellName);
}

// ----------------------------------------------------------------------

const char *PortalProperty::getCellAppearanceName(int index) const
{
	return getCellTemplate(index).getAppearanceName();
}

// ----------------------------------------------------------------------

/** 
 * Queue an object to be placed in the appropriate cell after EndBaseslines.
 * Use this function during database load when you find an object that cannot
 * be placed in a cell (due to deleted cells, etc.).  After the POB receives
 * EndBaselines, it will go through every object queued by this function and
 * find an appropriate cell for it.
 *
 * If the object is already in an (incorrect) cell, do not remove the object
 * from the cell before calling this function, because removing the object
 * will change its transform.  This function saves the transform, so you may
 * remove the object after calling this function.
 */

void PortalProperty::queueObjectForFixup(Object &object)
{
	FixupRec temp;
	temp.m_obj = &object;
	temp.m_transform = object.getTransform_o2p();  // removing the object changes its transform, so we save it now

	if (!m_fixupList)
		m_fixupList = new FixupList;
	m_fixupList->push_back(temp);
}

// ----------------------------------------------------------------------

int PortalProperty::getCrc() const
{
	return m_template->getCrc();
}

// ----------------------------------------------------------------------

const char *PortalProperty::getPobName() const
{
	return m_template->getCrcString().getString();
}

// ----------------------------------------------------------------------

const char *PortalProperty::getPobShortName() const
{
	return m_template ? m_template->getShortName().getString() : "";
}

// ----------------------------------------------------------------------

Transform const PortalProperty::getEjectionLocationTransform() const
{
	return m_template->getEjectionLocationTransform();
}

// ----------------------------------------------------------------------

bool PortalProperty::isContentItemObservedWith(Object const &item) const
{
	// Immediate contents of portallized objects are always observed with them.
	UNREF(item);
	return true;
}

// ----------------------------------------------------------------------

bool PortalProperty::isContentItemExposedWith(Object const &item) const
{
	// Portallized objects always expose their immediate contents with themselves.
	UNREF(item);
	return true;
}

// ----------------------------------------------------------------------

bool PortalProperty::canContentsBeObservedWith() const
{
	return true;
}

// ----------------------------------------------------------------------

bool PortalProperty::hasPassablePortalToParentCell() const
{
	return m_hasPassablePortalToParentCell;
}

// ----------------------------------------------------------------------
// position_l is in the space of the building.

CellProperty const * PortalProperty::findContainingCell(Vector const & position_l) const
{
	CellProperty const * cell = CellProperty::getWorldCellProperty();

	Object const & buildingObject = getOwner();
	
    float minDist = std::numeric_limits<float>::max();
	
    for (int i = 0; i < buildingObject.getNumberOfChildObjects(); i++)
    {
        Object const * const childObject = buildingObject.getChildObject(i);
        if (!childObject) 
			continue;
		
        CellProperty const * const cellProperty = childObject->getCellProperty();
        if (!cellProperty) 
			continue;
		
        Floor const * const floor = cellProperty->getFloor();
        if (!floor) 
			continue;
		
        FloorLocator loc;
	
        if (floor->findClosestLocation(position_l, loc))
        {
			float const distance = (position_l - loc.getPosition_p()).magnitudeSquared();
		
            if(distance < minDist)
            {
				minDist = distance;

				PortalProperty const * const portal = childObject->getPortalProperty();
				if (portal) 
				{
					CellProperty const * const childCellProperty = portal->findContainingCell(loc.getPosition_p());
					cell = childCellProperty && (childCellProperty != CellProperty::getWorldCellProperty()) ? childCellProperty : cellProperty;
				}
				else
				{
					cell = cellProperty;
				}
            }
        }
    }
	
	return cell;
}

// ----------------------------------------------------------------------

PortalPropertyTemplateCell const &PortalProperty::getCellTemplate(int cellIndex) const
{
	NOT_NULL(m_template);
	VALIDATE_RANGE_INCLUSIVE_EXCLUSIVE(0, cellIndex, getNumberOfCells());

	if (cellIndex < m_template->getNumberOfCells())
		return m_template->getCell(cellIndex);

	NOT_NULL(m_graftedCellMap);
	GraftedCellMap::const_iterator const i = m_graftedCellMap->find(cellIndex);
	FATAL(i == m_graftedCellMap->end() || !i->second.donorTemplate, ("PortalProperty::getCellTemplate - missing graft for cell %d", cellIndex));
	return i->second.donorTemplate->getCell(i->second.donorCellIndex);
}

// ----------------------------------------------------------------------

bool PortalProperty::isGraftedCell(int cellIndex) const
{
	return m_graftedCellMap && m_graftedCellMap->find(cellIndex) != m_graftedCellMap->end();
}

// ----------------------------------------------------------------------

int PortalProperty::reserveGraftedCellSlot(char const *donorPobName, int donorCellIndex)
{
	NOT_NULL(donorPobName);
	NOT_NULL(m_graftedCellMap);
	NOT_NULL(m_cellList);

	PortalPropertyTemplate const *const donorTemplate = PortalPropertyTemplateList::fetch(CrcLowerString(donorPobName));
	if (!donorTemplate)
	{
		WARNING(true, ("PortalProperty::reserveGraftedCellSlot - failed to fetch donor POB %s", donorPobName));
		return -1;
	}

	if (donorCellIndex < 1 || donorCellIndex >= donorTemplate->getNumberOfCells())
	{
		WARNING(true, ("PortalProperty::reserveGraftedCellSlot - donor cell %d out of range for %s", donorCellIndex, donorPobName));
		donorTemplate->release();
		return -1;
	}

	m_cellList->push_back(nullptr);
	int const graftedCellIndex = static_cast<int>(m_cellList->size()) - 1;

	GraftedCellRecord record;
	record.donorTemplate = donorTemplate;
	record.donorCellIndex = donorCellIndex;
	(*m_graftedCellMap)[graftedCellIndex] = record;
	return graftedCellIndex;
}

// ----------------------------------------------------------------------

bool PortalProperty::addCellObject(Object &cellObject, ContainerErrorCode &error)
{
	return addToContents(cellObject, error) >= 0;
}

// ----------------------------------------------------------------------

bool PortalProperty::ensureGraftedCellSlot(int graftedCellIndex, char const *donorPobName, int donorCellIndex)
{
	NOT_NULL(donorPobName);
	NOT_NULL(m_graftedCellMap);
	NOT_NULL(m_cellList);

	if (graftedCellIndex < 1)
		return false;

	if (isGraftedCell(graftedCellIndex))
		return true;

	PortalPropertyTemplate const *const donorTemplate = PortalPropertyTemplateList::fetch(CrcLowerString(donorPobName));
	if (!donorTemplate)
	{
		WARNING(true, ("PortalProperty::ensureGraftedCellSlot - failed to fetch donor POB %s", donorPobName));
		return false;
	}

	if (donorCellIndex < 1 || donorCellIndex >= donorTemplate->getNumberOfCells())
	{
		WARNING(true, ("PortalProperty::ensureGraftedCellSlot - donor cell %d out of range for %s", donorCellIndex, donorPobName));
		donorTemplate->release();
		return false;
	}

	while (static_cast<int>(m_cellList->size()) <= graftedCellIndex)
		m_cellList->push_back(nullptr);

	GraftedCellRecord record;
	record.donorTemplate = donorTemplate;
	record.donorCellIndex = donorCellIndex;
	(*m_graftedCellMap)[graftedCellIndex] = record;
	return true;
}

// ----------------------------------------------------------------------

bool PortalProperty::linkCellPortals(int cellIndexA, int portalIndexA, int cellIndexB, int portalIndexB)
{
	CellProperty *const cellA = getCell(cellIndexA);
	CellProperty *const cellB = getCell(cellIndexB);
	if (!cellA || !cellB)
	{
		WARNING(true, ("PortalProperty::linkCellPortals - missing cell %d or %d", cellIndexA, cellIndexB));
		return false;
	}

	Portal *const portalA = cellA->getPortal(portalIndexA);
	Portal *const portalB = cellB->getPortal(portalIndexB);
	if (!portalA || !portalB)
	{
		WARNING(true, ("PortalProperty::linkCellPortals - missing portal %d/%d or %d/%d", cellIndexA, portalIndexA, cellIndexB, portalIndexB));
		return false;
	}

	Portal::linkNeighbors(portalA, portalB);
	portalA->refreshDpvsPortal();
	portalB->refreshDpvsPortal();
	return true;
}

// ----------------------------------------------------------------------

bool PortalProperty::unlinkCellPortal(int cellIndex, int portalIndex)
{
	CellProperty *const cell = getCell(cellIndex);
	if (!cell)
		return false;

	Portal *const portal = cell->getPortal(portalIndex);
	if (!portal)
		return false;

	portal->clearNeighbor();
	return true;
}

// ----------------------------------------------------------------------

bool PortalProperty::unlinkHostPortal(int cellIndex, int portalIndex)
{
	if (isCustomSocketIndex(portalIndex))
	{
		CustomSocket customSocket;
		if (!findCustomSocket(cellIndex, portalIndex, customSocket))
			return false;
		if (customSocket.materializedPortalIndex < 0)
			return true;
		return unlinkCellPortal(cellIndex, customSocket.materializedPortalIndex);
	}

	return unlinkCellPortal(cellIndex, portalIndex);
}

// ----------------------------------------------------------------------

void PortalProperty::unlinkAllCellPortals(int cellIndex)
{
	CellProperty *const cell = getCell(cellIndex);
	if (!cell)
		return;

	Object & cellOwner = cell->getOwner();
	if (!cellOwner.isInitialized() || cellOwner.getCellProperty() != cell)
		return;

	cell->clearAllPortalNeighbors();
}

// ----------------------------------------------------------------------

bool PortalProperty::clearLoadedCellSlot(int cellIndex)
{
	NOT_NULL(m_cellList);
	if (cellIndex < 1 || cellIndex >= static_cast<int>(m_cellList->size()))
		return false;

	(*m_cellList)[static_cast<CellList::size_type>(cellIndex)] = nullptr;
	return true;
}

// ----------------------------------------------------------------------

bool PortalProperty::releaseGraftedCellSlot(int graftedCellIndex)
{
	NOT_NULL(m_graftedCellMap);
	GraftedCellMap::iterator const i = m_graftedCellMap->find(graftedCellIndex);
	if (i == m_graftedCellMap->end())
		return false;

	if (i->second.donorTemplate)
	{
		i->second.donorTemplate->release();
		i->second.donorTemplate = 0;
	}

	IGNORE_RETURN(m_graftedCellMap->erase(i));
	return true;
}

// ----------------------------------------------------------------------

bool PortalProperty::computeGraftCellTransform(int hostCellIndex, int hostPortalIndex, char const *donorPobName, int donorCellIndex, int donorPortalIndex, Transform &outCellTransform_o2p, int *outResolvedDonorPortalIndex) const
{
	NOT_NULL(donorPobName);

	if (outResolvedDonorPortalIndex)
		*outResolvedDonorPortalIndex = -1;

	if (!getCell(hostCellIndex))
		return false;

	Transform hostPortal_building;
	if (!getPortalSocketTransform_o2p(hostCellIndex, hostPortalIndex, hostPortal_building))
		return false;

	CellProperty const * const hostCell = getCell(hostCellIndex);
	if (hostCell && !isCustomSocketIndex(hostPortalIndex))
		ensurePortalTransformPointsOutward(*hostCell, hostPortal_building);

	PortalPropertyTemplate const *const donorTemplate = PortalPropertyTemplateList::fetch(CrcLowerString(donorPobName));
	if (!donorTemplate)
		return false;

	bool ok = false;
	if (donorCellIndex >= 1 && donorCellIndex < donorTemplate->getNumberOfCells())
	{
		PortalPropertyTemplateCell const &donorCell = donorTemplate->getCell(donorCellIndex);
		PortalPropertyTemplateCell::PortalPropertyTemplateCellPortalList const *const portalList = donorCell.getPortalList();

		Transform flip;
		flip.yaw_l(PI);

		Transform desiredPortal_building;
		desiredPortal_building.multiply(hostPortal_building, flip);

		int const resolvedDonorPortal = findBestDonorTemplatePortal(
			donorCell, desiredPortal_building, hostPortal_building, donorPortalIndex);

		if (portalList && resolvedDonorPortal >= 0 && resolvedDonorPortal < static_cast<int>(portalList->size()))
		{
			Transform const donorPortal_cell = (*portalList)[static_cast<size_t>(resolvedDonorPortal)]->getDoorTransform(false);
			Transform invDonorPortal;
			invDonorPortal.invert(donorPortal_cell);

			outCellTransform_o2p.multiply(desiredPortal_building, invDonorPortal);
			ensureTransformRightHanded(outCellTransform_o2p);
			if (outResolvedDonorPortalIndex)
				*outResolvedDonorPortalIndex = resolvedDonorPortal;
			ok = true;
		}
	}

	donorTemplate->release();
	return ok;
}

// ----------------------------------------------------------------------

bool PortalProperty::computeLinkedGraftCellTransform(int hostCellIndex, int hostPortalIndex, int graftCellIndex, int graftPortalIndex, Transform &outCellTransform_o2p) const
{
	Transform hostPortal_building;
	if (!getPortalSocketTransform_o2p(hostCellIndex, hostPortalIndex, hostPortal_building))
		return false;

	CellProperty const * const hostCell = getCell(hostCellIndex);
	if (hostCell && !isCustomSocketIndex(hostPortalIndex))
		ensurePortalTransformPointsOutward(*hostCell, hostPortal_building);

	CellProperty const * const graftCell = getCell(graftCellIndex);
	if (!graftCell)
		return false;

	int const resolvedGraftPortal = PortalProperty::resolveCellPortalIndex(graftCell, graftPortalIndex);
	if (resolvedGraftPortal < 0)
		return false;

	Portal const * const graftPortal = const_cast<CellProperty *>(graftCell)->getPortal(resolvedGraftPortal);
	if (!graftPortal)
		return false;

	Transform flip;
	flip.yaw_l(PI);

	Transform desiredPortal_building;
	desiredPortal_building.multiply(hostPortal_building, flip);

	Transform const graftPortal_cell = graftPortal->getDoorTransform();
	Transform invGraftPortal;
	invGraftPortal.invert(graftPortal_cell);

	outCellTransform_o2p.multiply(desiredPortal_building, invGraftPortal);
	ensureTransformRightHanded(outCellTransform_o2p);
	return true;
}

// ----------------------------------------------------------------------

bool PortalProperty::recordDynamicRoomGraft(DynamicRoomGraft const &graft)
{
	NOT_NULL(m_dynamicRoomGrafts);
	for (DynamicRoomGraftList::iterator i = m_dynamicRoomGrafts->begin(); i != m_dynamicRoomGrafts->end(); ++i)
	{
		if (i->graftedCellIndex == graft.graftedCellIndex)
		{
			*i = graft;
			return true;
		}
	}
	m_dynamicRoomGrafts->push_back(graft);
	return true;
}

// ----------------------------------------------------------------------

bool PortalProperty::removeDynamicRoomGraft(int graftedCellIndex)
{
	NOT_NULL(m_dynamicRoomGrafts);
	for (DynamicRoomGraftList::iterator i = m_dynamicRoomGrafts->begin(); i != m_dynamicRoomGrafts->end(); ++i)
	{
		if (i->graftedCellIndex == graftedCellIndex)
		{
			IGNORE_RETURN(m_dynamicRoomGrafts->erase(i));
			return true;
		}
	}
	return false;
}

// ----------------------------------------------------------------------

void PortalProperty::clearDynamicRoomGrafts()
{
	NOT_NULL(m_dynamicRoomGrafts);
	m_dynamicRoomGrafts->clear();
}

// ----------------------------------------------------------------------

bool PortalProperty::findDynamicRoomGraftForSocket(int cellIndex, int portalIndex, DynamicRoomGraft &outGraft) const
{
	NOT_NULL(m_dynamicRoomGrafts);
	for (DynamicRoomGraftList::const_iterator i = m_dynamicRoomGrafts->begin(); i != m_dynamicRoomGrafts->end(); ++i)
	{
		if ((i->hostCellIndex == cellIndex && i->hostPortalIndex == portalIndex) ||
			(i->graftedCellIndex == cellIndex && i->graftedPortalIndex == portalIndex))
		{
			outGraft = *i;
			return true;
		}
	}
	return false;
}

// ----------------------------------------------------------------------

PortalProperty::DynamicRoomGraftList const &PortalProperty::getDynamicRoomGrafts() const
{
	NOT_NULL(m_dynamicRoomGrafts);
	return *m_dynamicRoomGrafts;
}

// ----------------------------------------------------------------------

void PortalProperty::collectPortalSockets(PortalSocketInfoList &outSockets) const
{
	outSockets.clear();
	int const cellCount = getBaseTemplateCellCount();
	for (int cellIndex = 1; cellIndex < cellCount; ++cellIndex)
	{
		CellProperty *const cell = const_cast<PortalProperty *>(this)->getCell(cellIndex);
		if (!cell)
			continue;

		Object const & cellOwner = cell->getOwner();
		if (!cellOwner.isInitialized() || cellOwner.getCellProperty() != cell)
			continue;

		int const portalCount = cell->getPortalCount();
		for (int portalIndex = 0; portalIndex < portalCount; ++portalIndex)
		{
			if (m_customSockets)
			{
				bool skipPortal = false;
				for (CustomSocketList::const_iterator it = m_customSockets->begin(); it != m_customSockets->end(); ++it)
				{
					if (it->cellIndex == cellIndex && it->materializedPortalIndex == portalIndex)
					{
						skipPortal = true;
						break;
					}
				}
				if (skipPortal)
					continue;
			}

			Portal *const portal = cell->getPortal(portalIndex);
			if (!portal)
				continue;

			PortalSocketInfo info;
			info.cellIndex = cellIndex;
			info.portalIndex = portalIndex;
			info.passable = portal->isPassable();
			info.open = (portal->getNeighbor() == 0);
			outSockets.push_back(info);
		}
	}

	if (m_customSockets)
	{
		for (CustomSocketList::const_iterator it = m_customSockets->begin(); it != m_customSockets->end(); ++it)
		{
			PortalSocketInfo info;
			info.cellIndex = it->cellIndex;
			info.portalIndex = it->socketIndex;
			info.passable = true;
			info.open = it->open;
			outSockets.push_back(info);
		}
	}

	if (m_dynamicRoomGrafts)
	{
		for (DynamicRoomGraftList::const_iterator gi = m_dynamicRoomGrafts->begin(); gi != m_dynamicRoomGrafts->end(); ++gi)
		{
			DynamicRoomGraft const & graft = *gi;
			CellProperty *const graftCell = const_cast<PortalProperty *>(this)->getCell(graft.graftedCellIndex);
			if (!graftCell)
				continue;

			Object const & graftCellOwner = graftCell->getOwner();
			if (!graftCellOwner.isInitialized() || graftCellOwner.getCellProperty() != graftCell)
				continue;

			int const graftPortalIndex = PortalProperty::resolveCellPortalIndex(graftCell, graft.graftedPortalIndex);
			if (graftPortalIndex < 0)
				continue;

			bool alreadyListed = false;
			for (size_t si = 0; si < outSockets.size(); ++si)
			{
				if (outSockets[si].cellIndex == graft.graftedCellIndex && outSockets[si].portalIndex == graftPortalIndex)
				{
					alreadyListed = true;
					break;
				}
			}
			if (alreadyListed)
				continue;

			Portal *const graftPortal = graftCell->getPortal(graftPortalIndex);
			PortalSocketInfo info;
			info.cellIndex = graft.graftedCellIndex;
			info.portalIndex = graftPortalIndex;
			info.passable = graftPortal ? graftPortal->isPassable() : true;
			info.open = !graftPortal || graftPortal->getNeighbor() == 0;
			outSockets.push_back(info);
		}
	}
}

// ----------------------------------------------------------------------

int const PortalProperty::cms_customSocketBase = 10000;

// ----------------------------------------------------------------------

int PortalProperty::allocateCustomSocketIndex() const
{
	int nextIndex = cms_customSocketBase;
	if (m_customSockets)
	{
		for (CustomSocketList::const_iterator it = m_customSockets->begin(); it != m_customSockets->end(); ++it)
			nextIndex = std::max(nextIndex, it->socketIndex + 1);
	}
	return nextIndex;
}

// ----------------------------------------------------------------------

bool PortalProperty::addCustomSocket(CustomSocket const &socket)
{
	NOT_NULL(m_customSockets);
	for (CustomSocketList::iterator it = m_customSockets->begin(); it != m_customSockets->end(); ++it)
	{
		if (it->socketIndex == socket.socketIndex)
		{
			int const preservedPortalIndex = it->materializedPortalIndex;
			*it = socket;
			it->materializedPortalIndex = preservedPortalIndex;
			if (it->doorwayWidth < 0.01f)
				it->doorwayWidth = 1.0f;
			if (it->doorwayHeight < 0.01f)
				it->doorwayHeight = 2.0f;
			return true;
		}
	}
	CustomSocket entry = socket;
	entry.materializedPortalIndex = -1;
	entry.floorExtensionStartTri = -1;
	entry.floorExtensionTriCount = 0;
	if (entry.doorwayWidth < 0.01f)
		entry.doorwayWidth = 1.0f;
	if (entry.doorwayHeight < 0.01f)
		entry.doorwayHeight = 2.0f;
	m_customSockets->push_back(entry);
	return true;
}

// ----------------------------------------------------------------------

bool PortalProperty::removeCustomSocket(int socketIndex)
{
	NOT_NULL(m_customSockets);
	for (CustomSocketList::iterator it = m_customSockets->begin(); it != m_customSockets->end(); ++it)
	{
		if (it->socketIndex == socketIndex)
		{
			IGNORE_RETURN(m_customSockets->erase(it));
			return true;
		}
	}
	return false;
}

// ----------------------------------------------------------------------

void PortalProperty::clearCustomSockets()
{
	NOT_NULL(m_customSockets);
	m_customSockets->clear();
}

// ----------------------------------------------------------------------

PortalProperty::CustomSocketList const &PortalProperty::getCustomSockets() const
{
	NOT_NULL(m_customSockets);
	return *m_customSockets;
}

// ----------------------------------------------------------------------

bool PortalProperty::findCustomSocket(int cellIndex, int portalIndex, CustomSocket &outSocket) const
{
	NOT_NULL(m_customSockets);
	for (CustomSocketList::const_iterator it = m_customSockets->begin(); it != m_customSockets->end(); ++it)
	{
		if (it->cellIndex == cellIndex && it->socketIndex == portalIndex)
		{
			outSocket = *it;
			return true;
		}
	}
	return false;
}

// ----------------------------------------------------------------------

bool PortalProperty::isCustomSocketIndex(int portalIndex)
{
	return portalIndex >= cms_customSocketBase;
}

namespace PortalPropertyMaterializeNamespace
{
	struct DoorwayBoundaryEdge
	{
		int triIndex;
		int edgeIndex;
		Vector a;
		Vector b;

		bool isValid() const
		{
			return triIndex >= 0;
		}

		static DoorwayBoundaryEdge invalid()
		{
			DoorwayBoundaryEdge edge;
			edge.triIndex = -1;
			edge.edgeIndex = -1;
			edge.a = Vector::zero;
			edge.b = Vector::zero;
			return edge;
		}
	};

	void stabilizeCustomSocketTransform(CellProperty const & cell, Transform & doorTransform);
}

// ----------------------------------------------------------------------

bool PortalProperty::getPortalSocketTransform_o2p(int cellIndex, int portalIndex, Transform &outTransform_o2p) const
{
	if (cellIndex < 1 || cellIndex >= getNumberOfCells())
		return false;

	CellProperty const *const cell = getCell(cellIndex);
	if (!cell)
		return false;

	Object const &cellOwner = cell->getOwner();
	if (!cellOwner.isInitialized() || cellOwner.getCellProperty() != cell)
		return false;

	if (isCustomSocketIndex(portalIndex))
	{
		CustomSocket customSocket;
		if (!findCustomSocket(cellIndex, portalIndex, customSocket))
			return false;

		outTransform_o2p.multiply(cellOwner.getTransform_o2p(), customSocket.doorTransform_o2p);
		return true;
	}

	if (portalIndex < 0 || portalIndex >= cell->getPortalCount())
		return false;

	Portal const *const portal = const_cast<CellProperty *>(cell)->getPortal(portalIndex);
	if (!portal)
		return false;

	outTransform_o2p.multiply(cell->getOwner().getTransform_o2p(), portal->getDoorTransform());
	return true;
}

// ----------------------------------------------------------------------

bool PortalProperty::getPortalNeighbor(int cellIndex, int portalIndex, int &outNeighborCellIndex, int &outNeighborPortalIndex) const
{
	outNeighborCellIndex = -1;
	outNeighborPortalIndex = -1;

	if (isCustomSocketIndex(portalIndex))
		return false;

	CellProperty const *const cell = getCell(cellIndex);
	if (!cell)
		return false;

	if (portalIndex < 0 || portalIndex >= cell->getPortalCount())
		return false;

	Portal const *const portal = const_cast<CellProperty *>(cell)->getPortal(portalIndex);
	if (!portal || !portal->getNeighbor())
		return false;

	Portal const *const neighborPortal = portal->getNeighbor();
	CellProperty const *const neighborCell = neighborPortal->getParentCell();
	if (!neighborCell)
		return false;

	outNeighborCellIndex = neighborCell->getCellIndex();
	int const neighborPortalCount = neighborCell->getPortalCount();
	for (int pi = 0; pi < neighborPortalCount; ++pi)
	{
		if (const_cast<CellProperty *>(neighborCell)->getPortal(pi) == neighborPortal)
		{
			outNeighborPortalIndex = pi;
			return true;
		}
	}

	return false;
}

// ----------------------------------------------------------------------

IndexedTriangleList const * PortalProperty::getCellPortalGeometry(int cellIndex, int portalIndex) const
{
	CellProperty const * const cell = getCell(cellIndex);
	if (!cell)
		return 0;

	if (portalIndex < 0 || portalIndex >= cell->getPortalCount())
		return 0;

	Portal const * const portal = const_cast<CellProperty *>(cell)->getPortal(portalIndex);
	if (!portal)
		return 0;

	return &portal->getGeometry();
}

// ----------------------------------------------------------------------

bool PortalProperty::linkCustomSocketGraft(int hostCellIndex, int customSocketIndex, int graftCellIndex, int graftPortalIndex)
{
	CustomSocket customSocket;
	if (!findCustomSocket(hostCellIndex, customSocketIndex, customSocket))
		return false;

	CellProperty *const hostCell = getCell(hostCellIndex);
	CellProperty *const graftCell = getCell(graftCellIndex);
	if (!hostCell || !graftCell)
		return false;

	if (!materializeCustomSocketPortal(hostCellIndex, customSocketIndex))
		return false;
	if (!findCustomSocket(hostCellIndex, customSocketIndex, customSocket))
		return false;

	int const hostPortalIndex = customSocket.materializedPortalIndex;
	if (hostPortalIndex < 0)
		return false;

	int const resolvedGraftPortal = PortalProperty::resolveCellPortalIndex(graftCell, graftPortalIndex);
	if (resolvedGraftPortal < 0)
		return false;

	if (!linkCellPortals(hostCellIndex, hostPortalIndex, graftCellIndex, resolvedGraftPortal))
		return false;

	IGNORE_RETURN(finalizeCustomSocketPortalWalkthrough(hostCellIndex, customSocketIndex));
	return true;
}

// ----------------------------------------------------------------------

namespace PortalPropertyMaterializeNamespace
{
	Vector computeCellFloorCenter(CellProperty const & cell)
	{
		Floor const * const floor = cell.getFloor();
		if (!floor)
			return Vector::zero;

		FloorMesh const * const floorMesh = floor->getFloorMesh();
		if (!floorMesh)
			return Vector::zero;

		AxialBox box;
		for (int tri = 0; tri < floorMesh->getTriCount(); ++tri)
		{
			Triangle3d const T = floorMesh->getTriangle(tri);
			for (int c = 0; c < 3; ++c)
				box.add(T.getCorner(c));
		}
		return box.getCenter();
	}

	Vector getPortalWalkOutward(Transform const & doorTransform)
	{
		Vector outward = doorTransform.getLocalFrameK_p();
		outward.y = 0.0f;
		if (outward.normalize() < 0.01f)
			return Vector(0.0f, 0.0f, 1.0f);
		return -outward;
	}

	void ensurePortalTransformInward(CellProperty const & cell, Transform & doorTransform)
	{
		Vector const cellCenter = computeCellFloorCenter(cell);
		Vector const pos = doorTransform.getPosition_p();
		Vector inward = cellCenter - pos;

		Vector axisI = doorTransform.getLocalFrameI_p();
		Vector axisJ = doorTransform.getLocalFrameJ_p();
		Vector axisK = doorTransform.getLocalFrameK_p();

		bool const wallAligned = fabsf(axisK.y) < 0.15f;
		if (wallAligned)
		{
			inward.y = 0.0f;
			axisK.y = 0.0f;
			if (inward.normalize() < 0.01f || axisK.normalize() < 0.01f)
				return;

			if (axisK.dot(inward) < 0.0f)
			{
				axisI = -axisI;
				axisK = -axisK;
				doorTransform.setLocalFrameIJK_p(axisI, Vector(0.0f, 1.0f, 0.0f), axisK);
			}
		}
		else if (inward.normalize() > 0.01f && axisK.normalize() > 0.01f)
		{
			if (axisK.dot(inward) < 0.0f)
			{
				axisI = -axisI;
				axisJ = -axisJ;
				axisK = -axisK;
				doorTransform.setLocalFrameIJK_p(axisI, axisJ, axisK);
			}
		}
	}

	void ensurePortalTransformOutward(CellProperty const & cell, Transform & doorTransform)
	{
		Vector const cellCenter = computeCellFloorCenter(cell);
		Vector const pos = doorTransform.getPosition_p();
		Vector outward = pos - cellCenter;
		outward.y = 0.0f;
		if (outward.normalize() < 0.01f)
			return;

		Vector axisI = doorTransform.getLocalFrameI_p();
		Vector axisK = doorTransform.getLocalFrameK_p();
		axisK.y = 0.0f;
		if (axisK.normalize() < 0.01f)
			return;

		if (axisK.dot(outward) < 0.0f)
		{
			axisI = -axisI;
			axisK = -axisK;
			doorTransform.setLocalFrameIJK_p(axisI, Vector(0.0f, 1.0f, 0.0f), axisK);
		}
	}

	char const * pickRuntimeDoorStyle(Transform const & doorTransform)
	{
		Vector const axisI = doorTransform.getLocalFrameI_p();
		if (fabsf(axisI.x) >= fabsf(axisI.z))
			return "door_bunker_rebel_x_axis";
		return "poi_all_impl_bunker_int_door";
	}

	bool snapCustomSocketPositionToFloorBoundary(CellProperty const & cell, Transform & doorTransform, DoorwayBoundaryEdge * outEdge)
	{
		Floor const * const floor = cell.getFloor();
		if (!floor)
			return false;

		FloorMesh const * const floorMeshConst = floor->getFloorMesh();
		if (!floorMeshConst)
			return false;

		FloorMesh const & floorMesh = *floorMeshConst;

		Vector const axisI = doorTransform.getLocalFrameI_p();
		Vector const portalPos = doorTransform.getPosition_p();

		Vector preferredOutward = getPortalWalkOutward(doorTransform);
		bool const hasPreferredOutward = preferredOutward.normalize() > 0.01f;

		DoorwayBoundaryEdge bestEdge = DoorwayBoundaryEdge::invalid();
		float bestScore = std::numeric_limits<float>::max();

		for (int tri = 0; tri < floorMesh.getTriCount(); ++tri)
		{
			FloorTri const & F = floorMesh.getFloorTri(tri);
			Triangle3d const T = floorMesh.getTriangle(tri);
			for (int edge = 0; edge < 3; ++edge)
			{
				if (F.getNeighborIndex(edge) != -1)
					continue;

				Vector const a = T.getCorner(edge);
				Vector const b = T.getCorner(edge + 1);
				Vector edgeDir = b - a;
				edgeDir.y = 0.0f;
				float const edgeLen = edgeDir.magnitude();
				if (edgeLen < 0.25f)
					continue;
				edgeDir /= edgeLen;

				float const parallelI = fabsf(edgeDir.dot(axisI));
				float const wallAligned = hasPreferredOutward ? fabsf(edgeDir.dot(preferredOutward)) : 1.0f;
				if (parallelI < 0.85f && wallAligned > 0.15f)
					continue;

				Vector const edgeMid = (a + b) * 0.5f;
				Vector toEdge = edgeMid - portalPos;
				toEdge.y = 0.0f;
				float const dist = toEdge.magnitude();
				if (dist >= 3.0f)
					continue;

				if (hasPreferredOutward)
				{
					if (dist > 0.05f && toEdge.dot(preferredOutward) < 0.15f)
						continue;
				}

				float score = dist;
				if (hasPreferredOutward && dist > 0.01f && toEdge.dot(preferredOutward) < 0.0f)
					score += 50.0f;

				if (score < bestScore)
				{
					bestEdge.triIndex = tri;
					bestEdge.edgeIndex = edge;
					bestEdge.a = a;
					bestEdge.b = b;
					bestScore = score;
				}
			}
		}

		if (!bestEdge.isValid())
		{
			for (int tri = 0; tri < floorMesh.getTriCount(); ++tri)
			{
				FloorTri const & F = floorMesh.getFloorTri(tri);
				Triangle3d const T = floorMesh.getTriangle(tri);
				for (int edge = 0; edge < 3; ++edge)
				{
					if (F.getNeighborIndex(edge) != -1)
						continue;

					Vector const a = T.getCorner(edge);
					Vector const b = T.getCorner(edge + 1);
					Vector const edgeMid = (a + b) * 0.5f;
					Vector toEdge = edgeMid - portalPos;
					toEdge.y = 0.0f;
					float const dist = toEdge.magnitude();
					if (dist >= 3.0f)
						continue;

					if (hasPreferredOutward)
					{
						if (dist > 0.05f && toEdge.dot(preferredOutward) < 0.15f)
							continue;
					}

					float score = dist + 0.5f;
					if (hasPreferredOutward && dist > 0.01f && toEdge.dot(preferredOutward) < 0.0f)
						score += 50.0f;

					if (score < bestScore)
					{
						bestEdge.triIndex = tri;
						bestEdge.edgeIndex = edge;
						bestEdge.a = a;
						bestEdge.b = b;
						bestScore = score;
					}
				}
			}
		}

		if (!bestEdge.isValid())
			return false;

		Vector edgeDir = bestEdge.b - bestEdge.a;
		edgeDir.y = 0.0f;
		float const edgeLen = edgeDir.magnitude();
		if (edgeLen < 0.01f)
			return false;
		edgeDir /= edgeLen;

		Segment3d const edgeSeg(bestEdge.a, bestEdge.b);
		Vector snappedPos = Distance3d::ClosestPointSeg(portalPos, edgeSeg);
		doorTransform.setPosition_p(snappedPos);

		Vector alignedI = edgeDir;
		if (doorTransform.getLocalFrameI_p().dot(alignedI) < 0.0f)
			alignedI = -alignedI;
		Vector alignedK = Vector::unitY.cross(alignedI);
		if (alignedK.normalize() < 0.01f)
		{
			alignedK = doorTransform.getLocalFrameK_p();
			alignedK.y = 0.0f;
			if (alignedK.normalize() < 0.01f)
				alignedK = Vector(0.0f, 0.0f, 1.0f);
		}

		Vector const cellCenter = computeCellFloorCenter(cell);
		Vector inward = cellCenter - snappedPos;
		inward.y = 0.0f;
		if (inward.normalize() > 0.01f && alignedK.dot(inward) < 0.0f)
		{
			alignedI = -alignedI;
			alignedK = -alignedK;
		}

		doorTransform.setLocalFrameIJK_p(alignedI, Vector::unitY, alignedK);
		ensurePortalTransformInward(cell, doorTransform);

		if (outEdge)
			*outEdge = bestEdge;

		return true;
	}

	void stabilizeCustomSocketTransform(CellProperty const & cell, Transform & doorTransform)
	{
		if (!snapCustomSocketPositionToFloorBoundary(cell, doorTransform, 0))
			ensurePortalTransformInward(cell, doorTransform);
	}

	bool flagSnappedBoundaryEdgesDirect(
		FloorMesh * floorMesh,
		DoorwayBoundaryEdge const & snappedEdge,
		Transform const & doorTransform,
		float doorwayWidth,
		int portalIndex)
	{
		if (!floorMesh || !snappedEdge.isValid())
			return false;

		floorMesh->clearPortalEdges(portalIndex);

		Vector edgeDir = snappedEdge.b - snappedEdge.a;
		edgeDir.y = 0.0f;
		if (edgeDir.normalize() < 0.01f)
			return false;

		bool flagged = false;
		if (snappedEdge.triIndex >= 0 && snappedEdge.triIndex < floorMesh->getTriCount())
		{
			FloorTri & snappedTri = floorMesh->getFloorTri(snappedEdge.triIndex);
			if (snappedEdge.edgeIndex >= 0 && snappedEdge.edgeIndex < 3
				&& snappedTri.getNeighborIndex(snappedEdge.edgeIndex) == -1)
			{
				snappedTri.setPortalId(snappedEdge.edgeIndex, portalIndex);
				flagged = true;
			}
		}

		Vector const pos = doorTransform.getPosition_p();
		float const halfWidth = doorwayWidth * 0.5f;
		Segment3d const doorwaySpan(pos - edgeDir * halfWidth, pos + edgeDir * halfWidth);
		Segment3d const snappedSeg(snappedEdge.a, snappedEdge.b);

		for (int tri = 0; tri < floorMesh->getTriCount(); ++tri)
		{
			FloorTri & F = floorMesh->getFloorTri(tri);
			Triangle3d const T = floorMesh->getTriangle(tri);
			for (int edge = 0; edge < 3; ++edge)
			{
				if (F.getNeighborIndex(edge) != -1)
					continue;

				Vector const a = T.getCorner(edge);
				Vector const b = T.getCorner(edge + 1);
				Vector segDir = b - a;
				segDir.y = 0.0f;
				if (segDir.normalize() < 0.01f)
					continue;

				if (fabsf(fabsf(segDir.dot(edgeDir)) - 1.0f) > 0.2f)
					continue;

				Vector const aOnSnap = Distance3d::ClosestPointSeg(a, snappedSeg);
				Vector const bOnSnap = Distance3d::ClosestPointSeg(b, snappedSeg);
				if ((a - aOnSnap).magnitude() > 0.35f || (b - bOnSnap).magnitude() > 0.35f)
					continue;

				Vector const aProj = Distance3d::ClosestPointSeg(a, doorwaySpan);
				Vector const bProj = Distance3d::ClosestPointSeg(b, doorwaySpan);
				if ((a - aProj).magnitude() < halfWidth + 0.5f && (b - bProj).magnitude() < halfWidth + 0.5f)
				{
					F.setPortalId(edge, portalIndex);
					flagged = true;
				}
			}
		}

		return flagged;
	}

	bool flagNearbyBoundaryEdges(
		FloorMesh * floorMesh,
		Transform const & doorTransform,
		float doorwayWidth,
		int portalIndex)
	{
		if (!floorMesh)
			return false;

		Vector const pos = doorTransform.getPosition_p();
		Vector axisI = doorTransform.getLocalFrameI_p();
		axisI.y = 0.0f;
		if (axisI.normalize() < 0.01f)
			return false;

		float const halfWidth = doorwayWidth * 0.5f;
		Segment3d const doorwaySpan(pos - axisI * halfWidth, pos + axisI * halfWidth);

		bool flagged = false;
		for (int tri = 0; tri < floorMesh->getTriCount(); ++tri)
		{
			FloorTri & F = floorMesh->getFloorTri(tri);
			Triangle3d const T = floorMesh->getTriangle(tri);
			for (int edge = 0; edge < 3; ++edge)
			{
				if (F.getNeighborIndex(edge) != -1)
					continue;

				Vector const a = T.getCorner(edge);
				Vector const b = T.getCorner(edge + 1);
				Vector const edgeMid = (a + b) * 0.5f;
				Vector toEdge = edgeMid - pos;
				toEdge.y = 0.0f;
				if (toEdge.magnitude() > 2.0f)
					continue;

				Vector const aProj = Distance3d::ClosestPointSeg(a, doorwaySpan);
				Vector const bProj = Distance3d::ClosestPointSeg(b, doorwaySpan);
				if ((a - aProj).magnitude() < 0.75f && (b - bProj).magnitude() < 0.75f)
				{
					F.setPortalId(edge, portalIndex);
					flagged = true;
				}
			}
		}

		return flagged;
	}

	bool flagDoorwayBoundaryEdges(
		FloorMesh * floorMesh,
		DoorwayBoundaryEdge const & snappedEdge,
		Transform const & doorTransform,
		float doorwayWidth,
		float doorwayHeight,
		int portalIndex)
	{
		if (!floorMesh || !snappedEdge.isValid())
			return false;

		floorMesh->clearPortalEdges(portalIndex);

		Vector axisI = doorTransform.getLocalFrameI_p();
		axisI.y = 0.0f;
		if (axisI.normalize() < 0.01f)
			return false;

		Vector const axisJ = doorTransform.getLocalFrameJ_p();
		Vector const pos = doorTransform.getPosition_p();
		float const halfWidth = doorwayWidth * 0.5f;
		Segment3d const doorwaySpan(pos - axisI * halfWidth, pos + axisI * halfWidth);

		bool flagged = false;

		VectorVector portalVerts;
		portalVerts.push_back(pos - axisI * halfWidth);
		portalVerts.push_back(pos + axisI * halfWidth);
		portalVerts.push_back(pos + axisI * halfWidth + axisJ * doorwayHeight);
		portalVerts.push_back(pos - axisI * halfWidth + axisJ * doorwayHeight);
		if (floorMesh->flagPortalEdges(portalVerts, portalIndex))
			flagged = true;

		for (int tri = 0; tri < floorMesh->getTriCount(); ++tri)
		{
			FloorTri & F = floorMesh->getFloorTri(tri);
			Triangle3d const T = floorMesh->getTriangle(tri);
			for (int edge = 0; edge < 3; ++edge)
			{
				if (F.getNeighborIndex(edge) != -1)
					continue;

				Vector const a = T.getCorner(edge);
				Vector const b = T.getCorner(edge + 1);
				Vector edgeDir = b - a;
				edgeDir.y = 0.0f;
				if (edgeDir.normalize() < 0.01f)
					continue;

				if (fabsf(edgeDir.dot(axisI)) < 0.85f)
					continue;

				Vector const aProj = Distance3d::ClosestPointSeg(a, doorwaySpan);
				Vector const bProj = Distance3d::ClosestPointSeg(b, doorwaySpan);
				if ((a - aProj).magnitude() < 0.25f && (b - bProj).magnitude() < 0.25f)
				{
					F.setPortalId(edge, portalIndex);
					flagged = true;
				}
			}
		}

		return flagged;
	}

	bool flagBoundaryEdgesDirect(
		FloorMesh * floorMesh,
		Transform const & doorTransform,
		float doorwayWidth,
		int portalIndex)
	{
		if (!floorMesh)
			return false;

		Vector const axisI = doorTransform.getLocalFrameI_p();
		Vector const pos = doorTransform.getPosition_p();
		float const halfWidth = doorwayWidth * 0.5f;
		Vector const portalBottomA = pos - axisI * halfWidth;
		Vector const portalBottomB = pos + axisI * halfWidth;

		bool flagged = false;
		for (int tri = 0; tri < floorMesh->getTriCount(); ++tri)
		{
			FloorTri & F = floorMesh->getFloorTri(tri);
			Triangle3d const T = floorMesh->getTriangle(tri);
			for (int edge = 0; edge < 3; ++edge)
			{
				if (F.getNeighborIndex(edge) != -1)
					continue;

				Vector const a = T.getCorner(edge);
				Vector const b = T.getCorner(edge + 1);
				Vector edgeDir = b - a;
				edgeDir.y = 0.0f;
				if (edgeDir.normalize() < 0.01f)
					continue;

				if (fabsf(fabsf(edgeDir.dot(axisI)) - 1.0f) > 0.2f)
					continue;

				Vector const edgeMid = (a + b) * 0.5f;
				Vector toEdge = edgeMid - pos;
				toEdge.y = 0.0f;
				if (toEdge.magnitude() > 1.5f)
					continue;

				Segment3d const portalSeg(portalBottomA, portalBottomB);
				Vector const aProj = Distance3d::ClosestPointSeg(a, portalSeg);
				Vector const bProj = Distance3d::ClosestPointSeg(b, portalSeg);
				if ((a - aProj).magnitude() < 0.15f && (b - bProj).magnitude() < 0.15f)
				{
					F.setPortalId(edge, portalIndex);
					flagged = true;
				}
			}
		}

		return flagged;
	}

	bool flagPortalEdgesRobust(
		FloorMesh * floorMesh,
		Transform const & doorTransform,
		float doorwayWidth,
		float doorwayHeight,
		int portalIndex)
	{
		if (!floorMesh)
			return false;

		floorMesh->clearPortalEdges(portalIndex);

		float const halfWidth = doorwayWidth * 0.5f;
		Vector const axisI = doorTransform.getLocalFrameI_p();
		Vector const axisJ = doorTransform.getLocalFrameJ_p();
		Vector const walkOutward = getPortalWalkOutward(doorTransform);
		Vector const pos = doorTransform.getPosition_p();

		float const offsetsOutward[] = { 0.0f, 0.05f, 0.1f, 0.15f, 0.25f, 0.35f, 0.5f, 0.75f, 1.0f };
		float const offsetsI[] = { 0.0f, 0.05f, -0.05f, 0.1f, -0.1f, 0.15f, -0.15f };
		float const offsetsJ[] = { 0.0f, -0.05f, 0.05f, -0.1f, 0.1f };

		for (size_t oj = 0; oj < sizeof(offsetsJ) / sizeof(offsetsJ[0]); ++oj)
		{
			for (size_t oi = 0; oi < sizeof(offsetsI) / sizeof(offsetsI[0]); ++oi)
			{
				for (size_t ok = 0; ok < sizeof(offsetsOutward) / sizeof(offsetsOutward[0]); ++ok)
				{
					Vector const base = pos + axisJ * offsetsJ[oj] + axisI * offsetsI[oi] + walkOutward * offsetsOutward[ok];
					VectorVector nudged;
					nudged.push_back(base - axisI * halfWidth);
					nudged.push_back(base + axisI * halfWidth);
					nudged.push_back(base + axisI * halfWidth + axisJ * doorwayHeight);
					nudged.push_back(base - axisI * halfWidth + axisJ * doorwayHeight);
					if (floorMesh->flagPortalEdges(nudged, portalIndex))
						return true;
				}
			}
		}

		return flagBoundaryEdgesDirect(floorMesh, doorTransform, doorwayWidth, portalIndex);
	}

	void clearCustomSocketFloorExtension(PortalProperty::CustomSocket & socketEntry, FloorMesh * floorMesh)
	{
		if (!floorMesh || socketEntry.floorExtensionTriCount <= 0 || socketEntry.floorExtensionStartTri < 0)
		{
			socketEntry.floorExtensionStartTri = -1;
			socketEntry.floorExtensionTriCount = 0;
			return;
		}

		int const startTri = socketEntry.floorExtensionStartTri;
		int const triCount = socketEntry.floorExtensionTriCount;
		for (int triOffset = triCount - 1; triOffset >= 0; --triOffset)
		{
			int const triIndex = startTri + triOffset;
			if (triIndex >= 0 && triIndex < floorMesh->getTriCount())
				floorMesh->deleteTri(triIndex);
		}

		socketEntry.floorExtensionStartTri = -1;
		socketEntry.floorExtensionTriCount = 0;
	}

	bool extendFloorThroughDoorway(
		FloorMesh * floorMesh,
		Transform const & doorTransform,
		float doorwayWidth,
		float depth,
		int portalIndex,
		int & outStartTri,
		int & outTriCount)
	{
		if (!floorMesh || depth < 0.01f)
			return false;

		outStartTri = floorMesh->getTriCount();
		outTriCount = 0;

		Vector const axisI = doorTransform.getLocalFrameI_p();
		Vector const walkOutward = getPortalWalkOutward(doorTransform);
		Vector const pos = doorTransform.getPosition_p();
		float const halfWidth = doorwayWidth * 0.5f;

		Vector const innerLeft = pos - axisI * halfWidth;
		Vector const innerRight = pos + axisI * halfWidth;
		Vector const outerLeft = innerLeft + walkOutward * depth;
		Vector const outerRight = innerRight + walkOutward * depth;

		floorMesh->addTriangle(Triangle3d(innerLeft, innerRight, outerRight));
		floorMesh->addTriangle(Triangle3d(innerLeft, outerRight, outerLeft));
		outTriCount = 2;

		VectorVector innerPortalVerts;
		innerPortalVerts.push_back(innerLeft);
		innerPortalVerts.push_back(innerRight);
		innerPortalVerts.push_back(innerRight + Vector(0.0f, 0.05f, 0.0f));
		innerPortalVerts.push_back(innerLeft + Vector(0.0f, 0.05f, 0.0f));
		IGNORE_RETURN(floorMesh->flagPortalEdges(innerPortalVerts, portalIndex));

		VectorVector outerPortalVerts;
		outerPortalVerts.push_back(outerLeft);
		outerPortalVerts.push_back(outerRight);
		outerPortalVerts.push_back(outerRight + Vector(0.0f, 0.05f, 0.0f));
		outerPortalVerts.push_back(outerLeft + Vector(0.0f, 0.05f, 0.0f));
		IGNORE_RETURN(floorMesh->flagPortalEdges(outerPortalVerts, portalIndex));

		return true;
	}

	bool boxesOverlap(AxialBox const & a, AxialBox const & b)
	{
		Vector const aMin = a.getMin();
		Vector const aMax = a.getMax();
		Vector const bMin = b.getMin();
		Vector const bMax = b.getMax();
		return aMin.x <= bMax.x && aMax.x >= bMin.x
			&& aMin.y <= bMax.y && aMax.y >= bMin.y
			&& aMin.z <= bMax.z && aMax.z >= bMin.z;
	}

	AxialBox buildDoorwayCutBox(Transform const & doorTransform, float width, float height, float outwardDepth)
	{
		Vector const pos = doorTransform.getPosition_p();
		Vector const axisI = doorTransform.getLocalFrameI_p();
		Vector const axisJ = doorTransform.getLocalFrameJ_p();
		Vector inward = doorTransform.getLocalFrameK_p();
		if (inward.normalize() < 0.01f)
			inward = Vector(0.0f, 0.0f, 1.0f);
		Vector const outward = -inward;
		float const inwardDepth = 0.50f;
		float const halfW = width * 0.5f + 0.03f;
		float const topHeight = height + 0.05f;

		AxialBox box;
		for (int si = -1; si <= 1; si += 2)
		{
			for (int sj = 0; sj <= 1; ++sj)
			{
				float const heightOffset = sj == 0 ? 0.0f : topHeight;
				Vector const base = pos
					+ axisI * (halfW * static_cast<float>(si))
					+ axisJ * heightOffset;
				box.add(base - inward * inwardDepth);
				box.add(base + outward * outwardDepth);
			}
		}
		return box;
	}

	bool flagBoundaryEdgesInDoorwayBox(
		FloorMesh * floorMesh,
		AxialBox const & doorwayBox,
		int portalIndex)
	{
		if (!floorMesh)
			return false;

		bool flagged = false;
		for (int tri = 0; tri < floorMesh->getTriCount(); ++tri)
		{
			FloorTri & F = floorMesh->getFloorTri(tri);
			Triangle3d const T = floorMesh->getTriangle(tri);
			for (int edge = 0; edge < 3; ++edge)
			{
				if (F.getNeighborIndex(edge) != -1)
					continue;

				Vector const a = T.getCorner(edge);
				Vector const b = T.getCorner(edge + 1);
				Vector const edgeMid = (a + b) * 0.5f;
				if (doorwayBox.contains(edgeMid) || doorwayBox.contains(a) || doorwayBox.contains(b))
				{
					F.setPortalId(edge, portalIndex);
					flagged = true;
				}
			}
		}

		return flagged;
	}

	bool flagCustomSocketPortalFloor(
		FloorMesh * floorMesh,
		DoorwayBoundaryEdge const & boundaryEdge,
		Transform const & doorTransform,
		float doorwayWidth,
		float doorwayHeight,
		int portalIndex)
	{
		if (!floorMesh)
			return false;

		floorMesh->clearPortalEdges(portalIndex);

		bool flagged = false;
		if (boundaryEdge.isValid())
			flagged = flagSnappedBoundaryEdgesDirect(floorMesh, boundaryEdge, doorTransform, doorwayWidth, portalIndex);
		if (!flagged && boundaryEdge.isValid())
			flagged = flagDoorwayBoundaryEdges(floorMesh, boundaryEdge, doorTransform, doorwayWidth, doorwayHeight, portalIndex);
		if (!flagged)
			flagged = flagPortalEdgesRobust(floorMesh, doorTransform, doorwayWidth, doorwayHeight, portalIndex);
		if (!flagged)
			flagged = flagNearbyBoundaryEdges(floorMesh, doorTransform, doorwayWidth, portalIndex);
		if (!flagged)
		{
			AxialBox const doorwayBox = buildDoorwayCutBox(doorTransform, doorwayWidth, doorwayHeight, 0.5f);
			flagged = flagBoundaryEdgesInDoorwayBox(floorMesh, doorwayBox, portalIndex);
		}
		return flagged;
	}

	bool extentIntersectsDoorway(BaseExtent const * extent, AxialBox const & doorwayBox)
	{
		if (!extent)
			return false;
		return boxesOverlap(extent->getBoundingBox(), doorwayBox);
	}

	bool extentIsRoomScaleComparedToDoorway(BaseExtent const * extent, AxialBox const & doorwayBox)
	{
		if (!extent)
			return true;

		AxialBox const extBox = extent->getBoundingBox();
		Vector const extSize = extBox.getMax() - extBox.getMin();
		Vector const doorSize = doorwayBox.getMax() - doorwayBox.getMin();
		float const maxDoor = std::max(doorSize.x, std::max(doorSize.y, doorSize.z));
		float const maxExt = std::max(extSize.x, std::max(extSize.y, extSize.z));
		return maxExt > maxDoor * 2.5f;
	}

	void appendLocalDoorBoxAsExtent(
		CompositeExtent * next,
		Vector const & origin,
		Vector const & axisI,
		Vector const & axisJ,
		Vector const & axisK,
		float minI,
		float maxI,
		float minJ,
		float maxJ,
		float minK,
		float maxK)
	{
		if (!next || maxI <= minI + 0.02f || maxJ <= minJ + 0.02f || maxK <= minK + 0.02f)
			return;

		AxialBox axial;
		for (int ci = 0; ci <= 1; ++ci)
		{
			float const i = ci == 0 ? minI : maxI;
			for (int cj = 0; cj <= 1; ++cj)
			{
				float const j = cj == 0 ? minJ : maxJ;
				for (int ck = 0; ck <= 1; ++ck)
				{
					float const k = ck == 0 ? minK : maxK;
					axial.add(origin + axisI * i + axisJ * j + axisK * k);
				}
			}
		}

		next->attachExtent(new BoxExtent(axial));
	}

	bool splitRoomScaleBoxExtentAroundDoorway(
		BoxExtent const & boxExtent,
		Transform const & doorTransform,
		float width,
		float height,
		CompositeExtent * next)
	{
		if (!next)
			return false;

		Vector const origin = doorTransform.getPosition_p();
		Vector const axisI = doorTransform.getLocalFrameI_p();
		Vector const axisJ = doorTransform.getLocalFrameJ_p();
		Vector const axisK = doorTransform.getLocalFrameK_p();

		auto toLocal = [&](Vector const & p) -> Vector
		{
			Vector const rel = p - origin;
			return Vector(rel.dot(axisI), rel.dot(axisJ), rel.dot(axisK));
		};

		AxialBox const roomAxial = boxExtent.getBoundingBox();
		AxialBox roomLocal;
		for (int ci = 0; ci <= 1; ++ci)
		{
			float const x = ci == 0 ? roomAxial.getMin().x : roomAxial.getMax().x;
			for (int cj = 0; cj <= 1; ++cj)
			{
				float const y = cj == 0 ? roomAxial.getMin().y : roomAxial.getMax().y;
				for (int ck = 0; ck <= 1; ++ck)
				{
					float const z = ck == 0 ? roomAxial.getMin().z : roomAxial.getMax().z;
					roomLocal.add(toLocal(Vector(x, y, z)));
				}
			}
		}

		float const rMinI = roomLocal.getMin().x;
		float const rMaxI = roomLocal.getMax().x;
		float const rMinJ = roomLocal.getMin().y;
		float const rMaxJ = roomLocal.getMax().y;
		float const rMinK = roomLocal.getMin().z;
		float const rMaxK = roomLocal.getMax().z;

		float const halfW = width * 0.5f + 0.04f;
		float const doorTop = height + 0.04f;
		float const wallThickness = 0.55f;
		float const kWallOut = -wallThickness;
		float const kWallIn = 0.06f;
		float const kHi = std::max(rMinK, rMaxK);

		if (kHi > kWallIn + 0.05f)
			appendLocalDoorBoxAsExtent(next, origin, axisI, axisJ, axisK, rMinI, rMaxI, rMinJ, rMaxJ, kWallIn, kHi);

		if (rMinI < -halfW - 0.02f)
			appendLocalDoorBoxAsExtent(next, origin, axisI, axisJ, axisK, rMinI, -halfW, rMinJ, rMaxJ, kWallOut, kWallIn);

		if (rMaxI > halfW + 0.02f)
			appendLocalDoorBoxAsExtent(next, origin, axisI, axisJ, axisK, halfW, rMaxI, rMinJ, rMaxJ, kWallOut, kWallIn);

		if (doorTop < rMaxJ - 0.02f)
			appendLocalDoorBoxAsExtent(next, origin, axisI, axisJ, axisK, -halfW, halfW, doorTop, rMaxJ, kWallOut, kWallIn);

		if (rMinJ < -0.02f)
			appendLocalDoorBoxAsExtent(next, origin, axisI, axisJ, axisK, -halfW, halfW, rMinJ, 0.0f, kWallOut, kWallIn);

		return next->getExtentCount() > 0;
	}

	bool attachExtentWithDoorwayOpening(
		CompositeExtent * next,
		BaseExtent const * child,
		Transform const & doorTransform,
		float width,
		float height,
		AxialBox const & doorwayBox)
	{
		if (!next || !child)
			return false;

		if (child->getType() == ET_Box && extentIsRoomScaleComparedToDoorway(child, doorwayBox))
		{
			BoxExtent const * const boxExtent = safe_cast<BoxExtent const *>(child);
			if (boxExtent && splitRoomScaleBoxExtentAroundDoorway(*boxExtent, doorTransform, width, height, next))
				return true;
		}

		if (extentIntersectsDoorway(child, doorwayBox))
			return false;

		next->attachExtent(child->clone());
		return true;
	}
}

using namespace PortalPropertyMaterializeNamespace;

// ----------------------------------------------------------------------

bool PortalProperty::materializeCustomSocketPortal(int cellIndex, int customSocketIndex)
{
	NOT_NULL(m_customSockets);

	CustomSocket * socketEntry = 0;
	for (CustomSocketList::iterator it = m_customSockets->begin(); it != m_customSockets->end(); ++it)
	{
		if (it->cellIndex == cellIndex && it->socketIndex == customSocketIndex)
		{
			socketEntry = &(*it);
			break;
		}
	}

	if (!socketEntry)
		return false;

	CellProperty * const cell = getCell(cellIndex);
	if (!cell)
		return false;

	float width = socketEntry->doorwayWidth > 0.01f ? socketEntry->doorwayWidth : 1.0f;
	float height = socketEntry->doorwayHeight > 0.01f ? socketEntry->doorwayHeight : 2.0f;

	Transform doorTransform = socketEntry->doorTransform_o2p;
	DoorwayBoundaryEdge boundaryEdge = DoorwayBoundaryEdge::invalid();
	Vector const preAxisK = doorTransform.getLocalFrameK_p();
	bool const wallPlacedSocket = fabsf(preAxisK.y) < 0.15f;
	if (wallPlacedSocket)
		ensurePortalTransformInward(*cell, doorTransform);
	else if (!snapCustomSocketPositionToFloorBoundary(*cell, doorTransform, &boundaryEdge))
		stabilizeCustomSocketTransform(*cell, doorTransform);
	else
		ensurePortalTransformInward(*cell, doorTransform);
	socketEntry->doorTransform_o2p = doorTransform;

	char const * const doorStyle = pickRuntimeDoorStyle(doorTransform);
	IGNORE_RETURN(Portal::lookupDoorStyleDimensions(doorStyle, width, height));
	float const halfWidth = width * 0.5f;

	std::vector<Vector> doorLocalVertices;
	doorLocalVertices.reserve(4);
	doorLocalVertices.push_back(Vector(-halfWidth, 0.0f, 0.0f));
	doorLocalVertices.push_back(Vector(halfWidth, 0.0f, 0.0f));
	doorLocalVertices.push_back(Vector(halfWidth, height, 0.0f));
	doorLocalVertices.push_back(Vector(-halfWidth, height, 0.0f));

	Vector const axisK = doorTransform.getLocalFrameK_p();
	std::vector<Vector> cellSpaceVertices;
	cellSpaceVertices.reserve(4);
	for (size_t vi = 0; vi < doorLocalVertices.size(); ++vi)
		cellSpaceVertices.push_back(doorTransform.rotateTranslate_l2p(doorLocalVertices[vi]));

	Plane3d const portalPlane(
		cellSpaceVertices[0],
		cellSpaceVertices[1],
		cellSpaceVertices[2]);
	bool geometryWindingClockwise = true;
	if (portalPlane.getNormal().dot(axisK) > 0.0f)
	{
		std::reverse(cellSpaceVertices.begin(), cellSpaceVertices.end());
		geometryWindingClockwise = false;
	}

	IndexedTriangleList * const geometry = new IndexedTriangleList;
	geometry->addTriangleFan(&cellSpaceVertices[0], static_cast<int>(cellSpaceVertices.size()));

	char const * const doorStyleName = pickRuntimeDoorStyle(doorTransform);
	PortalPropertyTemplateCellPortal * const portalTemplate = PortalPropertyTemplateCellPortal::createRuntime(
		geometry,
		doorTransform,
		doorStyleName,
		false,
		geometryWindingClockwise,
		true);
	if (!portalTemplate)
	{
		delete geometry;
		return false;
	}

	if (!m_runtimePortalTemplates)
		m_runtimePortalTemplates = new RuntimePortalTemplateList;
	m_runtimePortalTemplates->push_back(portalTemplate);

	int portalIndex = socketEntry->materializedPortalIndex;
	if (portalIndex >= 0 && portalIndex < cell->getPortalCount() && cell->getPortal(portalIndex))
	{
		if (!cell->replaceRuntimePortal(portalIndex, *portalTemplate))
		{
			portalIndex = cell->appendRuntimePortal(*portalTemplate);
			if (portalIndex < 0)
				return false;
		}
	}
	else
	{
		portalIndex = cell->appendRuntimePortal(*portalTemplate);
		if (portalIndex < 0)
			return false;
	}

	socketEntry->materializedPortalIndex = portalIndex;
	socketEntry->doorwayWidth = width;
	socketEntry->doorwayHeight = height;

	IGNORE_RETURN(prepareCustomSocketPortalOpening(cellIndex, customSocketIndex));

	return true;
}

// ----------------------------------------------------------------------

bool PortalProperty::prepareCustomSocketPortalOpening(int cellIndex, int customSocketIndex)
{
	NOT_NULL(m_customSockets);

	CustomSocket * socketEntry = 0;
	for (CustomSocketList::iterator it = m_customSockets->begin(); it != m_customSockets->end(); ++it)
	{
		if (it->cellIndex == cellIndex && it->socketIndex == customSocketIndex)
		{
			socketEntry = &(*it);
			break;
		}
	}

	if (!socketEntry || socketEntry->materializedPortalIndex < 0)
		return false;

	CellProperty * const cell = getCell(cellIndex);
	if (!cell)
		return false;

	Portal * const portal = cell->getPortal(socketEntry->materializedPortalIndex);
	if (!portal)
		return false;

	float width = socketEntry->doorwayWidth > 0.01f ? socketEntry->doorwayWidth : 1.0f;
	float height = socketEntry->doorwayHeight > 0.01f ? socketEntry->doorwayHeight : 2.0f;

	Transform doorTransform = socketEntry->doorTransform_o2p;
	DoorwayBoundaryEdge boundaryEdge = DoorwayBoundaryEdge::invalid();
	Vector const preAxisK = doorTransform.getLocalFrameK_p();
	bool const wallPlacedSocket = fabsf(preAxisK.y) < 0.15f;
	if (wallPlacedSocket)
		ensurePortalTransformInward(*cell, doorTransform);
	else if (!snapCustomSocketPositionToFloorBoundary(*cell, doorTransform, &boundaryEdge))
		stabilizeCustomSocketTransform(*cell, doorTransform);
	else
		ensurePortalTransformInward(*cell, doorTransform);
	socketEntry->doorTransform_o2p = doorTransform;

	char const * const doorStyle = pickRuntimeDoorStyle(doorTransform);
	IGNORE_RETURN(Portal::lookupDoorStyleDimensions(doorStyle, width, height));
	socketEntry->doorwayWidth = width;
	socketEntry->doorwayHeight = height;

	Floor * const floor = cell->getFloor();
	FloorMesh * const floorMesh = floor ? const_cast<FloorMesh *>(floor->getFloorMesh()) : 0;
	if (floorMesh)
	{
		clearCustomSocketFloorExtension(*socketEntry, floorMesh);

		int const portalIndex = socketEntry->materializedPortalIndex;
		bool flagged = flagCustomSocketPortalFloor(floorMesh, boundaryEdge, doorTransform, width, height, portalIndex);

		int extensionStartTri = -1;
		int extensionTriCount = 0;
		bool const extended = extendFloorThroughDoorway(floorMesh, doorTransform, width, 1.25f, portalIndex, extensionStartTri, extensionTriCount);
		if (extended)
		{
			socketEntry->floorExtensionStartTri = extensionStartTri;
			socketEntry->floorExtensionTriCount = extensionTriCount;
		}

		if (!flagged && !extended)
		{
			WARNING(true, ("PortalProperty::prepareCustomSocketPortalOpening - failed to flag floor edges for portal %d in cell %d", portalIndex, cellIndex));
		}
	}

	portal->refreshDpvsPortal();
	IGNORE_RETURN(cutWallMeshForCustomSocketPortal(cellIndex, customSocketIndex));

	return true;
}

// ----------------------------------------------------------------------

bool PortalProperty::finalizeCustomSocketPortalWalkthrough(int cellIndex, int customSocketIndex)
{
	if (!prepareCustomSocketPortalOpening(cellIndex, customSocketIndex))
		return false;

	CellProperty * const cell = getCell(cellIndex);
	if (!cell)
		return false;

	CustomSocket const * socketEntry = 0;
	for (CustomSocketList::const_iterator it = m_customSockets->begin(); it != m_customSockets->end(); ++it)
	{
		if (it->cellIndex == cellIndex && it->socketIndex == customSocketIndex)
		{
			socketEntry = &(*it);
			break;
		}
	}

	if (!socketEntry || socketEntry->materializedPortalIndex < 0)
		return false;

	Portal * const portal = cell->getPortal(socketEntry->materializedPortalIndex);
	if (!portal)
		return false;

	Portal * const neighborPortal = portal->getNeighbor();
	if (neighborPortal)
		neighborPortal->refreshDpvsPortal();

	return true;
}

// ----------------------------------------------------------------------

void PortalProperty::dematerializeAllCustomSocketPortals()
{
	if (!m_customSockets || m_customSockets->empty())
		return;

	struct PortalRemoval
	{
		int cellIndex;
		int portalIndex;
	};

	std::vector<PortalRemoval> removals;
	removals.reserve(m_customSockets->size());

	for (CustomSocketList::const_iterator it = m_customSockets->begin(); it != m_customSockets->end(); ++it)
	{
		if (it->materializedPortalIndex >= 0)
		{
			PortalRemoval removal;
			removal.cellIndex = it->cellIndex;
			removal.portalIndex = it->materializedPortalIndex;
			removals.push_back(removal);
		}
	}

	for (size_t i = 0; i < removals.size(); ++i)
	{
		for (size_t j = i + 1; j < removals.size(); ++j)
		{
			if (removals[j].cellIndex < removals[i].cellIndex ||
				(removals[j].cellIndex == removals[i].cellIndex && removals[j].portalIndex > removals[i].portalIndex))
			{
				PortalRemoval const temp = removals[i];
				removals[i] = removals[j];
				removals[j] = temp;
			}
		}
	}

	for (size_t i = 0; i < removals.size(); ++i)
	{
		PortalRemoval const & removal = removals[i];
		IGNORE_RETURN(unlinkCellPortal(removal.cellIndex, removal.portalIndex));

		CellProperty * const cell = getCell(removal.cellIndex);
		if (!cell)
			continue;

		Floor * const floor = cell->getFloor();
		FloorMesh * const floorMesh = floor ? const_cast<FloorMesh *>(floor->getFloorMesh()) : 0;

		for (CustomSocketList::iterator it = m_customSockets->begin(); it != m_customSockets->end(); ++it)
		{
			if (it->cellIndex == removal.cellIndex && it->materializedPortalIndex == removal.portalIndex)
			{
				if (floorMesh)
				{
					clearCustomSocketFloorExtension(*it, floorMesh);
					floorMesh->clearPortalEdges(removal.portalIndex);
				}
				it->materializedPortalIndex = -1;
				it->floorExtensionStartTri = -1;
				it->floorExtensionTriCount = 0;
				break;
			}
		}

		IGNORE_RETURN(cell->removeRuntimePortal(removal.portalIndex));
	}

	refreshAllCustomSocketWallCuts();
}

// ----------------------------------------------------------------------

bool PortalProperty::cutWallMeshForCustomSocketPortal(int cellIndex, int customSocketIndex)
{
	UNREF(customSocketIndex);
	refreshCellWallCuts(cellIndex);
	return true;
}

// ----------------------------------------------------------------------

void PortalProperty::refreshCellWallCuts(int cellIndex)
{
	CellProperty * const cell = getCell(cellIndex);
	if (!cell)
		return;

	PortalPropertyTemplateCell const & cellTemplate = getCellTemplate(cellIndex);
	BaseExtent const * const baseExtent = cellTemplate.getCollisionExtent();
	if (!baseExtent)
	{
		cell->clearRuntimeCollisionExtent();
		return;
	}

	CompositeExtent const * baseComposite = 0;
	CompositeExtent * result = 0;
	if (baseExtent->getType() == ET_Detail)
	{
		baseComposite = safe_cast<DetailExtent const *>(baseExtent);
		result = new DetailExtent();
	}
	else if (baseExtent->getType() == ET_Component)
	{
		baseComposite = safe_cast<ComponentExtent const *>(baseExtent);
		result = new ComponentExtent();
	}
	else
	{
		cell->clearRuntimeCollisionExtent();
		return;
	}

	for (int i = 0; i < baseComposite->getExtentCount(); ++i)
		result->attachExtent(baseComposite->getExtent(i)->clone());

	if (m_customSockets)
	{
		for (CustomSocketList::const_iterator it = m_customSockets->begin(); it != m_customSockets->end(); ++it)
		{
			if (it->cellIndex != cellIndex || it->materializedPortalIndex < 0)
				continue;

			float const width = it->doorwayWidth > 0.01f ? it->doorwayWidth : 1.0f;
			float const height = it->doorwayHeight > 0.01f ? it->doorwayHeight : 2.0f;
			AxialBox const doorwayBox = buildDoorwayCutBox(it->doorTransform_o2p, width, height, 0.60f);

			CompositeExtent * const next = (baseExtent->getType() == ET_Detail)
				? static_cast<CompositeExtent *>(static_cast<DetailExtent *>(new DetailExtent()))
				: static_cast<CompositeExtent *>(new ComponentExtent());
			for (int childIndex = 0; childIndex < result->getExtentCount(); ++childIndex)
			{
				BaseExtent const * const child = result->getExtent(childIndex);
				if (!extentIntersectsDoorway(child, doorwayBox))
				{
					next->attachExtent(child->clone());
					continue;
				}

				IGNORE_RETURN(attachExtentWithDoorwayOpening(next, child, it->doorTransform_o2p, width, height, doorwayBox));
			}
			delete result;
			result = next;
		}
	}

	if (result->getExtentCount() <= 0)
	{
		delete result;
		cell->clearRuntimeCollisionExtent();
	}
	else
		cell->setRuntimeCollisionExtent(result);
}

// ----------------------------------------------------------------------

void PortalProperty::refreshAllCustomSocketWallCuts()
{
	std::set<int> cellIndices;
	if (m_customSockets)
	{
		for (CustomSocketList::const_iterator it = m_customSockets->begin(); it != m_customSockets->end(); ++it)
		{
			if (it->materializedPortalIndex >= 0)
				cellIndices.insert(it->cellIndex);
		}
	}

	for (std::set<int>::const_iterator it = cellIndices.begin(); it != cellIndices.end(); ++it)
		refreshCellWallCuts(*it);

	int const cellCount = getNumberOfCells();
	for (int cellIndex = 1; cellIndex < cellCount; ++cellIndex)
	{
		if (cellIndices.find(cellIndex) == cellIndices.end())
		{
			CellProperty * const cell = getCell(cellIndex);
			if (cell)
				cell->clearRuntimeCollisionExtent();
		}
	}
}

// ----------------------------------------------------------------------

void PortalProperty::refreshDynamicGraftPortalDpvs()
{
	NOT_NULL(m_dynamicRoomGrafts);

	for (size_t graftIndex = 0; graftIndex < m_dynamicRoomGrafts->size(); ++graftIndex)
	{
		DynamicRoomGraft const & graft = (*m_dynamicRoomGrafts)[graftIndex];

		CellProperty * const hostCell = getCell(graft.hostCellIndex);
		CellProperty * const graftCell = getCell(graft.graftedCellIndex);
		if (!hostCell || !graftCell)
			continue;

		int hostPortalIndex = graft.hostPortalIndex;
		if (isCustomSocketIndex(graft.hostPortalIndex))
		{
			CustomSocket customSocket;
			if (!findCustomSocket(graft.hostCellIndex, graft.hostPortalIndex, customSocket))
				continue;
			if (customSocket.materializedPortalIndex < 0)
				continue;
			hostPortalIndex = customSocket.materializedPortalIndex;
		}

		int const graftPortalIndex = PortalProperty::resolveCellPortalIndex(graftCell, graft.graftedPortalIndex);
		if (hostPortalIndex < 0 || graftPortalIndex < 0)
			continue;

		Portal * const hostPortal = hostCell->getPortal(hostPortalIndex);
		Portal * const graftPortal = graftCell->getPortal(graftPortalIndex);
		if (!hostPortal || !graftPortal)
			continue;

		hostPortal->refreshDpvsPortal();
		graftPortal->refreshDpvsPortal();
	}
}

// ----------------------------------------------------------------------

bool PortalProperty::markCustomSocketOpen(int cellIndex, int socketIndex, bool open)
{
	NOT_NULL(m_customSockets);
	for (CustomSocketList::iterator it = m_customSockets->begin(); it != m_customSockets->end(); ++it)
	{
		if (it->cellIndex == cellIndex && it->socketIndex == socketIndex)
		{
			it->open = open;
			return true;
		}
	}
	return false;
}

// ----------------------------------------------------------------------

void PortalProperty::clearBridgeSegments()
{
	if (m_bridgeSegments)
		m_bridgeSegments->clear();
}

// ----------------------------------------------------------------------

void PortalProperty::recordBridgeSegment(BridgeSegment const &segment)
{
	NOT_NULL(m_bridgeSegments);
	for (BridgeSegmentList::iterator it = m_bridgeSegments->begin(); it != m_bridgeSegments->end(); ++it)
	{
		if (it->hostCellIndex == segment.hostCellIndex &&
			it->hostPortalIndex == segment.hostPortalIndex &&
			it->graftedCellIndex == segment.graftedCellIndex &&
			it->graftedPortalIndex == segment.graftedPortalIndex)
		{
			*it = segment;
			return;
		}
	}
	m_bridgeSegments->push_back(segment);
}

// ----------------------------------------------------------------------

PortalProperty::BridgeSegmentList const &PortalProperty::getBridgeSegments() const
{
	NOT_NULL(m_bridgeSegments);
	return *m_bridgeSegments;
}

// ----------------------------------------------------------------------

uint32 PortalProperty::computeEffectiveLayoutCrc() const
{
	uint32 crc = static_cast<uint32>(getCrc());

	DynamicRoomGraftList const &grafts = getDynamicRoomGrafts();
	for (size_t i = 0; i < grafts.size(); ++i)
	{
		DynamicRoomGraft const &graft = grafts[i];
		crc = Crc::calculate(&graft.graftedCellIndex, sizeof(graft.graftedCellIndex), crc);
		crc = Crc::calculate(&graft.hostCellIndex, sizeof(graft.hostCellIndex), crc);
		crc = Crc::calculate(&graft.hostPortalIndex, sizeof(graft.hostPortalIndex), crc);
		crc = Crc::calculate(&graft.graftedPortalIndex, sizeof(graft.graftedPortalIndex), crc);
		crc = Crc::calculate(&graft.donorCellIndex, sizeof(graft.donorCellIndex), crc);
		if (!graft.donorPobName.empty())
			crc = Crc::calculate(graft.donorPobName.c_str(), static_cast<int>(graft.donorPobName.size()), crc);

		uint32 donorCrc = 0;
		if (PortalPropertyTemplate::extractPortalLayoutCrc(graft.donorPobName.c_str(), donorCrc))
			crc = Crc::calculate(&donorCrc, sizeof(donorCrc), crc);
	}

	if (m_customSockets)
	{
		for (CustomSocketList::const_iterator it = m_customSockets->begin(); it != m_customSockets->end(); ++it)
		{
			CustomSocket const &socket = *it;
			crc = Crc::calculate(&socket.cellIndex, sizeof(socket.cellIndex), crc);
			crc = Crc::calculate(&socket.socketIndex, sizeof(socket.socketIndex), crc);
			if (!socket.label.empty())
				crc = Crc::calculate(socket.label.c_str(), static_cast<int>(socket.label.size()), crc);

			Vector const pos = socket.doorTransform_o2p.getPosition_p();
			crc = Crc::calculate(&pos, sizeof(pos), crc);

			int const openFlag = socket.open ? 1 : 0;
			crc = Crc::calculate(&openFlag, sizeof(openFlag), crc);
		}
	}

	return crc;
}

// ======================================================================

