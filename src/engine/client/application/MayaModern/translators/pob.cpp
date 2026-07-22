#include "pob.h"
#include "SwgTranslatorNames.h"
#include "ImportPathResolver.h"
#include "ImportPob.h"
#include "MayaUtility.h"
#include "SwgImportTrace.h"

#include <maya/MArgList.h>
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
    return MString(swg_translator::kFilterPob);
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
    std::string pathStd = MayaUtility::fileObjectPathForIdentify(file);
    if (pathStd.empty())
    {
        const MString expanded = file.expandedFullName();
        if (expanded.length() > 0)
            pathStd = expanded.asChar();
    }
    pathStd = resolveWindowsMayaAbsolutePath(pathStd);
    if (pathStd.empty())
    {
        MGlobal::displayError("POB import: could not resolve file path from MFileObject.");
        return MS::kFailure;
    }
    SwgImportTrace::beginSession("PobTranslator::reader", pathStd.c_str());
    MArgList args;
    args.addArg(MString("-i"));
    args.addArg(MString(pathStd.c_str()));
    ImportPob importer;
    const MStatus status = importer.doIt(args);
    SwgImportTrace::stagef("PobTranslator::reader doIt %s", status ? "OK" : "FAILED");
    if (!status)
        MGlobal::displayError("POB import failed — see Script Editor / stderr for [ImportPob] logs.");
    SwgImportTrace::stage("PobTranslator::reader returning");
    return status;
}

MStatus PobTranslator::writer(const MFileObject& file, const MString& /*optionsString*/, MPxFileTranslator::FileAccessMode /*mode*/)
{
    const MString path = file.expandedFullName();
    MString cmd = "exportPob -i \"";
    cmd += path;
    cmd += "\"";
    return MGlobal::executeCommand(cmd);
}
