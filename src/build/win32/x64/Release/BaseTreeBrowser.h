/****************************************************************************
** Form interface generated from reading ui file 'D:\titan\client\src\game\client\application\SwgGodClient\src\shared\ui\BaseTreeBrowser.ui'
**
** Created: Thu Jul 23 03:18:16 2026
**      by: The User Interface Compiler ($Id: qt/main.cpp   3.3.4   edited Nov 24 2003 $)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#ifndef BASETREEBROWSER_H
#define BASETREEBROWSER_H

#include <qvariant.h>
#include <qpixmap.h>
#include <qwidget.h>

class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;
class QSpacerItem;
class ScriptListView;
class ServerTemplateListView;
class ClientTemplateListView;
class BuildoutAreaListView;
class QTabWidget;
class QListView;
class QListViewItem;
class QPushButton;
class QLineEdit;

class BaseTreeBrowser : public QWidget
{
    Q_OBJECT

public:
    BaseTreeBrowser( QWidget* parent = 0, const char* name = 0, WFlags fl = 0 );
    ~BaseTreeBrowser();

    QTabWidget* m_templateTabs;
    QWidget* ObjectTab;
    QListView* m_objectList;
    QPushButton* m_refreshButton;
    QLineEdit* m_objectSearchEdit;
    QPushButton* m_objectSearchClearButton;
    QWidget* ScriptsTab;
    QPushButton* m_scriptRefreshButton;
    QLineEdit* m_scriptSearchEdit;
    QPushButton* m_scriptSearchClearButton;
    ScriptListView* m_scriptList;
    QWidget* ServerTemplatesTab;
    QPushButton* m_serverTemplateRefreshButton;
    QLineEdit* m_serverTemplateSearchEdit;
    QPushButton* m_serverTemplateSearchClearButton;
    ServerTemplateListView* m_serverTemplateList;
    QWidget* ClientTemplatesTab;
    QPushButton* m_clientTemplateRefreshButton;
    QLineEdit* m_clientTemplateSearchEdit;
    QPushButton* m_clientTemplateSearchClearButton;
    ClientTemplateListView* m_clientTemplateList;
    QWidget* BuildoutAreasTab;
    QPushButton* m_buildoutAreaRefreshButton;
    BuildoutAreaListView* m_buildoutAreaList;

protected:
    QGridLayout* BaseTreeBrowserLayout;
    QGridLayout* ObjectTabLayout;
    QSpacerItem* objectsSpacer;
    QGridLayout* ScriptsTabLayout;
    QSpacerItem* scriptsSpacer;
    QGridLayout* ServerTemplatesTabLayout;
    QSpacerItem* serverTemplateSpacer;
    QGridLayout* ClientTemplatesTabLayout;
    QSpacerItem* clientTemplateSpacer;
    QGridLayout* BuildoutAreasTabLayout;
    QSpacerItem* buildoutAreaSpacer;

protected slots:
    virtual void languageChange();

private:
    QPixmap image0;
    QPixmap image1;

};

#endif // BASETREEBROWSER_H
