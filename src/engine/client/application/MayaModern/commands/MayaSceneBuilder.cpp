#include "MayaSceneBuilder.h"

#include "Vector.h"

#include <maya/MFnAnimCurve.h>
#include <maya/MFnBlendShapeDeformer.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnIkJoint.h>
#include <maya/MDagPath.h>
#include <maya/MFn.h>
#include <maya/MFnMesh.h>
#include <maya/MFnSingleIndexedComponent.h>
#include <maya/MFnSkinCluster.h>
#include <maya/MFnTransform.h>
#include <maya/MGlobal.h>
#include <maya/MObject.h>
#include <maya/MSelectionList.h>
#include <maya/MQuaternion.h>
#include <maya/MEulerRotation.h>
#include <maya/MVector.h>
#include <maya/MFloatPointArray.h>
#include <maya/MFloatArray.h>
#include <maya/MIntArray.h>
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>
#include <maya/MDGModifier.h>
#include <maya/MNamespace.h>

#include <atomic>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace
{
    static std::string getJointShortName(const std::string& s)
    {
        size_t dbl = s.find("__");
        if (dbl != std::string::npos && dbl > 0)
            return s.substr(0, dbl);
        return s;
    }

    static std::string getComponentString0(const std::string& s)
    {
        size_t u = s.find('_');
        if (u != std::string::npos && u > 0)
            return s.substr(0, u);
        return s;
    }

    static std::string stripUnderscores(const std::string& s)
    {
        std::string out;
        out.reserve(s.size());
        for (char c : s)
            if (c != '_')
                out += c;
        return out;
    }

    static void shaderLog(const char* fmt, ...)
    {
        char buf[1024];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        std::string msg = std::string("[Shader] ") + buf + "\n";
        std::cerr << msg;
#ifdef _WIN32
        OutputDebugStringA(msg.c_str());
#endif
    }

    static MString map1UvSetName("map1");

    static void ensureMap1UvSetExists(MFnMesh& meshFn)
    {
        MStringArray uvSetNames;
        meshFn.getUVSetNames(uvSetNames);
        for (unsigned i = 0; i < uvSetNames.length(); ++i)
        {
            if (uvSetNames[i] == map1UvSetName)
                return;
        }

        MStatus uvSetStatus = meshFn.createUVSetDataMesh(map1UvSetName);
        if (!uvSetStatus)
            meshFn.createUVSet(map1UvSetName, nullptr, nullptr);
    }
}

MStatus MayaSceneBuilder::applyMap1Uvs(
    MFnMesh& meshFn,
    const MFloatArray& uArray,
    const MFloatArray& vArray,
    const MIntArray& uvCounts,
    const MIntArray& uvIds)
{
    if (uArray.length() == 0 || vArray.length() == 0 || uvCounts.length() == 0 || uvIds.length() == 0)
        return MS::kSuccess;

    ensureMap1UvSetExists(meshFn);

    if (meshFn.numUVs(map1UvSetName) > static_cast<int>(uArray.length()))
        meshFn.clearUVs(&map1UvSetName);

    MStatus status = meshFn.setUVs(uArray, vArray, &map1UvSetName);
    if (!status)
        return status;

    status = meshFn.assignUVs(uvCounts, uvIds, &map1UvSetName);
    if (!status)
        return status;

    meshFn.setCurrentUVSetName(map1UvSetName);
    meshFn.syncObject();
    return MS::kSuccess;
}

MStatus MayaSceneBuilder::createMesh(
    const std::vector<float>& positions,
    const std::vector<float>&,
    const std::vector<ShaderGroupData>& shaderGroups,
    const std::string& meshName,
    MDagPath& outMeshPath)
{
    MStatus status;
    const int vertexCount = static_cast<int>(positions.size()) / 3;

    MFloatPointArray vertexArray(vertexCount);
    for (int i = 0; i < vertexCount; ++i)
    {
        const size_t base = static_cast<size_t>(i) * 3;
        vertexArray.set(i, -positions[base + 0], positions[base + 1], positions[base + 2]);
    }

    int totalTriangles = 0;
    for (size_t sg = 0; sg < shaderGroups.size(); ++sg)
        totalTriangles += static_cast<int>(shaderGroups[sg].triangles.size());

    MIntArray polygonCounts(totalTriangles, 3);
    MIntArray polygonConnects;
    polygonConnects.setLength(static_cast<unsigned>(totalTriangles * 3));

    int triIdx = 0;
    for (size_t sg = 0; sg < shaderGroups.size(); ++sg)
    {
        const ShaderGroupData& group = shaderGroups[sg];
        for (size_t t = 0; t < group.triangles.size(); ++t)
        {
            int pi0 = group.triangles[t].indices[0];
            int pi1 = group.triangles[t].indices[1];
            int pi2 = group.triangles[t].indices[2];
            if (!group.positionIndices.empty())
            {
                if (pi0 >= 0 && pi0 < static_cast<int>(group.positionIndices.size()))
                    pi0 = group.positionIndices[static_cast<size_t>(pi0)];
                if (pi1 >= 0 && pi1 < static_cast<int>(group.positionIndices.size()))
                    pi1 = group.positionIndices[static_cast<size_t>(pi1)];
                if (pi2 >= 0 && pi2 < static_cast<int>(group.positionIndices.size()))
                    pi2 = group.positionIndices[static_cast<size_t>(pi2)];
            }
            polygonConnects[static_cast<unsigned>(triIdx * 3 + 0)] = pi0;
            polygonConnects[static_cast<unsigned>(triIdx * 3 + 1)] = pi1;
            polygonConnects[static_cast<unsigned>(triIdx * 3 + 2)] = pi2;
            ++triIdx;
        }
    }

    MFnMesh meshFn;
    MObject meshObj = meshFn.create(
        vertexCount, totalTriangles, vertexArray,
        polygonCounts, polygonConnects, MObject::kNullObj, &status);
    if (!status)
    {
        std::cerr << "MayaSceneBuilder: failed to create mesh " << meshName << std::endl;
        return status;
    }
    meshFn.setName(MString(meshName.c_str()), &status);

    // Enable double-sided display so geometry is visible from both sides (avoids black backfaces)
    MString doubleSidedCmd = "setAttr \"" + meshFn.fullPathName() + ".doubleSided\" 1";
    MGlobal::executeCommand(doubleSidedCmd);

    // UVs — explicit map1 (required for Arnold / Maya 2027 UV data meshes)
    MFloatArray uArray, vArray;
    MIntArray uvCounts, uvIds;
    int totalUVCount = 0;
    for (size_t sg = 0; sg < shaderGroups.size(); ++sg)
        totalUVCount += static_cast<int>(shaderGroups[sg].uvs.size());
    if (totalUVCount > 0)
    {
        uArray.setLength(static_cast<unsigned>(totalUVCount));
        vArray.setLength(static_cast<unsigned>(totalUVCount));
        unsigned uvOffset = 0;
        for (size_t sg = 0; sg < shaderGroups.size(); ++sg)
        {
            const ShaderGroupData& group = shaderGroups[sg];
            for (size_t u = 0; u < group.uvs.size(); ++u)
            {
                uArray[uvOffset + static_cast<unsigned>(u)] = group.uvs[u].u;
                vArray[uvOffset + static_cast<unsigned>(u)] = 1.0f - group.uvs[u].v;
            }
            uvOffset += static_cast<unsigned>(group.uvs.size());
        }

        unsigned uvBaseOffset = 0;
        for (size_t sg = 0; sg < shaderGroups.size(); ++sg)
        {
            const ShaderGroupData& group = shaderGroups[sg];
            if (!group.uvs.empty())
            {
                for (size_t t = 0; t < group.triangles.size(); ++t)
                {
                    uvCounts.append(3);
                    for (int v = 0; v < 3; ++v)
                    {
                        int uvIdx = group.triangles[t].indices[v];
                        if (uvIdx >= 0 && uvIdx < static_cast<int>(group.uvs.size()))
                            uvIds.append(static_cast<int>(uvBaseOffset) + uvIdx);
                        else
                            uvIds.append(0);
                    }
                }
            }
            else
            {
                for (size_t t = 0; t < group.triangles.size(); ++t)
                {
                    uvCounts.append(3);
                    uvIds.append(0);
                    uvIds.append(0);
                    uvIds.append(0);
                }
            }
            uvBaseOffset += static_cast<unsigned>(group.uvs.size());
        }

        applyMap1Uvs(meshFn, uArray, vArray, uvCounts, uvIds);
    }

    status = meshFn.getPath(outMeshPath);
    return status;
}

namespace SceneBuilderHelpers
{
    /// Short token per assignMaterials() call — unique per mesh import. (Full DAG paths made MEL
    /// `importShader -materialBase "..."` long enough to truncate / hang inside MGlobal::executeCommand.)
    std::string nextMaterialImportInstanceToken()
    {
        static std::atomic<unsigned> s_importInstance{0};
        return std::string("m") + std::to_string(++s_importInstance);
    }

    // Strip trailing "SG" from shader name - MayaExporter uses shader name only (no SG) for paths
    std::string stripSGSuffix(const std::string& name)
    {
        return MayaSceneBuilder::stripSGSuffixFromShaderName(name);
    }

    std::string resolveShaderPath(const std::string& shaderTemplateName, const std::string& inputFilePath)
    {
        std::string normalizedInput = inputFilePath;
        for (size_t i = 0; i < normalizedInput.size(); ++i)
            if (normalizedInput[i] == '\\') normalizedInput[i] = '/';
        std::string baseDir;
        const char* envDataRoot = getenv("TITAN_DATA_ROOT");
        if (envDataRoot && envDataRoot[0])
        {
            baseDir = envDataRoot;
            if (!baseDir.empty() && baseDir.back() != '/') baseDir += '/';
        }
        else
        {
            size_t cgPos = normalizedInput.find("compiled/game/");
            if (cgPos != std::string::npos)
            {
                baseDir = normalizedInput.substr(0, cgPos + 14);
                if (!baseDir.empty() && baseDir.back() != '/') baseDir += '/';
            }
        }
        if (baseDir.empty()) return std::string();
        std::string shaderName = stripSGSuffix(shaderTemplateName);
        if (shaderName.find("shader/") != 0 && shaderName.find("shader\\") != 0)
            shaderName = std::string("shader/") + shaderName;
        std::string resolved = baseDir + shaderName;
        for (size_t i = 0; i < resolved.size(); ++i)
            if (resolved[i] == '\\') resolved[i] = '/';
        if (resolved.size() < 4 || resolved.compare(resolved.size() - 4, 4, ".sht") != 0)
            resolved += ".sht";
        return resolved;
    }

    bool shaderFileExists(const std::string& path)
    {
        FILE* f = fopen(path.c_str(), "rb");
        if (f) { fclose(f); return true; }
        std::string withExt = path;
        if (withExt.size() < 4 || withExt.compare(withExt.size() - 4, 4, ".sht") != 0)
            withExt += ".sht";
        f = fopen(withExt.c_str(), "rb");
        if (f) { fclose(f); return true; }
        return false;
    }

    std::string sanitizeMayaName(const std::string& name)
    {
        std::string result;
        for (size_t i = 0; i < name.size(); ++i)
        {
            char c = name[i];
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')
                result += c;
            else
                result += '_';
        }
        if (!result.empty() && result[0] >= '0' && result[0] <= '9')
            result = "_" + result;
        return result;
    }
}

std::string MayaSceneBuilder::stripSGSuffixFromShaderName(const std::string& name)
{
    if (name.size() > 2 &&
        (name[name.size() - 2] == 'S' || name[name.size() - 2] == 's') &&
        (name[name.size() - 1] == 'G' || name[name.size() - 1] == 'g'))
        return name.substr(0, name.size() - 2);
    return name;
}

MStatus MayaSceneBuilder::createMaterial(
    const std::string& shaderName,
    const std::string& texturePath,
    MObject& outShadingGroup,
    const std::string& swgShaderPath,
    const std::string& swgTexturePath)
{
    MStatus status;
    MStringArray result;
    MString cmd = "shadingNode -asShader lambert -n \"";
    cmd += shaderName.c_str();
    cmd += "\"";
    status = MGlobal::executeCommand(cmd, result);
    if (!status || result.length() == 0) return MS::kFailure;

    MString shaderNodeName = result[0];
    if (texturePath.empty())
        shaderLog("createMaterial: %s with NO texture (gray default)", shaderName.c_str());
    cmd = "setAttr ";
    cmd += shaderNodeName;
    cmd += ".color 0.5 0.5 0.5";
    MGlobal::executeCommand(cmd);
    cmd = "setAttr ";
    cmd += shaderNodeName;
    cmd += ".ambientColor 0.2 0.2 0.2";
    MGlobal::executeCommand(cmd);
    cmd = "sets -renderable true -noSurfaceShader true -empty -name \"";
    cmd += shaderNodeName;
    cmd += "SG\"";
    status = MGlobal::executeCommand(cmd, result);
    if (!status || result.length() == 0) return MS::kFailure;

    MString sgName = result[0];
    cmd = "connectAttr -f ";
    cmd += shaderNodeName;
    cmd += ".outColor ";
    cmd += sgName;
    cmd += ".surfaceShader";
    MGlobal::executeCommand(cmd);
    MString partCmd = "listConnections -source true -destination false \"";
    partCmd += sgName;
    partCmd += ".partition\"";
    MStringArray partConns;
    if (!MGlobal::executeCommand(partCmd, partConns, false, false) || partConns.length() == 0)
    {
        cmd = "connectAttr -f ";
        cmd += sgName;
        cmd += ".partition \":renderPartition.sets\" -na";
        MGlobal::executeCommand(cmd, false, false);
    }

    if (!texturePath.empty())
    {
        std::string pathToUse = texturePath;
        for (size_t i = 0; i < pathToUse.size(); ++i)
            if (pathToUse[i] == '\\') pathToUse[i] = '/';

        cmd = "shadingNode -asTexture file -name \"";
        cmd += shaderName.c_str();
        cmd += "_tex\"";
        status = MGlobal::executeCommand(cmd, result);
        if (status && result.length() > 0)
        {
            MString texNodeName = result[0];
            cmd = "setAttr -type \"string\" ";
            cmd += texNodeName;
            cmd += ".fileTextureName \"";
            cmd += pathToUse.c_str();
            cmd += "\"";
            status = MGlobal::executeCommand(cmd);
            if (!status)
                std::cerr << "ImportShader: setAttr fileTextureName FAILED for " << pathToUse << "\n";
            cmd = "setAttr -type \"string\" ";
            cmd += texNodeName;
            cmd += ".colorSpace \"sRGB\"";
            MGlobal::executeCommand(cmd);
            cmd = "setAttr ";
            cmd += texNodeName;
            cmd += ".ignoreColorSpaceFileRules 1";
            MGlobal::executeCommand(cmd);
            {
                MSelectionList texSel;
                if (texSel.add(texNodeName) == MS::kSuccess)
                {
                    MObject texObj;
                    if (texSel.getDependNode(0, texObj) == MS::kSuccess)
                    {
                        MFnDependencyNode texFn(texObj);
                        MPlug uvPlug = texFn.findPlug("uvSetName", false);
                        if (!uvPlug.isNull())
                            uvPlug.setValue(MString("map1"));
                    }
                }
            }
            cmd = "connectAttr -f ";
            cmd += texNodeName;
            cmd += ".outColor ";
            cmd += shaderNodeName;
            cmd += ".color";
            MGlobal::executeCommand(cmd);
            // Opaque diffuse: do not connect transparency (avoids white/washed appearance from alpha)
        }
    }

    MSelectionList selList;
    selList.add(sgName);
    selList.getDependNode(0, outShadingGroup);

    if (!swgShaderPath.empty() || !swgTexturePath.empty())
    {
        MFnDependencyNode sgFn(outShadingGroup);
        auto setStrAttr = [&](const char* name, const std::string& val) {
            if (val.empty()) return;
            MPlug p = sgFn.findPlug(name, true);
            if (p.isNull())
            {
                MFnTypedAttribute tAttr;
                MObject a = tAttr.create(name, name, MFnData::kString);
                if (sgFn.addAttribute(a))
                {
                    p = sgFn.findPlug(name, true);
                    if (!p.isNull()) p.setValue(MString(val.c_str()));
                }
            }
            else
                p.setValue(MString(val.c_str()));
        };
        setStrAttr("swgShaderPath", swgShaderPath);
        setStrAttr("swgTexturePath", swgTexturePath);
    }

    return MS::kSuccess;
}

MStatus MayaSceneBuilder::assignPobCollisionPreviewMaterial(const MDagPath& meshPathIn)
{
    MDagPath meshShapePath = meshPathIn;
    if (meshShapePath.hasFn(MFn::kTransform))
        meshShapePath.extendToShape();
    if (!meshShapePath.hasFn(MFn::kMesh))
        return MS::kFailure;

    MString createIfMissing =
        "if (!`objExists \"swgPob_collisionPreviewGreen\"`) {"
        "shadingNode -asShader lambert -n \"swgPob_collisionPreviewGreen\";"
        "setAttr \"swgPob_collisionPreviewGreen.color\" -type double3 0.14 0.68 0.22;"
        "sets -renderable true -noSurfaceShader true -empty -name \"swgPob_collisionPreviewGreenSG\";"
        "connectAttr -f swgPob_collisionPreviewGreen.outColor swgPob_collisionPreviewGreenSG.surfaceShader;"
        "}";
    MGlobal::executeCommand(createIfMissing);

    MString assignCmd = "hyperShade -assign \"swgPob_collisionPreviewGreenSG\" \"";
    assignCmd += meshShapePath.fullPathName();
    assignCmd += "\"";
    MStatus assignStatus = MGlobal::executeCommand(assignCmd, false, false);
    if (!assignStatus)
    {
        MFnMesh meshFn(meshShapePath);
        const int polyCount = meshFn.numPolygons();
        if (polyCount > 0)
        {
            assignCmd = "sets -e -forceElement \"swgPob_collisionPreviewGreenSG\" \"";
            assignCmd += meshShapePath.fullPathName();
            assignCmd += ".f[0:";
            assignCmd += (polyCount - 1);
            assignCmd += "]\"";
            assignStatus = MGlobal::executeCommand(assignCmd, false, false);
        }
    }
    return assignStatus;
}

MStatus MayaSceneBuilder::assignMaterials(
    const MDagPath& meshPath,
    const std::vector<ShaderGroupData>& shaderGroups,
    const std::string& inputFilePath)
{
    if (shaderGroups.empty()) return MS::kSuccess;

    // Per-mesh only: same shader template on another mesh must import again (own texture / nodes). A global cache
    // reused SGs across imports and forced the second mesh green or wrong texture.
    std::map<std::string, std::string> shaderToActualSgName;
    const std::string meshTok = SceneBuilderHelpers::nextMaterialImportInstanceToken();

    int faceOffset = 0;
    MDagPath meshShapePath = meshPath;
    if (meshShapePath.hasFn(MFn::kTransform))
        meshShapePath.extendToShape();
    MFnDagNode meshDagFn(meshShapePath);

    for (size_t sgIdx = 0; sgIdx < shaderGroups.size(); ++sgIdx)
    {
        const ShaderGroupData& group = shaderGroups[sgIdx];
        int faceCount = static_cast<int>(group.triangles.size());
        if (faceCount == 0) continue;

        std::string rawName = group.shaderTemplateName;
        size_t lastSlash = rawName.find_last_of("/\\");
        if (lastSlash != std::string::npos) rawName = rawName.substr(lastSlash + 1);
        size_t dotPos = rawName.find_last_of('.');
        if (dotPos != std::string::npos) rawName = rawName.substr(0, dotPos);
        rawName = SceneBuilderHelpers::stripSGSuffix(rawName);
        std::string shaderName = SceneBuilderHelpers::sanitizeMayaName(rawName);
        if (shaderName.empty()) shaderName = "material";

        const std::string materialInstanceBase = shaderName + "__m__" + meshTok;

        std::string cacheKey = group.shaderTemplateName;
        MString sgNodeName = (materialInstanceBase + "SG").c_str();
        MString fullPath = meshDagFn.fullPathName();
        int colonPos = fullPath.index(':');
        if (colonPos >= 0)
        {
            MString currentNs = MNamespace::currentNamespace();
            if (currentNs.length() > 0)
            {
                std::string nsStr(currentNs.asChar());
                while (!nsStr.empty() && nsStr[0] == ':')
                    nsStr.erase(0, 1);
                if (!nsStr.empty())
                    sgNodeName = (MString(nsStr.c_str()) + ":" + sgNodeName);
            }
        }

        // Same mesh, multiple face ranges: reuse SG for identical shader template (cacheKey).
        auto it = shaderToActualSgName.find(cacheKey);
        if (it != shaderToActualSgName.end())
        {
            sgNodeName = it->second.c_str();
        }
        else
        {
            MString checkCmd = "objExists \"";
            checkCmd += sgNodeName;
            checkCmd += "\"";
            int exists = 0;
            bool materialExists = (MGlobal::executeCommand(checkCmd, exists) && exists);

            if (!materialExists)
            {
                std::string resolvedShaderFile = SceneBuilderHelpers::resolveShaderPath(group.shaderTemplateName, inputFilePath);
                bool useDefault = resolvedShaderFile.empty() || !SceneBuilderHelpers::shaderFileExists(resolvedShaderFile);
                shaderLog("assignMaterials: shader=%s resolved=%s exists=%s", group.shaderTemplateName.c_str(), resolvedShaderFile.c_str(), useDefault ? "no" : "yes");

                if (!useDefault)
                {
                    MString importCmd = "importShader -i \"";
                    importCmd += resolvedShaderFile.c_str();
                    importCmd += "\" -materialBase \"";
                    importCmd += materialInstanceBase.c_str();
                    importCmd += "\"";
                    MStringArray importResult;
                    if (MGlobal::executeCommand(importCmd, importResult, false, false) && importResult.length() > 0)
                    {
                        materialExists = true;
                        sgNodeName = importResult[0];
                        shaderToActualSgName[cacheKey] = sgNodeName.asChar();
                    }
                    else
                    {
                        shaderLog("assignMaterials: importShader FAILED for %s", resolvedShaderFile.c_str());
                        useDefault = true;
                    }
                }
                if (useDefault)
                {
                    shaderLog("assignMaterials: using default (green) material for %s", materialInstanceBase.c_str());
                    MObject dummySG;
                    createMaterial(materialInstanceBase, std::string(), dummySG);
                    MFnDependencyNode fn(dummySG);
                    sgNodeName = fn.name();
                    shaderToActualSgName[cacheKey] = sgNodeName.asChar();
                }
            }
            else
            {
                shaderToActualSgName[cacheKey] = sgNodeName.asChar();
            }
        }

        // Use shape path for mesh face components (e.g. |parent|meshShape.f[0:N])
        char rangeBuf[64];
        sprintf(rangeBuf, "%d:%d", faceOffset, faceOffset + faceCount - 1);

        MString compPath = meshShapePath.fullPathName();
        compPath += ".f[";
        compPath += rangeBuf;
        compPath += "]";

        // Pass component path directly to sets -e -forceElement (more reliable than select-then-sets)
        MString setCmd = "sets -e -forceElement \"";
        setCmd += sgNodeName;
        setCmd += "\" \"";
        setCmd += compPath;
        setCmd += "\"";
        MStatus setStatus = MGlobal::executeCommand(setCmd);
        if (setStatus)
            shaderLog("assignMaterials: assigned %s to %s faces %d-%d", sgNodeName.asChar(), meshShapePath.fullPathName().asChar(), faceOffset, faceOffset + faceCount - 1);
        else
        {
            shaderLog("assignMaterials: sets -e -forceElement FAILED sg=%s comp=%s", sgNodeName.asChar(), compPath.asChar());
            std::cerr << "MayaSceneBuilder: sets -e -forceElement failed sg=" << sgNodeName.asChar() << " comp=" << compPath.asChar() << "\n";
        }

        faceOffset += faceCount;
    }
    return MS::kSuccess;
}

MStatus MayaSceneBuilder::createSkinCluster(
    const MDagPath& meshPath,
    const std::vector<MDagPath>& jointPaths,
    const std::vector<std::string>& transformNames,
    const std::vector<int>& weightHeaders,
    const std::vector<SkinWeight>& weightData,
    int maxInfluencesPerVertex)
{
    if (jointPaths.empty()) return MS::kSuccess;

    if (maxInfluencesPerVertex < 1)
        maxInfluencesPerVertex = 8;

    std::map<std::string, size_t> jointNameToIndex;
    
    auto addToMap = [&jointNameToIndex](const std::string& key, size_t idx) {
        if (jointNameToIndex.find(key) == jointNameToIndex.end())
            jointNameToIndex[key] = idx;
        // Also add lowercase version
        std::string lower;
        for (char c : key) lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (lower != key && jointNameToIndex.find(lower) == jointNameToIndex.end())
            jointNameToIndex[lower] = idx;
    };
    
    for (size_t j = 0; j < jointPaths.size(); ++j)
    {
        MFnDagNode dagFn(jointPaths[j]);
        std::string name(dagFn.name().asChar());
        addToMap(name, j);
        
        size_t dbl = name.find("__");
        if (dbl != std::string::npos && dbl > 0)
        {
            std::string baseName = name.substr(0, dbl);
            addToMap(baseName, j);
        }
        // Maya namespaced joints (e.g. "lom:lThigh") must match MGN transform names ("lThigh")
        size_t colon = name.rfind(':');
        if (colon != std::string::npos && colon + 1 < name.size())
        {
            std::string shortName = name.substr(colon + 1);
            addToMap(shortName, j);
        }
        // MayaExporter MayaCompoundString::getComponentString(0) - first segment before '_'
        size_t u = name.rfind(':');
        std::string base = (u != std::string::npos && u + 1 < name.size()) ? name.substr(u + 1) : name;
        u = base.find('_');
        if (u != std::string::npos && u > 0)
        {
            std::string comp0 = base.substr(0, u);
            addToMap(comp0, j);
        }
    }

    auto tryFind = [&jointNameToIndex](const std::string& key) {
        auto it = jointNameToIndex.find(key);
        if (it != jointNameToIndex.end()) return it;
        std::string lower;
        for (char c : key) lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return jointNameToIndex.find(lower);
    };

    std::map<int, size_t> transformToJoint;
    for (size_t t = 0; t < transformNames.size(); ++t)
    {
        const std::string& tn = transformNames[t];
        auto it = tryFind(tn);
        if (it == jointNameToIndex.end())
            it = tryFind(stripUnderscores(tn));
        if (it == jointNameToIndex.end())
            it = tryFind(getJointShortName(tn));
        if (it == jointNameToIndex.end())
            it = tryFind(getComponentString0(tn));
        if (it != jointNameToIndex.end())
            transformToJoint[static_cast<int>(t)] = it->second;
    }

    char miBuf[32];
    sprintf(miBuf, "%d", maxInfluencesPerVertex);
    MString melCmd = "skinCluster -tsb -mi ";
    melCmd += miBuf;
    melCmd += " ";
    for (size_t j = 0; j < jointPaths.size(); ++j)
    {
        MFnDagNode dagFn(jointPaths[j]);
        melCmd += dagFn.fullPathName();
        melCmd += " ";
    }
    {
        MFnDagNode meshDagFn(meshPath);
        melCmd += meshDagFn.fullPathName();
    }

    MStringArray melResult;
    MStatus status = MGlobal::executeCommand(melCmd, melResult);
    if (!status || melResult.length() == 0) return MS::kFailure;

    MSelectionList selList;
    selList.add(melResult[0]);
    MObject skinClusterObj;
    selList.getDependNode(0, skinClusterObj);

    MFnSkinCluster skinFn(skinClusterObj, &status);
    if (!status) return status;

    MDagPath meshShapePath = meshPath;
    meshShapePath.extendToShape();

    int weightOffset = 0;
    for (size_t v = 0; v < weightHeaders.size(); ++v)
    {
        int numWeights = weightHeaders[v];
        MIntArray influenceIndices;
        MDoubleArray weights;
        double weightSum = 0.0;

        for (int w = 0; w < numWeights && (weightOffset + w) < static_cast<int>(weightData.size()); ++w)
        {
            const SkinWeight& sw = weightData[static_cast<size_t>(weightOffset + w)];
            auto it = transformToJoint.find(sw.transformIndex);
            if (it != transformToJoint.end())
            {
                influenceIndices.append(static_cast<int>(it->second));
                double wval = static_cast<double>(sw.weight);
                weights.append(wval);
                weightSum += wval;
            }
        }

        if (influenceIndices.length() > 0)
        {
            if (weightSum > 0.0 && std::fabs(weightSum - 1.0) > 1e-6)
            {
                for (unsigned int i = 0; i < weights.length(); ++i)
                    weights[i] /= weightSum;
            }
            MFnSingleIndexedComponent singleVertComp;
            MObject singleVert = singleVertComp.create(MFn::kMeshVertComponent);
            singleVertComp.addElement(static_cast<int>(v));
            skinFn.setWeights(meshShapePath, singleVert, influenceIndices, weights, true);
        }
        weightOffset += numWeights;
    }
    return MS::kSuccess;
}

namespace
{
    MEulerRotation::RotationOrder jroToMayaOrder(int jro)
    {
        switch (jro)
        {
            case 0: return MEulerRotation::kXYZ;
            case 1: return MEulerRotation::kXZY;
            case 2: return MEulerRotation::kYXZ;
            case 3: return MEulerRotation::kYZX;
            case 4: return MEulerRotation::kZXY;
            case 5: return MEulerRotation::kZYX;
            default: return MEulerRotation::kXYZ;
        }
    }

    MTransformationMatrix::RotationOrder jroToTransformOrder(int jro)
    {
        switch (jro)
        {
            case 0: return MTransformationMatrix::kXYZ;
            case 1: return MTransformationMatrix::kXZY;
            case 2: return MTransformationMatrix::kYXZ;
            case 3: return MTransformationMatrix::kYZX;
            case 4: return MTransformationMatrix::kZXY;
            case 5: return MTransformationMatrix::kZYX;
            default: return MTransformationMatrix::kXYZ;
        }
    }
}

void MayaSceneBuilder::engineVectorToMaya(const float engineVec[3], double& outX, double& outY, double& outZ)
{
    outX = static_cast<double>(-engineVec[0]);
    outY = static_cast<double>(engineVec[1]);
    outZ = static_cast<double>(engineVec[2]);
}

void MayaSceneBuilder::engineQuatToMayaEuler(const float eq[4], int rotationOrder, double& outRx, double& outRy, double& outRz)
{
    MQuaternion mq(eq[0], eq[1], eq[2], eq[3]);
    MEulerRotation euler = mq.asEulerRotation();
    euler.reorderIt(jroToMayaOrder(rotationOrder));
    outRx = euler.x;
    outRy = -euler.y;
    outRz = -euler.z;
}

MStatus MayaSceneBuilder::createHardpoints(
    const std::vector<HardpointData>& hardpoints,
    const std::map<std::string, MDagPath>& parentJointToPathMap,
    const std::string& meshName,
    MObject defaultParentForEmpty)
{
    MStatus status;
    for (size_t hp = 0; hp < hardpoints.size(); ++hp)
    {
        const HardpointData& hd = hardpoints[hp];

        MObject parentObj = defaultParentForEmpty;
        if (!hd.parentJoint.empty())
        {
            auto it = parentJointToPathMap.find(hd.parentJoint);
            if (it == parentJointToPathMap.end())
                continue;
            parentObj = it->second.node();
        }

        MFnTransform transformFn;
        MObject hpObj = transformFn.create(parentObj, &status);
        if (!status)
            continue;

        std::string hpNodeName = "hp_";
        hpNodeName += meshName;
        hpNodeName += "_";
        hpNodeName += hd.name;
        transformFn.setName(MString(hpNodeName.c_str()));

        double hx, hy, hz;
        engineVectorToMaya(hd.position, hx, hy, hz);
        transformFn.setTranslation(MVector(hx, hy, hz), MSpace::kTransform);

        double hrx, hry, hrz;
        engineQuatToMayaEuler(hd.rotation, 0, hrx, hry, hrz);
        MEulerRotation hpEuler(hrx, hry, hrz);
        transformFn.setRotation(hpEuler.asQuaternion(), MSpace::kTransform);

        const MString mel = MString("string $p[] = `polyCube -w 0.5 -h 0.5 -d 0.5 -ax 0 1 0`;\n") + "parent $p[0] \"" +
                            transformFn.fullPathName() +
                            "\";\n"
                            "string $s[] = `listRelatives -f -s $p[0]`;\n"
                            "if (`size $s` > 0) {\n"
                            "  catchQuiet(`addAttr -ln swgExcludeFromStaticMeshExport -at bool -dv 1 $s[0]`);\n"
                            "  catchQuiet(`setAttr ($s[0] + \".swgExcludeFromStaticMeshExport\") 1`);\n"
                            "}\n";
        MGlobal::executeCommand(mel, false, true);
    }
    return MS::kSuccess;
}

namespace
{
    static std::string skmgEscField(const std::string& s)
    {
        std::string o;
        o.reserve(s.size());
        for (char c : s)
        {
            if (c == '\t' || c == '\n' || c == '\r')
                c = ' ';
            o += c;
        }
        return o;
    }
}

MStatus MayaSceneBuilder::storeSkmgHardpointsOnMeshTransform(
    MObject meshTransformObj,
    const std::vector<HardpointData>& staticHardpoints,
    const std::vector<HardpointData>& dynamicHardpoints)
{
    std::ostringstream oss;
    oss << "swgSkmgHp v1\n";
    oss.setf(std::ios::fixed);
    oss.precision(9);
    const auto line = [&](char kind, const HardpointData& h) {
        oss << kind << '\t' << skmgEscField(h.name) << '\t' << skmgEscField(h.parentJoint) << '\t' << h.position[0] << ' '
            << h.position[1] << ' ' << h.position[2] << '\t' << h.rotation[0] << ' ' << h.rotation[1] << ' ' << h.rotation[2]
            << ' ' << h.rotation[3] << '\n';
    };
    for (const auto& h : staticHardpoints)
        line('s', h);
    for (const auto& h : dynamicHardpoints)
        line('d', h);

    MFnDependencyNode fn(meshTransformObj);
    MPlug plug = fn.findPlug("swgSkmgHardpoints", true);
    if (plug.isNull())
    {
        MFnTypedAttribute ta;
        MObject attrObj = ta.create("swgSkmgHardpoints", "ssh", MFnData::kString);
        ta.setStorable(true);
        if (fn.addAttribute(attrObj))
            plug = fn.findPlug("swgSkmgHardpoints", true);
    }
    if (!plug.isNull())
        plug.setValue(MString(oss.str().c_str()));
    return MS::kSuccess;
}

MStatus MayaSceneBuilder::createBlendShapes(
    const MDagPath& meshPath,
    const std::vector<BlendTargetData>& blendTargets)
{
    if (blendTargets.empty())
        return MS::kSuccess;

    MStatus status;
    MDagPath shapePath = meshPath;
    if (shapePath.hasFn(MFn::kTransform))
        shapePath.extendToShape();
    if (!shapePath.hasFn(MFn::kMesh))
    {
        std::cerr << "MayaSceneBuilder::createBlendShapes: mesh path has no mesh shape" << std::endl;
        return MS::kFailure;
    }

    MFnMesh baseMeshFn(shapePath, &status);
    if (!status)
        return status;

    const unsigned int vertexCount = baseMeshFn.numVertices(&status);
    if (!status || vertexCount == 0)
        return MS::kFailure;

    MFloatPointArray basePoints;
    status = baseMeshFn.getPoints(basePoints, MSpace::kObject);
    if (!status)
        return status;

    MIntArray polygonCounts;
    MIntArray polygonConnects;
    const unsigned int numPolygons = baseMeshFn.numPolygons(&status);
    if (status)
    {
        for (unsigned int i = 0; i < numPolygons; ++i)
        {
            MIntArray faceVertices;
            baseMeshFn.getPolygonVertices(static_cast<int>(i), faceVertices);
            polygonCounts.append(static_cast<int>(faceVertices.length()));
            for (unsigned int j = 0; j < faceVertices.length(); ++j)
                polygonConnects.append(faceVertices[j]);
        }
    }

    MObjectArray targetShapeObjects;

    for (size_t bt = 0; bt < blendTargets.size(); ++bt)
    {
        const BlendTargetData& btData = blendTargets[bt];
        if (btData.positionIndices.empty())
            continue;

        MFloatPointArray targetPoints(basePoints);

        for (size_t i = 0; i < btData.positionIndices.size(); ++i)
        {
            const int vi = btData.positionIndices[i];
            if (vi < 0 || static_cast<unsigned int>(vi) >= vertexCount)
                continue;
            const size_t di = static_cast<size_t>(i) * 3;
            if (di + 2 >= btData.positionDeltas.size())
                continue;

            const float dx = btData.positionDeltas[di + 0];
            const float dy = btData.positionDeltas[di + 1];
            const float dz = btData.positionDeltas[di + 2];

            MFloatPoint p = basePoints[static_cast<unsigned int>(vi)];
            float ex = -p.x;
            float ey = p.y;
            float ez = p.z;
            ex += dx;
            ey += dy;
            ez += dz;
            targetPoints.set(static_cast<unsigned int>(vi), -ex, ey, ez);
        }

        MFnMesh targetMeshFn;
        MObject targetMeshObj = targetMeshFn.create(
            static_cast<int>(vertexCount), static_cast<int>(numPolygons), targetPoints,
            polygonCounts, polygonConnects, MObject::kNullObj, &status);
        if (!status)
        {
            std::cerr << "MayaSceneBuilder::createBlendShapes: failed to create target mesh for " << btData.name << std::endl;
            continue;
        }

        std::string safeName = btData.name;
        for (size_t c = 0; c < safeName.size(); ++c)
        {
            char ch = safeName[c];
            if (ch == '/' || ch == '\\' || ch == ':')
                safeName[c] = '_';
        }
        if (!safeName.empty() && safeName[0] >= '0' && safeName[0] <= '9')
            safeName = "_" + safeName;
        targetMeshFn.setName(MString(safeName.c_str()), &status);

        targetShapeObjects.append(targetMeshObj);
    }

    if (targetShapeObjects.length() == 0)
        return MS::kSuccess;

    MFnBlendShapeDeformer blendFn;
    MObject blendObj = blendFn.create(shapePath.node(), MFnBlendShapeDeformer::kLocalOrigin, &status);
    if (!status)
    {
        std::cerr << "MayaSceneBuilder::createBlendShapes: failed to create blend shape deformer" << std::endl;
        for (unsigned int i = 0; i < targetShapeObjects.length(); ++i)
        {
            MFnDagNode dagFn(targetShapeObjects[i]);
            if (!dagFn.isFromReferencedFile())
            {
                MString deleteCmd = "delete \"";
                deleteCmd += dagFn.fullPathName();
                deleteCmd += "\"";
                MGlobal::executeCommand(deleteCmd);
            }
        }
        return status;
    }

    for (unsigned int i = 0; i < targetShapeObjects.length(); ++i)
    {
        status = blendFn.addTarget(shapePath.node(), static_cast<int>(i), targetShapeObjects[i], 1.0);
        if (!status)
        {
            std::cerr << "MayaSceneBuilder::createBlendShapes: addTarget failed for index " << i << std::endl;
        }
        MFnDagNode dagFn(targetShapeObjects[i]);
        if (!dagFn.isFromReferencedFile())
        {
            MString deleteCmd = "delete \"";
            deleteCmd += dagFn.fullPathName();
            deleteCmd += "\"";
            MGlobal::executeCommand(deleteCmd);
        }
    }

    return MS::kSuccess;
}

MStatus MayaSceneBuilder::createJointHierarchy(
    const std::vector<JointData>& joints,
    const std::string& rootName,
    std::vector<MDagPath>& outJointPaths,
    MObject parentForRoot)
{
    MStatus status;
    outJointPaths.resize(joints.size());

    MObject rootParentObj = parentForRoot;
    if (rootParentObj.isNull())
    {
        MFnTransform masterFn;
        rootParentObj = masterFn.create(MObject::kNullObj, &status);
        if (!status)
        {
            std::cerr << "MayaSceneBuilder: failed to create master group" << std::endl;
            return status;
        }
        masterFn.setName("master", &status);
    }

    for (size_t i = 0; i < joints.size(); ++i)
    {
        const JointData& jd = joints[i];

        MFnIkJoint jointFn;
        MObject parentObj = rootParentObj;
        if (jd.parentIndex >= 0 && jd.parentIndex < static_cast<int>(i))
            parentObj = outJointPaths[static_cast<size_t>(jd.parentIndex)].node();

        MObject jointObj = jointFn.create(parentObj, &status);
        if (!status)
        {
            std::cerr << "MayaSceneBuilder: failed to create joint " << jd.name << std::endl;
            return status;
        }

        std::string jointName = jd.name;
        if (jd.parentIndex < 0 && !rootName.empty())
        {
            jointName = jd.name;
            jointName += "__";
            jointName += rootName;
        }
        jointFn.setName(MString(jointName.c_str()), &status);

        jointFn.setRotationOrder(jroToTransformOrder(jd.rotationOrder), false);

        double tx, ty, tz;
        engineVectorToMaya(jd.bindTranslation, tx, ty, tz);
        jointFn.setTranslation(MVector(tx, ty, tz), MSpace::kTransform);

        double rx, ry, rz;
        engineQuatToMayaEuler(jd.bindRotation, jd.rotationOrder, rx, ry, rz);
        MEulerRotation bindEuler(rx, ry, rz, jroToMayaOrder(jd.rotationOrder));
        jointFn.setRotation(bindEuler);

        double preRx, preRy, preRz;
        engineQuatToMayaEuler(jd.preRotation, 0, preRx, preRy, preRz);
        MEulerRotation preEuler(preRx, preRy, preRz);
        jointFn.setRotateOrientation(preEuler.asQuaternion(), MSpace::kTransform, false);

        double postRx, postRy, postRz;
        engineQuatToMayaEuler(jd.postRotation, 0, postRx, postRy, postRz);
        MEulerRotation postEuler(postRx, postRy, postRz);
        jointFn.setOrientation(postEuler);

        MDagPath dagPath;
        status = jointFn.getPath(dagPath);
        if (!status)
        {
            std::cerr << "MayaSceneBuilder: failed to get dag path for joint " << jd.name << std::endl;
            return status;
        }
        outJointPaths[i] = dagPath;
    }

    MGlobal::executeCommand("jointDisplayScale 0.07");
    return MS::kSuccess;
}

namespace
{
    void disconnectExistingAnimation(MPlug& plug)
    {
        if (!plug.isConnected())
            return;
        MPlugArray sources;
        MStatus st;
        plug.connectedTo(sources, true, false, &st);
        if (!st || sources.length() == 0)
            return;

        std::vector<MObject> curvesToDelete;
        curvesToDelete.reserve(sources.length());
        MDGModifier modifier;
        for (unsigned i = 0; i < sources.length(); ++i)
        {
            MObject srcNode = sources[i].node();
            modifier.disconnect(sources[i], plug);
            if (srcNode.hasFn(MFn::kAnimCurve))
                curvesToDelete.push_back(srcNode);
        }
        modifier.doIt();

        for (MObject curveObj : curvesToDelete)
            MGlobal::deleteNode(curveObj);
    }
}

MStatus MayaSceneBuilder::setKeyframesFromDeltas(
    const MDagPath& jointPath,
    const std::string& attribute,
    const std::vector<AnimKeyframe>& deltaKeyframes,
    float fps,
    double bindPoseValue,
    bool negateDelta)
{
    MStatus status;
    MFnDependencyNode depFn(jointPath.node());
    MPlug plug = depFn.findPlug(MString(attribute.c_str()), &status);
    if (!status)
        return status;

    disconnectExistingAnimation(plug);

    MFnAnimCurve animCurveFn;
    animCurveFn.create(plug, MFnAnimCurve::kAnimCurveTL, nullptr, &status);
    if (!status)
        return status;

    const double fpsVal = (fps > 0.0f) ? static_cast<double>(fps) : 30.0;

    // Frame 0 bind-pose keyframe prevents jolt when play starts (Maya holds first key before playback)
    MTime time0(0.0, MTime::kSeconds);
    animCurveFn.addKeyframe(time0, bindPoseValue);

    for (size_t k = 0; k < deltaKeyframes.size(); ++k)
    {
        double deltaVal = static_cast<double>(deltaKeyframes[k].value);
        if (negateDelta)
            deltaVal = -deltaVal;
        double finalVal = bindPoseValue + deltaVal;
        const double timeVal = static_cast<double>(deltaKeyframes[k].frame) / fpsVal;
        MTime time(timeVal, MTime::kSeconds);
        animCurveFn.addKeyframe(time, finalVal);
    }
    // Clamped tangents prevent spline overshoot (avoids twitching)
    const unsigned int numKeys = animCurveFn.numKeys();
    for (unsigned int i = 0; i < numKeys; ++i)
    {
        animCurveFn.setInTangentType(i, MFnAnimCurve::kTangentClamped);
        animCurveFn.setOutTangentType(i, MFnAnimCurve::kTangentClamped);
    }
    return MS::kSuccess;
}

MStatus MayaSceneBuilder::setRotationKeyframesFromDeltas(
    const MDagPath& jointPath,
    const std::vector<QuatKeyframe>& deltaKeyframes,
    float fps)
{
    MStatus status;
    MFnIkJoint jointFn(jointPath.node());
    MQuaternion bindQuat;
    jointFn.getRotation(bindQuat, MSpace::kTransform);

    MFnDependencyNode depFn(jointPath.node());
    MPlug rxPlug = depFn.findPlug("rotateX", &status);
    MPlug ryPlug = depFn.findPlug("rotateY", &status);
    MPlug rzPlug = depFn.findPlug("rotateZ", &status);
    if (!status)
        return status;

    disconnectExistingAnimation(rxPlug);
    disconnectExistingAnimation(ryPlug);
    disconnectExistingAnimation(rzPlug);

    MFnAnimCurve rxCurve, ryCurve, rzCurve;
    rxCurve.create(rxPlug, MFnAnimCurve::kAnimCurveTA, nullptr, &status);
    ryCurve.create(ryPlug, MFnAnimCurve::kAnimCurveTA, nullptr, &status);
    rzCurve.create(rzPlug, MFnAnimCurve::kAnimCurveTA, nullptr, &status);
    if (!status)
        return status;

    MEulerRotation::RotationOrder rotOrder = static_cast<MEulerRotation::RotationOrder>(jointFn.rotationOrder());
    const double fpsVal = (fps > 0.0f) ? static_cast<double>(fps) : 30.0;

    // Frame 0 bind-pose keyframe (Maya bind pose, no conversion needed)
    MEulerRotation bindEuler = bindQuat.asEulerRotation();
    bindEuler.reorderIt(rotOrder);
    MTime time0(0.0, MTime::kSeconds);
    rxCurve.addKeyframe(time0, bindEuler.x);
    ryCurve.addKeyframe(time0, bindEuler.y);
    rzCurve.addKeyframe(time0, bindEuler.z);

    MQuaternion prevFinalQ = bindQuat;

    // Import rotation keyframes using the same approach as legacy MayaExporter:
    // final = delta * bindPose (quaternion multiplication)
    // Then convert to Euler with Y,Z negation for coordinate system
    for (size_t k = 0; k < deltaKeyframes.size(); ++k)
    {
        const QuatKeyframe& qk = deltaKeyframes[k];
        const double timeVal = static_cast<double>(qk.frame) / fpsVal;
        MTime time(timeVal, MTime::kSeconds);

        // Delta quaternion from .ans file: stored as [x, y, z, w] in our struct
        MQuaternion deltaQ(qk.rotation[0], qk.rotation[1], qk.rotation[2], qk.rotation[3]);

        // Apply delta to bind pose (this is how the game engine does it)
        MQuaternion finalQ = deltaQ * bindQuat;

        // Quaternion sign flip for interpolation continuity (prevents flipping)
        const double dot = prevFinalQ.w * finalQ.w + prevFinalQ.x * finalQ.x +
                           prevFinalQ.y * finalQ.y + prevFinalQ.z * finalQ.z;
        if (dot < 0.0)
            finalQ = MQuaternion(-finalQ.x, -finalQ.y, -finalQ.z, -finalQ.w);
        prevFinalQ = finalQ;

        // Convert to Euler and apply coordinate conversion (negate Y, Z)
        MEulerRotation finalEuler = finalQ.asEulerRotation();
        finalEuler.reorderIt(rotOrder);

        // Coordinate system conversion: Maya Y,Z are negated relative to game
        rxCurve.addKeyframe(time, finalEuler.x);
        ryCurve.addKeyframe(time, -finalEuler.y);
        rzCurve.addKeyframe(time, -finalEuler.z);
    }

    // Clamped tangents prevent spline overshoot between keyframes (avoids bending/twitching)
    const unsigned int numKeys = rxCurve.numKeys();
    for (unsigned int i = 0; i < numKeys; ++i)
    {
        rxCurve.setInTangentType(i, MFnAnimCurve::kTangentClamped);
        rxCurve.setOutTangentType(i, MFnAnimCurve::kTangentClamped);
        ryCurve.setInTangentType(i, MFnAnimCurve::kTangentClamped);
        ryCurve.setOutTangentType(i, MFnAnimCurve::kTangentClamped);
        rzCurve.setInTangentType(i, MFnAnimCurve::kTangentClamped);
        rzCurve.setOutTangentType(i, MFnAnimCurve::kTangentClamped);
    }
    return MS::kSuccess;
}
