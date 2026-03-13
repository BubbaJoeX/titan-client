// ======================================================================
//
// ExportManager.cpp
// copyright 2001 Sony Online Entertainment
// 
// ======================================================================

#include "FirstMayaExporter.h"
#include "ExportManager.h"

#include "ExporterLog.h"
#include "Messenger.h"
#include "PluginMain.h"
#include "SetDirectoryCommand.h"

#include <sstream>

//////////////////////////////////////////////////////////////////////////////////

Messenger* ExportManager::messenger;

//////////////////////////////////////////////////////////////////////////////////

void ExportManager::install(Messenger* newMessenger)
{
	messenger = newMessenger;
}

//////////////////////////////////////////////////////////////////////////////////

void ExportManager::remove()
{
	messenger = NULL;
}

//////////////////////////////////////////////////////////////////////////////////

bool ExportManager::validateTextureList(bool showGUI)
{
	UNREF(showGUI);
	return true;
}

//////////////////////////////////////////////////////////////////////////////////

void ExportManager::LaunchViewer(const std::string& asset)
{
	std::string viewerFullPath = SetDirectoryCommand::getDirectoryString(VIEWER_LOCATION_INDEX);
	int truncIndex = viewerFullPath.find_last_of("\\");
	std::string viewerPath = viewerFullPath.substr(0,truncIndex);

	std::stringstream strCommandStream;
	strCommandStream << "\"" << viewerFullPath.c_str() << "\" " << asset.c_str();

	MESSENGER_LOG(("\n\nLaunching viewer:\n"));
	MESSENGER_LOG(("... strCommandStream.str().c_str() = [%s]\n",strCommandStream.str().c_str()));
	MESSENGER_LOG(("... viewerPath.c_str() = [%s]\n\n",viewerPath.c_str()));

	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	ZeroMemory(&si,sizeof(si));
	si.cb = sizeof(si);
	CreateProcess(NULL,(LPTSTR)strCommandStream.str().c_str(),NULL,NULL,false,0,NULL,viewerPath.c_str(),&si,&pi);
}

// ======================================================================
