// ======================================================================
//
// ImportShader.h
// Copyright 2006 Sony Online Entertainment, Inc.
// All Rights Reserved.
//
// ======================================================================

#ifndef INCLUDED_ImportShader_H
#define INCLUDED_ImportShader_H

// ======================================================================

#include "maya/MPxCommand.h"

class Messenger;

// ======================================================================

class ImportShader : public MPxCommand
{
public:

	static void    install(Messenger *newMessenger);
	static void    remove();
	static void   *creator();

public:

	MStatus        doIt(const MArgList &args);

private:

	ImportShader();
	ImportShader(const ImportShader &);
	ImportShader &operator=(const ImportShader &);
};

// ======================================================================

#endif
