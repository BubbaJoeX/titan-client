// ======================================================================
//
// ZoneAbilityTrayManager.cpp
// copyright 2026 Titan
// ======================================================================

#include "clientGame/FirstClientGame.h"
#include "clientGame/ZoneAbilityTrayManager.h"

ZoneAbilityTrayManager::TrayPayloadFn ZoneAbilityTrayManager::ms_payloadFn = 0;
ZoneAbilityTrayManager::TrayCloseFn ZoneAbilityTrayManager::ms_closeFn = 0;

void ZoneAbilityTrayManager::setTrayPayloadFn(TrayPayloadFn fn)
{
	ms_payloadFn = fn;
}

void ZoneAbilityTrayManager::setTrayCloseFn(TrayCloseFn fn)
{
	ms_closeFn = fn;
}

void ZoneAbilityTrayManager::clearTrayPayloadFn()
{
	ms_payloadFn = 0;
}

void ZoneAbilityTrayManager::clearTrayCloseFn()
{
	ms_closeFn = 0;
}

void ZoneAbilityTrayManager::notifyTrayPayload(std::string const & payload)
{
	if (ms_payloadFn)
		ms_payloadFn(payload);
}

void ZoneAbilityTrayManager::notifyTrayClose()
{
	if (ms_closeFn)
		ms_closeFn();
}
