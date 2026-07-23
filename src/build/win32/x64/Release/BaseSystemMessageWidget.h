/****************************************************************************
** Form interface generated from reading ui file 'D:\titan\client\src\game\client\application\SwgGodClient\src\shared\ui\BaseSystemMessageWidget.ui'
**
** Created: Thu Jul 23 03:18:15 2026
**      by: The User Interface Compiler ($Id: qt/main.cpp   3.3.4   edited Nov 24 2003 $)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#ifndef BASESYSTEMMESSAGEWIDGET_H
#define BASESYSTEMMESSAGEWIDGET_H

#include <qvariant.h>
#include <qwidget.h>

class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;
class QSpacerItem;
class QButtonGroup;
class QRadioButton;
class QGroupBox;
class QListView;
class QListViewItem;
class QLineEdit;
class QLabel;
class QPushButton;

class BaseSystemMessageWidget : public QWidget
{
    Q_OBJECT

public:
    BaseSystemMessageWidget( QWidget* parent = 0, const char* name = 0, WFlags fl = 0 );
    ~BaseSystemMessageWidget();

    QButtonGroup* m_targetSelectionButtonGroup;
    QRadioButton* m_currentPlanetRadioButton;
    QRadioButton* m_currentGalaxyRadioButton;
    QRadioButton* m_allSWGRadioButton;
    QRadioButton* m_sendToRoomRadioButton;
    QGroupBox* GroupBox1;
    QListView* m_roomsListView;
    QRadioButton* m_sendToPlayerRadioButton;
    QGroupBox* GroupBox2;
    QLineEdit* m_playerLineEdit;
    QLabel* TextLabel1;
    QLineEdit* m_messageLineEdit;
    QPushButton* m_cancelPushButton;
    QPushButton* m_sendPushButton;

public slots:
    virtual void sendMessage();

protected:
    QGridLayout* BaseSystemMessageWidgetLayout;
    QGridLayout* m_targetSelectionButtonGroupLayout;
    QVBoxLayout* Layout4;
    QVBoxLayout* Layout3;
    QVBoxLayout* Layout2;
    QVBoxLayout* Layout7;
    QSpacerItem* Spacer3;
    QHBoxLayout* Layout5;
    QSpacerItem* Spacer2;

protected slots:
    virtual void languageChange();

};

#endif // BASESYSTEMMESSAGEWIDGET_H
