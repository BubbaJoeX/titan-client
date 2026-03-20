#ifndef SWGMAYAEDITOR_GETDATAROOTDIRCOMMAND_H
#define SWGMAYAEDITOR_GETDATAROOTDIRCOMMAND_H

#include <maya/MPxCommand.h>

class GetDataRootDirCommand : public MPxCommand
{
public:
    static void* creator();
    MStatus doIt(const MArgList& args) override;
};

#endif
