//======================================================================
//
// SwgCuiClaimManagement.h
//
// Client shell for the claim management terminal. Gameplay actions are
// driven by server-side SUI from terminal_claim_management.java; this
// mediator exists for future dedicated UI wiring.
//
//======================================================================

#ifndef INCLUDED_SwgCuiClaimManagement_H
#define INCLUDED_SwgCuiClaimManagement_H

class UIPage;
#include "clientUserInterface/CuiMediator.h"

//----------------------------------------------------------------------

class SwgCuiClaimManagement : public CuiMediator
{
public:
	explicit SwgCuiClaimManagement(UIPage & page);

protected:
	virtual void performActivate();
	virtual void performDeactivate();

private:
	virtual ~SwgCuiClaimManagement();
	SwgCuiClaimManagement();
	SwgCuiClaimManagement(SwgCuiClaimManagement const &);
	SwgCuiClaimManagement & operator=(SwgCuiClaimManagement const &);
};

//======================================================================

#endif
