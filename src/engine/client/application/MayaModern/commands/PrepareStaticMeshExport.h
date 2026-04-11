#ifndef SWGMAYAEDITOR_PREPARESTATICMESHEXPORT_H
#define SWGMAYAEDITOR_PREPARESTATICMESHEXPORT_H

#include <maya/MPxCommand.h>

/**
 * Prepares polygon meshes for exportStaticMesh: optional polyUnite with UV merge, UV-set repair for map1,
 * and validation of swgShaderPath / per-face UVs (matches ExportStaticMesh behavior).
 */
class PrepareStaticMeshExport : public MPxCommand
{
public:
    static void* creator();
    MStatus doIt(const MArgList& args) override;
};

#endif
