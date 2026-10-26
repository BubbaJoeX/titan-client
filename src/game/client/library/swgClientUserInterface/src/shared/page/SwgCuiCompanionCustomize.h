//======================================================================

//

// SwgCuiCompanionCustomize.h

// copyright (c) 2001 Sony Online Entertainment

//

//======================================================================



#ifndef INCLUDED_SwgCuiCompanionCustomize_H

#define INCLUDED_SwgCuiCompanionCustomize_H



//======================================================================



#include "swgClientUserInterface/SwgCuiAvatarCustomizationBase.h"



class UIButton;

class UIPage;

class CreatureObject;



//----------------------------------------------------------------------



class SwgCuiCompanionCustomize : public SwgCuiAvatarCustomizationBase

{

public:

	explicit SwgCuiCompanionCustomize(UIPage & page);

	~SwgCuiCompanionCustomize();



protected:

	virtual void performActivate();

	virtual void performDeactivate();

	virtual void OnButtonPressed(UIWidget *context);

	virtual void updateSelectedHairColorState();

	virtual void setGroup(std::string const & groupName);

	virtual std::string getCustomizationSpeciesGender(ClientObject const & object) const;
	virtual std::string getHairStyleTemplateName(ClientObject const & object) const;



private:

	SwgCuiCompanionCustomize();

	SwgCuiCompanionCustomize(const SwgCuiCompanionCustomize & rhs);

	SwgCuiCompanionCustomize & operator= (const SwgCuiCompanionCustomize & rhs);



	void setColorPickerColumnAndMaxIndexes() const;

	void hideImageDesignerSessionWidgets() const;



private:

	UIButton * m_buttonCommit;

	UIButton * m_buttonCancel;

	UIButton * m_buttonRevert;

	UIPage *   m_hairPageOverlay;

	CreatureObject * m_previewCompanion;

};



//======================================================================



#endif

