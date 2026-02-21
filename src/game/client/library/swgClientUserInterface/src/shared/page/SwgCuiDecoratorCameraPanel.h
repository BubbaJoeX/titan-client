//======================================================================
//
// SwgCuiDecoratorCameraPanel.h
// copyright (c) 2024
//
// Decorator camera options panel: snap to grid, drop to terrain, reset selection.
//
//======================================================================

#ifndef INCLUDED_SwgCuiDecoratorCameraPanel_H
#define INCLUDED_SwgCuiDecoratorCameraPanel_H

//======================================================================

#include "clientUserInterface/CuiMediator.h"
#include "UIEventCallback.h"

class UIButton;
class UICheckbox;
class UIPage;
class UIText;

//----------------------------------------------------------------------

class SwgCuiDecoratorCameraPanel :
	public CuiMediator,
	public UIEventCallback
{
public:
	explicit SwgCuiDecoratorCameraPanel(UIPage & page);

	virtual void performActivate();
	virtual void performDeactivate();
	virtual void update(float deltaTimeSecs);

	virtual void OnButtonPressed(UIWidget * context);
	virtual void OnCheckboxSet(UIWidget * context);
	virtual void OnCheckboxUnset(UIWidget * context);

	static SwgCuiDecoratorCameraPanel * createInto(UIPage & parent);

private:
	~SwgCuiDecoratorCameraPanel();
	SwgCuiDecoratorCameraPanel(SwgCuiDecoratorCameraPanel const &);
	SwgCuiDecoratorCameraPanel & operator=(SwgCuiDecoratorCameraPanel const &);

	void positionCenterRight();
	void refreshSnapMetersText();
	void refreshOverlayText();

	UIButton *   m_buttonDropTerrain;
	UIButton *   m_buttonHelp;
	UIButton *   m_buttonResetSelection;
	UIButton *   m_buttonSpawning;
	UICheckbox * m_checkDragOnTerrain;
	UICheckbox * m_checkFocusCamera;
	UICheckbox * m_checkGimbalMovement;
	UICheckbox * m_checkMouse4Drag;
	UICheckbox * m_checkSnapHorizontal;
	UICheckbox * m_checkSnapVertical;
	UIText *     m_textPosition;
	UIText *     m_textSelection;
	UIText *     m_textSnapMeters;
};

//======================================================================

#endif
