#ifndef SWGMAYAEDITOR_EXPORTSTATICMESH_H
#define SWGMAYAEDITOR_EXPORTSTATICMESH_H

#include <maya/MPxCommand.h>

class ExportStaticMesh : public MPxCommand
{
public:
    static void* creator();
    MStatus doIt(const MArgList& args) override;

    /// Performs mesh export. Used by MshTranslator and MEL exportStaticMesh.
    /// legacyTriangleFlipFromCmd: exportStaticMesh -legacyTriangleFlip; legacyTriangleFlipFromFileDialog: SwgMsh Export Selection options.
    bool performExport(const class MDagPath& meshDagPath, const std::string& outputPath,
        std::string& outMeshPath, std::string& outAptPath, bool legacyTriangleFlipFromCmd = false,
        bool legacyTriangleFlipFromFileDialog = false);
};

#endif
