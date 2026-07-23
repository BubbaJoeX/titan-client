/****************************************************************************
** Form implementation generated from reading ui file 'D:\titan\client\src\game\client\application\SwgGodClient\src\shared\ui\BaseGotoDialog.ui'
**
** Created: Thu Jul 23 03:18:13 2026
**      by: The User Interface Compiler ($Id: qt/main.cpp   3.3.4   edited Nov 24 2003 $)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#include "D:\titan\client\src\build\win32\x64\Release\BaseGotoDialog.h"

#include <qvariant.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qpushbutton.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qwhatsthis.h>
#include <qimage.h>
#include <qpixmap.h>

/*
 *  Constructs a BaseGotoDialog as a child of 'parent', with the
 *  name 'name' and widget flags set to 'f'.
 *
 *  The dialog will by default be modeless, unless you set 'modal' to
 *  TRUE to construct a modal dialog.
 */
BaseGotoDialog::BaseGotoDialog( QWidget* parent, const char* name, bool modal, WFlags fl )
    : QDialog( parent, name, modal, fl )
{
    if ( !name )
	setName( "BaseGotoDialog" );
    setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)0, (QSizePolicy::SizeType)0, 0, 0, sizePolicy().hasHeightForWidth() ) );
    setMinimumSize( QSize( 192, 160 ) );
    setMaximumSize( QSize( 192, 160 ) );
    BaseGotoDialogLayout = new QGridLayout( this, 1, 1, 11, 6, "BaseGotoDialogLayout"); 
    BaseGotoDialogLayout->setResizeMode( QLayout::Fixed );

    textLabel3 = new QLabel( this, "textLabel3" );

    BaseGotoDialogLayout->addWidget( textLabel3, 2, 0 );

    textLabel2 = new QLabel( this, "textLabel2" );

    BaseGotoDialogLayout->addWidget( textLabel2, 1, 0 );

    textLabel1 = new QLabel( this, "textLabel1" );

    BaseGotoDialogLayout->addWidget( textLabel1, 0, 0 );

    m_z = new QLineEdit( this, "m_z" );
    m_z->setFrameShape( QLineEdit::LineEditPanel );
    m_z->setFrameShadow( QLineEdit::Sunken );

    BaseGotoDialogLayout->addMultiCellWidget( m_z, 2, 2, 1, 3 );

    m_x = new QLineEdit( this, "m_x" );

    BaseGotoDialogLayout->addMultiCellWidget( m_x, 0, 0, 1, 3 );

    m_y = new QLineEdit( this, "m_y" );

    BaseGotoDialogLayout->addMultiCellWidget( m_y, 1, 1, 1, 3 );

    m_okButton = new QPushButton( this, "m_okButton" );
    m_okButton->setDefault( TRUE );

    BaseGotoDialogLayout->addMultiCellWidget( m_okButton, 4, 4, 0, 2 );

    m_cancelButton = new QPushButton( this, "m_cancelButton" );

    BaseGotoDialogLayout->addWidget( m_cancelButton, 4, 3 );

    textLabel4 = new QLabel( this, "textLabel4" );

    BaseGotoDialogLayout->addMultiCellWidget( textLabel4, 3, 3, 0, 1 );

    m_distance = new QLineEdit( this, "m_distance" );

    BaseGotoDialogLayout->addMultiCellWidget( m_distance, 3, 3, 2, 3 );
    languageChange();
    resize( QSize(192, 160).expandedTo(minimumSizeHint()) );
    clearWState( WState_Polished );

    // signals and slots connections
    connect( m_okButton, SIGNAL( released() ), this, SLOT( accept() ) );
    connect( m_cancelButton, SIGNAL( released() ), this, SLOT( reject() ) );

    // tab order
    setTabOrder( m_x, m_y );
    setTabOrder( m_y, m_z );
    setTabOrder( m_z, m_distance );
    setTabOrder( m_distance, m_okButton );
    setTabOrder( m_okButton, m_cancelButton );
}

/*
 *  Destroys the object and frees any allocated resources
 */
BaseGotoDialog::~BaseGotoDialog()
{
    // no need to delete child widgets, Qt does it all for us
}

/*
 *  Sets the strings of the subwidgets using the current
 *  language.
 */
void BaseGotoDialog::languageChange()
{
    setCaption( tr( "GoTo" ) );
    textLabel3->setText( tr( "Z" ) );
    textLabel2->setText( tr( "Y" ) );
    textLabel1->setText( tr( "X" ) );
    QToolTip::add( m_z, tr( "Z location" ) );
    QWhatsThis::add( m_z, tr( "Z location" ) );
    QToolTip::add( m_x, tr( "X location" ) );
    QWhatsThis::add( m_x, tr( "X location" ) );
    QToolTip::add( m_y, tr( "Y location" ) );
    QWhatsThis::add( m_y, tr( "Y location" ) );
    m_okButton->setText( tr( "OK" ) );
    m_okButton->setAccel( QKeySequence( tr( "Return" ) ) );
    m_cancelButton->setText( tr( "CANCEL" ) );
    textLabel4->setText( tr( "Distance" ) );
    QToolTip::add( m_distance, tr( "Pivot distance" ) );
    QWhatsThis::add( m_distance, tr( "Pivot distance" ) );
}

/****************************************************************************
** BaseGotoDialog meta object code from reading C++ file 'BaseGotoDialog.h'
**
** Created: Thu Jul 23 03:18:13 2026
**      by: The Qt MOC ($Id: $)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#undef QT_NO_COMPAT
#include "D:\titan\client\src\build\win32\x64\Release\BaseGotoDialog.h"
#include <qmetaobject.h>
#include <qapplication.h>

#include <private/qucomextra_p.h>
#if !defined(Q_MOC_OUTPUT_REVISION) || (Q_MOC_OUTPUT_REVISION != 26)
#error "This file was generated using the moc from 3.3.4. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

const char *BaseGotoDialog::className() const
{
    return "BaseGotoDialog";
}

QMetaObject *BaseGotoDialog::metaObj = 0;
static QMetaObjectCleanUp cleanUp_BaseGotoDialog( "BaseGotoDialog", &BaseGotoDialog::staticMetaObject );

#ifndef QT_NO_TRANSLATION
QString BaseGotoDialog::tr( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "BaseGotoDialog", s, c, QApplication::DefaultCodec );
    else
	return QString::fromLatin1( s );
}
#ifndef QT_NO_TRANSLATION_UTF8
QString BaseGotoDialog::trUtf8( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "BaseGotoDialog", s, c, QApplication::UnicodeUTF8 );
    else
	return QString::fromUtf8( s );
}
#endif // QT_NO_TRANSLATION_UTF8

#endif // QT_NO_TRANSLATION

QMetaObject* BaseGotoDialog::staticMetaObject()
{
    if ( metaObj )
	return metaObj;
    QMetaObject* parentObject = QDialog::staticMetaObject();
    static const QUMethod slot_0 = {"languageChange", 0, 0 };
    static const QMetaData slot_tbl[] = {
	{ "languageChange()", &slot_0, QMetaData::Protected }
    };
    metaObj = QMetaObject::new_metaobject(
	"BaseGotoDialog", parentObject,
	slot_tbl, 1,
	0, 0,
#ifndef QT_NO_PROPERTIES
	0, 0,
	0, 0,
#endif // QT_NO_PROPERTIES
	0, 0 );
    cleanUp_BaseGotoDialog.setMetaObject( metaObj );
    return metaObj;
}

void* BaseGotoDialog::qt_cast( const char* clname )
{
    if ( !qstrcmp( clname, "BaseGotoDialog" ) )
	return this;
    return QDialog::qt_cast( clname );
}

bool BaseGotoDialog::qt_invoke( int _id, QUObject* _o )
{
    switch ( _id - staticMetaObject()->slotOffset() ) {
    case 0: languageChange(); break;
    default:
	return QDialog::qt_invoke( _id, _o );
    }
    return TRUE;
}

bool BaseGotoDialog::qt_emit( int _id, QUObject* _o )
{
    return QDialog::qt_emit(_id,_o);
}
#ifndef QT_NO_PROPERTIES

bool BaseGotoDialog::qt_property( int id, int f, QVariant* v)
{
    return QDialog::qt_property( id, f, v);
}

bool BaseGotoDialog::qt_static_property( QObject* , int , int , QVariant* ){ return FALSE; }
#endif // QT_NO_PROPERTIES
