// ======================================================================
//
// ClientBattlefieldMarkerOutlineObject.cpp
// asommers
//
// copyright 2003, sony online entertainment
//
// ======================================================================

#include "clientGame/FirstClientGame.h"
#include "clientGame/ClientBattlefieldMarkerOutlineObject.h"

#include "clientGame/ClientBattlefieldMarkerOutlineObjectNotification.h"
#include "clientGraphics/RenderWorld.h"
#include "clientObject/RibbonAppearance.h"
#include "sharedFile/Iff.h"
#include "sharedDebug/InstallTimer.h"
#include "sharedFoundation/TemporaryCrcString.h"
#include "sharedMath/VectorArgb.h"
#include "sharedObject/AppearanceTemplateList.h"
#include "sharedObject/ObjectList.h"
#include "sharedObject/StructureFootprint.h"
#include "sharedTerrain/TerrainObject.h"

#include <algorithm>
#include <vector>

// ======================================================================
// ClientBattlefieldMarkerOutlineObjectNamespace
// ======================================================================

namespace ClientBattlefieldMarkerOutlineObjectNamespace
{
	typedef std::vector<Vector> PointList;

	const Tag            TAG_OUTL = TAG (O,U,T,L);
	TemporaryCrcString   ms_appearanceTemplateName;
	VectorArgb           ms_color (0.5f, 1.f, 0.f, 0.f);
	float                ms_width  = 0.025f;
	float                ms_height = 1.f;
}

using namespace ClientBattlefieldMarkerOutlineObjectNamespace;

// ======================================================================
// STATIC PUBLIC ClientBattlefieldMarkerOutlineObject
// ======================================================================

int ClientBattlefieldMarkerOutlineObject::calculatePoleCountForRadius (float const radiusMeters)
{
	if (radiusMeters <= 0.f)
		return 8;

	int const poles = static_cast<int>(radiusMeters * 0.5f);
	return std::max (8, std::min (32, poles));
}

// ----------------------------------------------------------------------

void ClientBattlefieldMarkerOutlineObject::install ()
{
	InstallTimer const installTimer("ClientBattlefieldMarkerOutlineObject::install");

	Iff iff;
	if (iff.open ("object/client_battlefield_marker_outline_object.iff", true))
	{
		iff.enterForm (TAG_OUTL);

			iff.enterChunk (TAG_0000);

				ms_color  = iff.read_floatVectorArgb ();
				ms_width  = iff.read_float ();
				ms_height = iff.read_float ();

				char buffer [256];
				iff.read_string (buffer, 256);
				ms_appearanceTemplateName.set (buffer, true);

			iff.exitChunk (TAG_0000);

		iff.exitForm (TAG_OUTL);
	}
}

// ======================================================================
// PUBLIC ClientBattlefieldMarkerOutlineObject
// ======================================================================

ClientBattlefieldMarkerOutlineObject::ClientBattlefieldMarkerOutlineObject (const int numberOfPoles, const float radius) :
	Object (),
	m_objectList (new ObjectList),
	m_ribbonObject (0)
{
	addNotification (ClientBattlefieldMarkerOutlineObjectNotification::getInstance ());
	RenderWorld::addObjectNotifications (*this);

	DEBUG_FATAL (numberOfPoles == 0, (""));
	if (numberOfPoles > 0)
		create (numberOfPoles, radius);
}

// ----------------------------------------------------------------------

ClientBattlefieldMarkerOutlineObject::~ClientBattlefieldMarkerOutlineObject ()
{
	removeNotification (ClientBattlefieldMarkerOutlineObjectNotification::getInstance ());

	//-- these are added as child objects, so there is no need to delete them
	delete m_objectList;
	m_ribbonObject = 0;
}

// ----------------------------------------------------------------------

void ClientBattlefieldMarkerOutlineObject::resetBoundary ()
{
	if (!m_ribbonObject)
		return;

	const TerrainObject * const terrainObject = TerrainObject::getConstInstance ();
	if (!terrainObject)
		return;

	RibbonAppearance * const ribbonAppearance = dynamic_cast<RibbonAppearance *>(m_ribbonObject->getAppearance ());
	if (!ribbonAppearance)
		return;

	PointList pointList = ribbonAppearance->getPointList ();

	uint const poleCount = pointList.size ();
	for (uint i = 0; i < poleCount; ++i)
	{
		Object * const childObject = getChildObject (static_cast<int> (i));
		if (!childObject)
			continue;

		Vector point = rotateTranslate_o2w (pointList [i]);
		IGNORE_RETURN (terrainObject->getHeight (point, point.y));
		point = rotateTranslate_w2o (point);
		childObject->setPosition_p (point);

		pointList [i] = point + Vector::unitY * ms_height;
	}

	ribbonAppearance->setPointList (pointList);
}

// ----------------------------------------------------------------------

void ClientBattlefieldMarkerOutlineObject::setRibbonColor (VectorArgb const & color)
{
	if (!m_ribbonObject)
		return;

	RibbonAppearance const * const oldRibbon = safe_cast<RibbonAppearance const *> (m_ribbonObject->getAppearance ());
	if (!oldRibbon)
		return;

	RibbonAppearance::PointList const pointList = oldRibbon->getPointList ();
	m_ribbonObject->setAppearance (new RibbonAppearance (pointList, ms_width, color));
}

// ======================================================================
// PRIVATE ClientBattlefieldMarkerOutlineObject
// ======================================================================

void ClientBattlefieldMarkerOutlineObject::create (const int numberOfPoles, const float radius)
{
	PointList pointList;

	//-- create point list
	{
		Transform t;

		int i;
		for (i = 0; i < numberOfPoles; ++i)
		{
			t.yaw_l (PI_TIMES_2 / numberOfPoles);
			pointList.push_back (t.rotateTranslate_l2p (Vector::unitZ * radius));
		}
	}

	//-- take the point list and create the ribbon appearance and the markers
	{
		//-- create beacons
		uint i;
		for (i = 0; i < pointList.size (); ++i)
		{
			Object* const childObject = new Object ();
			childObject->setAppearance (AppearanceTemplateList::createAppearance (ms_appearanceTemplateName.getString ()));
			childObject->setPosition_p (pointList [i]);
			RenderWorld::addObjectNotifications (*childObject);
			addChildObject_p (childObject);

			m_objectList->addObject (childObject);
		}

		//-- create ribbon
		m_ribbonObject = new Object ();
		m_ribbonObject->setAppearance (new RibbonAppearance (pointList, ms_width, ms_color));
		RenderWorld::addObjectNotifications (*m_ribbonObject);
		addChildObject_p (m_ribbonObject);
	}
}

// ======================================================================
