#include "MayaSceneBuilder.h"
#include "ImportPathResolver.h"
#include "ImportShader.h"
#include "ImportSat.h"
#include "MayaUtility.h"
#include "SwgAnimMath.h"

#include "Vector.h"

#include <maya/MFnAnimCurve.h>
#include <maya/MFnBlendShapeDeformer.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnNumericData.h>
#include <maya/MFnIkJoint.h>
#include <maya/MDagPath.h>
#include <maya/MFn.h>
#include <maya/MFnMesh.h>
#include <maya/MFnSingleIndexedComponent.h>
#include <maya/MFnLambertShader.h>
#include <maya/MFnSet.h>
#include <maya/MColor.h>
#include <maya/MFnSkinCluster.h>
#include <maya/MFnTransform.h>
#include <maya/MFnMatrixData.h>
#include <maya/MMatrix.h>
#include <maya/MGlobal.h>
#include <maya/MObject.h>
#include <maya/MSelectionList.h>
#include <maya/MQuaternion.h>
#include <maya/MEulerRotation.h>
#include <maya/MVector.h>
#include <maya/MFloatPointArray.h>
#include <maya/MFloatArray.h>
#include <maya/MIntArray.h>
#include <maya/MStringArray.h>
#include <maya/MDoubleArray.h>
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>
#include <maya/MDGModifier.h>
#include <maya/MNamespace.h>
#include <maya/MEventMessage.h>
#include <maya/MItDependencyNodes.h>
#include <maya/MMessage.h>

#include <atomic>
#include <cstdlib>
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

    /// Maya 2027 OCIO rejects colorSpace "sRGB" (needs "sRGB - Texture" / "Raw"). An invalid
    /// space leaves the file node in error and VP2 shows defaultColor (medium gray).
    /// Do not pin uvSetName — an empty/missing map1 also falls back to defaultColor.
    static void bindMayaFileTexturePath(MFnDependencyNode& fileFn, const MString& path)
    {
        if (path.length() == 0)
            return;
        MPlug dest = fileFn.findPlug("fileTextureName", true);
        if (dest.isNull())
            return;
        dest.setString(path);

        MPlug ignoreCs = fileFn.findPlug("ignoreColorSpaceFileRules", true);
        if (!ignoreCs.isNull())
            ignoreCs.setBool(true);
        MPlug colorSpace = fileFn.findPlug("colorSpace", true);
        if (!colorSpace.isNull())
            colorSpace.setString("sRGB - Texture");

        MPlug uvPlug = fileFn.findPlug("uvSetName", true);
        if (!uvPlug.isNull())
            uvPlug.setString("");

        MPlug disable = fileFn.findPlug("disableFileLoad", true);
        if (!disable.isNull())
        {
            disable.setBool(true);
            disable.setBool(false);
        }
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

    const int nPoly = meshFn.numPolygons();
    if (nPoly <= 0 || uvCounts.length() != static_cast<unsigned>(nPoly))
    {
        MGlobal::displayInfo(MString("MayaSceneBuilder: UV skip (poly=")
            + nPoly + " uvCounts=" + static_cast<int>(uvCounts.length()) + ")");
        return MS::kSuccess;
    }
    unsigned idNeed = 0;
    for (unsigned i = 0; i < uvCounts.length(); ++i)
        idNeed += static_cast<unsigned>(uvCounts[i]);
    if (idNeed != uvIds.length())
    {
        MGlobal::displayInfo(MString("MayaSceneBuilder: UV skip (idNeed=")
            + static_cast<int>(idNeed) + " uvIds=" + static_cast<int>(uvIds.length()) + ")");
        return MS::kSuccess;
    }

    // DAG meshes from MFnMesh::create() already have a default UV set (usually map1).
    // createUVSetDataMesh is for MFnMeshData only — calling it on a scene mesh leaves map1
    // empty and makes named setUVs/assignUVs fail (swallowed, so chairs stay gray).
    MStringArray uvSetNames;
    meshFn.getUVSetNames(uvSetNames);
    MString setName = map1UvSetName;
    bool haveSet = false;
    for (unsigned i = 0; i < uvSetNames.length(); ++i)
    {
        if (uvSetNames[i] == map1UvSetName)
        {
            haveSet = true;
            break;
        }
    }
    if (!haveSet && uvSetNames.length() > 0)
    {
        setName = uvSetNames[0];
        haveSet = true;
    }
    if (!haveSet)
    {
        MStatus createSet = meshFn.createUVSet(setName);
        if (!createSet)
            MGlobal::displayWarning(MString("MayaSceneBuilder: createUVSet failed: ") + createSet.errorString());
    }

    MIntArray safeIds(uvIds);
    const int nuv = static_cast<int>(uArray.length());
    for (unsigned i = 0; i < safeIds.length(); ++i)
    {
        if (safeIds[i] < 0 || safeIds[i] >= nuv)
            safeIds[i] = 0;
    }

    meshFn.clearUVs(&setName);

    auto tryWrite = [&](const MString* name) -> MStatus {
        MStatus st = meshFn.setUVs(uArray, vArray, name);
        if (!st)
            return st;
        return meshFn.assignUVs(uvCounts, safeIds, name);
    };

    MStatus status = tryWrite(&setName);
    if (!status)
        status = tryWrite(nullptr);
    if (!status)
    {
        MGlobal::displayWarning(MString("MayaSceneBuilder: setUVs/assignUVs failed: ")
            + status.errorString()
            + " uvs=" + nuv
            + " faces=" + nPoly);
        return status;
    }

    meshFn.setCurrentUVSetName(setName);
    meshFn.updateSurface();
    meshFn.syncObject();
    MGlobal::displayInfo(MString("MayaSceneBuilder: UVs applied set=") + setName
        + " count=" + meshFn.numUVs(setName)
        + " faces=" + nPoly);
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
                else
                    continue;
                if (pi1 >= 0 && pi1 < static_cast<int>(group.positionIndices.size()))
                    pi1 = group.positionIndices[static_cast<size_t>(pi1)];
                else
                    continue;
                if (pi2 >= 0 && pi2 < static_cast<int>(group.positionIndices.size()))
                    pi2 = group.positionIndices[static_cast<size_t>(pi2)];
                else
                    continue;
            }
            if (pi0 < 0 || pi0 >= vertexCount || pi1 < 0 || pi1 >= vertexCount || pi2 < 0 || pi2 >= vertexCount)
                continue;
            polygonConnects[static_cast<unsigned>(triIdx * 3 + 0)] = pi0;
            polygonConnects[static_cast<unsigned>(triIdx * 3 + 1)] = pi1;
            polygonConnects[static_cast<unsigned>(triIdx * 3 + 2)] = pi2;
            ++triIdx;
        }
    }

    if (triIdx <= 0)
    {
        std::cerr << "MayaSceneBuilder: no valid triangles for mesh " << meshName << std::endl;
        return MS::kFailure;
    }
    polygonCounts.setLength(static_cast<unsigned>(triIdx));
    polygonConnects.setLength(static_cast<unsigned>(triIdx * 3));

    MFnMesh meshFn;
    MObject meshObj = meshFn.create(
        vertexCount, triIdx, vertexArray,
        polygonCounts, polygonConnects, MObject::kNullObj, &status);
    if (!status)
    {
        std::cerr << "MayaSceneBuilder: failed to create mesh " << meshName << std::endl;
        return status;
    }
    meshFn.setName(MString(meshName.c_str()), &status);

    MPlug doubleSidedPlug = meshFn.findPlug("doubleSided", true);
    if (!doubleSidedPlug.isNull())
        doubleSidedPlug.setBool(true);

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
            auto remapPos = [&](int localIdx) -> int {
                int pi = localIdx;
                if (!group.positionIndices.empty())
                {
                    if (pi < 0 || pi >= static_cast<int>(group.positionIndices.size()))
                        return -1;
                    pi = group.positionIndices[static_cast<size_t>(pi)];
                }
                if (pi < 0 || pi >= vertexCount)
                    return -1;
                return pi;
            };
            for (size_t t = 0; t < group.triangles.size(); ++t)
            {
                if (remapPos(group.triangles[t].indices[0]) < 0
                    || remapPos(group.triangles[t].indices[1]) < 0
                    || remapPos(group.triangles[t].indices[2]) < 0)
                    continue;
                uvCounts.append(3);
                if (!group.uvs.empty())
                {
                    for (int v = 0; v < 3; ++v)
                    {
                        int uvIdx = group.triangles[t].indices[v];
                        if (uvIdx >= 0 && uvIdx < static_cast<int>(group.uvs.size()))
                            uvIds.append(static_cast<int>(uvBaseOffset) + uvIdx);
                        else
                            uvIds.append(0);
                    }
                }
                else
                {
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
        std::string shaderName = stripSGSuffix(shaderTemplateName);
        if (shaderName.empty())
            return std::string();
        if (shaderName.size() < 4 || shaderName.compare(shaderName.size() - 4, 4, ".sht") != 0)
            shaderName += ".sht";
        return resolveTreeAssetPath(shaderName, inputFilePath, "shader/");
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

namespace
{
    /// A shadingEngine made with createNode is not in renderPartition, so renderers (Maya Software, Arnold,
    /// batch) skip it even though the viewport shows it; `sets -renderable true` would have added it.
    void addToRenderPartition(MObject sgObj)
    {
        MFnDependencyNode sgFn(sgObj);
        MPlug partition = sgFn.findPlug("partition", true);
        if (partition.isNull() || partition.isConnected())
            return;
        MGlobal::executeCommand("connectAttr -na \"" + sgFn.name() + ".partition\" renderPartition.sets", false, false);
    }
}

MStatus MayaSceneBuilder::createMaterial(
    const std::string& shaderName,
    const std::string& texturePath,
    MObject& outShadingGroup,
    const std::string& swgShaderPath,
    const std::string& swgTexturePath,
    const std::string& soeEffectName,
    const std::string& swgTextureSlots)
{
    MStatus status;
    if (texturePath.empty())
        shaderLog("createMaterial: %s with NO texture (gray default)", shaderName.c_str());

    MFnLambertShader lambertFn;
    MObject shaderObj = lambertFn.create(true, &status);
    if (!status)
        return status;
    lambertFn.setName(shaderName.c_str(), &status);
    lambertFn.setColor(MColor(0.5f, 0.5f, 0.5f));
    lambertFn.setTransparency(MColor(0.0f, 0.0f, 0.0f));
    {
        MPlug amb = lambertFn.findPlug("ambientColor", true);
        if (!amb.isNull() && amb.numChildren() >= 3)
        {
            amb.child(0).setFloat(0.65f);
            amb.child(1).setFloat(0.65f);
            amb.child(2).setFloat(0.65f);
        }
    }
    const MString shaderNodeName = lambertFn.name();

    MDGModifier sgMod;
    MObject sgObj = sgMod.createNode("shadingEngine", &status);
    if (!status)
        return status;
    status = sgMod.doIt();
    if (!status)
        return status;
    MFnDependencyNode sgFn(sgObj);
    sgFn.setName(shaderNodeName + "SG");
    addToRenderPartition(sgObj);

    {
        MDGModifier connectMod;
        MPlug outColor = lambertFn.findPlug("outColor", true);
        MPlug surfaceShader = sgFn.findPlug("surfaceShader", true);
        if (outColor.isNull() || surfaceShader.isNull())
            return MS::kFailure;
        status = connectMod.connect(outColor, surfaceShader);
        if (status)
            status = connectMod.doIt();
        if (!status)
            return status;
    }

    if (!texturePath.empty())
    {
        std::string pathToUse = texturePath;
        for (size_t i = 0; i < pathToUse.size(); ++i)
            if (pathToUse[i] == '\\') pathToUse[i] = '/';

        if (!MayaUtility::fileExists(pathToUse))
        {
            const std::string win = resolveWindowsMayaAbsolutePath(pathToUse);
            if (!win.empty() && MayaUtility::fileExists(win))
                pathToUse = win;
        }
        for (char& c : pathToUse)
            if (c == '\\')
                c = '/';

        MFnDependencyNode fileFn;
        MObject fileObj = fileFn.create("file", (shaderName + "_tex").c_str(), &status);
        if (status)
        {
            bindMayaFileTexturePath(fileFn, pathToUse.c_str());
            {
                MPlug ail = fileFn.findPlug("alphaIsLuminance", true);
                if (!ail.isNull())
                    ail.setBool(false);
            }
            MPlug fileNamePlug = fileFn.findPlug("fileTextureName", true);
            if (!fileNamePlug.isNull())
            {
                MString now;
                fileNamePlug.getValue(now);
                if (now.length() == 0)
                    std::cerr << "ImportShader: setAttr fileTextureName FAILED for " << pathToUse << "\n";
            }

            {
                MFnTypedAttribute tAttr;
                MObject a = tAttr.create("swgResolvedTexture", "swgResolvedTexture", MFnData::kString);
                if (fileFn.addAttribute(a))
                {
                    MPlug p = fileFn.findPlug("swgResolvedTexture", true);
                    if (!p.isNull())
                        p.setValue(MString(pathToUse.c_str()));
                }
            }

            // One connection per modifier. A single failed optional plug used to abort outUV too.
            MFnDependencyNode placeFn;
            MObject placeObj = placeFn.create("place2dTexture", (shaderName + "_p2d").c_str(), &status);
            if (status && !placeObj.isNull())
            {
                auto connectOne = [&](const char* src, const char* dst) {
                    MPlug a = placeFn.findPlug(src, true);
                    MPlug b = fileFn.findPlug(dst, true);
                    if (a.isNull() || b.isNull() || b.isConnected())
                        return;
                    MDGModifier mod;
                    if (mod.connect(a, b) == MS::kSuccess)
                        mod.doIt();
                };
                connectOne("outUV", "uvCoord");
                connectOne("outUvFilterSize", "uvFilterSize");
                connectOne("coverage", "coverage");
                connectOne("translateFrame", "translateFrame");
                connectOne("rotateFrame", "rotateFrame");
                connectOne("mirrorU", "mirrorU");
                connectOne("mirrorV", "mirrorV");
                connectOne("stagger", "stagger");
                connectOne("wrapU", "wrapU");
                connectOne("wrapV", "wrapV");
                connectOne("repeatUV", "repeatUV");
                connectOne("offset", "offset");
                connectOne("rotateUV", "rotateUV");
                connectOne("noiseUV", "noiseUV");
            }

            MDGModifier texMod;
            MPlug texOut = fileFn.findPlug("outColor", true);
            MPlug lambertColor = lambertFn.findPlug("color", true);
            if (!texOut.isNull() && !lambertColor.isNull())
            {
                if (texMod.connect(texOut, lambertColor) == MS::kSuccess)
                    texMod.doIt();
            }
            // Leave lambert color at mid-gray. Setting it to white made SAT meshes look
            // "default white" whenever VP2 failed to sample the file node (top-left TGA / OCIO).
        }
    }

    outShadingGroup = sgObj;

    if (!swgShaderPath.empty() || !swgTexturePath.empty() || !soeEffectName.empty() || !swgTextureSlots.empty())
    {
        auto setStrAttrOn = [](MFnDependencyNode& fn, const char* name, const std::string& val) {
            if (val.empty()) return;
            MPlug p = fn.findPlug(name, true);
            if (p.isNull())
            {
                MFnTypedAttribute tAttr;
                MObject a = tAttr.create(name, name, MFnData::kString);
                if (fn.addAttribute(a))
                {
                    p = fn.findPlug(name, true);
                    if (!p.isNull()) p.setValue(MString(val.c_str()));
                }
            }
            else
                p.setValue(MString(val.c_str()));
        };
        MFnDependencyNode sgFn(outShadingGroup);
        setStrAttrOn(sgFn, "swgShaderPath", swgShaderPath);
        setStrAttrOn(sgFn, "swgTexturePath", swgTexturePath);
        setStrAttrOn(sgFn, "swgTextureSlots", swgTextureSlots);
        setStrAttrOn(sgFn, "soe_effectName", soeEffectName);

        MSelectionList surfSel;
        if (surfSel.add(shaderNodeName) == MS::kSuccess)
        {
            MObject surfObj;
            if (surfSel.getDependNode(0, surfObj) == MS::kSuccess)
            {
                MFnDependencyNode surfFn(surfObj);
                setStrAttrOn(surfFn, "soe_effectName", soeEffectName);
                setStrAttrOn(surfFn, "soe_effect", soeEffectName);
            }
        }
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

    MStatus status;
    auto findDependNode = [](const char* name, MObject& out) -> bool {
        MSelectionList list;
        if (list.add(MString(name)) != MS::kSuccess || list.length() == 0)
            return false;
        return list.getDependNode(0, out) == MS::kSuccess;
    };

    static const char* kLambertName = "swgPob_collisionPreviewGreen";
    static const char* kSgName = "swgPob_collisionPreviewGreenSG";

    MObject lambertObj;
    if (!findDependNode(kLambertName, lambertObj))
    {
        MFnLambertShader lambertFn;
        lambertObj = lambertFn.create(true, &status);
        if (!status)
            return status;
        lambertFn.setName(kLambertName);
        lambertFn.setColor(MColor(0.14f, 0.68f, 0.22f));
    }

    MObject sgObj;
    if (!findDependNode(kSgName, sgObj))
    {
        MDGModifier createMod;
        sgObj = createMod.createNode("shadingEngine", &status);
        status = createMod.doIt();
        if (!status)
            return status;
        MFnDependencyNode sgDepFn(sgObj);
        sgDepFn.setName(kSgName);
        addToRenderPartition(sgObj);

        MDGModifier connectMod;
        MFnDependencyNode lambertDep(lambertObj);
        MPlug outColor = lambertDep.findPlug("outColor", true);
        MFnDependencyNode sgDep(sgObj);
        MPlug surfaceShader = sgDep.findPlug("surfaceShader", true);
        if (!outColor.isNull() && !surfaceShader.isNull())
        {
            status = connectMod.connect(outColor, surfaceShader);
            if (status)
                status = connectMod.doIt();
            if (!status)
                return status;
        }
    }

    MFnSingleIndexedComponent comp;
    comp.setCompleteData(MFn::kMeshPolygonComponent);
    MFnSet setFn(sgObj, &status);
    if (!status)
        return status;
    return setFn.addMember(meshShapePath, comp.object());
}

MStatus MayaSceneBuilder::assignMaterials(
    const MDagPath& meshPath,
    const std::vector<ShaderGroupData>& shaderGroups,
    const std::string& inputFilePath)
{
    if (shaderGroups.empty()) return MS::kSuccess;

    auto findDependNode = [](const MString& name, MObject& out) -> bool {
        MSelectionList list;
        if (list.add(name) != MS::kSuccess || list.length() == 0)
            return false;
        return list.getDependNode(0, out) == MS::kSuccess;
    };

    // Per-mesh only: same shader template on another mesh must import again (own texture / nodes). A global cache
    // reused SGs across imports and forced the second mesh green or wrong texture.
    std::map<std::string, std::string> shaderToActualSgName;
    std::map<std::string, MObject> shaderToSgObj;
    const std::string meshTok = SceneBuilderHelpers::nextMaterialImportInstanceToken();

    int faceOffset = 0;
    std::string storedFaceSgs;
    MDagPath meshShapePath = meshPath;
    if (meshShapePath.hasFn(MFn::kTransform))
        meshShapePath.extendToShape();
    MFnDagNode meshDagFn(meshShapePath);

    for (size_t sgIdx = 0; sgIdx < shaderGroups.size(); ++sgIdx)
    {
        const ShaderGroupData& group = shaderGroups[sgIdx];
        int faceCount = static_cast<int>(group.triangles.size());
        if (faceCount == 0) continue;
        MGlobal::displayInfo(MString("MayaSceneBuilder: material ") + static_cast<int>(sgIdx + 1)
            + "/" + static_cast<int>(shaderGroups.size()) + " " + group.shaderTemplateName.c_str());

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
            MObject sgObj;
            const bool materialExists = findDependNode(sgNodeName, sgObj);

            if (!materialExists)
            {
                std::string resolvedShaderFile = SceneBuilderHelpers::resolveShaderPath(group.shaderTemplateName, inputFilePath);
                bool useDefault = resolvedShaderFile.empty() || !SceneBuilderHelpers::shaderFileExists(resolvedShaderFile);
                shaderLog("assignMaterials: shader=%s resolved=%s exists=%s", group.shaderTemplateName.c_str(), resolvedShaderFile.c_str(), useDefault ? "no" : "yes");

                if (!useDefault)
                {
                    shaderLog("assignMaterials: importFromPath %s base=%s", resolvedShaderFile.c_str(), materialInstanceBase.c_str());
                    MString importedSg;
                    MObject importedObj;
                    if (ImportShader::importFromPath(resolvedShaderFile, materialInstanceBase, importedSg, &importedObj) == MS::kSuccess && importedSg.length() > 0)
                    {
                        sgNodeName = importedSg;
                        shaderToActualSgName[cacheKey] = sgNodeName.asChar();
                        shaderToSgObj[cacheKey] = importedObj;
                    }
                    else
                    {
                        shaderLog("assignMaterials: importFromPath FAILED for %s", resolvedShaderFile.c_str());
                        useDefault = true;
                    }
                }
                if (useDefault)
                {
                    shaderLog("assignMaterials: using default (green) material for %s", materialInstanceBase.c_str());
                    MObject dummySG;
                    createMaterial(materialInstanceBase, std::string(), dummySG);
                    MFnDependencyNode fn(dummySG);
                    sgNodeName = fn.absoluteName();
                    if (sgNodeName.length() == 0)
                        sgNodeName = fn.name();
                    shaderToActualSgName[cacheKey] = sgNodeName.asChar();
                    shaderToSgObj[cacheKey] = dummySG;
                }
            }
            else
            {
                shaderToActualSgName[cacheKey] = sgNodeName.asChar();
                shaderToSgObj[cacheKey] = sgObj;
            }
        }

        MObject shadingGroupObj;
        MStatus setStatus = MS::kFailure;
        auto objIt = shaderToSgObj.find(cacheKey);
        if (objIt != shaderToSgObj.end() && !objIt->second.isNull())
            shadingGroupObj = objIt->second;
        if (shadingGroupObj.isNull())
            findDependNode(sgNodeName, shadingGroupObj);
        if (!shadingGroupObj.isNull())
        {
            MFnSet setFn(shadingGroupObj, &setStatus);
            if (setStatus)
            {
                MIntArray faceIds;
                faceIds.setLength(static_cast<unsigned>(faceCount));
                for (int f = 0; f < faceCount; ++f)
                    faceIds[f] = faceOffset + f;

                // Static MSH/APT: one mesh per shader. Face-component addMember often fails
                // after parented create() / File>Import namespace — bind the whole shape first.
                if (shaderGroups.size() == 1)
                {
                    setStatus = setFn.addMember(meshShapePath);
                    if (!setStatus)
                        setStatus = setFn.addMember(meshShapePath.node());
                    if (setStatus)
                        shaderLog("assignMaterials: whole-mesh sg=%s mesh=%s", sgNodeName.asChar(), meshShapePath.fullPathName().asChar());
                }
                if (!setStatus)
                {
                    MFnSingleIndexedComponent compFn;
                    MObject compObj = compFn.create(MFn::kMeshPolygonComponent, &setStatus);
                    if (setStatus)
                        setStatus = compFn.addElements(faceIds);
                    if (setStatus)
                        setStatus = setFn.addMember(meshShapePath, compObj);
                }
                if (!setStatus)
                {
                    setStatus = setFn.addMember(meshShapePath);
                    if (!setStatus)
                        setStatus = setFn.addMember(meshShapePath.node());
                    if (setStatus)
                        shaderLog("assignMaterials: whole-mesh fallback sg=%s mesh=%s", sgNodeName.asChar(), meshShapePath.fullPathName().asChar());
                }
            }
        }

        {
            MString faceCmd = "sets -e -forceElement ";
            faceCmd += sgNodeName;
            faceCmd += " \"";
            faceCmd += meshShapePath.fullPathName();
            faceCmd += ".f[";
            faceCmd += faceOffset;
            faceCmd += ":";
            faceCmd += (faceOffset + faceCount - 1);
            faceCmd += "]\"";
            MGlobal::executeCommand(faceCmd, false, false);
            if (!storedFaceSgs.empty())
                storedFaceSgs += ';';
            storedFaceSgs += sgNodeName.asChar();
            storedFaceSgs += ' ';
            storedFaceSgs += std::to_string(faceOffset);
            storedFaceSgs += ' ';
            storedFaceSgs += std::to_string(faceCount);
        }

        if (setStatus)
        {
            shaderLog("assignMaterials: assigned %s to %s faces %d-%d", sgNodeName.asChar(), meshShapePath.fullPathName().asChar(), faceOffset, faceOffset + faceCount - 1);
            MGlobal::displayInfo(MString("MayaSceneBuilder: assigned ") + sgNodeName
                + " -> " + meshShapePath.fullPathName());
        }
        else
        {
            shaderLog("assignMaterials: MFnSet addMember FAILED sg=%s mesh=%s", sgNodeName.asChar(), meshShapePath.fullPathName().asChar());
            MGlobal::displayWarning(MString("MayaSceneBuilder: shading group NOT bound sg=")
                + sgNodeName + " mesh=" + meshShapePath.fullPathName());
            std::cerr << "MayaSceneBuilder: MFnSet addMember failed sg=" << sgNodeName.asChar() << " mesh=" << meshShapePath.fullPathName().asChar() << "\n";
        }

        faceOffset += faceCount;
    }

    {
        MFnMesh meshFn(meshShapePath);
        meshFn.updateSurface();
        meshFn.syncObject();
    }
    if (!storedFaceSgs.empty())
    {
        MDagPath xf = meshShapePath;
        if (xf.hasFn(MFn::kMesh) && xf.length() > 0)
            xf.pop(1);
        MFnDependencyNode xfFn(xf.node());
        MPlug existing = xfFn.findPlug("swgFaceShading", true);
        if (existing.isNull())
        {
            MFnTypedAttribute tAttr;
            MObject a = tAttr.create("swgFaceShading", "swgFaceShading", MFnData::kString);
            tAttr.setStorable(true);
            if (xfFn.addAttribute(a))
                existing = xfFn.findPlug("swgFaceShading", true);
        }
        if (!existing.isNull())
            existing.setValue(MString(storedFaceSgs.c_str()));
    }
    scheduleImportedTextureRebind();
    return MS::kSuccess;
}

void MayaSceneBuilder::reapplyStoredShading(const MDagPath& meshPath)
{
    // the visible shape: once a skinCluster/blendShape exists the transform also holds an intermediate
    // "...Orig" mesh, and extendToShape() no longer picks one
    MDagPath xf = meshPath;
    if (xf.hasFn(MFn::kMesh) && xf.length() > 0)
        xf.pop(1);
    MDagPath shape = meshPath;
    for (unsigned c = 0; c < xf.childCount(); ++c)
    {
        MDagPath child = xf;
        child.push(xf.child(c));
        if (child.hasFn(MFn::kMesh) && !MFnDagNode(child).isIntermediateObject())
        {
            shape = child;
            break;
        }
    }
    MFnDependencyNode xfFn(xf.node());
    MPlug p = xfFn.findPlug("swgFaceShading", true);
    if (p.isNull())
        return;
    MString blob;
    p.getValue(blob);
    const std::string s = blob.asChar();
    size_t i = 0;
    while (i < s.size())
    {
        size_t semi = s.find(';', i);
        if (semi == std::string::npos)
            semi = s.size();
        const std::string rec = s.substr(i, semi - i);
        int start = 0;
        int count = 0;
        std::string sg;
        {
            size_t sp1 = rec.find(' ');
            size_t sp2 = rec.rfind(' ');
            if (sp1 != std::string::npos && sp2 != std::string::npos && sp2 > sp1)
            {
                sg = rec.substr(0, sp1);
                start = atoi(rec.substr(sp1 + 1, sp2 - sp1 - 1).c_str());
                count = atoi(rec.substr(sp2 + 1).c_str());
            }
        }
        if (!sg.empty() && count > 0)
        {
            // (was wrapped in catchQuiet("..."): catchQuiet evaluates an expression, so the quoted command
            // string never ran and per-face materials were lost once a skin/blend deformer was added)
            MString cmd = "sets -e -forceElement ";
            cmd += sg.c_str();
            cmd += " \"";
            cmd += shape.fullPathName();
            cmd += ".f[";
            cmd += start;
            cmd += ":";
            cmd += (start + count - 1);
            cmd += "]\"";
            if (!MGlobal::executeCommand(cmd, false, false))
                std::cerr << "MayaSceneBuilder: could not reassign " << sg << " to " << shape.fullPathName().asChar() << std::endl;
        }
        i = semi + 1;
    }
    scheduleImportedTextureRebind();
}

namespace
{
    MCallbackId s_textureRebindCb = 0;

    void rebindImportedFileTextures(void*)
    {
        if (s_textureRebindCb)
        {
            MMessage::removeCallback(s_textureRebindCb);
            s_textureRebindCb = 0;
        }
        int rebound = 0;
        MItDependencyNodes it(MFn::kFileTexture);
        for (; !it.isDone(); it.next())
        {
            MFnDependencyNode fn(it.thisNode());
            MPlug src = fn.findPlug("swgResolvedTexture", true);
            if (src.isNull())
                continue;
            MString path;
            src.getValue(path);
            if (path.length() == 0)
                continue;
            MPlug dest = fn.findPlug("fileTextureName", true);
            if (dest.isNull())
                continue;
            bindMayaFileTexturePath(fn, path);
            ++rebound;
        }
        MayaSceneBuilder::forceVp2TexturedViewport();
        if (rebound > 0)
            MGlobal::executeCommand(
                "python(\"from swg_mocap.maya_bridge import fix_sat_viewport; fix_sat_viewport()\")",
                false);
            MGlobal::displayInfo(MString("SwgMayaEditor 20260831k: rebound ") + rebound + " file textures + MAIN PNG + VP2");
    }
}

void MayaSceneBuilder::forceVp2TexturedViewport()
{
    // Arnold's viewport override draws untranslated Maya Lamberts as solid neon green.
    MGlobal::executeCommand(
        "catchQuiet(`setAttr -type \"string\" defaultRenderGlobals.currentRenderer \"mayaHardware2\"`);"
        "string $p[] = `getPanel -type modelPanel`;"
        "for ($i = 0; $i < size($p); ++$i) {"
        "  catchQuiet(`modelEditor -e -rendererName \"vp2Renderer\" $p[$i]`);"
        "  catchQuiet(`modelEditor -e -rnm \"vp2Renderer\" $p[$i]`);"
        "  catchQuiet(`modelEditor -e -displayAppearance \"smoothShaded\" -displayTextures true -udm false $p[$i]`);"
        "}"
        "catchQuiet(`DisplayShadedAndTextured`);",
        false);
}

void MayaSceneBuilder::scheduleImportedTextureRebind()
{
    if (s_textureRebindCb)
        return;
    s_textureRebindCb = MEventMessage::addEventCallback("idle", rebindImportedFileTextures);
}

namespace
{
    MObject findMeshShapeUnder(const MObject& obj)
    {
        if (obj.isNull())
            return obj;
        if (obj.hasFn(MFn::kMesh))
            return obj;
        if (obj.hasFn(MFn::kTransform))
        {
            MFnDagNode dag(obj);
            for (unsigned i = 0; i < dag.childCount(); ++i)
            {
                const MObject child = dag.child(i);
                if (child.hasFn(MFn::kMesh))
                    return child;
            }
        }
        return MObject::kNullObj;
    }

    /// Insert a skinCluster with Maya API only. MEL skinCluster deadlocks File>Import / HumanIK.
    MStatus createSkinClusterApi(
        const MDagPath& meshPath,
        const std::vector<MDagPath>& jointPaths,
        int maxInfluencesPerVertex,
        MObject& outSkin)
    {
        outSkin = MObject::kNullObj;
        MStatus status;

        MDagPath shapePath = meshPath;
        if (shapePath.hasFn(MFn::kTransform))
        {
            status = shapePath.extendToShape();
            if (!status)
                return status;
        }
        if (!shapePath.hasFn(MFn::kMesh))
            return MS::kFailure;

        MFnMesh copier;
        MObject origObj = copier.copy(shapePath.node(), shapePath.transform(), &status);
        if (!status)
            return status;
        origObj = findMeshShapeUnder(origObj);
        if (origObj.isNull())
            return MS::kFailure;

        MFnDagNode origDag(origObj);
        origDag.setIntermediateObject(true);
        origDag.setName(MFnDagNode(shapePath).name() + "Orig");

        MDGModifier createMod;
        MObject skinObj = createMod.createNode("skinCluster", &status);
        if (!status)
            return status;
        status = createMod.doIt();
        if (!status)
            return status;

        MFnDependencyNode skinDep(skinObj);
        {
            MPlug maxInf = skinDep.findPlug("maxInfluences", true);
            if (!maxInf.isNull())
                maxInf.setInt(maxInfluencesPerVertex);
            MPlug useMax = skinDep.findPlug("maintainMaxInfluences", true);
            if (!useMax.isNull())
                useMax.setBool(true);
            MPlug normalize = skinDep.findPlug("normalizeWeights", true);
            if (!normalize.isNull())
                normalize.setInt(1);
        }

        MDGModifier conn;
        MPlug origWorld = origDag.findPlug("worldMesh", true);
        if (origWorld.isNull())
            return MS::kFailure;
        origWorld = origWorld.elementByLogicalIndex(0);
        MPlug input = skinDep.findPlug("input", true);
        if (input.isNull())
            return MS::kFailure;
        MPlug inputGeom = input.elementByLogicalIndex(0).child(0);
        MPlug outputGeom = skinDep.findPlug("outputGeometry", true);
        if (outputGeom.isNull())
            return MS::kFailure;
        outputGeom = outputGeom.elementByLogicalIndex(0);
        MFnDagNode meshDag(shapePath);
        MPlug inMesh = meshDag.findPlug("inMesh", true);
        if (inMesh.isNull())
            return MS::kFailure;
        status = conn.connect(origWorld, inputGeom);
        if (status)
            status = conn.connect(outputGeom, inMesh);
        if (!status)
            return status;

        MPlug matArr = skinDep.findPlug("matrix", true);
        MPlug preArr = skinDep.findPlug("bindPreMatrix", true);
        if (matArr.isNull() || preArr.isNull())
            return MS::kFailure;

        for (size_t i = 0; i < jointPaths.size(); ++i)
        {
            MFnDagNode jn(jointPaths[i]);
            MPlug worldMat = jn.findPlug("worldMatrix", true);
            if (worldMat.isNull())
                return MS::kFailure;
            worldMat = worldMat.elementByLogicalIndex(0);
            status = conn.connect(worldMat, matArr.elementByLogicalIndex(static_cast<unsigned>(i)));
            if (!status)
                return status;

            MFnMatrixData matData;
            MObject invObj = matData.create(jointPaths[i].inclusiveMatrixInverse(), &status);
            if (status)
                preArr.elementByLogicalIndex(static_cast<unsigned>(i)).setValue(invObj);
        }

        MPlug geomMat = skinDep.findPlug("geomMatrix", true);
        if (!geomMat.isNull())
        {
            MFnMatrixData matData;
            MObject gm = matData.create(shapePath.inclusiveMatrix(), &status);
            if (status)
                geomMat.setValue(gm);
        }

        status = conn.doIt();
        if (!status)
            return status;

        MFnSet setFn(skinObj, &status);
        if (status)
            setFn.addMember(shapePath.node());

        outSkin = skinObj;
        return MS::kSuccess;
    }
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

    MObject skinClusterObj;
    MStatus status = createSkinClusterApi(meshPath, jointPaths, maxInfluencesPerVertex, skinClusterObj);
    if (!status || skinClusterObj.isNull())
    {
        MGlobal::displayInfo(MString("MayaSceneBuilder: API skinCluster failed: ") + status.errorString());
        return status ? MS::kFailure : status;
    }

    MFnSkinCluster skinFn(skinClusterObj, &status);
    if (!status) return status;

    MDagPath meshShapePath = meshPath;
    if (meshShapePath.hasFn(MFn::kTransform))
        meshShapePath.extendToShape();

    const unsigned nVerts = static_cast<unsigned>(weightHeaders.size());
    const unsigned nInf = static_cast<unsigned>(jointPaths.size());
    if (nVerts == 0 || nInf == 0)
        return MS::kSuccess;

    MIntArray influenceIndices;
    influenceIndices.setLength(nInf);
    for (unsigned i = 0; i < nInf; ++i)
    {
        MStatus idxSt;
        const unsigned mayaIdx = skinFn.indexForInfluenceObject(jointPaths[i], &idxSt);
        influenceIndices[i] = static_cast<int>(idxSt ? mayaIdx : i);
    }

    MDoubleArray allWeights;
    allWeights.setLength(nVerts * nInf);
    for (unsigned i = 0; i < allWeights.length(); ++i)
        allWeights[i] = 0.0;

    int weightOffset = 0;
    for (unsigned v = 0; v < nVerts; ++v)
    {
        const int numWeights = (v < weightHeaders.size()) ? weightHeaders[v] : 0;
        double weightSum = 0.0;
        for (int w = 0; w < numWeights && (weightOffset + w) < static_cast<int>(weightData.size()); ++w)
        {
            const SkinWeight& sw = weightData[static_cast<size_t>(weightOffset + w)];
            auto it = transformToJoint.find(sw.transformIndex);
            if (it == transformToJoint.end() || it->second >= nInf)
                continue;
            const double wval = static_cast<double>(sw.weight);
            allWeights[v * nInf + static_cast<unsigned>(it->second)] += wval;
            weightSum += wval;
        }
        if (weightSum > 0.0 && std::fabs(weightSum - 1.0) > 1e-6)
        {
            for (unsigned i = 0; i < nInf; ++i)
                allWeights[v * nInf + i] /= weightSum;
        }
        weightOffset += numWeights;
    }

    MFnSingleIndexedComponent vertComp;
    MObject vertObj = vertComp.create(MFn::kMeshVertComponent, &status);
    if (!status)
        return status;
    status = vertComp.setCompleteData(static_cast<int>(nVerts));
    if (!status)
        return status;

    status = skinFn.setWeights(meshShapePath, vertObj, influenceIndices, allWeights, false);
    if (!status)
    {
        MGlobal::displayInfo("MayaSceneBuilder: setWeights failed");
        return status;
    }

    MGlobal::displayInfo(MString("MayaSceneBuilder: skinCluster API bind verts=")
        + static_cast<int>(nVerts) + " joints=" + static_cast<int>(nInf));
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

    // blendShape -frontOfChain: the morphs deform the bind-pose mesh ahead of any skinCluster (the engine
    // applies blend targets before skinning), and the command wires the deformer chain itself - the API
    // addTarget wants mesh shapes and left a half-built deformer that blocked the later skin bind.
    MString cmd = "blendShape -frontOfChain -origin local -name \"";
    cmd += MFnDagNode(shapePath.transform()).name() + "_blendShape\"";
    MStringArray targetPaths;
    for (unsigned int i = 0; i < targetShapeObjects.length(); ++i)
    {
        const MString path = MFnDagNode(targetShapeObjects[i]).fullPathName();
        targetPaths.append(path);
        cmd += " \"" + path + "\"";
    }
    cmd += " \"" + shapePath.fullPathName() + "\"";
    status = MGlobal::executeCommand(cmd, false, false);
    if (!status)
        std::cerr << "MayaSceneBuilder::createBlendShapes: blendShape failed for " << shapePath.fullPathName().asChar()
                  << std::endl;
    for (unsigned int i = 0; i < targetPaths.length(); ++i)
        MGlobal::executeCommand("delete \"" + targetPaths[i] + "\"", false, false);
    return status;
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

        {
            MPlug radiusPlug = jointFn.findPlug("radius", true);
            if (!radiusPlug.isNull())
                radiusPlug.setDouble(0.12);
        }

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
        storeJointBind(dagPath);
        outJointPaths[i] = dagPath;
    }

    return MS::kSuccess;
}

namespace
{
    void setDouble3Attr(MObject node, const char* longName, const char* shortName, double a, double b, double c)
    {
        MFnDependencyNode depFn(node);
        if (!depFn.hasAttribute(longName))
        {
            MFnNumericAttribute nAttr;
            MObject attr = nAttr.create(longName, shortName, MFnNumericData::k3Double);
            nAttr.setKeyable(false);
            nAttr.setStorable(true);
            depFn.addAttribute(attr);
        }
        MPlug plug = depFn.findPlug(longName, true);
        if (plug.isNull() || plug.numChildren() != 3)
            return;
        plug.child(0).setDouble(a);
        plug.child(1).setDouble(b);
        plug.child(2).setDouble(c);
    }

    bool getDouble3Attr(MObject node, const char* longName, double& a, double& b, double& c)
    {
        MFnDependencyNode depFn(node);
        if (!depFn.hasAttribute(longName))
            return false;
        MPlug plug = depFn.findPlug(longName, true);
        if (plug.isNull() || plug.numChildren() != 3)
            return false;
        a = plug.child(0).asDouble();
        b = plug.child(1).asDouble();
        c = plug.child(2).asDouble();
        return true;
    }
}

void MayaSceneBuilder::storeJointBind(const MDagPath& jointPath)
{
    MFnIkJoint jointFn(jointPath.node());
    MEulerRotation r;
    jointFn.getRotation(r);
    const MVector t = jointFn.getTranslation(MSpace::kTransform);
    setDouble3Attr(jointPath.node(), "swgBindRotate", "sbr", r.x, r.y, r.z);
    setDouble3Attr(jointPath.node(), "swgBindTranslate", "sbt", t.x, t.y, t.z);
}

bool MayaSceneBuilder::getJointBind(const MDagPath& jointPath, MEulerRotation& outRotate, MVector& outTranslate)
{
    MFnIkJoint jointFn(jointPath.node());
    const MEulerRotation::RotationOrder order = SwgAnimMath::eulerOrderOf(jointFn);
    double rx, ry, rz, tx, ty, tz;
    if (getDouble3Attr(jointPath.node(), "swgBindRotate", rx, ry, rz)
        && getDouble3Attr(jointPath.node(), "swgBindTranslate", tx, ty, tz))
    {
        outRotate = MEulerRotation(rx, ry, rz, order);
        outTranslate = MVector(tx, ty, tz);
        return true;
    }
    jointFn.getRotation(outRotate);
    outTranslate = jointFn.getTranslation(MSpace::kTransform);
    return false;
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
    // Linear tangents: the engine interpolates linearly between keys
    const unsigned int numKeys = animCurveFn.numKeys();
    for (unsigned int i = 0; i < numKeys; ++i)
    {
        animCurveFn.setInTangentType(i, MFnAnimCurve::kTangentLinear);
        animCurveFn.setOutTangentType(i, MFnAnimCurve::kTangentLinear);
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

    MEulerRotation::RotationOrder rotOrder = SwgAnimMath::eulerOrderOf(jointFn);
    const double fpsVal = (fps > 0.0f) ? static_cast<double>(fps) : 30.0;

    // The engine plays rotation = delta * bind in engine space; Maya's rotate is the X-mirror of that
    // (see SwgAnimMath.h). The bind is the skeleton's, recorded at import (else the current pose).
    MEulerRotation bindEuler;
    MVector bindTranslate;
    getJointBind(jointPath, bindEuler, bindTranslate);
    const SwgAnimMath::Quat bindEngine = SwgAnimMath::engineFromMaya(bindEuler);

    // The file keys are sparse and the engine slerps between them; three linear Euler curves would not
    // follow that path, so key every whole frame with the engine's own interpolation.
    auto keyDelta = [&](size_t k) {
        const QuatKeyframe& qk = deltaKeyframes[k];
        return SwgAnimMath::Quat(qk.rotation[3], qk.rotation[0], qk.rotation[1], qk.rotation[2]).normalized();
    };
    MEulerRotation prevEuler = bindEuler;
    auto addKey = [&](double frame, const SwgAnimMath::Quat& delta) {
        const SwgAnimMath::Quat finalEngine = (delta * bindEngine).normalized();
        const MEulerRotation e = SwgAnimMath::mayaFromEngine(finalEngine, rotOrder, &prevEuler);
        prevEuler = e;
        const MTime time(frame / fpsVal, MTime::kSeconds);
        rxCurve.addKeyframe(time, e.x);
        ryCurve.addKeyframe(time, e.y);
        rzCurve.addKeyframe(time, e.z);
    };
    for (size_t k = 0; k < deltaKeyframes.size(); ++k)
    {
        const double f0 = static_cast<double>(deltaKeyframes[k].frame);
        addKey(f0, keyDelta(k));
        if (k + 1 < deltaKeyframes.size())
        {
            const double f1 = static_cast<double>(deltaKeyframes[k + 1].frame);
            for (double f = std::floor(f0) + 1.0; f < f1 - 1e-6; f += 1.0)
                addKey(f, SwgAnimMath::slerp(keyDelta(k), keyDelta(k + 1), (f - f0) / (f1 - f0)));
        }
    }

    // Linear tangents: the engine interpolates linearly between keys
    const unsigned int numKeys = rxCurve.numKeys();
    for (unsigned int i = 0; i < numKeys; ++i)
    {
        rxCurve.setInTangentType(i, MFnAnimCurve::kTangentLinear);
        rxCurve.setOutTangentType(i, MFnAnimCurve::kTangentLinear);
        ryCurve.setInTangentType(i, MFnAnimCurve::kTangentLinear);
        ryCurve.setOutTangentType(i, MFnAnimCurve::kTangentLinear);
        rzCurve.setInTangentType(i, MFnAnimCurve::kTangentLinear);
        rzCurve.setOutTangentType(i, MFnAnimCurve::kTangentLinear);
    }
    return MS::kSuccess;
}
