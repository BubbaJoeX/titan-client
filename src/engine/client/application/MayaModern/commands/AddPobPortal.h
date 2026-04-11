#ifndef SWGMAYAEDITOR_ADDPOBPORTAL_H
#define SWGMAYAEDITOR_ADDPOBPORTAL_H

#include <maya/MPxCommand.h>

class AddPobPortal : public MPxCommand
{
public:
    static void* creator();
    MStatus doIt(const MArgList& args) override;
};

#endif
