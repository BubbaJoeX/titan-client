#ifndef SWGMAYAEDITOR_IMPORTSKELETON_H
#define SWGMAYAEDITOR_IMPORTSKELETON_H

#include <maya/MPxCommand.h>

class ImportSkeleton : public MPxCommand
{
public:
    static void* creator();
    MStatus doIt(const MArgList& args) override;
};

#endif
