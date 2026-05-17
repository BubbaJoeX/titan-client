// ======================================================================
//
// SwgCuiClaimManagement.cpp
//
// ======================================================================

#include "swgClientUserInterface/FirstSwgClientUserInterface.h"
#include "swgClientUserInterface/SwgCuiClaimManagement.h"

#include "UIButton.h"
#include "UIPage.h"
#include "UIText.h"
#include "UnicodeUtils.h"
#include "clientGame/ConfigClientGame.h"

// ======================================================================

SwgCuiClaimManagement::SwgCuiClaimManagement(UIPage & page) :
	CuiMediator("SwgCuiClaimManagement", page),
	UIEventCallback(),
	m_closeButton(0),
	m_infoText(0)
{
	getCodeDataObject(TUIButton, m_closeButton, "closebutton");
	getCodeDataObject(TUIText, m_infoText, "infotext");

	if (m_closeButton)
		registerMediatorObject(*m_closeButton, true);

	if (m_infoText)
	{
		char buf[512];
		sprintf(buf,
			"Open-world claims use structure placement for the marker deed.\r\n"
			"Footprint preview radius defaults to %.0f meters (ClientGame config).\r\n\r\n"
			"At the claim management terminal, use Item Use for the server menu:\r\n"
			"  - Pay maintenance\r\n"
			"  - Withdraw taxed resources\r\n"
			"  - Ban / unban (look-at target)\r\n\r\n"
			"Claim contents stay hidden until you enter the footprint (server enforced).",
			ConfigClientGame::getClaimDefaultFootprintRadiusMeters());
		m_infoText->SetLocalText(Unicode::narrowToWide(buf));
		m_infoText->SetPreLocalized(true);
	}
}

// ----------------------------------------------------------------------

SwgCuiClaimManagement::~SwgCuiClaimManagement()
{
	m_closeButton = 0;
	m_infoText = 0;
}

// ----------------------------------------------------------------------

void SwgCuiClaimManagement::OnButtonPressed(UIWidget * context)
{
	if (context == m_closeButton)
		deactivate();
}

// ----------------------------------------------------------------------

void SwgCuiClaimManagement::performActivate()
{
	getPage().SetVisible(true);
}

// ----------------------------------------------------------------------

void SwgCuiClaimManagement::performDeactivate()
{
	getPage().SetVisible(false);
}

// ======================================================================
