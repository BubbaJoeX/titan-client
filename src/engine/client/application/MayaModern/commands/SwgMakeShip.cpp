#include "SwgMakeShip.h"

#include <maya/MArgList.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnIkJoint.h>
#include <maya/MFnTransform.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MGlobal.h>
#include <maya/MPlug.h>
#include <maya/MObject.h>
#include <maya/MString.h>

#include <cctype>
#include <string>

namespace
{
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
}

void* SwgMakeShip::creator()
{
    return new SwgMakeShip();
}

MStatus SwgMakeShip::doIt(const MArgList& args)
{
    MStatus st;
    std::string rootName = "swgShip";
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
            rootName = sanitizeRootName(args.asString(++i, &st).asChar(), "swgShip");
            if (!st)
                return MS::kFailure;
        }
    }

    MFnTransform rootFn;
    MObject rootObj = rootFn.create(MObject::kNullObj, &st);
    if (!st)
    {
        MGlobal::displayError("swgMakeShip: failed to create root transform");
        return MS::kFailure;
    }
    rootFn.setName(MString(rootName.c_str()));

    MFnTransform geoFn;
    MObject geoObj = geoFn.create(rootObj, &st);
    if (!st)
        return MS::kFailure;
    geoFn.setName("swgShip_geo");

    MFnTransform skelFn;
    MObject skelObj = skelFn.create(rootObj, &st);
    if (!st)
        return MS::kFailure;
    skelFn.setName("swgShip_skeleton");

    MFnIkJoint jointFn;
    MObject jointObj = jointFn.create(skelObj, &st);
    if (!st)
    {
        MGlobal::displayError("swgMakeShip: failed to create joint");
        return MS::kFailure;
    }
    jointFn.setName("swgShip_root");

    MFnDependencyNode geoDep(geoObj, &st);
    if (!st)
        return MS::kFailure;
    if (!addStringAttr(geoDep, "swgShipBundlePaths", "swgSBP", ""))
    {
        MGlobal::displayError("swgMakeShip: failed to add swgShipBundlePaths");
        return MS::kFailure;
    }

    const MString selectCmd = MString("select -r \"") + rootFn.fullPathName() + "\"";
    MGlobal::executeCommandOnIdle(selectCmd, true);

    MGlobal::displayInfo(
        MString("swgMakeShip: created |") + rootName.c_str()
        + "|swgShip_geo (swgShipBundlePaths), |" + rootName.c_str()
        + "|swgShip_skeleton|swgShip_root. Parent skinned mesh under swgShip_geo, set bundle paths, export .mgn.");
    return MS::kSuccess;
}
