#ifndef SWGMAYAEDITOR_IMPORTANIMATION_H
#define SWGMAYAEDITOR_IMPORTANIMATION_H

#include <maya/MPxCommand.h>

class ImportAnimation : public MPxCommand
{
public:
    static void* creator();
    MStatus doIt(const MArgList& args) override;
};

#endif
