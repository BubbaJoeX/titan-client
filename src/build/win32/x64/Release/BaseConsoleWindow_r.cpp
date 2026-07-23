/****************************************************************************
** Form implementation generated from reading ui file 'D:\titan\client\src\game\client\application\SwgGodClient\src\shared\ui\BaseConsoleWindow.ui'
**
** Created: Thu Jul 23 03:18:12 2026
**      by: The User Interface Compiler ($Id: qt/main.cpp   3.3.4   edited Nov 24 2003 $)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#include "D:\titan\client\src\build\win32\x64\Release\BaseConsoleWindow.h"

#include <qvariant.h>
#include <qtextview.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qwhatsthis.h>
#include <qimage.h>
#include <qpixmap.h>

/*
 *  Constructs a BaseConsoleWindow as a child of 'parent', with the
 *  name 'name' and widget flags set to 'f'.
 */
BaseConsoleWindow::BaseConsoleWindow( QWidget* parent, const char* name, WFlags fl )
    : QWidget( parent, name, fl )
{
    if ( !name )
	setName( "BaseConsoleWindow" );
    setIcon( QPixmap::fromMimeSource( "hi16_action_console" ) );
    BaseConsoleWindowLayout = new QGridLayout( this, 1, 1, 4, 2, "BaseConsoleWindowLayout"); 

    m_textView = new QTextView( this, "m_textView" );
    m_textView->setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)7, (QSizePolicy::SizeType)7, 0, 0, m_textView->sizePolicy().hasHeightForWidth() ) );
    QFont m_textView_font(  m_textView->font() );
    m_textView_font.setFamily( "Lucida Console" );
    m_textView->setFont( m_textView_font ); 
    m_textView->setLineWidth( 2 );
    m_textView->setMargin( 0 );
    m_textView->setResizePolicy( QTextView::Manual );
    m_textView->setTextFormat( QTextView::PlainText );

    BaseConsoleWindowLayout->addWidget( m_textView, 0, 0 );
    languageChange();
    resize( QSize(997, 112).expandedTo(minimumSizeHint()) );
    clearWState( WState_Polished );

    // signals and slots connections
    init();
}

/*
 *  Destroys the object and frees any allocated resources
 */
BaseConsoleWindow::~BaseConsoleWindow()
{
    destroy();
    // no need to delete child widgets, Qt does it all for us
}

/*
 *  Sets the strings of the subwidgets using the current
 *  language.
 */
void BaseConsoleWindow::languageChange()
{
    setCaption( tr( "Console Window" ) );
    m_textView->setText( tr( "Welcome to the SwgGodClient" ) );
}

void BaseConsoleWindow::init()
{
}

void BaseConsoleWindow::destroy()
{
}

/****************************************************************************
** BaseConsoleWindow meta object code from reading C++ file 'BaseConsoleWindow.h'
**
** Created: Thu Jul 23 03:18:12 2026
**      by: The Qt MOC ($Id: $)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#undef QT_NO_COMPAT
#include "D:\titan\client\src\build\win32\x64\Release\BaseConsoleWindow.h"
#include <qmetaobject.h>
#include <qapplication.h>

#include <private/qucomextra_p.h>
#if !defined(Q_MOC_OUTPUT_REVISION) || (Q_MOC_OUTPUT_REVISION != 26)
#error "This file was generated using the moc from 3.3.4. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

const char *BaseConsoleWindow::className() const
{
    return "BaseConsoleWindow";
}

QMetaObject *BaseConsoleWindow::metaObj = 0;
static QMetaObjectCleanUp cleanUp_BaseConsoleWindow( "BaseConsoleWindow", &BaseConsoleWindow::staticMetaObject );

#ifndef QT_NO_TRANSLATION
QString BaseConsoleWindow::tr( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "BaseConsoleWindow", s, c, QApplication::DefaultCodec );
    else
	return QString::fromLatin1( s );
}
#ifndef QT_NO_TRANSLATION_UTF8
QString BaseConsoleWindow::trUtf8( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "BaseConsoleWindow", s, c, QApplication::UnicodeUTF8 );
    else
	return QString::fromUtf8( s );
}
#endif // QT_NO_TRANSLATION_UTF8

#endif // QT_NO_TRANSLATION

QMetaObject* BaseConsoleWindow::staticMetaObject()
{
    if ( metaObj )
	return metaObj;
    QMetaObject* parentObject = QWidget::staticMetaObject();
    static const QUMethod slot_0 = {"languageChange", 0, 0 };
    static const QUMethod slot_1 = {"init", 0, 0 };
    static const QUMethod slot_2 = {"destroy", 0, 0 };
    static const QMetaData slot_tbl[] = {
	{ "languageChange()", &slot_0, QMetaData::Protected },
	{ "init()", &slot_1, QMetaData::Protected },
	{ "destroy()", &slot_2, QMetaData::Protected }
    };
    metaObj = QMetaObject::new_metaobject(
	"BaseConsoleWindow", parentObject,
	slot_tbl, 3,
	0, 0,
#ifndef QT_NO_PROPERTIES
	0, 0,
	0, 0,
#endif // QT_NO_PROPERTIES
	0, 0 );
    cleanUp_BaseConsoleWindow.setMetaObject( metaObj );
    return metaObj;
}

void* BaseConsoleWindow::qt_cast( const char* clname )
{
    if ( !qstrcmp( clname, "BaseConsoleWindow" ) )
	return this;
    return QWidget::qt_cast( clname );
}

bool BaseConsoleWindow::qt_invoke( int _id, QUObject* _o )
{
    switch ( _id - staticMetaObject()->slotOffset() ) {
    case 0: languageChange(); break;
    case 1: init(); break;
    case 2: destroy(); break;
    default:
	return QWidget::qt_invoke( _id, _o );
    }
    return TRUE;
}

bool BaseConsoleWindow::qt_emit( int _id, QUObject* _o )
{
    return QWidget::qt_emit(_id,_o);
}
#ifndef QT_NO_PROPERTIES

bool BaseConsoleWindow::qt_property( int id, int f, QVariant* v)
{
    return QWidget::qt_property( id, f, v);
}

bool BaseConsoleWindow::qt_static_property( QObject* , int , int , QVariant* ){ return FALSE; }
#endif // QT_NO_PROPERTIES
