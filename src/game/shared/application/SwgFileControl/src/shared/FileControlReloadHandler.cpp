// ======================================================================
//
// FileControlReloadHandler.cpp
// copyright 2024 Sony Online Entertainment
//
// Client-side reload handler. When SwgFileControl runs on the client
// (Windows), it uses client-side reload APIs to refresh assets that
// have been updated by the file server.
//
// ======================================================================

#include "FirstSwgFileControl.h"
#include "FileControlReloadHandler.h"

#include "FileControlServer.h"
#include "HotReloadManager.h"

#include "sharedLog/Log.h"
#include "sharedUtility/DataTableManager.h"

#include <cstdio>
#include <cstring>
#include <fstream>

// ======================================================================

bool FileControlReloadHandler::ms_installed = false;
FileControlReloadHandler::TemplateReloadFunction FileControlReloadHandler::ms_templateReloadFunction = 0;

// ======================================================================

void FileControlReloadHandler::setTemplateReloadFunction(TemplateReloadFunction function)
{
	ms_templateReloadFunction = function;
}

// ======================================================================

void FileControlReloadHandler::install()
{
	if (ms_installed)
		return;

	ms_installed = true;
	LOG("FileControl", ("FileControlReloadHandler installed (client-side)"));
}

// ----------------------------------------------------------------------

void FileControlReloadHandler::remove()
{
	ms_installed = false;
}

// ======================================================================

bool FileControlReloadHandler::handleReloadNotify(const std::string & relativePath, const std::string & reloadCommand)
{
	if (!ms_installed)
		return false;

	LOG("FileControl", ("ReloadHandler: processing %s (command=%s)", relativePath.c_str(), reloadCommand.c_str()));

	HotReloadManager::FileType ft = HotReloadManager::detectFileType(relativePath);

	switch (ft)
	{
	case HotReloadManager::FT_JAVA_CLASS:
		{
			std::string scriptName = HotReloadManager::getScriptReloadName(relativePath);
			return reloadScript(scriptName);
		}

	case HotReloadManager::FT_DATATABLE_SERVER:
	case HotReloadManager::FT_DATATABLE_SHARED:
		{
			std::string dtName = HotReloadManager::getDatatableReloadName(relativePath);
			return reloadDatatable(dtName);
		}

	case HotReloadManager::FT_TEMPLATE_SERVER:
	case HotReloadManager::FT_TEMPLATE_SHARED:
		return reloadTemplate(relativePath);

	case HotReloadManager::FT_BUILDOUT:
		return reloadBuildout(relativePath);

	case HotReloadManager::FT_STRING_TABLE:
	case HotReloadManager::FT_TERRAIN:
	case HotReloadManager::FT_OTHER_IFF:
		return reloadGenericAsset(relativePath);

	case HotReloadManager::FT_UNKNOWN:
	default:
		LOG("FileControl", ("ReloadHandler: Unknown file type for %s", relativePath.c_str()));
		return false;
	}
}

// ======================================================================
// Client-side reload implementations
// ======================================================================

bool FileControlReloadHandler::reloadScript(const std::string & scriptName)
{
	LOG("FileControl", ("ReloadHandler: script reload %s (client-side - no-op, scripts are server-only)", scriptName.c_str()));
	return true;
}

// ----------------------------------------------------------------------

bool FileControlReloadHandler::reloadDatatable(const std::string & datatableName)
{
	LOG("FileControl", ("ReloadHandler: datatable reload %s", datatableName.c_str()));

	DataTableManager::reload(datatableName);
	LOG("FileControl", ("ReloadHandler: DataTableManager::reload(%s) called", datatableName.c_str()));

	return true;
}

// ----------------------------------------------------------------------

bool FileControlReloadHandler::reloadTemplate(const std::string & templatePath)
{
	LOG("FileControl", ("ReloadHandler: template reload %s", templatePath.c_str()));

	// The file is already on disk. Templates nobody has loaded yet are read from it on
	// next access; loaded ones are reloaded in place by the host's reload function
	// (the God Client's ActionsObjectTemplate::reloadClientTemplate).
	if (!ms_templateReloadFunction)
	{
		LOG("FileControl", ("ReloadHandler: template %s updated on disk, will be loaded on next access", templatePath.c_str()));
		return true;
	}

	// data/sku.0/sys.shared/compiled/game/object/foo.iff -> object/foo.iff
	std::string templateName = templatePath;
	for (std::string::size_type i = 0; i < templateName.size(); ++i)
	{
		if (templateName[i] == '\\')
			templateName[i] = '/';
	}
	const std::string::size_type gamePos = templateName.find("/compiled/game/");
	if (gamePos != std::string::npos)
		templateName = templateName.substr(gamePos + 15);

	return ms_templateReloadFunction(templateName);
}

// ----------------------------------------------------------------------

bool FileControlReloadHandler::reloadBuildout(const std::string & buildoutPath)
{
	LOG("FileControl", ("ReloadHandler: buildout reload %s", buildoutPath.c_str()));

	// Buildout data is loaded at scene creation time. The updated file
	// is now on disk and will be used when the scene is next loaded.
	// For live buildout updates, the GodClient would need to trigger
	// a scene rebuild.

	LOG("FileControl", ("ReloadHandler: buildout %s updated on disk", buildoutPath.c_str()));
	return true;
}

// ----------------------------------------------------------------------

bool FileControlReloadHandler::reloadGenericAsset(const std::string & assetPath)
{
	LOG("FileControl", ("ReloadHandler: generic asset reload %s", assetPath.c_str()));

	// Generic assets (string tables, terrain, other IFF) are updated on disk.
	// The engine will pick up changes on next access.

	return true;
}

// ======================================================================

bool FileControlReloadHandler::handleReceivedFile(const std::string & relativePath, const std::vector<unsigned char> & data)
{
	if (!ms_installed)
		return false;

	if (data.empty())
		return false;

	LOG("FileControl", ("ReloadHandler: received file %s (%d bytes)",
		relativePath.c_str(), static_cast<int>(data.size())));

	return true;
}

// ======================================================================
