/****************************************************************************
** Form interface generated from reading ui file 'D:\titan\client\src\game\client\application\SwgGodClient\src\shared\ui\BaseObjectEditor.ui'
**
** Created: Thu Jul 23 03:18:14 2026
**      by: The User Interface Compiler ($Id: qt/main.cpp   3.3.4   edited Nov 24 2003 $)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#ifndef BASEOBJECTEDITOR_H
#define BASEOBJECTEDITOR_H

#include <qvariant.h>
#include <qwidget.h>

class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;
class QSpacerItem;
class QTabWidget;
class QListView;
class QListViewItem;
class QListBox;
class QListBoxItem;

class BaseObjectEditor : public QWidget
{
    Q_OBJECT

public:
    BaseObjectEditor( QWidget* parent = 0, const char* name = 0, WFlags fl = 0 );
    ~BaseObjectEditor();

    QTabWidget* m_propertyTabs;
    QWidget* AttributesTab;
    QListView* m_attributesList;
    QWidget* tab;
    QListView* m_scriptsList;
    QWidget* tab_2;
    QListView* m_objVarsList;
    QWidget* tab_3;
    QListBox* m_creatureSkills;

protected:
    QGridLayout* BaseObjectEditorLayout;
    QGridLayout* AttributesTabLayout;
    QGridLayout* tabLayout;
    QGridLayout* tabLayout_2;
    QHBoxLayout* tabLayout_3;

protected slots:
    virtual void languageChange();

    virtual void init();
    virtual void destroy();


};

#endif // BASEOBJECTEDITOR_H
