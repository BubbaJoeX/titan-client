/****************************************************************************
** Form interface generated from reading ui file 'D:\titan\client\src\game\client\application\SwgGodClient\src\shared\ui\BaseConsoleWindow.ui'
**
** Created: Thu Jul 23 03:18:11 2026
**      by: The User Interface Compiler ($Id: qt/main.cpp   3.3.4   edited Nov 24 2003 $)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#ifndef BASECONSOLEWINDOW_H
#define BASECONSOLEWINDOW_H

#include <qvariant.h>
#include <qwidget.h>

class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;
class QSpacerItem;
class QTextView;

class BaseConsoleWindow : public QWidget
{
    Q_OBJECT

public:
    BaseConsoleWindow( QWidget* parent = 0, const char* name = 0, WFlags fl = 0 );
    ~BaseConsoleWindow();

    QTextView* m_textView;

protected:
    QGridLayout* BaseConsoleWindowLayout;

protected slots:
    virtual void languageChange();

    virtual void init();
    virtual void destroy();


};

#endif // BASECONSOLEWINDOW_H
