#include "SetBaseDirectory.h"
#include "ExportDirectoryBootstrap.h"

#include <maya/MArgList.h>
#include <maya/MStatus.h>

#include <iostream>
#include <string>

void* SetBaseDirectory::creator()
{
    return new SetBaseDirectory();
}

MStatus SetBaseDirectory::doIt(const MArgList& args)
{
    MStatus status;
    const unsigned argCount = args.length(&status);
    if (!status || argCount != 1)
    {
        std::cerr << "setBaseDir: requires one string argument (base path)" << std::endl;
        return MS::kFailure;
    }

    MString mayaArg = args.asString(0, &status);
    if (!status)
    {
        std::cerr << "setBaseDir: failed to get argument" << std::endl;
        return MS::kFailure;
    }

    ExportDirectoryBootstrap::applyExportDataRoot(mayaArg.asChar());
    std::cerr << "setBaseDir: data root configured from [" << mayaArg.asChar() << "]" << std::endl;
    return MS::kSuccess;
}
