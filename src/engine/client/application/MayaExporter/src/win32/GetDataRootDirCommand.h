// ======================================================================
//
// GetDataRootDirCommand.h
//
// ======================================================================

#ifndef GET_DATA_ROOT_DIR_COMMAND_H
#define GET_DATA_ROOT_DIR_COMMAND_H

// ======================================================================

#include "maya/MPxCommand.h"

// ======================================================================

class GetDataRootDirCommand : public MPxCommand
{
public:

	static void *creator();

	MStatus doIt(const MArgList &argList);

};

// ======================================================================

#endif
