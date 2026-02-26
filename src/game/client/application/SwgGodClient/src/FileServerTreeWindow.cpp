// ======================================================================
//
// FileServerTreeWindow.cpp
// copyright 2024 Sony Online Entertainment
//
// ======================================================================

#include "SwgGodClient/FirstSwgGodClient.h"
#include "FileServerTreeWindow.h"
#include "FileServerTreeWindow.moc"

#include "ActionsFileControl.h"
#include "MainFrame.h"

#include "FileControlClient.h"

#include <qapplication.h>
#include <qheader.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qlineedit.h>
#include <qlistview.h>
#include <qmessagebox.h>
#include <qpushbutton.h>
#include <qsplitter.h>

#include <sys/stat.h>
#include <process.h>

// ======================================================================

static const char * const PLACEHOLDER_TEXT = "...loading...";

// ======================================================================
// Thread helpers
// ======================================================================

struct ListingThreadArgs
{
	FileServerTreeWindow *                    window;
	FileServerTreeWindow::ListingResult *     result;
};

static unsigned __stdcall listingThreadFunc(void * arg)
{
	ListingThreadArgs * a = static_cast<ListingThreadArgs *>(arg);
	FileServerTreeWindow::ListingResult * r = a->result;

	std::vector<unsigned long> crcs;
	r->ok = FileControlClient::requestDirectoryListing(r->dirPath, r->files, r->sizes, crcs);

	QCustomEvent * ev = new QCustomEvent(FileServerTreeWindow::CE_LISTING_DONE);
	ev->setData(r);
	QApplication::postEvent(a->window, ev);

	delete a;
	return 0;
}

// ----------------------------------------------------------------------

struct SendThreadArgs
{
	FileServerTreeWindow *               window;
	FileServerTreeWindow::SendResult *   result;
};

static unsigned __stdcall sendThreadFunc(void * arg)
{
	SendThreadArgs * a = static_cast<SendThreadArgs *>(arg);
	FileServerTreeWindow::SendResult * r = a->result;

	r->ok = FileControlClient::requestSendAsset(r->path);

	QCustomEvent * ev = new QCustomEvent(FileServerTreeWindow::CE_SEND_DONE);
	ev->setData(r);
	QApplication::postEvent(a->window, ev);

	delete a;
	return 0;
}

// ----------------------------------------------------------------------

struct RetrieveThreadArgs
{
	FileServerTreeWindow *                  window;
	FileServerTreeWindow::RetrieveResult *  result;
};

static unsigned __stdcall retrieveThreadFunc(void * arg)
{
	RetrieveThreadArgs * a = static_cast<RetrieveThreadArgs *>(arg);
	FileServerTreeWindow::RetrieveResult * r = a->result;

	r->ok = FileControlClient::requestRetrieveAsset(r->path, r->data);

	QCustomEvent * ev = new QCustomEvent(FileServerTreeWindow::CE_RETRIEVE_DONE);
	ev->setData(r);
	QApplication::postEvent(a->window, ev);

	delete a;
	return 0;
}

// ----------------------------------------------------------------------

struct VerifyThreadArgs
{
	FileServerTreeWindow *                window;
	FileServerTreeWindow::VerifyResult *  result;
};

static unsigned __stdcall verifyThreadFunc(void * arg)
{
	VerifyThreadArgs * a = static_cast<VerifyThreadArgs *>(arg);
	FileServerTreeWindow::VerifyResult * r = a->result;

	r->ok = FileControlClient::requestVerifyAsset(r->path, r->size, r->crc);

	QCustomEvent * ev = new QCustomEvent(FileServerTreeWindow::CE_VERIFY_DONE);
	ev->setData(r);
	QApplication::postEvent(a->window, ev);

	delete a;
	return 0;
}

// ======================================================================

FileServerTreeWindow::FileServerTreeWindow(QWidget * parent, const char * name)
: QWidget(parent, name),
  m_splitter(0),
  m_treeView(0),
  m_detailsWidget(0),
  m_detailPath(0),
  m_detailLocalStatus(0),
  m_detailRemoteStatus(0),
  m_detailLocalSize(0),
  m_detailRemoteSize(0),
  m_sendButton(0),
  m_retrieveButton(0),
  m_infoButton(0),
  m_refreshButton(0),
  m_scopeEdit(0),
  m_statusLabel(0),
  m_rootScope("dsrc/"),
  m_expandedDirs(),
  m_busy(false)
{
	QVBoxLayout * mainLayout = new QVBoxLayout(this, 4, 4);

	QHBoxLayout * scopeLayout = new QHBoxLayout(0, 0, 4);
	QLabel * scopeLabel = new QLabel("Scope:", this);
	m_scopeEdit = new QLineEdit("dsrc/", this);
	m_refreshButton = new QPushButton("Refresh", this);
	scopeLayout->addWidget(scopeLabel);
	scopeLayout->addWidget(m_scopeEdit, 1);
	scopeLayout->addWidget(m_refreshButton);
	mainLayout->addLayout(scopeLayout);

	m_splitter = new QSplitter(Qt::Vertical, this);
	mainLayout->addWidget(m_splitter, 1);

	m_treeView = new QListView(m_splitter, "fileServerTree");
	m_treeView->addColumn("Name");
	m_treeView->setRootIsDecorated(true);
	m_treeView->setAllColumnsShowFocus(true);
	m_treeView->setSelectionMode(QListView::Single);
	m_treeView->header()->setClickEnabled(true);
	m_treeView->setSorting(0, true);

	m_detailsWidget = new QWidget(m_splitter);
	QVBoxLayout * detLayout = new QVBoxLayout(m_detailsWidget, 6, 2);

	m_detailPath         = new QLabel("Path: -", m_detailsWidget);
	m_detailLocalStatus  = new QLabel("Local: -", m_detailsWidget);
	m_detailRemoteStatus = new QLabel("Remote: -", m_detailsWidget);
	m_detailLocalSize    = new QLabel("Local Size: -", m_detailsWidget);
	m_detailRemoteSize   = new QLabel("Remote Size: -", m_detailsWidget);

	detLayout->addWidget(m_detailPath);
	detLayout->addWidget(m_detailLocalStatus);
	detLayout->addWidget(m_detailRemoteStatus);
	detLayout->addWidget(m_detailLocalSize);
	detLayout->addWidget(m_detailRemoteSize);
	detLayout->addStretch(1);

	m_statusLabel = new QLabel("Select a folder to expand, or click Refresh.", this);
	mainLayout->addWidget(m_statusLabel);

	QHBoxLayout * buttonLayout = new QHBoxLayout(0, 0, 4);
	m_sendButton     = new QPushButton("Send", this);
	m_retrieveButton = new QPushButton("Retrieve", this);
	m_infoButton     = new QPushButton("Info", this);
	buttonLayout->addStretch(1);
	buttonLayout->addWidget(m_sendButton);
	buttonLayout->addWidget(m_retrieveButton);
	buttonLayout->addWidget(m_infoButton);
	mainLayout->addLayout(buttonLayout);

	m_sendButton->setEnabled(false);
	m_retrieveButton->setEnabled(false);
	m_infoButton->setEnabled(false);

	IGNORE_RETURN(connect(m_refreshButton,  SIGNAL(clicked()), this, SLOT(onRefresh())));
	IGNORE_RETURN(connect(m_sendButton,     SIGNAL(clicked()), this, SLOT(onSend())));
	IGNORE_RETURN(connect(m_retrieveButton, SIGNAL(clicked()), this, SLOT(onRetrieve())));
	IGNORE_RETURN(connect(m_infoButton,     SIGNAL(clicked()), this, SLOT(onInfo())));
	IGNORE_RETURN(connect(m_scopeEdit,      SIGNAL(returnPressed()), this, SLOT(onScopeChanged())));
	IGNORE_RETURN(connect(m_treeView,       SIGNAL(selectionChanged(QListViewItem *)), this, SLOT(onSelectionChanged(QListViewItem *))));
	IGNORE_RETURN(connect(m_treeView,       SIGNAL(expanded(QListViewItem *)), this, SLOT(onItemExpanded(QListViewItem *))));
}

// ----------------------------------------------------------------------

FileServerTreeWindow::~FileServerTreeWindow()
{
}

// ----------------------------------------------------------------------

void FileServerTreeWindow::setRootScope(const std::string & rootPath)
{
	m_rootScope = rootPath;
	if (m_scopeEdit)
		m_scopeEdit->setText(rootPath.c_str());
}

// ======================================================================
// Lazy-loading helpers
// ======================================================================

bool FileServerTreeWindow::isDirectory(const std::string & name) const
{
	if (name.empty())
		return false;

	char last = name[name.size() - 1];
	return (last == '/' || last == '\\');
}

// ----------------------------------------------------------------------

void FileServerTreeWindow::addPlaceholder(QListViewItem * parentItem)
{
	QListViewItem * ph = new QListViewItem(parentItem, PLACEHOLDER_TEXT);
	UNREF(ph);
}

// ----------------------------------------------------------------------

bool FileServerTreeWindow::isPlaceholder(QListViewItem * item) const
{
	return (item && item->text(0) == PLACEHOLDER_TEXT);
}

// ======================================================================
// Busy state
// ======================================================================

void FileServerTreeWindow::setBusy(bool busy)
{
	m_busy = busy;
	m_refreshButton->setEnabled(!busy);
	if (busy)
	{
		m_sendButton->setEnabled(false);
		m_retrieveButton->setEnabled(false);
		m_infoButton->setEnabled(false);
	}
	else
	{
		updateButtonStates();
	}
}

// ======================================================================
// Tree population (async)
// ======================================================================

void FileServerTreeWindow::refreshTree()
{
	if (!m_treeView)
		return;

	m_treeView->clear();
	m_expandedDirs.clear();
	clearDetailsPane();

	setBusy(true);
	m_statusLabel->setText("Connecting...");

	ListingResult * result = new ListingResult;
	result->dirPath    = m_rootScope;
	result->ok         = false;
	result->parentItem = 0;
	result->isTopLevel = true;

	ListingThreadArgs * args = new ListingThreadArgs;
	args->window = this;
	args->result = result;

	_beginthreadex(0, 0, listingThreadFunc, args, 0, 0);
}

// ----------------------------------------------------------------------

void FileServerTreeWindow::applyListingResult(ListingResult * result)
{
	if (!result)
		return;

	if (!result->ok || result->files.empty())
	{
		if (result->isTopLevel)
			m_statusLabel->setText("No files returned (is server running?).");
		else
			m_statusLabel->setText(QString("Empty: %1").arg(result->dirPath.c_str()));
		setBusy(false);
		delete result;
		return;
	}

	const std::string & prefix = result->dirPath;
	std::map<std::string, unsigned long> childMap;

	for (size_t i = 0; i < result->files.size(); ++i)
	{
		const std::string & fullPath = result->files[i];

		std::string relative = fullPath;
		if (relative.find(prefix) == 0)
			relative = relative.substr(prefix.size());

		std::string::size_type slashPos = relative.find('/');
		if (slashPos != std::string::npos)
		{
			std::string dirName = relative.substr(0, slashPos);
			childMap[dirName + "/"] = 0;
		}
		else if (!relative.empty())
		{
			unsigned long sz = (i < result->sizes.size()) ? result->sizes[i] : 0;
			childMap[relative] = sz;
		}
	}

	for (std::map<std::string, unsigned long>::const_iterator it = childMap.begin(); it != childMap.end(); ++it)
	{
		const std::string & childName = it->first;

		QListViewItem * item = 0;
		if (result->parentItem)
			item = new QListViewItem(result->parentItem, childName.c_str());
		else
			item = new QListViewItem(m_treeView, childName.c_str());

		bool isDir = (childName.size() > 0 && childName[childName.size() - 1] == '/');
		if (isDir)
		{
			item->setOpen(false);
			addPlaceholder(item);
		}
	}

	m_statusLabel->setText(QString("Loaded %1  (%2 entries)")
		.arg(prefix.c_str())
		.arg(static_cast<int>(childMap.size())));

	setBusy(false);
	delete result;
}

// ======================================================================
// Details pane
// ======================================================================

void FileServerTreeWindow::clearDetailsPane()
{
	m_detailPath->setText("Path: -");
	m_detailLocalStatus->setText("Local: -");
	m_detailRemoteStatus->setText("Remote: -");
	m_detailLocalSize->setText("Local Size: -");
	m_detailRemoteSize->setText("Remote Size: -");
}

// ======================================================================
// Button states
// ======================================================================

void FileServerTreeWindow::updateButtonStates()
{
	QListViewItem * sel = m_treeView ? m_treeView->selectedItem() : 0;
	bool hasFile = false;

	if (sel && !isPlaceholder(sel))
	{
		std::string name = sel->text(0).latin1();
		hasFile = (!name.empty() && name[name.size() - 1] != '/');
	}

	m_sendButton->setEnabled(hasFile && !m_busy);
	m_retrieveButton->setEnabled(hasFile && !m_busy);
	m_infoButton->setEnabled(hasFile && !m_busy);
}

// ======================================================================
// Build full path from tree item
// ======================================================================

static std::string buildFullPath(QListViewItem * item, const std::string & rootScope)
{
	if (!item)
		return std::string();

	std::string path = item->text(0).latin1();

	QListViewItem * p = item->parent();
	while (p)
	{
		std::string seg = p->text(0).latin1();
		path = seg + path;
		p = p->parent();
	}

	return rootScope + path;
}

// ======================================================================
// Custom event handler (receives results from worker threads)
// ======================================================================

void FileServerTreeWindow::customEvent(QCustomEvent * event)
{
	if (!event)
		return;

	switch (event->type())
	{
	case CE_LISTING_DONE:
		{
			ListingResult * r = static_cast<ListingResult *>(event->data());
			applyListingResult(r);
		}
		break;

	case CE_SEND_DONE:
		{
			SendResult * r = static_cast<SendResult *>(event->data());
			if (r)
			{
				if (r->ok)
				{
					MainFrame::getInstance().textToConsole(("FileControl: Send -> " + r->path + " OK").c_str());
					m_statusLabel->setText(("Sent: " + r->path).c_str());
				}
				else
				{
					MainFrame::getInstance().textToConsole(("FileControl: Send -> " + r->path + " FAILED").c_str());
					m_statusLabel->setText(("Send failed: " + r->path).c_str());
				}
				setBusy(false);
				delete r;
			}
		}
		break;

	case CE_RETRIEVE_DONE:
		{
			RetrieveResult * r = static_cast<RetrieveResult *>(event->data());
			if (r)
			{
				if (r->ok)
				{
					char buf[256];
					snprintf(buf, sizeof(buf), "FileControl: Retrieved %s (%d bytes)", r->path.c_str(), static_cast<int>(r->data.size()));
					MainFrame::getInstance().textToConsole(buf);
					m_statusLabel->setText(("Retrieved: " + r->path).c_str());
				}
				else
				{
					MainFrame::getInstance().textToConsole(("FileControl: Retrieve -> " + r->path + " FAILED").c_str());
					m_statusLabel->setText(("Retrieve failed: " + r->path).c_str());
				}
				setBusy(false);
				delete r;
			}
		}
		break;

	case CE_VERIFY_DONE:
		{
			VerifyResult * r = static_cast<VerifyResult *>(event->data());
			if (r)
			{
				if (r->ok && (r->size > 0 || r->crc > 0))
				{
					m_detailRemoteStatus->setText("Remote: Available");
					char buf[64];
					snprintf(buf, sizeof(buf), "Remote Size: %lu bytes", r->size);
					m_detailRemoteSize->setText(buf);
				}
				else
				{
					m_detailRemoteStatus->setText("Remote: Available (on server)");
					m_detailRemoteSize->setText("Remote Size: -");
				}
				delete r;
			}
		}
		break;

	default:
		QWidget::customEvent(event);
		break;
	}
}

// ======================================================================
// Slots
// ======================================================================

void FileServerTreeWindow::onRefresh()
{
	if (m_busy)
		return;
	onScopeChanged();
	refreshTree();
}

// ----------------------------------------------------------------------

void FileServerTreeWindow::onScopeChanged()
{
	if (m_scopeEdit)
		m_rootScope = m_scopeEdit->text().latin1();
}

// ----------------------------------------------------------------------

void FileServerTreeWindow::onSelectionChanged(QListViewItem * item)
{
	updateButtonStates();

	if (!item || isPlaceholder(item))
	{
		clearDetailsPane();
		return;
	}

	std::string fullPath = buildFullPath(item, m_rootScope);

	m_detailPath->setText(QString("Path: %1").arg(fullPath.c_str()));

	bool isDir = (!fullPath.empty() && fullPath[fullPath.size() - 1] == '/');
	if (isDir)
	{
		m_detailLocalStatus->setText("Local: (directory)");
		m_detailRemoteStatus->setText("Remote: (directory)");
		m_detailLocalSize->setText("Local Size: -");
		m_detailRemoteSize->setText("Remote Size: -");
	}
	else
	{
#if defined(PLATFORM_WIN32)
		struct _stat localStat;
		bool localExists = (_stat(fullPath.c_str(), &localStat) == 0);
#else
		struct stat localStat;
		bool localExists = (stat(fullPath.c_str(), &localStat) == 0);
#endif
		if (localExists)
		{
			m_detailLocalStatus->setText("Local: Available");
			char buf[64];
			snprintf(buf, sizeof(buf), "Local Size: %lu bytes", static_cast<unsigned long>(localStat.st_size));
			m_detailLocalSize->setText(buf);
		}
		else
		{
			m_detailLocalStatus->setText("Local: Missing");
			m_detailLocalSize->setText("Local Size: -");
		}

		m_detailRemoteStatus->setText("Remote: checking...");
		m_detailRemoteSize->setText("Remote Size: ...");

		VerifyResult * vr = new VerifyResult;
		vr->path = fullPath;
		vr->ok   = false;
		vr->size = 0;
		vr->crc  = 0;

		VerifyThreadArgs * va = new VerifyThreadArgs;
		va->window = this;
		va->result = vr;

		_beginthreadex(0, 0, verifyThreadFunc, va, 0, 0);
	}

	emit selectedPathChanged(fullPath);
	ActionsFileControl::getInstance().onSelectedPathChanged(fullPath);
}

// ----------------------------------------------------------------------

void FileServerTreeWindow::onItemExpanded(QListViewItem * item)
{
	if (!item || m_busy)
		return;

	std::string name = item->text(0).latin1();
	bool isDir = (!name.empty() && name[name.size() - 1] == '/');
	if (!isDir)
		return;

	std::string fullDir = buildFullPath(item, m_rootScope);

	if (m_expandedDirs.find(fullDir) != m_expandedDirs.end())
		return;

	m_expandedDirs[fullDir] = true;

	QListViewItem * child = item->firstChild();
	while (child)
	{
		QListViewItem * next = child->nextSibling();
		if (isPlaceholder(child))
			delete child;
		child = next;
	}

	setBusy(true);
	m_statusLabel->setText(QString("Loading %1 ...").arg(fullDir.c_str()));

	ListingResult * result = new ListingResult;
	result->dirPath    = fullDir;
	result->ok         = false;
	result->parentItem = item;
	result->isTopLevel = false;

	ListingThreadArgs * args = new ListingThreadArgs;
	args->window = this;
	args->result = result;

	_beginthreadex(0, 0, listingThreadFunc, args, 0, 0);
}

// ----------------------------------------------------------------------

void FileServerTreeWindow::onSend()
{
	if (m_busy)
		return;

	QListViewItem * item = m_treeView ? m_treeView->selectedItem() : 0;
	if (!item || isPlaceholder(item))
		return;

	std::string path = buildFullPath(item, m_rootScope);

	setBusy(true);
	m_statusLabel->setText(("Sending " + path + " ...").c_str());

	SendResult * sr = new SendResult;
	sr->path = path;
	sr->ok   = false;

	SendThreadArgs * sa = new SendThreadArgs;
	sa->window = this;
	sa->result = sr;

	_beginthreadex(0, 0, sendThreadFunc, sa, 0, 0);
}

// ----------------------------------------------------------------------

void FileServerTreeWindow::onRetrieve()
{
	if (m_busy)
		return;

	QListViewItem * item = m_treeView ? m_treeView->selectedItem() : 0;
	if (!item || isPlaceholder(item))
		return;

	std::string path = buildFullPath(item, m_rootScope);

	setBusy(true);
	m_statusLabel->setText(("Retrieving " + path + " ...").c_str());

	RetrieveResult * rr = new RetrieveResult;
	rr->path = path;
	rr->ok   = false;

	RetrieveThreadArgs * ra = new RetrieveThreadArgs;
	ra->window = this;
	ra->result = rr;

	_beginthreadex(0, 0, retrieveThreadFunc, ra, 0, 0);
}

// ----------------------------------------------------------------------

void FileServerTreeWindow::onInfo()
{
	if (m_busy)
		return;

	QListViewItem * item = m_treeView ? m_treeView->selectedItem() : 0;
	if (!item || isPlaceholder(item))
		return;

	std::string path = buildFullPath(item, m_rootScope);

	unsigned long size = 0;
	unsigned long crc = 0;
	bool verified = FileControlClient::requestVerifyAsset(path, size, crc);

	QString info = QString("Path: %1").arg(path.c_str());

	if (verified)
	{
		info += QString("\n\nServer CRC: 0x%1\nServer Size: %2 bytes")
			.arg(crc, 8, 16, QChar('0'))
			.arg(size);
	}
	else
	{
		info += "\n\n(Could not verify with server)";
	}

	QMessageBox::information(&MainFrame::getInstance(), "Asset Info", info);
}

// ======================================================================
