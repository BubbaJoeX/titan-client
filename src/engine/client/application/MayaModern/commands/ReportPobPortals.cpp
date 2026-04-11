#include "ReportPobPortals.h"
#include "PobAuthoringShared.h"

#include <maya/MArgList.h>
#include <maya/MDagPath.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MGlobal.h>
#include <maya/MItDag.h>
#include <maya/MPlug.h>
#include <maya/MString.h>

#include <cstdio>
#include <iostream>
#include <map>
#include <string>
#include <vector>

void* ReportPobPortals::creator()
{
    return new ReportPobPortals();
}

MStatus ReportPobPortals::doIt(const MArgList& args)
{
    MStatus status;
    std::string rootPathStr;

    const unsigned argCount = args.length(&status);
    if (!status) return MS::kFailure;
    for (unsigned i = 0; i < argCount; ++i)
    {
        MString arg = args.asString(i, &status);
        if (!status) return MS::kFailure;
        if ((arg == "-r" || arg == "-root") && (i + 1) < argCount)
        {
            rootPathStr = args.asString(i + 1, &status).asChar();
            ++i;
        }
    }

    MDagPath rootPath;
    if (PobAuthoring::resolvePobRootFromSelectionOrArg(rootPathStr, rootPath) != MS::kSuccess)
    {
        std::cerr << "reportPobPortals: select any node under the POB, or use -root \"|buildingName\"" << std::endl;
        return MS::kFailure;
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

    MString report;
    report += "=== POB portal report: ";
    report += rootPath.partialPathName();
    report += " ===\n";
    int total = 0;
    int issues = 0;
    for (const auto& kv : byIndex)
    {
        const int idx = kv.first;
        const size_t cnt = kv.second.size();
        total += static_cast<int>(cnt);
        char line[512];
        if (cnt == 2u)
            sprintf(line, "Index %d: 2 instance(s) (typical CW/CCW pair) OK\n", idx);
        else if (cnt == 1u)
        {
            sprintf(line, "Index %d: 1 instance — MISSING opposite side (need same index, opposite -clockwise)\n", idx);
            ++issues;
        }
        else if (cnt == 0u)
            continue;
        else
        {
            sprintf(line, "Index %d: %zu instances — unexpected count (expected 2 per shared doorway)\n", idx, cnt);
            ++issues;
        }
        report += line;
        for (const auto& pathStr : kv.second)
        {
            report += "  ";
            report += pathStr.c_str();
            report += "\n";
        }
    }
    if (byIndex.empty())
        report += "(no transforms with buildingPortalIndex found)\n";
    else
    {
        char sum[256];
        if (issues > 0)
            sprintf(sum, "--- %d portal transform(s) across %zu index value(s) — %d index group(s) need review ---\n",
                    total, byIndex.size(), issues);
        else
            sprintf(sum, "--- %d portal transform(s) across %zu index value(s) ---\n", total, byIndex.size());
        report += sum;
    }

    MGlobal::displayInfo(report);
    return MS::kSuccess;
}
