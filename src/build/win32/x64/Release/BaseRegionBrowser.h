/****************************************************************************
** Form interface generated from reading ui file 'D:\titan\client\src\game\client\application\SwgGodClient\src\shared\ui\BaseRegionBrowser.ui'
**
** Created: Thu Jul 23 03:18:14 2026
**      by: The User Interface Compiler ($Id: qt/main.cpp   3.3.4   edited Nov 24 2003 $)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#ifndef BASEREGIONBROWSER_H
#define BASEREGIONBROWSER_H

#include <qvariant.h>
#include <qpixmap.h>
#include <qwidget.h>

class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;
class QSpacerItem;
class RegionRenderer;
class QLabel;
class QCheckBox;
class QListView;
class QListViewItem;

class BaseRegionBrowser : public QWidget
{
    Q_OBJECT

public:
    BaseRegionBrowser( QWidget* parent = 0, const char* name = 0, WFlags fl = 0 );
    ~BaseRegionBrowser();

    QLabel* TextLabel1;
    QCheckBox* m_pvpCheckBox;
    QCheckBox* m_municipalCheckBox;
    QCheckBox* m_buildableCheckBox;
    QCheckBox* m_geographicCheckBox;
    QCheckBox* m_difficultyCheckBox;
    QCheckBox* m_spawnableCheckBox;
    QCheckBox* m_missionCheckBox;
    QListView* m_regionTree;
    RegionRenderer* m_regionRenderer;

public slots:
    virtual void onMissionCheck(bool);
    virtual void onBuildableCheck(bool);
    virtual void onDifficultyCheck(bool);
    virtual void onGeographicCheck(bool);
    virtual void onMunicipalCheck(bool);
    virtual void onPvPCheck(bool);
    virtual void onSpawnableCheck(bool);

protected:
    QGridLayout* BaseRegionBrowserLayout;
    QHBoxLayout* Layout8;
    QSpacerItem* Spacer6;
    QHBoxLayout* LayoutMapRow;

protected slots:
    virtual void languageChange();

private:
    QPixmap image0;

};

#endif // BASEREGIONBROWSER_H
