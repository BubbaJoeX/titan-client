/**
 * Skeleton import/export: reader delegates to importSkeleton (SKTM, SLOD, NAME redirect, 0001/0002).
 */

#include "skt.h"
#include "SwgTranslatorNames.h"

#include "../commands/ImportSkeleton.h"
#include "MayaUtility.h"

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

void* SktTranslator::creator()
{
	return new SktTranslator();
}

MStatus SktTranslator::reader(const MFileObject& file, const MString& /*optionsString*/, MPxFileTranslator::FileAccessMode /*mode*/)
{
	const std::string pathStd = MayaUtility::fileObjectPathForIdentify(file);
	if (pathStd.empty())
	{
		MGlobal::displayError("SKT import: could not resolve file path from MFileObject.");
		return MS::kFailure;
	}
	MArgList args;
	args.addArg(MString("-i"));
	args.addArg(MString(pathStd.c_str()));
	ImportSkeleton importer;
	return importer.doIt(args);
}

MStatus SktTranslator::writer(const MFileObject& file, const MString& /*options*/, MPxFileTranslator::FileAccessMode /*mode*/)
{
	MString path = file.expandedFullName();
	MString cmd = "exportSkeleton -path \"" + path + "\"";
	return MGlobal::executeCommand(cmd);
}

MString SktTranslator::defaultExtension() const
{
	return "skt";
}

MString SktTranslator::filter() const
{
	return MString(swg_translator::kFilterSkt);
}

MPxFileTranslator::MFileKind SktTranslator::identifyFile(const MFileObject& fileName, const char* buffer, short size) const
{
	const std::string pathStr = MayaUtility::fileObjectPathForIdentify(fileName);
	const int nameLength = static_cast<int>(pathStr.size());
	if (nameLength > 4)
	{
		const char* ext = pathStr.c_str() + nameLength - 4;
		if (!STRICMP(ext, ".skt") || !STRICMP(ext, ".lod"))
			return kCouldBeMyFileType;
	}
	if (buffer && size >= 12 && buffer[0] == 'F' && buffer[1] == 'O' && buffer[2] == 'R' && buffer[3] == 'M')
	{
		if (size >= 12 && buffer[8] == 'S' && buffer[9] == 'K' && buffer[10] == 'T' && buffer[11] == 'M')
			return kIsMyFileType;
		if (size >= 12 && buffer[8] == 'S' && buffer[9] == 'L' && buffer[10] == 'O' && buffer[11] == 'D')
			return kIsMyFileType;
	}
	return kNotMyFileType;
}
