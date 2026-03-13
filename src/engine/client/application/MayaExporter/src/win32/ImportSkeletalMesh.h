// ======================================================================
//
// ImportSkeletalMesh.h
// Copyright 2006 Sony Online Entertainment, Inc.
// All Rights Reserved.
//
// ======================================================================

#ifndef INCLUDED_ImportSkeletalMesh_H
#define INCLUDED_ImportSkeletalMesh_H

// ======================================================================

#include "maya/MPxCommand.h"

class Messenger;

// ======================================================================

class ImportSkeletalMesh : public MPxCommand
{
public:

	static void    install(Messenger *newMessenger);
	static void    remove();
	static void   *creator();

public:

	MStatus        doIt(const MArgList &args);

private:

	ImportSkeletalMesh();
	ImportSkeletalMesh(const ImportSkeletalMesh &);
	ImportSkeletalMesh &operator=(const ImportSkeletalMesh &);
};

// ======================================================================

#endif
