// ======================================================================
//
// ClientBattlefieldMarkerOutlineObject.h
// asommers
//
// copyright 2003, sony online entertainment
//
// ======================================================================

#ifndef INCLUDED_ClientBattlefieldMarkerOutlineObject_H
#define INCLUDED_ClientBattlefieldMarkerOutlineObject_H

// ======================================================================

#include "sharedObject/Object.h"

class ObjectList;
class VectorArgb;

// ======================================================================

class ClientBattlefieldMarkerOutlineObject : public Object
{
public:

	static void install ();

	/** Pole count for circular claim / battlefield outlines (~1 pole per 4m, clamped 8–32). */
	static int calculatePoleCountForRadius (float radiusMeters);

public:

	ClientBattlefieldMarkerOutlineObject (int numberOfPoles, float radius);
	virtual ~ClientBattlefieldMarkerOutlineObject ();

	void resetBoundary ();
	void setRibbonColor (VectorArgb const & color);

private:

	void create (int numberOfPoles, float radius);

private:

	ClientBattlefieldMarkerOutlineObject ();
	ClientBattlefieldMarkerOutlineObject (const ClientBattlefieldMarkerOutlineObject&);
	ClientBattlefieldMarkerOutlineObject& operator= (const ClientBattlefieldMarkerOutlineObject&);
	
private:

	ObjectList* const m_objectList;
	Object*           m_ribbonObject;
};

// ======================================================================

#endif

