#include "ImportAnimation.h"
#include "ImportPathResolver.h"

#include <maya/MArgList.h>
#include <maya/MFileIO.h>
#include <maya/MGlobal.h>

#include <string>

void* ImportAnimation::creator()
{
    return new ImportAnimation();
}

MStatus ImportAnimation::doIt(const MArgList& args)
{
    MStatus status;
    std::string filename;

    const unsigned argCount = args.length(&status);
    if (!status) return MS::kFailure;

    for (unsigned i = 0; i < argCount; ++i)
    {
        MString argName = args.asString(i, &status);
        if (!status) return MS::kFailure;

        if (argName == "-i")
        {
            MString argValue = args.asString(i + 1, &status);
            if (!status)
            {
                std::cerr << "ImportAnimation: missing filename after -i" << std::endl;
                return MS::kFailure;
            }
            filename = argValue.asChar();
            ++i;
            break;
        }
    }

    if (filename.empty())
    {
        std::cerr << "ImportAnimation: missing -i <filename> argument" << std::endl;
        return MS::kFailure;
    }

    filename = resolveImportPath(filename);

    status = MFileIO::import(filename.c_str(), "", true);
    if (!status)
    {
        std::cerr << "ImportAnimation: failed to import " << filename << std::endl;
        return MS::kFailure;
    }

    return MS::kSuccess;
}
