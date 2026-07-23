/****************************************************************************
** Form interface generated from reading ui file 'D:\titan\client\src\game\client\application\SwgGodClient\src\shared\ui\BaseSplashScreen.ui'
**
** Created: Thu Jul 23 03:18:15 2026
**      by: The User Interface Compiler ($Id: qt/main.cpp   3.3.4   edited Nov 24 2003 $)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#ifndef BASESPLASHSCREEN_H
#define BASESPLASHSCREEN_H

#include <qvariant.h>
#include <qdialog.h>

class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;
class QSpacerItem;
class QLabel;

class BaseSplashScreen : public QDialog
{
    Q_OBJECT

public:
    BaseSplashScreen( QWidget* parent = 0, const char* name = 0, bool modal = FALSE, WFlags fl = 0 );
    ~BaseSplashScreen();

    QLabel* m_splashPixmap;
    QLabel* TextLabel;

protected:

protected slots:
    virtual void languageChange();

};

#endif // BASESPLASHSCREEN_H
