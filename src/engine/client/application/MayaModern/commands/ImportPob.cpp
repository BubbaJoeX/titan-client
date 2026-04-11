#include "ImportPob.h"
#include "ImportPathResolver.h"
#include "ImportLodMesh.h"
#include "MayaSceneBuilder.h"
#include "flr.h"

#include "Iff.h"
#include "Tag.h"
#include "Transform.h"
#include "Vector.h"

#include <maya/MArgList.h>
#include <maya/MFn.h>
#include <maya/MDagPath.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnTransform.h>
#include <maya/MMatrix.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MGlobal.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MSelectionList.h>

#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#endif

namespace
{
    static void pobLog(const char* fmt, ...)
    {
        char buf[1024];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        std::string msg = std::string("[ImportPob] ") + buf + "\n";
        std::cerr << msg;
#ifdef _WIN32
        OutputDebugStringA(msg.c_str());
#endif
    }

    static const Tag TAG_PRTO = TAG(P,R,T,O);
    static const Tag TAG_PRTS = TAG(P,R,T,S);
    static const Tag TAG_PRTL = TAG(P,R,T,L);
    static const Tag TAG_IDTL = TAG(I,D,T,L);
    static const Tag TAG_VERT = TAG(V,E,R,T);
    static const Tag TAG_INDX = TAG(I,N,D,X);
    static const Tag TAG_CELS = TAG(C,E,L,S);
    static const Tag TAG_CELL = TAG(C,E,L,L);
    static const Tag TAG_CRC = TAG3(C,R,C);
    static const Tag TAG_PGRF = TAG(P,G,R,F);

    struct PortalGeometry
    {
        std::vector<Vector> vertices;
        std::vector<int> indices;

        void loadFromPrtl(Iff& iff)
        {
            const int numVerts = iff.read_int32();
            vertices.resize(static_cast<size_t>(numVerts));
            for (int j = 0; j < numVerts; ++j)
                vertices[j] = iff.read_floatVector();
            if (numVerts >= 3)
            {
                for (int j = 1; j + 1 < numVerts; ++j)
                {
                    indices.push_back(0);
                    indices.push_back(j);
                    indices.push_back(j + 1);
                }
            }
        }

        void loadFromIdtl(Iff& iff)
        {
            iff.enterForm(TAG_IDTL);
            iff.enterForm(TAG_0000);
            iff.enterChunk(TAG_VERT);
            const int numVerts = iff.getChunkLengthLeft(12) / 12;
            vertices.resize(static_cast<size_t>(numVerts));
            iff.read_floatVector(numVerts, vertices.data());
            iff.exitChunk(TAG_VERT);
            iff.enterChunk(TAG_INDX);
            const int numIndices = iff.getChunkLengthLeft(4) / 4;
            indices.resize(static_cast<size_t>(numIndices));
            for (int j = 0; j < numIndices; ++j)
                indices[j] = iff.read_int32();
            iff.exitChunk(TAG_INDX);
            iff.exitForm(TAG_0000);
            iff.exitForm(TAG_IDTL);
        }
    };

    static std::string resolveTreeFilePath(const std::string& treeFilePath, const std::string& inputFilename)
    {
        if (treeFilePath.empty()) return std::string();
        std::string baseDir;
        const char* envExportRoot = getenv("TITAN_EXPORT_ROOT");
        if (envExportRoot && envExportRoot[0])
        {
            baseDir = envExportRoot;
            if (!baseDir.empty() && baseDir.back() != '/') baseDir += '/';
        }
        else
        {
            const char* envDataRoot = getenv("TITAN_DATA_ROOT");
            if (envDataRoot && envDataRoot[0])
            {
                baseDir = envDataRoot;
                if (!baseDir.empty() && baseDir.back() != '/') baseDir += '/';
            }
            else
            {
                std::string norm = inputFilename;
                for (auto& c : norm) if (c == '\\') c = '/';
                const auto cgPos = norm.find("compiled/game/");
                if (cgPos != std::string::npos)
                    baseDir = norm.substr(0, cgPos + 14);
            }
        }
        if (baseDir.empty()) return treeFilePath;
        std::string path = treeFilePath;
        if (path.find("appearance/") != 0 && path.find("appearance\\") != 0)
            path = "appearance/" + path;
        std::string resolved = baseDir + path;
        for (auto& c : resolved) if (c == '\\') c = '/';
        return resolved;
    }

    static std::string deriveFloorFromAppearance(const std::string& appearancePath)
    {
        std::string base;
        const auto lastSlash = appearancePath.find_last_of("/\\");
        base = (lastSlash != std::string::npos) ? appearancePath.substr(lastSlash + 1) : appearancePath;
        const auto dot = base.find_last_of('.');
        if (dot != std::string::npos) base = base.substr(0, dot);
        const auto meshPos = base.find("_mesh");
        base = (meshPos != std::string::npos) ? base.substr(0, meshPos) + "_collision_floor" : base + "_collision_floor";
        return "appearance/collision/" + base;
    }

    struct PortalData
    {
        int portalIndex = -1;
        bool clockwise = false;
        int targetCell = -1;
        bool disabled = false;
        bool passable = true;
        std::string doorStyle;
        bool hasDoorHardpoint = false;
        Transform doorTransform = Transform::identity;
    };

    /// PRTL chunk field order matches PortalPropertyTemplateCellPortal::load_0001..0005.
    static PortalData readPortalDataFromPrtl(Iff& iff)
    {
        PortalData pd;
        const Tag chunkTag = iff.getCurrentName();
        iff.enterChunk(chunkTag);
        if (chunkTag == TAG_0005)
        {
            pd.disabled = iff.read_bool8();
            pd.passable = iff.read_bool8();
            pd.portalIndex = iff.read_int32();
            pd.clockwise = iff.read_bool8();
            pd.targetCell = iff.read_int32();
            pd.doorStyle = iff.read_stdstring();
            pd.hasDoorHardpoint = iff.read_bool8();
            pd.doorTransform = iff.read_floatTransform();
        }
        else if (chunkTag == TAG_0004)
        {
            pd.disabled = false;
            pd.passable = iff.read_bool8();
            pd.portalIndex = iff.read_int32();
            pd.clockwise = iff.read_bool8();
            pd.targetCell = iff.read_int32();
            pd.doorStyle = iff.read_stdstring();
            pd.hasDoorHardpoint = iff.read_bool8();
            pd.doorTransform = iff.read_floatTransform();
        }
        else if (chunkTag == TAG_0003)
        {
            pd.disabled = false;
            pd.passable = iff.read_bool8();
            pd.portalIndex = iff.read_int32();
            pd.clockwise = iff.read_bool8();
            pd.targetCell = iff.read_int32();
            pd.doorStyle = iff.read_stdstring();
        }
        else if (chunkTag == TAG_0002)
        {
            pd.disabled = false;
            pd.passable = iff.read_bool8();
            pd.portalIndex = iff.read_int32();
            pd.clockwise = iff.read_bool8();
            pd.targetCell = iff.read_int32();
        }
        else if (chunkTag == TAG_0001)
        {
            pd.disabled = false;
            pd.passable = true;
            pd.portalIndex = iff.read_int32();
            pd.clockwise = iff.read_bool8();
            pd.targetCell = iff.read_int32();
        }
        iff.exitChunk(chunkTag);
        return pd;
    }

    static void engineTransformToMayaMatrix(const Transform& t, MMatrix& out)
    {
        const Transform::matrix_t& mm = t.getMatrix();
        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 4; ++col)
                out[row][col] = static_cast<double>(mm[row][col]);
        out[3][0] = 0.0;
        out[3][1] = 0.0;
        out[3][2] = 0.0;
        out[3][3] = 1.0;
    }

    static void addPortalAuthoringAttrs(MFnDependencyNode& transformDepFn, const PortalData& pd)
    {
        auto addIntAttr = [&](const char* name, int val) {
            MPlug p = transformDepFn.findPlug(name, true);
            if (p.isNull()) {
                MFnNumericAttribute nAttr;
                MObject a = nAttr.create(name, name, MFnNumericData::kInt);
                if (transformDepFn.addAttribute(a))
                    p = transformDepFn.findPlug(name, true);
            }
            if (!p.isNull()) p.setInt(val);
        };
        auto addBoolAttr = [&](const char* name, bool val) {
            MPlug p = transformDepFn.findPlug(name, true);
            if (p.isNull()) {
                MFnNumericAttribute nAttr;
                MObject a = nAttr.create(name, name, MFnNumericData::kBoolean);
                if (transformDepFn.addAttribute(a))
                    p = transformDepFn.findPlug(name, true);
            }
            if (!p.isNull()) p.setBool(val);
        };
        auto addStrAttr = [&](const char* name, const std::string& val) {
            MPlug p = transformDepFn.findPlug(name, true);
            if (p.isNull()) {
                MFnTypedAttribute tAttr;
                MObject a = tAttr.create(name, name, MFnData::kString);
                if (transformDepFn.addAttribute(a))
                    p = transformDepFn.findPlug(name, true);
            }
            if (!p.isNull()) p.setValue(MString(val.c_str()));
        };
        addIntAttr("buildingPortalIndex", pd.portalIndex);
        addBoolAttr("portalClockwise", pd.clockwise);
        addIntAttr("portalTargetCell", pd.targetCell);
        addBoolAttr("portalDisabled", pd.disabled);
        addBoolAttr("portalPassable", pd.passable);
        addStrAttr("doorStyle", pd.doorStyle);
    }

    static MStatus attachDoorHardpoint(MObject portalTransformObj, const PortalData& pd)
    {
        if (!pd.hasDoorHardpoint)
            return MS::kSuccess;
        MStatus st;
        MFnTransform doorFn;
        MObject doorObj = doorFn.create(portalTransformObj, &st);
        if (!st) return st;
        doorFn.setName("doorHardpoint");
        MMatrix dm;
        engineTransformToMayaMatrix(pd.doorTransform, dm);
        return doorFn.set(dm);
    }

    /// Creates portal transform under `parentObj`: mesh (if geometry exists) plus POB authoring attrs and optional `doorHardpoint` child.
    static MStatus createPortalRepresentation(
        const PortalGeometry* geom,
        const char* portalName,
        MObject parentObj,
        const PortalData& pd,
        MDagPath& outPortalTransformPath)
    {
        const bool hasMesh = geom && !geom->vertices.empty() && geom->indices.size() >= 3;
        MStatus st;
        if (hasMesh)
        {
            std::vector<float> positions;
            positions.reserve(geom->vertices.size() * 3);
            for (size_t i = 0; i < geom->vertices.size(); ++i)
            {
                positions.push_back(geom->vertices[i].x);
                positions.push_back(geom->vertices[i].y);
                positions.push_back(geom->vertices[i].z);
            }
            std::vector<float> normals(positions.size(), 0.0f);

            MayaSceneBuilder::ShaderGroupData sg;
            sg.shaderTemplateName = "shader/placeholder";
            for (size_t t = 0; t + 2 < geom->indices.size(); t += 3)
            {
                MayaSceneBuilder::TriangleData tri;
                tri.indices[0] = geom->indices[t];
                tri.indices[1] = geom->indices[t + 1];
                tri.indices[2] = geom->indices[t + 2];
                sg.triangles.push_back(tri);
            }

            std::vector<MayaSceneBuilder::ShaderGroupData> groups(1, sg);
            MDagPath meshShapePath;
            st = MayaSceneBuilder::createMesh(positions, normals, groups, portalName, meshShapePath);
            if (!st) return st;

            MObject shapeObj = meshShapePath.node();
            std::string shapeFullPath = meshShapePath.fullPathName().asChar();
            MString addCmd = "addAttr -ln portal -at bool \"";
            addCmd += shapeFullPath.c_str();
            addCmd += "\"";
            MGlobal::executeCommand(addCmd);
            MString setCmd = "setAttr \"";
            setCmd += shapeFullPath.c_str();
            setCmd += ".portal\" 1";
            MGlobal::executeCommand(setCmd);

            meshShapePath.pop(1);
            outPortalTransformPath = meshShapePath;
            MFnDependencyNode transformDepFn(outPortalTransformPath.node());
            addPortalAuthoringAttrs(transformDepFn, pd);

            MFnDagNode parentFn(parentObj);
            MString parentCmd = "parent \"";
            parentCmd += outPortalTransformPath.fullPathName();
            parentCmd += "\" \"";
            parentCmd += parentFn.fullPathName();
            parentCmd += "\"";
            MGlobal::executeCommand(parentCmd);
            return attachDoorHardpoint(outPortalTransformPath.node(), pd);
        }

        MFnTransform portalFn;
        MObject portalObj = portalFn.create(parentObj, &st);
        if (!st) return st;
        portalFn.setName(MString(portalName));
        portalFn.getPath(outPortalTransformPath);
        MFnDependencyNode depFn(portalObj);
        addPortalAuthoringAttrs(depFn, pd);
        return attachDoorHardpoint(portalObj, pd);
    }
}

void* ImportPob::creator()
{
    return new ImportPob();
}

MStatus ImportPob::doIt(const MArgList& args)
{
    MStatus status;
    std::string filename;

    const unsigned argCount = args.length(&status);
    if (!status) return MS::kFailure;

    for (unsigned i = 0; i < argCount; ++i)
    {
        MString arg = args.asString(i, &status);
        if (!status) return MS::kFailure;
        if (arg == "-i" && (i + 1) < argCount)
        {
            filename = args.asString(i + 1, &status).asChar();
            ++i;
        }
    }

    if (filename.empty())
    {
        std::cerr << "ImportPob: no filename specified, use -i <filename>" << std::endl;
        return MS::kFailure;
    }

    pobLog("Starting import: %s", filename.c_str());
    filename = resolveImportPath(filename);
    pobLog("Resolved path: %s", filename.c_str());

    Iff iff;
    if (!iff.open(filename.c_str(), false))
    {
        pobLog("FAILED to open file: %s", filename.c_str());
        std::cerr << "ImportPob: failed to open " << filename << std::endl;
        return MS::kFailure;
    }

    pobLog("File opened successfully");

    if (iff.getCurrentName() != TAG_PRTO)
    {
        std::cerr << "ImportPob: expected FORM PRTO" << std::endl;
        return MS::kFailure;
    }

    iff.enterForm(TAG_PRTO);

    Tag versionTag = iff.getCurrentName();
    int version = -1;
    if (versionTag == TAG_0000) version = 0;
    else if (versionTag == TAG_0001) version = 1;
    else if (versionTag == TAG_0002) version = 2;
    else if (versionTag == TAG_0003) version = 3;
    else if (versionTag == TAG_0004) version = 4;

    if (version < 0)
    {
        pobLog("FAILED: unsupported PRTO version");
        std::cerr << "ImportPob: unsupported PRTO version" << std::endl;
        return MS::kFailure;
    }

    pobLog("PRTO version: %d", version);
    iff.enterForm(versionTag);

    int numberOfPortals = 0, numberOfCells = 0;
    iff.enterChunk(TAG_DATA);
    numberOfPortals = iff.read_int32();
    numberOfCells = iff.read_int32();
    iff.exitChunk(TAG_DATA);

    pobLog("PRTO v%d: %d portals, %d cells", version, numberOfPortals, numberOfCells);

    std::vector<PortalGeometry> portalGeometries(static_cast<size_t>(numberOfPortals));
    iff.enterForm(TAG_PRTS);
    for (int i = 0; i < numberOfPortals && iff.getNumberOfBlocksLeft() > 0; ++i)
    {
        if (version >= 4 && iff.isCurrentForm() && iff.getCurrentName() == TAG_IDTL)
        {
            portalGeometries[static_cast<size_t>(i)].loadFromIdtl(iff);
        }
        else if (version < 4)
        {
            iff.enterChunk(TAG_PRTL);
            portalGeometries[static_cast<size_t>(i)].loadFromPrtl(iff);
            iff.exitChunk(TAG_PRTL);
        }
    }
    iff.exitForm(TAG_PRTS);

    std::string rootName = filename;
    const auto lastSlash = rootName.find_last_of("/\\");
    if (lastSlash != std::string::npos) rootName = rootName.substr(lastSlash + 1);
    const auto dotPos = rootName.find_last_of('.');
    if (dotPos != std::string::npos) rootName = rootName.substr(0, dotPos);

    MFnTransform rootFn;
    MObject rootObj = rootFn.create(MObject::kNullObj, &status);
    if (!status)
    {
        std::cerr << "ImportPob: failed to create root" << std::endl;
        return MS::kFailure;
    }
    rootFn.setName(MString(rootName.c_str()));
    pobLog("Created root: %s", rootName.c_str());

    std::vector<MObject> cellTransforms(static_cast<size_t>(numberOfCells));
    for (int i = 0; i < numberOfCells; ++i)
    {
        char cellName[32];
        sprintf(cellName, "r%d", i);
        MFnTransform cellFn;
        cellTransforms[static_cast<size_t>(i)] = cellFn.create(rootObj, &status);
        if (!status) return MS::kFailure;
        cellFn.setName(MString(cellName));
    }

    iff.enterForm(TAG_CELS);
    for (int i = 0; i < numberOfCells; ++i)
    {
        iff.enterForm(TAG_CELL);
        Tag cellVersionTag = iff.getCurrentName();
        int cellVersion = (cellVersionTag == TAG_0001) ? 1 : (cellVersionTag == TAG_0002) ? 2 :
            (cellVersionTag == TAG_0003) ? 3 : (cellVersionTag == TAG_0004) ? 4 : (cellVersionTag == TAG_0005) ? 5 : 0;

        iff.enterForm(cellVersionTag);

        std::string cellName, appearanceName, floorName;
        int32 cellPortalCount = 0;

        iff.enterChunk(TAG_DATA);
        cellPortalCount = iff.read_int32();
        iff.read_bool8();
        if (cellVersion >= 5)
            cellName = iff.read_stdstring();
        else
            { char buf[32]; sprintf(buf, "cell_%d", i); cellName = buf; }
        appearanceName = iff.read_stdstring();
        if (cellVersion >= 2)
        {
            if (iff.read_bool8())
                floorName = iff.read_stdstring();
        }
        iff.exitChunk(TAG_DATA);

        if (cellVersion >= 5 && iff.getNumberOfBlocksLeft() > 0 && iff.isCurrentForm())
        {
            iff.enterForm();
            iff.exitForm();
        }

        std::vector<PortalData> cellPortalData;
        for (int32 p = 0; p < cellPortalCount && iff.getNumberOfBlocksLeft() > 0; ++p)
        {
            if (iff.isCurrentForm() && iff.getCurrentName() == TAG_PRTL)
            {
                iff.enterForm(TAG_PRTL);
                PortalData pd = readPortalDataFromPrtl(iff);
                iff.exitForm(TAG_PRTL);
                if (pd.portalIndex >= 0 && pd.portalIndex < numberOfPortals)
                    cellPortalData.push_back(pd);
            }
            else if (iff.isCurrentForm())
            {
                iff.enterForm();
                iff.exitForm();
            }
        }

        if (cellVersion >= 3 && iff.getNumberOfBlocksLeft() > 0 && !iff.isCurrentForm())
        {
            iff.enterChunk();
            iff.exitChunk();
        }

        iff.exitForm(cellVersionTag);
        iff.exitForm(TAG_CELL);

        if (floorName.empty() && !appearanceName.empty())
            floorName = deriveFloorFromAppearance(appearanceName);

        pobLog("Cell r%d: appearance=%s floor=%s portals=%zu", i, appearanceName.c_str(), floorName.c_str(), cellPortalData.size());

        if (!appearanceName.empty())
        {
            std::string resolvedPath = resolveTreeFilePath(appearanceName, filename);
            resolvedPath = resolveLodOrAptPath(resolvedPath);
            MObject cellObj = cellTransforms[static_cast<size_t>(i)];
            MFnTransform cellFn(cellObj);
            MString meshCmd = "createNode transform -n \"mesh\" -p \"" + cellFn.fullPathName() + "\"";
            MStringArray meshResult;
            status = MGlobal::executeCommand(meshCmd, meshResult);
            MObject meshTransformObj;
            std::string parentCellPath = std::string(cellFn.fullPathName().asChar()) + "|mesh";
            if (status && meshResult.length() > 0)
            {
                MSelectionList sel;
                if (sel.add(meshResult[0]) == MS::kSuccess)
                {
                    sel.getDependNode(0, meshTransformObj);
                    parentCellPath = meshResult[0].asChar();
                }
            }

            pobLog("  Loading mesh: %s -> %s", appearanceName.c_str(), resolvedPath.c_str());
            MString cmd = "importLodMesh -i \"";
            cmd += resolvedPath.c_str();
            cmd += "\" -parent \"";
            cmd += parentCellPath.c_str();
            cmd += "\"";
            status = MGlobal::executeCommand(cmd, true, true);
            pobLog("  importLodMesh %s", status ? "OK" : "FAILED");
            if (status && !meshTransformObj.isNull())
            {
                MFnDependencyNode meshDepFn(meshTransformObj);
                MPlug plug = meshDepFn.findPlug("external_reference", true);
                if (plug.isNull())
                {
                    MFnTypedAttribute tAttr;
                    MObject attrObj = tAttr.create("external_reference", "extref", MFnData::kString);
                    tAttr.setStorable(true);
                    if (meshDepFn.addAttribute(attrObj))
                        plug = meshDepFn.findPlug("external_reference", true);
                }
                if (!plug.isNull())
                    plug.setValue(MString(appearanceName.c_str()));
            }
        }

        if (!cellPortalData.empty())
        {
            pobLog("  Creating %zu portal(s)", cellPortalData.size());
            MObject cellObj = cellTransforms[static_cast<size_t>(i)];
            MFnTransform cellFn(cellObj);
            MString cmd = "createNode transform -n \"portals\" -p \"" + cellFn.fullPathName() + "\"";
            MStringArray result;
            MGlobal::executeCommand(cmd, result);
            MObject portalsObj;
            if (result.length() > 0)
            {
                MSelectionList sel;
                if (sel.add(result[0]) == MS::kSuccess)
                    sel.getDependNode(0, portalsObj);
            }
            if (!portalsObj.isNull())
            {
                for (size_t p = 0; p < cellPortalData.size(); ++p)
                {
                    int geomIdx = cellPortalData[p].portalIndex;
                    const PortalGeometry* pg = nullptr;
                    if (geomIdx >= 0 && geomIdx < numberOfPortals)
                        pg = &portalGeometries[static_cast<size_t>(geomIdx)];
                    char portalName[32];
                    sprintf(portalName, "p%zu", p);
                    MDagPath portalXformPath;
                    status = createPortalRepresentation(pg, portalName, portalsObj, cellPortalData[p], portalXformPath);
                    if (!status)
                        pobLog("  Portal p%zu: createPortalRepresentation failed", p);
                }
            }
        }

        if (!floorName.empty())
        {
            std::string resolvedFloor = resolveTreeFilePath(floorName, filename);
            if (resolvedFloor.size() < 4 ||
                (resolvedFloor.compare(resolvedFloor.size() - 4, 4, ".flr") != 0 &&
                 resolvedFloor.compare(resolvedFloor.size() - 4, 4, ".FLR") != 0))
                resolvedFloor += ".flr";

            struct stat st = {};
            if (stat(resolvedFloor.c_str(), &st) != 0)
            {
                pobLog("  Floor file not found, skipping: %s", resolvedFloor.c_str());
            }
            else
            {
                pobLog("  Loading floor: %s", floorName.c_str());
                MObject cellObj = cellTransforms[static_cast<size_t>(i)];
                MFnTransform cellFn(cellObj);
                MString cmd = "createNode transform -n \"collision\" -p \"" + cellFn.fullPathName() + "\"";
                MStringArray result;
                MGlobal::executeCommand(cmd, result);
                MObject collisionObj;
                MSelectionList collisionSel;
                if (result.length() > 0)
                {
                    if (collisionSel.add(result[0]) == MS::kSuccess)
                        collisionSel.getDependNode(0, collisionObj);
                }
                if (!collisionObj.isNull())
                {
                    MDagPath floorMeshPath;
                    MStatus flrStatus = FlrTranslator::createMeshFromFlr(resolvedFloor.c_str(), "floor0", collisionObj, floorMeshPath);
                    if (flrStatus)
                    {
                        pobLog("    Floor mesh loaded: %s", resolvedFloor.c_str());
                        MDagPath floorTransformPath = floorMeshPath;
                        if (floorTransformPath.hasFn(MFn::kMesh)) floorTransformPath.pop(1);
                        MFnDependencyNode floorDepFn(floorTransformPath.node());
                        MPlug plug = floorDepFn.findPlug("external_reference", true);
                        if (plug.isNull())
                        {
                            MFnTypedAttribute tAttr;
                            MObject attrObj = tAttr.create("external_reference", "extref", MFnData::kString);
                            tAttr.setStorable(true);
                            if (floorDepFn.addAttribute(attrObj))
                                plug = floorDepFn.findPlug("external_reference", true);
                        }
                        if (!plug.isNull())
                            plug.setValue(MString(floorName.c_str()));
                    }
                    else
                    {
                        pobLog("    Floor load failed, using fallback plane: %s", resolvedFloor.c_str());
                        MFnDagNode collisionDagFn(collisionObj);
                        std::string collisionPath = collisionDagFn.fullPathName().asChar();
                        MGlobal::executeCommand("polyPlane -w 1 -h 1 -sx 1 -sy 1 -n \"floor0\"");
                        collisionSel.clear();
                        MGlobal::getActiveSelectionList(collisionSel);
                        if (collisionSel.length() > 0)
                        {
                            MDagPath floorPath;
                            collisionSel.getDagPath(0, floorPath);
                            if (!floorPath.hasFn(MFn::kTransform)) floorPath.pop();
                            MDagPath fallbackShapePath = floorPath;
                            fallbackShapePath.extendToShape();
                            MFnDependencyNode fallbackDepFn(fallbackShapePath.node());
                            MPlug fallbackPlug = fallbackDepFn.findPlug("external_reference", true);
                            if (fallbackPlug.isNull())
                            {
                                MFnTypedAttribute tAttr;
                                MObject attrObj = tAttr.create("external_reference", "extref", MFnData::kString);
                                tAttr.setStorable(true);
                                if (fallbackDepFn.addAttribute(attrObj))
                                    fallbackPlug = fallbackDepFn.findPlug("external_reference", true);
                            }
                            if (!fallbackPlug.isNull())
                                fallbackPlug.setValue(MString(floorName.c_str()));
                            MString parentCmd = "parent \"" + floorPath.fullPathName() + "\" \"" + collisionPath.c_str() + "\"";
                            MGlobal::executeCommand(parentCmd);
                        }
                    }
                }
            }
        }
    }
    iff.exitForm(TAG_CELS);

    if (iff.getNumberOfBlocksLeft() > 0 && iff.isCurrentForm() && iff.getCurrentName() == TAG_PGRF)
    {
        iff.enterForm(TAG_PGRF);
        iff.exitForm(TAG_PGRF);
    }
    if (iff.getNumberOfBlocksLeft() > 0 && !iff.isCurrentForm() && iff.getCurrentName() == TAG_CRC)
    {
        iff.enterChunk(TAG_CRC);
        iff.exitChunk(TAG_CRC);
    }

    iff.exitForm(versionTag);
    iff.exitForm(TAG_PRTO);

    pobLog("Import complete: %s", rootName.c_str());
    return MS::kSuccess;
}
