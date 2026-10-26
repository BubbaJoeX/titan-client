#include "ExportPob.h"
#include "ImportPathResolver.h"
#include "PobAuthoringShared.h"

#include "Iff.h"
#include "Tag.h"
#include "Transform.h"
#include "Vector.h"
#include "VectorArgb.h"

#include <maya/MArgList.h>
#include <maya/MDagPath.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFloatPointArray.h>
#include <maya/MFnMesh.h>
#include <maya/MFnTransform.h>
#include <maya/MMatrix.h>
#include <maya/MGlobal.h>
#include <maya/MItDag.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MPoint.h>
#include <maya/MSelectionList.h>
#include <maya/MFnLight.h>
#include <maya/MFnMatrixData.h>

#include <algorithm>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>
#include <cstdio>

namespace
{
    static const Tag TAG_PRTO = TAG(P,R,T,O);
    static const Tag TAG_PRTS = TAG(P,R,T,S);
    static const Tag TAG_PRTL = TAG(P,R,T,L);
    static const Tag TAG_IDTL = TAG(I,D,T,L);
    static const Tag TAG_VERT = TAG(V,E,R,T);
    static const Tag TAG_INDX = TAG(I,N,D,X);
    static const Tag TAG_CELS = TAG(C,E,L,S);
    static const Tag TAG_CELL = TAG(C,E,L,L);
    static const Tag TAG_CRC = TAG3(C,R,C);
    static const Tag TAG_NULL = TAG(N,U,L,L);
    static const Tag TAG_LGHT = TAG(L,G,H,T);
    static const Tag TAG_CMSH = TAG(C,M,S,H);

    struct PortalGeometry
    {
        std::vector<Vector> vertices;
        std::vector<int> indices;
    };

    struct PortalInfo
    {
        int portalIndex;
        bool clockwise;
        int targetCell;
        bool disabled;
        bool passable;
        std::string doorStyle;
        bool hasDoorHardpoint = false;
        Transform doorTransform = Transform::identity;
    };

    struct PobCellLightOut
    {
        int type = 0;
        VectorArgb diffuse;
        VectorArgb specular;
        Transform transform = Transform::identity;
        float constantAttenuation = 1.f;
        float linearAttenuation = 0.f;
        float quadraticAttenuation = 0.f;
    };

    struct CellData
    {
        std::string name;
        std::string appearanceName;
        std::string floorName;
        bool canSeeWorldCell = false;
        std::vector<byte> collisionExtent;  // raw IFF block (ExtentList) carried over from importPob
        PortalGeometry collisionMesh;       // authored rN|collision|extent meshes -> CMSH
        std::vector<PortalInfo> portals;
        std::vector<PobCellLightOut> lights;
    };

    static bool getStringAttr(MObject node, const char* attrName, std::string& out)
    {
        MFnDependencyNode fn(node);
        MPlug plug = fn.findPlug(attrName, true);
        if (plug.isNull()) return false;
        MString val;
        if (plug.getValue(val) != MS::kSuccess) return false;
        out = val.asChar();
        return true;
    }

    static bool getIntAttr(MObject node, const char* attrName, int& out)
    {
        MFnDependencyNode fn(node);
        MPlug plug = fn.findPlug(attrName, true);
        if (plug.isNull()) return false;
        return plug.getValue(out) == MS::kSuccess;
    }

    static bool getBoolAttr(MObject node, const char* attrName, bool& out)
    {
        MFnDependencyNode fn(node);
        MPlug plug = fn.findPlug(attrName, true);
        if (plug.isNull()) return false;
        bool val;
        if (plug.getValue(val) != MS::kSuccess) return false;
        out = val;
        return true;
    }

    static bool hasPortalAttr(MDagPath meshPath)
    {
        if (meshPath.hasFn(MFn::kMesh))
            meshPath.extendToShape();
        MFnDependencyNode fn(meshPath.node());
        return !fn.findPlug("portal", true).isNull();
    }

    /// Inverse of PobAuthoring::applyDoorHardpointAttributes: the doorHardpointMatrix attr holds the engine 3x4 verbatim.
    static Transform mayaMatrixToEngineTransform(const MMatrix& m)
    {
        Transform t = Transform::identity;
        const Vector i(static_cast<float>(m[0][0]), static_cast<float>(m[1][0]), static_cast<float>(m[2][0]));
        const Vector j(static_cast<float>(m[0][1]), static_cast<float>(m[1][1]), static_cast<float>(m[2][1]));
        const Vector k(static_cast<float>(m[0][2]), static_cast<float>(m[1][2]), static_cast<float>(m[2][2]));
        const Vector p(static_cast<float>(m[0][3]), static_cast<float>(m[1][3]), static_cast<float>(m[2][3]));
        t.setLocalFrameIJK_p(i, j, k);
        t.setPosition_p(p);
        return t;
    }

    static bool getFloatAttr(MObject node, const char* attrName, float& out)
    {
        MFnDependencyNode fn(node);
        MPlug plug = fn.findPlug(attrName, true);
        if (plug.isNull()) return false;
        return plug.getValue(out) == MS::kSuccess;
    }

    static void readDoorHardpointFromLegacyChild(MObject portalTransformObj, PortalInfo& pi)
    {
        MFnDagNode pfn(portalTransformObj);
        for (unsigned c = 0; c < pfn.childCount(); ++c)
        {
            MObject ch = pfn.child(c);
            if (!ch.hasFn(MFn::kTransform)) continue;
            MFnDagNode cfn(ch);
            if (cfn.name() != "doorHardpoint") continue;
            pi.hasDoorHardpoint = true;
            MFnTransform tfn(ch);
            pi.doorTransform = PobAuthoring::mayaSceneMatrixToEngineTransform(tfn.transformationMatrix());
            return;
        }
    }

    /// Prefers `doorHardpointEnabled` + `doorHardpointMatrix` (data only). Falls back to legacy `doorHardpoint` child.
    static void readDoorHardpointForExport(MObject portalTransformObj, PortalInfo& pi)
    {
        pi.hasDoorHardpoint = false;
        pi.doorTransform = Transform::identity;
        MFnDependencyNode pDep(portalTransformObj);
        MPlug enPl = pDep.findPlug("doorHardpointEnabled", true);
        if (!enPl.isNull())
        {
            enPl.getValue(pi.hasDoorHardpoint);
            if (pi.hasDoorHardpoint)
            {
                MPlug mPl = pDep.findPlug("doorHardpointMatrix", true);
                if (!mPl.isNull())
                {
                    MObject matObj;
                    if (mPl.getValue(matObj) == MS::kSuccess)
                    {
                        MFnMatrixData mdata(matObj);
                        pi.doorTransform = mayaMatrixToEngineTransform(mdata.matrix());
                    }
                }
            }
            return;
        }
        readDoorHardpointFromLegacyChild(portalTransformObj, pi);
    }

    /// Portal verts are written in building space. `toCell` maps the shape's object space into its cell
    /// (portal transform + `portals` group), so moved/rotated portal transforms export where they sit,
    /// while cell layout offsets (viewport-only spacing) are ignored.
    static bool extractMeshGeometry(MDagPath meshPath, const MMatrix& toCell, PortalGeometry& geom)
    {
        if (!meshPath.hasFn(MFn::kMesh))
            meshPath.extendToShape();
        if (!meshPath.hasFn(MFn::kMesh)) return false;

        MFnMesh meshFn(meshPath);
        MFloatPointArray points;
        if (meshFn.getPoints(points, MSpace::kObject) != MS::kSuccess) return false;

        geom.vertices.resize(static_cast<size_t>(points.length()));
        for (unsigned i = 0; i < points.length(); ++i)
        {
            const MPoint p = MPoint(points[i]) * toCell;
            geom.vertices[static_cast<size_t>(i)].x = static_cast<float>(-p.x);
            geom.vertices[static_cast<size_t>(i)].y = static_cast<float>(p.y);
            geom.vertices[static_cast<size_t>(i)].z = static_cast<float>(p.z);
        }

        // Triangulate quads / n-gons (polyCreateFacet and plane primitives give quads).
        MItMeshPolygon polyIt(meshPath);
        for (; !polyIt.isDone(); polyIt.next())
        {
            MIntArray verts;
            polyIt.getVertices(verts);
            for (unsigned t = 1; t + 1 < verts.length(); ++t)
            {
                geom.indices.push_back(verts[0]);
                geom.indices.push_back(verts[t]);
                geom.indices.push_back(verts[t + 1]);
            }
        }
        return !geom.vertices.empty() && geom.indices.size() >= 3;
    }

    static void writeIdtl(Iff& iff, const PortalGeometry& geom)
    {
        iff.insertForm(TAG_IDTL);
        iff.insertForm(TAG_0000);
        iff.insertChunk(TAG_VERT);
        for (const auto& v : geom.vertices)
            iff.insertChunkFloatVector(v);
        iff.exitChunk(TAG_VERT);
        iff.insertChunk(TAG_INDX);
        for (int idx : geom.indices)
            iff.insertChunkData(static_cast<int32>(idx));
        iff.exitChunk(TAG_INDX);
        iff.exitForm(TAG_0000);
        iff.exitForm(TAG_IDTL);
    }
}

void* ExportPob::creator()
{
    return new ExportPob();
}

MStatus ExportPob::doIt(const MArgList& args)
{
    MStatus status;
    std::string filename;

    for (unsigned i = 0; i < args.length(&status); ++i)
    {
        MString arg = args.asString(i, &status);
        if (arg == "-i" && (i + 1) < args.length(&status))
        {
            filename = args.asString(i + 1, &status).asChar();
            ++i;
        }
    }

    if (filename.empty())
    {
        std::cerr << "ExportPob: no filename specified, use -i <filename>" << std::endl;
        return MS::kFailure;
    }

    filename = resolveImportPath(filename);
    for (auto& c : filename) if (c == '\\') c = '/';
    if (filename.size() > 4 && filename.compare(filename.size() - 4, 4, ".pob") != 0)
        filename += ".pob";

    MSelectionList sel;
    MGlobal::getActiveSelectionList(sel);
    if (sel.length() == 0)
    {
        std::cerr << "ExportPob: select the POB root (or a cell) to export" << std::endl;
        return MS::kFailure;
    }

    MDagPath rootPath;
    sel.getDagPath(0, rootPath);
    if (rootPath.hasFn(MFn::kMesh))
        rootPath.pop(1);

    MFnDagNode rootFn(rootPath);
    MString rootName = rootFn.name();
    if (rootName.length() >= 2 && rootName.asChar()[0] == 'r' && rootName.asChar()[1] >= '0' && rootName.asChar()[1] <= '9' && rootPath.length() > 1)
        rootPath.pop(1);

    MObject rootObj = rootPath.node();
    rootFn.setObject(rootObj);

    std::vector<MObject> cellObjs;
    for (unsigned i = 0; i < rootFn.childCount(); ++i)
    {
        MObject child = rootFn.child(i);
        if (!child.hasFn(MFn::kTransform)) continue;
        MFnDagNode childFn(child);
        MString name = childFn.name();
        if (name.length() >= 2 && name.asChar()[0] == 'r' && name.asChar()[1] >= '0' && name.asChar()[1] <= '9')
            cellObjs.push_back(child);
    }

    if (cellObjs.empty())
    {
        std::cerr << "ExportPob: root has no cell children (r0, r1, ...)" << std::endl;
        return MS::kFailure;
    }

    // Cell index = N in rN (portalTargetCell refers to it), not Outliner order.
    std::stable_sort(cellObjs.begin(), cellObjs.end(), [](const MObject& a, const MObject& b) {
        return std::atoi(MFnDagNode(a).name().asChar() + 1) < std::atoi(MFnDagNode(b).name().asChar() + 1);
    });
    for (size_t i = 0; i < cellObjs.size(); ++i)
    {
        if (std::atoi(MFnDagNode(cellObjs[i]).name().asChar() + 1) != static_cast<int>(i))
        {
            std::cerr << "ExportPob: cells must be numbered r0..r" << (cellObjs.size() - 1)
                      << " without gaps (found " << MFnDagNode(cellObjs[i]).name().asChar() << " at index " << i << ")" << std::endl;
            return MS::kFailure;
        }
    }

    std::map<int, PortalGeometry> portalGeometries;
    std::vector<CellData> cells;
    std::vector<bool> cellUsesImportedSeeWorld(cellObjs.size(), false);

    for (size_t cellIdx = 0; cellIdx < cellObjs.size(); ++cellIdx)
    {
        CellData cell;
        char buf[32];
        sprintf(buf, "r%zu", cellIdx);
        cell.name = buf;
        {
            std::string pcn;
            if (getStringAttr(cellObjs[cellIdx], "pobCellName", pcn) && !pcn.empty())
                cell.name = pcn;
            std::string hex;
            if (getStringAttr(cellObjs[cellIdx], "pobCellCollisionExtent", hex) && hex.size() >= 16 && hex.size() % 2 == 0)
            {
                cell.collisionExtent.resize(hex.size() / 2);
                for (size_t b = 0; b < cell.collisionExtent.size(); ++b)
                    cell.collisionExtent[b] = static_cast<byte>(std::strtoul(hex.substr(b * 2, 2).c_str(), nullptr, 16));
            }
        }
        cell.canSeeWorldCell = false;
        {
            MFnDependencyNode cellNodeDep(cellObjs[cellIdx]);
            // Without an explicit attr, only cells portalled to r0 see the world (set below); r0 itself does
            // not (639 of 641 stock .pob files).
            if (!cellNodeDep.findPlug("pobCellCanSeeWorld", true).isNull())
            {
                cellUsesImportedSeeWorld[cellIdx] = true;
                getBoolAttr(cellObjs[cellIdx], "pobCellCanSeeWorld", cell.canSeeWorldCell);
            }
        }

        MFnDagNode cellFn(cellObjs[cellIdx]);
        MDagPath cellPath;
        cellFn.getPath(cellPath);
        const MMatrix cellInverse = cellPath.inclusiveMatrixInverse();
        for (unsigned j = 0; j < cellFn.childCount(); ++j)
        {
            MObject child = cellFn.child(j);
            MFnDagNode childFn(child);
            MString childName = childFn.name();

            if (childName == "mesh")
            {
                getStringAttr(child, "external_reference", cell.appearanceName);
                if (cell.appearanceName.empty())
                {
                    for (unsigned k = 0; k < childFn.childCount(); ++k)
                        getStringAttr(childFn.child(k), "external_reference", cell.appearanceName);
                }
            }
            else if (childName == "portals")
            {
                for (unsigned k = 0; k < childFn.childCount(); ++k)
                {
                    MObject portalObj = childFn.child(k);
                    if (!portalObj.hasFn(MFn::kTransform)) continue;
                    MFnDagNode portalFn(portalObj);
                    PortalInfo pi;
                    pi.portalIndex = 0;
                    if (!getIntAttr(portalObj, "buildingPortalIndex", pi.portalIndex))
                        continue;
                    pi.clockwise = false;
                    pi.targetCell = -1;
                    pi.disabled = false;
                    pi.passable = true;
                    getBoolAttr(portalObj, "portalClockwise", pi.clockwise);
                    getIntAttr(portalObj, "portalTargetCell", pi.targetCell);
                    getBoolAttr(portalObj, "portalDisabled", pi.disabled);
                    getBoolAttr(portalObj, "portalPassable", pi.passable);
                    getStringAttr(portalObj, "doorStyle", pi.doorStyle);
                    readDoorHardpointForExport(portalObj, pi);

                    for (unsigned m = 0; m < portalFn.childCount(); ++m)
                    {
                        MObject meshChild = portalFn.child(m);
                        if (meshChild.hasFn(MFn::kTransform) && MFnDagNode(meshChild).name() == "doorHardpoint")
                            continue;
                        MDagPath meshPath;
                        MFnDagNode(meshChild).getPath(meshPath);
                        if (!hasPortalAttr(meshPath)) continue;
                        PortalGeometry geom;
                        if (extractMeshGeometry(meshPath, meshPath.inclusiveMatrix() * cellInverse, geom))
                        {
                            // Both sides of a portal share one IDTL; first (counter-clockwise preferred) wins.
                            if (!pi.clockwise || portalGeometries.find(pi.portalIndex) == portalGeometries.end())
                                portalGeometries[pi.portalIndex] = geom;
                        }
                        break;
                    }
                    if (pi.portalIndex < 0)
                    {
                        std::cerr << "ExportPob: " << portalFn.name().asChar() << " has negative buildingPortalIndex, skipped" << std::endl;
                        continue;
                    }
                    cell.portals.push_back(pi);
                }
            }
            else if (childName == "lights")
            {
                for (unsigned k = 0; k < childFn.childCount(); ++k)
                {
                    MObject xfmObj = childFn.child(k);
                    if (!xfmObj.hasFn(MFn::kTransform)) continue;
                    MFnDagNode xfn(xfmObj);
                    MObject shapeObj;
                    for (unsigned m = 0; m < xfn.childCount(); ++m)
                    {
                        MObject ch = xfn.child(m);
                        if (ch.hasFn(MFn::kLight))
                        {
                            shapeObj = ch;
                            break;
                        }
                    }
                    if (shapeObj.isNull()) continue;

                    PobCellLightOut L;
                    if (shapeObj.hasFn(MFn::kAmbientLight))
                        L.type = 0;
                    else if (shapeObj.hasFn(MFn::kDirectionalLight))
                        L.type = 1;
                    else if (shapeObj.hasFn(MFn::kPointLight) || shapeObj.hasFn(MFn::kSpotLight))
                        L.type = 2;
                    else
                        continue;

                    MFnLight lfn(shapeObj);
                    MStatus lstat;
                    const MColor c = lfn.color(&lstat);
                    const float intens = lfn.intensity(&lstat);
                    // Alpha and specular have no Maya light equivalent: importPob keeps them in swg* attrs.
                    // Authored lights follow the dominant stock convention (diffuse alpha 0; specular = diffuse
                    // for parallel/point, (1,0,0,0) for ambient).
                    float diffuseAlpha = 0.f;
                    getFloatAttr(shapeObj, "swgDiffuseAlpha", diffuseAlpha);
                    L.diffuse.set(diffuseAlpha, c.r * intens, c.g * intens, c.b * intens);
                    float sr = 0.f, sg = 0.f, sb = 0.f, sa = 0.f;
                    if (getFloatAttr(shapeObj, "swgSpecularR", sr))
                    {
                        getFloatAttr(shapeObj, "swgSpecularG", sg);
                        getFloatAttr(shapeObj, "swgSpecularB", sb);
                        getFloatAttr(shapeObj, "swgSpecularAlpha", sa);
                        L.specular.set(sa, sr, sg, sb);
                    }
                    else if (L.type == 0)
                        L.specular.set(1.f, 0.f, 0.f, 0.f);
                    else
                        L.specular = L.diffuse;

                    {
                        // cell-relative, so viewport layout offsets on rN do not leak into the light
                        MDagPath lightXformPath;
                        MFnDagNode(xfmObj).getPath(lightXformPath);
                        L.transform = PobAuthoring::mayaSceneMatrixToEngineTransform(lightXformPath.inclusiveMatrix() * cellInverse);
                    }

                    if (L.type == 2)
                    {
                        MFnDependencyNode dep(shapeObj);
                        MPlug plug = dep.findPlug("constantAttenuation", true);
                        if (!plug.isNull()) plug.getValue(L.constantAttenuation);
                        plug = dep.findPlug("linearAttenuation", true);
                        if (!plug.isNull()) plug.getValue(L.linearAttenuation);
                        plug = dep.findPlug("quadraticAttenuation", true);
                        if (!plug.isNull()) plug.getValue(L.quadraticAttenuation);
                    }

                    cell.lights.push_back(L);
                }
            }
            else if (childName == "collision")
            {
                for (unsigned k = 0; k < childFn.childCount(); ++k)
                {
                    MObject colObj = childFn.child(k);
                    MFnDagNode colFn(colObj);
                    if (colFn.name() == "floor0")
                    {
                        getStringAttr(colObj, "external_reference", cell.floorName);
                        for (unsigned m = 0; m < colFn.childCount() && cell.floorName.empty(); ++m)
                            getStringAttr(colFn.child(m), "external_reference", cell.floorName);
                    }
                    else if (colFn.name() == "extent")
                    {
                        // Collision mesh (CMSH): every mesh below rN|collision|extent, in cell space.
                        MDagPath extentPath;
                        colFn.getPath(extentPath);
                        MItDag it;
                        it.reset(extentPath, MItDag::kDepthFirst, MFn::kMesh);
                        for (; !it.isDone(); it.next())
                        {
                            MDagPath shapePath;
                            it.getPath(shapePath);
                            if (MFnDagNode(shapePath).isIntermediateObject()) continue;
                            PortalGeometry part;
                            if (!extractMeshGeometry(shapePath, shapePath.inclusiveMatrix() * cellInverse, part)) continue;
                            const int base = static_cast<int>(cell.collisionMesh.vertices.size());
                            cell.collisionMesh.vertices.insert(cell.collisionMesh.vertices.end(), part.vertices.begin(), part.vertices.end());
                            // Keep the Maya order: X is mirrored on export, which flips cross(B-A, C-A) to the side
                            // away from the Maya face, and that is the stock layout (measured: stock room CMSH
                            // triangles have cross(B-A, C-A) pointing out of the room; the engine takes the other
                            // side as the front, like FLOR). Swapping corners here put the front outside the room,
                            // so the chase camera slid through the walls from inside and the view went black.
                            for (size_t t = 0; t < part.indices.size(); ++t)
                                cell.collisionMesh.indices.push_back(base + part.indices[t]);
                        }
                    }
                }
            }
        }

        cells.push_back(cell);
    }

    // Size the portal table from both geometry and cell references so every PRTL index is in range.
    int numberOfPortals = 0;
    for (const auto& p : portalGeometries)
        if (p.first >= numberOfPortals)
            numberOfPortals = p.first + 1;
    for (const auto& c : cells)
        for (const auto& pi : c.portals)
            if (pi.portalIndex >= numberOfPortals)
                numberOfPortals = pi.portalIndex + 1;
    for (int i = 0; i < numberOfPortals; ++i)
        if (portalGeometries.find(i) == portalGeometries.end())
            std::cerr << "ExportPob: warning: portal " << i << " has no mesh with a 'portal' attribute; writing degenerate geometry" << std::endl;

    std::vector<int> clockwiseCell(numberOfPortals, -1);
    std::vector<int> counterClockwiseCell(numberOfPortals, -1);
    for (size_t j = 0; j < cells.size(); ++j)
    {
        for (const auto& pi : cells[j].portals)
        {
            int idx = pi.portalIndex;
            if (idx < 0 || idx >= numberOfPortals) continue;
            if (pi.clockwise)
                clockwiseCell[static_cast<size_t>(idx)] = static_cast<int>(j);
            else
                counterClockwiseCell[static_cast<size_t>(idx)] = static_cast<int>(j);
        }
    }
    for (size_t m = 0; m < static_cast<size_t>(numberOfPortals); ++m)
    {
        if (clockwiseCell[m] == 0 && counterClockwiseCell[m] >= 0)
        {
            const size_t idx = static_cast<size_t>(counterClockwiseCell[m]);
            if (idx < cells.size() && !cellUsesImportedSeeWorld[idx])
                cells[idx].canSeeWorldCell = true;
        }
        if (counterClockwiseCell[m] == 0 && clockwiseCell[m] >= 0)
        {
            const size_t idx = static_cast<size_t>(clockwiseCell[m]);
            if (idx < cells.size() && !cellUsesImportedSeeWorld[idx])
                cells[idx].canSeeWorldCell = true;
        }
    }

    Iff iff(65536, true);
    iff.insertForm(TAG_PRTO);
    iff.insertForm(TAG_0004);

    iff.insertChunk(TAG_DATA);
    iff.insertChunkData(static_cast<int32>(numberOfPortals));
    iff.insertChunkData(static_cast<int32>(static_cast<int>(cells.size())));
    iff.exitChunk(TAG_DATA);

    iff.insertForm(TAG_PRTS);
    for (int i = 0; i < numberOfPortals; ++i)
    {
        auto it = portalGeometries.find(i);
        if (it != portalGeometries.end())
            writeIdtl(iff, it->second);
        else
        {
            PortalGeometry empty;
            empty.vertices.resize(3);
            empty.indices = {0, 1, 2};
            writeIdtl(iff, empty);
        }
    }
    iff.exitForm(TAG_PRTS);

    iff.insertForm(TAG_CELS);
    for (size_t j = 0; j < cells.size(); ++j)
    {
        const CellData& cell = cells[j];
        iff.insertForm(TAG_CELL);
        iff.insertForm(TAG_0005);

        iff.insertChunk(TAG_DATA);
        iff.insertChunkData(static_cast<int32>(cell.portals.size()));
        iff.insertChunkData(static_cast<int8>(cell.canSeeWorldCell ? 1 : 0));
        iff.insertChunkString(cell.name.c_str());
        iff.insertChunkString(cell.appearanceName.c_str());
        iff.insertChunkData(static_cast<int8>(!cell.floorName.empty() ? 1 : 0));
        if (!cell.floorName.empty())
            iff.insertChunkString(cell.floorName.c_str());
        iff.exitChunk(TAG_DATA);

        // Collision extent: verbatim from import (pobCellCollisionExtent), an authored collision mesh
        // (rN|collision|extent -> CMSH/0000/IDTL, the stock layout), or an empty NULL extent.
        if (!cell.collisionExtent.empty())
        {
            Iff extent(static_cast<int>(cell.collisionExtent.size()), cell.collisionExtent.data(), false);
            iff.insertIff(&extent);
        }
        else if (cell.collisionMesh.indices.size() >= 3)
        {
            iff.insertForm(TAG_CMSH);
            iff.insertForm(TAG_0000);
            writeIdtl(iff, cell.collisionMesh);
            iff.exitForm(TAG_0000);
            iff.exitForm(TAG_CMSH);
        }
        else
        {
            iff.insertForm(TAG_NULL);
            iff.exitForm(TAG_NULL);
        }

        for (const auto& pi : cell.portals)
        {
            iff.insertForm(TAG_PRTL);
            iff.insertChunk(TAG_0005);
            iff.insertChunkData(static_cast<int8>(pi.disabled ? 1 : 0));
            iff.insertChunkData(static_cast<int8>(pi.passable ? 1 : 0));
            iff.insertChunkData(static_cast<int32>(pi.portalIndex));
            int targetCell = pi.targetCell;
            if (targetCell < 0 || targetCell >= static_cast<int>(cells.size()))
            {
                if (pi.clockwise && counterClockwiseCell[static_cast<size_t>(pi.portalIndex)] >= 0)
                    targetCell = counterClockwiseCell[static_cast<size_t>(pi.portalIndex)];
                else if (!pi.clockwise && clockwiseCell[static_cast<size_t>(pi.portalIndex)] >= 0)
                    targetCell = clockwiseCell[static_cast<size_t>(pi.portalIndex)];
            }
            iff.insertChunkData(static_cast<int8>(pi.clockwise ? 1 : 0));
            iff.insertChunkData(static_cast<int32>(targetCell));
            iff.insertChunkString(pi.doorStyle.empty() ? "" : pi.doorStyle.c_str());
            iff.insertChunkData(static_cast<int8>(pi.hasDoorHardpoint ? 1 : 0));
            iff.insertChunkFloatTransform(pi.hasDoorHardpoint ? pi.doorTransform : Transform::identity);
            iff.exitChunk(TAG_0005);
            iff.exitForm(TAG_PRTL);
        }

        iff.insertChunk(TAG_LGHT);
        iff.insertChunkData(static_cast<int32>(static_cast<int>(cell.lights.size())));
        for (const auto& L : cell.lights)
        {
            iff.insertChunkData(static_cast<int8>(L.type));
            iff.insertChunkFloatVectorArgb(L.diffuse);
            iff.insertChunkFloatVectorArgb(L.specular);
            iff.insertChunkFloatTransform(L.transform);
            iff.insertChunkData(L.constantAttenuation);
            iff.insertChunkData(L.linearAttenuation);
            iff.insertChunkData(L.quadraticAttenuation);
        }
        iff.exitChunk(TAG_LGHT);

        iff.exitForm(TAG_0005);
        iff.exitForm(TAG_CELL);
    }
    iff.exitForm(TAG_CELS);

    uint32 crc = iff.calculateCrc();
    iff.insertChunk(TAG_CRC);
    iff.insertChunkData(crc);
    iff.exitChunk(TAG_CRC);

    iff.exitForm(TAG_0004);
    iff.exitForm(TAG_PRTO);

    if (!iff.write(filename.c_str(), false))
    {
        std::cerr << "ExportPob: failed to write " << filename << std::endl;
        return MS::kFailure;
    }

    setResult(MString(filename.c_str()));
    return MS::kSuccess;
}
