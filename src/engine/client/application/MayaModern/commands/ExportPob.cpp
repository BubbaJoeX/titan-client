#include "ExportPob.h"
#include "ImportPathResolver.h"

#include "Iff.h"
#include "Tag.h"
#include "Vector.h"

#include <maya/MArgList.h>
#include <maya/MDagPath.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFloatPointArray.h>
#include <maya/MFnMesh.h>
#include <maya/MFnTransform.h>
#include <maya/MGlobal.h>
#include <maya/MItDag.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MSelectionList.h>

#include <algorithm>
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
    };

    struct CellData
    {
        std::string name;
        std::string appearanceName;
        std::string floorName;
        bool canSeeWorldCell = false;
        std::vector<PortalInfo> portals;
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

    static bool extractMeshGeometry(MDagPath meshPath, PortalGeometry& geom)
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
            geom.vertices[static_cast<size_t>(i)].x = -points[i].x;
            geom.vertices[static_cast<size_t>(i)].y = points[i].y;
            geom.vertices[static_cast<size_t>(i)].z = points[i].z;
        }

        MItMeshPolygon polyIt(meshPath);
        for (; !polyIt.isDone(); polyIt.next())
        {
            if (polyIt.polygonVertexCount() != 3) return false;
            MIntArray verts;
            polyIt.getVertices(verts);
            if (verts.length() >= 3)
            {
                geom.indices.push_back(verts[0]);
                geom.indices.push_back(verts[1]);
                geom.indices.push_back(verts[2]);
            }
        }
        return !geom.vertices.empty() && geom.indices.size() >= 3;
    }

    static void writeIdtl(Iff& iff, const PortalGeometry& geom)
    {
        iff.insertForm(TAG_IDTL);
        iff.insertForm(TAG_0000);
        iff.insertChunk(TAG_VERT);
        iff.insertChunkData(static_cast<int32>(geom.vertices.size()));
        for (const auto& v : geom.vertices)
            iff.insertChunkFloatVector(v);
        iff.exitChunk(TAG_VERT);
        iff.insertChunk(TAG_INDX);
        iff.insertChunkData(static_cast<int32>(geom.indices.size()));
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

    std::map<int, PortalGeometry> portalGeometries;
    std::vector<CellData> cells;

    for (size_t cellIdx = 0; cellIdx < cellObjs.size(); ++cellIdx)
    {
        CellData cell;
        char buf[32];
        sprintf(buf, "r%zu", cellIdx);
        cell.name = buf;

        MFnDagNode cellFn(cellObjs[cellIdx]);
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
                    MDagPath portalPath;
                    portalFn.getPath(portalPath);
                    for (unsigned m = 0; m < portalFn.childCount(); ++m)
                    {
                        MDagPath meshPath;
                        MFnDagNode(portalFn.child(m)).getPath(meshPath);
                        if (hasPortalAttr(meshPath))
                        {
                            PortalInfo pi;
                            pi.portalIndex = 0;
                            pi.clockwise = false;
                            pi.targetCell = -1;
                            pi.disabled = false;
                            pi.passable = true;
                            getIntAttr(portalObj, "buildingPortalIndex", pi.portalIndex);
                            getBoolAttr(portalObj, "portalClockwise", pi.clockwise);
                            getIntAttr(portalObj, "portalTargetCell", pi.targetCell);
                            getBoolAttr(portalObj, "portalDisabled", pi.disabled);
                            getBoolAttr(portalObj, "portalPassable", pi.passable);
                            getStringAttr(portalObj, "doorStyle", pi.doorStyle);

                            PortalGeometry geom;
                            if (extractMeshGeometry(meshPath, geom))
                                portalGeometries[pi.portalIndex] = geom;
                            cell.portals.push_back(pi);
                            break;
                        }
                    }
                }
            }
            else if (childName == "collision")
            {
                for (unsigned k = 0; k < childFn.childCount(); ++k)
                {
                    MObject floorObj = childFn.child(k);
                    MFnDagNode floorFn(floorObj);
                    if (floorFn.name() != "floor0") continue;
                    getStringAttr(floorObj, "external_reference", cell.floorName);
                    for (unsigned m = 0; m < floorFn.childCount() && cell.floorName.empty(); ++m)
                        getStringAttr(floorFn.child(m), "external_reference", cell.floorName);
                    break;
                }
            }
        }

        if (cellIdx == 0)
            cell.canSeeWorldCell = true;
        cells.push_back(cell);
    }

    int numberOfPortals = 0;
    for (const auto& p : portalGeometries)
        if (p.first >= numberOfPortals)
            numberOfPortals = p.first + 1;

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
        if (clockwiseCell[m] == 0)
            cells[static_cast<size_t>(counterClockwiseCell[m])].canSeeWorldCell = true;
        if (counterClockwiseCell[m] == 0)
            cells[static_cast<size_t>(clockwiseCell[m])].canSeeWorldCell = true;
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

        iff.insertForm(TAG_NULL);
        iff.exitForm(TAG_NULL);

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
            iff.insertChunkData(static_cast<int8>(0));
            for (int m = 0; m < 16; ++m)
                iff.insertChunkData(0.0f);
            iff.exitChunk(TAG_0005);
            iff.exitForm(TAG_PRTL);
        }

        iff.insertChunk(TAG_LGHT);
        iff.insertChunkData(static_cast<int32>(0));
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

    return MS::kSuccess;
}
