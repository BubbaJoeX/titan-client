/****************************************************************************
** Form interface generated from reading ui file 'D:\titan\client\src\game\client\application\SwgGodClient\src\shared\ui\BaseGameWindow.ui'
**
** Created: Thu Jul 23 03:18:13 2026
**      by: The User Interface Compiler ($Id: qt/main.cpp   3.3.4   edited Nov 24 2003 $)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#ifndef BASEGAMEWINDOW_H
#define BASEGAMEWINDOW_H

#include <qvariant.h>
#include <qpixmap.h>
#include <qwidget.h>

class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;
class QSpacerItem;
class GameWidget;
class QLCDNumber;
class QPushButton;
class QLabel;

class BaseGameWindow : public QWidget
{
    Q_OBJECT

public:
    BaseGameWindow( QWidget* parent = 0, const char* name = 0, WFlags fl = 0 );
    ~BaseGameWindow();

    GameWidget* m_gameWidget;
    QLCDNumber* m_fpsLCD;
    QPushButton* m_gameButton;
    QLabel* m_focusLabel;
    QLabel* m_distanceLabel;
    QWidget* m_terrainGameStatusPanel;
    QLabel* m_terrainGameStatusLabel;
    QPushButton* m_clearRegionTerrainToolsButton;
    QLabel* m_buildoutRegionLabel;
    QLabel* m_positionLabel;

protected:
    QGridLayout* BaseGameWindowLayout;
    QSpacerItem* Spacer4;
    QSpacerItem* Spacer4_2;
    QSpacerItem* Spacer1;
    QSpacerItem* Spacer1_2;
    QHBoxLayout* layout3;
    QHBoxLayout* m_terrainGameStatusPanelLayout;
    QSpacerItem* m_terrainGameStatusSpacerLeft;
    QSpacerItem* m_terrainGameStatusSpacerRight;

protected slots:
    virtual void languageChange();

private:
    QPixmap image0;

};

#endif // BASEGAMEWINDOW_H
