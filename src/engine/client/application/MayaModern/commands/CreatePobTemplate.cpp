#include "CreatePobTemplate.h"

#include <maya/MArgList.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnTransform.h>
#include <maya/MVector.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MGlobal.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MSelectionList.h>
#include <maya/MString.h>

#include <cctype>
#include <cstdio>
#include <iostream>
#include <string>

namespace
{
    static void ensureExternalReferenceAttr(MObject transformNode, const char* value)
    {
        MFnDependencyNode dn(transformNode);
        MPlug p = dn.findPlug("external_reference", true);
        if (p.isNull())
        {
            MFnTypedAttribute tAttr;
            MObject a = tAttr.create("external_reference", "extref", MFnData::kString);
            tAttr.setStorable(true);
            if (dn.addAttribute(a))
                p = dn.findPlug("external_reference", true);
        }
        if (!p.isNull())
            p.setValue(MString(value));
    }

    static std::string sanitizeRootName(const char* raw)
    {
        std::string s = raw ? raw : "new_pob";
        std::string out;
        for (char c : s)
        {
            if (std::isalnum(static_cast<unsigned char>(c)) || c == '_')
                out += c;
        }
        if (out.empty())
            out = "new_pob";
        return out;
    }
}

void* CreatePobTemplate::creator()
{
    return new CreatePobTemplate();
}

MStatus CreatePobTemplate::doIt(const MArgList& args)
{
    MStatus status;
    std::string rootNameStr = "new_pob";
    int cellCount = 2;
    double layoutSpacing = 0.0;

    const unsigned argCount = args.length(&status);
    if (!status) return MS::kFailure;

    for (unsigned i = 0; i < argCount; ++i)
    {
        MString arg = args.asString(i, &status);
        if (!status) return MS::kFailure;
        if ((arg == "-n" || arg == "-name") && (i + 1) < argCount)
        {
            rootNameStr = args.asString(i + 1, &status).asChar();
            ++i;
        }
        else if ((arg == "-c" || arg == "-cells") && (i + 1) < argCount)
        {
            cellCount = static_cast<int>(args.asInt(i + 1, &status));
            ++i;
            if (!status) return MS::kFailure;
        }
        else if ((arg == "-layoutSpacing" || arg == "-spacing") && (i + 1) < argCount)
        {
            layoutSpacing = args.asDouble(i + 1, &status);
            ++i;
            if (!status) return MS::kFailure;
        }
    }

    if (cellCount < 1) cellCount = 1;
    if (cellCount > 256) cellCount = 256;

    const std::string safeRoot = sanitizeRootName(rootNameStr.c_str());

    MFnTransform rootFn;
    MObject rootObj = rootFn.create(MObject::kNullObj, &status);
    if (!status)
    {
        std::cerr << "createPobTemplate: failed to create root transform" << std::endl;
        return MS::kFailure;
    }
    rootFn.setName(MString(safeRoot.c_str()));

    for (int ci = 0; ci < cellCount; ++ci)
    {
        char cellName[32];
        sprintf(cellName, "r%d", ci);

        MFnTransform cellFn;
        MObject cellObj = cellFn.create(rootObj, &status);
        if (!status) return MS::kFailure;
        cellFn.setName(MString(cellName));
        if (layoutSpacing != 0.0)
            cellFn.setTranslation(MVector(static_cast<double>(ci) * layoutSpacing, 0.0, 0.0), MSpace::kTransform);

        MFnTransform meshFn;
        MObject meshObj = meshFn.create(cellObj, &status);
        if (!status) return MS::kFailure;
        meshFn.setName("mesh");
        ensureExternalReferenceAttr(meshObj, "");

        MFnTransform portalsFn;
        MObject portalsObj = portalsFn.create(cellObj, &status);
        if (!status) return MS::kFailure;
        portalsFn.setName("portals");

        MFnTransform collisionFn;
        MObject collisionObj = collisionFn.create(cellObj, &status);
        if (!status) return MS::kFailure;
        collisionFn.setName("collision");

        MFnTransform floorFn;
        MObject floorObj = floorFn.create(collisionObj, &status);
        if (!status) return MS::kFailure;
        floorFn.setName("floor0");
        ensureExternalReferenceAttr(floorObj, "");
    }

    MSelectionList sel;
    sel.add(rootObj);
    MGlobal::setActiveSelectionList(sel);

    char infoBuf[512];
    sprintf(infoBuf,
            "createPobTemplate: created \"%s\" with %d cell(s)%s. Use addPobPortal on each \"portals\" group, "
            "reportPobPortals to audit pairs, then exportPob.\n",
            safeRoot.c_str(), cellCount,
            layoutSpacing != 0.0 ? " (laid out along +X)" : "");
    MGlobal::displayInfo(MString(infoBuf));

    return MS::kSuccess;
}
