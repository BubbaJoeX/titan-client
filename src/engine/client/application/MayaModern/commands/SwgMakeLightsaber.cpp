#include "SwgMakeLightsaber.h"

#include "LsbAttributes.h"

#include <maya/MArgList.h>
#include <maya/MDagPath.h>
#include <maya/MGlobal.h>
#include <maya/MSelectionList.h>
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
}

void* SwgMakeLightsaber::creator()
{
    return new SwgMakeLightsaber();
}

MStatus SwgMakeLightsaber::doIt(const MArgList& args)
{
    MStatus st;
    std::string rootName = "swgLightsaber";
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
            rootName = sanitizeRootName(args.asString(++i, &st).asChar(), "swgLightsaber");
            if (!st)
                return MS::kFailure;
        }
    }

    MString mel;
    mel += "string $pc[] = `polyCylinder -r 0.05 -h 0.6 -sx 16 -sy 1 -sz 1 -ax 0 1 0 -n \"swg_lsb_hilt\"`;\n";
    mel += "string $grp = `group -name \"";
    mel += rootName.c_str();
    mel += "\" -world $pc[0]`;\n";
    mel += "select -r $grp;\n";

    st = MGlobal::executeCommand(mel, false, false);
    if (!st)
    {
        MGlobal::displayError("swgMakeLightsaber: MEL execution failed");
        return MS::kFailure;
    }

    MSelectionList sel;
    if (MGlobal::getActiveSelectionList(sel) != MS::kSuccess || sel.length() < 1)
    {
        MGlobal::displayError("swgMakeLightsaber: no group selected after build");
        return MS::kFailure;
    }
    MDagPath dag;
    if (!sel.getDagPath(0, dag))
        return MS::kFailure;

    st = lsbApplyDefaultTemplateAttributes(dag.node());
    if (!st)
    {
        MGlobal::displayError("swgMakeLightsaber: failed to add LSB attributes");
        return MS::kFailure;
    }

    MGlobal::displayInfo(MString("swgMakeLightsaber: created \"") + rootName.c_str()
                         + "\" (0.1m x 0.6m capped cylinder). Set paths in Extra Attributes, export with SwgLsb.");
    return MS::kSuccess;
}
