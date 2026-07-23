/****************************************************************************
** Form interface generated from reading ui file 'D:\titan\client\src\game\client\application\SwgGodClient\src\shared\ui\BaseSnapToGridSettings.ui'
**
** Created: Thu Jul 23 03:18:15 2026
**      by: The User Interface Compiler ($Id: qt/main.cpp   3.3.4   edited Nov 24 2003 $)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#ifndef BASESNAPTOGRIDSETTINGS_H
#define BASESNAPTOGRIDSETTINGS_H

#include <qvariant.h>
#include <qdialog.h>

class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;
class QSpacerItem;
class QPushButton;
class QGroupBox;
class QCheckBox;
class QSpinBox;
class QSlider;
class QLabel;
class QLineEdit;

class BaseSnapToGridSettings : public QDialog
{
    Q_OBJECT

public:
    BaseSnapToGridSettings( QWidget* parent = 0, const char* name = 0, bool modal = FALSE, WFlags fl = 0 );
    ~BaseSnapToGridSettings();

    QPushButton* m_cancelButton;
    QPushButton* m_okButton;
    QGroupBox* Horizontal;
    QCheckBox* m_snapToHorizontalGrid;
    QSpinBox* m_horizontalSensitivitySpinBox;
    QSlider* m_horizontalSensitivitySlider;
    QLabel* TextLabel1;
    QLabel* TextLabel2;
    QLineEdit* m_horizontalGridSize;
    QLineEdit* m_horizontalGridSegments;
    QLabel* TextLabel3;
    QGroupBox* Vertical;
    QCheckBox* m_snapToVerticalGrid;
    QLabel* TextLabel6;
    QSlider* m_verticalSensitivitySlider;
    QSpinBox* m_verticalSensitivitySpinBox;
    QLineEdit* m_verticalGridSegments;
    QLineEdit* m_verticalGridSize;
    QLabel* TextLabel5;
    QLabel* TextLabel4;

protected:
    QGridLayout* BaseSnapToGridSettingsLayout;
    QGridLayout* HorizontalLayout;
    QGridLayout* VerticalLayout;

protected slots:
    virtual void languageChange();

};

#endif // BASESNAPTOGRIDSETTINGS_H
