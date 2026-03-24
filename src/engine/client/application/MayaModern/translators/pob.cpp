#include "pob.h"
#include "MayaUtility.h"

#include <maya/MFileObject.h>
#include <maya/MGlobal.h>
#include <maya/MPxFileTranslator.h>

#include <cstring>
#include <string>

#ifdef _WIN32
#define STRICMP _stricmp
#else
#define STRICMP strcasecmp
#endif

void* PobTranslator::creator()
{
    return new PobTranslator();
}

MString PobTranslator::defaultExtension() const
{
    return "pob";
}

MString PobTranslator::filter() const
{
    return "Portal Object - SWG (*.pob)";
}

MPxFileTranslator::MFileKind PobTranslator::identifyFile(const MFileObject& fileName, const char* /*buffer*/, short /*size*/) const
{
    const std::string pathStr = MayaUtility::fileObjectPathForIdentify(fileName);
    const int nameLength = static_cast<int>(pathStr.size());
    if (nameLength > 4 && STRICMP(pathStr.c_str() + nameLength - 4, ".pob") == 0)
        return kCouldBeMyFileType;
    return kNotMyFileType;
}

MStatus PobTranslator::reader(const MFileObject& file, const MString& /*optionsString*/, MPxFileTranslator::FileAccessMode /*mode*/)
{
    const std::string pathStd = MayaUtility::fileObjectPathForIdentify(file);
    if (pathStd.empty())
    {
        MGlobal::displayError("POB import: could not resolve file path from MFileObject.");
        return MS::kFailure;
    }
    const MString path(pathStd.c_str());
    MString cmd = "importPob -i \"";
    cmd += path;
    cmd += "\"";
    return MGlobal::executeCommand(cmd);
}

MStatus PobTranslator::writer(const MFileObject& file, const MString& /*optionsString*/, MPxFileTranslator::FileAccessMode /*mode*/)
{
    const MString path = file.expandedFullName();
    MString cmd = "exportPob -i \"";
    cmd += path;
    cmd += "\"";
    return MGlobal::executeCommand(cmd);
}
