#include "PrepareStaticMeshExport.h"

#include <maya/MArgList.h>
#include <maya/MDagPath.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnMesh.h>
#include <maya/MGlobal.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MItSelectionList.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MSelectionList.h>
#include <maya/MString.h>

#include <iostream>
#include <set>
#include <vector>

namespace
{
    void collectMeshShapesFromSelection(MSelectionList& outMeshShapes)
    {
        outMeshShapes.clear();
        MSelectionList sel;
        if (MGlobal::getActiveSelectionList(sel) != MS::kSuccess)
            return;

        for (unsigned i = 0; i < sel.length(); ++i)
        {
            MDagPath path;
            if (sel.getDagPath(i, path) != MS::kSuccess)
                continue;

            if (path.hasFn(MFn::kMesh))
            {
                outMeshShapes.add(path);
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
                    outMeshShapes.add(meshPath);
                }
            }
        }
    }

    MString validateMeshExportReadiness(const MDagPath& meshPath, int& outWarnings)
    {
        outWarnings = 0;
        MStatus st;
        MFnMesh meshFn(meshPath, &st);
        if (!st)
            return MString("Not a mesh: ") + meshPath.fullPathName();

        MString report;
        MStringArray uvSetNames;
        meshFn.getUVSetNames(uvSetNames);
        if (uvSetNames.length() == 0)
        {
            report += "ERROR: Mesh has no UV sets.\n";
            ++outWarnings;
        }
        else
        {
            report += MString("UV set for exportStaticMesh: current / file uvSetName / map1 (not \"first in list\").\n");
        }

        MObjectArray shaderObjs;
        MIntArray faceToShader;
        if (!meshFn.getConnectedShaders(meshPath.instanceNumber(), shaderObjs, faceToShader))
        {
            report += "ERROR: getConnectedShaders failed.\n";
            ++outWarnings;
        }
        else
        {
            std::set<unsigned> sgIndices;
            for (unsigned fi = 0; fi < faceToShader.length(); ++fi)
            {
                const int idx = faceToShader[fi];
                if (idx >= 0 && static_cast<unsigned>(idx) < shaderObjs.length())
                    sgIndices.insert(static_cast<unsigned>(idx));
            }

            for (unsigned idx : sgIndices)
            {
                MFnDependencyNode sgFn(shaderObjs[idx], &st);
                if (!st)
                    continue;
                MPlug plug = sgFn.findPlug("swgShaderPath", true, &st);
                MString val;
                if (plug.isNull() || !st || plug.getValue(val) != MS::kSuccess || val.length() == 0)
                {
                    report += MString("INFO: Shading group \"") + sgFn.name()
                        + "\" has no swgShaderPath - exportStaticMesh will auto-create a .sht from the diffuse network, "
                          "swgTexturePath, or the prototype shader (set swgHueable=1 for shaderPrototypeHueableSht).\n";
                    ++outWarnings;
                }
            }
        }

        if (uvSetNames.length() > 0)
        {
            MString exportUvSet;
            if (meshFn.getCurrentUVSetName(exportUvSet) != MS::kSuccess || exportUvSet.length() == 0)
                exportUvSet = uvSetNames[0];
            int facesMissingUv = 0;
            MItMeshPolygon polyIt(meshPath, MObject::kNullObj, &st);
            if (st)
            {
                for (; !polyIt.isDone(); polyIt.next())
                {
                    const int vc = polyIt.polygonVertexCount(&st);
                    bool ok = true;
                    for (int v = 0; v < vc; ++v)
                    {
                        float2 uv{};
                        if (polyIt.getUV(static_cast<unsigned>(v), uv, &exportUvSet) != MS::kSuccess)
                        {
                            ok = false;
                            break;
                        }
                    }
                    if (!ok)
                        ++facesMissingUv;
                }
            }
            if (facesMissingUv > 0)
            {
                report += MString("WARN: ") + facesMissingUv + " faces have incomplete UVs in \"" + exportUvSet + "\".\n";
                ++outWarnings;
            }
        }

        return report;
    }

    bool fixUvSetsForExport(MDagPath meshPath, MString& logOut)
    {
        MStatus st;
        MFnMesh meshFn(meshPath, &st);
        if (!st)
            return false;

        MStringArray names;
        meshFn.getUVSetNames(names);
        if (names.length() == 0)
            return false;

        const MString shapeQuoted = MString("\"") + meshPath.fullPathName() + "\"";

        auto hasMap1 = [&]() -> bool {
            for (unsigned i = 0; i < names.length(); ++i)
                if (names[i] == MString("map1"))
                    return true;
            return false;
        };

        meshFn.getUVSetNames(names);

        // Ensure map1 exists and is populated when another set holds the real layout.
        if (!hasMap1())
        {
            const MString& src = names[0];
            MString cmd = "polyUVSet -copy -uvSet \"";
            cmd += src;
            cmd += "\" -newUVSet \"map1\" ";
            cmd += shapeQuoted;
            cmd += ";";
            st = MGlobal::executeCommand(cmd, false, true);
            if (st != MS::kSuccess)
            {
                logOut += MString("fixUvSet: polyUVSet -copy failed for ") + meshPath.fullPathName() + "\n";
                return false;
            }
            logOut += MString("Created map1 from UV set \"") + src + "\".\n";
            meshFn.setObject(meshPath);
            meshFn.setCurrentUVSetName(MString("map1"));
            return true;
        }

        const int map1Count = meshFn.numUVs(MString("map1"), &st);
        if (map1Count == 0 && names.length() > 1)
        {
            MString src;
            for (unsigned i = 0; i < names.length(); ++i)
            {
                if (names[i] == MString("map1"))
                    continue;
                if (meshFn.numUVs(names[i], &st) > 0)
                {
                    src = names[i];
                    break;
                }
            }
            if (src.length() > 0)
            {
                MString cmd = "polyUVSet -delete -uvSet \"map1\" ";
                cmd += shapeQuoted;
                cmd += "; polyUVSet -copy -uvSet \"";
                cmd += src;
                cmd += "\" -newUVSet \"map1\" ";
                cmd += shapeQuoted;
                cmd += ";";
                st = MGlobal::executeCommand(cmd, false, true);
                if (st != MS::kSuccess)
                {
                    logOut += MString("fixUvSet: failed to rebuild map1 from \"") + src + "\".\n";
                    return false;
                }
                logOut += MString("Replaced empty map1 from \"") + src + "\".\n";
                meshFn.setObject(meshPath);
                meshFn.setCurrentUVSetName(MString("map1"));
                return true;
            }
        }

        meshFn.setCurrentUVSetName(MString("map1"));
        logOut += "UV sets OK (map1 present).\n";
        return true;
    }
}

void* PrepareStaticMeshExport::creator()
{
    return new PrepareStaticMeshExport();
}

MStatus PrepareStaticMeshExport::doIt(const MArgList& args)
{
    MStatus status;
    bool doCombine = false;
    bool validateOnly = false;
    bool fixUv = false;

    for (unsigned i = 0; i < args.length(&status); ++i)
    {
        if (!status)
            return MS::kFailure;
        MString a = args.asString(i, &status);
        if (!status)
            return MS::kFailure;
        if (a == "-combine" || a == "-c")
            doCombine = true;
        else if (a == "-validateOnly" || a == "-v")
            validateOnly = true;
        else if (a == "-fixUvSet" || a == "-f")
            fixUv = true;
    }

    MSelectionList meshShapes;
    collectMeshShapesFromSelection(meshShapes);

    if (meshShapes.length() == 0)
    {
        MGlobal::displayError("swgPrepareStaticMeshExport: select one or more polygon meshes (or their transforms).");
        return MS::kFailure;
    }

    if (doCombine)
    {
        if (meshShapes.length() < 2)
        {
            MGlobal::displayError("swgPrepareStaticMeshExport: -combine requires at least two mesh shapes selected.");
            return MS::kFailure;
        }

        MString cmd = "select -r ";
        for (unsigned i = 0; i < meshShapes.length(); ++i)
        {
            MDagPath dp;
            meshShapes.getDagPath(i, dp);
            cmd += MString("\"") + dp.fullPathName() + MString("\" ");
        }
        // mergeUVSets 1 = merge by UV set name (map1 + map1 -> single map1); preserves per-face shader bins.
        cmd += "; polyUnite -ch 1 -mergeUVSets 1;";
        status = MGlobal::executeCommand(cmd, false, true);
        if (status != MS::kSuccess)
        {
            MGlobal::displayError("swgPrepareStaticMeshExport: polyUnite failed (see Script Editor).");
            return MS::kFailure;
        }

        MStringArray selResult;
        MGlobal::executeCommand(MString("ls -sl"), selResult, false, true);
        if (selResult.length() > 0)
            MGlobal::executeCommand(MString("select -r \"") + selResult[0] + MString("\""), false, true);

        collectMeshShapesFromSelection(meshShapes);
        if (meshShapes.length() != 1)
        {
            MGlobal::displayWarning(
                "swgPrepareStaticMeshExport: combine finished but could not resolve a single result mesh; validate selection manually.");
        }

        MGlobal::displayInfo("swgPrepareStaticMeshExport: combined meshes with mergeUVSets=1 (by name). Select result and run exportStaticMesh.");
    }

    if (meshShapes.length() != 1)
    {
        if (validateOnly && !doCombine)
        {
            int totalWarn = 0;
            for (unsigned i = 0; i < meshShapes.length(); ++i)
            {
                MDagPath dp;
                meshShapes.getDagPath(i, dp);
                int w = 0;
                MString r = validateMeshExportReadiness(dp, w);
                totalWarn += w;
                MGlobal::displayInfo(MString("[") + dp.partialPathName() + MString("]\n") + r);
                std::cerr << "[swgPrepareStaticMeshExport] " << dp.fullPathName().asChar() << "\n" << r.asChar() << std::endl;
            }
            if (totalWarn > 0)
                MGlobal::displayWarning("swgPrepareStaticMeshExport: validation reported warnings/errors (see Script Editor).");
            return MS::kSuccess;
        }

        MGlobal::displayError(
            "swgPrepareStaticMeshExport: need exactly one mesh after preparation (use -combine, or select one combined mesh).");
        return MS::kFailure;
    }

    MDagPath meshPath;
    meshShapes.getDagPath(0, meshPath);

    if (fixUv && !validateOnly)
    {
        MString fixLog;
        if (!fixUvSetsForExport(meshPath, fixLog))
            MGlobal::displayWarning(fixLog);
        else
            MGlobal::displayInfo(fixLog);
    }

    int warnings = 0;
    MString report = validateMeshExportReadiness(meshPath, warnings);
    MGlobal::displayInfo(report);
    std::cerr << "[swgPrepareStaticMeshExport] " << meshPath.fullPathName().asChar() << "\n" << report.asChar() << std::endl;

    if (warnings > 0)
        MGlobal::displayWarning("swgPrepareStaticMeshExport: fix swgShaderPath / UVs as needed, or run with -fixUvSet.");

    return MS::kSuccess;
}
