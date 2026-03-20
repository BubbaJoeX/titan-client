#ifndef SWGMAYAEDITOR_IMPORTSKELETALMESH_H
#define SWGMAYAEDITOR_IMPORTSKELETALMESH_H

#include <maya/MPxCommand.h>

class ImportSkeletalMesh : public MPxCommand
{
public:
    static void* creator();
    MStatus doIt(const MArgList& args) override;
};

#endif
