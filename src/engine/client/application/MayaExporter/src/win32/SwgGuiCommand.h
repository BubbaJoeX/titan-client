// ======================================================================
//
// SwgGuiCommand.h
//
// ======================================================================

#ifndef INCLUDED_SwgGuiCommand_H
#define INCLUDED_SwgGuiCommand_H

// ======================================================================

class MArgList;
class MStatus;

#include "maya/MPxCommand.h"

// ======================================================================

class SwgGuiCommand : public MPxCommand
{
public:
	static void  *creator();
	MStatus       doIt(const MArgList &argList);

private:
	SwgGuiCommand();
	SwgGuiCommand(const SwgGuiCommand &);
	SwgGuiCommand &operator=(const SwgGuiCommand &);
};

// ======================================================================

#endif
