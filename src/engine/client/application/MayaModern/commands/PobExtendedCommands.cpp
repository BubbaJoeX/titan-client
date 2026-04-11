#include "PobExtendedCommands.h"
#include "PobAuthoringShared.h"

#include <maya/MArgList.h>
#include <maya/MDagPath.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnTransform.h>
#include <maya/MGlobal.h>
#include <maya/MItDag.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MSelectionList.h>
#include <maya/MString.h>
#include <maya/MVector.h>

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace
{
    static MStatus findCellDirectChildByName(MObject pobRoot, const MString& shortName, MDagPath& outCellPath)
    {
        MFnDagNode fn(pobRoot);
        for (unsigned i = 0; i < fn.childCount(); ++i)
        {
            MObject ch = fn.child(i);
            if (!ch.hasFn(MFn::kTransform)) continue;
            if (MFnDagNode(ch).name() == shortName)
                return MFnDagNode(ch).getPath(outCellPath);
        }
        return MS::kFailure;
    }

    static int maxCellRIndex(MObject pobRoot)
    {
        int m = -1;
        MFnDagNode fn(pobRoot);
        for (unsigned i = 0; i < fn.childCount(); ++i)
        {
            MObject ch = fn.child(i);
            if (!ch.hasFn(MFn::kTransform)) continue;
            const int idx = PobAuthoring::cellIndexFromRName(MFnDagNode(ch).name());
            if (idx > m) m = idx;
        }
        return m;
    }

    static void collectCellsSorted(MObject pobRoot, std::vector<std::pair<int, MObject>>& out)
    {
        MFnDagNode fn(pobRoot);
        for (unsigned i = 0; i < fn.childCount(); ++i)
        {
            MObject ch = fn.child(i);
            if (!ch.hasFn(MFn::kTransform)) continue;
            MString nm = MFnDagNode(ch).name();
            const int idx = PobAuthoring::cellIndexFromRName(nm);
            if (idx >= 0)
                out.push_back(std::make_pair(idx, ch));
        }
        std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    }

    static bool plugStringEmpty(MObject node, const char* attrName)
    {
        MFnDependencyNode dn(node);
        MPlug p = dn.findPlug(attrName, true);
        if (p.isNull()) return true;
        MString v;
        if (p.getValue(v) != MS::kSuccess) return true;
        return v.length() == 0;
    }

    static MObject resolveCellFromSelectionFirst(MDagPath& outCellPath)
    {
        MSelectionList sel;
        MGlobal::getActiveSelectionList(sel);
        if (sel.length() == 0) return MObject::kNullObj;
        MDagPath p;
        if (sel.getDagPath(0, p) != MS::kSuccess) return MObject::kNullObj;
        MFnDagNode fn(p.node());
        if (PobAuthoring::isCellName(fn.name()))
        {
            outCellPath = p;
            return p.node();
        }
        if (p.length() >= 2)
        {
            MDagPath parent = p;
            parent.pop(1);
            MFnDagNode pfn(parent.node());
            if (PobAuthoring::isCellName(pfn.name()))
            {
                outCellPath = parent;
                return parent.node();
            }
        }
        return MObject::kNullObj;
    }
}

void* ValidatePob::creator()
{
    return new ValidatePob();
}

MStatus ValidatePob::doIt(const MArgList& args)
{
    MStatus status;
    std::string rootPathStr;
    const unsigned n = args.length(&status);
    if (!status) return MS::kFailure;
    for (unsigned i = 0; i < n; ++i)
    {
        MString a = args.asString(i, &status);
        if ((a == "-r" || a == "-root") && (i + 1) < n)
        {
            rootPathStr = args.asString(i + 1, &status).asChar();
            ++i;
        }
    }

    MDagPath rootPath;
    if (PobAuthoring::resolvePobRootFromSelectionOrArg(rootPathStr, rootPath) != MS::kSuccess)
    {
        std::cerr << "validatePob: select anything under the POB or use -root \"|building\"" << std::endl;
        return MS::kFailure;
    }

    MString report;
    report += "=== validatePob: ";
    report += rootPath.partialPathName();
    report += " ===\n";

    std::vector<std::pair<int, MObject>> cells;
    collectCellsSorted(rootPath.node(), cells);
    if (cells.empty())
        report += "WARNING: no rN cell transforms found under root.\n";

    int cellIssues = 0;
    for (const auto& ce : cells)
    {
        const int ci = ce.first;
        MObject cellObj = ce.second;
        MDagPath cellPath;
        MFnDagNode(cellObj).getPath(cellPath);
        char hdr[128];
        sprintf(hdr, "--- r%d (%s) ---\n", ci, cellPath.partialPathName().asChar());
        report += hdr;

        MDagPath meshP, portalsP, collisionP;
        const bool hasMesh = PobAuthoring::getChildTransformByName(cellObj, "mesh", meshP) == MS::kSuccess;
        const bool hasPortals = PobAuthoring::getChildTransformByName(cellObj, "portals", portalsP) == MS::kSuccess;
        const bool hasCol = PobAuthoring::getChildTransformByName(cellObj, "collision", collisionP) == MS::kSuccess;
        if (!hasMesh) { report += "  MISSING: mesh\n"; ++cellIssues; }
        if (!hasPortals) { report += "  MISSING: portals\n"; ++cellIssues; }
        if (!hasCol) { report += "  MISSING: collision\n"; ++cellIssues; }

        if (hasMesh && plugStringEmpty(meshP.node(), "external_reference"))
        {
            report += "  WARNING: mesh.external_reference is empty\n";
            ++cellIssues;
        }
        if (hasCol)
        {
            MDagPath floorP;
            if (PobAuthoring::getChildTransformByName(collisionP.node(), "floor0", floorP) == MS::kSuccess)
            {
                if (plugStringEmpty(floorP.node(), "external_reference"))
                {
                    report += "  WARNING: floor0.external_reference is empty\n";
                    ++cellIssues;
                }
            }
            else
                report += "  WARNING: collision has no floor0 child\n";
        }
    }

    std::map<int, std::vector<std::string>> byIndex;
    MItDag it(MItDag::kDepthFirst, MFn::kTransform);
    it.reset(rootPath, MItDag::kDepthFirst, MFn::kTransform);
    for (; !it.isDone(); it.next())
    {
        MDagPath p;
        if (it.getPath(p) != MS::kSuccess) continue;
        MFnDependencyNode dn(p.node());
        MPlug pl = dn.findPlug("buildingPortalIndex", true);
        if (pl.isNull()) continue;
        int idx = 0;
        if (pl.getValue(idx) != MS::kSuccess) continue;
        byIndex[idx].push_back(p.fullPathName().asChar());
    }

    report += "--- portal indices ---\n";
    int portalIssues = 0;
    if (byIndex.empty())
        report += "(no buildingPortalIndex in hierarchy)\n";
    for (const auto& kv : byIndex)
    {
        const size_t cnt = kv.second.size();
        if (cnt != 2u)
        {
            char l[256];
            sprintf(l, "  index %d: %zu instance(s) (expect 2 for a paired doorway)\n", kv.first, cnt);
            report += l;
            ++portalIssues;
        }
        for (const auto& s : kv.second)
        {
            report += "    ";
            report += s.c_str();
            report += "\n";
        }
    }

    char sum[256];
    sprintf(sum, "--- summary: %zu cell(s), %d cell warnings, %d portal index warning(s) ---\n",
            cells.size(), cellIssues, portalIssues);
    report += sum;
    MGlobal::displayInfo(report);
    return MS::kSuccess;
}

void* ConnectPobCells::creator()
{
    return new ConnectPobCells();
}

MStatus ConnectPobCells::doIt(const MArgList& args)
{
    MStatus status;
    std::string rootPathStr;
    std::string fromName;
    std::string toName;
    float width = 2.0f;
    float height = 2.5f;
    bool doorHp = false;
    std::string doorStyle;

    const unsigned n = args.length(&status);
    if (!status) return MS::kFailure;
    for (unsigned i = 0; i < n; ++i)
    {
        MString a = args.asString(i, &status);
        if ((a == "-r" || a == "-root") && (i + 1) < n) { rootPathStr = args.asString(++i, &status).asChar(); }
        else if ((a == "-from" || a == "-f") && (i + 1) < n) { fromName = args.asString(++i, &status).asChar(); }
        else if ((a == "-to" || a == "-t") && (i + 1) < n) { toName = args.asString(++i, &status).asChar(); }
        else if ((a == "-w" || a == "-width") && (i + 1) < n) { width = static_cast<float>(args.asDouble(++i, &status)); }
        else if ((a == "-h" || a == "-height") && (i + 1) < n) { height = static_cast<float>(args.asDouble(++i, &status)); }
        else if (a == "-doorHardpoint") doorHp = true;
        else if (a == "-doorStyle" && (i + 1) < n) { doorStyle = args.asString(++i, &status).asChar(); }
    }

    MDagPath rootPath;
    if (PobAuthoring::resolvePobRootFromSelectionOrArg(rootPathStr, rootPath) != MS::kSuccess)
    {
        std::cerr << "connectPobCells: need POB context (selection or -root)" << std::endl;
        return MS::kFailure;
    }
    MObject pobRoot = rootPath.node();

    MDagPath fromCell, toCell;
    if (fromName.empty() || toName.empty())
    {
        MSelectionList sel;
        MGlobal::getActiveSelectionList(sel);
        if (sel.length() < 2)
        {
            std::cerr << "connectPobCells: use -from r2 -to r5, or select exactly two cells (from, to)" << std::endl;
            return MS::kFailure;
        }
        if (sel.getDagPath(0, fromCell) != MS::kSuccess || sel.getDagPath(1, toCell) != MS::kSuccess)
            return MS::kFailure;
        if (!PobAuthoring::isCellName(MFnDagNode(fromCell.node()).name()) ||
            !PobAuthoring::isCellName(MFnDagNode(toCell.node()).name()))
        {
            std::cerr << "connectPobCells: selection must be two rN cell transforms" << std::endl;
            return MS::kFailure;
        }
    }
    else
    {
        if (findCellDirectChildByName(pobRoot, MString(fromName.c_str()), fromCell) != MS::kSuccess ||
            findCellDirectChildByName(pobRoot, MString(toName.c_str()), toCell) != MS::kSuccess)
        {
            std::cerr << "connectPobCells: -from / -to must name direct children of POB root (e.g. r2)" << std::endl;
            return MS::kFailure;
        }
    }

    const int fromIdx = PobAuthoring::cellIndexFromRName(MFnDagNode(fromCell.node()).name());
    const int toIdx = PobAuthoring::cellIndexFromRName(MFnDagNode(toCell.node()).name());
    if (fromIdx < 0 || toIdx < 0)
        return MS::kFailure;

    MDagPath fromPortals, toPortals;
    if (PobAuthoring::getChildTransformByName(fromCell.node(), "portals", fromPortals) != MS::kSuccess ||
        PobAuthoring::getChildTransformByName(toCell.node(), "portals", toPortals) != MS::kSuccess)
    {
        std::cerr << "connectPobCells: both cells need a \"portals\" child" << std::endl;
        return MS::kFailure;
    }

    const int portalIdx = PobAuthoring::maxBuildingPortalIndexUnderRoot(pobRoot) + 1;

    char nameA[64], nameB[64];
    sprintf(nameA, "connect_p%d_a", portalIdx);
    sprintf(nameB, "connect_p%d_b", portalIdx);

    MDagPath portalA, portalB;
    status = PobAuthoring::createPortalDoorMeshAndParent(fromPortals.node(), nameA, width, height, portalIdx,
                                                         false, toIdx, false, true, doorStyle, doorHp, portalA);
    if (!status) return status;
    status = PobAuthoring::createPortalDoorMeshAndParent(toPortals.node(), nameB, width, height, portalIdx,
                                                         true, fromIdx, false, true, doorStyle, doorHp, portalB);
    if (!status) return status;

    MSelectionList outSel;
    outSel.add(portalA.node());
    outSel.add(portalB.node());
    MGlobal::setActiveSelectionList(outSel);

    char buf[384];
    sprintf(buf,
            "connectPobCells: portal index %d links r%d <-> r%d (cw=0 on first/from side, cw=1 on second/to). "
            "Move/rotate portal meshes in each cell as needed.\n",
            portalIdx, fromIdx, toIdx);
    MGlobal::displayInfo(MString(buf));
    return MS::kSuccess;
}

void* LayoutPobCells::creator()
{
    return new LayoutPobCells();
}

MStatus LayoutPobCells::doIt(const MArgList& args)
{
    MStatus status;
    std::string rootPathStr;
    int cols = 8;
    double dx = 12.0;
    double dz = 0.0;

    const unsigned n = args.length(&status);
    if (!status) return MS::kFailure;
    for (unsigned i = 0; i < n; ++i)
    {
        MString a = args.asString(i, &status);
        if ((a == "-r" || a == "-root") && (i + 1) < n) { rootPathStr = args.asString(++i, &status).asChar(); }
        else if ((a == "-cols" || a == "-c") && (i + 1) < n) { cols = static_cast<int>(args.asInt(++i, &status)); }
        else if ((a == "-dx") && (i + 1) < n) { dx = args.asDouble(++i, &status); }
        else if ((a == "-dz") && (i + 1) < n) { dz = args.asDouble(++i, &status); }
    }
    if (cols < 1) cols = 1;

    MDagPath rootPath;
    if (PobAuthoring::resolvePobRootFromSelectionOrArg(rootPathStr, rootPath) != MS::kSuccess)
    {
        std::cerr << "layoutPobCells: select under POB or -root \"|building\"" << std::endl;
        return MS::kFailure;
    }

    std::vector<std::pair<int, MObject>> cells;
    collectCellsSorted(rootPath.node(), cells);
    if (cells.empty())
    {
        std::cerr << "layoutPobCells: no rN cells under root" << std::endl;
        return MS::kFailure;
    }

    for (size_t i = 0; i < cells.size(); ++i)
    {
        const int col = static_cast<int>(i) % cols;
        const int row = static_cast<int>(i) / cols;
        MFnTransform tfn(cells[i].second, &status);
        if (!status) continue;
        tfn.setTranslation(MVector(static_cast<double>(col) * dx, 0.0, static_cast<double>(row) * dz),
                             MSpace::kTransform);
    }

    char buf[256];
    sprintf(buf, "layoutPobCells: positioned %zu cells (%d columns, dx=%.2f dz=%.2f)\n", cells.size(), cols, dx, dz);
    MGlobal::displayInfo(MString(buf));
    return MS::kSuccess;
}

void* DuplicatePobCell::creator()
{
    return new DuplicatePobCell();
}

MStatus DuplicatePobCell::doIt(const MArgList& args)
{
    MStatus status;
    std::string rootPathStr;
    bool stripPortals = false;
    int remapOffset = 0;
    bool doRemap = false;

    const unsigned n = args.length(&status);
    if (!status) return MS::kFailure;
    for (unsigned i = 0; i < n; ++i)
    {
        MString a = args.asString(i, &status);
        if ((a == "-r" || a == "-root") && (i + 1) < n) { rootPathStr = args.asString(++i, &status).asChar(); }
        else if (a == "-stripPortals") stripPortals = true;
        else if (a == "-remapPortalIndices" && (i + 1) < n)
        {
            remapOffset = static_cast<int>(args.asInt(++i, &status));
            doRemap = true;
        }
    }

    MDagPath cellPath;
    MObject cellObj = resolveCellFromSelectionFirst(cellPath);
    if (cellObj.isNull())
    {
        std::cerr << "duplicatePobCell: select a cell transform (rN)" << std::endl;
        return MS::kFailure;
    }

    MDagPath rootPath;
    if (PobAuthoring::resolvePobRootFromSelectionOrArg(rootPathStr, rootPath) != MS::kSuccess)
    {
        std::cerr << "duplicatePobCell: could not resolve POB root" << std::endl;
        return MS::kFailure;
    }

    MObject pobRoot = rootPath.node();
    MFnDagNode cellFn(cellObj);
    if (cellFn.parentCount() == 0 || cellFn.parent(0) != pobRoot)
    {
        std::cerr << "duplicatePobCell: selected cell must be a direct child of the POB root" << std::endl;
        return MS::kFailure;
    }

    const int newIndex = maxCellRIndex(pobRoot) + 1;
    char newName[32];
    sprintf(newName, "r%d", newIndex);

    MString mel = "string $swgDup[] = `duplicate -rr \"";
    mel += cellPath.fullPathName();
    mel += "\"`; parent $swgDup[0] \"";
    mel += rootPath.fullPathName();
    mel += "\"; rename $swgDup[0] \"";
    mel += newName;
    mel += "\";";
    status = MGlobal::executeCommand(mel);
    if (!status)
    {
        std::cerr << "duplicatePobCell: duplicate/parent/rename failed" << std::endl;
        return MS::kFailure;
    }

    MDagPath dupCellPath;
    if (findCellDirectChildByName(pobRoot, MString(newName), dupCellPath) != MS::kSuccess)
    {
        std::cerr << "duplicatePobCell: could not find new cell transform" << std::endl;
        return MS::kFailure;
    }

    if (stripPortals)
    {
        MDagPath portalsP;
        if (PobAuthoring::getChildTransformByName(dupCellPath.node(), "portals", portalsP) == MS::kSuccess)
        {
            MFnDagNode pfn(portalsP.node());
            for (unsigned c = pfn.childCount(); c-- > 0;)
            {
                MObject ch = pfn.child(c);
                if (ch.hasFn(MFn::kTransform))
                {
                    MDagPath chp;
                    MFnDagNode(ch).getPath(chp);
                    MString delCmd = "delete \"";
                    delCmd += chp.fullPathName();
                    delCmd += "\"";
                    MGlobal::executeCommand(delCmd);
                }
            }
        }
    }

    if (doRemap && remapOffset != 0)
    {
        MItDag it(MItDag::kDepthFirst, MFn::kTransform);
        it.reset(dupCellPath, MItDag::kDepthFirst, MFn::kTransform);
        for (; !it.isDone(); it.next())
        {
            MDagPath p;
            if (it.getPath(p) != MS::kSuccess) continue;
            MFnDependencyNode dn(p.node());
            MPlug pl = dn.findPlug("buildingPortalIndex", true);
            if (pl.isNull()) continue;
            int v = 0;
            if (pl.getValue(v) != MS::kSuccess) continue;
            pl.setInt(v + remapOffset);
        }
    }

    MSelectionList sel;
    sel.add(dupCellPath.node());
    MGlobal::setActiveSelectionList(sel);

    char buf[320];
    sprintf(buf,
            "duplicatePobCell: created %s — tweak portals/indices; use connectPobCells or addPobPortal. "
            "Flags: -stripPortals clears duplicated portals; -remapPortalIndices N adds N to each portal index.\n",
            newName);
    MGlobal::displayInfo(MString(buf));
    return MS::kSuccess;
}
