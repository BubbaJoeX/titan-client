//======================================================================
//
// SwgCuiClaimManagement.h
//
// Client reference panel for the open-world claim system. Terminal
// actions (maintenance, tax, bans) are driven by server SUI.
//
//======================================================================

#ifndef INCLUDED_SwgCuiClaimManagement_H
#define INCLUDED_SwgCuiClaimManagement_H

class UIPage;
class UIButton;
class UIText;

#include "UIEventCallback.h"
#include "clientUserInterface/CuiMediator.h"

//----------------------------------------------------------------------

class SwgCuiClaimManagement : public UIEventCallback, public CuiMediator
{
public:
	explicit SwgCuiClaimManagement(UIPage & page);

	virtual void OnButtonPressed(UIWidget * context);

protected:
	virtual void performActivate();
	virtual void performDeactivate();

private:
	virtual ~SwgCuiClaimManagement();
	SwgCuiClaimManagement();
	SwgCuiClaimManagement(SwgCuiClaimManagement const &);
	SwgCuiClaimManagement & operator=(SwgCuiClaimManagement const &);

	UIButton * m_closeButton;
	UIText *   m_infoText;
};

//======================================================================

#endif
