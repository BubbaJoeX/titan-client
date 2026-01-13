//======================================================================
//
// SwgCuiDecoratorSpawn.h
// copyright (c) 2024
//
// Object spawning UI for decorator camera mode
//
//======================================================================

#ifndef INCLUDED_SwgCuiDecoratorSpawn_H
#define INCLUDED_SwgCuiDecoratorSpawn_H

//======================================================================

#include "clientUserInterface/CuiMediator.h"
#include "UIEventCallback.h"

class UIButton;
class UITreeView;
class UIText;
class UITextbox;
class CuiWidget3dObjectListViewer;

//----------------------------------------------------------------------

class SwgCuiDecoratorSpawn :
public CuiMediator,
public UIEventCallback
{
public:
	explicit                    SwgCuiDecoratorSpawn      (UIPage & page);

	virtual void                performActivate           ();
	virtual void                performDeactivate         ();

	virtual void                OnButtonPressed           (UIWidget * context);
	virtual void                OnGenericSelectionChanged (UIWidget * context);
	virtual void                OnTextboxChanged          (UIWidget * context);

	static SwgCuiDecoratorSpawn * createInto              (UIPage & parent);

private:
	                           ~SwgCuiDecoratorSpawn      ();
	                            SwgCuiDecoratorSpawn      (SwgCuiDecoratorSpawn const &);
	SwgCuiDecoratorSpawn &      operator=                 (SwgCuiDecoratorSpawn const &);

	void                        populateTree              ();
	void                        filterTree                (Unicode::String const & filter);
	void                        updatePreview             ();
	void                        spawnSelectedObject       ();
	std::string                 getSelectedTemplatePath   () const;
	std::string                 convertToServerTemplate   (std::string const & sharedTemplate) const;

	UITreeView *                      m_tree;
	UIButton *                        m_buttonPreview;
	UIButton *                        m_buttonSpawn;
	UIButton *                        m_buttonClose;
	UITextbox *                       m_filterTextbox;
	UIText *                          m_selectedText;
	CuiWidget3dObjectListViewer *     m_viewer;

	typedef stdvector<std::string>::fwd TemplateList;
	TemplateList *                    m_allTemplates;
};

//======================================================================

#endif
