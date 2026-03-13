// ======================================================================
//
// ImportLodMesh.h
// Copyright 2006 Sony Online Entertainment, Inc.
// All Rights Reserved.
//
// ======================================================================

#ifndef INCLUDED_ImportLodMesh_H
#define INCLUDED_ImportLodMesh_H

// ======================================================================

#include "maya/MPxCommand.h"

class Messenger;

// ======================================================================

class ImportLodMesh : public MPxCommand
{
public:

	static void    install(Messenger *newMessenger);
	static void    remove();
	static void   *creator();

public:

	MStatus        doIt(const MArgList &args);

private:

	ImportLodMesh();
	ImportLodMesh(const ImportLodMesh &);
	ImportLodMesh &operator=(const ImportLodMesh &);
};

// ======================================================================

#endif
