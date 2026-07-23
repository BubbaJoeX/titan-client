/****************************************************************************
** Form interface generated from reading ui file 'D:\titan\client\src\game\client\application\SwgGodClient\src\shared\ui\BaseTriggerWindow.ui'
**
** Created: Thu Jul 23 03:18:16 2026
**      by: The User Interface Compiler ($Id: qt/main.cpp   3.3.4   edited Nov 24 2003 $)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#ifndef BASETRIGGERWINDOW_H
#define BASETRIGGERWINDOW_H

#include <qvariant.h>
#include <qdialog.h>

class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;
class QSpacerItem;
class QTable;
class QPushButton;

class BaseTriggerWindow : public QDialog
{
    Q_OBJECT

public:
    BaseTriggerWindow( QWidget* parent = 0, const char* name = 0, bool modal = FALSE, WFlags fl = 0 );
    ~BaseTriggerWindow();

    QTable* m_triggerTable;
    QPushButton* m_okButton;
    QPushButton* m_cancelButton;

protected:
    QGridLayout* BaseTriggerWindowLayout;

protected slots:
    virtual void languageChange();

};

#endif // BASETRIGGERWINDOW_H
