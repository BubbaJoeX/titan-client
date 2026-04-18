#ifndef SWGMAYAEDITOR_SWGAPPLYWAVEFRONTMTL_H
#define SWGMAYAEDITOR_SWGAPPLYWAVEFRONTMTL_H

#include <maya/MPxCommand.h>

/// Optional: parses a Wavefront .mtl and sets swgShaderPath / swgTexturePath on matching shading groups.
/// Maya’s OBJ importer usually wires .mtl into file textures already; exportStaticMesh discovers those and
/// publishes DDS + .sht without this command—use when paths or attrs need to be repaired manually.
class SwgApplyWavefrontMtl : public MPxCommand
{
public:
    static void* creator();
    MStatus doIt(const MArgList& args) override;
};

#endif
