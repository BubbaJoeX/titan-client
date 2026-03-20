#include "MayaUtility.h"
#include "MayaCompoundString.h"

#include <maya/MAnimControl.h>
#include <maya/MDagPath.h>
#include <maya/MFn.h>
#include <maya/MFnDagNode.h>
#include <maya/MObject.h>
#include <maya/MGlobal.h>
#include <maya/MStatus.h>
#include <maya/MTime.h>

#include <cstring>
#include <stack>
#include <string>

#ifdef _WIN32
#include <windows.h>
#include <string.h>
#endif

namespace
{
    const char* const s_commandDisableAllNodes = "doEnableNodeItems false all;";
    const char* const s_commandGoToBindPose = "dagPose -r -g -bp;";
    const char* const s_commandEnableAllNodes = "doEnableNodeItems true all;";
    const char* const s_commandDgDirty = "dgdirty -a;";
}

std::string MayaUtility::parseFileNameToNodeName(const std::string& fileName)
{
    const auto lastSlash = fileName.find_last_of("/\\");
    const auto lastDot = fileName.find_last_of('.');
    if (lastSlash != std::string::npos)
    {
        if (lastDot != std::string::npos && lastDot > lastSlash)
            return fileName.substr(lastSlash + 1, lastDot - lastSlash - 1);
        return fileName.substr(lastSlash + 1);
    }
    if (lastDot != std::string::npos)
        return fileName.substr(0, lastDot);
    return fileName;
}

bool MayaUtility::goToBindPose(int alternateBindPoseFrameNumber)
{
    MStatus status;
    MTime mayaFrameTime;
    status = mayaFrameTime.setValue(static_cast<double>(alternateBindPoseFrameNumber));
    if (!status)
        return false;

    status = MAnimControl::setCurrentTime(mayaFrameTime);
    if (!status)
        return false;

    status = MGlobal::executeCommand(MString(s_commandDisableAllNodes), true, true);
    if (status)
    {
        status = MGlobal::executeCommand(MString(s_commandGoToBindPose), true, true);
        if (!status)
            enableDeformers();
    }
    return true;
}

bool MayaUtility::enableDeformers()
{
    MStatus status = MGlobal::executeCommand(MString(s_commandEnableAllNodes), true, true);
    if (!status)
        return false;
    status = MGlobal::executeCommand(MString(s_commandDgDirty), true, true);
    return status;
}

bool MayaUtility::ignoreNode(const MDagPath& dagPath)
{
    MStatus status;
    MObject object = dagPath.node(&status);
    if (!status)
        return true;

    MFnDagNode fnDagNode(object, &status);
    if (!status)
        return true;

    MayaCompoundString compoundName(fnDagNode.name(&status));
    if (!status)
        return true;

    const int componentCount = compoundName.getComponentCount();
    for (int i = 1; i < componentCount; ++i)
    {
        MString component = compoundName.getComponentString(i);
        if (_stricmp(component.asChar(), "ignore") == 0)
            return true;
    }
    return false;
}

static bool hasNodeTypeInHierarchyHelper(const MDagPath& path, int nodeType, bool* found)
{
    if (*found)
        return true;
    MStatus status;
    if (path.apiType(&status) == nodeType)
    {
        *found = true;
        return true;
    }
    const unsigned childCount = path.childCount(&status);
    if (!status)
        return false;
    for (unsigned i = 0; i < childCount && !*found; ++i)
    {
        MObject child = path.child(i, &status);
        if (!status)
            continue;
        MFnDagNode fnChild(child, &status);
        if (!status)
            continue;
        MDagPath childPath;
        if (!fnChild.getPath(childPath))
            continue;
        if (!hasNodeTypeInHierarchyHelper(childPath, nodeType, found))
            return false;
    }
    return true;
}

bool MayaUtility::hasNodeTypeInHierarchy(const MDagPath& hierarchyRoot, int nodeType)
{
    bool found = false;
    return hasNodeTypeInHierarchyHelper(hierarchyRoot, nodeType, &found) && found;
}

bool MayaUtility::createDirectory(const char* directory)
{
#ifdef _WIN32
    std::stack<std::string> directoryStack;
    std::string currentDirectory = directory;

    while (!currentDirectory.empty())
    {
        if (currentDirectory.back() == '\\')
            currentDirectory.pop_back();
        if (currentDirectory.back() == ':')
            break;
        if (!currentDirectory.empty())
            directoryStack.push(currentDirectory);
        const auto pos = currentDirectory.find_last_of('\\');
        if (pos == std::string::npos)
            break;
        currentDirectory.resize(pos);
    }

    while (!directoryStack.empty())
    {
        currentDirectory = directoryStack.top();
        directoryStack.pop();
        CreateDirectoryA(currentDirectory.c_str(), nullptr);
    }
#endif
    return true;
}
