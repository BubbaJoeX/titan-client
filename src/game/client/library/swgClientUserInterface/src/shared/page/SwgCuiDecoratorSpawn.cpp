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
#include "clientUserInterface/CuiWidget3dObjectListViewer.h"
#include "sharedFoundation/Crc.h"
#include "sharedObject/ObjectTemplate.h"
#include "sharedObject/ObjectTemplateList.h"
#include "UIButton.h"
#include "UIData.h"
#include "UIDataSource.h"
#include "UIDataSourceContainer.h"
#include "UIPage.h"
#include "UIText.h"
#include "UITextbox.h"
#include "UITreeView.h"
#include "UnicodeUtils.h"

#include <algorithm>
#include <map>

//======================================================================

namespace SwgCuiDecoratorSpawnNamespace
{
	std::string const cms_objectPrefix = "object/";
	
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

SwgCuiDecoratorSpawn::SwgCuiDecoratorSpawn(UIPage & page) :
CuiMediator("SwgCuiDecoratorSpawn", page),
UIEventCallback(),
m_tree(NULL),
m_buttonPreview(NULL),
m_buttonSpawn(NULL),
m_buttonClose(NULL),
m_filterTextbox(NULL),
m_selectedText(NULL),
m_viewer(NULL),
m_allTemplates(new TemplateList)
{
	// Get UI elements from CodeData
	getCodeDataObject(TUITreeView, m_tree, "tree");
	getCodeDataObject(TUIButton, m_buttonPreview, "buttonPreview");
	getCodeDataObject(TUIButton, m_buttonSpawn, "buttonSpawn");
	getCodeDataObject(TUIButton, m_buttonClose, "buttonClose");
	getCodeDataObject(TUITextbox, m_filterTextbox, "filterTextbox");
	getCodeDataObject(TUIText, m_selectedText, "selectedText");
	
	UIWidget * viewerWidget = NULL;
	getCodeDataObject(TUIWidget, viewerWidget, "viewer");
	m_viewer = dynamic_cast<CuiWidget3dObjectListViewer *>(viewerWidget);
	
	registerMediatorObject(*m_tree, true);
	registerMediatorObject(*m_buttonPreview, true);
	registerMediatorObject(*m_buttonSpawn, true);
	registerMediatorObject(*m_buttonClose, true);
	registerMediatorObject(*m_filterTextbox, true);
}

//----------------------------------------------------------------------

SwgCuiDecoratorSpawn::~SwgCuiDecoratorSpawn()
{
	delete m_allTemplates;
	m_allTemplates = NULL;
	
	m_tree = NULL;
	m_buttonPreview = NULL;
	m_buttonSpawn = NULL;
	m_buttonClose = NULL;
	m_filterTextbox = NULL;
	m_selectedText = NULL;
	m_viewer = NULL;
}

//----------------------------------------------------------------------

void SwgCuiDecoratorSpawn::performActivate()
{
	CuiMediator::performActivate();
	
	// Populate the tree with object templates
	populateTree();
	
	if (m_filterTextbox)
	{
		m_filterTextbox->SetLocalText(Unicode::emptyString);
	}
	
	if (m_selectedText)
	{
		m_selectedText->SetLocalText(Unicode::narrowToWide("No object selected"));
	}
	
	setPointerInputActive(true);
	setKeyboardInputActive(true);
	setInputToggleActive(false);
}

//----------------------------------------------------------------------

void SwgCuiDecoratorSpawn::performDeactivate()
{
	CuiMediator::performDeactivate();
	
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
}

//----------------------------------------------------------------------

void SwgCuiDecoratorSpawn::OnGenericSelectionChanged(UIWidget * context)
{
	if (context == m_tree)
	{
		std::string const path = getSelectedTemplatePath();
		if (!path.empty() && m_selectedText)
		{
			m_selectedText->SetLocalText(Unicode::narrowToWide(path));
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
	
	// Filter to only object/ .iff files
	for (stdvector<const char *>::fwd::const_iterator it = templateNames.begin(); it != templateNames.end(); ++it)
	{
		std::string const name(*it);
		if (startsWith(name, cms_objectPrefix) && name.find(".iff") != std::string::npos)
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
	
	// Close the window after spawning
	deactivate();
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

SwgCuiDecoratorSpawn * SwgCuiDecoratorSpawn::createInto(UIPage & parent)
{
	UIPage * const page = NON_NULL(safe_cast<UIPage *>(parent.GetObjectFromPath("/PDA.DecoratorSpawn", TUIPage)));
	return new SwgCuiDecoratorSpawn(*page);
}

//======================================================================
