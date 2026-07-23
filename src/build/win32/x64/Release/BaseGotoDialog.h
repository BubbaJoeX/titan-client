/****************************************************************************
** Form interface generated from reading ui file 'D:\titan\client\src\game\client\application\SwgGodClient\src\shared\ui\BaseGotoDialog.ui'
**
** Created: Thu Jul 23 03:18:13 2026
**      by: The User Interface Compiler ($Id: qt/main.cpp   3.3.4   edited Nov 24 2003 $)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#ifndef BASEGOTODIALOG_H
#define BASEGOTODIALOG_H

#include <qvariant.h>
#include <qdialog.h>

class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;
class QSpacerItem;
class QLabel;
class QLineEdit;
class QPushButton;

class BaseGotoDialog : public QDialog
{
    Q_OBJECT

public:
    BaseGotoDialog( QWidget* parent = 0, const char* name = 0, bool modal = FALSE, WFlags fl = 0 );
    ~BaseGotoDialog();

    QLabel* textLabel3;
    QLabel* textLabel2;
    QLabel* textLabel1;
    QLineEdit* m_z;
    QLineEdit* m_x;
    QLineEdit* m_y;
    QPushButton* m_okButton;
    QPushButton* m_cancelButton;
    QLabel* textLabel4;
    QLineEdit* m_distance;

protected:
    QGridLayout* BaseGotoDialogLayout;

protected slots:
    virtual void languageChange();

};

#endif // BASEGOTODIALOG_H
