//======================================================================
//
// SwgCuiDecoratorSpawn.cpp
// copyright (c) 2024
//
// Object spawning UI for decorator camera mode
//
//======================================================================

#include "swgClientUserInterface/FirstSwgClientUserInterface.h"
#include "swgClientUserInterface/SwgCuiDecoratorSpawn.h"

#include "clientGame/ClientCommandQueue.h"
#include "clientGame/Game.h"
#include "clientUserInterface/CuiFurnitureMovementManager.h"
#include "clientUserInterface/CuiManager.h"
#include "clientUserInterface/CuiWidget3dObjectListViewer.h"
#include "sharedFoundation/Crc.h"
#include "sharedGame/SharedObjectTemplate.h"
#include "sharedObject/Appearance.h"
#include "sharedObject/Hardpoint.h"
#include "sharedObject/ObjectTemplate.h"
#include "sharedObject/ObjectTemplateList.h"
#include "StringId.h"
#include "UIButton.h"
#include "UICheckbox.h"
#include "UIData.h"
#include "UIDataSource.h"
#include "UIDataSourceContainer.h"
#include "UIPage.h"
#include "UISliderbar.h"
#include "UITabbedPane.h"
#include "UIText.h"
#include "UITreeView.h"
#include "UnicodeUtils.h"

#include <algorithm>
#include <map>

//======================================================================

namespace SwgCuiDecoratorSpawnNamespace
{
	std::string const cms_objectPrefix = "object/";
	std::string const cms_objectIntangiblePrefix = "object/intangible/";
	
	bool startsWith(std::string const & str, std::string const & prefix)
	{
		if (prefix.size() > str.size()) return false;
		return str.compare(0, prefix.size(), prefix) == 0;
	}
	
	bool containsIgnoreCase(std::string const & str, std::string const & substr)
	{
		if (substr.empty()) return true;
		std::string strLower = str;
		std::string substrLower = substr;
		std::transform(strLower.begin(), strLower.end(), strLower.begin(), ::tolower);
		std::transform(substrLower.begin(), substrLower.end(), substrLower.begin(), ::tolower);
		return strLower.find(substrLower) != std::string::npos;
	}
}

using namespace SwgCuiDecoratorSpawnNamespace;

//======================================================================

int const SwgCuiDecoratorSpawn::ms_itemsPerPage = 100;

//======================================================================

SwgCuiDecoratorSpawn::SwgCuiDecoratorSpawn(UIPage & page) :
CuiMediator("SwgCuiDecoratorSpawn", page),
UIEventCallback(),
m_tree(NULL),
m_buttonPreview(NULL),
m_buttonSpawn(NULL),
m_buttonClose(NULL),
m_buttonNextPage(NULL),
m_buttonPrevPage(NULL),
m_buttonSearch(NULL),
m_filterTextbox(NULL),
m_selectedText(NULL),
m_pageText(NULL),
m_viewer(NULL),
m_tabs(NULL),
m_searchPage(NULL),
m_settingsPage(NULL),
m_infoPage(NULL),
m_infoText(NULL),
m_checkAutoZoom(NULL),
m_checkHeadshot(NULL),
m_checkRotate(NULL),
m_checkDragYaw(NULL),
m_checkDragPitch(NULL),
m_checkShadows(NULL),
m_sliderFOV(NULL),
m_sliderFitDistance(NULL),
m_sliderCameraYaw(NULL),
m_sliderCameraPitch(NULL),
m_sliderLightYaw(NULL),
m_sliderLightPitch(NULL),
m_sliderAmbientR(NULL),
m_sliderAmbientG(NULL),
m_sliderAmbientB(NULL),
m_sliderLightR(NULL),
m_sliderLightG(NULL),
m_sliderLightB(NULL),
m_openedFromDecoratorCamera(false),
m_allTemplates(new TemplateList),
m_filteredTemplates(new TemplateList),
m_currentFilter(),
m_currentPage(0),
m_totalPages(0),
m_templatesLoaded(false)
{
	// Get UI elements from CodeData
	getCodeDataObject(TUITreeView, m_tree, "tree");
	getCodeDataObject(TUIButton, m_buttonPreview, "buttonPreview");
	getCodeDataObject(TUIButton, m_buttonSpawn, "buttonSpawn");
	getCodeDataObject(TUIButton, m_buttonClose, "buttonClose");
	getCodeDataObject(TUIButton, m_buttonSearch, "buttonSearch", true);
	getCodeDataObject(TUIText, m_filterTextbox, "filterTextbox");
	getCodeDataObject(TUIText, m_selectedText, "selectedText");
	getCodeDataObject(TUIText, m_pageText, "pageText", true);
	
	// Tabs
	getCodeDataObject(TUITabbedPane, m_tabs, "tabs", true);
	
	// Settings controls
	getCodeDataObject(TUICheckbox, m_checkAutoZoom, "checkAutoZoom", true);
	getCodeDataObject(TUICheckbox, m_checkHeadshot, "checkHeadshot", true);
	getCodeDataObject(TUICheckbox, m_checkRotate, "checkRotate", true);
	getCodeDataObject(TUICheckbox, m_checkDragYaw, "checkDragYaw", true);
	getCodeDataObject(TUICheckbox, m_checkDragPitch, "checkDragPitch", true);
	getCodeDataObject(TUICheckbox, m_checkShadows, "checkShadows", true);
	getCodeDataObject(TUISliderbar, m_sliderFOV, "sliderFOV", true);
	getCodeDataObject(TUISliderbar, m_sliderFitDistance, "sliderFitDistance", true);
	getCodeDataObject(TUISliderbar, m_sliderCameraYaw, "sliderCameraYaw", true);
	getCodeDataObject(TUISliderbar, m_sliderCameraPitch, "sliderCameraPitch", true);
	getCodeDataObject(TUISliderbar, m_sliderLightYaw, "sliderLightYaw", true);
	getCodeDataObject(TUISliderbar, m_sliderLightPitch, "sliderLightPitch", true);
	getCodeDataObject(TUISliderbar, m_sliderAmbientR, "sliderAmbientR", true);
	getCodeDataObject(TUISliderbar, m_sliderAmbientG, "sliderAmbientG", true);
	getCodeDataObject(TUISliderbar, m_sliderAmbientB, "sliderAmbientB", true);
	getCodeDataObject(TUISliderbar, m_sliderLightR, "sliderLightR", true);
	getCodeDataObject(TUISliderbar, m_sliderLightG, "sliderLightG", true);
	getCodeDataObject(TUISliderbar, m_sliderLightB, "sliderLightB", true);
	
	UIWidget * viewerWidget = NULL;
	getCodeDataObject(TUIWidget, viewerWidget, "viewer");
	m_viewer = dynamic_cast<CuiWidget3dObjectListViewer *>(viewerWidget);
	
	// Tabs
	getCodeDataObject(TUITabbedPane, m_tabs, "tabs", true);
	getCodeDataObject(TUIText, m_infoText, "infoText", true);
	
	// Get tab pages for visibility toggling
	UIPage * tabsContent = dynamic_cast<UIPage *>(getPage().GetChild("tabsContent"));
	if (tabsContent)
	{
		m_searchPage = dynamic_cast<UIPage *>(tabsContent->GetChild("searchPage"));
		m_settingsPage = dynamic_cast<UIPage *>(tabsContent->GetChild("settingsPage"));
		m_infoPage = dynamic_cast<UIPage *>(tabsContent->GetChild("infoPage"));
	}
	
	registerMediatorObject(*m_tree, true);
	registerMediatorObject(*m_buttonPreview, true);
	registerMediatorObject(*m_buttonSpawn, true);
	registerMediatorObject(*m_buttonClose, true);
	registerMediatorObject(*m_filterTextbox, true);
	
	if (m_buttonSearch)
		registerMediatorObject(*m_buttonSearch, true);
	if (m_buttonNextPage)
		registerMediatorObject(*m_buttonNextPage, true);
	if (m_buttonPrevPage)
		registerMediatorObject(*m_buttonPrevPage, true);
	if (m_tabs)
		registerMediatorObject(*m_tabs, true);
	if (m_checkAutoZoom)
		registerMediatorObject(*m_checkAutoZoom, true);
	if (m_checkHeadshot)
		registerMediatorObject(*m_checkHeadshot, true);
	if (m_checkRotate)
		registerMediatorObject(*m_checkRotate, true);
	if (m_checkDragYaw)
		registerMediatorObject(*m_checkDragYaw, true);
	if (m_checkDragPitch)
		registerMediatorObject(*m_checkDragPitch, true);
	if (m_checkShadows)
		registerMediatorObject(*m_checkShadows, true);
	if (m_sliderFOV)
		registerMediatorObject(*m_sliderFOV, true);
	if (m_sliderFitDistance)
		registerMediatorObject(*m_sliderFitDistance, true);
	if (m_sliderCameraYaw)
		registerMediatorObject(*m_sliderCameraYaw, true);
	if (m_sliderCameraPitch)
		registerMediatorObject(*m_sliderCameraPitch, true);
	if (m_sliderLightYaw)
		registerMediatorObject(*m_sliderLightYaw, true);
	if (m_sliderLightPitch)
		registerMediatorObject(*m_sliderLightPitch, true);
	if (m_sliderAmbientR)
		registerMediatorObject(*m_sliderAmbientR, true);
	if (m_sliderAmbientG)
		registerMediatorObject(*m_sliderAmbientG, true);
	if (m_sliderAmbientB)
		registerMediatorObject(*m_sliderAmbientB, true);
	if (m_sliderLightR)
		registerMediatorObject(*m_sliderLightR, true);
	if (m_sliderLightG)
		registerMediatorObject(*m_sliderLightG, true);
	if (m_sliderLightB)
		registerMediatorObject(*m_sliderLightB, true);
}

//----------------------------------------------------------------------

SwgCuiDecoratorSpawn::~SwgCuiDecoratorSpawn()
{
	delete m_allTemplates;
	m_allTemplates = NULL;
	
	delete m_filteredTemplates;
	m_filteredTemplates = NULL;
	
	m_tree = NULL;
	m_buttonPreview = NULL;
	m_buttonSpawn = NULL;
	m_buttonClose = NULL;
	m_buttonNextPage = NULL;
	m_buttonPrevPage = NULL;
	m_buttonSearch = NULL;
	m_filterTextbox = NULL;
	m_selectedText = NULL;
	m_pageText = NULL;
	m_infoText = NULL;
	m_viewer = NULL;
	m_tabs = NULL;
	m_searchPage = NULL;
	m_settingsPage = NULL;
	m_infoPage = NULL;
	m_checkAutoZoom = NULL;
	m_checkHeadshot = NULL;
	m_checkRotate = NULL;
	m_checkDragYaw = NULL;
	m_checkDragPitch = NULL;
	m_checkShadows = NULL;
	m_sliderFOV = NULL;
	m_sliderFitDistance = NULL;
	m_sliderCameraYaw = NULL;
	m_sliderCameraPitch = NULL;
	m_sliderLightYaw = NULL;
	m_sliderLightPitch = NULL;
	m_sliderAmbientR = NULL;
	m_sliderAmbientG = NULL;
	m_sliderAmbientB = NULL;
	m_sliderLightR = NULL;
	m_sliderLightG = NULL;
	m_sliderLightB = NULL;
}

//----------------------------------------------------------------------

void SwgCuiDecoratorSpawn::performActivate()
{
	CuiMediator::performActivate();

	m_openedFromDecoratorCamera = CuiFurnitureMovementManager::isDecoratorCameraActive();
	
	// Request pointer for mouse interaction
	CuiManager::requestPointer(true);
	
	// Load templates only once
	if (!m_templatesLoaded)
	{
		populateTemplateList();
		m_templatesLoaded = true;
	}
	
	if (m_filterTextbox)
	{
		m_filterTextbox->SetEditable(true);
		m_filterTextbox->SetSelectable(true);
		m_filterTextbox->SetGetsInput(true);
		m_filterTextbox->SetLocalText(Unicode::emptyString);
		// Set focus to the textbox to capture keyboard input
		m_filterTextbox->SetFocus();
	}
	
	if (m_selectedText)
	{
		m_selectedText->SetLocalText(Unicode::narrowToWide("Type to search, then select an object"));
	}
	
	m_currentFilter.clear();
	m_currentPage = 0;
	m_filteredTemplates->clear();
	
	// Start with empty tree - user must search first
	updateTreeDisplay();
	
	setIsUpdating(true);
}

//----------------------------------------------------------------------

void SwgCuiDecoratorSpawn::performDeactivate()
{
	CuiMediator::performDeactivate();
	
	// Release pointer request
	CuiManager::requestPointer(false);
	
	setIsUpdating(false);
	
	// Clear the viewer
	if (m_viewer)
	{
		m_viewer->clearObjects();
	}
}

//----------------------------------------------------------------------

void SwgCuiDecoratorSpawn::OnButtonPressed(UIWidget * context)
{
	if (context == m_buttonPreview)
	{
		updatePreview();
	}
	else if (context == m_buttonSpawn)
	{
		spawnSelectedObject();
	}
	else if (context == m_buttonClose)
	{
		deactivate();
	}
	else if (context == m_buttonSearch)
	{
		performSearch();
	}
	else if (context == m_buttonNextPage)
	{
		nextPage();
	}
	else if (context == m_buttonPrevPage)
	{
		prevPage();
	}
}

//----------------------------------------------------------------------

void SwgCuiDecoratorSpawn::OnGenericSelectionChanged(UIWidget * context)
{
	if (context == m_tree)
	{
		std::string const path = getSelectedTemplatePath();
		if (!path.empty())
		{
			// Update selected text (localized name when from decorator camera, else path)
			if (m_selectedText)
			{
				std::string const display = getDisplayNameForTemplate(path);
				m_selectedText->SetLocalText(Unicode::narrowToWide(m_openedFromDecoratorCamera ? display : (display.empty() ? path : display)));
			}
			
			// Auto-update viewer and info on selection
			updatePreview();
		}
	}
}

//----------------------------------------------------------------------

void SwgCuiDecoratorSpawn::OnTextboxChanged(UIWidget * context)
{
	if (context == m_filterTextbox)
	{
		Unicode::String filterText;
		m_filterTextbox->GetLocalText(filterText);
		filterTree(filterText);
	}
}

//----------------------------------------------------------------------

void SwgCuiDecoratorSpawn::populateTree()
{
	if (!m_tree) return;
	
	// Get all object template names
	m_allTemplates->clear();
	
	stdvector<const char *>::fwd templateNames;
	ObjectTemplateList::getAllTemplateNamesFromCrcStringTable(templateNames);
	
	// Filter to object/ .iff files, exclude object/intangible/
	for (stdvector<const char *>::fwd::const_iterator it = templateNames.begin(); it != templateNames.end(); ++it)
	{
		std::string const name(*it);
		if (startsWith(name, cms_objectPrefix) && !startsWith(name, cms_objectIntangiblePrefix) && name.find(".iff") != std::string::npos)
		{
			m_allTemplates->push_back(name);
		}
	}
	
	// Sort alphabetically
	std::sort(m_allTemplates->begin(), m_allTemplates->end());
	
	// Build tree structure
	filterTree(Unicode::emptyString);
}

//----------------------------------------------------------------------

void SwgCuiDecoratorSpawn::populateTemplateList()
{
	m_allTemplates->clear();
	
	stdvector<const char *>::fwd templateNames;
	ObjectTemplateList::getAllTemplateNamesFromCrcStringTable(templateNames);
	
	// Filter to object/ .iff files, exclude object/intangible/
	for (stdvector<const char *>::fwd::const_iterator it = templateNames.begin(); it != templateNames.end(); ++it)
	{
		std::string const name(*it);
		if (startsWith(name, cms_objectPrefix) && !startsWith(name, cms_objectIntangiblePrefix) && name.find(".iff") != std::string::npos)
		{
			m_allTemplates->push_back(name);
		}
	}
	
	// Sort alphabetically
	std::sort(m_allTemplates->begin(), m_allTemplates->end());
}

//----------------------------------------------------------------------

void SwgCuiDecoratorSpawn::filterTree(Unicode::String const & filter)
{
	if (!m_tree) return;
	
	std::string const filterNarrow = Unicode::wideToNarrow(filter);
	
	UIDataSourceContainer * const dsc = m_tree->GetDataSourceContainer();
	if (!dsc) return;
	
	dsc->Clear();
	
	// Build a hierarchical tree from the paths
	typedef std::map<std::string, UIDataSourceContainer *> FolderMap;
	FolderMap folders;
	
	for (TemplateList::const_iterator it = m_allTemplates->begin(); it != m_allTemplates->end(); ++it)
	{
		std::string const & path = *it;
		
		// Apply filter
		if (!filterNarrow.empty() && !containsIgnoreCase(path, filterNarrow))
			continue;
		
		// Split path into parts
		std::string remaining = path;
		UIDataSourceContainer * parent = dsc;
		
		size_t pos;
		while ((pos = remaining.find('/')) != std::string::npos)
		{
			std::string const folder = remaining.substr(0, pos);
			remaining = remaining.substr(pos + 1);
			
			std::string const fullPath = path.substr(0, path.length() - remaining.length() - 1);
			
			FolderMap::iterator folderIt = folders.find(fullPath);
			if (folderIt == folders.end())
			{
				UIDataSourceContainer * newFolder = new UIDataSourceContainer;
				newFolder->SetName(folder);
				newFolder->SetProperty(UITreeView::DataProperties::LocalText, Unicode::narrowToWide(folder));
				parent->AddChild(newFolder);
				folders[fullPath] = newFolder;
				parent = newFolder;
			}
			else
			{
				parent = folderIt->second;
			}
		}
		
		// Add the file as a leaf node
		if (!remaining.empty())
		{
			UIDataSource * leaf = new UIDataSource;
			leaf->SetName(remaining);
			leaf->SetProperty(UITreeView::DataProperties::LocalText, Unicode::narrowToWide(remaining));
			leaf->SetProperty(UILowerString("TemplatePath"), Unicode::narrowToWide(path));
			parent->AddChild(leaf);
		}
	}
	
	m_tree->SetDataSourceContainer(dsc, true);
}

//----------------------------------------------------------------------

void SwgCuiDecoratorSpawn::performSearch()
{
	if (!m_filterTextbox) return;
	
	Unicode::String filterText;
	m_filterTextbox->GetLocalText(filterText);
	std::string const newFilter = Unicode::wideToNarrow(filterText);
	
	m_currentFilter = newFilter;
	m_currentPage = 0;
	
	// Filter the templates
	m_filteredTemplates->clear();
	
	if (m_currentFilter.length() >= 2)
	{
		for (TemplateList::const_iterator it = m_allTemplates->begin(); it != m_allTemplates->end(); ++it)
		{
			if (containsIgnoreCase(*it, m_currentFilter))
			{
				m_filteredTemplates->push_back(*it);
			}
		}
		
		// Sort results alphabetically A-Z
		std::sort(m_filteredTemplates->begin(), m_filteredTemplates->end());
	}
	
	// Calculate total pages
	int const totalItems = static_cast<int>(m_filteredTemplates->size());
	m_totalPages = (totalItems + ms_itemsPerPage - 1) / ms_itemsPerPage;
	if (m_totalPages < 1) m_totalPages = 1;
	
	updateTreeDisplay();
}

//----------------------------------------------------------------------

void SwgCuiDecoratorSpawn::nextPage()
{
	if (m_currentPage < m_totalPages - 1)
	{
		++m_currentPage;
		updateTreeDisplay();
	}
}

//----------------------------------------------------------------------

void SwgCuiDecoratorSpawn::prevPage()
{
	if (m_currentPage > 0)
	{
		--m_currentPage;
		updateTreeDisplay();
	}
}

//----------------------------------------------------------------------

void SwgCuiDecoratorSpawn::updateTreeDisplay()
{
	if (!m_tree) return;
	
	UIDataSourceContainer * const dsc = m_tree->GetDataSourceContainer();
	if (!dsc) return;
	
	dsc->Clear();
	
	// Update page text
	if (m_pageText)
	{
		char pageStr[64];
		snprintf(pageStr, sizeof(pageStr), "Page %d / %d (%d results)", 
			m_currentPage + 1, m_totalPages, static_cast<int>(m_filteredTemplates->size()));
		m_pageText->SetLocalText(Unicode::narrowToWide(pageStr));
	}
	
	// If no filter or filter too short, show instruction
	if (m_currentFilter.length() < 2)
	{
		UIDataSourceContainer * info = new UIDataSourceContainer;
		info->SetName("info");
		info->SetProperty(UITreeView::DataProperties::LocalText, Unicode::narrowToWide("Minimum of 2 characters to search..."));
		dsc->AddChild(info);
		m_tree->SetDataSourceContainer(dsc, true);
		return;
	}
	
	// Calculate page bounds
	int const startIdx = m_currentPage * ms_itemsPerPage;
	int const endIdx = std::min(startIdx + ms_itemsPerPage, static_cast<int>(m_filteredTemplates->size()));
	
	if (startIdx >= static_cast<int>(m_filteredTemplates->size()))
	{
		UIDataSourceContainer * info = new UIDataSourceContainer;
		info->SetName("info");
		info->SetProperty(UITreeView::DataProperties::LocalText, Unicode::narrowToWide("No results found"));
		dsc->AddChild(info);
		m_tree->SetDataSourceContainer(dsc, true);
		return;
	}
	
	// Build flat list for current page
	for (int i = startIdx; i < endIdx; ++i)
	{
		std::string const & path = (*m_filteredTemplates)[static_cast<size_t>(i)];
		
		// Get localized display name from StringId
		std::string displayName = getDisplayNameForTemplate(path);
		
		UIDataSourceContainer * item = new UIDataSourceContainer;
		item->SetName(displayName);
		item->SetProperty(UITreeView::DataProperties::LocalText, Unicode::narrowToWide(displayName));
		item->SetProperty(UILowerString("TemplatePath"), Unicode::narrowToWide(path));
		item->SetProperty(UITreeView::DataProperties::Selectable, Unicode::narrowToWide("true"));
		dsc->AddChild(item);
	}
	
	m_tree->SetDataSourceContainer(dsc, true);
}

//----------------------------------------------------------------------

void SwgCuiDecoratorSpawn::updatePreview()
{
	if (!m_viewer) return;
	
	std::string const path = getSelectedTemplatePath();
	if (path.empty()) return;
	
	// Clear existing objects
	m_viewer->clearObjects();
	
	// Create object from template for preview
	const ObjectTemplate * const objectTemplate = ObjectTemplateList::fetch(path);
	if (objectTemplate)
	{
		Object * const previewObject = objectTemplate->createObject();
		if (previewObject)
		{
			m_viewer->addObject(*previewObject);
			m_viewer->setViewDirty(true);
			m_viewer->recomputeZoom();
			
			// Update info display with the new object
			updateInfoDisplay();
		}
		objectTemplate->releaseReference();
	}
}

//----------------------------------------------------------------------

void SwgCuiDecoratorSpawn::spawnSelectedObject()
{
	std::string const sharedPath = getSelectedTemplatePath();
	if (sharedPath.empty()) return;
	
	std::string const serverPath = convertToServerTemplate(sharedPath);
	if (serverPath.empty()) return;
	
	// Build spawn command: /spawn <template> 1 0 0
	char buffer[512];
	snprintf(buffer, sizeof(buffer), "%s 1 0 0", serverPath.c_str());
	
	uint32 const hashSpawn = Crc::normalizeAndCalculate("spawn");
	ClientCommandQueue::enqueueCommand(hashSpawn, NetworkId::cms_invalid, Unicode::narrowToWide(buffer));
}

//----------------------------------------------------------------------

std::string SwgCuiDecoratorSpawn::getSelectedTemplatePath() const
{
	if (!m_tree) return std::string();
	
	int const selectedRow = m_tree->GetLastSelectedRow();
	if (selectedRow < 0) return std::string();
	
	UIDataSourceBase const * const selectedData = m_tree->GetDataSourceContainerAtRow(selectedRow);
	if (!selectedData) return std::string();
	
	// Check if it's a leaf node (has TemplatePath property)
	Unicode::String templatePath;
	if (selectedData->GetProperty(UILowerString("TemplatePath"), templatePath))
	{
		return Unicode::wideToNarrow(templatePath);
	}
	
	return std::string();
}

//----------------------------------------------------------------------

std::string SwgCuiDecoratorSpawn::convertToServerTemplate(std::string const & sharedTemplate) const
{
	// Convert shared_<name>.iff to <name>.iff
	// e.g., object/tangible/furniture/shared_chair.iff -> object/tangible/furniture/chair.iff
	
	std::string result = sharedTemplate;
	
	// Find and remove "shared_" from the filename
	size_t const lastSlash = result.rfind('/');
	if (lastSlash != std::string::npos)
	{
		size_t const sharedPos = result.find("shared_", lastSlash);
		if (sharedPos != std::string::npos)
		{
			result.erase(sharedPos, 7); // Remove "shared_" (7 characters)
		}
	}
	
	return result;
}

//----------------------------------------------------------------------

std::string SwgCuiDecoratorSpawn::getDisplayNameForTemplate(std::string const & templatePath) const
{
	// Try to get localized name from the object template's objectName StringId
	ObjectTemplate const * const objectTemplate = ObjectTemplateList::fetch(templatePath);
	if (objectTemplate)
	{
		SharedObjectTemplate const * const sharedTemplate = dynamic_cast<SharedObjectTemplate const *>(objectTemplate);
		if (sharedTemplate)
		{
			StringId const & nameId = sharedTemplate->getObjectName();
			if (!nameId.isInvalid())
			{
				Unicode::String localizedName;
				if (nameId.localize(localizedName) && !localizedName.empty())
				{
					objectTemplate->releaseReference();
					return Unicode::wideToNarrow(localizedName);
				}
			}
		}
		objectTemplate->releaseReference();
	}
	
	// When opened from Decorator Camera, do not render encoded name (path/filename fallback)
	if (m_openedFromDecoratorCamera)
		return std::string();
	
	// Fallback: extract and clean filename
	std::string filename;
	size_t const lastSlash = templatePath.rfind('/');
	if (lastSlash != std::string::npos)
	{
		filename = templatePath.substr(lastSlash + 1);
	}
	else
	{
		filename = templatePath;
	}
	
	// Remove .iff extension
	size_t const dotPos = filename.rfind('.');
	if (dotPos != std::string::npos)
	{
		filename = filename.substr(0, dotPos);
	}
	
	// Remove shared_ prefix
	if (filename.compare(0, 7, "shared_") == 0)
	{
		filename = filename.substr(7);
	}
	
	return filename;
}

//----------------------------------------------------------------------

void SwgCuiDecoratorSpawn::OnTabbedPaneChanged(UIWidget * context)
{
	if (context == m_tabs)
	{
		int const activeTab = m_tabs->GetActiveTab();
		if (m_searchPage)
			m_searchPage->SetVisible(activeTab == 0);
		if (m_settingsPage)
			m_settingsPage->SetVisible(activeTab == 1);
		if (m_infoPage)
			m_infoPage->SetVisible(activeTab == 2);
	}
}

//----------------------------------------------------------------------

void SwgCuiDecoratorSpawn::OnCheckboxSet(UIWidget * context)
{
	if (context == m_checkAutoZoom || context == m_checkHeadshot || context == m_checkRotate || 
		context == m_checkDragYaw || context == m_checkDragPitch || context == m_checkShadows)
	{
		updateViewerSettings();
	}
}

//----------------------------------------------------------------------

void SwgCuiDecoratorSpawn::OnCheckboxUnset(UIWidget * context)
{
	if (context == m_checkAutoZoom || context == m_checkHeadshot || context == m_checkRotate || 
		context == m_checkDragYaw || context == m_checkDragPitch || context == m_checkShadows)
	{
		updateViewerSettings();
	}
}

//----------------------------------------------------------------------

void SwgCuiDecoratorSpawn::OnSliderbarChanged(UIWidget * context)
{
	if (context == m_sliderFOV || context == m_sliderFitDistance || context == m_sliderCameraYaw || 
	    context == m_sliderCameraPitch || context == m_sliderLightYaw || context == m_sliderLightPitch || 
	    context == m_sliderAmbientR || context == m_sliderAmbientG || context == m_sliderAmbientB || 
	    context == m_sliderLightR || context == m_sliderLightG || context == m_sliderLightB)
	{
		updateViewerSettings();
	}
}

//----------------------------------------------------------------------

void SwgCuiDecoratorSpawn::updateViewerSettings()
{
	if (!m_viewer) return;
	
	// Update auto zoom
	if (m_checkAutoZoom)
	{
		m_viewer->setCameraAutoZoom(m_checkAutoZoom->IsChecked());
	}
	
	// Update headshot mode via property
	if (m_checkHeadshot)
	{
		m_viewer->SetProperty(CuiWidget3dObjectListViewer::PropertyName::CameraLookAtCenter, 
			Unicode::narrowToWide(m_checkHeadshot->IsChecked() ? "false" : "true"));
	}
	
	// Update rotation
	if (m_checkRotate)
	{
		m_viewer->setRotateSpeed(m_checkRotate->IsChecked() ? 4.0f : 0.0f);
	}
	
	// Update drag yaw
	if (m_checkDragYaw)
	{
		m_viewer->SetProperty(CuiWidget3dObjectListViewer::PropertyName::DragYawOk, 
			Unicode::narrowToWide(m_checkDragYaw->IsChecked() ? "true" : "false"));
	}
	
	// Update drag pitch
	if (m_checkDragPitch)
	{
		m_viewer->SetProperty(CuiWidget3dObjectListViewer::PropertyName::DragPitchOk, 
			Unicode::narrowToWide(m_checkDragPitch->IsChecked() ? "true" : "false"));
	}
	
	// Update shadows
	if (m_checkShadows)
	{
		m_viewer->SetProperty(CuiWidget3dObjectListViewer::PropertyName::Shadows, 
			Unicode::narrowToWide(m_checkShadows->IsChecked() ? "true" : "false"));
	}
	
	// Update FOV (convert slider value to degrees for property)
	if (m_sliderFOV)
	{
		long fovDegrees = m_sliderFOV->GetValue();
		char fovStr[32];
		snprintf(fovStr, sizeof(fovStr), "%ld", fovDegrees);
		m_viewer->SetProperty(CuiWidget3dObjectListViewer::PropertyName::FieldOfView, 
			Unicode::narrowToWide(fovStr));
	}
	
	// Update fit distance factor (slider value / 100 for 0.5 - 3.0 range)
	if (m_sliderFitDistance)
	{
		long fitDistancePercent = m_sliderFitDistance->GetValue();
		float fitDistanceFactor = static_cast<float>(fitDistancePercent) / 100.0f;
		char fitStr[32];
		snprintf(fitStr, sizeof(fitStr), "%.2f", fitDistanceFactor);
		m_viewer->SetProperty(CuiWidget3dObjectListViewer::PropertyName::FitDistanceFactor, 
			Unicode::narrowToWide(fitStr));
	}
	
	// Update camera yaw (convert degrees to radians)
	if (m_sliderCameraYaw)
	{
		long yawDegrees = m_sliderCameraYaw->GetValue();
		float yawRadians = static_cast<float>(yawDegrees) * 3.14159f / 180.0f;
		char yawStr[32];
		snprintf(yawStr, sizeof(yawStr), "%.4f", yawRadians);
		m_viewer->SetProperty(CuiWidget3dObjectListViewer::PropertyName::CameraYaw, 
			Unicode::narrowToWide(yawStr));
	}
	
	// Update camera pitch (convert degrees to radians)
	if (m_sliderCameraPitch)
	{
		long pitchDegrees = m_sliderCameraPitch->GetValue();
		float pitchRadians = static_cast<float>(pitchDegrees) * 3.14159f / 180.0f;
		char pitchStr[32];
		snprintf(pitchStr, sizeof(pitchStr), "%.4f", pitchRadians);
		m_viewer->SetProperty(CuiWidget3dObjectListViewer::PropertyName::CameraPitch, 
			Unicode::narrowToWide(pitchStr));
	}
	
	// Update light yaw (convert degrees to radians)
	if (m_sliderLightYaw)
	{
		long lightYawDegrees = m_sliderLightYaw->GetValue();
		float lightYawRadians = static_cast<float>(lightYawDegrees) * 3.14159f / 180.0f;
		char yawStr[32];
		snprintf(yawStr, sizeof(yawStr), "%.4f", lightYawRadians);
		m_viewer->SetProperty(CuiWidget3dObjectListViewer::PropertyName::LightYaw, 
			Unicode::narrowToWide(yawStr));
	}
	
	// Update light pitch (convert degrees to radians)
	if (m_sliderLightPitch)
	{
		long lightPitchDegrees = m_sliderLightPitch->GetValue();
		float lightPitchRadians = static_cast<float>(lightPitchDegrees) * 3.14159f / 180.0f;
		char pitchStr[32];
		snprintf(pitchStr, sizeof(pitchStr), "%.4f", lightPitchRadians);
		m_viewer->SetProperty(CuiWidget3dObjectListViewer::PropertyName::LightPitch, 
			Unicode::narrowToWide(pitchStr));
	}
	
	// Update ambient light color
	if (m_sliderAmbientR && m_sliderAmbientG && m_sliderAmbientB)
	{
		long r = m_sliderAmbientR->GetValue();
		long g = m_sliderAmbientG->GetValue();
		long b = m_sliderAmbientB->GetValue();
		
		// Format as #RRGGBB hex color
		char colorStr[16];
		snprintf(colorStr, sizeof(colorStr), "#%02X%02X%02X", 
			static_cast<int>(r), static_cast<int>(g), static_cast<int>(b));
		m_viewer->SetProperty(CuiWidget3dObjectListViewer::PropertyName::LightAmbientColor, 
			Unicode::narrowToWide(colorStr));
	}
	
	// Update light color
	if (m_sliderLightR && m_sliderLightG && m_sliderLightB)
	{
		long r = m_sliderLightR->GetValue();
		long g = m_sliderLightG->GetValue();
		long b = m_sliderLightB->GetValue();
		
		// Format as #RRGGBB hex color
		char colorStr[16];
		snprintf(colorStr, sizeof(colorStr), "#%02X%02X%02X", 
			static_cast<int>(r), static_cast<int>(g), static_cast<int>(b));
		m_viewer->SetProperty(CuiWidget3dObjectListViewer::PropertyName::LightColor, 
			Unicode::narrowToWide(colorStr));
	}
	
	m_viewer->setViewDirty(true);
}

//----------------------------------------------------------------------

void SwgCuiDecoratorSpawn::updateInfoDisplay()
{
	if (!m_infoText) return;
	
	std::string const templatePath = getSelectedTemplatePath();
	if (templatePath.empty())
	{
		m_infoText->SetLocalText(Unicode::narrowToWide("Select an object and click Preview to see information..."));
		return;
	}
	
	std::string infoStr;
	
	// Template Path and Server Path (omit when from Decorator Camera - do not render encoded names)
	if (!m_openedFromDecoratorCamera)
	{
		infoStr += "\\#ffffff== Template Info ==\\#cccccc\n";
		infoStr += "Template Path:\n  " + templatePath + "\n\n";
		std::string const serverPath = convertToServerTemplate(templatePath);
		infoStr += "Server Path:\n  " + serverPath + "\n\n";
	}
	
	// Load the template for more info
	ObjectTemplate const * const objectTemplate = ObjectTemplateList::fetch(templatePath);
	if (objectTemplate)
	{
		SharedObjectTemplate const * const sharedTemplate = dynamic_cast<SharedObjectTemplate const *>(objectTemplate);
		if (sharedTemplate)
		{
			// Object Name StringId
			StringId const & nameId = sharedTemplate->getObjectName();
			if (!nameId.isInvalid())
			{
				if (!m_openedFromDecoratorCamera)
				{
					infoStr += "String File:\n  " + nameId.getTable() + "\n";
					infoStr += "String ID:\n  " + nameId.getText() + "\n";
				}
				Unicode::String localizedName;
				if (nameId.localize(localizedName) && !localizedName.empty())
				{
					infoStr += "Localized Name:\n  " + Unicode::wideToNarrow(localizedName) + "\n";
				}
				infoStr += "\n";
			}
			
			// Detailed Description
			StringId const & descId = sharedTemplate->getDetailedDescription();
			if (!descId.isInvalid())
			{
				infoStr += "Description String:\n  " + descId.getTable() + ":" + descId.getText() + "\n\n";
			}
			
			// Appearance File
			std::string const & appearanceFile = sharedTemplate->getAppearanceFilename();
			if (!appearanceFile.empty())
			{
				infoStr += "\\#ffffff== Appearance ==\\#cccccc\n";
				infoStr += "Appearance File:\n  " + appearanceFile + "\n\n";
			}
			
			// Portal Layout
			std::string const & portalFile = sharedTemplate->getPortalLayoutFilename();
			if (!portalFile.empty())
			{
				infoStr += "Portal Layout:\n  " + portalFile + "\n\n";
			}
			
			// Client Data File
			std::string const & clientDataFile = sharedTemplate->getClientDataFile();
			if (!clientDataFile.empty())
			{
				infoStr += "Client Data:\n  " + clientDataFile + "\n\n";
			}
			
			// Slot Descriptor
			std::string const & slotFile = sharedTemplate->getSlotDescriptorFilename();
			if (!slotFile.empty())
			{
				infoStr += "Slot Descriptor:\n  " + slotFile + "\n\n";
			}
			
			// Arrangement Descriptor
			std::string const & arrangementFile = sharedTemplate->getArrangementDescriptorFilename();
			if (!arrangementFile.empty())
			{
				infoStr += "Arrangement:\n  " + arrangementFile + "\n\n";
			}
			
			// Scale Info
			infoStr += "\\#ffffff== Scale ==\\#cccccc\n";
			char scaleStr[64];
			snprintf(scaleStr, sizeof(scaleStr), "Scale: %.2f (Min: %.2f, Max: %.2f)\n\n",
				sharedTemplate->getScale(),
				sharedTemplate->getScaleMin(),
				sharedTemplate->getScaleMax());
			infoStr += scaleStr;
			
			// Game Object Type
			int const got = static_cast<int>(sharedTemplate->getGameObjectType());
			char gotStr[64];
			snprintf(gotStr, sizeof(gotStr), "Game Object Type: 0x%08X\n\n", got);
			infoStr += gotStr;
			
			// Container Info
			int const containerType = static_cast<int>(sharedTemplate->getContainerType());
			if (containerType > 0)
			{
				infoStr += "\\#ffffff== Container ==\\#cccccc\n";
				char containerStr[128];
				snprintf(containerStr, sizeof(containerStr), "Container Type: %d\nVolume Limit: %d\n\n",
					containerType,
					sharedTemplate->getContainerVolumeLimit());
				infoStr += containerStr;
			}
		}
		objectTemplate->releaseReference();
	}
	
	// Get hardpoint info from the viewer's object
	if (m_viewer)
	{
		Object const * const obj = m_viewer->getLastObject();
		if (obj)
		{
			Appearance const * const appearance = obj->getAppearance();
			if (appearance)
			{
				int const hardpointCount = appearance->getHardpointCount();
				if (hardpointCount > 0)
				{
					infoStr += "\\#ffffff== Hardpoints ==\\#cccccc\n";
					char hpCountStr[32];
					snprintf(hpCountStr, sizeof(hpCountStr), "Count: %d\n", hardpointCount);
					infoStr += hpCountStr;
					
					for (int i = 0; i < hardpointCount && i < 50; ++i)
					{
						Hardpoint const & hp = appearance->getHardpoint(i);
						infoStr += "  " + std::string(hp.getName().getString()) + "\n";
					}
					
					if (hardpointCount > 50)
					{
						infoStr += "  ... and more\n";
					}
					infoStr += "\n";
				}
			}
		}
	}
	
	m_infoText->SetLocalText(Unicode::narrowToWide(infoStr));
}

//----------------------------------------------------------------------

SwgCuiDecoratorSpawn * SwgCuiDecoratorSpawn::createInto(UIPage & parent)
{
	UIPage * const page = NON_NULL(safe_cast<UIPage *>(parent.GetObjectFromPath("/PDA.DecoratorSpawn", TUIPage)));
	return new SwgCuiDecoratorSpawn(*page);
}

//======================================================================
