#include "StaticMeshExportPipeline.h"
#include "ExportStaticMesh.h"
#include "MayaUtility.h"

#include <maya/MGlobal.h>
#include <maya/MSelectionList.h>

MStatus StaticMeshExportPipeline::findExportMeshFromActiveSelection(
    MDagPath& outMeshShapePath, bool& outMeshWithoutShader)
{
    outMeshWithoutShader = false;

    MSelectionList sel;
    MStatus status = MGlobal::getActiveSelectionList(sel);
    if (!status || sel.length() == 0)
        return MS::kFailure;

    for (unsigned si = 0; si < sel.length(); ++si)
    {
        MDagPath dagPath;
        status = sel.getDagPath(si, dagPath);
        if (!status)
            continue;
        MDagPath meshPath;
        if (MayaUtility::findFirstMeshShapeWithShadersInHierarchy(dagPath, meshPath))
        {
            outMeshShapePath = meshPath;
            return MS::kSuccess;
        }
        if (MayaUtility::findFirstMeshShapeInHierarchy(dagPath, meshPath))
            outMeshWithoutShader = true;
    }
    return MS::kFailure;
}

bool StaticMeshExportPipeline::exportMesh(
    const MDagPath& meshShapePath,
    const std::string& outputPathOverride,
    const StaticMeshTransferOptions& options,
    std::string& outMeshTreePath,
    std::string& outAptTreePath)
{
    ExportStaticMesh cmd;
    return cmd.performExport(meshShapePath, outputPathOverride, outMeshTreePath, outAptTreePath, options);
}
