#include "pob.h"

#include <maya/MFileObject.h>
#include <maya/MGlobal.h>
#include <maya/MPxFileTranslator.h>

#include <cstring>

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
    return "Portal Object | SWG (*.pob)";
}

MPxFileTranslator::MFileKind PobTranslator::identifyFile(const MFileObject& fileName, const char* /*buffer*/, short /*size*/) const
{
    const char* name = fileName.resolvedName().asChar();
    const int nameLength = static_cast<int>(strlen(name));
    if (nameLength > 4 && STRICMP(name + nameLength - 4, ".pob") == 0)
        return kIsMyFileType;
    return kNotMyFileType;
}

MStatus PobTranslator::reader(const MFileObject& file, const MString& /*optionsString*/, MPxFileTranslator::FileAccessMode /*mode*/)
{
    const MString path = file.expandedFullName();
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
