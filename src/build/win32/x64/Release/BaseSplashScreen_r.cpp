/****************************************************************************
** Form implementation generated from reading ui file 'D:\titan\client\src\game\client\application\SwgGodClient\src\shared\ui\BaseSplashScreen.ui'
**
** Created: Thu Jul 23 03:18:15 2026
**      by: The User Interface Compiler ($Id: qt/main.cpp   3.3.4   edited Nov 24 2003 $)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#include "D:\titan\client\src\build\win32\x64\Release\BaseSplashScreen.h"

#include <qvariant.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qwhatsthis.h>
#include <qimage.h>
#include <qpixmap.h>

/*
 *  Constructs a BaseSplashScreen as a child of 'parent', with the
 *  name 'name' and widget flags set to 'f'.
 *
 *  The dialog will by default be modeless, unless you set 'modal' to
 *  TRUE to construct a modal dialog.
 */
BaseSplashScreen::BaseSplashScreen( QWidget* parent, const char* name, bool modal, WFlags fl )
    : QDialog( parent, name, modal, fl )
{
    if ( !name )
	setName( "BaseSplashScreen" );
    setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)0, (QSizePolicy::SizeType)0, 0, 0, sizePolicy().hasHeightForWidth() ) );
    setMinimumSize( QSize( 400, 240 ) );

    m_splashPixmap = new QLabel( this, "m_splashPixmap" );
    m_splashPixmap->setGeometry( QRect( -1, -1, 399, 199 ) );
    m_splashPixmap->setPaletteBackgroundPixmap( QPixmap::fromMimeSource( "splash" ) );
    m_splashPixmap->setScaledContents( TRUE );

    TextLabel = new QLabel( this, "TextLabel" );
    TextLabel->setGeometry( QRect( 86, 210, 237, 18 ) );
    languageChange();
    resize( QSize(400, 240).expandedTo(minimumSizeHint()) );
    clearWState( WState_Polished );
}

/*
 *  Destroys the object and frees any allocated resources
 */
BaseSplashScreen::~BaseSplashScreen()
{
    // no need to delete child widgets, Qt does it all for us
}

/*
 *  Sets the strings of the subwidgets using the current
 *  language.
 */
void BaseSplashScreen::languageChange()
{
    setCaption( tr( "GodClient Splash Screen" ) );
    TextLabel->setText( tr( "Starting SWG Editor" ) );
}

/****************************************************************************
** BaseSplashScreen meta object code from reading C++ file 'BaseSplashScreen.h'
**
** Created: Thu Jul 23 03:18:15 2026
**      by: The Qt MOC ($Id: $)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#undef QT_NO_COMPAT
#include "D:\titan\client\src\build\win32\x64\Release\BaseSplashScreen.h"
#include <qmetaobject.h>
#include <qapplication.h>

#include <private/qucomextra_p.h>
#if !defined(Q_MOC_OUTPUT_REVISION) || (Q_MOC_OUTPUT_REVISION != 26)
#error "This file was generated using the moc from 3.3.4. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

const char *BaseSplashScreen::className() const
{
    return "BaseSplashScreen";
}

QMetaObject *BaseSplashScreen::metaObj = 0;
static QMetaObjectCleanUp cleanUp_BaseSplashScreen( "BaseSplashScreen", &BaseSplashScreen::staticMetaObject );

#ifndef QT_NO_TRANSLATION
QString BaseSplashScreen::tr( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "BaseSplashScreen", s, c, QApplication::DefaultCodec );
    else
	return QString::fromLatin1( s );
}
#ifndef QT_NO_TRANSLATION_UTF8
QString BaseSplashScreen::trUtf8( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "BaseSplashScreen", s, c, QApplication::UnicodeUTF8 );
    else
	return QString::fromUtf8( s );
}
#endif // QT_NO_TRANSLATION_UTF8

#endif // QT_NO_TRANSLATION

QMetaObject* BaseSplashScreen::staticMetaObject()
{
    if ( metaObj )
	return metaObj;
    QMetaObject* parentObject = QDialog::staticMetaObject();
    static const QUMethod slot_0 = {"languageChange", 0, 0 };
    static const QMetaData slot_tbl[] = {
	{ "languageChange()", &slot_0, QMetaData::Protected }
    };
    metaObj = QMetaObject::new_metaobject(
	"BaseSplashScreen", parentObject,
	slot_tbl, 1,
	0, 0,
#ifndef QT_NO_PROPERTIES
	0, 0,
	0, 0,
#endif // QT_NO_PROPERTIES
	0, 0 );
    cleanUp_BaseSplashScreen.setMetaObject( metaObj );
    return metaObj;
}

void* BaseSplashScreen::qt_cast( const char* clname )
{
    if ( !qstrcmp( clname, "BaseSplashScreen" ) )
	return this;
    return QDialog::qt_cast( clname );
}

bool BaseSplashScreen::qt_invoke( int _id, QUObject* _o )
{
    switch ( _id - staticMetaObject()->slotOffset() ) {
    case 0: languageChange(); break;
    default:
	return QDialog::qt_invoke( _id, _o );
    }
    return TRUE;
}

bool BaseSplashScreen::qt_emit( int _id, QUObject* _o )
{
    return QDialog::qt_emit(_id,_o);
}
#ifndef QT_NO_PROPERTIES

bool BaseSplashScreen::qt_property( int id, int f, QVariant* v)
{
    return QDialog::qt_property( id, f, v);
}

bool BaseSplashScreen::qt_static_property( QObject* , int , int , QVariant* ){ return FALSE; }
#endif // QT_NO_PROPERTIES
