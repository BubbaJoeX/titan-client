#ifndef SWGMAYAEDITOR_EXPORTSAT_H
#define SWGMAYAEDITOR_EXPORTSAT_H

#include <maya/MPxCommand.h>

class ExportSat : public MPxCommand
{
public:
	static void* creator();
	MStatus doIt(const MArgList& args) override;
};

#endif
