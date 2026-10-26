#include "sat.h"
#include "SwgTranslatorNames.h"
#include "../commands/ExportSat.h"
#include "../commands/ImportSat.h"
#include "MayaUtility.h"

#include <maya/MFileObject.h>
#include <maya/MGlobal.h>
#include <maya/MArgList.h>
#include <maya/MPxFileTranslator.h>

#include <cstring>
#include <string>

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
    return MString(swg_translator::kFilterSat);
}

MPxFileTranslator::MFileKind SatTranslator::identifyFile(const MFileObject& fileName, const char* buffer, short size) const
{
    const bool smatHeader = buffer && size >= 12
        && buffer[0] == 'F' && buffer[1] == 'O' && buffer[2] == 'R' && buffer[3] == 'M'
        && buffer[8] == 'S' && buffer[9] == 'M' && buffer[10] == 'A' && buffer[11] == 'T';
    if (smatHeader)
        return kIsMyFileType;

    const std::string pathStr = MayaUtility::fileObjectPathForIdentify(fileName);
    const int nameLength = static_cast<int>(pathStr.size());
    if (nameLength > 4 && STRICMP(pathStr.c_str() + nameLength - 4, ".sat") == 0)
    {
        // Claim SWG .sat firmly. Maya's built-in SAT_ATF (ACIS) also uses *.sat and
        // pops 2–3 parse errors if it wins the File>Import type picker.
        return kIsMyFileType;
    }

    return kNotMyFileType;
}

MStatus SatTranslator::reader(const MFileObject& file, const MString& /*optionsString*/, MPxFileTranslator::FileAccessMode /*mode*/)
{
    SwgFileImportScope importScope;
    MGlobal::displayInfo("SwgMayaEditor 20260831k: File>Import SwgSat (not Maya ACIS SAT)");

    const std::string pathStd = MayaUtility::fileObjectPathForIdentify(file);
    if (pathStd.empty())
    {
        MGlobal::displayInfo("SAT import: could not resolve file path from MFileObject.");
        return MS::kSuccess;
    }
    const MString path(pathStd.c_str());
    MArgList args;
    args.addArg(MString("-i"));
    args.addArg(path);

    ImportSat importer;
    const MStatus status = importer.doIt(args);
    if (!status)
        MGlobal::displayInfo(MString("SWG SAT import finished with errors: ") + path);
    // Always succeed so Maya's File Import dialog cannot deadlock on displayError.
    return MS::kSuccess;
}

MStatus SatTranslator::writer(const MFileObject& file, const MString& /*optionsString*/, MPxFileTranslator::FileAccessMode /*mode*/)
{
    const std::string pathStd = MayaUtility::fileObjectPathForIdentify(file);
    if (pathStd.empty())
    {
        MGlobal::displayError("SAT export: could not resolve output path from MFileObject.");
        return MS::kFailure;
    }
    MArgList args;
    args.addArg(MString("-o"));
    args.addArg(MString(pathStd.c_str()));
    ExportSat exporter;
    return exporter.doIt(args);
}
