//======================================================================
//
// SwgCuiCompanionTab.h
// copyright (c) 2008 Sony Online Entertainment
//
//======================================================================

#ifndef INCLUDED_SwgCuiCompanionTab_H
#define INCLUDED_SwgCuiCompanionTab_H

//======================================================================
#include "clientUserInterface/CuiMediator.h"

#include "../../../../../../engine/shared/library/sharedFoundation/include/public/sharedFoundation/Timer.h"

#include "UIEventCallback.h"
//----------------------------------------------------------------------

class CuiWidget3dObjectListViewer;
class UIPage;
class UIText;
class UIButton;
class UICheckbox;
class CuiDragInfo;

#include <vector>
//----------------------------------------------------------------------

class SwgCuiCompanionTab: public CuiMediator, public UIEventCallback
{
public:
	explicit SwgCuiCompanionTab(UIPage & page);
	~SwgCuiCompanionTab();

	virtual void update(float delta);

	void         OnButtonPressed(UIWidget *context );
	void         OnCheckboxSet(UIWidget *context);
	void         OnCheckboxUnset(UIWidget *context);

	bool         OnMessage(UIWidget *Context, const UIMessage & msg );

	typedef stdvector<UIText *>::fwd                      TextVector;
	typedef stdvector<CuiWidget3dObjectListViewer *>::fwd ViewerVector;

protected:
	virtual void performActivate();
	virtual void performDeactivate();

private:
	SwgCuiCompanionTab(const SwgCuiCompanionTab & rhs);
	SwgCuiCompanionTab & operator= (const SwgCuiCompanionTab & rhs);

private:

	CuiWidget3dObjectListViewer * m_characterViewer;
	UIText*                       m_characterName;
	UIButton*                     m_closeButton;
	UIButton*                     m_renameButton;
	UIButton*                     m_imageDesignButton;
	UICheckbox*                   m_showInventoryItems;
	UIPage*                       m_viewerPage;

	ViewerVector                  m_slotViewers;
	TextVector                    m_slotText;

	CuiDragInfo *                 m_lastDragItem;
	Timer                         m_dragTimer;
};

//======================================================================

#endif
