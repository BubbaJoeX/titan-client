// ======================================================================
//
// ConsoleWindow.h
// copyright (c) 2001 Sony Online Entertainment
//
// ======================================================================

#ifndef INCLUDED_ConsoleWindow_H
#define INCLUDED_ConsoleWindow_H

// ======================================================================

#include "BaseConsoleWindow.h"

class QLineEdit;

// ======================================================================
/**
* ConsoleWindow outputs all console messages from the server, and takes typed
* commands: server console commands, or "/command" slash commands on the selection.
*/
class ConsoleWindow : public BaseConsoleWindow, public MessageDispatch::Receiver
{
	Q_OBJECT; //lint !e1516 !e19 !e1924 !e1762 various deficiencies in the Qt macro

public:
	explicit ConsoleWindow(QWidget *parent=0, const char *name=0);
	virtual ~ConsoleWindow();

	virtual void              receiveMessage (const MessageDispatch::Emitter & source, const MessageDispatch::MessageBase & message);

public slots:
	void onConsoleCommandIssued (const char *);

private slots:
	void onCommandEntered       ();

private:
	QLineEdit *               m_input;
};

// ======================================================================

#endif
