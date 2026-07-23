/****************************************************************************
** Form implementation generated from reading ui file 'D:\titan\client\src\game\client\application\SwgGodClient\src\shared\ui\BaseFavoritesWindow.ui'
**
** Created: Thu Jul 23 03:18:12 2026
**      by: The User Interface Compiler ($Id: qt/main.cpp   3.3.4   edited Nov 24 2003 $)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#include "D:\titan\client\src\build\win32\x64\Release\BaseFavoritesWindow.h"

#include <qvariant.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qwhatsthis.h>
#include <qimage.h>
#include <qpixmap.h>

#include "FavoritesListView.h"
/*
 *  Constructs a BaseFavoritesWindow as a child of 'parent', with the
 *  name 'name' and widget flags set to 'f'.
 */
BaseFavoritesWindow::BaseFavoritesWindow( QWidget* parent, const char* name, WFlags fl )
    : QWidget( parent, name, fl )
{
    if ( !name )
	setName( "BaseFavoritesWindow" );
    setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)5, (QSizePolicy::SizeType)5, 0, 0, sizePolicy().hasHeightForWidth() ) );
    setAcceptDrops( TRUE );
    BaseFavoritesWindowLayout = new QHBoxLayout( this, 11, 6, "BaseFavoritesWindowLayout"); 

    m_favoritesList = new FavoritesListView( this, "m_favoritesList" );
    m_favoritesList->setAcceptDrops( TRUE );
    BaseFavoritesWindowLayout->addWidget( m_favoritesList );
    languageChange();
    resize( QSize(281, 250).expandedTo(minimumSizeHint()) );
    clearWState( WState_Polished );
}

/*
 *  Destroys the object and frees any allocated resources
 */
BaseFavoritesWindow::~BaseFavoritesWindow()
{
    // no need to delete child widgets, Qt does it all for us
}

/*
 *  Sets the strings of the subwidgets using the current
 *  language.
 */
void BaseFavoritesWindow::languageChange()
{
    setCaption( tr( "Favorites" ) );
}

/****************************************************************************
** BaseFavoritesWindow meta object code from reading C++ file 'BaseFavoritesWindow.h'
**
** Created: Thu Jul 23 03:18:12 2026
**      by: The Qt MOC ($Id: $)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#undef QT_NO_COMPAT
#include "D:\titan\client\src\build\win32\x64\Release\BaseFavoritesWindow.h"
#include <qmetaobject.h>
#include <qapplication.h>

#include <private/qucomextra_p.h>
#if !defined(Q_MOC_OUTPUT_REVISION) || (Q_MOC_OUTPUT_REVISION != 26)
#error "This file was generated using the moc from 3.3.4. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

const char *BaseFavoritesWindow::className() const
{
    return "BaseFavoritesWindow";
}

QMetaObject *BaseFavoritesWindow::metaObj = 0;
static QMetaObjectCleanUp cleanUp_BaseFavoritesWindow( "BaseFavoritesWindow", &BaseFavoritesWindow::staticMetaObject );

#ifndef QT_NO_TRANSLATION
QString BaseFavoritesWindow::tr( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "BaseFavoritesWindow", s, c, QApplication::DefaultCodec );
    else
	return QString::fromLatin1( s );
}
#ifndef QT_NO_TRANSLATION_UTF8
QString BaseFavoritesWindow::trUtf8( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "BaseFavoritesWindow", s, c, QApplication::UnicodeUTF8 );
    else
	return QString::fromUtf8( s );
}
#endif // QT_NO_TRANSLATION_UTF8

#endif // QT_NO_TRANSLATION

QMetaObject* BaseFavoritesWindow::staticMetaObject()
{
    if ( metaObj )
	return metaObj;
    QMetaObject* parentObject = QWidget::staticMetaObject();
    static const QUMethod slot_0 = {"languageChange", 0, 0 };
    static const QMetaData slot_tbl[] = {
	{ "languageChange()", &slot_0, QMetaData::Protected }
    };
    metaObj = QMetaObject::new_metaobject(
	"BaseFavoritesWindow", parentObject,
	slot_tbl, 1,
	0, 0,
#ifndef QT_NO_PROPERTIES
	0, 0,
	0, 0,
#endif // QT_NO_PROPERTIES
	0, 0 );
    cleanUp_BaseFavoritesWindow.setMetaObject( metaObj );
    return metaObj;
}

void* BaseFavoritesWindow::qt_cast( const char* clname )
{
    if ( !qstrcmp( clname, "BaseFavoritesWindow" ) )
	return this;
    return QWidget::qt_cast( clname );
}

bool BaseFavoritesWindow::qt_invoke( int _id, QUObject* _o )
{
    switch ( _id - staticMetaObject()->slotOffset() ) {
    case 0: languageChange(); break;
    default:
	return QWidget::qt_invoke( _id, _o );
    }
    return TRUE;
}

bool BaseFavoritesWindow::qt_emit( int _id, QUObject* _o )
{
    return QWidget::qt_emit(_id,_o);
}
#ifndef QT_NO_PROPERTIES

bool BaseFavoritesWindow::qt_property( int id, int f, QVariant* v)
{
    return QWidget::qt_property( id, f, v);
}

bool BaseFavoritesWindow::qt_static_property( QObject* , int , int , QVariant* ){ return FALSE; }
#endif // QT_NO_PROPERTIES
