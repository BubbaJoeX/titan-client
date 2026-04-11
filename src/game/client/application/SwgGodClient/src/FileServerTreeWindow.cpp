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
#include <qtimer.h>

#include <sys/stat.h>
#include <process.h>
#include <windows.h>

#include <algorithm>
#include <cstring>

// ======================================================================

static const char * const PLACEHOLDER_TEXT = "...loading...";
static const size_t       BATCH_SIZE       = 200;
static const int          BATCH_INTERVAL   = 10;

// ======================================================================
// Path helpers (used before definition order elsewhere in this TU)
// ======================================================================

static std::string stripSegForPath(std::string s)
{
	bool const dir = !s.empty() && (s[s.size() - 1] == '/' || s[s.size() - 1] == '\\');
	std::string core = dir ? s.substr(0, s.size() - 1) : s;
	static char const * const pats[] = { " (L | R)", " (L)", " (R)" };
	for (int i = 0; i < 3; ++i)
	{
		size_t const len = std::strlen(pats[i]);
		if (core.size() >= len && core.compare(core.size() - len, len, pats[i]) == 0)
		{
			core = core.substr(0, core.size() - len);
			break;
		}
	}
	if (dir)
		return core + "/";
	return core;
}

static std::string normalizeRootOnly(std::string r)
{
	for (size_t i = 0; i < r.size(); ++i)
	{
		if (r[i] == '\\')
			r[i] = '/';
	}
	return r;
}

static std::string buildFullPath(QListViewItem * item, const std::string & rootScope)
{
	if (!item)
		return std::string();

	std::string path = stripSegForPath(std::string(item->text(0).latin1()));

	QListViewItem * p = item->parent();
	while (p)
	{
		std::string seg = stripSegForPath(std::string(p->text(0).latin1()));
		path = seg + path;
		p = p->parent();
	}

	std::string root = normalizeRootOnly(rootScope);
	if (!root.empty() && root[root.size() - 1] != '/')
		root += '/';
	return root + path;
}

// ======================================================================
// Thread helpers (server operations only — send/retrieve/verify)
// ======================================================================

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

// ----------------------------------------------------------------------

namespace
{
std::string normalizeScopeSlashes(std::string s)
{
	for (size_t i = 0; i < s.size(); ++i)
	{
		if (s[i] == '\\')
			s[i] = '/';
	}
	return s;
}

std::string ensureTrailingSlash(std::string s)
{
	if (!s.empty() && s[s.size() - 1] != '/')
		s += '/';
	return s;
}

const char * availabilitySuffix(bool hasL, bool hasR)
{
	if (hasL && hasR)
		return " (L | R)";
	if (hasL)
		return " (L)";
	if (hasR)
		return " (R)";
	return "";
}

std::string buildTreeText(std::string const & sortKey, bool hasL, bool hasR)
{
	std::string const suf = availabilitySuffix(hasL, hasR);
	bool const isDir = (!sortKey.empty() && sortKey[sortKey.size() - 1] == '/');
	if (isDir)
	{
		std::string const core = sortKey.substr(0, sortKey.size() - 1);
		return core + suf + "/";
	}
	return sortKey + suf;
}

typedef std::map<std::string, std::pair<bool, unsigned long> > RemoteMap;

void collectRemoteDirectChildren(RemoteMap & out, std::string const & scopeNorm, std::vector<std::string> const & paths, std::vector<unsigned long> const & sizes)
{
	if (paths.size() != sizes.size())
		return;

	for (size_t i = 0; i < paths.size(); ++i)
	{
		std::string p = paths[i];
		for (size_t j = 0; j < p.size(); ++j)
		{
			if (p[j] == '\\')
				p[j] = '/';
		}
		if (p.size() <= scopeNorm.size() || p.compare(0, scopeNorm.size(), scopeNorm) != 0)
			continue;

		std::string rest = p.substr(scopeNorm.size());
		if (rest.empty())
			continue;

		bool isDir = (!rest.empty() && rest[rest.size() - 1] == '/');
		if (isDir)
			rest = rest.substr(0, rest.size() - 1);

		if (rest.find('/') != std::string::npos)
			continue;

		std::string const key = isDir ? (rest + "/") : rest;
		out[key] = std::make_pair(true, sizes[i]);
	}
}
} // namespace

// ----------------------------------------------------------------------

struct ListThreadResult
{
	unsigned                                              m_token;
	std::string                                           m_scopePath;
	QListViewItem *                                       m_parentItem;
	bool                                                  m_listOk;
	std::vector<FileServerTreeWindow::LocalScanRow>       m_localRows;
	std::vector<std::string>                              m_remotePaths;
	std::vector<unsigned long>                            m_remoteSizes;
};

struct ListThreadArgs
{
	FileServerTreeWindow *                           m_window;
	unsigned                                         m_token;
	std::string                                      m_scopePath;
	QListViewItem *                                  m_parentItem;
	std::vector<FileServerTreeWindow::LocalScanRow>  m_localRows;
};

static unsigned __stdcall listThreadFunc(void * arg)
{
	ListThreadArgs * a = static_cast<ListThreadArgs *>(arg);

	std::vector<std::string> files;
	std::vector<unsigned long> sz;
	std::vector<unsigned long> crc;
	bool ok = FileControlClient::requestDirectoryListing(a->m_scopePath, files, sz, crc);

	ListThreadResult * r = new ListThreadResult;
	r->m_token = a->m_token;
	r->m_scopePath = a->m_scopePath;
	r->m_parentItem = a->m_parentItem;
	r->m_listOk = ok;
	r->m_localRows.swap(a->m_localRows);
	r->m_remotePaths.swap(files);
	r->m_remoteSizes.swap(sz);

	QCustomEvent * ev = new QCustomEvent(FileServerTreeWindow::CE_LIST_DONE);
	ev->setData(r);
	QApplication::postEvent(a->m_window, ev);

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
  m_rootScope("data/"),
  m_expandedDirs(),
  m_busy(false),
  m_batchTimer(0),
  m_activePopulateParent(0),
  m_populateToken(0)
{
	m_pendingBatch.parentItem = 0;
	m_pendingBatch.nextIndex = 0;

	QVBoxLayout * mainLayout = new QVBoxLayout(this, 4, 4);

	QHBoxLayout * scopeLayout = new QHBoxLayout(0, 0, 4);
	QLabel * scopeLabel = new QLabel("Scope:", this);
	m_scopeEdit = new QLineEdit("data/", this);
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

	m_batchTimer = new QTimer(this);

	IGNORE_RETURN(connect(m_refreshButton,  SIGNAL(clicked()), this, SLOT(onRefresh())));
	IGNORE_RETURN(connect(m_sendButton,     SIGNAL(clicked()), this, SLOT(onSend())));
	IGNORE_RETURN(connect(m_retrieveButton, SIGNAL(clicked()), this, SLOT(onRetrieve())));
	IGNORE_RETURN(connect(m_infoButton,     SIGNAL(clicked()), this, SLOT(onInfo())));
	IGNORE_RETURN(connect(m_scopeEdit,      SIGNAL(returnPressed()), this, SLOT(onScopeChanged())));
	IGNORE_RETURN(connect(m_treeView,       SIGNAL(selectionChanged(QListViewItem *)), this, SLOT(onSelectionChanged(QListViewItem *))));
	IGNORE_RETURN(connect(m_treeView,       SIGNAL(expanded(QListViewItem *)), this, SLOT(onItemExpanded(QListViewItem *))));
	IGNORE_RETURN(connect(m_batchTimer,     SIGNAL(timeout()), this, SLOT(onBatchInsertTimer())));
}

// ----------------------------------------------------------------------

FileServerTreeWindow::~FileServerTreeWindow()
{
	if (m_batchTimer)
		m_batchTimer->stop();
}

// ----------------------------------------------------------------------

void FileServerTreeWindow::setRootScope(const std::string & rootPath)
{
	m_rootScope = rootPath;
	if (m_scopeEdit)
		m_scopeEdit->setText(rootPath.c_str());
}

// ======================================================================
// Local filesystem helpers
// ======================================================================

std::string FileServerTreeWindow::resolveLocalDir(const std::string & scopePath) const
{
	std::string candidate = scopePath;
	DWORD attr = GetFileAttributesA(candidate.c_str());
	if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY))
		return candidate;

	candidate = "../../" + scopePath;
	attr = GetFileAttributesA(candidate.c_str());
	if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY))
		return candidate;

	return scopePath;
}

// ----------------------------------------------------------------------

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
// Local filesystem enumeration (shallow, depth-1)
// ======================================================================

void FileServerTreeWindow::populateFromLocal(QListViewItem * parentItem, const std::string & scopePath)
{
	std::string const scope = normalizeScopeSlashes(scopePath);
	std::string const localDir = resolveLocalDir(scope);

	std::vector<LocalScanRow> localRows;

	std::string searchPattern = localDir;
	if (!searchPattern.empty() && searchPattern[searchPattern.size() - 1] != '/' && searchPattern[searchPattern.size() - 1] != '\\')
		searchPattern += '/';
	searchPattern += '*';

	for (size_t i = 0; i < searchPattern.size(); ++i)
	{
		if (searchPattern[i] == '/')
			searchPattern[i] = '\\';
	}

	WIN32_FIND_DATAA fd;
	HANDLE const hFind = FindFirstFileA(searchPattern.c_str(), &fd);
	if (hFind == INVALID_HANDLE_VALUE)
	{
		m_statusLabel->setText(QString("Local folder missing or empty; fetching remote listing: %1").arg(scope.c_str()));
	}
	else
	{
		do
		{
			const char * name = fd.cFileName;
			if (name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0')))
				continue;

			LocalScanRow row;
			row.isDir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
			row.sortKey = name;
			if (row.isDir)
				row.sortKey += '/';
			row.hasLocal = true;
			row.localSize = row.isDir ? 0UL : static_cast<unsigned long>(fd.nFileSizeLow);
			localRows.push_back(row);
		}
		while (FindNextFileA(hFind, &fd));

		FindClose(hFind);
	}

	++m_populateToken;
	m_activePopulateScope = scope;
	m_activePopulateParent = parentItem;

	setBusy(true);
	m_statusLabel->setText(QString("Fetching remote listing for %1...").arg(scope.c_str()));

	ListThreadArgs * const a = new ListThreadArgs;
	a->m_window = this;
	a->m_token = m_populateToken;
	a->m_scopePath = scope;
	a->m_parentItem = parentItem;
	a->m_localRows.swap(localRows);

	_beginthreadex(0, 0, listThreadFunc, a, 0, 0);
}

// ----------------------------------------------------------------------

void FileServerTreeWindow::completeMergedPopulation(unsigned token, QListViewItem * parentItem, const std::string & scopePath, bool listOk,
	std::vector<LocalScanRow> const & localRows, std::vector<std::string> const & remotePaths, std::vector<unsigned long> const & remoteSizes)
{
	if (token != m_populateToken)
		return;

	std::string const scopeNorm = ensureTrailingSlash(normalizeScopeSlashes(scopePath));

	RemoteMap remoteByKey;
	if (listOk)
		collectRemoteDirectChildren(remoteByKey, scopeNorm, remotePaths, remoteSizes);

	typedef std::map<std::string, PendingBatch::Entry> Merged;
	Merged merged;

	for (size_t i = 0; i < localRows.size(); ++i)
	{
		LocalScanRow const & lr = localRows[i];
		RemoteMap::const_iterator const rit = remoteByKey.find(lr.sortKey);
		bool const hasR = (rit != remoteByKey.end());

		PendingBatch::Entry e;
		e.sortKey = lr.sortKey;
		e.isDir = lr.isDir;
		e.treeText = buildTreeText(lr.sortKey, lr.hasLocal, hasR);
		merged[lr.sortKey] = e;

		std::string const fullPath = scopeNorm + lr.sortKey;
		FileEntry fe;
		fe.relativePath = fullPath;
		fe.localAvailable = lr.hasLocal;
		fe.remoteAvailable = hasR;
		fe.localSize = lr.localSize;
		fe.remoteSize = hasR ? rit->second.second : 0UL;
		fe.localCrc = 0;
		fe.remoteCrc = 0;
		m_entryCache[fullPath] = fe;
	}

	for (RemoteMap::const_iterator it = remoteByKey.begin(); it != remoteByKey.end(); ++it)
	{
		if (merged.find(it->first) != merged.end())
			continue;

		PendingBatch::Entry e;
		e.sortKey = it->first;
		e.isDir = (!it->first.empty() && it->first[it->first.size() - 1] == '/');
		e.treeText = buildTreeText(it->first, false, true);
		merged[it->first] = e;

		std::string const fullPath = scopeNorm + it->first;
		FileEntry fe;
		fe.relativePath = fullPath;
		fe.localAvailable = false;
		fe.remoteAvailable = true;
		fe.localSize = 0;
		fe.remoteSize = it->second.second;
		fe.localCrc = 0;
		fe.remoteCrc = 0;
		m_entryCache[fullPath] = fe;
	}

	m_pendingBatch.entries.clear();
	m_pendingBatch.nextIndex = 0;
	m_pendingBatch.parentItem = parentItem;
	m_pendingBatch.scopePath = scopePath;

	for (Merged::const_iterator it = merged.begin(); it != merged.end(); ++it)
		m_pendingBatch.entries.push_back(it->second);

	std::sort(m_pendingBatch.entries.begin(), m_pendingBatch.entries.end());

	if (m_pendingBatch.entries.empty())
	{
		m_statusLabel->setText(QString("Empty: %1").arg(scopePath.c_str()));
		setBusy(false);
		return;
	}

	QString extra = listOk ? QString() : QString(" (remote list failed)");
	m_statusLabel->setText(QString("Loading %1 (%2 entries)%3")
		.arg(scopePath.c_str())
		.arg(static_cast<int>(m_pendingBatch.entries.size()))
		.arg(extra));

	onBatchInsertTimer();
}

// ----------------------------------------------------------------------

void FileServerTreeWindow::onBatchInsertTimer()
{
	if (m_pendingBatch.nextIndex >= m_pendingBatch.entries.size())
	{
		m_batchTimer->stop();
		m_statusLabel->setText(QString("Loaded %1  (%2 entries)")
			.arg(m_pendingBatch.scopePath.c_str())
			.arg(static_cast<int>(m_pendingBatch.entries.size())));
		setBusy(false);
		return;
	}

	size_t end = m_pendingBatch.nextIndex + BATCH_SIZE;
	if (end > m_pendingBatch.entries.size())
		end = m_pendingBatch.entries.size();

	for (size_t i = m_pendingBatch.nextIndex; i < end; ++i)
	{
		const PendingBatch::Entry & entry = m_pendingBatch.entries[i];

		QListViewItem * item = 0;
		if (m_pendingBatch.parentItem)
			item = new QListViewItem(m_pendingBatch.parentItem, entry.treeText.c_str());
		else
			item = new QListViewItem(m_treeView, entry.treeText.c_str());

		if (entry.isDir)
		{
			item->setOpen(false);
			addPlaceholder(item);
		}
	}

	m_pendingBatch.nextIndex = end;

	if (m_pendingBatch.nextIndex < m_pendingBatch.entries.size())
	{
		m_statusLabel->setText(QString("Loading %1 ... (%2/%3)")
			.arg(m_pendingBatch.scopePath.c_str())
			.arg(static_cast<int>(m_pendingBatch.nextIndex))
			.arg(static_cast<int>(m_pendingBatch.entries.size())));
		m_batchTimer->start(BATCH_INTERVAL, true);
	}
	else
	{
		m_batchTimer->stop();
		m_statusLabel->setText(QString("Loaded %1  (%2 entries)")
			.arg(m_pendingBatch.scopePath.c_str())
			.arg(static_cast<int>(m_pendingBatch.entries.size())));
		setBusy(false);
	}
}

// ======================================================================
// Tree population
// ======================================================================

void FileServerTreeWindow::refreshTree()
{
	if (!m_treeView)
		return;

	if (m_batchTimer)
		m_batchTimer->stop();

	++m_populateToken;
	m_entryCache.clear();

	m_treeView->clear();
	m_expandedDirs.clear();
	clearDetailsPane();

	populateFromLocal(0, m_rootScope);
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
		std::string name = stripSegForPath(std::string(sel->text(0).latin1()));
		hasFile = (!name.empty() && name[name.size() - 1] != '/');
	}

	m_sendButton->setEnabled(hasFile && !m_busy);
	m_retrieveButton->setEnabled(hasFile && !m_busy);
	m_infoButton->setEnabled(hasFile && !m_busy);
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
					std::map<std::string, FileEntry>::iterator c = m_entryCache.find(r->path);
					if (c != m_entryCache.end())
					{
						c->second.remoteSize = r->size;
						c->second.remoteCrc = r->crc;
						c->second.remoteAvailable = true;
					}
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

	case CE_LIST_DONE:
		{
			ListThreadResult * r = static_cast<ListThreadResult *>(event->data());
			if (r)
			{
				completeMergedPopulation(r->m_token, r->m_parentItem, r->m_scopePath, r->m_listOk, r->m_localRows, r->m_remotePaths, r->m_remoteSizes);
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
		std::map<std::string, FileEntry>::const_iterator const dit = m_entryCache.find(fullPath);
		if (dit != m_entryCache.end())
		{
			FileEntry const & fe = dit->second;
			m_detailLocalStatus->setText(fe.localAvailable ? "Local: (directory — present)" : "Local: (directory — absent)");
			m_detailRemoteStatus->setText(fe.remoteAvailable ? "Remote: (directory — present)" : "Remote: (directory — absent)");
		}
		else
		{
			m_detailLocalStatus->setText("Local: (directory)");
			m_detailRemoteStatus->setText("Remote: (directory)");
		}
		m_detailLocalSize->setText("Local Size: -");
		m_detailRemoteSize->setText("Remote Size: -");
	}
	else
	{
		std::map<std::string, FileEntry>::const_iterator const cit = m_entryCache.find(fullPath);
		if (cit != m_entryCache.end())
		{
			FileEntry const & fe = cit->second;
			m_detailLocalStatus->setText(fe.localAvailable ? "Local: Available" : "Local: Missing");
			if (fe.localAvailable && fe.localSize > 0)
			{
				char buf[64];
				snprintf(buf, sizeof(buf), "Local Size: %lu bytes", fe.localSize);
				m_detailLocalSize->setText(buf);
			}
			else if (fe.localAvailable)
			{
				struct _stat localStat;
				if (_stat(fullPath.c_str(), &localStat) == 0)
				{
					char buf[64];
					snprintf(buf, sizeof(buf), "Local Size: %lu bytes", static_cast<unsigned long>(localStat.st_size));
					m_detailLocalSize->setText(buf);
				}
				else
					m_detailLocalSize->setText("Local Size: -");
			}
			else
				m_detailLocalSize->setText("Local Size: -");

			if (fe.remoteAvailable)
			{
				m_detailRemoteStatus->setText("Remote: Available");
				char buf[64];
				snprintf(buf, sizeof(buf), "Remote Size: %lu bytes", fe.remoteSize);
				m_detailRemoteSize->setText(buf);
			}
			else
			{
				m_detailRemoteStatus->setText("Remote: not in listing");
				m_detailRemoteSize->setText("Remote Size: -");
			}
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
		}

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

	std::string name = stripSegForPath(std::string(item->text(0).latin1()));
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

	populateFromLocal(item, fullDir);
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
