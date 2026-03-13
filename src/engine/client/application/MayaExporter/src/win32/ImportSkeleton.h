// ======================================================================
//
// ImportSkeleton.h
// Copyright 2006 Sony Online Entertainment, Inc.
// All Rights Reserved.
//
// ======================================================================

#ifndef INCLUDED_ImportSkeleton_H
#define INCLUDED_ImportSkeleton_H

// ======================================================================

#include "maya/MPxCommand.h"

class Messenger;

// ======================================================================

class ImportSkeleton : public MPxCommand
{
public:

	static void    install(Messenger *newMessenger);
	static void    remove();
	static void   *creator();

public:

	MStatus        doIt(const MArgList &args);

private:

	ImportSkeleton();
	ImportSkeleton(const ImportSkeleton &);
	ImportSkeleton &operator=(const ImportSkeleton &);
};

// ======================================================================

#endif
