/****************************************************************************
** Form interface generated from reading ui file 'D:\titan\client\src\game\client\application\SwgGodClient\src\shared\ui\BaseObjectTransformWindow.ui'
**
** Created: Thu Jul 23 03:18:14 2026
**      by: The User Interface Compiler ($Id: qt/main.cpp   3.3.4   edited Nov 24 2003 $)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#ifndef BASEOBJECTTRANSFORMWINDOW_H
#define BASEOBJECTTRANSFORMWINDOW_H

#include <qvariant.h>
#include <qdialog.h>

class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;
class QSpacerItem;
class QGroupBox;
class QLabel;
class QLineEdit;
class QSpinBox;
class QDial;
class QFrame;
class QPushButton;

class BaseObjectTransformWindow : public QDialog
{
    Q_OBJECT

public:
    BaseObjectTransformWindow( QWidget* parent = 0, const char* name = 0, bool modal = FALSE, WFlags fl = 0 );
    ~BaseObjectTransformWindow();

    QGroupBox* GroupBoxTranslations;
    QLabel* TextLabelXMeters;
    QLineEdit* m_LineEditX;
    QLabel* TextLabelYMeters;
    QLineEdit* m_LineEditY;
    QLabel* TextLabelY;
    QLabel* TextLabelZ;
    QLineEdit* m_LineEditZ;
    QLabel* TextLabelZMeters;
    QLabel* TextLabelX;
    QGroupBox* GroupBoxRotations;
    QLabel* YawText;
    QSpinBox* m_yawEdit;
    QDial* m_yawDial;
    QFrame* Line1;
    QLabel* PitchText;
    QSpinBox* m_pitchEdit;
    QDial* m_pitchDial;
    QFrame* Line1_2;
    QLabel* RollText;
    QSpinBox* m_rollEdit;
    QDial* m_rollDial;
    QPushButton* m_okButton;
    QPushButton* m_cancelButton;

protected:
    QGridLayout* BaseObjectTransformWindowLayout;
    QSpacerItem* SpacerButtons;
    QGridLayout* GroupBoxTranslationsLayout;
    QSpacerItem* SpacerXText;
    QSpacerItem* SpacerYText;
    QSpacerItem* SpacerZText;
    QGridLayout* GroupBoxRotationsLayout;
    QHBoxLayout* LayoutAllRotation;
    QVBoxLayout* LayoutAllYaw;
    QHBoxLayout* Layout2;
    QVBoxLayout* LayoutAllPitch;
    QHBoxLayout* Layout2_2;
    QVBoxLayout* LayoutAllRoll;
    QHBoxLayout* Layout2_3;

protected slots:
    virtual void languageChange();

};

#endif // BASEOBJECTTRANSFORMWINDOW_H
