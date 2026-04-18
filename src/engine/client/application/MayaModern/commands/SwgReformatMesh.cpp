#include "SwgReformatMesh.h"

#include "MayaUtility.h"

#include <maya/MArgList.h>
#include <maya/MDagPath.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnMesh.h>
#include <maya/MGlobal.h>
#include <maya/MItDag.h>
#include <maya/MObject.h>
#include <maya/MSelectionList.h>
#include <maya/MStatus.h>
#include <maya/MString.h>
#include <maya/MStringArray.h>

#include <cctype>
#include <set>
#include <string>
#include <vector>

namespace
{
    std::string sanitizeName(const char* raw, const char* defVal)
    {
        std::string s = raw && raw[0] ? raw : defVal;
        std::string out;
        for (char c : s)
        {
            if (std::isalnum(static_cast<unsigned char>(c)) || c == '_')
                out += c;
        }
        if (out.empty())
            out = defVal;
        return out;
    }

    void collectMeshShapesFromSelection(MSelectionList& outMeshes)
    {
        outMeshes.clear();
        MSelectionList sel;
        if (MGlobal::getActiveSelectionList(sel) != MS::kSuccess || sel.length() == 0)
            return;

        for (unsigned i = 0; i < sel.length(); ++i)
        {
            MDagPath path;
            if (sel.getDagPath(i, path) != MS::kSuccess)
                continue;

            if (path.hasFn(MFn::kMesh))
            {
                if (!MayaUtility::meshShapeExcludedFromStaticMeshExport(path))
                    outMeshes.add(path);
                continue;
            }

            if (!path.hasFn(MFn::kTransform))
                continue;

            MFnDagNode dag(path);
            const unsigned n = dag.childCount();
            for (unsigned c = 0; c < n; ++c)
            {
                MObject child = dag.child(c);
                if (!child.isNull() && child.hasFn(MFn::kMesh))
                {
                    MDagPath meshPath(path);
                    meshPath.push(child);
                    if (!MayaUtility::meshShapeExcludedFromStaticMeshExport(meshPath))
                        outMeshes.add(meshPath);
                }
            }
        }
    }

    MDagPath rootTransformForMesh(const MDagPath& meshPath)
    {
        MDagPath root(meshPath);
        if (root.hasFn(MFn::kMesh))
            root.pop();
        MStatus st;
        while (root.length() > 1)
        {
            st = root.pop();
            if (!st)
                break;
        }
        return root;
    }

    void collectUniqueRoots(const MSelectionList& meshes, std::vector<MDagPath>& outRoots)
    {
        std::set<std::string> seen;
        outRoots.clear();
        const unsigned n = meshes.length();
        for (unsigned i = 0; i < n; ++i)
        {
            MDagPath mp;
            if (meshes.getDagPath(i, mp) != MS::kSuccess)
                continue;
            MDagPath r = rootTransformForMesh(mp);
            const std::string key(r.fullPathName().asChar());
            if (seen.insert(key).second)
                outRoots.push_back(r);
        }
    }

    MStatus deleteUnusedShadingNodes()
    {
        // Hypershade: Edit > Delete Unused Nodes
        return MGlobal::executeCommand(
            "catchQuiet(`hyperShadePanelMenuCommand \"deleteUnusedNodes\" \"\"`);", false, false);
    }
}

void* SwgReformatMesh::creator()
{
    return new SwgReformatMesh();
}

MStatus SwgReformatMesh::doIt(const MArgList& args)
{
    MStatus st;
    std::string groupName = "swgStaticMesh";

    const unsigned n = args.length(&st);
    if (!st)
        return MS::kFailure;
    for (unsigned i = 0; i < n; ++i)
    {
        MString a = args.asString(i, &st);
        if (!st)
            return MS::kFailure;
        if (a == "-root" && i + 1 < n)
            groupName = sanitizeName(args.asString(++i, &st).asChar(), "swgStaticMesh");
    }

    MSelectionList keepMeshes;
    collectMeshShapesFromSelection(keepMeshes);
    if (keepMeshes.length() == 0)
    {
        MGlobal::displayError("swgReformatMesh: select one or more polygon meshes (or their transforms).");
        return MS::kFailure;
    }

    std::set<std::string> keepMeshPaths;
    for (unsigned i = 0; i < keepMeshes.length(); ++i)
    {
        MDagPath p;
        if (keepMeshes.getDagPath(i, p) == MS::kSuccess)
            keepMeshPaths.insert(p.fullPathName().asChar());
    }

    std::vector<MObject> toDelete;
    MItDag it(MItDag::kDepthFirst, MFn::kMesh);
    for (; !it.isDone(); it.next())
    {
        MDagPath meshPath;
        if (it.getPath(meshPath) != MS::kSuccess)
            continue;
        const std::string fp(meshPath.fullPathName().asChar());
        if (keepMeshPaths.find(fp) != keepMeshPaths.end())
            continue;

        MFnMesh fn(meshPath, &st);
        if (!st)
            continue;

        toDelete.push_back(meshPath.node());
    }

    for (MObject& obj : toDelete)
        MGlobal::deleteNode(obj);

    MSelectionList remainingMeshes;
    MItDag it2(MItDag::kDepthFirst, MFn::kMesh);
    for (; !it2.isDone(); it2.next())
    {
        MDagPath meshPath;
        if (it2.getPath(meshPath) != MS::kSuccess)
            continue;
        MFnMesh fn(meshPath, &st);
        if (!st)
            continue;
        remainingMeshes.add(meshPath);
    }

    if (remainingMeshes.length() == 0)
    {
        MGlobal::displayError("swgReformatMesh: no polygon meshes left after deletion.");
        return MS::kFailure;
    }

    std::vector<MDagPath> roots;
    collectUniqueRoots(remainingMeshes, roots);
    if (roots.empty())
    {
        MGlobal::displayError("swgReformatMesh: could not resolve mesh roots.");
        return MS::kFailure;
    }

    MString selectCmd = "select -clear;";
    for (const MDagPath& r : roots)
    {
        selectCmd += MString("select -add \"");
        selectCmd += r.fullPathName();
        selectCmd += "\";";
    }

    st = MGlobal::executeCommand(selectCmd, false, true);
    if (!st)
    {
        MGlobal::displayError("swgReformatMesh: failed to select mesh roots.");
        return st;
    }

    const MString groupMel =
        MString("group -name \"") + groupName.c_str() + "\" -world;";
    MStringArray groupResult;
    st = MGlobal::executeCommand(groupMel, groupResult, false, true);
    if (!st || groupResult.length() == 0)
    {
        MGlobal::displayError("swgReformatMesh: group failed (see Script Editor).");
        return MS::kFailure;
    }

    deleteUnusedShadingNodes();

    const MString selNew = MString("select -r \"") + groupResult[0] + "\";";
    MGlobal::executeCommand(selNew, false, true);

    MGlobal::displayInfo(
        MString("swgReformatMesh: kept ") + static_cast<int>(remainingMeshes.length())
        + " mesh shape(s) under \"" + groupResult[0]
        + "\". Other polygon meshes removed; unused shading nodes pruned where possible.");

    return MS::kSuccess;
}
