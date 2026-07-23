/****************************************************************************
** Form implementation generated from reading ui file 'D:\titan\client\src\game\client\application\SwgGodClient\src\shared\ui\BaseGroupObjectWindow.ui'
**
** Created: Thu Jul 23 03:18:14 2026
**      by: The User Interface Compiler ($Id: qt/main.cpp   3.3.4   edited Nov 24 2003 $)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#include "D:\titan\client\src\build\win32\x64\Release\BaseGroupObjectWindow.h"

#include <qvariant.h>
#include <qpushbutton.h>
#include <qtabwidget.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qwhatsthis.h>
#include <qimage.h>
#include <qpixmap.h>

#include "BrushListView.h"
#include "PaletteListView.h"
/*
 *  Constructs a BaseGroupObjectWindow as a child of 'parent', with the
 *  name 'name' and widget flags set to 'f'.
 */
BaseGroupObjectWindow::BaseGroupObjectWindow( QWidget* parent, const char* name, WFlags fl )
    : QWidget( parent, name, fl )
{
    if ( !name )
	setName( "BaseGroupObjectWindow" );
    BaseGroupObjectWindowLayout = new QGridLayout( this, 1, 1, 4, 6, "BaseGroupObjectWindowLayout"); 

    m_groupObjectsTabs = new QTabWidget( this, "m_groupObjectsTabs" );
    m_groupObjectsTabs->setTabShape( QTabWidget::Rounded );

    Brushes = new QWidget( m_groupObjectsTabs, "Brushes" );
    BrushesLayout = new QGridLayout( Brushes, 1, 1, 4, 2, "BrushesLayout"); 

    m_brushesList = new BrushListView( Brushes, "m_brushesList" );
    m_brushesList->setAcceptDrops( FALSE );

    BrushesLayout->addWidget( m_brushesList, 0, 0 );
    m_groupObjectsTabs->insertTab( Brushes, QString::fromLatin1("") );

    Palettes = new QWidget( m_groupObjectsTabs, "Palettes" );
    PalettesLayout = new QGridLayout( Palettes, 1, 1, 4, 2, "PalettesLayout"); 

    m_palettesList = new PaletteListView( Palettes, "m_palettesList" );
    m_palettesList->setAcceptDrops( TRUE );

    PalettesLayout->addWidget( m_palettesList, 0, 0 );
    m_groupObjectsTabs->insertTab( Palettes, QString::fromLatin1("") );

    BaseGroupObjectWindowLayout->addWidget( m_groupObjectsTabs, 0, 0 );
    languageChange();
    resize( QSize(341, 309).expandedTo(minimumSizeHint()) );
    clearWState( WState_Polished );
}

/*
 *  Destroys the object and frees any allocated resources
 */
BaseGroupObjectWindow::~BaseGroupObjectWindow()
{
    // no need to delete child widgets, Qt does it all for us
}

/*
 *  Sets the strings of the subwidgets using the current
 *  language.
 */
void BaseGroupObjectWindow::languageChange()
{
    setCaption( tr( "Group Object Window" ) );
    m_groupObjectsTabs->changeTab( Brushes, tr( "Brushes" ) );
    m_groupObjectsTabs->changeTab( Palettes, tr( "Palettes" ) );
}

/****************************************************************************
** BaseGroupObjectWindow meta object code from reading C++ file 'BaseGroupObjectWindow.h'
**
** Created: Thu Jul 23 03:18:14 2026
**      by: The Qt MOC ($Id: $)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#undef QT_NO_COMPAT
#include "D:\titan\client\src\build\win32\x64\Release\BaseGroupObjectWindow.h"
#include <qmetaobject.h>
#include <qapplication.h>

#include <private/qucomextra_p.h>
#if !defined(Q_MOC_OUTPUT_REVISION) || (Q_MOC_OUTPUT_REVISION != 26)
#error "This file was generated using the moc from 3.3.4. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

const char *BaseGroupObjectWindow::className() const
{
    return "BaseGroupObjectWindow";
}

QMetaObject *BaseGroupObjectWindow::metaObj = 0;
static QMetaObjectCleanUp cleanUp_BaseGroupObjectWindow( "BaseGroupObjectWindow", &BaseGroupObjectWindow::staticMetaObject );

#ifndef QT_NO_TRANSLATION
QString BaseGroupObjectWindow::tr( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "BaseGroupObjectWindow", s, c, QApplication::DefaultCodec );
    else
	return QString::fromLatin1( s );
}
#ifndef QT_NO_TRANSLATION_UTF8
QString BaseGroupObjectWindow::trUtf8( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "BaseGroupObjectWindow", s, c, QApplication::UnicodeUTF8 );
    else
	return QString::fromUtf8( s );
}
#endif // QT_NO_TRANSLATION_UTF8

#endif // QT_NO_TRANSLATION

QMetaObject* BaseGroupObjectWindow::staticMetaObject()
{
    if ( metaObj )
	return metaObj;
    QMetaObject* parentObject = QWidget::staticMetaObject();
    static const QUMethod slot_0 = {"languageChange", 0, 0 };
    static const QMetaData slot_tbl[] = {
	{ "languageChange()", &slot_0, QMetaData::Protected }
    };
    metaObj = QMetaObject::new_metaobject(
	"BaseGroupObjectWindow", parentObject,
	slot_tbl, 1,
	0, 0,
#ifndef QT_NO_PROPERTIES
	0, 0,
	0, 0,
#endif // QT_NO_PROPERTIES
	0, 0 );
    cleanUp_BaseGroupObjectWindow.setMetaObject( metaObj );
    return metaObj;
}

void* BaseGroupObjectWindow::qt_cast( const char* clname )
{
    if ( !qstrcmp( clname, "BaseGroupObjectWindow" ) )
	return this;
    return QWidget::qt_cast( clname );
}

bool BaseGroupObjectWindow::qt_invoke( int _id, QUObject* _o )
{
    switch ( _id - staticMetaObject()->slotOffset() ) {
    case 0: languageChange(); break;
    default:
	return QWidget::qt_invoke( _id, _o );
    }
    return TRUE;
}

bool BaseGroupObjectWindow::qt_emit( int _id, QUObject* _o )
{
    return QWidget::qt_emit(_id,_o);
}
#ifndef QT_NO_PROPERTIES

bool BaseGroupObjectWindow::qt_property( int id, int f, QVariant* v)
{
    return QWidget::qt_property( id, f, v);
}

bool BaseGroupObjectWindow::qt_static_property( QObject* , int , int , QVariant* ){ return FALSE; }
#endif // QT_NO_PROPERTIES
