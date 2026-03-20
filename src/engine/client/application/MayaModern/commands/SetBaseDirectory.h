#ifndef SWGMAYAEDITOR_SETBASEDIRECTORY_H
#define SWGMAYAEDITOR_SETBASEDIRECTORY_H

#include <maya/MPxCommand.h>

class SetBaseDirectory : public MPxCommand
{
public:
    static void* creator();
    MStatus doIt(const MArgList& args) override;
};

#endif
