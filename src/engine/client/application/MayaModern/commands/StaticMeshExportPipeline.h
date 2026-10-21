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
    /// `outRootPath` (optional) receives the selected node that contains it: the export root whose mesh shapes
    /// are combined and whose non-geometry child transforms become hardpoints.
    static MStatus findExportMeshFromActiveSelection(MDagPath& outMeshShapePath, bool& outMeshWithoutShader,
                                                     MDagPath* outRootPath = nullptr);

    /// File > Export SwgMsh and `exportStaticMesh` both call this.
    static bool exportMesh(
        const MDagPath& meshShapePath,
        const std::string& outputPathOverride,
        const StaticMeshTransferOptions& options,
        std::string& outMeshTreePath,
        std::string& outAptTreePath,
        const MDagPath* exportRoot = nullptr);
};

#endif
