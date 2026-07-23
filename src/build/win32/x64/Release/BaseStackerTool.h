/****************************************************************************
** Form interface generated from reading ui file 'D:\titan\client\src\game\client\application\SwgGodClient\src\shared\ui\BaseStackerTool.ui'
**
** Created: Thu Jul 23 03:18:10 2026
**      by: The User Interface Compiler ($Id: qt/main.cpp   3.3.4   edited Nov 24 2003 $)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#ifndef BASESTACKERTOOL_H
#define BASESTACKERTOOL_H

#include <qvariant.h>
#include <qwidget.h>

class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;
class QSpacerItem;
class QLabel;
class QSpinBox;
class QComboBox;
class QLineEdit;
class QCheckBox;
class QGroupBox;
class QPushButton;

class BaseStackerTool : public QWidget
{
    Q_OBJECT

public:
    BaseStackerTool( QWidget* parent = 0, const char* name = 0, WFlags fl = 0 );
    ~BaseStackerTool();

    QLabel* m_countLabel;
    QSpinBox* m_countSpinBox;
    QLabel* m_orientationLabel;
    QComboBox* m_orientationCombo;
    QLabel* m_distanceLabel;
    QLineEdit* m_distanceEdit;
    QCheckBox* m_useMeshExtentCheck;
    QGroupBox* m_gimbalGroup;
    QLabel* m_gimbalLegend;
    QPushButton* m_stackButton;

protected:
    QGridLayout* BaseStackerToolLayout;
    QGridLayout* m_gimbalGroupLayout;

protected slots:
    virtual void languageChange();

};

#endif // BASESTACKERTOOL_H
