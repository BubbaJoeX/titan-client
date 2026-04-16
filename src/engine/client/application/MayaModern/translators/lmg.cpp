#include "lmg.h"
#include "SwgTranslatorNames.h"

#include "../commands/ImportSkeletalMesh.h"
#include "MayaUtility.h"
#include "Iff.h"
#include "Tag.h"

#include <maya/MArgList.h>
#include <maya/MFileObject.h>
#include <maya/MGlobal.h>
#include <maya/MSelectionList.h>
#include <maya/MDagPath.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MPlug.h>

#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#define STRICMP _stricmp
#else
#define STRICMP strcasecmp
#endif

namespace {
	const Tag TAG_MLOD = TAG(M,L,O,D);
	const Tag TAG_CHLD = TAG(C,H,L,D);
}

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

MStatus LmgTranslator::writer(const MFileObject& file, const MString& /*optionsString*/, MPxFileTranslator::FileAccessMode /*mode*/)
{
	MString mpath = file.expandedFullName();
	const char* fileName = mpath.asChar();

	// Get selected objects and look for swgLmgChildren attribute
	MSelectionList sel;
	MGlobal::getActiveSelectionList(sel);
	
	std::vector<std::string> childPaths;
	
	// Look for LMG info on selected transforms
	for (unsigned i = 0; i < sel.length(); ++i)
	{
		MDagPath dp;
		if (sel.getDagPath(i, dp) != MS::kSuccess)
			continue;
		
		MFnDependencyNode fn(dp.node());
		if (fn.hasAttribute("swgLmgChildren"))
		{
			MPlug plug = fn.findPlug("swgLmgChildren", true);
			MString val;
			if (plug.getValue(val) == MS::kSuccess && val.length() > 0)
			{
				// Parse tab-separated paths
				std::string paths(val.asChar());
				size_t start = 0;
				while (start < paths.size())
				{
					size_t end = paths.find('\t', start);
					if (end == std::string::npos) end = paths.size();
					std::string p = paths.substr(start, end - start);
					if (!p.empty()) childPaths.push_back(p);
					start = end + 1;
				}
				break;
			}
		}
	}

	if (childPaths.empty())
	{
		MGlobal::displayError("LMG export: select a transform with swgLmgChildren attribute, or import an LMG first");
		return MS::kFailure;
	}

	// Write MLOD file (LMG uses same container format)
	Iff iff(64 * 1024);
	iff.insertForm(TAG_MLOD);
	{
		iff.insertForm(TAG_0000);
		{
			// INFO chunk
			iff.insertChunk(TAG_INFO);
			iff.insertChunkData(static_cast<int16>(childPaths.size()));
			iff.exitChunk(TAG_INFO);

			// Child references
			for (const std::string& path : childPaths)
			{
				iff.insertChunk(TAG_CHLD);
				iff.insertChunkString(path.c_str());
				iff.exitChunk(TAG_CHLD);
			}
		}
		iff.exitForm(TAG_0000);
	}
	iff.exitForm(TAG_MLOD);

	if (!iff.write(fileName))
	{
		MGlobal::displayError(MString("LMG export: failed to write ") + fileName);
		return MS::kFailure;
	}

	MGlobal::displayInfo(MString("LMG exported: ") + fileName + " (" + childPaths.size() + " children)");
	return MS::kSuccess;
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
