// ======================================================================
//
// CallStack.h
// asommers
//
// Copyright 2004, Sony Online Entertainment
// All Rights Reserved
//
// ======================================================================

#ifndef INCLUDED_CallStack_H
#define INCLUDED_CallStack_H

#include <cstdint>

// ======================================================================

class CallStack
{
public:


public:

	CallStack();
	~CallStack();

	bool operator<(const CallStack &o) const;

	void sample();

	void debugPrint() const;
	void debugLog() const;

private:

	enum
	{
		//-- This is to avoid memory allocations and needing to free the memory
		S_callStack = 32
	};

private:

	std::uintptr_t m_callStack[S_callStack];
};

// ======================================================================

#endif
