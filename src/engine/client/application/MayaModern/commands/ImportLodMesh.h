#ifndef SWGMAYAEDITOR_IMPORTLODMESH_H
#define SWGMAYAEDITOR_IMPORTLODMESH_H

#include <maya/MPxCommand.h>

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

#endif
