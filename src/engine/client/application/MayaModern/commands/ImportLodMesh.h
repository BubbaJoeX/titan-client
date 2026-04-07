#ifndef SWGMAYAEDITOR_IMPORTLODMESH_H
#define SWGMAYAEDITOR_IMPORTLODMESH_H

#include <maya/MPxCommand.h>
#include <string>

namespace lod_path_helpers
{
/** Resolve SWG tree paths (SAT MSGN / SKTI, MLOD NAME, etc.) relative to an anchor file (e.g. the .sat path). */
std::string resolveTreeFilePath(const std::string& treeFilePath, const std::string& anchorPath);
/** Prefer .mgn / .msh / .shp when the path has no extension. */
std::string resolveMeshPath(const std::string& basePath);
}

class ImportLodMesh : public MPxCommand
{
public:
    static void* creator();
    MStatus doIt(const MArgList& args) override;
};

/** Resolve appearance path to .lod or .apt file. Tries .lod first, then .apt. */
std::string resolveLodOrAptPath(const std::string& baseResolvedPath);

/** Resolve path for static mesh: prefer .apt over .msh. APT is the entry point for static meshes. */
std::string resolveStaticMeshPath(const std::string& basePath);

/** Open APT, get redirect, return path to load. For static meshes only. Never loads .msh when .apt exists. */
std::string resolvePathViaApt(const std::string& filePath);

/**
 * If the file is a wrapper (FORM MLOD as used by .lmg, or FORM APT redirect) whose first child is a
 * skeletal mesh, returns the resolved path to that .mgn (SKMG). Otherwise returns the input path.
 * Call with a path already passed through resolveImportPath.
 */
std::string resolveSkmgPathThroughWrappers(const std::string& resolvedImportPath);

#endif
