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
#include "sharedCollision/Floor.h"
#include "sharedCollision/FloorManager.h"
#include "sharedCollision/FloorLocator.h"
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
#include "sharedMath/IndexedTriangleList.h"

#include <algorithm>
#include <limits>
#include <cstdio>

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

void PortalProperty::cellLoaded(int cellIndex, Object &cellObject, bool shouldCreateAppearance)
{
	DEBUG_FATAL(cellIndex < 1 || cellIndex >= static_cast<int>(m_cellList->size()), ("invalid cell index %d/%d", cellIndex, static_cast<int>(m_cellList->size())));
	DEBUG_FATAL((*m_cellList)[static_cast<CellList::size_type>(cellIndex)], ("cell already loaded"));
	if ((*m_cellList)[static_cast<CellList::size_type>(cellIndex)])
	{
		WARNING(true, ("CellProblem for portal %s cell index %d already loaded", getOwner().getNetworkId().getValueString().c_str(), cellIndex));
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

CellProperty *PortalProperty::getCell(int index)
{
	VALIDATE_RANGE_INCLUSIVE_EXCLUSIVE(0, index, getNumberOfCells());
	return (*m_cellList)[static_cast<CellList::size_type>(index)];
}

// ----------------------------------------------------------------------

const CellProperty *PortalProperty::getCell(int index) const
{
	VALIDATE_RANGE_INCLUSIVE_EXCLUSIVE(0, index, getNumberOfCells());
	return (*m_cellList)[static_cast<CellList::size_type>(index)];
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
	return m_template->getShortName().getString();
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

void PortalProperty::unlinkAllCellPortals(int cellIndex)
{
	CellProperty *const cell = getCell(cellIndex);
	if (!cell)
		return;

	int const portalCount = cell->getPortalCount();
	for (int portalIndex = 0; portalIndex < portalCount; ++portalIndex)
	{
		Portal *const portal = cell->getPortal(portalIndex);
		if (portal)
			portal->clearNeighbor();
	}
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

bool PortalProperty::computeGraftCellTransform(int hostCellIndex, int hostPortalIndex, char const *donorPobName, int donorCellIndex, int donorPortalIndex, Transform &outCellTransform_o2p) const
{
	NOT_NULL(donorPobName);

	CellProperty const *const hostCell = getCell(hostCellIndex);
	if (!hostCell)
		return false;

	Transform hostPortal_cell;
	if (PortalProperty::isCustomSocketIndex(hostPortalIndex))
	{
		CustomSocket customSocket;
		if (!findCustomSocket(hostCellIndex, hostPortalIndex, customSocket))
			return false;
		hostPortal_cell = customSocket.doorTransform_o2p;
	}
	else
	{
		Portal const *const hostPortal = const_cast<CellProperty *>(hostCell)->getPortal(hostPortalIndex);
		if (!hostPortal)
			return false;
		hostPortal_cell = hostPortal->getDoorTransform();
	}

	PortalPropertyTemplate const *const donorTemplate = PortalPropertyTemplateList::fetch(CrcLowerString(donorPobName));
	if (!donorTemplate)
		return false;

	bool ok = false;
	if (donorCellIndex >= 1 && donorCellIndex < donorTemplate->getNumberOfCells())
	{
		PortalPropertyTemplateCell const &donorCell = donorTemplate->getCell(donorCellIndex);
		PortalPropertyTemplateCell::PortalPropertyTemplateCellPortalList const *const portalList = donorCell.getPortalList();
		if (portalList && donorPortalIndex >= 0 && donorPortalIndex < static_cast<int>(portalList->size()))
		{
			Transform hostPortal_building;
			hostPortal_building.multiply(hostCell->getOwner().getTransform_o2p(), hostPortal_cell);

			Transform flip;
			flip.yaw_l(PI);

			Transform desiredPortal_building;
			desiredPortal_building.multiply(hostPortal_building, flip);

			Transform const donorPortal_cell = (*portalList)[static_cast<size_t>(donorPortalIndex)]->getDoorTransform();
			Transform invDonorPortal;
			invDonorPortal.invert(donorPortal_cell);

			outCellTransform_o2p.multiply(desiredPortal_building, invDonorPortal);
			ok = true;
		}
	}

	donorTemplate->release();
	return ok;
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
	int const cellCount = getNumberOfCells();
	for (int cellIndex = 1; cellIndex < cellCount; ++cellIndex)
	{
		CellProperty *const cell = const_cast<PortalProperty *>(this)->getCell(cellIndex);
		if (!cell)
			continue;

		int const portalCount = cell->getPortalCount();
		for (int portalIndex = 0; portalIndex < portalCount; ++portalIndex)
		{
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
			*it = socket;
			if (it->materializedPortalIndex < 0)
				it->materializedPortalIndex = -1;
			if (it->doorwayWidth < 0.01f)
				it->doorwayWidth = 1.0f;
			if (it->doorwayHeight < 0.01f)
				it->doorwayHeight = 2.0f;
			return true;
		}
	}
	CustomSocket entry = socket;
	if (entry.materializedPortalIndex < 0)
		entry.materializedPortalIndex = -1;
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

// ----------------------------------------------------------------------

bool PortalProperty::getPortalSocketTransform_o2p(int cellIndex, int portalIndex, Transform &outTransform_o2p) const
{
	CellProperty const *const cell = getCell(cellIndex);
	if (!cell)
		return false;

	if (isCustomSocketIndex(portalIndex))
	{
		CustomSocket customSocket;
		if (!findCustomSocket(cellIndex, portalIndex, customSocket))
			return false;
		outTransform_o2p.multiply(cell->getOwner().getTransform_o2p(), customSocket.doorTransform_o2p);
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

bool PortalProperty::linkCustomSocketGraft(int hostCellIndex, int customSocketIndex, int graftCellIndex, int graftPortalIndex)
{
	CustomSocket customSocket;
	if (!findCustomSocket(hostCellIndex, customSocketIndex, customSocket))
		return false;

	CellProperty *const hostCell = getCell(hostCellIndex);
	if (!hostCell || !getCell(graftCellIndex))
		return false;

	int hostPortalIndex = customSocket.materializedPortalIndex;
	if (hostPortalIndex < 0)
	{
		Transform customPortal_building;
		customPortal_building.multiply(hostCell->getOwner().getTransform_o2p(), customSocket.doorTransform_o2p);
		Vector const customPos = customPortal_building.getPosition_p();

		int bestPortal = -1;
		float bestDistSq = std::numeric_limits<float>::max();
		int const portalCount = hostCell->getPortalCount();
		for (int portalIndex = 0; portalIndex < portalCount; ++portalIndex)
		{
			Portal *const portal = hostCell->getPortal(portalIndex);
			if (!portal || !portal->isPassable())
			 continue;

			Transform portal_building;
			portal_building.multiply(hostCell->getOwner().getTransform_o2p(), portal->getDoorTransform());
			Vector const delta = portal_building.getPosition_p() - customPos;
			float const distSq = delta.magnitudeSquared();
			if (distSq < bestDistSq)
			{
				bestDistSq = distSq;
				bestPortal = portalIndex;
			}
		}

		if (bestPortal >= 0 && bestDistSq <= 256.0f)
			hostPortalIndex = bestPortal;
	}

	if (hostPortalIndex < 0)
	{
		if (!materializeCustomSocketPortal(hostCellIndex, customSocketIndex))
			return false;
		if (!findCustomSocket(hostCellIndex, customSocketIndex, customSocket))
			return false;
		hostPortalIndex = customSocket.materializedPortalIndex;
	}

	if (hostPortalIndex < 0)
		return false;

	return linkCellPortals(hostCellIndex, hostPortalIndex, graftCellIndex, graftPortalIndex);
}

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

	if (socketEntry->materializedPortalIndex >= 0)
	{
		CellProperty * const cell = getCell(cellIndex);
		if (cell && cell->getPortal(socketEntry->materializedPortalIndex))
			return true;
		socketEntry->materializedPortalIndex = -1;
	}

	CellProperty * const cell = getCell(cellIndex);
	if (!cell)
		return false;

	float const width = socketEntry->doorwayWidth > 0.01f ? socketEntry->doorwayWidth : 1.0f;
	float const height = socketEntry->doorwayHeight > 0.01f ? socketEntry->doorwayHeight : 2.0f;
	float const halfWidth = width * 0.5f;

	std::vector<Vector> vertices;
	vertices.push_back(Vector(-halfWidth, 0.0f, 0.0f));
	vertices.push_back(Vector( halfWidth, 0.0f, 0.0f));
	vertices.push_back(Vector( halfWidth, height, 0.0f));
	vertices.push_back(Vector(-halfWidth, height, 0.0f));

	IndexedTriangleList * const geometry = new IndexedTriangleList;
	geometry->addTriangleFan(&vertices[0], static_cast<int>(vertices.size()));

	PortalPropertyTemplateCellPortal * const portalTemplate = PortalPropertyTemplateCellPortal::createRuntime(
		geometry,
		socketEntry->doorTransform_o2p,
		"default");
	if (!portalTemplate)
	{
		delete geometry;
		return false;
	}

	if (!m_runtimePortalTemplates)
		m_runtimePortalTemplates = new RuntimePortalTemplateList;
	m_runtimePortalTemplates->push_back(portalTemplate);

	int const portalIndex = cell->appendRuntimePortal(*portalTemplate);
	if (portalIndex < 0)
		return false;

	socketEntry->materializedPortalIndex = portalIndex;
	return true;
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

