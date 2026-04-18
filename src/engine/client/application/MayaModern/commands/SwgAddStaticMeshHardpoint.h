#ifndef SWGMAYAEDITOR_SWGADDSTATICMESHHARDPOINT_H
#define SWGMAYAEDITOR_SWGADDSTATICMESHHARDPOINT_H

#include <maya/MPxCommand.h>

/// Under the static mesh root (parent of the export mesh), creates hp_<name> with a 0.5m viewport cube
/// tagged swgExcludeFromStaticMeshExport so exportStaticMesh writes only transform (HPNT), not cube geometry.
class SwgAddStaticMeshHardpoint : public MPxCommand
{
public:
    static void* creator();
    MStatus doIt(const MArgList& args) override;
};

#endif
