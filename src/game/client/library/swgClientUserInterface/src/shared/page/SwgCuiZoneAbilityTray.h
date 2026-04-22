// ======================================================================
//
// SwgCuiZoneAbilityTray.h
// copyright 2026 Titan
//
// Zone encounter ability tray (server-driven icons + descriptions).
// ======================================================================

#ifndef INCLUDED_SwgCuiZoneAbilityTray_H
#define INCLUDED_SwgCuiZoneAbilityTray_H

#include "clientUserInterface/CuiMediator.h"
#include "UIEventCallback.h"

#include <string>
#include <vector>

class UIButton;
class UIImage;
class UIText;
class UIPage;

class SwgCuiZoneAbilityTray :
	public CuiMediator,
	public UIEventCallback
{
public:
	explicit SwgCuiZoneAbilityTray(UIPage & page);
	virtual ~SwgCuiZoneAbilityTray();

	virtual void performActivate();
	virtual void performDeactivate();

	virtual void OnButtonPressed(UIWidget * context);

	// Server -> client payload (see dynamic_encounter_ui.java).
	void applyPayload(std::string const & payload);

	static void installCallbacks();
	static void onServerPayload(std::string const & payload);
	static void onServerClose();

private:
	SwgCuiZoneAbilityTray(SwgCuiZoneAbilityTray const &);
	SwgCuiZoneAbilityTray & operator=(SwgCuiZoneAbilityTray const &);

	void clearSlots();
	void sendUse(size_t slotIndex);

private:
	UIButton * m_buttonClose;
	UIText * m_textTitle;
	UIText * m_textDesc;

	UIButton * m_buttonSlot[6];
	UIImage * m_imageSlot[6];

	std::string m_encounterKey;
	std::vector<std::string> m_slotAbilityNames;
	std::vector<std::string> m_slotDescriptions;
};

#endif
