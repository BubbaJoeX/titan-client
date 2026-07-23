/****************************************************************************
** Form interface generated from reading ui file 'D:\titan\client\src\game\client\application\SwgGodClient\src\shared\ui\BaseGroupObjectWindow.ui'
**
** Created: Thu Jul 23 03:18:13 2026
**      by: The User Interface Compiler ($Id: qt/main.cpp   3.3.4   edited Nov 24 2003 $)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#ifndef BASEGROUPOBJECTWINDOW_H
#define BASEGROUPOBJECTWINDOW_H

#include <qvariant.h>
#include <qpixmap.h>
#include <qwidget.h>

class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;
class QSpacerItem;
class BrushListView;
class PaletteListView;
class QTabWidget;

class BaseGroupObjectWindow : public QWidget
{
    Q_OBJECT

public:
    BaseGroupObjectWindow( QWidget* parent = 0, const char* name = 0, WFlags fl = 0 );
    ~BaseGroupObjectWindow();

    QTabWidget* m_groupObjectsTabs;
    QWidget* Brushes;
    BrushListView* m_brushesList;
    QWidget* Palettes;
    PaletteListView* m_palettesList;

protected:
    QGridLayout* BaseGroupObjectWindowLayout;
    QGridLayout* BrushesLayout;
    QGridLayout* PalettesLayout;

protected slots:
    virtual void languageChange();

private:
    QPixmap image0;

};

#endif // BASEGROUPOBJECTWINDOW_H
