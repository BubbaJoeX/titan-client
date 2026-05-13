// ======================================================================
//
// ClientTemplateListView.cpp
// copyright(c) 2001 Sony Online Entertainment
//
// ======================================================================

#include "SwgGodClient/FirstSwgGodClient.h"
#include "ClientTemplateListView.h"
#include "ClientTemplateListView.moc"

#include "AbstractFilesystemTree.h"
#include "ActionHack.h"
#include "ActionsObjectTemplate.h"
#include "ConfigGodClient.h"
#include "FileSystemTree.h"
#include "IconLoader.h"
#include "ObjectTemplateData.h"

#include <qmessagebox.h>
#include <qdragobject.h>
#include <qpopupmenu.h>

//----------------------------------------------------------------------

ClientTemplateListView::ClientTemplateListView(QWidget* theParent, const char* theName)
: QListView(theParent, theName),
  m_currentSearchText()
{
	IGNORE_RETURN(QListView::addColumn("Name"));
	QListView::setResizeMode(QListView::NoColumn);
	QListView::setRootIsDecorated(true);
	QListView::setColumnWidthMode(0, QListView::Maximum);

	IGNORE_RETURN(connect(this, SIGNAL(contextMenuRequested(QListViewItem*, const QPoint&, int)), this, SLOT(onContextMenuRequested(QListViewItem*, const QPoint&, int))));
	IGNORE_RETURN(connect(this, SIGNAL(selectionChanged()), this, SLOT(onSelectionChanged())));

	const ActionsObjectTemplate* aot = &ActionsObjectTemplate::getInstance();

	//if someone triggers the "refresh object templates" action, refresh the list view
	IGNORE_RETURN(connect(aot->m_clientRefresh, SIGNAL(activated()), SLOT(onRefreshList())));
}

//----------------------------------------------------------------------

/**
 * If the user drags an object template item off this window, generate a message that lets other windows process this event
 *
 */
QDragObject* ClientTemplateListView::dragObject()
{
	return new QTextDrag(ObjectTemplateData::DragMessages::CLIENT_TEMPLATE_DRAGGED, this, "menu");
}

// ======================================================================

/**
 * Fill (a part of) the template list view with the items from perforce, and use the given pixmaps to differentiate this part of the tree from the others
 *
 */
 void ClientTemplateListView::populateTemplateTree(const char* name, const QPixmap* pix, const QPixmap* folderPix, QListView* parent, const AbstractFilesystemTree* afst) const
{
	if(afst)
	{
		const AbstractFilesystemTree::Node* const node = afst->getRootNode();
		
		if(node)
		{
			QListViewItem* const addedItem = name ? new QListViewItem(parent, name) : 0;
			if(addedItem)
			{
				if(folderPix)
					addedItem->setPixmap(0,*folderPix);

				addedItem->setSelectable(false);
				AbstractFilesystemTree::populateListItem(addedItem, node, true, folderPix, pix, true, true);
			}
			else
			{
				AbstractFilesystemTree::populateListItem(parent, node, true, folderPix, pix, true, true);
			}
		}
	}
}

//----------------------------------------------------------------------

/**
 * Take the full path of an item, and build a tree item out of it (one directory per branch)
 *
 */
const std::string ClientTemplateListView::constructRelativePath(const QListViewItem* item, bool& isLeaf, bool& isNew, bool& isEdit) const
{
	std::string result;

	isLeaf = item->childCount() == 0;

	const QListViewItem* p = item;
	
	while(p)
	{
		const QString text = p->text(0);
		
		if(text == "[NEW]")
		{
			isNew = true;
			break;
		}
		else if(text == "[EDIT]")
		{
			isEdit = true;
			break;
		}
		
		if(!result.empty())
			result = std::string("/") + result;
		
		result = std::string(p->text(0)) + result;
		
		p = p->parent();
	}
	
	return result;
}

//----------------------------------------------------------------------

void ClientTemplateListView::onSelectionChanged() const
{
	QListViewItem* item = selectedItem();
	
	bool isLeaf = false;
	bool isNew  = false;
	bool isEdit = false;
	const std::string path = item ? constructRelativePath(item, isLeaf, isNew, isEdit) : std::string("");
	ActionsObjectTemplate::getInstance().onClientObjectTemplatePathSelectionChanged(path, isLeaf, isNew, isEdit);
}

//----------------------------------------------------------------------

/**
 * Refresh the list view.  Ask perforce for all regular, edited, and new templates
 * NOTE: this doesn't work as intended, because it uses the COMPILED directory tree as the hierarchy to show,
 * but the source files do *not* exist in a corresponding tree and can't be found using the same hierarchy.
 */
void ClientTemplateListView::onRefreshList()
{
	clear();
	
	setCursor(static_cast<int>(Qt::WaitCursor));

		FilesystemTree* fst = new FilesystemTree();

		std::string path = ConfigGodClient::getData().localClientDataPath;
		path += "/object";

		fst->setRootPath(path);
		fst->setFilter("*.iff");
		fst->populateTree();

		{
			const QPixmap pix       = IL_PIXMAP(hi16_mime_document);
			const QPixmap folderPix = IL_PIXMAP(hi16_filesys_folder_red);
			populateTemplateTree(0,&pix,&folderPix, this, fst);
		}

		delete fst;


	unsetCursor();
}

//----------------------------------------------------------------------

/**
 * Show a right click menu, mimicing the "Menu-Script" menu
 */
void ClientTemplateListView::onContextMenuRequested(QListViewItem* item, const QPoint& p, int)
{
	if(item == 0)
		return;

	//create a popup, its name is unimportant
	QPopupMenu* const m_pop = new QPopupMenu(this, "menu");

	ActionsObjectTemplate* aot =&ActionsObjectTemplate::getInstance();

	IGNORE_RETURN(aot->m_clientAddToFavorites->addTo(m_pop));
	IGNORE_RETURN(m_pop->insertSeparator());

	IGNORE_RETURN(aot->m_clientRefresh->addTo(m_pop));

	IGNORE_RETURN(m_pop->insertSeparator());

	IGNORE_RETURN(aot->m_clientCreate->addTo (m_pop));
	IGNORE_RETURN(aot->m_clientEdit->addTo   (m_pop));
	IGNORE_RETURN(aot->m_clientView->addTo   (m_pop));
	IGNORE_RETURN(aot->m_clientRevert->addTo (m_pop));
	IGNORE_RETURN(aot->m_clientSubmit->addTo (m_pop));

	IGNORE_RETURN(m_pop->insertSeparator());

	IGNORE_RETURN(aot->m_clientCompile->addTo(m_pop));
	IGNORE_RETURN(aot->m_clientReplace->addTo(m_pop));

	m_pop->popup(p);
} //lint !e818 item "could" be const, but Qt allows us to change it

//----------------------------------------------------------------------

/**
 * Handle live search text changes - filter the tree while preserving hierarchy.
 * When a child matches, its parent folders remain visible to maintain context.
 */
void ClientTemplateListView::onSearchTextChanged(const QString& text)
{
	m_currentSearchText = text.lower();
	
	if(m_currentSearchText.isEmpty())
	{
		showAllItems(firstChild());
	}
	else
	{
		QListViewItem* item = firstChild();
		while(item)
		{
			filterTreeItem(item, m_currentSearchText);
			item = item->nextSibling();
		}
	}
	
	triggerUpdate();
}

//----------------------------------------------------------------------

/**
 * Clear the search filter and show all items.
 */
void ClientTemplateListView::onClearSearch()
{
	m_currentSearchText = "";
	showAllItems(firstChild());
	triggerUpdate();
}

//----------------------------------------------------------------------

/**
 * Recursively filter a tree item and its children based on the filter text.
 * Returns true if this item or any of its descendants match the filter.
 * Preserves tree hierarchy - parent folders remain visible when children match.
 */
bool ClientTemplateListView::filterTreeItem(QListViewItem* item, const QString& filterText) const
{
	if(item == 0)
		return false;
	
	bool hasMatchingDescendant = false;
	
	QListViewItem* child = item->firstChild();
	while(child)
	{
		if(filterTreeItem(child, filterText))
			hasMatchingDescendant = true;
		child = child->nextSibling();
	}
	
	const QString itemText = item->text(0).lower();
	const bool thisMatches = itemText.contains(filterText);
	
	const bool shouldShow = thisMatches || hasMatchingDescendant;
	item->setVisible(shouldShow);
	
	if(hasMatchingDescendant)
		item->setOpen(true);
	
	return shouldShow;
}

//----------------------------------------------------------------------

/**
 * Recursively show all items in the tree - used when clearing the filter.
 */
void ClientTemplateListView::showAllItems(QListViewItem* item) const
{
	while(item)
	{
		item->setVisible(true);
		
		if(item->firstChild())
			showAllItems(item->firstChild());
		
		item = item->nextSibling();
	}
}

// ======================================================================
