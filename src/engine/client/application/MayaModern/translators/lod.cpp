#include "lod.h"
#include "SwgTranslatorNames.h"

#include "../commands/ImportLodMesh.h"
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

void* LodTranslator::creator()
{
	return new LodTranslator();
}

MStatus LodTranslator::reader(const MFileObject& file, const MString& /*optionsString*/, MPxFileTranslator::FileAccessMode /*mode*/)
{
	const std::string pathStd = MayaUtility::fileObjectPathForIdentify(file);
	if (pathStd.empty())
	{
		MGlobal::displayError("LOD import: could not resolve file path from MFileObject.");
		return MS::kFailure;
	}
	MArgList args;
	args.addArg(MString("-i"));
	args.addArg(MString(pathStd.c_str()));
	ImportLodMesh importer;
	return importer.doIt(args);
}

MString LodTranslator::defaultExtension() const
{
	return "lod";
}

MString LodTranslator::filter() const
{
	return MString(swg_translator::kFilterLod);
}

MPxFileTranslator::MFileKind LodTranslator::identifyFile(const MFileObject& fileName, const char* buffer, short size) const
{
	const std::string pathStr = MayaUtility::fileObjectPathForIdentify(fileName);
	const int nameLength = static_cast<int>(pathStr.size());
	if (nameLength > 4)
	{
		if (!STRICMP(pathStr.c_str() + nameLength - 4, ".lod"))
		{
			if (buffer && size >= 12 && buffer[0] == 'F' && buffer[1] == 'O' && buffer[2] == 'R' && buffer[3] == 'M')
			{
				if (buffer[8] == 'M' && buffer[9] == 'L' && buffer[10] == 'O' && buffer[11] == 'D')
					return kIsMyFileType;
			}
			return kCouldBeMyFileType;
		}
	}
	return kNotMyFileType;
}
