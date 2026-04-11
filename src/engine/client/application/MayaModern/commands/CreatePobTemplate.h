#ifndef SWGMAYAEDITOR_CREATEPOBTEMPLATE_H
#define SWGMAYAEDITOR_CREATEPOBTEMPLATE_H

#include <maya/MPxCommand.h>

class CreatePobTemplate : public MPxCommand
{
public:
    static void* creator();
    MStatus doIt(const MArgList& args) override;
};

#endif
