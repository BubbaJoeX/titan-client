#ifndef SWGMAYAEDITOR_IMPORTSAT_H
#define SWGMAYAEDITOR_IMPORTSAT_H

#include <maya/MPxCommand.h>

class ImportSat : public MPxCommand
{
public:
    static void* creator();
    MStatus doIt(const MArgList& args) override;
};

#endif
