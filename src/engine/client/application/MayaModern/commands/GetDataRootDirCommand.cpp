#include "GetDataRootDirCommand.h"
#include "SetDirectoryCommand.h"

#include <maya/MArgList.h>
#include <maya/MString.h>

void* GetDataRootDirCommand::creator()
{
    return new GetDataRootDirCommand();
}

MStatus GetDataRootDirCommand::doIt(const MArgList&)
{
    const char* dataRoot = SetDirectoryCommand::getDirectoryString(SetDirectoryCommand::DATA_ROOT_DIR_INDEX);
    setResult(MString(dataRoot ? dataRoot : ""));
    return MS::kSuccess;
}
