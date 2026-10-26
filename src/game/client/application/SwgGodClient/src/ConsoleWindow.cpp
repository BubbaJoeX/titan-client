// ======================================================================
//
// ConsoleWindow.cpp
// copyright(c) 2001 Sony Online Entertainment
//
// ======================================================================

#include "SwgGodClient/FirstSwgGodClient.h"
#include "ConsoleWindow.h"
#include "ConsoleWindow.moc"

#include "sharedNetworkMessages/ConsoleChannelMessages.h"

#include "clientGame/GameNetwork.h"

#include "GodClientData.h"
#include "GodClientPerforce.h"
#include "ServerCommander.h"

#include <qlayout.h>
#include <qlineedit.h>
#include <qtextview.h>
#include <qtooltip.h>

#include <vector>

// ======================================================================

namespace ConsoleWindowNamespace
{
	/// Console input with Up / Down command history.
	class ConsoleInputLine : public QLineEdit
	{
	public:
		explicit ConsoleInputLine(QWidget * const parent)
		: QLineEdit(parent, "consoleInput"),
		  m_history(),
		  m_index(0)
		{
		}

		void remember(QString const & line)
		{
			if (!line.isEmpty() && (m_history.empty() || m_history.back() != line))
				m_history.push_back(line);
			if (m_history.size() > 50)
				IGNORE_RETURN(m_history.erase(m_history.begin()));
			m_index = m_history.size();
		}

	protected:
		virtual void keyPressEvent(QKeyEvent * e)
		{
			if (e->key() == static_cast<int>(Qt::Key_Up))
			{
				if (m_index > 0)
					setText(m_history[--m_index]);
				return;
			}
			if (e->key() == static_cast<int>(Qt::Key_Down))
			{
				if (m_index + 1 < m_history.size())
					setText(m_history[++m_index]);
				else
				{
					m_index = m_history.size();
					clear();
				}
				return;
			}
			QLineEdit::keyPressEvent(e);
		}

	private:
		std::vector<QString> m_history;
		size_t               m_index;
	};
}

using namespace ConsoleWindowNamespace;

// ======================================================================

ConsoleWindow::ConsoleWindow(QWidget*parent, const char*name)
: BaseConsoleWindow(parent, name),
  MessageDispatch::Receiver(),
  m_input(0)
{
	connectToMessage(GodClientPerforce::Messages::COMMAND_MESSAGE);
	connectToMessage("ConGenericMessage");
	IGNORE_RETURN(connect(&ServerCommander::getInstance(), SIGNAL(consoleCommandIssued(const char*)), this, SLOT(onConsoleCommandIssued(const char*))));

	m_input = new ConsoleInputLine(this);
	QToolTip::add(m_input, "Server console command (e.g. object get <id>), or /command args for a slash command on the selected object. Up/Down: history.");
	QGridLayout * const grid = dynamic_cast<QGridLayout *>(layout());
	if (grid)
		grid->addWidget(m_input, 1, 0);
	IGNORE_RETURN(connect(m_input, SIGNAL(returnPressed()), this, SLOT(onCommandEntered())));
}

//-----------------------------------------------------------------

ConsoleWindow::~ConsoleWindow()
{
}

//-----------------------------------------------------------------

void ConsoleWindow::receiveMessage(const MessageDispatch::Emitter & source, const MessageDispatch::MessageBase & message)
{
	UNREF(source);
	if(message.isType("ConGenericMessage"))
	{
		Archive::ReadIterator ri = dynamic_cast<const GameNetworkMessage &>(message).getByteStream().begin();
		ConGenericMessage cg(ri);
		m_textView->append(cg.getMsg().c_str());		
	}
	else if(message.isType(GodClientPerforce::Messages::COMMAND_MESSAGE))
	{
		const GodClientPerforce::CommandMessage* const cm = NON_NULL(dynamic_cast<const GodClientPerforce::CommandMessage*>(&message));
		const std::string txt = std::string("P4: ") + cm->getMessage();
		m_textView->append(txt.c_str());
	}
}

//-----------------------------------------------------------------

void ConsoleWindow::onConsoleCommandIssued(const char* cmd)
{
	m_textView->append(cmd);
}

//-----------------------------------------------------------------

void ConsoleWindow::onCommandEntered()
{
	QString const line(m_input->text().stripWhiteSpace());
	if (line.isEmpty())
		return;

	static_cast<ConsoleInputLine *>(m_input)->remember(line);
	m_input->clear();

	GodClientData::ObjectList_t selection;
	GodClientData::getInstance().getSelection(selection);
	NetworkId const target(selection.empty() ? NetworkId::cms_invalid : selection.front()->getNetworkId());

	IGNORE_RETURN(ServerCommander::getInstance().consoleCommand(std::string(line.latin1()), target));
}


// ======================================================================
