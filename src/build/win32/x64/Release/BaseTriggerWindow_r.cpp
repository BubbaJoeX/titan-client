/****************************************************************************
** Form implementation generated from reading ui file 'D:\titan\client\src\game\client\application\SwgGodClient\src\shared\ui\BaseTriggerWindow.ui'
**
** Created: Thu Jul 23 03:18:16 2026
**      by: The User Interface Compiler ($Id: qt/main.cpp   3.3.4   edited Nov 24 2003 $)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#include "D:\titan\client\src\build\win32\x64\Release\BaseTriggerWindow.h"

#include <qvariant.h>
#include <qtable.h>
#include <qpushbutton.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qwhatsthis.h>
#include <qimage.h>
#include <qpixmap.h>

/*
 *  Constructs a BaseTriggerWindow as a child of 'parent', with the
 *  name 'name' and widget flags set to 'f'.
 *
 *  The dialog will by default be modeless, unless you set 'modal' to
 *  TRUE to construct a modal dialog.
 */
BaseTriggerWindow::BaseTriggerWindow( QWidget* parent, const char* name, bool modal, WFlags fl )
    : QDialog( parent, name, modal, fl )
{
    if ( !name )
	setName( "BaseTriggerWindow" );
    BaseTriggerWindowLayout = new QGridLayout( this, 1, 1, 11, 6, "BaseTriggerWindowLayout"); 

    m_triggerTable = new QTable( this, "m_triggerTable" );
    m_triggerTable->setNumCols( m_triggerTable->numCols() + 1 );
    m_triggerTable->horizontalHeader()->setLabel( m_triggerTable->numCols() - 1, tr( "Name" ) );
    m_triggerTable->setNumCols( m_triggerTable->numCols() + 1 );
    m_triggerTable->horizontalHeader()->setLabel( m_triggerTable->numCols() - 1, tr( "Radius" ) );
    m_triggerTable->setNumRows( 0 );
    m_triggerTable->setNumCols( 2 );
    m_triggerTable->setReadOnly( TRUE );
    m_triggerTable->setSorting( TRUE );
    m_triggerTable->setSelectionMode( QTable::SingleRow );
    m_triggerTable->setFocusStyle( QTable::SpreadSheet );

    BaseTriggerWindowLayout->addMultiCellWidget( m_triggerTable, 0, 0, 0, 1 );

    m_okButton = new QPushButton( this, "m_okButton" );

    BaseTriggerWindowLayout->addWidget( m_okButton, 1, 1 );

    m_cancelButton = new QPushButton( this, "m_cancelButton" );

    BaseTriggerWindowLayout->addWidget( m_cancelButton, 1, 0 );
    languageChange();
    resize( QSize(329, 271).expandedTo(minimumSizeHint()) );
    clearWState( WState_Polished );

    // signals and slots connections
    connect( m_okButton, SIGNAL( clicked() ), this, SLOT( accept() ) );
    connect( m_cancelButton, SIGNAL( clicked() ), this, SLOT( reject() ) );
}

/*
 *  Destroys the object and frees any allocated resources
 */
BaseTriggerWindow::~BaseTriggerWindow()
{
    // no need to delete child widgets, Qt does it all for us
}

/*
 *  Sets the strings of the subwidgets using the current
 *  language.
 */
void BaseTriggerWindow::languageChange()
{
    setCaption( tr( "Trigger Settings" ) );
    m_triggerTable->horizontalHeader()->setLabel( 0, tr( "Name" ) );
    m_triggerTable->horizontalHeader()->setLabel( 1, tr( "Radius" ) );
    m_okButton->setText( tr( "Ok" ) );
    m_cancelButton->setText( tr( "Cancel" ) );
}

/****************************************************************************
** BaseTriggerWindow meta object code from reading C++ file 'BaseTriggerWindow.h'
**
** Created: Thu Jul 23 03:18:16 2026
**      by: The Qt MOC ($Id: $)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#undef QT_NO_COMPAT
#include "D:\titan\client\src\build\win32\x64\Release\BaseTriggerWindow.h"
#include <qmetaobject.h>
#include <qapplication.h>

#include <private/qucomextra_p.h>
#if !defined(Q_MOC_OUTPUT_REVISION) || (Q_MOC_OUTPUT_REVISION != 26)
#error "This file was generated using the moc from 3.3.4. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

const char *BaseTriggerWindow::className() const
{
    return "BaseTriggerWindow";
}

QMetaObject *BaseTriggerWindow::metaObj = 0;
static QMetaObjectCleanUp cleanUp_BaseTriggerWindow( "BaseTriggerWindow", &BaseTriggerWindow::staticMetaObject );

#ifndef QT_NO_TRANSLATION
QString BaseTriggerWindow::tr( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "BaseTriggerWindow", s, c, QApplication::DefaultCodec );
    else
	return QString::fromLatin1( s );
}
#ifndef QT_NO_TRANSLATION_UTF8
QString BaseTriggerWindow::trUtf8( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "BaseTriggerWindow", s, c, QApplication::UnicodeUTF8 );
    else
	return QString::fromUtf8( s );
}
#endif // QT_NO_TRANSLATION_UTF8

#endif // QT_NO_TRANSLATION

QMetaObject* BaseTriggerWindow::staticMetaObject()
{
    if ( metaObj )
	return metaObj;
    QMetaObject* parentObject = QDialog::staticMetaObject();
    static const QUMethod slot_0 = {"languageChange", 0, 0 };
    static const QMetaData slot_tbl[] = {
	{ "languageChange()", &slot_0, QMetaData::Protected }
    };
    metaObj = QMetaObject::new_metaobject(
	"BaseTriggerWindow", parentObject,
	slot_tbl, 1,
	0, 0,
#ifndef QT_NO_PROPERTIES
	0, 0,
	0, 0,
#endif // QT_NO_PROPERTIES
	0, 0 );
    cleanUp_BaseTriggerWindow.setMetaObject( metaObj );
    return metaObj;
}

void* BaseTriggerWindow::qt_cast( const char* clname )
{
    if ( !qstrcmp( clname, "BaseTriggerWindow" ) )
	return this;
    return QDialog::qt_cast( clname );
}

bool BaseTriggerWindow::qt_invoke( int _id, QUObject* _o )
{
    switch ( _id - staticMetaObject()->slotOffset() ) {
    case 0: languageChange(); break;
    default:
	return QDialog::qt_invoke( _id, _o );
    }
    return TRUE;
}

bool BaseTriggerWindow::qt_emit( int _id, QUObject* _o )
{
    return QDialog::qt_emit(_id,_o);
}
#ifndef QT_NO_PROPERTIES

bool BaseTriggerWindow::qt_property( int id, int f, QVariant* v)
{
    return QDialog::qt_property( id, f, v);
}

bool BaseTriggerWindow::qt_static_property( QObject* , int , int , QVariant* ){ return FALSE; }
#endif // QT_NO_PROPERTIES
