// ======================================================================
//
// FileServerTreeWindow.h
// copyright 2024 Sony Online Entertainment
//
// Lazy-loading folder tree: each level merges local directory listing with
// FileControl directory listing. Names show (L), (R), or (L | R). Subdirectories
// load on expand; send/retrieve/verify behave as before.
//
// ======================================================================

#ifndef INCLUDED_FileServerTreeWindow_H
#define INCLUDED_FileServerTreeWindow_H

// ======================================================================

#include <qwidget.h>
#include <qevent.h>
#include <string>
#include <vector>
#include <map>

class QListView;
class QListViewItem;
class QPushButton;
class QLabel;
class QLineEdit;
class QSplitter;
class QTimer;

// ======================================================================

class FileServerTreeWindow : public QWidget
{
	Q_OBJECT; //lint !e1516 !e19 !e1924 !e1762

public:

	explicit FileServerTreeWindow(QWidget * parent = 0, const char * name = 0);
	virtual ~FileServerTreeWindow();

	void refreshTree();
	void setRootScope(const std::string & rootPath);

	struct FileEntry
	{
		std::string relativePath;
		bool        localAvailable;
		bool        remoteAvailable;
		unsigned long localSize;
		unsigned long remoteSize;
		unsigned long localCrc;
		unsigned long remoteCrc;
	};

	enum CustomEventType
	{
		CE_SEND_DONE     = 10002,
		CE_RETRIEVE_DONE = 10003,
		CE_VERIFY_DONE   = 10004,
		CE_LIST_DONE     = 10005
	};

	struct SendResult
	{
		std::string path;
		bool        ok;
	};

	struct RetrieveResult
	{
		std::string                path;
		bool                       ok;
		std::vector<unsigned char> data;
	};

	struct VerifyResult
	{
		std::string   path;
		bool          ok;
		unsigned long size;
		unsigned long crc;
	};

	// Used by directory listing worker in .cpp (must be public for file-scope thread structs).
	struct LocalScanRow
	{
		std::string   sortKey;
		bool          isDir;
		unsigned long localSize;
		bool          hasLocal;
	};

signals:
	void statusMessage(const char * msg);
	void selectedPathChanged(const std::string & path);

public slots:
	void onRefresh();
	void onSend();
	void onRetrieve();
	void onInfo();
	void onScopeChanged();
	void onSelectionChanged(QListViewItem * item);
	void onItemExpanded(QListViewItem * item);
	void onBatchInsertTimer();

protected:
	virtual void customEvent(QCustomEvent * event);

private:
	FileServerTreeWindow(const FileServerTreeWindow &);
	FileServerTreeWindow & operator=(const FileServerTreeWindow &);

	void addPlaceholder(QListViewItem * parentItem);
	bool isPlaceholder(QListViewItem * item) const;
	bool isDirectory(const std::string & name) const;
	void clearDetailsPane();
	void updateButtonStates();
	void setBusy(bool busy);

	std::string resolveLocalDir(const std::string & scopePath) const;
	void populateFromLocal(QListViewItem * parentItem, const std::string & scopePath);
	void completeMergedPopulation(unsigned token, QListViewItem * parentItem, const std::string & scopePath, bool listOk,
		std::vector<LocalScanRow> const & localRows, std::vector<std::string> const & remotePaths, std::vector<unsigned long> const & remoteSizes);

	std::string     m_activePopulateScope;
	QListViewItem * m_activePopulateParent;
	unsigned        m_populateToken;

	struct PendingBatch
	{
		struct Entry
		{
			// Plain relative segment (e.g. "foo/" or "bar.iff") — no (L)/(R) suffix
			std::string sortKey;
			// Full text for QListView column 0, including availability suffix
			std::string treeText;
			bool        isDir;

			bool operator<(const Entry & rhs) const
			{
				if (isDir != rhs.isDir)
					return isDir;
				return sortKey < rhs.sortKey;
			}
		};
		QListViewItem *        parentItem;
		std::string            scopePath;
		std::vector<Entry>     entries;
		size_t                 nextIndex;
	};

	QSplitter *   m_splitter;
	QListView *   m_treeView;
	QWidget *     m_detailsWidget;
	QLabel *      m_detailPath;
	QLabel *      m_detailLocalStatus;
	QLabel *      m_detailRemoteStatus;
	QLabel *      m_detailLocalSize;
	QLabel *      m_detailRemoteSize;

	QPushButton * m_sendButton;
	QPushButton * m_retrieveButton;
	QPushButton * m_infoButton;
	QPushButton * m_refreshButton;
	QLineEdit *   m_scopeEdit;
	QLabel *      m_statusLabel;

	std::string   m_rootScope;
	std::map<std::string, bool> m_expandedDirs;
	bool          m_busy;

	QTimer *      m_batchTimer;
	PendingBatch  m_pendingBatch;

	// Latest known sizes per asset path (from merged local/remote listing)
	std::map<std::string, FileEntry> m_entryCache;
};

// ======================================================================

#endif
