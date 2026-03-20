#ifndef SWGMAYAEDITOR_IMPORTSTATICMESH_H
#define SWGMAYAEDITOR_IMPORTSTATICMESH_H

#include <maya/MPxCommand.h>

class ImportStaticMesh : public MPxCommand
{
public:
    static void* creator();
    MStatus doIt(const MArgList& args) override;
};

#endif
