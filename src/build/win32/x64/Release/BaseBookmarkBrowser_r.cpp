/****************************************************************************
** Form implementation generated from reading ui file 'D:\titan\client\src\game\client\application\SwgGodClient\src\shared\ui\BaseBookmarkBrowser.ui'
**
** Created: Thu Jul 23 03:18:11 2026
**      by: The User Interface Compiler ($Id: qt/main.cpp   3.3.4   edited Nov 24 2003 $)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#include "D:\titan\client\src\build\win32\x64\Release\BaseBookmarkBrowser.h"

#include <qvariant.h>
#include <qheader.h>
#include <qlistview.h>
#include <qtoolbutton.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qwhatsthis.h>
#include <qimage.h>
#include <qpixmap.h>

/*
 *  Constructs a BaseBookmarkBrowser as a child of 'parent', with the
 *  name 'name' and widget flags set to 'f'.
 */
BaseBookmarkBrowser::BaseBookmarkBrowser( QWidget* parent, const char* name, WFlags fl )
    : QWidget( parent, name, fl )
{
    if ( !name )
	setName( "BaseBookmarkBrowser" );
    BaseBookmarkBrowserLayout = new QGridLayout( this, 1, 1, 4, 2, "BaseBookmarkBrowserLayout"); 

    m_bookmarkList = new QListView( this, "m_bookmarkList" );
    m_bookmarkList->addColumn( tr( "Bookmark" ) );
    m_bookmarkList->header()->setLabel( m_bookmarkList->header()->count() - 1, QPixmap::fromMimeSource( "hi16_action_bookmark_toolbar" ), tr( "Bookmark" ) );
    m_bookmarkList->setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)7, (QSizePolicy::SizeType)7, 0, 0, m_bookmarkList->sizePolicy().hasHeightForWidth() ) );
    m_bookmarkList->setAllColumnsShowFocus( TRUE );
    m_bookmarkList->setShowSortIndicator( TRUE );
    m_bookmarkList->setRootIsDecorated( TRUE );
    m_bookmarkList->setResizeMode( QListView::AllColumns );
    m_bookmarkList->setDefaultRenameAction( QListView::Reject );

    BaseBookmarkBrowserLayout->addMultiCellWidget( m_bookmarkList, 1, 1, 0, 1 );

    m_deleteButton = new QToolButton( this, "m_deleteButton" );
    m_deleteButton->setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)1, (QSizePolicy::SizeType)1, 0, 0, m_deleteButton->sizePolicy().hasHeightForWidth() ) );

    BaseBookmarkBrowserLayout->addWidget( m_deleteButton, 0, 0 );
    Spacer1 = new QSpacerItem( 20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum );
    BaseBookmarkBrowserLayout->addItem( Spacer1, 0, 1 );
    languageChange();
    resize( QSize(429, 332).expandedTo(minimumSizeHint()) );
    clearWState( WState_Polished );
}

/*
 *  Destroys the object and frees any allocated resources
 */
BaseBookmarkBrowser::~BaseBookmarkBrowser()
{
    // no need to delete child widgets, Qt does it all for us
}

/*
 *  Sets the strings of the subwidgets using the current
 *  language.
 */
void BaseBookmarkBrowser::languageChange()
{
    setCaption( tr( "Bookmark Browser" ) );
    m_bookmarkList->header()->setLabel( 0, tr( "Bookmark" ) );
    m_bookmarkList->clear();
    QListViewItem * item = new QListViewItem( m_bookmarkList, 0 );
    item->setText( 0, tr( "Item" ) );
    item->setPixmap( 0, QPixmap::fromMimeSource( "hi16_action_bookmark" ) );

    m_deleteButton->setText( tr( "Delete" ) );
    QWhatsThis::add( m_deleteButton, tr( "Delete a camera bookmark." ) );
}

/****************************************************************************
** BaseBookmarkBrowser meta object code from reading C++ file 'BaseBookmarkBrowser.h'
**
** Created: Thu Jul 23 03:18:11 2026
**      by: The Qt MOC ($Id: $)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#undef QT_NO_COMPAT
#include "D:\titan\client\src\build\win32\x64\Release\BaseBookmarkBrowser.h"
#include <qmetaobject.h>
#include <qapplication.h>

#include <private/qucomextra_p.h>
#if !defined(Q_MOC_OUTPUT_REVISION) || (Q_MOC_OUTPUT_REVISION != 26)
#error "This file was generated using the moc from 3.3.4. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

const char *BaseBookmarkBrowser::className() const
{
    return "BaseBookmarkBrowser";
}

QMetaObject *BaseBookmarkBrowser::metaObj = 0;
static QMetaObjectCleanUp cleanUp_BaseBookmarkBrowser( "BaseBookmarkBrowser", &BaseBookmarkBrowser::staticMetaObject );

#ifndef QT_NO_TRANSLATION
QString BaseBookmarkBrowser::tr( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "BaseBookmarkBrowser", s, c, QApplication::DefaultCodec );
    else
	return QString::fromLatin1( s );
}
#ifndef QT_NO_TRANSLATION_UTF8
QString BaseBookmarkBrowser::trUtf8( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "BaseBookmarkBrowser", s, c, QApplication::UnicodeUTF8 );
    else
	return QString::fromUtf8( s );
}
#endif // QT_NO_TRANSLATION_UTF8

#endif // QT_NO_TRANSLATION

QMetaObject* BaseBookmarkBrowser::staticMetaObject()
{
    if ( metaObj )
	return metaObj;
    QMetaObject* parentObject = QWidget::staticMetaObject();
    static const QUMethod slot_0 = {"languageChange", 0, 0 };
    static const QMetaData slot_tbl[] = {
	{ "languageChange()", &slot_0, QMetaData::Protected }
    };
    metaObj = QMetaObject::new_metaobject(
	"BaseBookmarkBrowser", parentObject,
	slot_tbl, 1,
	0, 0,
#ifndef QT_NO_PROPERTIES
	0, 0,
	0, 0,
#endif // QT_NO_PROPERTIES
	0, 0 );
    cleanUp_BaseBookmarkBrowser.setMetaObject( metaObj );
    return metaObj;
}

void* BaseBookmarkBrowser::qt_cast( const char* clname )
{
    if ( !qstrcmp( clname, "BaseBookmarkBrowser" ) )
	return this;
    return QWidget::qt_cast( clname );
}

bool BaseBookmarkBrowser::qt_invoke( int _id, QUObject* _o )
{
    switch ( _id - staticMetaObject()->slotOffset() ) {
    case 0: languageChange(); break;
    default:
	return QWidget::qt_invoke( _id, _o );
    }
    return TRUE;
}

bool BaseBookmarkBrowser::qt_emit( int _id, QUObject* _o )
{
    return QWidget::qt_emit(_id,_o);
}
#ifndef QT_NO_PROPERTIES

bool BaseBookmarkBrowser::qt_property( int id, int f, QVariant* v)
{
    return QWidget::qt_property( id, f, v);
}

bool BaseBookmarkBrowser::qt_static_property( QObject* , int , int , QVariant* ){ return FALSE; }
#endif // QT_NO_PROPERTIES
