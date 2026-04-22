// ======================================================================
//
// SwgCuiZoneAbilityTray.cpp
// copyright 2026 Titan
// ======================================================================

#include "swgClientUserInterface/FirstSwgClientUserInterface.h"
#include "swgClientUserInterface/SwgCuiZoneAbilityTray.h"

#include "clientGame/GameNetwork.h"
#include "clientGame/ZoneAbilityTrayManager.h"
#include "clientUserInterface/CuiMediatorFactory.h"
#include "sharedFile/TreeFile.h"
#include "sharedNetworkMessages/GenericValueTypeMessage.h"
#include "Unicode.h"

#include "UIButton.h"
#include "UIImage.h"
#include "UIMessage.h"
#include "UIPage.h"
#include "UIText.h"

#include <cstdio>
#include <vector>

namespace
{
	void splitByChar(std::string const & s, char delim, std::vector<std::string> & out)
	{
		out.clear();
		size_t start = 0;
		while (start <= s.size())
		{
			size_t pos = s.find(delim, start);
			if (pos == std::string::npos)
			{
				out.push_back(s.substr(start));
				break;
			}
			out.push_back(s.substr(start, pos - start));
			start = pos + 1;
		}
	}
}

//----------------------------------------------------------------------

SwgCuiZoneAbilityTray::SwgCuiZoneAbilityTray(UIPage & page) :
	CuiMediator("SwgCuiZoneAbilityTray", page),
	UIEventCallback(),
	m_buttonClose(0),
	m_textTitle(0),
	m_textDesc(0)
{
	for (int i = 0; i < 6; ++i)
	{
		m_buttonSlot[i] = 0;
		m_imageSlot[i] = 0;
	}

	getCodeDataObject(TUIButton, m_buttonClose, "buttonClose");
	getCodeDataObject(TUIText, m_textTitle, "textTitle");
	getCodeDataObject(TUIText, m_textDesc, "textDesc");

	char buf[32];
	for (int i = 0; i < 6; ++i)
	{
		snprintf(buf, sizeof(buf), "buttonSlot%d", i);
		getCodeDataObject(TUIButton, m_buttonSlot[i], buf);
		snprintf(buf, sizeof(buf), "imageSlot%d", i);
		getCodeDataObject(TUIImage, m_imageSlot[i], buf);
	}

	registerMediatorObject(getPage(), true);
	if (m_buttonClose)
		registerMediatorObject(*m_buttonClose, true);
	for (int i = 0; i < 6; ++i)
	{
		if (m_buttonSlot[i])
			registerMediatorObject(*m_buttonSlot[i], true);
	}
}

//----------------------------------------------------------------------

SwgCuiZoneAbilityTray::~SwgCuiZoneAbilityTray()
{
}

//----------------------------------------------------------------------

void SwgCuiZoneAbilityTray::performActivate()
{
	CuiMediator::performActivate();
}

//----------------------------------------------------------------------

void SwgCuiZoneAbilityTray::performDeactivate()
{
	CuiMediator::performDeactivate();
}

//----------------------------------------------------------------------

void SwgCuiZoneAbilityTray::clearSlots()
{
	m_encounterKey.clear();
	m_slotAbilityNames.clear();
	m_slotDescriptions.clear();
	for (int i = 0; i < 6; ++i)
	{
		if (m_buttonSlot[i])
		{
			m_buttonSlot[i]->SetText(Unicode::String());
			m_buttonSlot[i]->SetVisible(false);
		}
		if (m_imageSlot[i])
		{
			m_imageSlot[i]->SetVisible(false);
		}
	}
	if (m_textDesc)
		m_textDesc->SetLocalText(Unicode::String());
}

//----------------------------------------------------------------------

void SwgCuiZoneAbilityTray::applyPayload(std::string const & payload)
{
	clearSlots();
	if (payload.empty())
		return;

	std::vector<std::string> abilities;
	splitByChar(payload, '|', abilities);
	if (abilities.empty())
		return;

	size_t slot = 0;
	for (size_t a = 0; a < abilities.size() && slot < 6; ++a)
	{
		std::string const & rec = abilities[a];
		if (rec.empty())
			continue;

		std::vector<std::string> fields;
		splitByChar(rec, '^', fields);
		if (fields.size() < 9)
			continue;

		if (m_encounterKey.empty())
			m_encounterKey = fields[0];

		std::string const & abilityName = fields[1];
		std::string const & iconPath = fields[2];
		std::string const & displayTitle = fields[7];
		std::string const & displayDesc = fields[8];

		m_slotAbilityNames.push_back(abilityName);
		m_slotDescriptions.push_back(displayDesc);

		if (m_buttonSlot[slot])
		{
			m_buttonSlot[slot]->SetText(Unicode::narrowToWide(displayTitle.empty() ? abilityName : displayTitle));
			m_buttonSlot[slot]->SetVisible(true);
		}

		if (m_imageSlot[slot])
		{
			if (!iconPath.empty() && TreeFile::exists(iconPath.c_str()))
				m_imageSlot[slot]->SetSourceResource(Unicode::narrowToWide(iconPath));
			m_imageSlot[slot]->SetVisible(true);
		}

		++slot;
	}

	if (m_textTitle)
		m_textTitle->SetLocalText(Unicode::narrowToWide("Zone Abilities"));
	if (m_textDesc && !m_slotDescriptions.empty())
		m_textDesc->SetLocalText(Unicode::narrowToWide(m_slotDescriptions[0]));
}

//----------------------------------------------------------------------

void SwgCuiZoneAbilityTray::sendUse(size_t slotIndex)
{
	if (slotIndex >= m_slotAbilityNames.size())
		return;
	if (m_encounterKey.empty())
		return;

	std::string const payload = m_encounterKey + "|" + m_slotAbilityNames[slotIndex];
	GenericValueTypeMessage<std::string> const msg("ZoneAbilityUseRequest", payload);
	GameNetwork::send(msg, true);
}

//----------------------------------------------------------------------

void SwgCuiZoneAbilityTray::OnButtonPressed(UIWidget * context)
{
	if (context == m_buttonClose)
	{
		CuiMediatorFactory::deactivateInWorkspace("WS_ZoneAbilityTray");
		return;
	}
	for (int i = 0; i < 6; ++i)
	{
		if (context == m_buttonSlot[i])
		{
			if (m_textDesc && i < static_cast<int>(m_slotDescriptions.size()))
				m_textDesc->SetLocalText(Unicode::narrowToWide(m_slotDescriptions[static_cast<size_t>(i)]));
			sendUse(static_cast<size_t>(i));
			return;
		}
	}
}

//----------------------------------------------------------------------

void SwgCuiZoneAbilityTray::installCallbacks()
{
	ZoneAbilityTrayManager::setTrayPayloadFn(&SwgCuiZoneAbilityTray::onServerPayload);
	ZoneAbilityTrayManager::setTrayCloseFn(&SwgCuiZoneAbilityTray::onServerClose);
}

//----------------------------------------------------------------------

void SwgCuiZoneAbilityTray::onServerPayload(std::string const & payload)
{
	CuiMediator * const m = CuiMediatorFactory::activateInWorkspace("WS_ZoneAbilityTray", true, false);
	SwgCuiZoneAbilityTray * const tray = dynamic_cast<SwgCuiZoneAbilityTray *>(m);
	if (tray)
		tray->applyPayload(payload);
}

//----------------------------------------------------------------------

void SwgCuiZoneAbilityTray::onServerClose()
{
	CuiMediatorFactory::deactivateInWorkspace("WS_ZoneAbilityTray");
}
