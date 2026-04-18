#include "SwgMakeVehicle.h"

#include <maya/MArgList.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnIkJoint.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnTransform.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MGlobal.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MString.h>
#include <maya/MVector.h>

#include <algorithm>
#include <cstdio>
#include <cctype>
#include <string>

namespace
{
    constexpr int kMinSeats = 1;
    constexpr int kMaxSeats = 16;

    std::string sanitizeRootName(const char* raw, const char* defVal)
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

    bool addStringAttr(MFnDependencyNode& node, const char* longName, const char* shortName, const char* value)
    {
        MStatus st;
        if (node.hasAttribute(longName))
        {
            MPlug p = node.findPlug(longName, true, &st);
            if (st && !p.isNull())
                p.setValue(MString(value));
            return true;
        }
        MFnTypedAttribute tAttr;
        MObject a = tAttr.create(longName, shortName, MFnData::kString);
        tAttr.setStorable(true);
        tAttr.setKeyable(true);
        st = node.addAttribute(a);
        if (!st)
            return false;
        MPlug p = node.findPlug(longName, true, &st);
        if (!st || p.isNull())
            return false;
        p.setValue(MString(value));
        return true;
    }

    bool addLongAttr(MFnDependencyNode& node, const char* longName, const char* shortName, int value)
    {
        MStatus st;
        if (node.hasAttribute(longName))
        {
            MPlug p = node.findPlug(longName, true, &st);
            if (st && !p.isNull())
                p.setValue(value);
            return true;
        }
        MFnNumericAttribute nAttr;
        MObject a = nAttr.create(MString(longName), MString(shortName), MFnNumericData::kLong, 0.0, &st);
        if (!st)
            return false;
        nAttr.setStorable(true);
        nAttr.setKeyable(true);
        st = node.addAttribute(a);
        if (!st)
            return false;
        MPlug p = node.findPlug(longName, true, &st);
        if (!st || p.isNull())
            return false;
        p.setValue(value);
        return true;
    }
}

void* SwgMakeVehicle::creator()
{
    return new SwgMakeVehicle();
}

MStatus SwgMakeVehicle::doIt(const MArgList& args)
{
    MStatus st;
    std::string rootName = "swgVehicle";
    int seatCount = 1;

    const unsigned n = args.length(&st);
    if (!st)
        return MS::kFailure;
    for (unsigned i = 0; i < n; ++i)
    {
        MString a = args.asString(i, &st);
        if (!st)
            return MS::kFailure;
        if ((a == "-n" || a == "-name") && i + 1 < n)
        {
            rootName = sanitizeRootName(args.asString(++i, &st).asChar(), "swgVehicle");
            if (!st)
                return MS::kFailure;
        }
        else if ((a == "-seats" || a == "-s") && i + 1 < n)
        {
            seatCount = static_cast<int>(args.asInt(++i, &st));
            if (!st)
                return MS::kFailure;
        }
    }

    seatCount = std::max(kMinSeats, std::min(kMaxSeats, seatCount));

    MFnTransform rootFn;
    MObject rootObj = rootFn.create(MObject::kNullObj, &st);
    if (!st)
    {
        MGlobal::displayError("swgMakeVehicle: failed to create root transform");
        return MS::kFailure;
    }
    rootFn.setName(MString(rootName.c_str()));

    MFnTransform geoFn;
    MObject geoObj = geoFn.create(rootObj, &st);
    if (!st)
    {
        MGlobal::displayError("swgMakeVehicle: failed to create geo group");
        return MS::kFailure;
    }
    geoFn.setName("swgVehicle_geo");

    MFnTransform skelFn;
    MObject skelObj = skelFn.create(rootObj, &st);
    if (!st)
    {
        MGlobal::displayError("swgMakeVehicle: failed to create skeleton group");
        return MS::kFailure;
    }
    skelFn.setName("swgVehicle_skeleton");

    MFnIkJoint jointFn;
    MObject rootJointObj = jointFn.create(skelObj, &st);
    if (!st)
    {
        MGlobal::displayError("swgMakeVehicle: failed to create root joint");
        return MS::kFailure;
    }
    jointFn.setName("swgVehicle_root");

    for (int si = 0; si < seatCount; ++si)
    {
        MFnIkJoint seatFn;
        MObject seatObj = seatFn.create(rootJointObj, &st);
        if (!st)
        {
            MGlobal::displayError("swgMakeVehicle: failed to create seat joint");
            return MS::kFailure;
        }
        char seatName[32];
        snprintf(seatName, sizeof(seatName), "seat_%d", si);
        seatFn.setName(MString(seatName));
        const double spread = 0.35 * static_cast<double>(si);
        seatFn.setTranslation(MVector(spread, 0.0, 0.0), MSpace::kTransform);
    }

    MFnTransform hpFn;
    hpFn.create(rootObj, &st);
    if (!st)
    {
        MGlobal::displayError("swgMakeVehicle: failed to create hardpoints group");
        return MS::kFailure;
    }
    hpFn.setName("hardpoints");

    MFnDependencyNode geoDep(geoObj, &st);
    if (!st)
        return MS::kFailure;
    if (!addStringAttr(geoDep, "swgVehicleBundlePaths", "swgVBP", ""))
    {
        MGlobal::displayError("swgMakeVehicle: failed to add swgVehicleBundlePaths");
        return MS::kFailure;
    }

    MFnDependencyNode rootDep(rootObj, &st);
    if (!st)
        return MS::kFailure;
    if (!addLongAttr(rootDep, "swgVehicleSeatCount", "swgVSC", seatCount))
    {
        MGlobal::displayError("swgMakeVehicle: failed to add swgVehicleSeatCount");
        return MS::kFailure;
    }

    const MString selectCmd = MString("select -r \"") + rootFn.fullPathName() + "\"";
    MGlobal::executeCommandOnIdle(selectCmd, true);

    char seatRange[64];
    snprintf(seatRange, sizeof(seatRange), "seat_0..seat_%d", seatCount - 1);
    MGlobal::displayInfo(
        MString("swgMakeVehicle: created |") + rootName.c_str()
        + "|swgVehicle_geo (swgVehicleBundlePaths), |" + rootName.c_str()
        + "|swgVehicle_skeleton|swgVehicle_root with " + seatRange + ", |" + rootName.c_str()
        + "|hardpoints. Not a ship rig (use swgMakeShip for spacecraft). seat_0 = primary. Export .mgn.");

    return MS::kSuccess;
}
