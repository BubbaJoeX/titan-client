#ifndef SWGMAYAEDITOR_EXPORTPOB_H
#define SWGMAYAEDITOR_EXPORTPOB_H

#include <maya/MPxCommand.h>

class ExportPob : public MPxCommand
{
public:
    static void* creator();
    MStatus doIt(const MArgList& args) override;
};

#endif
