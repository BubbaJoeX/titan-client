// ======================================================================
//
// ImportPathResolver.cpp
//
// ======================================================================

#include "FirstMayaExporter.h"
#include "ImportPathResolver.h"
#include "PluginMain.h"
#include "SetDirectoryCommand.h"

#include <cstdlib>

// ======================================================================

namespace
{
	void stripTrailingAppearance(std::string &base)
	{
		// Ensure base does not end with "appearance/" to avoid doubling when path is "appearance/..."
		while (base.size() >= 10)
		{
			std::string::size_type pos = base.rfind("appearance/");
			if (pos != std::string::npos && pos + 10 == base.size())
			{
				base.resize(pos);
				while (!base.empty() && (base[base.size() - 1] == '/' || base[base.size() - 1] == '\\'))
					base.resize(base.size() - 1);
				if (!base.empty())
					base.push_back('/');
			}
			else
				break;
		}
	}

	bool isAbsolutePath(const std::string &path)
	{
		if (path.empty())
			return false;

		// Windows: "D:\" or "D:/"
		if (path.size() >= 2 && path[1] == ':')
			return true;

		// Unix-style or path starts with /
		if (path[0] == '/')
			return true;

		return false;
	}

	bool hasTreeFilePrefix(const std::string &path)
	{
		if (path.size() >= 10 && (path.compare(0, 10, "appearance/") == 0 || path.compare(0, 10, "appearance\\") == 0))
			return true;
		if (path.size() >= 7 && (path.compare(0, 7, "shader/") == 0 || path.compare(0, 7, "shader\\") == 0))
			return true;
		if (path.size() >= 7 && (path.compare(0, 7, "effect/") == 0 || path.compare(0, 7, "effect\\") == 0))
			return true;
		if (path.size() >= 8 && (path.compare(0, 8, "texture/") == 0 || path.compare(0, 8, "texture\\") == 0))
			return true;
		return false;
	}

	std::string getImportBaseDir()
	{
		const char *envDataRoot = getenv("TITAN_DATA_ROOT");
		if (envDataRoot && envDataRoot[0])
		{
			std::string base = envDataRoot;
			for (std::string::size_type i = 0; i < base.size(); ++i)
				if (base[i] == '\\') base[i] = '/';
			if (!base.empty() && base[base.size() - 1] != '/')
				base.push_back('/');
			stripTrailingAppearance(base);
			return base;
		}

		const char *envExportRoot = getenv("TITAN_EXPORT_ROOT");
		if (envExportRoot && envExportRoot[0])
		{
			std::string base = envExportRoot;
			for (std::string::size_type i = 0; i < base.size(); ++i)
				if (base[i] == '\\') base[i] = '/';
			if (!base.empty() && base[base.size() - 1] != '/')
				base.push_back('/');
			stripTrailingAppearance(base);
			return base;
		}

		const char *dataRootDir = SetDirectoryCommand::getDirectoryString(DATA_ROOT_DIR_INDEX);
		if (dataRootDir && dataRootDir[0])
		{
			std::string base = dataRootDir;
			for (std::string::size_type i = 0; i < base.size(); ++i)
				if (base[i] == '\\')
					base[i] = '/';
			if (!base.empty() && base[base.size() - 1] != '/')
				base.push_back('/');
			stripTrailingAppearance(base);
			return base;
		}

		const char *appearanceWriteDir = SetDirectoryCommand::getDirectoryString(APPEARANCE_WRITE_DIR_INDEX);
		if (appearanceWriteDir && appearanceWriteDir[0])
		{
			std::string base = appearanceWriteDir;
			// Check if it's an absolute path (has drive letter)
			if (base.size() >= 2 && base[1] == ':')
			{
				for (std::string::size_type i = 0; i < base.size(); ++i)
					if (base[i] == '\\')
						base[i] = '/';
				// Get parent directory (e.g. "D:/exported/appearance/" -> "D:/exported/")
				while (!base.empty() && (base[base.size() - 1] == '/' || base[base.size() - 1] == '\\'))
					base.resize(base.size() - 1);
				std::string::size_type lastSlash = base.find_last_of('/');
				if (lastSlash != std::string::npos)
					return base.substr(0, lastSlash + 1);
			}
		}

		return std::string();
	}
}

// ======================================================================

std::string resolveImportPath(const std::string &path)
{
	if (path.empty())
		return path;

	if (isAbsolutePath(path))
		return path;

	std::string baseDir = getImportBaseDir();
	if (baseDir.empty())
		return path;

	std::string treePath = path;
	for (std::string::size_type i = 0; i < treePath.size(); ++i)
		if (treePath[i] == '\\')
			treePath[i] = '/';

	if (!hasTreeFilePrefix(treePath))
		treePath = std::string("appearance/") + treePath;

	std::string resolved = baseDir + treePath;
	for (std::string::size_type i = 0; i < resolved.size(); ++i)
		if (resolved[i] == '\\')
			resolved[i] = '/';

	return resolved;
}

// ======================================================================
