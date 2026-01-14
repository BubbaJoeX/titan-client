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
class UICheckbox;
class UIPage;
class UISliderbar;
class UITabbedPane;
class UITreeView;
class UIText;
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
	virtual void                OnTabbedPaneChanged       (UIWidget * context);
	virtual void                OnCheckboxSet             (UIWidget * context);
	virtual void                OnCheckboxUnset           (UIWidget * context);
	virtual void                OnSliderbarChanged        (UIWidget * context);

	static SwgCuiDecoratorSpawn * createInto              (UIPage & parent);

private:
	                           ~SwgCuiDecoratorSpawn      ();
	                            SwgCuiDecoratorSpawn      (SwgCuiDecoratorSpawn const &);
	SwgCuiDecoratorSpawn &      operator=                 (SwgCuiDecoratorSpawn const &);

	void                        populateTree              ();
	void                        populateTemplateList      ();
	void                        updateTreeDisplay         ();
	void                        filterTree                (Unicode::String const & filter);
	void                        performSearch             ();
	void                        nextPage                  ();
	void                        prevPage                  ();
	void                        updatePreview             ();
	void                        updateInfoDisplay         ();
	void                        updateViewerSettings      ();
	void                        spawnSelectedObject       ();
	std::string                 getSelectedTemplatePath   () const;
	std::string                 convertToServerTemplate   (std::string const & sharedTemplate) const;
	std::string                 getDisplayNameForTemplate (std::string const & templatePath) const;

	UITreeView *                      m_tree;
	UIButton *                        m_buttonPreview;
	UIButton *                        m_buttonSpawn;
	UIButton *                        m_buttonClose;
	UIButton *                        m_buttonNextPage;
	UIButton *                        m_buttonPrevPage;
	UIButton *                        m_buttonSearch;
	UIText *                          m_filterTextbox;
	UIText *                          m_selectedText;
	UIText *                          m_pageText;
	CuiWidget3dObjectListViewer *     m_viewer;

	// Tabs
	UITabbedPane *                    m_tabs;
	UIPage *                          m_searchPage;
	UIPage *                          m_settingsPage;
	UIPage *                          m_infoPage;
	UIText *                          m_infoText;
	
	// Settings controls
	UICheckbox *                      m_checkAutoZoom;
	UICheckbox *                      m_checkHeadshot;
	UICheckbox *                      m_checkRotate;
	UICheckbox *                      m_checkDragYaw;
	UICheckbox *                      m_checkDragPitch;
	UICheckbox *                      m_checkShadows;
	UISliderbar *                     m_sliderFOV;
	UISliderbar *                     m_sliderFitDistance;
	UISliderbar *                     m_sliderCameraYaw;
	UISliderbar *                     m_sliderCameraPitch;
	UISliderbar *                     m_sliderLightYaw;
	UISliderbar *                     m_sliderLightPitch;
	UISliderbar *                     m_sliderAmbientR;
	UISliderbar *                     m_sliderAmbientG;
	UISliderbar *                     m_sliderAmbientB;
	UISliderbar *                     m_sliderLightR;
	UISliderbar *                     m_sliderLightG;
	UISliderbar *                     m_sliderLightB;

	typedef stdvector<std::string>::fwd TemplateList;
	TemplateList *                    m_allTemplates;
	TemplateList *                    m_filteredTemplates;
	
	std::string                       m_currentFilter;
	int                               m_currentPage;
	int                               m_totalPages;
	bool                              m_templatesLoaded;
	
	static int const                  ms_itemsPerPage;
};

//======================================================================

#endif
