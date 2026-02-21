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
}

//======================================================================

SwgCuiAirspeederPanel * SwgCuiAirspeederPanel::createInto(UIPage & parent)
{
	UIPage * page = NULL;
	parent.GetCodeDataObject(TUIPage, page, "airspeederPanel");
	if (!page)
		return NULL;
	return new SwgCuiAirspeederPanel(*page);
}

//----------------------------------------------------------------------

SwgCuiAirspeederPanel::SwgCuiAirspeederPanel(UIPage & page) :
	CuiMediator("SwgCuiAirspeederPanel", page),
	UIEventCallback(),
	m_buttonSkyway(NULL),
	m_buttonBoost(NULL),
	m_buttonTraffic(NULL),
	m_inSkyway(false),
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
			refreshSkywayButtonText();
			if (m_buttonSkyway)
				m_buttonSkyway->SetEnabled(true);
		}
	}
}

void SwgCuiAirspeederPanel::OnButtonPressed(UIWidget * context)
{
	if (context == m_buttonSkyway)
	{
		if (m_ascending)
			return;
		if (m_inSkyway)
		{
			m_inSkyway = false;
			refreshSkywayButtonText();
			sendAirspeederCommand("skyway");
		}
		else
		{
			sendAirspeederCommand("skyway");
			m_ascending = true;
			m_ascentEndTime = Clock::timeMs() * 0.001f + ascentDurationSecs;
			if (m_buttonSkyway)
				m_buttonSkyway->SetEnabled(false);
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
	if (m_buttonSkyway)
		m_buttonSkyway->SetLocalText(m_inSkyway ? Unicode::narrowToWide("Exit Skyway") : Unicode::narrowToWide("Enter Skyway"));
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

void SwgCuiAirspeederPanel::sendAirspeederCommand(const char* action)
{
	GenericValueTypeMessage<std::string> const msg("AirspeederControl", action);
	GameNetwork::send(msg, true);
}

//======================================================================
