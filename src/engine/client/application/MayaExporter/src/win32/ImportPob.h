// ======================================================================
//
// ImportPob.h
// Copyright 2006 Sony Online Entertainment, Inc.
// All Rights Reserved.
//
// ======================================================================

#ifndef INCLUDED_ImportPob_H
#define INCLUDED_ImportPob_H

// ======================================================================

#include "maya/MPxCommand.h"

class Messenger;

// ======================================================================

class ImportPob : public MPxCommand
{
public:

	static void    install(Messenger *newMessenger);
	static void    remove();
	static void   *creator();

public:

	MStatus        doIt(const MArgList &args);

private:

	ImportPob();
	ImportPob(const ImportPob &);
	ImportPob &operator=(const ImportPob &);
};

// ======================================================================

#endif
