// ======================================================================
//
// ImportSat.h
// Copyright 2006 Sony Online Entertainment, Inc.
// All Rights Reserved.
//
// ======================================================================

#ifndef INCLUDED_ImportSat_H
#define INCLUDED_ImportSat_H

// ======================================================================

#include "maya/MPxCommand.h"

class Messenger;

// ======================================================================

class ImportSat : public MPxCommand
{
public:

	static void    install(Messenger *newMessenger);
	static void    remove();
	static void   *creator();

public:

	MStatus        doIt(const MArgList &args);

private:

	ImportSat();
	ImportSat(const ImportSat &);
	ImportSat &operator=(const ImportSat &);
};

// ======================================================================

#endif
