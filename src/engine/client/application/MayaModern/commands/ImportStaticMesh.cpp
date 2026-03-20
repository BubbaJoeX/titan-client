#include "ImportStaticMesh.h"
#include "ImportPathResolver.h"

#include <maya/MArgList.h>
#include <maya/MGlobal.h>

#include <string>

void* ImportStaticMesh::creator()
{
    return new ImportStaticMesh();
}

MStatus ImportStaticMesh::doIt(const MArgList& args)
{
    MStatus status;
    std::string filename;
    std::string parentPath;

    const unsigned argCount = args.length(&status);
    if (!status) return MS::kFailure;

    for (unsigned i = 0; i < argCount; ++i)
    {
        MString argName = args.asString(i, &status);
        if (!status) return MS::kFailure;
        if (argName == "-i" && (i + 1) < argCount)
        {
            filename = args.asString(i + 1, &status).asChar();
            ++i;
        }
        else if (argName == "-parent" && (i + 1) < argCount)
        {
            parentPath = args.asString(i + 1, &status).asChar();
            ++i;
        }
    }

    if (filename.empty())
    {
        std::cerr << "ImportStaticMesh: missing -i <filename>" << std::endl;
        return MS::kFailure;
    }

    filename = resolveImportPath(filename);
    MString cmd = "importLodMesh -i \"";
    cmd += filename.c_str();
    cmd += "\" -parent \"";
    cmd += parentPath.c_str();
    cmd += "\"";
    return MGlobal::executeCommand(cmd);
}
