/****************************************************************************
** Form interface generated from reading ui file 'D:\titan\client\src\game\client\application\SwgGodClient\src\shared\ui\BaseFavoritesWindow.ui'
**
** Created: Thu Jul 23 03:18:12 2026
**      by: The User Interface Compiler ($Id: qt/main.cpp   3.3.4   edited Nov 24 2003 $)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#ifndef BASEFAVORITESWINDOW_H
#define BASEFAVORITESWINDOW_H

#include <qvariant.h>
#include <qpixmap.h>
#include <qwidget.h>

class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;
class QSpacerItem;
class FavoritesListView;

class BaseFavoritesWindow : public QWidget
{
    Q_OBJECT

public:
    BaseFavoritesWindow( QWidget* parent = 0, const char* name = 0, WFlags fl = 0 );
    ~BaseFavoritesWindow();

    FavoritesListView* m_favoritesList;

protected:
    QHBoxLayout* BaseFavoritesWindowLayout;

protected slots:
    virtual void languageChange();

private:
    QPixmap image0;

};

#endif // BASEFAVORITESWINDOW_H
