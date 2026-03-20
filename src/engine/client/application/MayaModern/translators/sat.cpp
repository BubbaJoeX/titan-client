#include "sat.h"
#include "../commands/ImportSat.h"

#include <maya/MFileObject.h>
#include <maya/MGlobal.h>
#include <maya/MArgList.h>
#include <maya/MPxFileTranslator.h>

#include <cstring>

#ifdef _WIN32
#define STRICMP _stricmp
#else
#define STRICMP strcasecmp
#endif

void* SatTranslator::creator()
{
    return new SatTranslator();
}

MString SatTranslator::defaultExtension() const
{
    return "sat";
}

MString SatTranslator::filter() const
{
    return "Skeletal Appearance Template | SWG (*.sat)";
}

MPxFileTranslator::MFileKind SatTranslator::identifyFile(const MFileObject& fileName, const char* /*buffer*/, short /*size*/) const
{
    const char* name = fileName.resolvedName().asChar();
    const int nameLength = static_cast<int>(strlen(name));
    if (nameLength > 4 && STRICMP(name + nameLength - 4, ".sat") == 0)
        return kIsMyFileType;
    return kNotMyFileType;
}

MStatus SatTranslator::reader(const MFileObject& file, const MString& /*optionsString*/, MPxFileTranslator::FileAccessMode /*mode*/)
{
    const MString path = file.expandedFullName();
    MArgList args;
    args.add("-i");
    args.add(path);

    ImportSat importer;
    const MStatus status = importer.doIt(args);
    if (!status)
    {
        MString err = "SAT_ATF reader failed to import: ";
        err += path;
        MGlobal::displayError(err);
    }
    return status;
}
