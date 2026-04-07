#include "lmg.h"
#include "SwgTranslatorNames.h"

#include "../commands/ImportSkeletalMesh.h"
#include "MayaUtility.h"

#include <maya/MArgList.h>
#include <maya/MFileObject.h>
#include <maya/MGlobal.h>

#include <cstring>
#include <string>

#ifdef _WIN32
#define STRICMP _stricmp
#else
#define STRICMP strcasecmp
#endif

void* LmgTranslator::creator()
{
	return new LmgTranslator();
}

MStatus LmgTranslator::reader(const MFileObject& file, const MString& /*optionsString*/, MPxFileTranslator::FileAccessMode /*mode*/)
{
	const std::string pathStd = MayaUtility::fileObjectPathForIdentify(file);
	if (pathStd.empty())
	{
		MGlobal::displayError("LMG import: could not resolve file path from MFileObject.");
		return MS::kFailure;
	}
	MArgList args;
	args.addArg(MString("-i"));
	args.addArg(MString(pathStd.c_str()));
	ImportSkeletalMesh importer;
	return importer.doIt(args);
}

MString LmgTranslator::defaultExtension() const
{
	return "lmg";
}

MString LmgTranslator::filter() const
{
	return MString(swg_translator::kFilterLmg);
}

MPxFileTranslator::MFileKind LmgTranslator::identifyFile(const MFileObject& fileName, const char* buffer, short size) const
{
	const std::string pathStr = MayaUtility::fileObjectPathForIdentify(fileName);
	const int nameLength = static_cast<int>(pathStr.size());
	if (nameLength > 4 && !STRICMP(pathStr.c_str() + nameLength - 4, ".lmg"))
	{
		if (buffer && size >= 12 && buffer[0] == 'F' && buffer[1] == 'O' && buffer[2] == 'R' && buffer[3] == 'M')
		{
			if (buffer[8] == 'S' && buffer[9] == 'K' && buffer[10] == 'M' && buffer[11] == 'G')
				return kIsMyFileType;
		}
		return kCouldBeMyFileType;
	}
	return kNotMyFileType;
}
