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

#include "clientGame/CreatureObject.h"
#include "clientGame/Game.h"
#include "clientGame/GameNetwork.h"
#include "clientGame/PlayerCreatureController.h"
#include "clientGraphics/ConfigClientGraphics.h"
#include "clientUserInterface/CuiWorkspace.h"
#include "sharedFoundation/GameControllerMessage.h"
#include "sharedNetworkMessages/GenericValueTypeMessage.h"
#include "UnicodeUtils.h"

#include "UIButton.h"
#include "UIPage.h"
#include "UIText.h"

#include <cmath>

//======================================================================

namespace
{
	int const marginPixels = 16;
	float const cs_autoPilotArrivalThreshold = 15.0f;
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
	m_buttonAutoPilotCancel(NULL),
	m_textAutoPilotStatus(NULL),
	m_inSkyway(false),
	m_boostMode(false),
	m_trafficMode(false),
	m_ascending(false),
	m_autoPilotActive(false),
	m_autoPilotWaypointsRemaining(0),
	m_autoPilotEngaged(false),
	m_autoPilotTargetX(0.0f),
	m_autoPilotTargetZ(0.0f)
{
	getCodeDataObject(TUIButton, m_buttonSkyway, "buttonSkyway");
	getCodeDataObject(TUIButton, m_buttonBoost, "buttonBoost");
	getCodeDataObject(TUIButton, m_buttonTraffic, "buttonTraffic");
	getCodeDataObject(TUIButton, m_buttonHorn, "buttonHorn");
	getCodeDataObject(TUIButton, m_buttonAutoPilotCancel, "buttonAutoPilotCancel");
	getCodeDataObject(TUIText, m_textAutoPilotStatus, "textAutoPilotStatus");

	registerMediatorObject(getPage(), true);
	if (m_buttonSkyway)
		registerMediatorObject(*m_buttonSkyway, true);
	if (m_buttonBoost)
		registerMediatorObject(*m_buttonBoost, true);
	if (m_buttonTraffic)
		registerMediatorObject(*m_buttonTraffic, true);
	if (m_buttonHorn)
		registerMediatorObject(*m_buttonHorn, true);
	if (m_buttonAutoPilotCancel)
		registerMediatorObject(*m_buttonAutoPilotCancel, true);
}

//----------------------------------------------------------------------

SwgCuiAirspeederPanel::~SwgCuiAirspeederPanel()
{
	m_buttonSkyway = NULL;
	m_buttonBoost = NULL;
	m_buttonTraffic = NULL;
	m_buttonHorn = NULL;
	m_buttonAutoPilotCancel = NULL;
	m_textAutoPilotStatus = NULL;
}

//----------------------------------------------------------------------

void SwgCuiAirspeederPanel::performActivate()
{
	setIsUpdating(true);
	positionCenterRight();
	refreshSkywayButtonText();
	refreshBoostTrafficButtons();
	refreshAutoPilotUI();
	getPage().SetVisible(true);
}

//----------------------------------------------------------------------

void SwgCuiAirspeederPanel::performDeactivate()
{
	setIsUpdating(false);
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
	if (context == m_buttonAutoPilotCancel)
	{
		m_autoPilotEngaged = false;
		m_autoPilotActive = false;

		CreatureObject * const player = Game::getPlayerCreature();
		if (player)
		{
			PlayerCreatureController * const controller = safe_cast<PlayerCreatureController *>(player->getController());
			if (controller)
			{
				controller->setAutoPilotLocked(false);
				controller->appendMessage(CM_cancelAutoRun, 0.0f);
			}
		}

		refreshAutoPilotUI();

		GenericValueTypeMessage<std::string> const msg("AutoPilotCancel", "cancel");
		GameNetwork::send(msg, true);
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
		panel->m_autoPilotActive = false;
		panel->m_autoPilotWaypointsRemaining = 0;
		panel->m_autoPilotEngaged = false;
		panel->refreshAutoPilotUI();

		CreatureObject * const player = Game::getPlayerCreature();
		if (player)
		{
			PlayerCreatureController * const controller = safe_cast<PlayerCreatureController *>(player->getController());
			if (controller)
				controller->setAutoPilotLocked(false);
		}
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

//----------------------------------------------------------------------

void SwgCuiAirspeederPanel::update(float deltaTimeSecs)
{
	CuiMediator::update(deltaTimeSecs);

	if (m_autoPilotEngaged)
		refreshAutoPilotUI();

	if (!m_autoPilotEngaged)
		return;

	CreatureObject * const player = Game::getPlayerCreature();
	if (!player)
		return;

	CreatureObject * const mount = player->getMountedCreature();
	Object * const movementObject = mount ? static_cast<Object *>(mount) : static_cast<Object *>(player);

	Vector const & pos = movementObject->getPosition_w();
	float const dx = m_autoPilotTargetX - pos.x;
	float const dz = m_autoPilotTargetZ - pos.z;
	float const distSq = dx * dx + dz * dz;

	if (distSq <= cs_autoPilotArrivalThreshold * cs_autoPilotArrivalThreshold)
	{
		m_autoPilotEngaged = false;
		m_autoPilotActive = false;

		PlayerCreatureController * const controller = safe_cast<PlayerCreatureController *>(player->getController());
		if (controller)
		{
			controller->setAutoPilotLocked(false);
			controller->appendMessage(CM_cancelAutoRun, 0.0f);
		}

		refreshAutoPilotUI();

		GenericValueTypeMessage<std::string> const msg("AutoPilotArrived", "arrived");
		GameNetwork::send(msg, true);
		return;
	}

	float const targetYaw = atan2(dx, dz);

	PlayerCreatureController * const controller = safe_cast<PlayerCreatureController *>(player->getController());
	if (controller)
	{
		controller->setDesiredYaw_w(targetYaw, true);

		if (!controller->getAutoRun())
			controller->appendMessage(CM_autoRun, 0.0f);
	}
}

//----------------------------------------------------------------------

void SwgCuiAirspeederPanel::engageAutoPilot(float targetX, float targetZ)
{
	CuiWorkspace * const ws = CuiWorkspace::getGameWorkspace();
	if (!ws)
		return;

	CuiMediator * const med = ws->findMediatorByType(typeid(SwgCuiAirspeederPanel));
	if (!med)
		return;

	SwgCuiAirspeederPanel * const panel = dynamic_cast<SwgCuiAirspeederPanel *>(med);
	if (!panel)
		return;

	panel->m_autoPilotEngaged = true;
	panel->m_autoPilotActive = true;
	panel->m_autoPilotTargetX = targetX;
	panel->m_autoPilotTargetZ = targetZ;

	CreatureObject * const player = Game::getPlayerCreature();
	if (player)
	{
		PlayerCreatureController * const controller = safe_cast<PlayerCreatureController *>(player->getController());
		if (controller)
		{
			controller->setAutoPilotLocked(true);

			CreatureObject * const mount = player->getMountedCreature();
			Object * const movementObject = mount ? static_cast<Object *>(mount) : static_cast<Object *>(player);
			Vector const & pos = movementObject->getPosition_w();
			float const dx = targetX - pos.x;
			float const dz = targetZ - pos.z;
			float const targetYaw = atan2(dx, dz);
			controller->setDesiredYaw_w(targetYaw, true);

			if (!controller->getAutoRun())
				controller->appendMessage(CM_autoRun, 0.0f);
		}
	}

	panel->refreshAutoPilotUI();
}

//----------------------------------------------------------------------

void SwgCuiAirspeederPanel::disengageAutoPilot()
{
	CuiWorkspace * const ws = CuiWorkspace::getGameWorkspace();
	if (!ws)
		return;

	CuiMediator * const med = ws->findMediatorByType(typeid(SwgCuiAirspeederPanel));
	if (!med)
		return;

	SwgCuiAirspeederPanel * const panel = dynamic_cast<SwgCuiAirspeederPanel *>(med);
	if (!panel)
		return;

	panel->m_autoPilotEngaged = false;
	panel->m_autoPilotActive = false;

	CreatureObject * const player = Game::getPlayerCreature();
	if (player)
	{
		PlayerCreatureController * const controller = safe_cast<PlayerCreatureController *>(player->getController());
		if (controller)
		{
			controller->setAutoPilotLocked(false);
			controller->appendMessage(CM_cancelAutoRun, 0.0f);
		}
	}

	panel->refreshAutoPilotUI();
}

//----------------------------------------------------------------------

bool SwgCuiAirspeederPanel::isInSkyway()
{
	CuiWorkspace * const ws = CuiWorkspace::getGameWorkspace();
	if (!ws)
		return false;

	CuiMediator * const med = ws->findMediatorByType(typeid(SwgCuiAirspeederPanel));
	if (!med)
		return false;

	SwgCuiAirspeederPanel * const panel = dynamic_cast<SwgCuiAirspeederPanel *>(med);
	if (!panel)
		return false;

	return panel->m_inSkyway;
}

//----------------------------------------------------------------------

void SwgCuiAirspeederPanel::setAutoPilotStatus(bool active, int waypointsRemaining)
{
	CuiWorkspace * const ws = CuiWorkspace::getGameWorkspace();
	if (!ws)
		return;

	CuiMediator * const med = ws->findMediatorByType(typeid(SwgCuiAirspeederPanel));
	if (!med)
		return;

	SwgCuiAirspeederPanel * const panel = dynamic_cast<SwgCuiAirspeederPanel *>(med);
	if (!panel)
		return;

	panel->m_autoPilotActive = active;
	panel->m_autoPilotWaypointsRemaining = waypointsRemaining;
	panel->refreshAutoPilotUI();
}

//----------------------------------------------------------------------

void SwgCuiAirspeederPanel::refreshAutoPilotUI()
{
	bool const showAP = m_autoPilotActive || m_autoPilotEngaged;

	if (m_buttonAutoPilotCancel)
		m_buttonAutoPilotCancel->SetVisible(showAP);

	if (m_textAutoPilotStatus)
	{
		if (showAP)
		{
			char buf[128];
			if (m_autoPilotEngaged)
			{
				CreatureObject * const player = Game::getPlayerCreature();
				if (player)
				{
					CreatureObject * const mount = player->getMountedCreature();
					Object * const movObj = mount ? static_cast<Object *>(mount) : static_cast<Object *>(player);
					Vector const & pos = movObj->getPosition_w();
					float const dx = m_autoPilotTargetX - pos.x;
					float const dz = m_autoPilotTargetZ - pos.z;
					float const dist = sqrt(dx * dx + dz * dz);
					snprintf(buf, sizeof(buf), "Auto-Pilot: %.0fm to (%.0f, %.0f)", dist, m_autoPilotTargetX, m_autoPilotTargetZ);
				}
				else
				{
					snprintf(buf, sizeof(buf), "Auto-Pilot: En route...");
				}
			}
			else
			{
				snprintf(buf, sizeof(buf), "Auto-Pilot: Standing by");
			}
			m_textAutoPilotStatus->SetLocalText(Unicode::narrowToWide(buf));
			m_textAutoPilotStatus->SetVisible(true);
		}
		else
		{
			m_textAutoPilotStatus->SetVisible(false);
		}
	}
}

//======================================================================
