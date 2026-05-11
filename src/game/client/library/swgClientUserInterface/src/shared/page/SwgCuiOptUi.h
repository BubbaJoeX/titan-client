//======================================================================
//
// SwgCuiOptUi.h
// copyright (c) 2003 Sony Online Entertainment
//
//======================================================================

#ifndef INCLUDED_SwgCuiOptUi_H
#define INCLUDED_SwgCuiOptUi_H

#include "swgClientUserInterface/SwgCuiOptBase.h"

#include <string>

struct UIMessage;

//======================================================================

class UIComboBox;
class UIText;
class UITextbox;
class UIButton;
class UITextboxStyle;

class SwgCuiOptUi : 
public SwgCuiOptBase
{
public:
	explicit SwgCuiOptUi (UIPage & page);

	static int  onComboPaletteGet    (const SwgCuiOptBase & , const UIComboBox &);
	static void onComboPaletteSet    (const SwgCuiOptBase & , const UIComboBox & , int value);

	virtual void             resetDefaults     (bool confirmed);

	virtual void             OnButtonPressed   (UIWidget * context);
	virtual void             OnTextboxChanged  (UIWidget * context);
	virtual bool             OnMessage         (UIWidget * context, UIMessage const & msg);

protected:
	void           performActivate ();
	void           performDeactivate ();

private:

	~SwgCuiOptUi ();
	SwgCuiOptUi & operator=(const SwgCuiOptUi & rhs);
	SwgCuiOptUi            (const SwgCuiOptUi & rhs);

	UIComboBox *   m_combo;

	UIComboBox *     m_comboUiFont;
	UITextbox *      m_textUiFontFilter;
	UIButton *       m_buttonUiFontSearch;
	UIButton *       m_buttonUiFontApply;
	UIText *         m_textUiFontSample;
	UITextboxStyle * m_ownedComboTextboxStyleClone;
	UITextboxStyle * m_ownedFilterTextboxStyleClone;

	void             rebuildUiFontCombo ();
	void             selectComboFaceUtf8 (std::string const &utf8Face);
	void             syncComboSelectionAfterRebuild ();
	void             performFontSearchRefresh ();
	void             refreshFontPickerChrome ();
	void             updateFontSampleText ();
	void             releaseOwnedFontPickerTextboxStyles ();
	
	class CallbackReceiverWaypointMonitor;
	CallbackReceiverWaypointMonitor * m_callbackReceiverWaypointMonitor;
	
	class CallbackReceiverExpMonitor;
	CallbackReceiverExpMonitor * m_callbackReceiverExpMonitor;

	class CallbackReceiverLocationDisplay;
	CallbackReceiverLocationDisplay * m_callbackReceiverLocationDisplay;
};

//======================================================================

#endif
