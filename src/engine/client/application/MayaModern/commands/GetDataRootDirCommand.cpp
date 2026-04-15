#include "GetDataRootDirCommand.h"
#include "ImportPathResolver.h"

#include <maya/MArgList.h>
#include <maya/MString.h>

void* GetDataRootDirCommand::creator()
{
    return new GetDataRootDirCommand();
}

MStatus GetDataRootDirCommand::doIt(const MArgList&)
{
    // Match resolveImportPath: env vars first, then setBaseDir / cfg (see ImportPathResolver.cpp).
    const std::string root = getImportDataRoot();
    setResult(MString(root.c_str()));
    return MS::kSuccess;
}
