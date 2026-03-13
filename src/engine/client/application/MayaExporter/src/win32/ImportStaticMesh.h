// ======================================================================
//
// ImportStaticMesh.h
// Copyright 2006 Sony Online Entertainment, Inc.
// All Rights Reserved.
//
// ======================================================================

#ifndef INCLUDED_ImportStaticMesh_H
#define INCLUDED_ImportStaticMesh_H

// ======================================================================

#include "maya/MPxCommand.h"

class Messenger;

// ======================================================================

class ImportStaticMesh : public MPxCommand
{
public:

	static void    install(Messenger *newMessenger);
	static void    remove();
	static void   *creator();

public:

	MStatus        doIt(const MArgList &args);

private:

	ImportStaticMesh();
	ImportStaticMesh(const ImportStaticMesh &);
	ImportStaticMesh &operator=(const ImportStaticMesh &);
};

// ======================================================================

#endif
