// ======================================================================
//
// FileServerTreeWindow.h
// copyright 2024 Sony Online Entertainment
//
// Lazy-loading folder tree for the FileControl server with a details
// pane.  Only the immediate children of a folder are fetched when the
// user expands it, preventing overload on large asset trees.
//
// Network operations run on a background thread so the UI stays
// responsive.
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
		CE_LISTING_DONE  = 10001,
		CE_SEND_DONE     = 10002,
		CE_RETRIEVE_DONE = 10003,
		CE_VERIFY_DONE   = 10004
	};

	struct ListingResult
	{
		std::string                dirPath;
		bool                       ok;
		std::vector<std::string>   files;
		std::vector<unsigned long> sizes;
		QListViewItem *            parentItem;
		bool                       isTopLevel;
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
	void applyListingResult(ListingResult * result);
	void setBusy(bool busy);

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
};

// ======================================================================

#endif
