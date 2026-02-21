//======================================================================
//
// SwgCuiDecoratorCameraPanel.cpp
// copyright (c) 2024
//
// Decorator camera options panel: snap to grid, drop to terrain, reset selection.
//
//======================================================================

#include "swgClientUserInterface/FirstSwgClientUserInterface.h"
#include "swgClientUserInterface/SwgCuiDecoratorCameraPanel.h"

#include "clientGraphics/ConfigClientGraphics.h"
#include "clientUserInterface/CuiFurnitureMovementManager.h"
#include "sharedFoundation/FormattedString.h"
#include "UnicodeUtils.h"

#include "UIButton.h"
#include "UICheckbox.h"
#include "UIPage.h"
#include "UIText.h"

//======================================================================

namespace
{
	int const marginPixels = 16;
}

//======================================================================

SwgCuiDecoratorCameraPanel * SwgCuiDecoratorCameraPanel::createInto(UIPage & parent)
{
	UIPage * page = NULL;
	parent.GetCodeDataObject(TUIPage, page, "decoratorCameraPanel");
	if (!page)
		return NULL;
	return new SwgCuiDecoratorCameraPanel(*page);
}

//----------------------------------------------------------------------

SwgCuiDecoratorCameraPanel::SwgCuiDecoratorCameraPanel(UIPage & page) :
	CuiMediator("SwgCuiDecoratorCameraPanel", page),
	UIEventCallback(),
	m_buttonDropTerrain(NULL),
	m_buttonHelp(NULL),
	m_buttonResetSelection(NULL),
	m_buttonSpawning(NULL),
	m_checkDragOnTerrain(NULL),
	m_checkFocusCamera(NULL),
	m_checkGimbalMovement(NULL),
	m_checkMouse4Drag(NULL),
	m_checkSnapHorizontal(NULL),
	m_checkSnapVertical(NULL),
	m_textPosition(NULL),
	m_textSelection(NULL),
	m_textSnapMeters(NULL)
{
	getCodeDataObject(TUIButton,   m_buttonDropTerrain,    "buttonDropTerrain");
	getCodeDataObject(TUIButton,   m_buttonHelp,          "buttonHelp", true);
	getCodeDataObject(TUIButton,   m_buttonResetSelection, "buttonResetSelection");
	getCodeDataObject(TUIButton,   m_buttonSpawning,       "buttonSpawning");
	getCodeDataObject(TUICheckbox, m_checkDragOnTerrain,   "checkDragOnTerrain");
	getCodeDataObject(TUICheckbox, m_checkFocusCamera,     "checkFocusCamera", true);
	getCodeDataObject(TUICheckbox, m_checkGimbalMovement,   "checkGimbalMovement");
	getCodeDataObject(TUICheckbox, m_checkMouse4Drag,       "checkMouse4Drag");
	getCodeDataObject(TUICheckbox, m_checkSnapHorizontal,  "checkSnapHorizontal");
	getCodeDataObject(TUICheckbox, m_checkSnapVertical,    "checkSnapVertical");
	getCodeDataObject(TUIText,     m_textPosition,        "textPosition", true);
	getCodeDataObject(TUIText,     m_textSelection,       "textSelection", true);
	getCodeDataObject(TUIText,     m_textSnapMeters,      "textSnapMeters");

	registerMediatorObject(getPage(), true);
	if (m_buttonDropTerrain)
		registerMediatorObject(*m_buttonDropTerrain, true);
	if (m_buttonHelp)
		registerMediatorObject(*m_buttonHelp, true);
	if (m_buttonResetSelection)
		registerMediatorObject(*m_buttonResetSelection, true);
	if (m_buttonSpawning)
		registerMediatorObject(*m_buttonSpawning, true);
	if (m_checkDragOnTerrain)
		registerMediatorObject(*m_checkDragOnTerrain, true);
	if (m_checkFocusCamera)
		registerMediatorObject(*m_checkFocusCamera, true);
	if (m_checkGimbalMovement)
		registerMediatorObject(*m_checkGimbalMovement, true);
	if (m_checkMouse4Drag)
		registerMediatorObject(*m_checkMouse4Drag, true);
	if (m_checkSnapHorizontal)
		registerMediatorObject(*m_checkSnapHorizontal, true);
	if (m_checkSnapVertical)
		registerMediatorObject(*m_checkSnapVertical, true);
}

//----------------------------------------------------------------------

SwgCuiDecoratorCameraPanel::~SwgCuiDecoratorCameraPanel()
{
	m_buttonDropTerrain = NULL;
	m_buttonHelp = NULL;
	m_buttonResetSelection = NULL;
	m_buttonSpawning = NULL;
	m_checkDragOnTerrain = NULL;
	m_checkFocusCamera = NULL;
	m_checkGimbalMovement = NULL;
	m_checkMouse4Drag = NULL;
	m_checkSnapHorizontal = NULL;
	m_checkSnapVertical = NULL;
	m_textPosition = NULL;
	m_textSelection = NULL;
	m_textSnapMeters = NULL;
}

//----------------------------------------------------------------------

void SwgCuiDecoratorCameraPanel::performActivate()
{
	// Do not auto-open Item Spawner here; user must click Spawning button to open it manually.
	positionCenterRight();
	getPage().SetVisible(true);
	if (m_checkSnapHorizontal)
		m_checkSnapHorizontal->SetChecked(CuiFurnitureMovementManager::getSnapToGridHorizontal());
	if (m_checkSnapVertical)
		m_checkSnapVertical->SetChecked(CuiFurnitureMovementManager::getSnapToGridVertical());
	if (m_checkFocusCamera)
		m_checkFocusCamera->SetChecked(CuiFurnitureMovementManager::getFocusCameraOnSelection());
	if (m_checkGimbalMovement)
		m_checkGimbalMovement->SetChecked(CuiFurnitureMovementManager::getUseGimbalMovement());
	if (m_checkMouse4Drag)
		m_checkMouse4Drag->SetChecked(CuiFurnitureMovementManager::getUseMouse4Drag());
	if (m_checkDragOnTerrain)
		m_checkDragOnTerrain->SetChecked(CuiFurnitureMovementManager::getDragOnTerrain());
	refreshSnapMetersText();
	setIsUpdating(true);
}

//----------------------------------------------------------------------

void SwgCuiDecoratorCameraPanel::performDeactivate()
{
	setIsUpdating(false);
	getPage().SetVisible(false);
}

//----------------------------------------------------------------------

void SwgCuiDecoratorCameraPanel::update(float deltaTimeSecs)
{
	CuiMediator::update(deltaTimeSecs);
	if (!CuiFurnitureMovementManager::isDecoratorCameraActive())
		deactivate();
	else
	{
		refreshSnapMetersText();
		refreshOverlayText();
	}
}

//----------------------------------------------------------------------

void SwgCuiDecoratorCameraPanel::positionCenterRight()
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

void SwgCuiDecoratorCameraPanel::refreshSnapMetersText()
{
	if (!m_textSnapMeters)
		return;
	float const meters = CuiFurnitureMovementManager::getSnapGridMeters();
	m_textSnapMeters->SetLocalText(Unicode::narrowToWide(FormattedString<64>().sprintf("Grid: %.1f m (G to cycle)", meters)));
}

//----------------------------------------------------------------------

void SwgCuiDecoratorCameraPanel::refreshOverlayText()
{
	if (m_textPosition)
	{
		std::string const pos = CuiFurnitureMovementManager::getSelectionOverlayPositionLine();
		m_textPosition->SetLocalText(pos.empty() ? Unicode::narrowToWide("X: --  Z: --  Y: --") : Unicode::narrowToWide(pos));
	}
	if (m_textSelection)
	{
		std::string const sel = CuiFurnitureMovementManager::getSelectionOverlaySelectionLine();
		m_textSelection->SetLocalText(sel.empty() ? Unicode::narrowToWide("Selection: (none)") : Unicode::narrowToWide(sel));
	}
}

//----------------------------------------------------------------------

void SwgCuiDecoratorCameraPanel::OnButtonPressed(UIWidget * context)
{
	if (context == m_buttonDropTerrain)
	{
		CuiFurnitureMovementManager::dropSelectionToTerrain();
		return;
	}
	if (context == m_buttonHelp)
	{
		CuiFurnitureMovementManager::sendControlsHelp();
		return;
	}
	if (context == m_buttonResetSelection)
	{
		CuiFurnitureMovementManager::resetSelectionRotation();
		return;
	}
	if (context == m_buttonSpawning)
	{
		CuiFurnitureMovementManager::openSpawnUI();
		return;
	}
}

//----------------------------------------------------------------------

void SwgCuiDecoratorCameraPanel::OnCheckboxSet(UIWidget * context)
{
	if (context == m_checkSnapHorizontal)
		CuiFurnitureMovementManager::setSnapToGridHorizontal(true);
	else if (context == m_checkSnapVertical)
		CuiFurnitureMovementManager::setSnapToGridVertical(true);
	else if (context == m_checkFocusCamera)
		CuiFurnitureMovementManager::setFocusCameraOnSelection(true);
	else if (context == m_checkGimbalMovement)
		CuiFurnitureMovementManager::setUseGimbalMovement(true);
	else if (context == m_checkMouse4Drag)
		CuiFurnitureMovementManager::setUseMouse4Drag(true);
	else if (context == m_checkDragOnTerrain)
		CuiFurnitureMovementManager::setDragOnTerrain(true);
}

//----------------------------------------------------------------------

void SwgCuiDecoratorCameraPanel::OnCheckboxUnset(UIWidget * context)
{
	if (context == m_checkSnapHorizontal)
		CuiFurnitureMovementManager::setSnapToGridHorizontal(false);
	else if (context == m_checkSnapVertical)
		CuiFurnitureMovementManager::setSnapToGridVertical(false);
	else if (context == m_checkFocusCamera)
		CuiFurnitureMovementManager::setFocusCameraOnSelection(false);
	else if (context == m_checkGimbalMovement)
		CuiFurnitureMovementManager::setUseGimbalMovement(false);
	else if (context == m_checkMouse4Drag)
		CuiFurnitureMovementManager::setUseMouse4Drag(false);
	else if (context == m_checkDragOnTerrain)
		CuiFurnitureMovementManager::setDragOnTerrain(false);
}

//======================================================================
