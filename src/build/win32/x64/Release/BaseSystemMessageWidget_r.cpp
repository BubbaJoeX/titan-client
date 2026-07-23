/****************************************************************************
** Form implementation generated from reading ui file 'D:\titan\client\src\game\client\application\SwgGodClient\src\shared\ui\BaseSystemMessageWidget.ui'
**
** Created: Thu Jul 23 03:18:16 2026
**      by: The User Interface Compiler ($Id: qt/main.cpp   3.3.4   edited Nov 24 2003 $)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#include "D:\titan\client\src\build\win32\x64\Release\BaseSystemMessageWidget.h"

#include <qvariant.h>
#include <qpushbutton.h>
#include <qbuttongroup.h>
#include <qradiobutton.h>
#include <qgroupbox.h>
#include <qheader.h>
#include <qlistview.h>
#include <qlineedit.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qwhatsthis.h>
#include <qimage.h>
#include <qpixmap.h>

/*
 *  Constructs a BaseSystemMessageWidget as a child of 'parent', with the
 *  name 'name' and widget flags set to 'f'.
 */
BaseSystemMessageWidget::BaseSystemMessageWidget( QWidget* parent, const char* name, WFlags fl )
    : QWidget( parent, name, fl )
{
    if ( !name )
	setName( "BaseSystemMessageWidget" );
    setIcon( QPixmap::fromMimeSource( "hi16_mime_document" ) );
    BaseSystemMessageWidgetLayout = new QGridLayout( this, 1, 1, 11, 6, "BaseSystemMessageWidgetLayout"); 

    m_targetSelectionButtonGroup = new QButtonGroup( this, "m_targetSelectionButtonGroup" );
    m_targetSelectionButtonGroup->setExclusive( TRUE );
    m_targetSelectionButtonGroup->setColumnLayout(0, Qt::Vertical );
    m_targetSelectionButtonGroup->layout()->setSpacing( 6 );
    m_targetSelectionButtonGroup->layout()->setMargin( 11 );
    m_targetSelectionButtonGroupLayout = new QGridLayout( m_targetSelectionButtonGroup->layout() );
    m_targetSelectionButtonGroupLayout->setAlignment( Qt::AlignTop );

    Layout4 = new QVBoxLayout( 0, 0, 6, "Layout4"); 

    m_currentPlanetRadioButton = new QRadioButton( m_targetSelectionButtonGroup, "m_currentPlanetRadioButton" );
    m_currentPlanetRadioButton->setChecked( TRUE );
    Layout4->addWidget( m_currentPlanetRadioButton );

    m_currentGalaxyRadioButton = new QRadioButton( m_targetSelectionButtonGroup, "m_currentGalaxyRadioButton" );
    m_currentGalaxyRadioButton->setChecked( FALSE );
    Layout4->addWidget( m_currentGalaxyRadioButton );

    m_allSWGRadioButton = new QRadioButton( m_targetSelectionButtonGroup, "m_allSWGRadioButton" );
    m_allSWGRadioButton->setChecked( FALSE );
    Layout4->addWidget( m_allSWGRadioButton );

    Layout3 = new QVBoxLayout( 0, 0, 6, "Layout3"); 

    m_sendToRoomRadioButton = new QRadioButton( m_targetSelectionButtonGroup, "m_sendToRoomRadioButton" );
    m_sendToRoomRadioButton->setChecked( FALSE );
    Layout3->addWidget( m_sendToRoomRadioButton );

    GroupBox1 = new QGroupBox( m_targetSelectionButtonGroup, "GroupBox1" );
    GroupBox1->setEnabled( FALSE );

    m_roomsListView = new QListView( GroupBox1, "m_roomsListView" );
    m_roomsListView->addColumn( tr( "Room" ) );
    m_roomsListView->setGeometry( QRect( 10, 16, 540, 104 ) );
    Layout3->addWidget( GroupBox1 );
    Layout4->addLayout( Layout3 );

    Layout2 = new QVBoxLayout( 0, 0, 6, "Layout2"); 

    m_sendToPlayerRadioButton = new QRadioButton( m_targetSelectionButtonGroup, "m_sendToPlayerRadioButton" );
    m_sendToPlayerRadioButton->setEnabled( TRUE );
    Layout2->addWidget( m_sendToPlayerRadioButton );

    GroupBox2 = new QGroupBox( m_targetSelectionButtonGroup, "GroupBox2" );
    GroupBox2->setEnabled( FALSE );

    m_playerLineEdit = new QLineEdit( GroupBox2, "m_playerLineEdit" );
    m_playerLineEdit->setGeometry( QRect( 10, 21, 541, 20 ) );
    m_playerLineEdit->setFrameShape( QLineEdit::LineEditPanel );
    m_playerLineEdit->setFrameShadow( QLineEdit::Sunken );
    Layout2->addWidget( GroupBox2 );
    Layout4->addLayout( Layout2 );

    m_targetSelectionButtonGroupLayout->addLayout( Layout4, 0, 0 );

    BaseSystemMessageWidgetLayout->addWidget( m_targetSelectionButtonGroup, 0, 0 );

    Layout7 = new QVBoxLayout( 0, 0, 6, "Layout7"); 
    Spacer3 = new QSpacerItem( 0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding );
    Layout7->addItem( Spacer3 );

    TextLabel1 = new QLabel( this, "TextLabel1" );
    Layout7->addWidget( TextLabel1 );

    m_messageLineEdit = new QLineEdit( this, "m_messageLineEdit" );
    Layout7->addWidget( m_messageLineEdit );

    Layout5 = new QHBoxLayout( 0, 0, 6, "Layout5"); 
    Spacer2 = new QSpacerItem( 0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum );
    Layout5->addItem( Spacer2 );

    m_cancelPushButton = new QPushButton( this, "m_cancelPushButton" );
    Layout5->addWidget( m_cancelPushButton );

    m_sendPushButton = new QPushButton( this, "m_sendPushButton" );
    Layout5->addWidget( m_sendPushButton );
    Layout7->addLayout( Layout5 );

    BaseSystemMessageWidgetLayout->addLayout( Layout7, 1, 0 );
    languageChange();
    resize( QSize(606, 493).expandedTo(minimumSizeHint()) );
    clearWState( WState_Polished );

    // signals and slots connections
    connect( m_sendToRoomRadioButton, SIGNAL( toggled(bool) ), GroupBox2, SLOT( setDisabled(bool) ) );
    connect( m_sendToPlayerRadioButton, SIGNAL( toggled(bool) ), GroupBox1, SLOT( setDisabled(bool) ) );
    connect( m_cancelPushButton, SIGNAL( pressed() ), this, SLOT( close() ) );
    connect( m_currentPlanetRadioButton, SIGNAL( toggled(bool) ), GroupBox1, SLOT( setDisabled(bool) ) );
    connect( m_currentPlanetRadioButton, SIGNAL( toggled(bool) ), GroupBox2, SLOT( setDisabled(bool) ) );
    connect( m_currentGalaxyRadioButton, SIGNAL( toggled(bool) ), GroupBox1, SLOT( setDisabled(bool) ) );
    connect( m_currentGalaxyRadioButton, SIGNAL( toggled(bool) ), GroupBox2, SLOT( setDisabled(bool) ) );
    connect( m_allSWGRadioButton, SIGNAL( toggled(bool) ), GroupBox1, SLOT( setDisabled(bool) ) );
    connect( m_allSWGRadioButton, SIGNAL( toggled(bool) ), GroupBox2, SLOT( setDisabled(bool) ) );
    connect( m_sendPushButton, SIGNAL( clicked() ), this, SLOT( sendMessage() ) );
}

/*
 *  Destroys the object and frees any allocated resources
 */
BaseSystemMessageWidget::~BaseSystemMessageWidget()
{
    // no need to delete child widgets, Qt does it all for us
}

/*
 *  Sets the strings of the subwidgets using the current
 *  language.
 */
void BaseSystemMessageWidget::languageChange()
{
    setCaption( tr( "Send System Message" ) );
    m_targetSelectionButtonGroup->setTitle( tr( "Target Selection" ) );
    m_currentPlanetRadioButton->setText( tr( "Send To Current Planet" ) );
    m_currentGalaxyRadioButton->setText( tr( "Send To Current Galaxy" ) );
    m_allSWGRadioButton->setText( tr( "Send to all SWG" ) );
    m_sendToRoomRadioButton->setText( tr( "Send To Room" ) );
    GroupBox1->setTitle( tr( "Room" ) );
    m_roomsListView->header()->setLabel( 0, tr( "Room" ) );
    m_sendToPlayerRadioButton->setText( tr( "Send To Player" ) );
    GroupBox2->setTitle( tr( "Player" ) );
    TextLabel1->setText( tr( "Message" ) );
    m_cancelPushButton->setText( tr( "Cancel" ) );
    m_sendPushButton->setText( tr( "Send" ) );
}

void BaseSystemMessageWidget::sendMessage()
{
    qWarning( "BaseSystemMessageWidget::sendMessage(): Not implemented yet" );
}

/****************************************************************************
** BaseSystemMessageWidget meta object code from reading C++ file 'BaseSystemMessageWidget.h'
**
** Created: Thu Jul 23 03:18:16 2026
**      by: The Qt MOC ($Id: $)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#undef QT_NO_COMPAT
#include "D:\titan\client\src\build\win32\x64\Release\BaseSystemMessageWidget.h"
#include <qmetaobject.h>
#include <qapplication.h>

#include <private/qucomextra_p.h>
#if !defined(Q_MOC_OUTPUT_REVISION) || (Q_MOC_OUTPUT_REVISION != 26)
#error "This file was generated using the moc from 3.3.4. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

const char *BaseSystemMessageWidget::className() const
{
    return "BaseSystemMessageWidget";
}

QMetaObject *BaseSystemMessageWidget::metaObj = 0;
static QMetaObjectCleanUp cleanUp_BaseSystemMessageWidget( "BaseSystemMessageWidget", &BaseSystemMessageWidget::staticMetaObject );

#ifndef QT_NO_TRANSLATION
QString BaseSystemMessageWidget::tr( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "BaseSystemMessageWidget", s, c, QApplication::DefaultCodec );
    else
	return QString::fromLatin1( s );
}
#ifndef QT_NO_TRANSLATION_UTF8
QString BaseSystemMessageWidget::trUtf8( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "BaseSystemMessageWidget", s, c, QApplication::UnicodeUTF8 );
    else
	return QString::fromUtf8( s );
}
#endif // QT_NO_TRANSLATION_UTF8

#endif // QT_NO_TRANSLATION

QMetaObject* BaseSystemMessageWidget::staticMetaObject()
{
    if ( metaObj )
	return metaObj;
    QMetaObject* parentObject = QWidget::staticMetaObject();
    static const QUMethod slot_0 = {"sendMessage", 0, 0 };
    static const QUMethod slot_1 = {"languageChange", 0, 0 };
    static const QMetaData slot_tbl[] = {
	{ "sendMessage()", &slot_0, QMetaData::Public },
	{ "languageChange()", &slot_1, QMetaData::Protected }
    };
    metaObj = QMetaObject::new_metaobject(
	"BaseSystemMessageWidget", parentObject,
	slot_tbl, 2,
	0, 0,
#ifndef QT_NO_PROPERTIES
	0, 0,
	0, 0,
#endif // QT_NO_PROPERTIES
	0, 0 );
    cleanUp_BaseSystemMessageWidget.setMetaObject( metaObj );
    return metaObj;
}

void* BaseSystemMessageWidget::qt_cast( const char* clname )
{
    if ( !qstrcmp( clname, "BaseSystemMessageWidget" ) )
	return this;
    return QWidget::qt_cast( clname );
}

bool BaseSystemMessageWidget::qt_invoke( int _id, QUObject* _o )
{
    switch ( _id - staticMetaObject()->slotOffset() ) {
    case 0: sendMessage(); break;
    case 1: languageChange(); break;
    default:
	return QWidget::qt_invoke( _id, _o );
    }
    return TRUE;
}

bool BaseSystemMessageWidget::qt_emit( int _id, QUObject* _o )
{
    return QWidget::qt_emit(_id,_o);
}
#ifndef QT_NO_PROPERTIES

bool BaseSystemMessageWidget::qt_property( int id, int f, QVariant* v)
{
    return QWidget::qt_property( id, f, v);
}

bool BaseSystemMessageWidget::qt_static_property( QObject* , int , int , QVariant* ){ return FALSE; }
#endif // QT_NO_PROPERTIES
