// ======================================================================
//
// ZoneAbilityTrayManager.h
// copyright 2026 Titan
//
// Bridges GameNetwork (clientGame) to SWG UI for the zone ability tray.
// ======================================================================

#ifndef INCLUDED_ZoneAbilityTrayManager_H
#define INCLUDED_ZoneAbilityTrayManager_H

#include <string>

class ZoneAbilityTrayManager
{
public:
	typedef void (*TrayPayloadFn)(std::string const & payload);
	typedef void (*TrayCloseFn)();

	static void setTrayPayloadFn(TrayPayloadFn fn);
	static void setTrayCloseFn(TrayCloseFn fn);
	static void clearTrayPayloadFn();
	static void clearTrayCloseFn();
	static void notifyTrayPayload(std::string const & payload);
	static void notifyTrayClose();

private:
	static TrayPayloadFn ms_payloadFn;
	static TrayCloseFn ms_closeFn;
};

#endif
