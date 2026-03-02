//======================================================================
//
// SwgCuiAirspeederPanel.h
// copyright (c) 2024
//
// Skyway control panel for airspeeder hover vehicles.
//
//======================================================================

#ifndef INCLUDED_SwgCuiAirspeederPanel_H
#define INCLUDED_SwgCuiAirspeederPanel_H

//======================================================================

#include "clientUserInterface/CuiMediator.h"
#include "UIEventCallback.h"

class UIButton;
class UIPage;

//----------------------------------------------------------------------

class SwgCuiAirspeederPanel :
	public CuiMediator,
	public UIEventCallback
{
public:
	explicit SwgCuiAirspeederPanel(UIPage & page);

	virtual void performActivate();
	virtual void performDeactivate();
	virtual void update(float deltaTimeSecs);

	virtual void OnButtonPressed(UIWidget * context);

	static SwgCuiAirspeederPanel * createInto(UIPage & parent);
	static void resetPersistedState();

private:
	~SwgCuiAirspeederPanel();
	SwgCuiAirspeederPanel(SwgCuiAirspeederPanel const &);
	SwgCuiAirspeederPanel & operator=(SwgCuiAirspeederPanel const &);

	void positionCenterRight();
	void refreshSkywayButtonText();
	void refreshBoostTrafficButtons();

	void sendAirspeederCommand(const char* action);

	UIButton * m_buttonSkyway;
	UIButton * m_buttonBoost;
	UIButton * m_buttonTraffic;

	bool m_inSkyway;
	bool m_boostMode;
	bool m_trafficMode;
	bool m_ascending;
	float m_ascentEndTime;
};

//======================================================================

#endif
