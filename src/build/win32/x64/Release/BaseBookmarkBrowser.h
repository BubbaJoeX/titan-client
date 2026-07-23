/****************************************************************************
** Form interface generated from reading ui file 'D:\titan\client\src\game\client\application\SwgGodClient\src\shared\ui\BaseBookmarkBrowser.ui'
**
** Created: Thu Jul 23 03:18:11 2026
**      by: The User Interface Compiler ($Id: qt/main.cpp   3.3.4   edited Nov 24 2003 $)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#ifndef BASEBOOKMARKBROWSER_H
#define BASEBOOKMARKBROWSER_H

#include <qvariant.h>
#include <qwidget.h>

class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;
class QSpacerItem;
class QListView;
class QListViewItem;
class QToolButton;

class BaseBookmarkBrowser : public QWidget
{
    Q_OBJECT

public:
    BaseBookmarkBrowser( QWidget* parent = 0, const char* name = 0, WFlags fl = 0 );
    ~BaseBookmarkBrowser();

    QListView* m_bookmarkList;
    QToolButton* m_deleteButton;

protected:
    QGridLayout* BaseBookmarkBrowserLayout;
    QSpacerItem* Spacer1;

protected slots:
    virtual void languageChange();

};

#endif // BASEBOOKMARKBROWSER_H
