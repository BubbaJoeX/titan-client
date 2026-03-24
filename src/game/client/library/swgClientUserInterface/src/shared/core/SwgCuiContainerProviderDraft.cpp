//======================================================================
//
// SwgCuiContainerProviderDraft.cpp
// copyright (c) 2002 Sony Online Entertainment
//
//======================================================================

#include "swgClientUserInterface/FirstSwgClientUserInterface.h"
#include "swgClientUserInterface/SwgCuiContainerProviderDraft.h"

#include "swgClientUserInterface/SwgCuiContainerProviderFilter.h"
#include "sharedMessageDispatch/Transceiver.h"
#include "clientGame/DraftSchematicManager.h"
#include "clientGame/DraftSchematicInfo.h"
#include "clientGame/Game.h"
#include "clientGame/CreatureObject.h"
#include <algorithm>

//======================================================================

const size_t SwgCuiContainerProviderDraft::cms_initialDraftIconBatch    = static_cast<size_t>(DraftSchematicInfo::ms_maxDraftsFullSkeletalAppearance);
const size_t SwgCuiContainerProviderDraft::cms_lazyDraftIconIncrement  = 40;

//======================================================================

SwgCuiContainerProviderDraft::SwgCuiContainerProviderDraft () :
SwgCuiContainerProvider (),
m_objects               (new ObjectWatcherVector),
m_callback              (new MessageDispatch::Callback),
m_lazyVisibleDraftCount (cms_initialDraftIconBatch),
m_lastDraftListSize     (0),
m_bypassLazyLoadForSearch (false)
{
	m_callback->connect (*this, &SwgCuiContainerProviderDraft::onDraftsChanged,          static_cast<DraftSchematicManager::Messages::Changed*> (0));
}

//----------------------------------------------------------------------

SwgCuiContainerProviderDraft::~SwgCuiContainerProviderDraft ()
{
	delete m_callback;
	m_callback = 0;

	delete m_objects;
	m_objects = 0;
}

//----------------------------------------------------------------------

void SwgCuiContainerProviderDraft::onDraftsChanged (const DraftSchematicManager::Messages::Changed::Payload & creature)
{
	if (static_cast<const Object *>(Game::getPlayer ()) == &creature)
	{
		m_lazyVisibleDraftCount = cms_initialDraftIconBatch;
		setContentDirty (true);
	}
}

//----------------------------------------------------------------------

void SwgCuiContainerProviderDraft::getObjectVector        (ObjectWatcherVector & owv)
{
	if (isContentDirty ())
		updateObjectVector ();

	owv = *m_objects;
}

//----------------------------------------------------------------------

bool SwgCuiContainerProviderDraft::populateFromDrafts (const ObjectSet & currentObjSet, ObjectSet & actualObjSet)
{
	bool changed = false;

	//----------------------------------------------------------------------
	//-- add object from new container

	const Filter * const filter = getFilter ();

	DraftSchematicManager::InfoVector iv;

	DraftSchematicManager::getPlayerDraftSchematics (iv);

	// Beyond ms_maxDraftsFullSkeletalAppearance, never build crafted skeletal previews (memory-safe icon/table rows).
	DraftSchematicInfo::setUseMinimalClientPreviewsForDatapad (
		iv.size () > static_cast<size_t>(DraftSchematicInfo::ms_maxDraftsFullSkeletalAppearance));

	m_lastDraftListSize = iv.size ();
	if (m_lazyVisibleDraftCount > m_lastDraftListSize)
		m_lazyVisibleDraftCount = m_lastDraftListSize;

	const size_t materializeCount = m_bypassLazyLoadForSearch ? m_lastDraftListSize : std::min (m_lastDraftListSize, m_lazyVisibleDraftCount);

	size_t draftIndex = 0;
	for (DraftSchematicManager::InfoVector::const_iterator it = iv.begin (); it != iv.end (); ++it)
	{
		++draftIndex;
		if (draftIndex > materializeCount)
			break;

		const DraftSchematicInfo * const info = *it;
		NOT_NULL (info);

		ClientObject * const obj = info->getClientObject ();
		
		if (obj)
		{
			//-- filter the object
			if (filter)
			{
				if (!filter->showObject (*obj))
					continue;
			}
			
			if (!obj->getAppearance ())
			{
				continue;
			}

			actualObjSet.insert (obj);
			if (currentObjSet.find (obj) == currentObjSet.end ())
			{
				m_objects->push_back (ObjectWatcher (obj));
				changed = true;
			}
		}
	}

	return changed;
}

//----------------------------------------------------------------------

void SwgCuiContainerProviderDraft::updateObjectVector ()
{
	bool changed = false;
	
	ObjectSet currentObjSet;
	
	{
		const Filter * const filter = getFilter ();
		
		for (ObjectWatcherVector::iterator it = m_objects->begin (); it != m_objects->end ();)
		{
			const ObjectWatcher & watcher = *it;
			const ClientObject * const obj = watcher.getPointer ();
			
			//-- re-filter the object in case filter or object has changed
			if (obj && (!filter || filter->showObject (*obj)))
			{
				currentObjSet.insert (obj);
				++it;
			}
			else
			{
				it = m_objects->erase (it);
				changed = true;
			}
		}
	}
	
	ObjectSet actualObjSet;

	populateFromDrafts (currentObjSet, actualObjSet);
	
	//----------------------------------------------------------------------
	//-- remove objects that are no longer in the container (s)

	{
		for (ObjectSet::iterator it = currentObjSet.begin (); it != currentObjSet.end (); ++it)
		{
			const ClientObject * obj = *it;
			const ObjectSet::iterator fit = actualObjSet.find (obj);
			if (fit == actualObjSet.end ())
			{
				m_objects->erase (std::remove (m_objects->begin (), m_objects->end (), obj), m_objects->end ());
				changed = true;

			}
			//-- erase the element from the set to speed up the find in future iterations of this loop
			else
				actualObjSet.erase (fit);
		}
	}

	setContentDirty (false);
}

//----------------------------------------------------------------------

void SwgCuiContainerProviderDraft::setObjectSorting (const IntVector & iv)
{
	if (iv.size () != m_objects->size ())
	{
		WARNING (true, ("bad vector sizes"));
		return;
	}

	ObjectWatcherVector newObjects;
	const int size = m_objects->size ();
	newObjects.reserve (size);

	int which = 0;

	for (IntVector::const_iterator it = iv.begin (); it != iv.end (); ++it, ++which)
	{
		const int index = *it;

		if (index < 0 || index >= size)
		{
			DEBUG_FATAL (true, ("bad index %d for sort at position %d", index, which));
			return;
		}
		ClientObject * const obj = (*m_objects) [index];
		DEBUG_WARNING (obj == NULL, ("setObjectSorting(): NULL object found, unexpected"));
		newObjects.push_back (ObjectWatcher (obj));
	}

	*m_objects = newObjects;

	setContentDirty (true);
}

//----------------------------------------------------------------------

void SwgCuiContainerProviderDraft::tickLazyContentLoad ()
{
	if (m_bypassLazyLoadForSearch)
		return;
	if (m_lazyVisibleDraftCount >= m_lastDraftListSize)
		return;

	const size_t next = std::min (m_lastDraftListSize, m_lazyVisibleDraftCount + cms_lazyDraftIconIncrement);
	if (next != m_lazyVisibleDraftCount)
	{
		m_lazyVisibleDraftCount = next;
		setContentDirty (true);
	}
}

//----------------------------------------------------------------------

void SwgCuiContainerProviderDraft::setBypassLazyLoadForSearch (bool bypass)
{
	if (m_bypassLazyLoadForSearch == bypass)
		return;
	m_bypassLazyLoadForSearch = bypass;
	setContentDirty (true);
}

//======================================================================
