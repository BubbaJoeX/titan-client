#ifndef SWGMAYAEDITOR_MAYAUTILITY_H
#define SWGMAYAEDITOR_MAYAUTILITY_H

#include <maya/MDagPath.h>
#include <string>

class MayaUtility
{
public:
    static std::string parseFileNameToNodeName(const std::string& fileName);
    static bool createDirectory(const char* directory);

    static bool goToBindPose(int alternateBindPoseFrameNumber = -10);
    static bool enableDeformers();

    static bool ignoreNode(const MDagPath& dagPath);
    static bool hasNodeTypeInHierarchy(const MDagPath& hierarchyRoot, int nodeType);
};
#endif //SWGMAYAEDITOR_MAYAUTILITY_H
