// ======================================================================
//
// ImportSat.cpp
// Copyright 2006 Sony Online Entertainment, Inc.
// All Rights Reserved.
//
// ======================================================================

#include "FirstMayaExporter.h"
#include "ImportPathResolver.h"
#include "ImportSat.h"

#include "Messenger.h"
#include "PluginMain.h"
#include "SetDirectoryCommand.h"

#include "sharedFile/Iff.h"

#include "maya/MArgList.h"
#include "maya/MGlobal.h"

#include <string>
#include <vector>

// ======================================================================

namespace
{
	Messenger *messenger;

	const Tag TAG_SMAT = TAG(S,M,A,T);
	const Tag TAG_MSGN = TAG(M,S,G,N);
	const Tag TAG_SKTI = TAG(S,K,T,I);

	const Tag TAG_V002 = TAG(0,0,0,2);
	const Tag TAG_V003 = TAG(0,0,0,3);
}

// ======================================================================

void ImportSat::install(Messenger *newMessenger)
{
	messenger = newMessenger;
}

// ----------------------------------------------------------------------

void ImportSat::remove()
{
	messenger = 0;
}

// ----------------------------------------------------------------------

void *ImportSat::creator()
{
	return new ImportSat();
}

// ======================================================================

ImportSat::ImportSat()
{
}

// ======================================================================

static std::string resolveTreeFilePath(const std::string &treeFilePath, const std::string &inputFilename)
{
	std::string baseDir;

	const char *envExportRoot = getenv("TITAN_EXPORT_ROOT");
	if (envExportRoot && envExportRoot[0])
	{
		baseDir = envExportRoot;
		if (!baseDir.empty() && baseDir[baseDir.size() - 1] != '\\' && baseDir[baseDir.size() - 1] != '/')
			baseDir.push_back('/');
	}
	else
	{
		const char *envDataRoot = getenv("TITAN_DATA_ROOT");
		if (envDataRoot && envDataRoot[0])
		{
			baseDir = envDataRoot;
			if (!baseDir.empty() && baseDir[baseDir.size() - 1] != '\\' && baseDir[baseDir.size() - 1] != '/')
				baseDir.push_back('/');
		}
		else
		{
			std::string normalizedInput = inputFilename;
			for (std::string::size_type i = 0; i < normalizedInput.size(); ++i)
			{
				if (normalizedInput[i] == '\\')
					normalizedInput[i] = '/';
			}

			std::string::size_type cgPos = normalizedInput.find("compiled/game/");
			if (cgPos != std::string::npos)
			{
				baseDir = normalizedInput.substr(0, cgPos + 14);
			}
			else
			{
				std::string::size_type firstSlash = treeFilePath.find_first_of("/\\");
				if (firstSlash != std::string::npos)
				{
					std::string firstComponent = "/" + treeFilePath.substr(0, firstSlash + 1);
					std::string::size_type pos = normalizedInput.find(firstComponent);
					if (pos != std::string::npos)
						baseDir = normalizedInput.substr(0, pos + 1);
				}
			}

			if (baseDir.empty())
			{
				const std::string::size_type lastSlash = normalizedInput.find_last_of('/');
				if (lastSlash != std::string::npos)
					baseDir = normalizedInput.substr(0, lastSlash + 1);
			}
		}
	}

	std::string resolved = baseDir + treeFilePath;

	for (std::string::size_type i = 0; i < resolved.size(); ++i)
	{
		if (resolved[i] == '\\')
			resolved[i] = '/';
	}

	return resolved;
}

// ----------------------------------------------------------------------

static bool hasExtension(const std::string &path, const char *ext)
{
	const std::string::size_type dotPos = path.find_last_of('.');
	if (dotPos == std::string::npos)
		return false;

	std::string pathExt = path.substr(dotPos);
	for (std::string::size_type i = 0; i < pathExt.size(); ++i)
	{
		if (pathExt[i] >= 'A' && pathExt[i] <= 'Z')
			pathExt[i] = static_cast<char>(pathExt[i] + ('a' - 'A'));
	}

	return pathExt == ext;
}

// ======================================================================

MStatus ImportSat::doIt(const MArgList &args)
{
	MStatus status;

	MESSENGER_LOG(("ImportSat: begin\n"));

	//-- parse arguments: expect -i <filename>
	std::string filename;
	{
		const unsigned argCount = args.length(&status);
		MESSENGER_REJECT_STATUS(!status, ("failed to get arg count\n"));

		bool haveFilename = false;

		for (unsigned i = 0; i < argCount; ++i)
		{
			MString argName = args.asString(i, &status);
			MESSENGER_REJECT_STATUS(!status, ("failed to get arg [%u]\n", i));

			if (argName == "-i")
			{
				MESSENGER_REJECT_STATUS(haveFilename, ("-i specified multiple times\n"));

				MString argValue = args.asString(i + 1, &status);
				MESSENGER_REJECT_STATUS(!status, ("failed to get filename after -i\n"));

				filename = argValue.asChar();
				++i;
				haveFilename = true;
			}
			else
			{
				MESSENGER_LOG_ERROR(("unknown argument [%s]\n", argName.asChar()));
				return MS::kFailure;
			}
		}

		MESSENGER_REJECT_STATUS(!haveFilename, ("missing required -i <filename> argument\n"));
	}

	filename = resolveImportPath(filename);
	MESSENGER_LOG(("ImportSat: opening file [%s]\n", filename.c_str()));

	//-- open the IFF
	Iff iff(filename.c_str());

	iff.enterForm(TAG_SMAT);

	//-- detect version
	const Tag versionTag = iff.getCurrentName();
	const bool isVersion2 = (versionTag == TAG_V002);
	const bool isVersion3 = (versionTag == TAG_V003);

	MESSENGER_REJECT_STATUS(!isVersion2 && !isVersion3, ("unsupported SAT version tag (expected 0002 or 0003)\n"));

	iff.enterForm(versionTag);

	//-- read INFO chunk
	int32 meshGeneratorCount     = 0;
	int32 skeletonTemplateCount  = 0;

	iff.enterChunk(TAG_INFO);
	{
		meshGeneratorCount    = iff.read_int32();
		skeletonTemplateCount = iff.read_int32();

		if (isVersion2)
		{
			char *asgPathname = iff.read_string();
			MESSENGER_LOG(("ImportSat: INFO asgPathname = [%s]\n", asgPathname));
			delete [] asgPathname;
		}
		else
		{
			const int8 createAnimationController = iff.read_int8();
			MESSENGER_LOG(("ImportSat: INFO createAnimationController = %d\n", static_cast<int>(createAnimationController)));
		}
	}
	iff.exitChunk(TAG_INFO);

	MESSENGER_LOG(("ImportSat: meshGeneratorCount=%d skeletonTemplateCount=%d\n", meshGeneratorCount, skeletonTemplateCount));

	//-- read MSGN chunk (mesh generator paths)
	std::vector<std::string> meshGeneratorPaths;
	iff.enterChunk(TAG_MSGN);
	{
		for (int32 i = 0; i < meshGeneratorCount; ++i)
		{
			char *s = iff.read_string();
			meshGeneratorPaths.push_back(s);
			delete [] s;
		}
	}
	iff.exitChunk(TAG_MSGN);

	//-- read SKTI chunk (skeleton template info: pairs of name + attachment transform)
	std::vector<std::string> skeletonTemplatePaths;
	std::vector<std::string> attachmentTransformNames;
	iff.enterChunk(TAG_SKTI);
	{
		for (int32 i = 0; i < skeletonTemplateCount; ++i)
		{
			char *skelName = iff.read_string();
			char *attachName = iff.read_string();
			skeletonTemplatePaths.push_back(skelName);
			attachmentTransformNames.push_back(attachName);
			delete [] skelName;
			delete [] attachName;
		}
	}
	iff.exitChunk(TAG_SKTI);

	//-- skip remaining optional chunks/forms (LATX, LDTB, SFSK, APAG)
	while (iff.getNumberOfBlocksLeft() > 0)
	{
		if (iff.isCurrentForm())
		{
			iff.enterForm();
			iff.exitForm(true);
		}
		else
		{
			iff.enterChunk();
			iff.exitChunk(true);
		}
	}

	iff.exitForm(versionTag);
	iff.exitForm(TAG_SMAT);

	//-- import skeleton templates
	for (int i = 0; i < static_cast<int>(skeletonTemplatePaths.size()); ++i)
	{
		const std::string &relativePath = skeletonTemplatePaths[static_cast<size_t>(i)];
		const std::string &attachTransform = attachmentTransformNames[static_cast<size_t>(i)];
		const std::string resolvedPath = resolveTreeFilePath(relativePath, filename);

		MESSENGER_LOG(("ImportSat: skeleton %d -> [%s] attach=[%s] (resolved: [%s])\n",
			i, relativePath.c_str(), attachTransform.c_str(), resolvedPath.c_str()));

		MString cmd = "importSkeleton -i \"";
		cmd += resolvedPath.c_str();
		cmd += "\"";

		status = MGlobal::executeCommand(cmd, true, true);
		if (!status)
		{
			MESSENGER_LOG_WARNING(("ImportSat: failed to import skeleton [%s]\n", resolvedPath.c_str()));
		}
	}

	//-- import mesh generators
	for (int i = 0; i < static_cast<int>(meshGeneratorPaths.size()); ++i)
	{
		const std::string &relativePath = meshGeneratorPaths[static_cast<size_t>(i)];
		const std::string resolvedPath = resolveTreeFilePath(relativePath, filename);

		MESSENGER_LOG(("ImportSat: mesh generator %d -> [%s] (resolved: [%s])\n",
			i, relativePath.c_str(), resolvedPath.c_str()));

		// Dispatch based on file extension or path content
		if (hasExtension(relativePath, ".lmg"))
		{
			MString cmd = "importLodMesh -i \"";
			cmd += resolvedPath.c_str();
			cmd += "\"";

			status = MGlobal::executeCommand(cmd, true, true);
			if (!status)
				MESSENGER_LOG_WARNING(("ImportSat: failed to import LOD mesh [%s]\n", resolvedPath.c_str()));
		}
		else
		{
			MString cmd = "importSkeletalMesh -i \"";
			cmd += resolvedPath.c_str();
			cmd += "\"";

			status = MGlobal::executeCommand(cmd, true, true);
			if (!status)
				MESSENGER_LOG_WARNING(("ImportSat: failed to import skeletal mesh [%s]\n", resolvedPath.c_str()));
		}
	}

	MESSENGER_LOG(("ImportSat: done, imported %d skeletons and %d mesh generators\n",
		static_cast<int>(skeletonTemplatePaths.size()),
		static_cast<int>(meshGeneratorPaths.size())));

	return MS::kSuccess;
}

// ======================================================================
