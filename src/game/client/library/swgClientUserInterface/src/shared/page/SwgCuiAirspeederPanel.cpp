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
#include "sharedFoundation/Clock.h"
#include "sharedFoundation/NetworkId.h"
#include "sharedNetworkMessages/GenericValueTypeMessage.h"
#include "UnicodeUtils.h"

#include "UIButton.h"
#include "UIPage.h"

//======================================================================

namespace
{
	int const marginPixels = 16;
	float const ascentDurationSecs = 5.0f;
	bool s_persistedInSkyway = false;
}

//======================================================================

SwgCuiAirspeederPanel * SwgCuiAirspeederPanel::createInto(UIPage & parent)
{
	// parent is the AirspeederPanel page from /GroundHUD.AirspeederPanel - use it directly
	return new SwgCuiAirspeederPanel(parent);
}

//----------------------------------------------------------------------

SwgCuiAirspeederPanel::SwgCuiAirspeederPanel(UIPage & page) :
	CuiMediator("SwgCuiAirspeederPanel", page),
	UIEventCallback(),
	m_buttonSkyway(NULL),
	m_buttonBoost(NULL),
	m_buttonTraffic(NULL),
	m_inSkyway(s_persistedInSkyway),
	m_boostMode(false),
	m_trafficMode(false),
	m_ascending(false),
	m_ascentEndTime(0.0f)
{
	getCodeDataObject(TUIButton, m_buttonSkyway, "buttonSkyway");
	getCodeDataObject(TUIButton, m_buttonBoost, "buttonBoost");
	getCodeDataObject(TUIButton, m_buttonTraffic, "buttonTraffic");

	registerMediatorObject(getPage(), true);
	if (m_buttonSkyway)
		registerMediatorObject(*m_buttonSkyway, true);
	if (m_buttonBoost)
		registerMediatorObject(*m_buttonBoost, true);
	if (m_buttonTraffic)
		registerMediatorObject(*m_buttonTraffic, true);
}

//----------------------------------------------------------------------

SwgCuiAirspeederPanel::~SwgCuiAirspeederPanel()
{
	m_buttonSkyway = NULL;
	m_buttonBoost = NULL;
	m_buttonTraffic = NULL;
}

//----------------------------------------------------------------------

void SwgCuiAirspeederPanel::performActivate()
{
	m_inSkyway = s_persistedInSkyway;
	setIsUpdating(true);
	positionCenterRight();
	refreshSkywayButtonText();
	refreshBoostTrafficButtons();
	getPage().SetVisible(true);
}

//----------------------------------------------------------------------

void SwgCuiAirspeederPanel::performDeactivate()
{
	setIsUpdating(false);
	s_persistedInSkyway = m_inSkyway;
	getPage().SetVisible(false);
}

//----------------------------------------------------------------------

void SwgCuiAirspeederPanel::update(float deltaTimeSecs)
{
	CuiMediator::update(deltaTimeSecs);
	if (m_ascending && m_ascentEndTime > 0.0f)
	{
		float const now = Clock::timeMs() * 0.001f;
		if (now >= m_ascentEndTime)
		{
			m_ascending = false;
			m_inSkyway = true;
			s_persistedInSkyway = true;
			refreshSkywayButtonText();
		}
	}
}

void SwgCuiAirspeederPanel::OnButtonPressed(UIWidget * context)
{
	if (context == m_buttonSkyway)
	{
		if (m_ascending)
		{
			m_ascending = false;
			m_ascentEndTime = 0.0f;
			m_inSkyway = false;
			s_persistedInSkyway = false;
			refreshSkywayButtonText();
			sendAirspeederCommand("skyway");
		}
		else if (m_inSkyway)
		{
			m_inSkyway = false;
			s_persistedInSkyway = false;
			refreshSkywayButtonText();
			sendAirspeederCommand("skyway");
		}
		else
		{
			sendAirspeederCommand("skyway");
			m_ascending = true;
			m_ascentEndTime = Clock::timeMs() * 0.001f + ascentDurationSecs;
			refreshSkywayButtonText();
		}
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
	s_persistedInSkyway = false;
}

void SwgCuiAirspeederPanel::sendAirspeederCommand(const char* action)
{
	GenericValueTypeMessage<std::string> const msg("AirspeederControl", action);
	GameNetwork::send(msg, true);
}

//======================================================================
