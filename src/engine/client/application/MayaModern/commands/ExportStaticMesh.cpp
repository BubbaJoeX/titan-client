#include "ExportStaticMesh.h"
#include "StaticMeshWriter.h"
#include "SetDirectoryCommand.h"
#include "ShaderExporter.h"
#include "MayaConversions.h"
#include "MayaCompoundString.h"
#include "Iff.h"
#include "Quaternion.h"
#include "Tag.h"
#include "Vector.h"

#include <maya/MArgList.h>
#include <maya/MDagPath.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnMesh.h>
#include <maya/MFnTransform.h>
#include <maya/MGlobal.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MObject.h>
#include <maya/MFloatPointArray.h>
#include <maya/MIntArray.h>
#include <maya/MPointArray.h>
#include <maya/MPlug.h>
#include <maya/MSelectionList.h>
#include <maya/MStatus.h>
#include <maya/MString.h>
#include <maya/MTypes.h>
#include <maya/MVector.h>

#include <algorithm>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace
{
    constexpr int DEFAULT_IFF_SIZE = 65536;

    const Tag TAG_APT = TAG3(A,P,T);

    static std::string getStringAttr(MFnDependencyNode& fn, const char* attrName)
    {
        MPlug p = fn.findPlug(attrName, true);
        if (p.isNull()) return std::string();
        MString val;
        if (p.getValue(val) != MS::kSuccess) return std::string();
        return val.asChar();
    }

    static std::string toTreePath(const std::string& path)
    {
        std::string p = path;
        for (size_t i = 0; i < p.size(); ++i)
            if (p[i] == '\\') p[i] = '/';
        // Strip leading backslashes and drive
        while (!p.empty() && (p[0] == '/' || p[0] == '\\')) p.erase(0, 1);
        size_t colon = p.find(':');
        if (colon != std::string::npos && colon > 0)
            p = p.substr(colon + 1);
        return p;
    }

    static std::string ensureTrailingSlash(const std::string& s)
    {
        if (s.empty()) return s;
        char c = s.back();
        if (c != '/' && c != '\\') return s + '\\';
        return s;
    }
}

void* ExportStaticMesh::creator()
{
    return new ExportStaticMesh();
}

MStatus ExportStaticMesh::doIt(const MArgList& args)
{
    MStatus status;
    std::string outputPath;
    std::string meshName;

    for (unsigned i = 0; i < args.length(&status); ++i)
    {
        if (!status) return MS::kFailure;
        MString argName = args.asString(i, &status);
        if (!status) return MS::kFailure;

        if (argName == "-path" && i + 1 < args.length(&status))
        {
            outputPath = args.asString(i + 1, &status).asChar();
            if (status) ++i;
        }
        else if (argName == "-name" && i + 1 < args.length(&status))
        {
            meshName = args.asString(i + 1, &status).asChar();
            if (status) ++i;
        }
    }

    MSelectionList sel;
    status = MGlobal::getActiveSelectionList(sel);
    if (!status || sel.length() == 0)
    {
        std::cerr << "ExportStaticMesh: select a mesh" << std::endl;
        return MS::kFailure;
    }

    MDagPath dagPath;
    status = sel.getDagPath(0, dagPath);
    if (!status)
    {
        std::cerr << "ExportStaticMesh: failed to get DAG path" << std::endl;
        return MS::kFailure;
    }

    if (!dagPath.hasFn(MFn::kMesh))
        dagPath.extendToShape();
    if (!dagPath.hasFn(MFn::kMesh))
    {
        std::cerr << "ExportStaticMesh: selection is not a mesh" << std::endl;
        return MS::kFailure;
    }

    std::string outMeshPath, outAptPath;
    if (!performExport(dagPath, outputPath, outMeshPath, outAptPath))
        return MS::kFailure;

    MGlobal::displayInfo(MString("Exported mesh: ") + outMeshPath.c_str());
    if (!outAptPath.empty())
        MGlobal::displayInfo(MString("Exported APT: ") + outAptPath.c_str());
    return MS::kSuccess;
}

bool ExportStaticMesh::performExport(const MDagPath& meshDagPath, const std::string& outputPathOverride,
    std::string& outMeshPath, std::string& outAptPath)
{
    MStatus status;
    outMeshPath.clear();
    outAptPath.clear();

    MDagPath shapePath = meshDagPath;
    if (shapePath.hasFn(MFn::kTransform))
        shapePath.extendToShape();

    MFnMesh meshFn(shapePath, &status);
    if (!status)
    {
        std::cerr << "ExportStaticMesh: MFnMesh failed" << std::endl;
        return false;
    }

    MDagPath transformPath = shapePath;
    if (transformPath.hasFn(MFn::kMesh))
        transformPath.pop();

    MFnDagNode transformFn(transformPath, &status);
    if (!status) return false;

    MString nodeName = transformFn.name(&status);
    if (!status) return false;

    MayaCompoundString compoundName(nodeName);
    std::string meshName;
    if (compoundName.getComponentCount() > 1)
        meshName = compoundName.getComponentStdString(1);
    else
        meshName = nodeName.asChar();

    size_t dot = meshName.find_last_of('.');
    if (dot != std::string::npos)
        meshName = meshName.substr(0, dot);

    std::string meshWriteDir;
    if (!outputPathOverride.empty())
    {
        meshWriteDir = outputPathOverride;
        if (meshWriteDir.find('.') != std::string::npos)
        {
            size_t slash = meshWriteDir.find_last_of("/\\");
            if (slash != std::string::npos)
            {
                meshName = meshWriteDir.substr(slash + 1);
                size_t ext = meshName.find('.');
                if (ext != std::string::npos) meshName = meshName.substr(0, ext);
            }
            meshWriteDir = meshWriteDir.substr(0, meshWriteDir.find_last_of("/\\") + 1);
        }
    }
    else
    {
        const char* baseDir = SetDirectoryCommand::getDirectoryString(SetDirectoryCommand::APPEARANCE_WRITE_DIR_INDEX);
        if (!baseDir || !baseDir[0])
        {
            std::cerr << "ExportStaticMesh: appearance output directory not configured" << std::endl;
            return false;
        }
        meshWriteDir = ensureTrailingSlash(baseDir) + "mesh\\";
    }

    meshWriteDir = ensureTrailingSlash(meshWriteDir);

    MObjectArray shaderObjs;
    MIntArray faceToShader;
    status = meshFn.getConnectedShaders(shapePath.instanceNumber(), shaderObjs, faceToShader);
    if (!status)
    {
        std::cerr << "ExportStaticMesh: getConnectedShaders failed" << std::endl;
        return false;
    }

    MPointArray mayaPoints;
    status = meshFn.getPoints(mayaPoints, MSpace::kObject);
    if (!status)
    {
        std::cerr << "ExportStaticMesh: getPoints failed" << std::endl;
        return false;
    }

    MStringArray uvSetNames;
    meshFn.getUVSetNames(uvSetNames);
    const MString uvSetName = (uvSetNames.length() > 0) ? uvSetNames[0] : MString();

    std::map<int, StaticMeshWriterShaderGroup> shaderGroups;

    MItMeshPolygon polyIt(shapePath, MObject::kNullObj, &status);
    if (!status)
    {
        std::cerr << "ExportStaticMesh: MItMeshPolygon failed" << std::endl;
        return false;
    }

    for (; !polyIt.isDone(); polyIt.next())
    {
        int faceIdx = polyIt.index();
        int shaderIdx = (faceIdx < faceToShader.length()) ? faceToShader[faceIdx] : 0;
        if (shaderIdx < 0 || static_cast<unsigned>(shaderIdx) >= shaderObjs.length())
            shaderIdx = 0;

        MObject sgObj = shaderObjs[static_cast<unsigned>(shaderIdx)];
        MFnDependencyNode sgFn(sgObj, &status);
        std::string shaderTemplateName = "shader/placeholder";
        if (status)
        {
            std::string swgPath = getStringAttr(sgFn, "swgShaderPath");
            if (!swgPath.empty())
                shaderTemplateName = swgPath;
        }

        StaticMeshWriterShaderGroup& sg = shaderGroups[shaderIdx];
        if (sg.shaderTemplateName.empty())
            sg.shaderTemplateName = shaderTemplateName;

        MPointArray triPoints;
        MIntArray triVerts;
        status = polyIt.getTriangles(triPoints, triVerts, MSpace::kObject);
        if (!status) continue;

        auto getUV = [&](int meshVert) -> std::pair<float, float> {
            if (uvSetName.length() == 0) return {0.0f, 0.0f};
            for (unsigned i = 0; i < polyIt.polygonVertexCount(&status); ++i)
            {
                if (polyIt.vertexIndex(static_cast<int>(i), &status) == meshVert)
                {
                    float2 uv;
                    if (polyIt.getUV(i, uv, &uvSetName) == MS::kSuccess)
                        return {uv[0], uv[1]};
                    return {0.0f, 0.0f};
                }
            }
            return {0.0f, 0.0f};
        };

        for (unsigned t = 0; t + 2 < triVerts.length(); t += 3)
        {
            int v0 = triVerts[t];
            int v1 = triVerts[t + 1];
            int v2 = triVerts[t + 2];

            MVector n0, n1, n2;
            meshFn.getPolygonNormal(faceIdx, n0);
            n1 = n0;
            n2 = n0;

            float u0 = getUV(v0).first, v0_ = getUV(v0).second;
            float u1 = getUV(v1).first, v1_ = getUV(v1).second;
            float u2 = getUV(v2).first, v2_ = getUV(v2).second;

            auto addVert = [&](int vi, float u, float v, const MVector& n) {
                const MPoint& pt = mayaPoints[static_cast<unsigned>(vi)];
                Vector enginePos = MayaConversions::convertVector(MVector(pt.x, pt.y, pt.z));
                Vector engineNorm = MayaConversions::convertVector(n);

                size_t base = sg.positions.size() / 3;
                sg.positions.push_back(enginePos.x);
                sg.positions.push_back(enginePos.y);
                sg.positions.push_back(enginePos.z);
                sg.normals.push_back(engineNorm.x);
                sg.normals.push_back(engineNorm.y);
                sg.normals.push_back(engineNorm.z);
                sg.uvs.push_back(u);
                sg.uvs.push_back(1.0f - static_cast<float>(v));

                return static_cast<uint16_t>(base);
            };

            uint16_t i0 = addVert(v0, static_cast<float>(u0), static_cast<float>(v0_), n0);
            uint16_t i1 = addVert(v1, static_cast<float>(u1), static_cast<float>(v1_), n1);
            uint16_t i2 = addVert(v2, static_cast<float>(u2), static_cast<float>(v2_), n2);

            sg.indices.push_back(i0);
            sg.indices.push_back(i1);
            sg.indices.push_back(i2);
        }
    }

    std::set<std::string> exportedShaders;
    for (const auto& kv : shaderGroups)
    {
        const std::string& path = kv.second.shaderTemplateName;
        if (path.empty() || path == "shader/placeholder") continue;
        if (exportedShaders.insert(path).second)
        {
            std::string outPath = ShaderExporter::exportShader(path);
            if (!outPath.empty())
                std::cerr << "[ExportStaticMesh] Exported shader: " << outPath << "\n";
        }
    }

    StaticMeshWriter writer;

    for (unsigned i = 0; i < shaderObjs.length(); ++i)
    {
        auto it = shaderGroups.find(static_cast<int>(i));
        if (it != shaderGroups.end() && !it->second.positions.empty())
            writer.addShaderGroup(it->second);
    }

    if (transformPath.childCount(&status) > 0 && status)
    {
        for (unsigned c = 0; c < transformPath.childCount(&status); ++c)
        {
            MObject childObj = transformPath.child(c, &status);
            if (!status) continue;
            MFnDagNode childFn(childObj, &status);
            if (!status) continue;
            if (childFn.typeId(&status) != MFn::kTransform) continue;

            MDagPath childPath;
            if (!childFn.getPath(childPath)) continue;

            MFnTransform childTransform(childPath, &status);
            if (!status) continue;

            MString hpName = childTransform.name(&status);
            if (!status) continue;

            if (hpName == "floor_component")
            {
                MFnDependencyNode depFn(childPath.node(), &status);
                if (status)
                {
                    MPlug fp = depFn.findPlug("floorPath", false);
                    if (!fp.isNull())
                    {
                        MString pathVal;
                        if (fp.getValue(pathVal) == MS::kSuccess && pathVal.length() > 0)
                            writer.setFloorReference(pathVal.asChar());
                    }
                }
                continue;
            }

            MVector mayaTrans = childTransform.translation(MSpace::kTransform, &status);
            if (!status) continue;

            MEulerRotation mayaRot;
            status = childTransform.getRotation(mayaRot);
            if (!status) continue;

            Vector enginePos = MayaConversions::convertVector(mayaTrans);
            Quaternion engineRot = MayaConversions::convertRotation(mayaRot);

            StaticMeshWriterHardpoint hp;
            hp.name = hpName.asChar();
            hp.position[0] = enginePos.x;
            hp.position[1] = enginePos.y;
            hp.position[2] = enginePos.z;
            hp.rotation[0] = engineRot.x;
            hp.rotation[1] = engineRot.y;
            hp.rotation[2] = engineRot.z;
            hp.rotation[3] = engineRot.w;
            writer.addHardpoint(hp);
        }
    }

    std::string mshFullPath = meshWriteDir + meshName + ".msh";
    Iff iff(DEFAULT_IFF_SIZE, true);
    if (!writer.write(iff))
    {
        std::cerr << "ExportStaticMesh: StaticMeshWriter::write failed" << std::endl;
        return false;
    }
    if (!iff.write(mshFullPath.c_str(), true))
    {
        std::cerr << "ExportStaticMesh: failed to write " << mshFullPath << std::endl;
        return false;
    }

    outMeshPath = toTreePath(mshFullPath);
    if (outMeshPath.find("appearance/") != 0 && outMeshPath.find("appearance\\") != 0)
        outMeshPath = "appearance/mesh/" + meshName + ".msh";

    std::string aptWriteDir = ensureTrailingSlash(SetDirectoryCommand::getDirectoryString(SetDirectoryCommand::APPEARANCE_WRITE_DIR_INDEX));
    if (!aptWriteDir.empty())
    {
        std::string aptPath = aptWriteDir + meshName + ".apt";
        Iff aptIff(4096, true);
        aptIff.insertForm(TAG_APT);
        aptIff.insertForm(::TAG_0000);
        aptIff.insertChunk(::TAG_NAME);
        aptIff.insertChunkString(outMeshPath.c_str());
        aptIff.exitChunk(::TAG_NAME);
        aptIff.exitForm(::TAG_0000);
        aptIff.exitForm(TAG_APT);
        if (aptIff.write(aptPath.c_str(), true))
            outAptPath = toTreePath(aptPath);
    }

    return true;
}
