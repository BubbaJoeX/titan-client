//======================================================================
//
// SwgCuiAirspeederPanel.cpp
// copyright (c) 2024
//
// Skyway control panel for airspeeder hover vehicles.
//
//======================================================================

#include "swgClientUserInterface/FirstSwgClientUserInterface.h"
#include "swgClientUserInterface/SwgCuiAirspeederPanel.h"

#include "clientGame/GameNetwork.h"
#include "clientGraphics/ConfigClientGraphics.h"
#include "clientUserInterface/CuiWorkspace.h"
#include "sharedNetworkMessages/GenericValueTypeMessage.h"
#include "UnicodeUtils.h"

#include "UIButton.h"
#include "UIPage.h"

//======================================================================

namespace
{
	int const marginPixels = 16;
}

//======================================================================

SwgCuiAirspeederPanel * SwgCuiAirspeederPanel::createInto(UIPage & parent)
{
	return new SwgCuiAirspeederPanel(parent);
}

//----------------------------------------------------------------------

SwgCuiAirspeederPanel::SwgCuiAirspeederPanel(UIPage & page) :
	CuiMediator("SwgCuiAirspeederPanel", page),
	UIEventCallback(),
	m_buttonSkyway(NULL),
	m_buttonBoost(NULL),
	m_buttonTraffic(NULL),
	m_buttonHorn(NULL),
	m_inSkyway(false),
	m_boostMode(false),
	m_trafficMode(false),
	m_ascending(false)
{
	getCodeDataObject(TUIButton, m_buttonSkyway, "buttonSkyway");
	getCodeDataObject(TUIButton, m_buttonBoost, "buttonBoost");
	getCodeDataObject(TUIButton, m_buttonTraffic, "buttonTraffic");
	getCodeDataObject(TUIButton, m_buttonHorn, "buttonHorn");

	registerMediatorObject(getPage(), true);
	if (m_buttonSkyway)
		registerMediatorObject(*m_buttonSkyway, true);
	if (m_buttonBoost)
		registerMediatorObject(*m_buttonBoost, true);
	if (m_buttonTraffic)
		registerMediatorObject(*m_buttonTraffic, true);
	if (m_buttonHorn)
		registerMediatorObject(*m_buttonHorn, true);
}

//----------------------------------------------------------------------

SwgCuiAirspeederPanel::~SwgCuiAirspeederPanel()
{
	m_buttonSkyway = NULL;
	m_buttonBoost = NULL;
	m_buttonTraffic = NULL;
	m_buttonHorn = NULL;
}

//----------------------------------------------------------------------

void SwgCuiAirspeederPanel::performActivate()
{
	positionCenterRight();
	refreshSkywayButtonText();
	refreshBoostTrafficButtons();
	getPage().SetVisible(true);
}

//----------------------------------------------------------------------

void SwgCuiAirspeederPanel::performDeactivate()
{
	getPage().SetVisible(false);
}

//----------------------------------------------------------------------

void SwgCuiAirspeederPanel::OnButtonPressed(UIWidget * context)
{
	if (context == m_buttonSkyway)
	{
		sendAirspeederCommand("skyway");
		if (m_ascending)
		{
			m_ascending = false;
			m_inSkyway = false;
		}
		else if (m_inSkyway)
		{
			m_inSkyway = false;
			m_ascending = false;
		}
		else
		{
			m_ascending = true;
		}
		refreshSkywayButtonText();
		return;
	}
	if (context == m_buttonBoost)
	{
		if (m_trafficMode)
			return;
		m_boostMode = !m_boostMode;
		refreshBoostTrafficButtons();
		sendAirspeederCommand("boost");
		return;
	}
	if (context == m_buttonTraffic)
	{
		if (m_boostMode)
			return;
		m_trafficMode = !m_trafficMode;
		refreshBoostTrafficButtons();
		sendAirspeederCommand("traffic");
		return;
	}
	if (context == m_buttonHorn)
	{
		sendAirspeederCommand("horn");
		return;
	}
}

//----------------------------------------------------------------------

void SwgCuiAirspeederPanel::positionCenterRight()
{
	UIPage & page = getPage();
	UISize const size = page.GetSize();
	int const screenW = ConfigClientGraphics::getScreenWidth();
	int const screenH = ConfigClientGraphics::getScreenHeight();
	int const x = screenW - size.x - marginPixels;
	int const y = (screenH - size.y) / 2 - marginPixels;
	page.SetLocation(UIPoint(x, y));
}

//----------------------------------------------------------------------

void SwgCuiAirspeederPanel::refreshSkywayButtonText()
{
	if (!m_buttonSkyway)
		return;

	if (m_ascending)
		m_buttonSkyway->SetLocalText(Unicode::narrowToWide("Ascending..."));
	else if (m_inSkyway)
		m_buttonSkyway->SetLocalText(Unicode::narrowToWide("Exit Skyway"));
	else
		m_buttonSkyway->SetLocalText(Unicode::narrowToWide("Enter Skyway"));

	m_buttonSkyway->SetEnabled(true);
}

void SwgCuiAirspeederPanel::refreshBoostTrafficButtons()
{
	if (m_buttonBoost)
	{
		m_buttonBoost->SetLocalText(m_boostMode ? Unicode::narrowToWide("Boost Mode (ON)") : Unicode::narrowToWide("Boost Mode"));
		m_buttonBoost->SetEnabled(!m_trafficMode);
	}
	if (m_buttonTraffic)
	{
		m_buttonTraffic->SetLocalText(m_trafficMode ? Unicode::narrowToWide("Traffic Mode (ON)") : Unicode::narrowToWide("Traffic Mode"));
		m_buttonTraffic->SetEnabled(!m_boostMode);
	}
}

void SwgCuiAirspeederPanel::resetPersistedState()
{
	CuiWorkspace * const ws = CuiWorkspace::getGameWorkspace();
	if (!ws)
		return;

	CuiMediator * const med = ws->findMediatorByType(typeid(SwgCuiAirspeederPanel));
	if (!med)
		return;

	SwgCuiAirspeederPanel * const panel = dynamic_cast<SwgCuiAirspeederPanel *>(med);
	if (panel)
	{
		panel->m_inSkyway = false;
		panel->m_ascending = false;
		panel->m_boostMode = false;
		panel->m_trafficMode = false;
	}
}

void SwgCuiAirspeederPanel::setSkywayActive()
{
	CuiWorkspace * const ws = CuiWorkspace::getGameWorkspace();
	if (!ws)
		return;

	CuiMediator * const med = ws->findMediatorByType(typeid(SwgCuiAirspeederPanel));
	if (!med)
		return;

	SwgCuiAirspeederPanel * const panel = dynamic_cast<SwgCuiAirspeederPanel *>(med);
	if (panel)
	{
		panel->m_ascending = false;
		panel->m_inSkyway = true;
		panel->refreshSkywayButtonText();
	}
}

void SwgCuiAirspeederPanel::sendAirspeederCommand(const char* action)
{
	GenericValueTypeMessage<std::string> const msg("AirspeederControl", action);
	GameNetwork::send(msg, true);
}

//======================================================================
