// ======================================================================
//
// CommandPaletteDialog.cpp
//
// ======================================================================

#include "SwgGodClient/FirstSwgGodClient.h"
#include "CommandPaletteDialog.h"
#include "CommandPaletteDialog.moc"

#include <qaccel.h>
#include <qaction.h>
#include <qapplication.h>
#include <qcheckbox.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qlineedit.h>
#include <qlistview.h>
#include <qobjectlist.h>
#include <qsignal.h>
#include <qstringlist.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <utility>
// ======================================================================

namespace CommandPaletteDialogNamespace
{
	/// The 3D view's hardcoded keys and mouse/pen gestures (GameWidget), listed for reference only.
	struct BuiltInKey
	{
		char const * command;
		char const * shortcut;
		char const * where;
	};

	BuiltInKey const s_builtInKeys[] =
	{
		{ "Move selection on screen plane / up-down / toward camera", "Hold T + left / right / middle drag", "3D view" },
		{ "Move selection along the ground / up-down",               "Hold G + left / right drag",           "3D view" },
		{ "Rotate selection: yaw / pitch / roll",                     "Hold R + left / middle / right drag",  "3D view" },
		{ "Scale selection: height / footprint / uniform",            "Hold S + left / right / middle drag",  "3D view" },
		{ "Apply transform (commit ghosts)",                          "Space",                                "3D view" },
		{ "World / local transform axes",                             "Ctrl+Shift+L",                         "3D view" },
		{ "Clear ghosts, then the selection",                         "Esc",                                  "3D view" },
		{ "Rotate selection 45 degrees",                              "< / >",                                "3D view" },
		{ "Raise / lower selection",                                  "; / '",                                "3D view" },
		{ "Nudge selection",                                          "Arrow keys",                           "3D view" },
		{ "Push selection away / pull toward camera (10%)",           "[ / ]",                                "3D view" },
		{ "Terrain brush size (Shift: strength)",                     "[ / ]  with a terrain tool",           "3D view" },
		{ "Persist selection to the database",                        "P",                                    "3D view" },
		{ "Go to coordinates",                                        "=",                                    "3D view" },
		{ "Warp avatar to the cursor",                                "A",                                    "3D view" },
		{ "Select / add / toggle object",                             "Click / Shift+click / Ctrl+click",     "mouse" },
		{ "Rubber-band select",                                       "Left drag on empty ground",            "mouse" },
		{ "Drag a selected object",                                   "Left drag on it",                      "mouse" },
		{ "Center camera on object or ground",                        "Double-click",                         "mouse" },
		{ "Context menu",                                             "Right click",                          "mouse" },
		{ "Orbit camera",                                             "Alt + left drag",                      "camera" },
		{ "Pan camera",                                               "Alt + right drag",                     "camera" },
		{ "Zoom camera",                                              "Alt + middle drag, or wheel",          "camera" },
		{ "Fly-look / slide along ground",                            "Alt+Ctrl + left / right drag",         "camera" },
		{ "Fly forward",                                              "Alt+Ctrl + hold middle (Shift runs)",  "camera" },
		{ "Orbit camera with the pen",                                "Barrel button + drag",                 "pen" },
		{ "Pan / zoom camera with the pen",                           "Barrel + Shift / Ctrl + drag",         "pen" },
		{ "Swap terrain Raise and Lower",                             "Eraser end",                           "pen" },
		{ "Pressure-scaled terrain brush",                            "Pen tip pressure",                     "pen" }
	};

	/// List row that remembers which entry it shows.
	class EntryItem : public QListViewItem
	{
	public:
		EntryItem(QListView * const parent, size_t const index, QString const & command, QString const & shortcut, QString const & where)
		: QListViewItem(parent, command, shortcut, where),
		  m_index(index)
		{
		}

		size_t getIndex() const
		{
			return m_index;
		}

	private:
		size_t m_index;
	};

	/// How well one typed word matches a row, lower is better: command prefix, start of a word
	/// in the command, anywhere in the command, anywhere in the shortcut or notes; -1 = no match.
	int scoreTerm(QString const & term, QString const & command, QString const & rest)
	{
		if (command.startsWith(term))
			return 0;

		int const at = command.find(term);
		if (at > 0)
		{
			QChar const before = command[at - 1];
			return (before.isLetterOrNumber()) ? 2 : 1;
		}

		return rest.find(term) >= 0 ? 3 : -1;
	}

	QString cleanMenuText(QString text)
	{
		// "&Undo" -> "Undo", "Rotate &&Move" -> "Rotate &Move"
		text.replace("&&", "\x01");
		text.replace("&", "");
		text.replace("\x01", "&");
		return text.stripWhiteSpace();
	}
}

using namespace CommandPaletteDialogNamespace;

// ======================================================================

CommandPaletteDialog::CommandPaletteDialog(QWidget * const parent, QObject * const actionOwner, bool const shortcutsOnly, char const * const name)
: QDialog(parent, name, true),
  m_entries(),
  m_filterEdit(0),
  m_shortcutsOnlyCheck(0),
  m_list(0),
  m_countLabel(0),
  m_chosen()
{
	setCaption(shortcutsOnly ? "Keyboard Shortcuts" : "Command Palette");

	QVBoxLayout * const outer = new QVBoxLayout(this, 8, 6);

	QHBoxLayout * const top = new QHBoxLayout(0, 0, 6);
	m_filterEdit = new QLineEdit(this, "commandFilter");
	m_shortcutsOnlyCheck = new QCheckBox("Only with shortcuts", this);
	m_shortcutsOnlyCheck->setChecked(shortcutsOnly);
	top->addWidget(m_filterEdit, 1);
	top->addWidget(m_shortcutsOnlyCheck);
	outer->addLayout(top);

	m_list = new QListView(this, "commandList");
	IGNORE_RETURN(m_list->addColumn("Command"));
	IGNORE_RETURN(m_list->addColumn("Shortcut"));
	IGNORE_RETURN(m_list->addColumn("Notes"));
	m_list->setAllColumnsShowFocus(true);
	m_list->setSorting(-1); // rebuildList orders rows by match quality
	m_list->setMinimumSize(640, 420);
	outer->addWidget(m_list, 1);

	m_countLabel = new QLabel(this);
	outer->addWidget(m_countLabel);

	collectActions(actionOwner);
	addBuiltInKeys();
	rebuildList();

	m_filterEdit->installEventFilter(this);
	m_filterEdit->setFocus();

	IGNORE_RETURN(connect(m_filterEdit, SIGNAL(textChanged(const QString &)), this, SLOT(onFilterChanged(const QString &))));
	IGNORE_RETURN(connect(m_filterEdit, SIGNAL(returnPressed()), this, SLOT(onReturnPressed())));
	IGNORE_RETURN(connect(m_shortcutsOnlyCheck, SIGNAL(toggled(bool)), this, SLOT(onShortcutsOnlyToggled(bool))));
	IGNORE_RETURN(connect(m_list, SIGNAL(doubleClicked(QListViewItem *)), this, SLOT(onItemExecuted(QListViewItem *))));
	IGNORE_RETURN(connect(m_list, SIGNAL(returnPressed(QListViewItem *)), this, SLOT(onItemExecuted(QListViewItem *))));
}

// ----------------------------------------------------------------------

CommandPaletteDialog::~CommandPaletteDialog()
{
}

// ----------------------------------------------------------------------

void CommandPaletteDialog::run(QWidget * const parent, QObject * const actionOwner, bool const shortcutsOnly)
{
	QGuardedPtr<QAction> chosen;
	{
		CommandPaletteDialog dialog(parent, actionOwner, shortcutsOnly, "CommandPaletteDialog");
		if (dialog.exec() != QDialog::Accepted)
			return;
		chosen = dialog.m_chosen;
	}

	//-- run it after the dialog is gone, so modal commands open over the main window
	if (!chosen || !chosen->isEnabled())
		return;

	//-- the same path as a menu click: a toggle action flips, then activated() fires
	QSignal trigger;
	if (trigger.connect(chosen, SLOT(internalActivation())))
		trigger.activate();
	else if (chosen->isToggleAction())
		chosen->setOn(!chosen->isOn());
	else
		chosen->activate();
}

// ----------------------------------------------------------------------

bool CommandPaletteDialog::eventFilter(QObject * const watched, QEvent * const event)
{
	//-- Up / Down in the filter box move through the list, so the mouse is never needed
	if (watched == m_filterEdit && event->type() == QEvent::KeyPress)
	{
		QKeyEvent * const keyEvent = static_cast<QKeyEvent *>(event);
		QListViewItem * const current = m_list->currentItem();
		QListViewItem * next = 0;
		if (keyEvent->key() == static_cast<int>(Qt::Key_Down))
			next = current ? current->itemBelow() : m_list->firstChild();
		else if (keyEvent->key() == static_cast<int>(Qt::Key_Up))
			next = current ? current->itemAbove() : 0;
		else
			return QDialog::eventFilter(watched, event);

		if (next)
		{
			m_list->setCurrentItem(next);
			m_list->setSelected(next, true);
			m_list->ensureItemVisible(next);
		}
		return true;
	}

	return QDialog::eventFilter(watched, event);
}

// ----------------------------------------------------------------------

void CommandPaletteDialog::onFilterChanged(QString const &)
{
	rebuildList();
}

// ----------------------------------------------------------------------

void CommandPaletteDialog::onShortcutsOnlyToggled(bool)
{
	rebuildList();
}

// ----------------------------------------------------------------------

void CommandPaletteDialog::onItemExecuted(QListViewItem * const item)
{
	if (item)
		m_list->setCurrentItem(item);
	chooseCurrent();
}

// ----------------------------------------------------------------------

void CommandPaletteDialog::onReturnPressed()
{
	chooseCurrent();
}

// ----------------------------------------------------------------------

void CommandPaletteDialog::collectActions(QObject * const actionOwner)
{
	if (!actionOwner)
		return;

	QObjectList * const actions = actionOwner->queryList("QAction");
	if (!actions)
		return;

	for (QObjectListIt it(*actions); it.current(); ++it)
	{
		QAction * const action = static_cast<QAction *>(it.current());
		if (action->inherits("QActionGroup"))
			continue;

		Entry entry;
		entry.command = cleanMenuText(action->menuText().isEmpty() ? action->text() : action->menuText());
		if (entry.command.isEmpty())
			continue;

		if (!action->accel().isEmpty())
			entry.shortcut = QAccel::keyToString(action->accel());

		//-- actions inside a group (Rotate Mode, Pivot Mode) say which group
		QObject const * const owner = action->parent();
		if (owner && owner->inherits("QActionGroup"))
			entry.where = cleanMenuText(static_cast<QAction const *>(owner)->menuText());

		if (action->isToggleAction())
			entry.where += entry.where.isEmpty() ? (action->isOn() ? "on" : "off") : (action->isOn() ? ", on" : ", off");

		entry.action = action;
		m_entries.push_back(entry);
	}

	delete actions;
}

// ----------------------------------------------------------------------

void CommandPaletteDialog::addBuiltInKeys()
{
	for (size_t i = 0; i < sizeof(s_builtInKeys) / sizeof(s_builtInKeys[0]); ++i)
	{
		Entry entry;
		entry.command  = s_builtInKeys[i].command;
		entry.shortcut = s_builtInKeys[i].shortcut;
		entry.where    = s_builtInKeys[i].where;
		m_entries.push_back(entry);
	}
}

// ----------------------------------------------------------------------

void CommandPaletteDialog::rebuildList()
{
	m_list->clear();

	//-- every word typed must appear somewhere in the row; best matches first, runnable commands
	//-- ahead of the reference-only 3D view rows, then alphabetical
	QStringList const terms(QStringList::split(' ', m_filterEdit->text().lower().simplifyWhiteSpace()));
	bool const shortcutsOnly = m_shortcutsOnlyCheck->isChecked();

	std::vector<std::pair<std::string, size_t> > ranked;
	for (size_t i = 0; i < m_entries.size(); ++i)
	{
		Entry const & entry = m_entries[i];
		if (shortcutsOnly && entry.shortcut.isEmpty())
			continue;

		QString const command(entry.command.lower());
		QString const rest((entry.shortcut + " " + entry.where).lower());
		int score = 0;
		bool matches = true;
		for (QStringList::ConstIterator t = terms.begin(); t != terms.end() && matches; ++t)
		{
			int const termScore = scoreTerm(*t, command, rest);
			matches = termScore >= 0;
			score += termScore;
		}
		if (!matches)
			continue;

		char prefix[16];
		IGNORE_RETURN(_snprintf(prefix, sizeof(prefix), "%04d %d ", score, entry.action ? 0 : 1));
		prefix[sizeof(prefix) - 1] = '\0';
		ranked.push_back(std::make_pair(std::string(prefix) + (command.latin1() ? command.latin1() : ""), i));
	}
	std::sort(ranked.begin(), ranked.end());

	int shown = 0;
	QListViewItem * after = 0;
	for (size_t r = 0; r < ranked.size(); ++r)
	{
		size_t const i = ranked[r].second;
		Entry const & entry = m_entries[i];

		QString where(entry.where);
		if (entry.action && !entry.action->isEnabled())
			where += where.isEmpty() ? "disabled" : ", disabled";

		EntryItem * const item = new EntryItem(m_list, i, entry.command, entry.shortcut, where);
		if (after)
			item->moveItem(after);
		after = item;
		++shown;
	}

	QListViewItem * const first = m_list->firstChild();
	if (first)
	{
		m_list->setCurrentItem(first);
		m_list->setSelected(first, true);
	}

	QString count;
	count.sprintf("%d of %d. Enter or double-click runs a command; 3D view, mouse, camera and pen rows are reference only.", shown, static_cast<int>(m_entries.size()));
	m_countLabel->setText(count);
}

// ----------------------------------------------------------------------

void CommandPaletteDialog::chooseCurrent()
{
	EntryItem const * const item = static_cast<EntryItem const *>(m_list->currentItem());
	if (!item)
		return;

	Entry const & entry = m_entries[item->getIndex()];
	if (!entry.action)
	{
		m_countLabel->setText(entry.command + " is a 3D view gesture: " + entry.shortcut + ".");
		return;
	}

	if (!entry.action->isEnabled())
	{
		QApplication::beep();
		m_countLabel->setText(entry.command + " is disabled right now (it usually needs a selection).");
		return;
	}

	m_chosen = entry.action;
	accept();
}

// ======================================================================
