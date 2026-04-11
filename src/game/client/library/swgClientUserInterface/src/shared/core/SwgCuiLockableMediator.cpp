//======================================================================
//
// SwgCuiLockableMediator.cpp
// copyright (c) 2008 Sony Online Entertainment
//
//======================================================================

#include "swgClientUserInterface/FirstSwgClientUserInterface.h"
#include "swgClientUserInterface/SwgCuiLockableMediator.h"

#include "clientUserInterface/CuiMenuInfoHelper.h"
#include "clientUserInterface/CuiMenuInfoTypes.h"
#include "clientUserInterface/CuiSettings.h"

#include "UIButton.h"
#include "UIManager.h"
#include "UIMessage.h"
#include "UIPage.h"
#include "UIPopupMenu.h"
#include "UIPopupMenustyle.h"
#include "UISliderbar.h"
#include "UILowerString.h"
#include "UIUtils.h"
#include "UIWidget.h"
#include "UnicodeUtils.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>


namespace SwgCuiLockableMediatorNameSpace
{
	namespace PopupIds
	{
		const std::string lock_window   = "window_lock";
		const std::string unlock_window = "window_unlock";
		const std::string window_scale  = "window_scale";
	}
	namespace Settings
	{
		const std::string popupHelpEnabled = "popupHelpEnabled";
		const std::string userMovable      = "UserMovable";
		const std::string windowContentScale = "windowContentScale";
		const UILowerString unlockedStateFlags("unlockedflags");
	}

	// Client-only synthetic menu type for radial/helper menus (must not collide with real Cui::MenuInfoTypes values).
	int const WINDOW_SCALE_MENU_TYPE = 55502;

	UILowerString const g_cuiContentScaleProperty ("CuiContentScale");

	struct WindowScaleDialogCallback : UIEventCallback
	{
		SwgCuiLockableMediator *target;
		UIPage *dialog;
		UISliderbar *slider;

		WindowScaleDialogCallback () : target (0), dialog (0), slider (0) {}

		void OnSliderbarChanged (UIWidget *context)
		{
			if (target && slider && context == slider)
			{
				long const v = slider->GetValue ();
				float const f = static_cast<float>(v) * 0.01f;
				target->setWindowContentScaleFactor (f);
			}
		}

		void OnButtonPressed (UIWidget *context)
		{
			if (!target || !dialog || !context)
				return;
			if (context->GetName () != "ok")
				return;
			target->saveSettings ();
			UIManager::gUIManager ().PopContextWidgets (dialog);
			dialog->Destroy ();
			dialog = 0;
			slider = 0;
			target = 0;
		}
	};

	WindowScaleDialogCallback s_windowScaleDialogCb;

	inline void setFlag(uint32 & field, uint32 flag, bool onOff)
	{
		if(onOff)
		{
			field |= flag;
		}
		else
		{
			field &= ~flag;
		}
	}

	inline bool isFlagSet(uint32 field, uint32 flag)
	{
		return (field & flag) != 0;
	}
}

using namespace SwgCuiLockableMediatorNameSpace;


SwgCuiLockableMediator::SwgCuiLockableMediator(const char * const mediatorDebugName, UIPage & newPage):
CuiMediator(mediatorDebugName, newPage),
UIEventCallback(),
m_pageLockFlags(LTF_none),
m_pageToLock(&newPage)
{
	setupUnlockedState();
}

SwgCuiLockableMediator::~SwgCuiLockableMediator(void)
{
	m_pageToLock = NULL;
}

//----------------------------------------------------------------------

void SwgCuiLockableMediator::generateLockablePopup  (UIWidget * context, const UIMessage & msg)
{
	if (!context || !context->IsA(TUIPage) || !m_pageToLock)
		return;

	UIPopupMenu * const pop = new UIPopupMenu(m_pageToLock);

	if (!pop)
		return;


	pop->SetStyle(m_pageToLock->FindPopupStyle());

	pop->AddItem(PopupIds::window_scale, Unicode::narrowToWide ("Scale"));

	if (!getPageIsLocked())
	{
		pop->AddItem(PopupIds::lock_window, Cui::MenuInfoTypes::getLocalizedLabel(Cui::MenuInfoTypes::WINDOW_LOCK, 0));
	}
	else
	{
		pop->AddItem(PopupIds::unlock_window, Cui::MenuInfoTypes::getLocalizedLabel(Cui::MenuInfoTypes::WINDOW_UNLOCK, 0));
	}

	pop->SetLocation(context->GetWorldLocation() + msg.MouseCoords);
	UIManager::gUIManager().PushContextWidget(*pop);
	pop->AddCallback(getCallbackObject());
}

//----------------------------------------------------------------------

void SwgCuiLockableMediator::OnPopupMenuSelection (UIWidget * context)
{		
	if (!context || !context->IsA(TUIPopupMenu) || !m_pageToLock)
		return;

	UIPopupMenu * const pop = safe_cast<UIPopupMenu *>(context);

	if (!pop)
		return;

	const std::string & selection = pop->GetSelectedName();
	const int menuSelection = atoi(selection.c_str());

	if (selection == PopupIds::window_scale)
	{
		showWindowContentScaleDialog ();
	}
	else if (menuSelection == WINDOW_SCALE_MENU_TYPE)
	{
		showWindowContentScaleDialog ();
	}
	else if (selection == PopupIds::lock_window || menuSelection == Cui::MenuInfoTypes::WINDOW_LOCK)
	{
		setPageLocked(true);
	}
	else if (selection == PopupIds::unlock_window || menuSelection == Cui::MenuInfoTypes::WINDOW_UNLOCK)
	{
		setPageLocked(false);
	}
}

//----------------------------------------------------------------------

bool SwgCuiLockableMediator::OnMessage(UIWidget *context, const UIMessage & msg)
{
	if(!context || !context->IsA(TUIPage) || !m_pageToLock )
		return true;

	if (msg.Type == UIMessage::RightMouseUp)
	{
		generateLockablePopup(context, msg);
		return false;
	}
	
	return true;
}

//----------------------------------------------------------------------

void SwgCuiLockableMediator::appendPopupOptions (UIPopupMenu * pop)
{
	if (!pop || !m_pageToLock)
		return;

	pop->AddItem(PopupIds::window_scale, Unicode::narrowToWide ("Scale"));

	if (!getPageIsLocked())
	{
		pop->AddItem(PopupIds::lock_window, Cui::MenuInfoTypes::getLocalizedLabel(Cui::MenuInfoTypes::WINDOW_LOCK, 0));
	}
	else
	{
		pop->AddItem(PopupIds::unlock_window, Cui::MenuInfoTypes::getLocalizedLabel(Cui::MenuInfoTypes::WINDOW_UNLOCK, 0));
	}
}

//----------------------------------------------------------------------

void SwgCuiLockableMediator::appendHelperPopupOptions (CuiMenuInfoHelper * menuHelper)
{
	if (!menuHelper || !m_pageToLock)
		return;

	IGNORE_RETURN (menuHelper->addRootMenu (static_cast<Cui::MenuInfoTypes::Type>(WINDOW_SCALE_MENU_TYPE), Unicode::narrowToWide ("Scale")));

	if (!getPageIsLocked())
	{
		menuHelper->addRootMenu(Cui::MenuInfoTypes::WINDOW_LOCK, Cui::MenuInfoTypes::getLocalizedLabel(Cui::MenuInfoTypes::WINDOW_LOCK, 0));
	}
	else
	{
		menuHelper->addRootMenu(Cui::MenuInfoTypes::WINDOW_UNLOCK, Cui::MenuInfoTypes::getLocalizedLabel(Cui::MenuInfoTypes::WINDOW_UNLOCK, 0));
	}
}

//----------------------------------------------------------------------

void SwgCuiLockableMediator::setDefaultWindowResizable(bool b)
{
	setFlag(m_pageLockFlags, LTF_resizable, b);
}

//----------------------------------------------------------------------

void SwgCuiLockableMediator::setDefaultWindowUserMovable(bool b)
{
	setFlag(m_pageLockFlags, LTF_userMovable, b);
}

//----------------------------------------------------------------------

void SwgCuiLockableMediator::setDefaultWindowAcceptsChildMove(bool b)
{
	setFlag(m_pageLockFlags, LTF_acceptsChildMove, b);
}

//----------------------------------------------------------------------

void SwgCuiLockableMediator::setPageToLock(UIPage * page)
{
	m_pageToLock = page;
	setupUnlockedState();
}

//----------------------------------------------------------------------

UIPage * SwgCuiLockableMediator::getPageToLock (void)
{
	return m_pageToLock;
}

//----------------------------------------------------------------------

bool SwgCuiLockableMediator::getPageIsLocked() const
{
	//since things can manually change the properties that make a page
	//'locked' we will only return true if the page is completely locked
	bool const notLocked = (m_pageToLock == NULL)
		|| (isFlagSet(m_pageLockFlags, LTF_userMovable) && m_pageToLock->IsUserMovable())
		|| (isFlagSet(m_pageLockFlags, LTF_resizable) && m_pageToLock->IsUserResizable())
		|| (isFlagSet(m_pageLockFlags, LTF_acceptsChildMove) && m_pageToLock->GetAcceptsMoveFromChildren());

	return !notLocked;
}

//----------------------------------------------------------------------

bool SwgCuiLockableMediator::setPageLocked(bool lockIt)
{
	if (m_pageToLock != NULL)
	{
		if(isFlagSet(m_pageLockFlags, LTF_userMovable))
			m_pageToLock->SetUserMovable(!lockIt);

		if(isFlagSet(m_pageLockFlags, LTF_acceptsChildMove))
			m_pageToLock->SetAcceptsMoveFromChildren(!lockIt);

		if (isFlagSet(m_pageLockFlags, LTF_resizable))
			m_pageToLock->SetUserResizable(!lockIt);

		return true;
	}
	return false;
}

//----------------------------------------------------------------------

void SwgCuiLockableMediator::setupUnlockedState()
{
	m_pageLockFlags = LTF_none;
	if(m_pageToLock)
	{
		int storedFlags = LTF_none;
		UIString propertyString;
		//if this page has a cached unlocked state (meaning it may have had its properties
		//changed via a lock) use the cached version instead of the current one
		if(m_pageToLock->GetProperty(Settings::unlockedStateFlags, propertyString) && UIUtils::ParseInteger(propertyString, storedFlags))
		{
			setDefaultWindowResizable(isFlagSet(storedFlags, LTF_resizable));
			setDefaultWindowUserMovable(isFlagSet(storedFlags, LTF_userMovable));
			setDefaultWindowAcceptsChildMove(isFlagSet(storedFlags, LTF_acceptsChildMove));
		}
		else
		{
			setDefaultWindowResizable(m_pageToLock->IsUserResizable());
			setDefaultWindowUserMovable(m_pageToLock->IsUserMovable());
			setDefaultWindowAcceptsChildMove(m_pageToLock->GetAcceptsMoveFromChildren());
			m_pageToLock->SetPropertyInteger(Settings::unlockedStateFlags, m_pageLockFlags);
		}
	}
}

//----------------------------------------------------------------------

void SwgCuiLockableMediator::loadSizeLocation (bool doSize, bool doLocation)
{
	UISize size  = getPage ().GetSize ();
	UIPoint loc  = getPage ().GetLocation ();

	if (doSize)
		CuiSettings::loadSize (getMediatorDebugName(), size);

	if (doLocation)
		CuiSettings::loadLocation (getMediatorDebugName(), loc);

	if (m_pageToLock)
	{
		bool isMovable = true;
		CuiSettings::loadBoolean(getMediatorDebugName(), Settings::userMovable, isMovable);

		setPageLocked(!isMovable);

		std::string scaleData;
		if (CuiSettings::loadData (getMediatorDebugName (), Settings::windowContentScale, scaleData))
		{
			float const f = static_cast<float>(atof (scaleData.c_str ()));
			if (f >= 0.25f && f <= 4.f)
				setWindowContentScaleFactor (f);
		}
 	}

	UIRect rect (loc, size);	

	getPage ().SetRect (rect);
}

//----------------------------------------------------------------------

void SwgCuiLockableMediator::saveSettings () const
{
	if (getPopupHelpData())
		CuiSettings::saveBoolean (getMediatorDebugName(), Settings::popupHelpEnabled, hasState (MS_popupHelpOk));

	if (hasState (MS_settingsAutoSize))
	{
		const UISize & size = getPage ().GetSize ();
		CuiSettings::saveSize     (getMediatorDebugName(), size);
	}

	if (hasState (MS_settingsAutoLoc))
	{
		const UIPoint & loc = getPage ().GetLocation ();
		CuiSettings::saveLocation (getMediatorDebugName(), loc);
	}

	if (m_pageToLock)
	{
		bool const isMovable = !getPageIsLocked();
		CuiSettings::saveBoolean(getMediatorDebugName(), Settings::userMovable, isMovable);

		char buf [64];
		_snprintf (buf, sizeof (buf), "%.4f", getWindowContentScaleFactor ());
		buf[sizeof (buf) - 1] = 0;
		CuiSettings::saveData (getMediatorDebugName (), Settings::windowContentScale, buf);
	}
}

//----------------------------------------------------------------------

void SwgCuiLockableMediator::setWindowContentScaleFactor (float scaleFactor)
{
	if (!m_pageToLock)
		return;
	if (scaleFactor < 0.25f)
		scaleFactor = 0.25f;
	else if (scaleFactor > 4.f)
		scaleFactor = 4.f;
	m_pageToLock->SetPropertyFloat (g_cuiContentScaleProperty, scaleFactor);
}

//----------------------------------------------------------------------

float SwgCuiLockableMediator::getWindowContentScaleFactor () const
{
	if (!m_pageToLock)
		return 1.f;
	float f = 1.f;
	if (!m_pageToLock->GetPropertyFloat (g_cuiContentScaleProperty, f))
		return 1.f;
	return f;
}

//----------------------------------------------------------------------

void SwgCuiLockableMediator::showWindowContentScaleDialog ()
{
	if (s_windowScaleDialogCb.dialog)
	{
		UIManager::gUIManager ().PopContextWidgets (s_windowScaleDialogCb.dialog);
		s_windowScaleDialogCb.dialog->Destroy ();
		s_windowScaleDialogCb.dialog = 0;
		s_windowScaleDialogCb.slider = 0;
		s_windowScaleDialogCb.target = 0;
	}

	UIPage const *const proto = safe_cast<UIPage const *>(UIManager::gUIManager ().GetObjectFromPath ("/WindowScalePopup", TUIPage));
	if (!proto)
		return;

	UIPage *const dlg = safe_cast<UIPage *>(proto->DuplicateObject ());
	if (!dlg)
		return;

	UISliderbar *const slider = safe_cast<UISliderbar *>(dlg->GetChild ("slider"));
	UIButton *const okBtn    = safe_cast<UIButton *>(dlg->GetChild ("ok"));
	if (!slider || !okBtn)
	{
		dlg->Destroy ();
		return;
	}

	s_windowScaleDialogCb.target = this;
	s_windowScaleDialogCb.dialog = dlg;
	s_windowScaleDialogCb.slider = slider;

	long const v = static_cast<long>(getWindowContentScaleFactor () * 100.f + 0.5f);
	slider->SetValue (std::max (50L, std::min (200L, v)), false);

	dlg->AddCallback (&s_windowScaleDialogCb);
	okBtn->AddCallback (&s_windowScaleDialogCb);
	slider->AddCallback (&s_windowScaleDialogCb);

	UIManager::gUIManager ().PushContextWidget (*dlg, UIManager::CWA_Center, true);
}

//======================================================================