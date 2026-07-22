#include "ImportPob.h"
#include "ImportPathResolver.h"
#include "ImportLodMesh.h"
#include "MayaUtility.h"
#include "MayaSceneBuilder.h"
#include "PobAuthoringShared.h"
#include "SwgImportTrace.h"
#include "flr.h"

#include "Iff.h"
#include "Tag.h"
#include "Transform.h"
#include "Vector.h"
#include "VectorArgb.h"

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
#include <maya/MStringArray.h>
#include <maya/MColor.h>
#include <maya/MFnAmbientLight.h>
#include <maya/MFnDirectionalLight.h>
#include <maya/MFnLight.h>
#include <maya/MFnPointLight.h>
#include <maya/MFnMesh.h>
#include <maya/MFloatPointArray.h>
#include <maya/MIntArray.h>
#include <maya/MDagModifier.h>

#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>

#ifdef _WIN32
#include <windows.h>
#endif

namespace
{
    static void pobLog(const char* fmt, ...)
    {
        va_list args;
        va_start(args, fmt);
        SwgImportTrace::logV("ImportPob", fmt, args);
        va_end(args);
    }

    /** Suspend viewport refresh during bulk import; restore on scope exit. */
    struct ImportRefreshGuard
    {
        ImportRefreshGuard()
        {
            (void)MGlobal::executeCommand("refresh -suspend true", false, false);
        }
        ~ImportRefreshGuard()
        {
            SwgImportTrace::stage("import refresh unsuspend begin");
            (void)MGlobal::executeCommand("refresh -suspend false", false, false);
            SwgImportTrace::stage("import refresh unsuspend OK");
        }
    };

    static MStatus createNamedChildTransform(MObject parentObj, const char* name, MObject& outTransform)
    {
        MStatus st;
        MFnTransform fn;
        outTransform = fn.create(parentObj, &st);
        if (!st)
            return st;
        MStatus nameSt;
        (void)fn.setName(MString(name), &nameSt);
        return nameSt;
    }

    static MStatus reparentTransform(MObject childTransform, MObject newParent)
    {
        MDagModifier mod;
        MStatus st = mod.reparentNode(childTransform, newParent);
        if (!st)
            return st;
        return mod.doIt();
    }

    static void ensureBoolAttr(MFnDependencyNode& dep, const char* name, bool value)
    {
        MPlug p = dep.findPlug(name, true);
        if (p.isNull())
        {
            MFnNumericAttribute nAttr;
            MObject a = nAttr.create(name, name, MFnNumericData::kBoolean);
            nAttr.setStorable(true);
            if (dep.addAttribute(a))
                p = dep.findPlug(name, true);
        }
        if (!p.isNull())
            p.setBool(value);
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
    static const Tag TAG_LGHT = TAG(L,G,H,T);

    /// Matches `PortalPropertyTemplateCellLight::Type` / LGHT chunk int8.
    enum PobCellLightType
    {
        PobLight_ambient = 0,
        PobLight_parallel = 1,
        PobLight_point = 2
    };

    struct PobCellLight
    {
        int type = PobLight_ambient;
        VectorArgb diffuse;
        VectorArgb specular;
        Transform transform = Transform::identity;
        float constantAttenuation = 1.f;
        float linearAttenuation = 0.f;
        float quadraticAttenuation = 0.f;
    };

    static void skipOneIffBlock(Iff& iff);

    struct PortalGeometry
    {
        std::vector<Vector> vertices;
        std::vector<int> indices;

        void loadFromPrtl(Iff& iff)
        {
            const int numVerts = iff.read_int32();
            if (numVerts < 0 || numVerts > 65536)
            {
                pobLog("PRTL: invalid vertex count %d", numVerts);
                return;
            }
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
            const Tag idtlVersion = iff.getCurrentName();
            if (idtlVersion != TAG_0000)
            {
                pobLog("IDTL: unsupported version, skipping");
                skipOneIffBlock(iff);
                iff.exitForm(TAG_IDTL);
                return;
            }
            iff.enterForm(TAG_0000);
            iff.enterChunk(TAG_VERT);
            const int numVerts = iff.getChunkLengthLeft(12) / 12;
            if (numVerts < 0 || numVerts > 65536)
            {
                pobLog("IDTL: invalid vertex count %d", numVerts);
                iff.exitChunk(TAG_VERT);
                iff.exitForm(TAG_0000);
                iff.exitForm(TAG_IDTL);
                return;
            }
            vertices.resize(static_cast<size_t>(numVerts));
            if (numVerts > 0)
                iff.read_floatVector(numVerts, vertices.data());
            iff.exitChunk(TAG_VERT);
            iff.enterChunk(TAG_INDX);
            const int numIndices = iff.getChunkLengthLeft(4) / 4;
            if (numIndices < 0 || numIndices > 196608)
            {
                pobLog("IDTL: invalid index count %d", numIndices);
                iff.exitChunk(TAG_INDX);
                iff.exitForm(TAG_0000);
                iff.exitForm(TAG_IDTL);
                return;
            }
            indices.resize(static_cast<size_t>(numIndices));
            for (int j = 0; j < numIndices; ++j)
                indices[static_cast<size_t>(j)] = iff.read_int32();
            iff.exitChunk(TAG_INDX);
            iff.exitForm(TAG_0000);
            iff.exitForm(TAG_IDTL);
        }
    };

    static PortalGeometry makeFallbackPortalQuad()
    {
        // Matches `PobAuthoring::createPortalDoorMeshAndParent` default quad (1m x 2m, local XY plane).
        const float hw = 0.5f;
        const float height = 2.0f;
        PortalGeometry g;
        g.vertices.resize(4);
        g.vertices[0] = Vector(hw, 0.f, 0.f);
        g.vertices[1] = Vector(-hw, 0.f, 0.f);
        g.vertices[2] = Vector(-hw, height, 0.f);
        g.vertices[3] = Vector(hw, height, 0.f);
        g.indices.push_back(0);
        g.indices.push_back(1);
        g.indices.push_back(2);
        g.indices.push_back(0);
        g.indices.push_back(2);
        g.indices.push_back(3);
        return g;
    }

    static void skipOneIffBlock(Iff& iff)
    {
        if (iff.getNumberOfBlocksLeft() <= 0)
            return;
        if (iff.isCurrentForm())
        {
            iff.enterForm();
            while (iff.getNumberOfBlocksLeft() > 0)
                skipOneIffBlock(iff);
            iff.exitForm();
        }
        else
        {
            iff.enterChunk();
            iff.exitChunk();
        }
    }

    static std::string resolveTreeFilePath(const std::string& treeFilePath, const std::string& inputFilename)
    {
        if (treeFilePath.empty())
            return std::string();

        std::string normalized = treeFilePath;
        for (auto& c : normalized)
            if (c == '\\')
                c = '/';
        if (normalized.size() >= 2 && normalized[1] == ':')
            return normalized;

        std::string probe = normalized;
        if (probe.find("appearance/") != 0 && probe.find("shader/") != 0 &&
            probe.find("texture/") != 0 && probe.find("effect/") != 0)
            probe = "appearance/" + probe;

        if (!inputFilename.empty())
        {
            std::string normIn = inputFilename;
            for (auto& c : normIn)
                if (c == '\\')
                    c = '/';
            const auto cgPos = normIn.find("compiled/game/");
            if (cgPos != std::string::npos)
            {
                const std::string baseDir = normIn.substr(0, cgPos + 14);
                const std::string resolved = baseDir + probe;
                if (MayaUtility::fileExists(resolved))
                    return resolved;
                std::string resolvedBs = resolved;
                for (char& c : resolvedBs)
                    if (c == '/')
                        c = '\\';
                if (resolvedBs != resolved && MayaUtility::fileExists(resolvedBs))
                    return resolvedBs;
            }
        }

        const std::string fromGame = resolveGameAssetPath(probe);
        if (!fromGame.empty())
            return fromGame;

        std::string baseDir;
        const char* envExportRoot = getenv("TITAN_EXPORT_ROOT");
        if (envExportRoot && envExportRoot[0])
        {
            baseDir = envExportRoot;
            if (!baseDir.empty() && baseDir.back() != '/')
                baseDir += '/';
        }
        else
        {
            const char* envDataRoot = getenv("TITAN_DATA_ROOT");
            if (envDataRoot && envDataRoot[0])
            {
                baseDir = envDataRoot;
                if (!baseDir.empty() && baseDir.back() != '/')
                    baseDir += '/';
            }
            else
            {
                std::string norm = inputFilename;
                for (auto& c : norm)
                    if (c == '\\')
                        c = '/';
                const auto cgPos = norm.find("compiled/game/");
                if (cgPos != std::string::npos)
                    baseDir = norm.substr(0, cgPos + 14);
            }
        }
        if (baseDir.empty())
            return normalized;

        std::string path = normalized;
        if (path.find("appearance/") != 0 && path.find("appearance\\") != 0)
            path = "appearance/" + path;
        std::string resolved = baseDir + path;
        for (auto& c : resolved)
            if (c == '\\')
                c = '/';
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

    /// Creates portal transform under `parentObj`: mesh (real IDTL/PRTL or fallback quad) plus POB authoring attrs and door hardpoint **data** (`doorHardpointEnabled` + matrix).
    static MStatus createPortalRepresentation(
        const PortalGeometry* geom,
        const char* portalName,
        MObject parentObj,
        const PortalData& pd,
        MDagPath& outPortalTransformPath)
    {
        PortalGeometry fallback;
        const PortalGeometry* useGeom = geom;
        if (!useGeom || useGeom->vertices.empty() || useGeom->indices.size() < 3)
        {
            fallback = makeFallbackPortalQuad();
            useGeom = &fallback;
        }

        MStatus st;
        std::vector<float> positions;
        positions.reserve(useGeom->vertices.size() * 3);
        for (size_t i = 0; i < useGeom->vertices.size(); ++i)
        {
            positions.push_back(useGeom->vertices[i].x);
            positions.push_back(useGeom->vertices[i].y);
            positions.push_back(useGeom->vertices[i].z);
        }
        std::vector<float> normals(positions.size(), 0.0f);

        MayaSceneBuilder::ShaderGroupData sg;
        sg.shaderTemplateName = "shader/placeholder";
        for (size_t t = 0; t + 2 < useGeom->indices.size(); t += 3)
        {
            MayaSceneBuilder::TriangleData tri;
            tri.indices[0] = useGeom->indices[t];
            tri.indices[1] = useGeom->indices[t + 1];
            tri.indices[2] = useGeom->indices[t + 2];
            sg.triangles.push_back(tri);
        }

        std::vector<MayaSceneBuilder::ShaderGroupData> groups(1, sg);
        MDagPath meshShapePath;
        st = MayaSceneBuilder::createMesh(positions, normals, groups, portalName, meshShapePath);
        if (!st) return st;

        st = MayaSceneBuilder::assignPobCollisionPreviewMaterial(meshShapePath);
        if (!st)
            pobLog("  assignPobCollisionPreviewMaterial failed for portal mesh");

        MFnDependencyNode shapeDepFn(meshShapePath.node());
        ensureBoolAttr(shapeDepFn, "portal", true);

        meshShapePath.pop(1);
        outPortalTransformPath = meshShapePath;
        MFnDependencyNode transformDepFn(outPortalTransformPath.node());
        addPortalAuthoringAttrs(transformDepFn, pd);

        st = reparentTransform(outPortalTransformPath.node(), parentObj);
        if (!st)
            return st;
        PobAuthoring::applyDoorHardpointAttributes(outPortalTransformPath.node(), pd.hasDoorHardpoint, pd.doorTransform);
        return MS::kSuccess;
    }

    static void readCellLightsChunk(Iff& iff, std::vector<PobCellLight>& out)
    {
        const int n = iff.read_int32();
        out.clear();
        if (n <= 0)
            return;
        out.reserve(static_cast<size_t>(n));
        for (int j = 0; j < n; ++j)
        {
            PobCellLight L;
            L.type = static_cast<int>(iff.read_int8());
            L.diffuse = iff.read_floatVectorArgb();
            L.specular = iff.read_floatVectorArgb();
            L.transform = iff.read_floatTransform();
            L.constantAttenuation = iff.read_float();
            L.linearAttenuation = iff.read_float();
            L.quadraticAttenuation = iff.read_float();
            out.push_back(L);
        }
    }

    static void trySetFloatPlug(MObject node, const char* plugName, float v)
    {
        MFnDependencyNode dep(node);
        MPlug p = dep.findPlug(plugName, true);
        if (!p.isNull())
            p.setFloat(v);
    }

    static void trySetBoolPlug(MObject node, const char* plugName, bool v)
    {
        MFnDependencyNode dep(node);
        MPlug p = dep.findPlug(plugName, true);
        if (!p.isNull())
            p.setBool(v);
    }

    /// Maya 2026+ removed `MFnLight::setSpecularColor`; drive the `specularColor` compound (or R/G/B plugs) instead.
    static void trySetSpecularRgb(MObject lightShape, float r, float g, float b)
    {
        MFnDependencyNode dep(lightShape);
        MPlug sc = dep.findPlug("specularColor", true);
        if (!sc.isNull() && sc.isCompound() && sc.numChildren() >= 3)
        {
            sc.child(0).setFloat(r);
            sc.child(1).setFloat(g);
            sc.child(2).setFloat(b);
            return;
        }
        trySetFloatPlug(lightShape, "specularColorR", r);
        trySetFloatPlug(lightShape, "specularColorG", g);
        trySetFloatPlug(lightShape, "specularColorB", b);
    }

    static void applyPobLightColors(MFnLight& lightFn, const PobCellLight& L)
    {
        lightFn.setColor(MColor(L.diffuse.r, L.diffuse.g, L.diffuse.b));
        lightFn.setIntensity(1.0f);
        if (L.type != PobLight_ambient)
        {
            trySetSpecularRgb(lightFn.object(), L.specular.r, L.specular.g, L.specular.b);
            trySetBoolPlug(lightFn.object(), "emitSpecular", true);
        }
    }

    /// Parents cell LGHT under `lights` group (like engine `PortalPropertyTemplateCell` LGHT). Does not apply engine `yaw_l(PI)` (that is a runtime load fix for legacy Maya exports).
    static MStatus createCellLightRepresentations(MObject cellObj, const std::vector<PobCellLight>& lights)
    {
        if (lights.empty())
            return MS::kSuccess;
        MStatus st;
        MFnTransform cellFn(cellObj);
        MObject lightsGroup;
        st = createNamedChildTransform(cellObj, "lights", lightsGroup);
        if (!st || lightsGroup.isNull())
            return MS::kFailure;

        for (size_t i = 0; i < lights.size(); ++i)
        {
            const PobCellLight& L = lights[i];
            char nameBuf[48];
            sprintf(nameBuf, "cellLight_%zu", i);

            if (L.type == PobLight_ambient)
            {
                MFnAmbientLight fn;
                MObject shape = fn.create(lightsGroup, true, false, &st);
                if (!st)
                    continue;
                applyPobLightColors(fn, L);
                MFnDagNode dag(shape);
                MDagPath path;
                dag.getPath(path);
                path.pop(1);
                MFnTransform(path).setName(MString(nameBuf));
            }
            else if (L.type == PobLight_parallel)
            {
                MFnDirectionalLight fn;
                MObject shape = fn.create(lightsGroup, true, false, &st);
                if (!st)
                    continue;
                applyPobLightColors(fn, L);
                MFnDagNode dag(shape);
                MDagPath path;
                dag.getPath(path);
                path.pop(1);
                MFnTransform tf(path);
                tf.setName(MString(nameBuf));
                MMatrix m;
                engineTransformToMayaMatrix(L.transform, m);
                tf.set(m);
            }
            else if (L.type == PobLight_point)
            {
                MFnPointLight fn;
                MObject shape = fn.create(lightsGroup, true, false, &st);
                if (!st)
                    continue;
                applyPobLightColors(fn, L);
                trySetFloatPlug(shape, "constantAttenuation", L.constantAttenuation);
                trySetFloatPlug(shape, "linearAttenuation", L.linearAttenuation);
                trySetFloatPlug(shape, "quadraticAttenuation", L.quadraticAttenuation);
                MFnDagNode dag(shape);
                MDagPath path;
                dag.getPath(path);
                path.pop(1);
                MFnTransform tf(path);
                tf.setName(MString(nameBuf));
                MMatrix m;
                engineTransformToMayaMatrix(L.transform, m);
                tf.set(m);
            }
            else
                pobLog("  Unknown cell light type %d, skipped", L.type);
        }
        return MS::kSuccess;
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

    SwgImportTrace::beginSession("importPob", filename.c_str());
    SwgImportTrace::stage("importPob begin");
    ImportRefreshGuard refreshGuard;

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
    SwgImportTrace::stage("PRTO header parsed");

    std::vector<PortalGeometry> portalGeometries(static_cast<size_t>(numberOfPortals));
    iff.enterForm(TAG_PRTS);
    for (int i = 0; i < numberOfPortals && iff.getNumberOfBlocksLeft() > 0; ++i)
    {
        SwgImportTrace::stagef("PRTS portal %d begin", i);
        if (version >= 4)
        {
            if (iff.isCurrentForm() && iff.getCurrentName() == TAG_IDTL)
                portalGeometries[static_cast<size_t>(i)].loadFromIdtl(iff);
            else
            {
                pobLog("PRTS portal %d: expected IDTL form in PRTO v%d, skipping block", i, version);
                skipOneIffBlock(iff);
            }
        }
        else if (!iff.isCurrentForm() && iff.getCurrentName() == TAG_PRTL)
        {
            iff.enterChunk(TAG_PRTL);
            portalGeometries[static_cast<size_t>(i)].loadFromPrtl(iff);
            iff.exitChunk(TAG_PRTL);
        }
        else
        {
            pobLog("PRTS portal %d: expected PRTL chunk in PRTO v%d, skipping block", i, version);
            skipOneIffBlock(iff);
        }
        SwgImportTrace::stagef("PRTS portal %d OK (%zu verts)", i, portalGeometries[static_cast<size_t>(i)].vertices.size());
    }
    iff.exitForm(TAG_PRTS);
    SwgImportTrace::stage("portal geometry parsed");

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
        SwgImportTrace::stagef("cell r%d parse begin", i);

        iff.enterForm(TAG_CELL);
        Tag cellVersionTag = iff.getCurrentName();
        const bool cellVersionKnown =
            cellVersionTag == TAG_0001 || cellVersionTag == TAG_0002 || cellVersionTag == TAG_0003 ||
            cellVersionTag == TAG_0004 || cellVersionTag == TAG_0005;
        if (!cellVersionKnown)
        {
            pobLog("Cell r%d: unsupported CELL version, skipping", i);
            skipOneIffBlock(iff);
            iff.exitForm(TAG_CELL);
            continue;
        }

        int cellVersion = (cellVersionTag == TAG_0001) ? 1 : (cellVersionTag == TAG_0002) ? 2 :
            (cellVersionTag == TAG_0003) ? 3 : (cellVersionTag == TAG_0004) ? 4 : 5;

        iff.enterForm(cellVersionTag);

        std::string cellName, appearanceName, floorName;
        int32 cellPortalCount = 0;

        iff.enterChunk(TAG_DATA);
        cellPortalCount = iff.read_int32();
        const bool pobCellCanSeeWorldInFile = iff.read_bool8() != 0;
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

        {
            MFnDependencyNode cellAuthoringFn(cellTransforms[static_cast<size_t>(i)]);
            auto ensureStrAttr = [&](const char* longName, const char* shortName, const std::string& val) {
                MPlug p = cellAuthoringFn.findPlug(longName, true);
                if (p.isNull())
                {
                    MFnTypedAttribute tAttr;
                    MObject attrObj = tAttr.create(longName, shortName, MFnData::kString);
                    tAttr.setStorable(true);
                    if (cellAuthoringFn.addAttribute(attrObj))
                        p = cellAuthoringFn.findPlug(longName, true);
                }
                if (!p.isNull())
                    p.setValue(MString(val.c_str()));
            };
            auto ensureBoolAttr = [&](const char* longName, const char* shortName, bool val) {
                MPlug p = cellAuthoringFn.findPlug(longName, true);
                if (p.isNull())
                {
                    MFnNumericAttribute nAttr;
                    MObject attrObj = nAttr.create(longName, shortName, MFnNumericData::kBoolean);
                    nAttr.setStorable(true);
                    if (cellAuthoringFn.addAttribute(attrObj))
                        p = cellAuthoringFn.findPlug(longName, true);
                }
                if (!p.isNull())
                    p.setBool(val);
            };
            ensureStrAttr("pobCellName", "pcnm", cellName);
            ensureBoolAttr("pobCellCanSeeWorld", "pccsw", pobCellCanSeeWorldInFile);
        }

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
                if (version == 2)
                    pd.passable = !pd.passable;
                if (pd.portalIndex >= 0 && pd.portalIndex < numberOfPortals)
                    cellPortalData.push_back(pd);
            }
            else if (iff.isCurrentForm())
            {
                iff.enterForm();
                iff.exitForm();
            }
            else
                skipOneIffBlock(iff);
        }

        std::vector<PobCellLight> cellLights;
        if (cellVersion >= 3 && iff.getNumberOfBlocksLeft() > 0 && !iff.isCurrentForm())
        {
            const Tag nextChunk = iff.getCurrentName();
            if (nextChunk == TAG_LGHT)
            {
                iff.enterChunk(TAG_LGHT);
                readCellLightsChunk(iff, cellLights);
                iff.exitChunk(TAG_LGHT);
            }
            else
            {
                iff.enterChunk(nextChunk);
                iff.exitChunk(nextChunk);
            }
        }

        iff.exitForm(cellVersionTag);
        iff.exitForm(TAG_CELL);

        if (floorName.empty() && !appearanceName.empty())
            floorName = deriveFloorFromAppearance(appearanceName);

        pobLog("Cell r%d: appearance=%s floor=%s portals=%zu", i, appearanceName.c_str(), floorName.c_str(), cellPortalData.size());

        MObject cellObj = cellTransforms[static_cast<size_t>(i)];
        MObject meshTransformObj;
        MObject portalsTransformObj;
        MObject collisionTransformObj;
        SwgImportTrace::stagef("cell r%d groups begin", i);
        if (!createNamedChildTransform(cellObj, "mesh", meshTransformObj))
            pobLog("  Failed to create mesh group");
        if (!createNamedChildTransform(cellObj, "portals", portalsTransformObj))
            pobLog("  Failed to create portals group");
        if (!createNamedChildTransform(cellObj, "collision", collisionTransformObj))
            pobLog("  Failed to create collision group");
        SwgImportTrace::stagef("cell r%d groups OK", i);

        if (!appearanceName.empty())
        {
            std::string resolvedPath = resolveTreeFilePath(appearanceName, filename);
            resolvedPath = resolveLodOrAptPath(resolvedPath);
            std::string parentCellPath;
            if (!meshTransformObj.isNull())
            {
                MFnDagNode meshDag(meshTransformObj);
                parentCellPath = meshDag.fullPathName().asChar();
            }
            else
            {
                MFnTransform cellFn(cellObj);
                parentCellPath = std::string(cellFn.fullPathName().asChar()) + "|mesh";
            }

            SwgImportTrace::stagef("cell r%d importLodMesh begin: %s", i, resolvedPath.c_str());
            pobLog("  Loading mesh: %s -> %s", appearanceName.c_str(), resolvedPath.c_str());
            status = importLodMeshFile(resolvedPath, parentCellPath);
            SwgImportTrace::stagef("cell r%d importLodMesh %s", i, status ? "OK" : "FAILED");
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
            if (portalsTransformObj.isNull())
            {
                pobLog("  Missing portals group");
            }
            else
            {
                for (size_t p = 0; p < cellPortalData.size(); ++p)
                {
                    int geomIdx = cellPortalData[p].portalIndex;
                    const PortalGeometry* pg = nullptr;
                    if (geomIdx >= 0 && geomIdx < numberOfPortals)
                        pg = &portalGeometries[static_cast<size_t>(geomIdx)];
                    char portalName[32];
                    sprintf(portalName, "p%zu", p);
                    SwgImportTrace::stagef("cell r%d portal p%zu begin", i, p);
                    MDagPath portalXformPath;
                    status = createPortalRepresentation(pg, portalName, portalsTransformObj, cellPortalData[p], portalXformPath);
                    SwgImportTrace::stagef("cell r%d portal p%zu %s", i, p, status ? "OK" : "FAILED");
                    if (!status)
                        pobLog("  Portal p%zu: createPortalRepresentation failed", p);
                }
            }
        }

        SwgImportTrace::stagef("cell r%d post-portals", i);

        if (!cellLights.empty())
        {
            pobLog("  Creating %zu cell light(s)", cellLights.size());
            status = createCellLightRepresentations(cellTransforms[static_cast<size_t>(i)], cellLights);
            if (!status)
                pobLog("  cell lights: createCellLightRepresentations failed");
        }

        if (!floorName.empty())
        {
            SwgImportTrace::stagef("cell r%d floor resolve begin", i);
            std::string resolvedFloor = resolveTreeFilePath(floorName, filename);
            if (resolvedFloor.size() < 4 ||
                (resolvedFloor.compare(resolvedFloor.size() - 4, 4, ".flr") != 0 &&
                 resolvedFloor.compare(resolvedFloor.size() - 4, 4, ".FLR") != 0))
                resolvedFloor += ".flr";

            SwgImportTrace::stagef("cell r%d floor resolved: %s", i, resolvedFloor.c_str());

            if (!MayaUtility::fileExists(resolvedFloor))
            {
                pobLog("  Floor file not found, skipping: %s", resolvedFloor.c_str());
            }
            else
            {
                SwgImportTrace::stagef("cell r%d floor load begin: %s", i, resolvedFloor.c_str());
                pobLog("  Loading floor: %s", floorName.c_str());
                if (collisionTransformObj.isNull())
                {
                    pobLog("  Missing collision group");
                }
                else
                {
                    MDagPath floorMeshPath;
                    MStatus flrStatus = FlrTranslator::createMeshFromFlr(resolvedFloor.c_str(), "floor0", collisionTransformObj, floorMeshPath);
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
                        MFloatPointArray verts;
                        verts.append(MFloatPoint(0.5f, 0.0f, 0.5f));
                        verts.append(MFloatPoint(-0.5f, 0.0f, 0.5f));
                        verts.append(MFloatPoint(-0.5f, 0.0f, -0.5f));
                        verts.append(MFloatPoint(0.5f, 0.0f, -0.5f));
                        MIntArray counts;
                        counts.append(4);
                        MIntArray connects;
                        connects.append(0);
                        connects.append(1);
                        connects.append(2);
                        connects.append(3);
                        MFnMesh meshFn;
                        MObject floorShape = meshFn.create(4, 1, verts, counts, connects, collisionTransformObj, &status);
                        if (status && !floorShape.isNull())
                        {
                            meshFn.setName(MString("floor0"));
                            MDagPath floorPath;
                            MFnDagNode(floorShape).getPath(floorPath);
                            if (floorPath.hasFn(MFn::kMesh)) floorPath.pop(1);
                            MFnDependencyNode fallbackDepFn(floorPath.node());
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
                        }
                    }
                }
            }
            SwgImportTrace::stagef("cell r%d floor done", i);
        }

        SwgImportTrace::stagef("cell r%d complete", i);
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
    SwgImportTrace::stage("importPob complete");
    SwgImportTrace::stage("importPob returning");
    return MS::kSuccess;
}
