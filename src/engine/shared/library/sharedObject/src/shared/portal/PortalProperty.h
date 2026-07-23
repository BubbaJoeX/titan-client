// ======================================================================
//
// PortalProperty.h
// Copyright 2001 Sony Online Entertainment
// All Rights Reserved.
//
// ======================================================================

#ifndef INCLUDED_PortalProperty_H
#define INCLUDED_PortalProperty_H

// ======================================================================

class CellProperty;
class Floor;
class Iff;
class Object;
class Portal;
class PortalPropertyTemplate;
class PortalPropertyTemplateCell;
class PortalPropertyTemplateCellPortal;
class Vector;

#include "sharedObject/Container.h"
#include "sharedMath/Transform.h"

#include <map>
#include <string>
#include <vector>

// ======================================================================

class PortalProperty : public Container
{
	class Notification;

	friend class CellProperty;
	friend class RenderWorld;
	friend class Notification;

public:

	static PropertyId getClassPropertyId();

public:

	typedef std::vector<Portal *>        PortalList;
	typedef std::vector<const char *>    CellNameList;
	typedef std::vector<Vector>          VertexList;

	typedef Object *(*BeginCreateObjectFunction)(int cellIndex);
	typedef void    (*EndCreateObjectFunction)(Object *newObject);

public:

	static void install(BeginCreateObjectFunction beginCreateObjectFunction, EndCreateObjectFunction endCreateObjectFunction);

public:

	PortalProperty(Object &owner, const char *portalPropertyFileName);
	virtual ~PortalProperty();

	virtual void                  initializeFirstTimeObject();
	void                          clientSinglePlayerInitializeFirstTimeObject();

	virtual void                  addToWorld();
	virtual void                  removeFromWorld();

	virtual bool                  isContentItemObservedWith(Object const &item) const;
	virtual bool                  isContentItemExposedWith(Object const &item) const;
	virtual bool                  canContentsBeObservedWith() const;

	virtual bool                  mayAdd(const Object &item, ContainerErrorCode& error) const;
	virtual bool                  remove(Object &item, ContainerErrorCode& error);
	virtual bool                  remove(ContainerIterator &pos, ContainerErrorCode& error);
	virtual int                   getTypeId() const;
	virtual void                  debugPrint(std::string &buffer) const;

	bool                          serverEndBaselines(int crc,std::vector<Object*> &unfixables, bool authoritative);

	const PortalPropertyTemplate &getPortalPropertyTemplate() const;
	int                           getCrc() const;
	const char                   *getPobName() const;
	const char                   *getPobShortName() const;
	int                           getNumberOfCells() const;
	CellProperty                 *getCell(int index);
	const CellProperty           *getCell(int index) const;
	const char                   *getCellAppearanceName(int index) const;

	const CellNameList           &getCellNames() const;
	CellProperty                 *getCell(const char *cellName);
	const CellProperty           *getCell(const char *cellName) const;

	Transform const               getEjectionLocationTransform() const;

	char const                   *getExteriorFloorName() const;

	void                          createAppearance();
	void                          cellLoaded(int cellIndex, Object &cellObject, bool shouldCreateAppearance);

	bool                          findContainingCell(Vector const & buildingPos, Vector & outPos, Object * & outCell);
	void                          queueObjectForFixup(Object &object);
	bool                          fixupObject(Object &object, Transform intendedTransform);
	bool                          hasPassablePortalToParentCell() const;
	CellProperty const * findContainingCell(Vector const & position_l) const;

	// Dynamic bunker room grafting: reserve a synthetic cell index, register a donor
	// POB cell as its template source, then create/load a CellObject into that index
	// and call linkCellPortals() to snap the inward portals together.
	struct DynamicRoomGraft
	{
		int graftedCellIndex;
		int hostCellIndex;
		int hostPortalIndex;
		int graftedPortalIndex;
		int donorCellIndex;
		std::string donorPobName;
	};

	typedef std::vector<DynamicRoomGraft> DynamicRoomGraftList;

	int                           getBaseTemplateCellCount() const;
	PortalPropertyTemplateCell const &getCellTemplate(int cellIndex) const;
	bool                          isGraftedCell(int cellIndex) const;
	int                           reserveGraftedCellSlot(char const *donorPobName, int donorCellIndex);
	bool                          ensureGraftedCellSlot(int graftedCellIndex, char const *donorPobName, int donorCellIndex);
	bool                          addCellObject(Object &cellObject, ContainerErrorCode &error);
	bool                          linkCellPortals(int cellIndexA, int portalIndexA, int cellIndexB, int portalIndexB);
	bool                          unlinkCellPortal(int cellIndex, int portalIndex);
	void                          unlinkAllCellPortals(int cellIndex);
	bool                          clearLoadedCellSlot(int cellIndex);
	bool                          releaseGraftedCellSlot(int graftedCellIndex);
	bool                          computeGraftCellTransform(int hostCellIndex, int hostPortalIndex, char const *donorPobName, int donorCellIndex, int donorPortalIndex, Transform &outCellTransform_o2p) const;
	bool                          recordDynamicRoomGraft(DynamicRoomGraft const &graft);
	bool                          removeDynamicRoomGraft(int graftedCellIndex);
	bool                          findDynamicRoomGraftForSocket(int cellIndex, int portalIndex, DynamicRoomGraft &outGraft) const;
	DynamicRoomGraftList const   &getDynamicRoomGrafts() const;

	static int const              cms_customSocketBase;

	struct CustomSocket
	{
		int         cellIndex;
		int         socketIndex;
		std::string label;
		Transform   doorTransform_o2p;
		bool        open;
		int         materializedPortalIndex;
		float       doorwayWidth;
		float       doorwayHeight;
	};

	typedef std::vector<CustomSocket> CustomSocketList;

	struct BridgeSegment
	{
		int       hostCellIndex;
		int       hostPortalIndex;
		int       graftedCellIndex;
		int       graftedPortalIndex;
		Transform transform_o2p;
		float     length;
		float     width;
		float     height;
	};

	typedef std::vector<BridgeSegment> BridgeSegmentList;

	int                           allocateCustomSocketIndex() const;
	bool                          addCustomSocket(CustomSocket const &socket);
	bool                          removeCustomSocket(int socketIndex);
	void                          clearCustomSockets();
	CustomSocketList const       &getCustomSockets() const;
	bool                          findCustomSocket(int cellIndex, int portalIndex, CustomSocket &outSocket) const;
	static bool                   isCustomSocketIndex(int portalIndex);
	uint32                        computeEffectiveLayoutCrc() const;
	bool                          getPortalSocketTransform_o2p(int cellIndex, int portalIndex, Transform &outTransform_o2p) const;
	bool                          getPortalNeighbor(int cellIndex, int portalIndex, int &outNeighborCellIndex, int &outNeighborPortalIndex) const;
	bool                          linkCustomSocketGraft(int hostCellIndex, int customSocketIndex, int graftCellIndex, int graftPortalIndex);
	bool                          materializeCustomSocketPortal(int cellIndex, int customSocketIndex);
	bool                          markCustomSocketOpen(int cellIndex, int socketIndex, bool open);

	void                          clearBridgeSegments();
	void                          recordBridgeSegment(BridgeSegment const &segment);
	BridgeSegmentList const      &getBridgeSegments() const;

	struct PortalSocketInfo
	{
		int  cellIndex;
		int  portalIndex;
		bool open;
		bool passable;
	};

	typedef std::vector<PortalSocketInfo> PortalSocketInfoList;
	void                          collectPortalSockets(PortalSocketInfoList &outSockets) const;

private:

	PortalProperty();
	PortalProperty(const PortalProperty &);
	PortalProperty &operator =(const PortalProperty &);

private:

	struct FixupRec
	{
		Object    *m_obj;
		Transform  m_transform;
	};
	typedef std::vector<FixupRec>        FixupList;
	typedef std::vector<CellProperty*>   CellList;

	struct GraftedCellRecord
	{
		PortalPropertyTemplate const *donorTemplate;
		int                           donorCellIndex;
	};

	typedef std::map<int, GraftedCellRecord> GraftedCellMap;
	typedef std::vector<PortalPropertyTemplateCellPortal *> RuntimePortalTemplateList;

private:

	static BeginCreateObjectFunction    ms_beginCreateObjectFunction;
	static EndCreateObjectFunction      ms_endCreateObjectFunction;
	static Notification                 ms_notification;

private:

	const PortalPropertyTemplate  *m_template;
	CellList                      *m_cellList;
	FixupList                     *m_fixupList;
	bool                           m_hasPassablePortalToParentCell;
	GraftedCellMap                *m_graftedCellMap;
	DynamicRoomGraftList          *m_dynamicRoomGrafts;
	CustomSocketList              *m_customSockets;
	BridgeSegmentList             *m_bridgeSegments;
	RuntimePortalTemplateList     *m_runtimePortalTemplates;
};

// ======================================================================

#endif
