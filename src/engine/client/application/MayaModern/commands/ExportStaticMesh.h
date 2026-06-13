#ifndef SWGMAYAEDITOR_EXPORTSTATICMESH_H
#define SWGMAYAEDITOR_EXPORTSTATICMESH_H

#include "StaticMeshTransferOptions.h"

#include <maya/MPxCommand.h>
#include <maya/MString.h>

#include <string>

class ExportStaticMesh : public MPxCommand
{
public:
    static void* creator();
    MStatus doIt(const MArgList& args) override;

    /// Core static mesh export. Prefer StaticMeshExportPipeline::exportMesh from translators and MEL.
    bool performExport(
        const class MDagPath& meshDagPath,
        const std::string& outputPath,
        std::string& outMeshPath,
        std::string& outAptPath,
        const StaticMeshTransferOptions& options);
};

#endif
