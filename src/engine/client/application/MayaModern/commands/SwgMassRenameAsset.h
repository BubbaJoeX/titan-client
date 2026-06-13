#ifndef SWGMAYAEDITOR_SWGMASSRENAMEASSET_H
#define SWGMAYAEDITOR_SWGMASSRENAMEASSET_H

#include <maya/MPxCommand.h>

/// Mass-rename an imported SWG static mesh asset after duplicating or retargeting art.
/// Replaces a token in DAG node names, shading networks, swgShaderPath / swgTexturePath,
/// file texture paths, and optional drop-in images under textureWriteDir.
/// Usage: select mesh transform(s), then swgMassRenameAsset -from edb_taanabtonic -to bubbajuice;
class SwgMassRenameAsset : public MPxCommand
{
public:
    static void* creator();
    MStatus doIt(const MArgList& args) override;
};

#endif
