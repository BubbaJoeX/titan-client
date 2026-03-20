#ifndef SWGMAYAEDITOR_IMPORTPOB_H
#define SWGMAYAEDITOR_IMPORTPOB_H

#include <maya/MPxCommand.h>

class ImportPob : public MPxCommand
{
public:
    static void* creator();
    MStatus doIt(const MArgList& args) override;
};

#endif
