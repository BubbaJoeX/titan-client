#ifndef SWGMAYAEDITOR_SWGREFORMATMESH_H
#define SWGMAYAEDITOR_SWGREFORMATMESH_H

#include <maya/MPxCommand.h>

/// Deletes all polygon meshes not in the selection, then parents remaining mesh roots under a new
/// transform (default swgStaticMesh) for a clean static-mesh style hierarchy. Preserves materials
/// on kept meshes; removes unused shading nodes after.
class SwgReformatMesh : public MPxCommand
{
public:
    static void* creator();
    MStatus doIt(const MArgList& args) override;
};

#endif
