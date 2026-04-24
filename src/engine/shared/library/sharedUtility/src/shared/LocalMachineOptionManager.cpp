// ======================================================================
//
// LocalMachineOptionManager.cpp
// asommers
//
// copyright 2003, sony online entertainment
//
// ======================================================================

#include "sharedUtility/FirstSharedUtility.h"
#include "sharedUtility/LocalMachineOptionManager.h"

#include "sharedFoundation/ExitChain.h"
#include "sharedUtility/OptionManager.h"

#if defined(_WIN32)
#include <windows.h>
#endif

// ======================================================================
// LocalMachineOptionManagerNamespace
// ======================================================================

namespace LocalMachineOptionManagerNamespace
{
	char const * const cms_fileName = "local_machine_options.iff";

	bool               ms_installed;
	OptionManager *    ms_optionManager;
}

using namespace LocalMachineOptionManagerNamespace;

// ======================================================================
#if defined(_WIN32)
// Iff / OptionManager::load can still hit legacy paths or a bad local_device iff;
// 0xC0000005 here must not kill startup — recover with empty in-memory options.
namespace
{
bool loadLocalOptionsNoThrow (OptionManager *om, char const *path)
{
	__try
	{
		om->load (path);
		return true;
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		::OutputDebugStringA(
			"[Titan] LocalMachineOptionManager: OptionManager::load SEH; "
			"leaking partial OptionManager, using empty options (check local_machine_options.iff)\r\n");
		::OutputDebugStringA(
			"[Titan] LocalMachineOptionManager: If the client crashes shortly after, rename or delete "
			"local_machine_options.iff and retry — prior AV during load may have corrupted the heap.\r\n");
		return false;
	}
}
}
#endif
// ======================================================================
// STATIC PUBLIC LocalMachineOptionManager
// ======================================================================

void LocalMachineOptionManager::install ()
{
	DEBUG_FATAL (ms_installed, ("LocalMachineOptionManager::install: already installed"));
	ms_installed = true;

#if defined(_WIN32)
	::OutputDebugStringA ("[Titan] LocalMachineOptionManager: new OptionManager + load (local_machine_options.iff)\r\n");
#endif
	ms_optionManager = new OptionManager;
#if defined(_WIN32)
	if (!loadLocalOptionsNoThrow (ms_optionManager, cms_fileName))
	{
		// Must not delete om — may be heap-corrupt; leak it and start fresh.
		ms_optionManager = new OptionManager;
	}
	::OutputDebugStringA ("[Titan] LocalMachineOptionManager: install path done\r\n");
#else
	ms_optionManager->load (cms_fileName);
#endif

	ExitChain::add (remove, "LocalMachineOptionManager::remove");
}

// ----------------------------------------------------------------------

void LocalMachineOptionManager::remove ()
{
	DEBUG_FATAL (!ms_installed, ("LocalMachineOptionManager::remove: not installed"));
	ms_installed = false;

	delete ms_optionManager;
	ms_optionManager = 0;
}

// ----------------------------------------------------------------------

void LocalMachineOptionManager::save ()
{
	DEBUG_FATAL (!ms_installed, ("LocalMachineOptionManager::save: not installed"));
#if defined(_WIN32)
	::OutputDebugStringA("[Titan] LocalMachineOptionManager: save() -> (local_machine_options.iff)\r\n");
#endif
	ms_optionManager->save (cms_fileName);
#if defined(_WIN32)
	::OutputDebugStringA("[Titan] LocalMachineOptionManager: save() <-\r\n");
#endif
}

// ----------------------------------------------------------------------

void LocalMachineOptionManager::registerOption (bool & variable, char const * const section, char const * const name, const int version)
{
	DEBUG_FATAL (!ms_installed, ("LocalMachineOptionManager::registerOption: not installed"));
	ms_optionManager->registerOption (variable, section, name, version);
}

// ----------------------------------------------------------------------

void LocalMachineOptionManager::registerOption (float & variable, char const * const section, char const * const name, const int version)
{
	DEBUG_FATAL (!ms_installed, ("LocalMachineOptionManager::registerOption: not installed"));
	ms_optionManager->registerOption (variable, section, name, version);
}

// ----------------------------------------------------------------------

void LocalMachineOptionManager::registerOption (int & variable, char const * const section, char const * const name, const int version)
{
	DEBUG_FATAL (!ms_installed, ("LocalMachineOptionManager::registerOption: not installed"));
	ms_optionManager->registerOption (variable, section, name, version);
}

// ----------------------------------------------------------------------

void LocalMachineOptionManager::registerOption (std::string & variable, char const * const section, char const * const name, const int version)
{
	DEBUG_FATAL (!ms_installed, ("LocalMachineOptionManager::registerOption: not installed"));
	ms_optionManager->registerOption (variable, section, name, version);
}

// ----------------------------------------------------------------------

void LocalMachineOptionManager::registerOption (Unicode::String & variable, char const * const section, char const * const name, const int version)
{
	DEBUG_FATAL (!ms_installed, ("LocalMachineOptionManager::registerOption: not installed"));
	ms_optionManager->registerOption (variable, section, name, version);
}

// ----------------------------------------------------------------------

float LocalMachineOptionManager::findFloat(char const * const section, char const * const name, float const defaultValue)
{
	DEBUG_FATAL (!ms_installed, ("LocalMachineOptionManager::findFloat: not installed"));
	return ms_optionManager->findFloat (section, name, defaultValue);
}

// ======================================================================
