/****************************************************************************
** Form interface generated from reading ui file 'D:\titan\client\src\game\client\application\SwgGodClient\src\shared\ui\BaseFilterWindow.ui'
**
** Created: Thu Jul 23 03:18:12 2026
**      by: The User Interface Compiler ($Id: qt/main.cpp   3.3.4   edited Nov 24 2003 $)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#ifndef BASEFILTERWINDOW_H
#define BASEFILTERWINDOW_H

#include <qvariant.h>
#include <qpixmap.h>
#include <qwidget.h>

class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;
class QSpacerItem;
class ServerTemplateListView;
class ClientTemplateListView;
class QTabWidget;
class QCheckBox;
class QLabel;
class QLineEdit;

class BaseFilterWindow : public QWidget
{
    Q_OBJECT

public:
    BaseFilterWindow( QWidget* parent = 0, const char* name = 0, WFlags fl = 0 );
    ~BaseFilterWindow();

    QTabWidget* m_tabWidget;
    QWidget* NetworkIdTab;
    QCheckBox* m_networkIdFilterCheck;
    QLabel* TextLabel1_2;
    QLabel* TextLabel1;
    QLineEdit* m_networkIdLowerBoundEdit;
    QLineEdit* m_networkIdUpperBoundEdit;
    QWidget* DistanceTab;
    QCheckBox* m_radiusFilterCheck;
    QLineEdit* m_maxDistanceEdit;
    QLabel* TextLabel2_3;
    QLabel* TextLabel3;
    QLineEdit* m_minDistanceEdit;
    QLabel* TextLabel2_2;
    QLabel* TextLabel2;
    QWidget* tab;
    ClientTemplateListView* m_clientTemplateListView;
    ServerTemplateListView* m_serverTemplateListView;
    QCheckBox* m_objectIdFilterCheck_3_2;
    QCheckBox* m_objectIdFilterCheck_3;

protected:
    QGridLayout* BaseFilterWindowLayout;
    QGridLayout* NetworkIdTabLayout;
    QGridLayout* DistanceTabLayout;
    QGridLayout* tabLayout;

protected slots:
    virtual void languageChange();

private:
    QPixmap image0;

};

#endif // BASEFILTERWINDOW_H
