#include "ImportStructure.h"
#include "ImportPathResolver.h"
#include "SwgTranslatorNames.h"

#include <maya/MArgList.h>
#include <maya/MGlobal.h>
#include <maya/MString.h>

#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

namespace
{
	std::string normalizeSlashes(std::string s)
	{
		for (auto& c : s)
			if (c == '\\') c = '/';
		return s;
	}

	bool endsWithIgnoreCase(const std::string& s, const std::string& suffix)
	{
		if (suffix.size() > s.size()) return false;
		for (size_t i = 0; i < suffix.size(); ++i)
		{
			const unsigned char a = static_cast<unsigned char>(s[s.size() - suffix.size() + i]);
			const unsigned char b = static_cast<unsigned char>(suffix[i]);
			if (std::tolower(a) != std::tolower(b)) return false;
		}
		return true;
	}

	std::string stripKnownExtensions(std::string s)
	{
		s = normalizeSlashes(std::move(s));
		while (!s.empty() && s.back() == '/') s.pop_back();
		static const char* ext[] = { ".pob", ".flr", ".msh", ".apt", ".lod" };
		for (const char* e : ext)
		{
			const std::string suf(e);
			if (endsWithIgnoreCase(s, suf))
			{
				s.resize(s.size() - suf.size());
				break;
			}
		}
		return s;
	}

	static std::string melEscapePath(std::string s)
	{
		std::string out;
		out.reserve(s.size() + 8);
		for (char c : s)
		{
			if (c == '\\' || c == '\"')
				out.push_back('\\');
			out.push_back(c);
		}
		return out;
	}

	static bool diskFileExists(const std::string& path)
	{
		if (path.empty()) return false;
		std::error_code ec;
		return fs::is_regular_file(fs::path(path), ec);
	}

	static void structureLog(const char* fmt, ...)
	{
		char buf[1024];
		va_list args;
		va_start(args, fmt);
#if defined(_MSC_VER)
		vsnprintf_s(buf, sizeof(buf), _TRUNCATE, fmt, args);
#else
		vsnprintf(buf, sizeof(buf), fmt, args);
#endif
		va_end(args);
		std::cerr << "[importStructure] " << buf << std::endl;
	}
}

void* ImportStructure::creator()
{
	return new ImportStructure();
}

MStatus ImportStructure::doIt(const MArgList& args)
{
	MStatus status;
	std::string input;
	bool forceFlr = false;
	std::string shaderTreePath;

	for (unsigned i = 0; i < args.length(&status); ++i)
	{
		if (!status) return MS::kFailure;
		MString a = args.asString(i, &status);
		if (!status) return MS::kFailure;
		if (a == "-i" && i + 1 < args.length(&status))
		{
			input = args.asString(++i, &status).asChar();
		}
		else if (a == "-flr")
		{
			forceFlr = true;
		}
		else if ((a == "-shader" || a == "-s") && i + 1 < args.length(&status))
		{
			shaderTreePath = args.asString(++i, &status).asChar();
		}
	}

	if (input.empty())
	{
		MGlobal::displayError("importStructure: use -i <appearance/path/basename>  (optional: -flr, -shader \"shader/foo/bar\")");
		return MS::kFailure;
	}

	std::string treeBase = stripKnownExtensions(std::move(input));
	treeBase = normalizeSlashes(treeBase);
	if (treeBase.find("appearance/") != 0)
		treeBase = "appearance/" + treeBase;

	const std::string pobDisk = resolveImportPath(treeBase + ".pob");
	const std::string flrDisk = resolveImportPath(treeBase + ".flr");
	const std::string mshSameDir = resolveImportPath(treeBase + ".msh");

	std::string basename = treeBase;
	{
		const size_t slash = basename.find_last_of('/');
		if (slash != std::string::npos)
			basename = basename.substr(slash + 1);
	}
	const std::string mshMeshDir = resolveImportPath(std::string("appearance/mesh/") + basename);

	bool importedSomething = false;
	const bool hadPob = diskFileExists(pobDisk);

	if (hadPob)
	{
		const std::string mel = std::string("importPob -i \"") + melEscapePath(treeBase + ".pob") + "\"";
		structureLog("POB: %s", pobDisk.c_str());
		status = MGlobal::executeCommand(MString(mel.c_str()), false, true);
		if (!status)
		{
			MGlobal::displayError("importStructure: importPob failed");
			return status;
		}
		importedSomething = true;
	}
	else
	{
		structureLog("No POB at %s", pobDisk.c_str());
	}

	if (diskFileExists(flrDisk) && (!hadPob || forceFlr))
	{
		const std::string melPath = melEscapePath(normalizeSlashes(flrDisk));
		MString fileCmd = "file -import -type \"";
		fileCmd += swg_translator::kTypeFlr;
		fileCmd += "\" -ra true \"";
		fileCmd += melPath.c_str();
		fileCmd += "\"";
		structureLog("FLR: %s", flrDisk.c_str());
		status = MGlobal::executeCommand(fileCmd, false, true);
		if (!status)
			MGlobal::displayWarning(MString("importStructure: FLR import failed: ") + flrDisk.c_str());
		else
			importedSomething = true;
	}

	if (diskFileExists(mshSameDir))
	{
		MString cmd = "importStaticMesh -i \"";
		cmd += melEscapePath(treeBase + ".msh").c_str();
		cmd += "\"";
		structureLog("MSH (co-located): %s", mshSameDir.c_str());
		status = MGlobal::executeCommand(cmd, false, true);
		if (status)
			importedSomething = true;
		else
			MGlobal::displayWarning(MString("importStructure: static mesh import failed: ") + mshSameDir.c_str());
	}
	else
	{
		static const char* meshExt[] = { ".msh", ".apt", ".lod" };
		bool meshOk = false;
		for (const char* ext : meshExt)
		{
			const std::string p = mshMeshDir + ext;
			if (!diskFileExists(p))
				continue;
			MString cmd = "importStaticMesh -i \"";
			cmd += melEscapePath(std::string("appearance/mesh/") + basename + ext).c_str();
			cmd += "\"";
			structureLog("MSH (mesh dir): %s", p.c_str());
			status = MGlobal::executeCommand(cmd, false, true);
			if (status)
			{
				importedSomething = true;
				meshOk = true;
				break;
			}
		}
		if (!meshOk)
			structureLog("No shell mesh at %s or appearance/mesh/%s.*", mshSameDir.c_str(), basename.c_str());
	}

	if (!shaderTreePath.empty())
	{
		MString sc = "importShader -i \"";
		sc += melEscapePath(normalizeSlashes(shaderTreePath)).c_str();
		sc += "\"";
		structureLog("Shader: %s", shaderTreePath.c_str());
		status = MGlobal::executeCommand(sc, false, true);
		if (status)
			importedSomething = true;
		else
			MGlobal::displayWarning(MString("importStructure: importShader failed: ") + shaderTreePath.c_str());
	}

	if (!importedSomething)
	{
		MGlobal::displayError("importStructure: no matching files (POB/FLR/MSH) under data root for the given path.");
		return MS::kFailure;
	}

	MGlobal::displayInfo("importStructure: done.");
	return MS::kSuccess;
}
