// ======================================================================
//
// SwgPrepCommand.h
// Prepares Maya hierarchy for SWG asset export (POB, MSH, MGN).
//
// ======================================================================

#ifndef INCLUDED_SwgPrepCommand_H
#define INCLUDED_SwgPrepCommand_H

// ======================================================================

#include "maya/MPxCommand.h"

class MArgList;
class Messenger;

// ======================================================================

class SwgPrepCommand : public MPxCommand
{
public:

	static void  install(Messenger *newMessenger);
	static void  remove();
	static void *creator();

	MStatus      doIt(const MArgList &args);

private:

	SwgPrepCommand();
	SwgPrepCommand(const SwgPrepCommand &);
	SwgPrepCommand &operator=(const SwgPrepCommand &);

	MStatus prepPob();
	MStatus prepMsh();
	MStatus prepMgn();
};

// ======================================================================

#endif
