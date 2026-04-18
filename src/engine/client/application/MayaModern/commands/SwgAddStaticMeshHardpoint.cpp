#include "SwgAddStaticMeshHardpoint.h"

#include "MayaUtility.h"

#include <maya/MArgList.h>
#include <maya/MDagPath.h>
#include <maya/MFnTransform.h>
#include <maya/MGlobal.h>
#include <maya/MSelectionList.h>
#include <maya/MStatus.h>
#include <maya/MString.h>

#include <cctype>
#include <string>

namespace
{
    std::string sanitizeHpName(const char* raw)
    {
        std::string s = raw && raw[0] ? raw : "gun";
        std::string out;
        for (char c : s)
        {
            if (std::isalnum(static_cast<unsigned char>(c)) || c == '_')
                out += c;
        }
        if (out.empty())
            out = "gun";
        return out;
    }
}

void* SwgAddStaticMeshHardpoint::creator()
{
    return new SwgAddStaticMeshHardpoint();
}

MStatus SwgAddStaticMeshHardpoint::doIt(const MArgList& args)
{
    MStatus st;
    std::string hpNameStr = "gun";

    const unsigned n = args.length(&st);
    if (!st)
        return MS::kFailure;
    for (unsigned i = 0; i < n; ++i)
    {
        MString a = args.asString(i, &st);
        if (!st)
            return MS::kFailure;
        if ((a == "-n" || a == "-name") && i + 1 < n)
            hpNameStr = sanitizeHpName(args.asString(++i, &st).asChar());
    }

    MSelectionList sel;
    if (MGlobal::getActiveSelectionList(sel) != MS::kSuccess || sel.length() == 0)
    {
        MGlobal::displayError("swgAddStaticMeshHardpoint: select the static mesh transform or mesh (same as exportStaticMesh).");
        return MS::kFailure;
    }

    MDagPath dagPath;
    if (sel.getDagPath(0, dagPath) != MS::kSuccess)
    {
        MGlobal::displayError("swgAddStaticMeshHardpoint: invalid selection.");
        return MS::kFailure;
    }

    MDagPath meshPath;
    if (!MayaUtility::findFirstMeshShapeWithShadersInHierarchy(dagPath, meshPath))
    {
        if (!MayaUtility::findFirstMeshShapeInHierarchy(dagPath, meshPath))
        {
            MGlobal::displayError(
                "swgAddStaticMeshHardpoint: could not find exportable mesh under selection (need shaded mesh).");
            return MS::kFailure;
        }
    }

    MDagPath meshParent = meshPath;
    meshParent.pop();

    MFnTransform hpFn;
    hpFn.create(meshParent.node(), &st);
    if (!st)
    {
        MGlobal::displayError("swgAddStaticMeshHardpoint: failed to create hardpoint transform.");
        return MS::kFailure;
    }

    const MString nodeName = MString("hp_") + hpNameStr.c_str();
    hpFn.setName(nodeName);

    const MString mel = MString("string $p[] = `polyCube -w 0.5 -h 0.5 -d 0.5 -ax 0 1 0`;\n") + "parent $p[0] \"" +
                        hpFn.fullPathName() +
                        "\";\n"
                        "string $s[] = `listRelatives -f -s $p[0]`;\n"
                        "if (`size $s` > 0) {\n"
                        "  catchQuiet(`addAttr -ln swgExcludeFromStaticMeshExport -at bool -dv 1 $s[0]`);\n"
                        "  catchQuiet(`setAttr ($s[0] + \".swgExcludeFromStaticMeshExport\") 1`);\n"
                        "}\n";

    st = MGlobal::executeCommand(mel, false, true);
    if (!st)
    {
        MGlobal::displayWarning("swgAddStaticMeshHardpoint: hardpoint transform created but viewport cube failed (see Script Editor).");
        return MS::kSuccess;
    }

    MGlobal::displayInfo(MString("swgAddStaticMeshHardpoint: created ") + hpFn.fullPathName() +
                         " with 0.5m preview cube (excluded from .msh geometry).");
    return MS::kSuccess;
}
