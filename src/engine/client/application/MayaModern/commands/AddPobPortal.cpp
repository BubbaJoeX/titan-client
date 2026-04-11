#include "AddPobPortal.h"
#include "PobAuthoringShared.h"

#include <maya/MArgList.h>
#include <maya/MDagPath.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnTransform.h>
#include <maya/MGlobal.h>
#include <maya/MObject.h>
#include <maya/MSelectionList.h>
#include <maya/MString.h>

#include <cstdio>
#include <iostream>
#include <string>

void* AddPobPortal::creator()
{
    return new AddPobPortal();
}

MStatus AddPobPortal::doIt(const MArgList& args)
{
    MStatus status;
    std::string parentPathStr;
    std::string portalNameOverride;
    std::string doorStyle;
    float width = 2.0f;
    float height = 2.5f;
    int portalIndex = -1;
    bool clockwise = false;
    int targetCell = -1;
    bool disabled = false;
    bool passable = true;
    bool addDoorHardpoint = false;
    int preset = -1;

    const unsigned argCount = args.length(&status);
    if (!status) return MS::kFailure;

    for (unsigned i = 0; i < argCount; ++i)
    {
        MString arg = args.asString(i, &status);
        if (!status) return MS::kFailure;
        if ((arg == "-p" || arg == "-parent") && (i + 1) < argCount)
        {
            parentPathStr = args.asString(i + 1, &status).asChar();
            ++i;
        }
        else if ((arg == "-w" || arg == "-width") && (i + 1) < argCount)
        {
            width = static_cast<float>(args.asDouble(i + 1, &status));
            ++i;
        }
        else if ((arg == "-h" || arg == "-height") && (i + 1) < argCount)
        {
            height = static_cast<float>(args.asDouble(i + 1, &status));
            ++i;
        }
        else if ((arg == "-index" || arg == "-i") && (i + 1) < argCount)
        {
            portalIndex = static_cast<int>(args.asInt(i + 1, &status));
            ++i;
        }
        else if (arg == "-clockwise" && (i + 1) < argCount)
        {
            clockwise = (args.asInt(i + 1, &status) != 0);
            ++i;
        }
        else if ((arg == "-target" || arg == "-targetCell") && (i + 1) < argCount)
        {
            targetCell = static_cast<int>(args.asInt(i + 1, &status));
            ++i;
        }
        else if (arg == "-disabled" && (i + 1) < argCount)
        {
            disabled = (args.asInt(i + 1, &status) != 0);
            ++i;
        }
        else if (arg == "-passable" && (i + 1) < argCount)
        {
            passable = (args.asInt(i + 1, &status) != 0);
            ++i;
        }
        else if ((arg == "-doorStyle") && (i + 1) < argCount)
        {
            doorStyle = args.asString(i + 1, &status).asChar();
            ++i;
        }
        else if ((arg == "-n" || arg == "-name") && (i + 1) < argCount)
        {
            portalNameOverride = args.asString(i + 1, &status).asChar();
            ++i;
        }
        else if (arg == "-preset" && (i + 1) < argCount)
        {
            preset = static_cast<int>(args.asInt(i + 1, &status));
            ++i;
        }
        else if (arg == "-doorHardpoint")
            addDoorHardpoint = true;
    }

    if (preset >= 0)
    {
        static const float kW[] = {2.0f, 3.0f, 1.0f, 2.0f, 4.0f};
        static const float kH[] = {2.5f, 2.5f, 2.5f, 3.2f, 3.0f};
        const int nPresets = static_cast<int>(sizeof(kW) / sizeof(kW[0]));
        if (preset < nPresets)
        {
            width = kW[preset];
            height = kH[preset];
        }
        else
        {
            std::cerr << "addPobPortal: -preset must be 0.." << (nPresets - 1) << " (default|wide|narrow|tall|garage)" << std::endl;
            return MS::kFailure;
        }
    }

    if (width <= 0.0f || height <= 0.0f)
    {
        std::cerr << "addPobPortal: width and height must be positive" << std::endl;
        return MS::kFailure;
    }

    MDagPath portalsPath;
    if (!parentPathStr.empty())
    {
        if (PobAuthoring::resolvePortalsGroupFromPathStr(parentPathStr, portalsPath) != MS::kSuccess)
        {
            std::cerr << "addPobPortal: -parent must be a transform named \"portals\"" << std::endl;
            return MS::kFailure;
        }
    }
    else
    {
        if (PobAuthoring::resolvePortalsGroupFromSelection(portalsPath) != MS::kSuccess)
        {
            std::cerr << "addPobPortal: select a cell or its \"portals\" group, or use -parent \"|path|to|portals\""
                      << std::endl;
            return MS::kFailure;
        }
    }

    MObject pobRoot = PobAuthoring::findPobRootFromPath(portalsPath);
    if (pobRoot.isNull())
    {
        std::cerr << "addPobPortal: could not find POB root (transform with rN cell children)" << std::endl;
        return MS::kFailure;
    }

    if (portalIndex < 0)
        portalIndex = PobAuthoring::maxBuildingPortalIndexUnderRoot(pobRoot) + 1;

    MObject portalsObj = portalsPath.node();
    char defaultName[32];
    if (portalNameOverride.empty())
    {
        sprintf(defaultName, "p%d", PobAuthoring::nextPortalChildSuffix(portalsObj));
        portalNameOverride = defaultName;
    }

    MDagPath portalTransformPath;
    status = PobAuthoring::createPortalDoorMeshAndParent(portalsObj, portalNameOverride, width, height, portalIndex,
                                                         clockwise, targetCell, disabled, passable, doorStyle,
                                                         addDoorHardpoint, portalTransformPath);
    if (!status)
    {
        std::cerr << "addPobPortal: failed to create portal mesh" << std::endl;
        return MS::kFailure;
    }

    MSelectionList outSel;
    outSel.add(portalTransformPath.node());
    MGlobal::setActiveSelectionList(outSel);

    char buf[384];
    sprintf(buf,
            "addPobPortal: created \"%s\" index=%d cw=%s targetCell=%d. Mate: same -index, opposite -clockwise.\n",
            portalNameOverride.c_str(), portalIndex, clockwise ? "1" : "0", targetCell);
    MGlobal::displayInfo(MString(buf));

    return MS::kSuccess;
}
