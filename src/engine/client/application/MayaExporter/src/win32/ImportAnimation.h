// ======================================================================
//
// ImportAnimation.h
// Copyright 2006 Sony Online Entertainment, Inc.
// All Rights Reserved.
//
// ======================================================================

#ifndef INCLUDED_ImportAnimation_H
#define INCLUDED_ImportAnimation_H

// ======================================================================

#include "maya/MPxCommand.h"

class Messenger;

// ======================================================================

class ImportAnimation : public MPxCommand
{
public:

	static void    install(Messenger *newMessenger);
	static void    remove();
	static void   *creator();

public:

	MStatus        doIt(const MArgList &args);

private:

	ImportAnimation();
	ImportAnimation(const ImportAnimation &);
	ImportAnimation &operator=(const ImportAnimation &);
};

// ======================================================================

#endif
