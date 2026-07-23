/****************************************************************************
** Form implementation generated from reading ui file 'D:\titan\client\src\game\client\application\SwgGodClient\src\shared\ui\BaseFormWindow.ui'
**
** Created: Thu Jul 23 03:18:13 2026
**      by: The User Interface Compiler ($Id: qt/main.cpp   3.3.4   edited Nov 24 2003 $)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#include "D:\titan\client\src\build\win32\x64\Release\BaseFormWindow.h"

#include <qvariant.h>
#include <qpushbutton.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qwhatsthis.h>
#include <qimage.h>
#include <qpixmap.h>

#include "../../../../game/client/application/SwgGodClient/src/shared/ui/BaseFormWindow.ui.h"
/*
 *  Constructs a BaseFormWindow as a child of 'parent', with the
 *  name 'name' and widget flags set to 'f'.
 *
 *  The dialog will by default be modeless, unless you set 'modal' to
 *  TRUE to construct a modal dialog.
 */
BaseFormWindow::BaseFormWindow( QWidget* parent, const char* name, bool modal, WFlags fl )
    : QDialog( parent, name, modal, fl )
{
    if ( !name )
	setName( "BaseFormWindow" );
    BaseFormWindowLayout = new QGridLayout( this, 1, 1, 11, 6, "BaseFormWindowLayout"); 

    m_layoutBottom = new QHBoxLayout( 0, 0, 6, "m_layoutBottom"); 
    m_bottomSpacer = new QSpacerItem( 391, 0, QSizePolicy::Expanding, QSizePolicy::Minimum );
    m_layoutBottom->addItem( m_bottomSpacer );

    m_cancelButton = new QPushButton( this, "m_cancelButton" );
    m_cancelButton->setAutoDefault( FALSE );
    m_layoutBottom->addWidget( m_cancelButton );

    m_okButton = new QPushButton( this, "m_okButton" );
    m_layoutBottom->addWidget( m_okButton );

    BaseFormWindowLayout->addLayout( m_layoutBottom, 2, 0 );
    m_middleSpacer = new QSpacerItem( 0, 81, QSizePolicy::Minimum, QSizePolicy::Expanding );
    BaseFormWindowLayout->addItem( m_middleSpacer, 1, 0 );
    languageChange();
    resize( QSize(371, 403).expandedTo(minimumSizeHint()) );
    clearWState( WState_Polished );

    // signals and slots connections
    connect( m_okButton, SIGNAL( clicked() ), this, SLOT( onOkPressed() ) );
    connect( m_cancelButton, SIGNAL( clicked() ), this, SLOT( onCancelPressed() ) );

    // tab order
    setTabOrder( m_okButton, m_cancelButton );
}

/*
 *  Destroys the object and frees any allocated resources
 */
BaseFormWindow::~BaseFormWindow()
{
    // no need to delete child widgets, Qt does it all for us
}

/*
 *  Sets the strings of the subwidgets using the current
 *  language.
 */
void BaseFormWindow::languageChange()
{
    setCaption( tr( "Form Window" ) );
    m_cancelButton->setText( tr( "Cancel" ) );
    m_okButton->setText( tr( "Ok" ) );
}

/****************************************************************************
** BaseFormWindow meta object code from reading C++ file 'BaseFormWindow.h'
**
** Created: Thu Jul 23 03:18:13 2026
**      by: The Qt MOC ($Id: $)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#undef QT_NO_COMPAT
#include "D:\titan\client\src\build\win32\x64\Release\BaseFormWindow.h"
#include <qmetaobject.h>
#include <qapplication.h>

#include <private/qucomextra_p.h>
#if !defined(Q_MOC_OUTPUT_REVISION) || (Q_MOC_OUTPUT_REVISION != 26)
#error "This file was generated using the moc from 3.3.4. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

const char *BaseFormWindow::className() const
{
    return "BaseFormWindow";
}

QMetaObject *BaseFormWindow::metaObj = 0;
static QMetaObjectCleanUp cleanUp_BaseFormWindow( "BaseFormWindow", &BaseFormWindow::staticMetaObject );

#ifndef QT_NO_TRANSLATION
QString BaseFormWindow::tr( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "BaseFormWindow", s, c, QApplication::DefaultCodec );
    else
	return QString::fromLatin1( s );
}
#ifndef QT_NO_TRANSLATION_UTF8
QString BaseFormWindow::trUtf8( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "BaseFormWindow", s, c, QApplication::UnicodeUTF8 );
    else
	return QString::fromUtf8( s );
}
#endif // QT_NO_TRANSLATION_UTF8

#endif // QT_NO_TRANSLATION

QMetaObject* BaseFormWindow::staticMetaObject()
{
    if ( metaObj )
	return metaObj;
    QMetaObject* parentObject = QDialog::staticMetaObject();
    static const QUMethod slot_0 = {"onOkPressed", 0, 0 };
    static const QUMethod slot_1 = {"onCancelPressed", 0, 0 };
    static const QUMethod slot_2 = {"languageChange", 0, 0 };
    static const QMetaData slot_tbl[] = {
	{ "onOkPressed()", &slot_0, QMetaData::Public },
	{ "onCancelPressed()", &slot_1, QMetaData::Public },
	{ "languageChange()", &slot_2, QMetaData::Protected }
    };
    metaObj = QMetaObject::new_metaobject(
	"BaseFormWindow", parentObject,
	slot_tbl, 3,
	0, 0,
#ifndef QT_NO_PROPERTIES
	0, 0,
	0, 0,
#endif // QT_NO_PROPERTIES
	0, 0 );
    cleanUp_BaseFormWindow.setMetaObject( metaObj );
    return metaObj;
}

void* BaseFormWindow::qt_cast( const char* clname )
{
    if ( !qstrcmp( clname, "BaseFormWindow" ) )
	return this;
    return QDialog::qt_cast( clname );
}

bool BaseFormWindow::qt_invoke( int _id, QUObject* _o )
{
    switch ( _id - staticMetaObject()->slotOffset() ) {
    case 0: onOkPressed(); break;
    case 1: onCancelPressed(); break;
    case 2: languageChange(); break;
    default:
	return QDialog::qt_invoke( _id, _o );
    }
    return TRUE;
}

bool BaseFormWindow::qt_emit( int _id, QUObject* _o )
{
    return QDialog::qt_emit(_id,_o);
}
#ifndef QT_NO_PROPERTIES

bool BaseFormWindow::qt_property( int id, int f, QVariant* v)
{
    return QDialog::qt_property( id, f, v);
}

bool BaseFormWindow::qt_static_property( QObject* , int , int , QVariant* ){ return FALSE; }
#endif // QT_NO_PROPERTIES
