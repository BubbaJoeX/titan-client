// ======================================================================
//
// LeakFinder.h
// bearhart
//
// Copyright 2005, Sony Online Entertainment
// All Rights Reserved
//
// ======================================================================

#ifndef INCLUDED_LeakFinder_H
#define INCLUDED_LeakFinder_H

// ======================================================================

#include "sharedDebug/CallStack.h"

#include <vector>
#include <list>
#ifdef _WIN64
#include <unordered_map>
#else
#include <hash_map>
#endif

// ======================================================================

class LeakFinder
{
public:

	// --------------------------------------------------------

	LeakFinder();
	~LeakFinder();

	// --------------------------------------------------------

	void clear()                                             { liveObjects.clear(); }

	void onAllocate(void *object);
	void onFree(void *object);

	void onReference(void *object);
	void onDereference(void *object);

	// --------------------------------------------------------

	struct ReferenceCountChange
	{
		enum ChangeDirection { up, down };

		ReferenceCountChange() : dir(up) {}

		ChangeDirection dir;
		CallStack       callStack;
	};

	typedef std::list<ReferenceCountChange> ReferenceCountingData;

	struct LiveObject
	{
		void                                  *object;
		CallStack                              callStack;
		const ReferenceCountingData           *referenceData;
	};
	typedef std::vector<LiveObject> LiveObjectList;

	// --------------------------------------------------------

	bool empty()                                       const { return liveObjects.empty(); }
	int  size()                                        const { return liveObjects.size(); }
	void getCurrentObjects(LiveObjectList &o_objects)  const;

	// --------------------------------------------------------

	void debugPrint() const;

protected:

	void _printReferenceCountingData(const ReferenceCountingData &) const;

	struct ptr_hash {
#ifdef _WIN64
		size_t operator()(void *p) const { return std::hash<uintptr_t>()(reinterpret_cast<uintptr_t>(p)); }
#else
		size_t operator()(void *p) const { return std::hash<unsigned long>()((unsigned long)p); }
#endif
	};

	struct ObjectData
	{
		ObjectData() : referenceData(0) { }
		~ObjectData() { if (referenceData) { delete referenceData; } }

		CallStack callStack;
		ReferenceCountingData *referenceData;
	};

#ifdef _WIN64
	typedef std::unordered_map<void *, ObjectData, ptr_hash> ObjectMap;
#else
	typedef std::hash_map<void *, ObjectData, ptr_hash> ObjectMap;
#endif

	ObjectMap liveObjects;
};

// ======================================================================

#endif
