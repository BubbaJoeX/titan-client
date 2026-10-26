// ======================================================================
//
// CommandPaletteDialog.h
//
// God Client tool: type to find any menu command and run it (Ctrl+K),
// and the keyboard / mouse / pen cheat sheet (Help > Keyboard Shortcuts).
//
// ======================================================================

#ifndef INCLUDED_CommandPaletteDialog_H
#define INCLUDED_CommandPaletteDialog_H

// ======================================================================

#include <qdialog.h>
#include <qguardedptr.h>

#include <vector>

class QAction;
class QCheckBox;
class QLabel;
class QLineEdit;
class QListView;
class QListViewItem;

// ======================================================================

class CommandPaletteDialog : public QDialog
{
	Q_OBJECT; //lint !e1516 !e19 !e1924 !e1762

public:

	/// Lists the QActions found under \a actionOwner (the main window) plus the 3D view's built-in keys.
	CommandPaletteDialog(QWidget * parent, QObject * actionOwner, bool shortcutsOnly, char const * name = 0);
	virtual ~CommandPaletteDialog();

	/// Modal; runs the chosen command after the dialog closes.
	static void run(QWidget * parent, QObject * actionOwner, bool shortcutsOnly);

protected:

	virtual bool eventFilter(QObject * watched, QEvent * event);

private slots:

	void onFilterChanged(QString const & text);
	void onShortcutsOnlyToggled(bool on);
	void onItemExecuted(QListViewItem * item);
	void onReturnPressed();

private:

	struct Entry
	{
		QString              command;
		QString              shortcut;
		QString              where;
		QGuardedPtr<QAction> action;  // null for built-in keys, which are listed but not run
	};

	void collectActions(QObject * actionOwner);
	void addBuiltInKeys();
	void rebuildList();
	void chooseCurrent();

private:

	CommandPaletteDialog(CommandPaletteDialog const &);
	CommandPaletteDialog & operator=(CommandPaletteDialog const &);

private:

	std::vector<Entry>   m_entries;
	QLineEdit *          m_filterEdit;
	QCheckBox *          m_shortcutsOnlyCheck;
	QListView *          m_list;
	QLabel *             m_countLabel;
	QGuardedPtr<QAction> m_chosen;
};

// ======================================================================

#endif
