#ifndef SWGMAYAEDITOR_MAYAUTILITY_H
#define SWGMAYAEDITOR_MAYAUTILITY_H

#include <maya/MDagPath.h>
#include <maya/MFileObject.h>
#include <string>

class MayaUtility
{
public:
	// resolvedName() is often empty while the import dialog lists files; fall back so identifyFile() can match extensions.
	static std::string fileObjectPathForIdentify(const MFileObject& fileObject);

    static std::string parseFileNameToNodeName(const std::string& fileName);
    static bool createDirectory(const char* directory);

    static bool goToBindPose(int alternateBindPoseFrameNumber = -10);
    static bool enableDeformers();

    static bool ignoreNode(const MDagPath& dagPath);
    static bool hasNodeTypeInHierarchy(const MDagPath& hierarchyRoot, int nodeType);

    /// Finds the first mesh shape under root (including root if it is already a mesh). Handles nested
    /// transform groups; MDagPath::extendToShape() only reaches a direct mesh child, which fails for
    /// common rigs (group → geo → mesh).
    static bool findFirstMeshShapeInHierarchy(const MDagPath& root, MDagPath& outMeshPath);

    /// Like findFirstMeshShapeInHierarchy, but skips mesh shapes with no shading group (e.g. leftover
    /// geometry after combine). Depth-first order among meshes that have at least one material.
    static bool findFirstMeshShapeWithShadersInHierarchy(const MDagPath& root, MDagPath& outMeshPath);
};
#endif //SWGMAYAEDITOR_MAYAUTILITY_H
