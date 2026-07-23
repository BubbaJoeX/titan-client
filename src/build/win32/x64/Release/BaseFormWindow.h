/****************************************************************************
** Form interface generated from reading ui file 'D:\titan\client\src\game\client\application\SwgGodClient\src\shared\ui\BaseFormWindow.ui'
**
** Created: Thu Jul 23 03:18:12 2026
**      by: The User Interface Compiler ($Id: qt/main.cpp   3.3.4   edited Nov 24 2003 $)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#ifndef BASEFORMWINDOW_H
#define BASEFORMWINDOW_H

#include <qvariant.h>
#include <qdialog.h>

class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;
class QSpacerItem;
class QPushButton;

class BaseFormWindow : public QDialog
{
    Q_OBJECT

public:
    BaseFormWindow( QWidget* parent = 0, const char* name = 0, bool modal = FALSE, WFlags fl = 0 );
    ~BaseFormWindow();

    QPushButton* m_cancelButton;
    QPushButton* m_okButton;

public slots:
    virtual void onOkPressed();
    virtual void onCancelPressed();

protected:
    QGridLayout* BaseFormWindowLayout;
    QSpacerItem* m_middleSpacer;
    QHBoxLayout* m_layoutBottom;
    QSpacerItem* m_bottomSpacer;

protected slots:
    virtual void languageChange();

};

#endif // BASEFORMWINDOW_H
