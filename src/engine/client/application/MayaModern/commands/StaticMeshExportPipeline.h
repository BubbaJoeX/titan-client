#ifndef SWGMAYAEDITOR_STATICMESHEXPORTPIPELINE_H
#define SWGMAYAEDITOR_STATICMESHEXPORTPIPELINE_H

#include "StaticMeshTransferOptions.h"

#include <maya/MDagPath.h>
#include <maya/MStatus.h>
#include <maya/MString.h>

#include <string>

/// Unified static mesh export entry: selection → performExport with shared options and directory bootstrap.
class StaticMeshExportPipeline
{
public:
    /// Resolve first mesh shape with a material under the active selection.
    static MStatus findExportMeshFromActiveSelection(MDagPath& outMeshShapePath, bool& outMeshWithoutShader);

    /// File > Export SwgMsh and `exportStaticMesh` both call this.
    static bool exportMesh(
        const MDagPath& meshShapePath,
        const std::string& outputPathOverride,
        const StaticMeshTransferOptions& options,
        std::string& outMeshTreePath,
        std::string& outAptTreePath);
};

#endif
