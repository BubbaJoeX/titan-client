/****************************************************************************
** Form implementation generated from reading ui file 'D:\titan\client\src\game\client\application\SwgGodClient\src\shared\ui\BaseObjectTransformWindow.ui'
**
** Created: Thu Jul 23 03:18:14 2026
**      by: The User Interface Compiler ($Id: qt/main.cpp   3.3.4   edited Nov 24 2003 $)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#include "D:\titan\client\src\build\win32\x64\Release\BaseObjectTransformWindow.h"

#include <qvariant.h>
#include <qpushbutton.h>
#include <qgroupbox.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qspinbox.h>
#include <qdial.h>
#include <qframe.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qwhatsthis.h>
#include <qimage.h>
#include <qpixmap.h>

/*
 *  Constructs a BaseObjectTransformWindow as a child of 'parent', with the
 *  name 'name' and widget flags set to 'f'.
 *
 *  The dialog will by default be modeless, unless you set 'modal' to
 *  TRUE to construct a modal dialog.
 */
BaseObjectTransformWindow::BaseObjectTransformWindow( QWidget* parent, const char* name, bool modal, WFlags fl )
    : QDialog( parent, name, modal, fl )
{
    if ( !name )
	setName( "BaseObjectTransformWindow" );
    BaseObjectTransformWindowLayout = new QGridLayout( this, 1, 1, 11, 6, "BaseObjectTransformWindowLayout"); 

    GroupBoxTranslations = new QGroupBox( this, "GroupBoxTranslations" );
    GroupBoxTranslations->setColumnLayout(0, Qt::Vertical );
    GroupBoxTranslations->layout()->setSpacing( 6 );
    GroupBoxTranslations->layout()->setMargin( 11 );
    GroupBoxTranslationsLayout = new QGridLayout( GroupBoxTranslations->layout() );
    GroupBoxTranslationsLayout->setAlignment( Qt::AlignTop );
    SpacerXText = new QSpacerItem( 20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum );
    GroupBoxTranslationsLayout->addItem( SpacerXText, 0, 3 );

    TextLabelXMeters = new QLabel( GroupBoxTranslations, "TextLabelXMeters" );

    GroupBoxTranslationsLayout->addWidget( TextLabelXMeters, 0, 2 );

    m_LineEditX = new QLineEdit( GroupBoxTranslations, "m_LineEditX" );
    m_LineEditX->setFrameShape( QLineEdit::StyledPanel );
    m_LineEditX->setFrameShadow( QLineEdit::Sunken );

    GroupBoxTranslationsLayout->addWidget( m_LineEditX, 0, 1 );
    SpacerYText = new QSpacerItem( 20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum );
    GroupBoxTranslationsLayout->addItem( SpacerYText, 1, 3 );

    TextLabelYMeters = new QLabel( GroupBoxTranslations, "TextLabelYMeters" );

    GroupBoxTranslationsLayout->addWidget( TextLabelYMeters, 1, 2 );

    m_LineEditY = new QLineEdit( GroupBoxTranslations, "m_LineEditY" );

    GroupBoxTranslationsLayout->addWidget( m_LineEditY, 1, 1 );

    TextLabelY = new QLabel( GroupBoxTranslations, "TextLabelY" );

    GroupBoxTranslationsLayout->addWidget( TextLabelY, 1, 0 );

    TextLabelZ = new QLabel( GroupBoxTranslations, "TextLabelZ" );

    GroupBoxTranslationsLayout->addWidget( TextLabelZ, 2, 0 );

    m_LineEditZ = new QLineEdit( GroupBoxTranslations, "m_LineEditZ" );

    GroupBoxTranslationsLayout->addWidget( m_LineEditZ, 2, 1 );

    TextLabelZMeters = new QLabel( GroupBoxTranslations, "TextLabelZMeters" );

    GroupBoxTranslationsLayout->addWidget( TextLabelZMeters, 2, 2 );

    TextLabelX = new QLabel( GroupBoxTranslations, "TextLabelX" );

    GroupBoxTranslationsLayout->addWidget( TextLabelX, 0, 0 );
    SpacerZText = new QSpacerItem( 20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum );
    GroupBoxTranslationsLayout->addItem( SpacerZText, 2, 3 );

    BaseObjectTransformWindowLayout->addMultiCellWidget( GroupBoxTranslations, 0, 0, 0, 2 );

    GroupBoxRotations = new QGroupBox( this, "GroupBoxRotations" );
    GroupBoxRotations->setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)5, (QSizePolicy::SizeType)5, 0, 0, GroupBoxRotations->sizePolicy().hasHeightForWidth() ) );
    GroupBoxRotations->setColumnLayout(0, Qt::Vertical );
    GroupBoxRotations->layout()->setSpacing( 6 );
    GroupBoxRotations->layout()->setMargin( 11 );
    GroupBoxRotationsLayout = new QGridLayout( GroupBoxRotations->layout() );
    GroupBoxRotationsLayout->setAlignment( Qt::AlignTop );

    LayoutAllRotation = new QHBoxLayout( 0, 0, 6, "LayoutAllRotation"); 

    LayoutAllYaw = new QVBoxLayout( 0, 0, 6, "LayoutAllYaw"); 

    Layout2 = new QHBoxLayout( 0, 0, 6, "Layout2"); 

    YawText = new QLabel( GroupBoxRotations, "YawText" );
    YawText->setAlignment( int( QLabel::AlignAuto | QLabel::AlignCenter ) );
    Layout2->addWidget( YawText );

    m_yawEdit = new QSpinBox( GroupBoxRotations, "m_yawEdit" );
    m_yawEdit->setMouseTracking( FALSE );
    m_yawEdit->setButtonSymbols( QSpinBox::PlusMinus );
    m_yawEdit->setMaxValue( 180 );
    m_yawEdit->setMinValue( -180 );
    m_yawEdit->setLineStep( 1 );
    Layout2->addWidget( m_yawEdit );
    LayoutAllYaw->addLayout( Layout2 );

    m_yawDial = new QDial( GroupBoxRotations, "m_yawDial" );
    m_yawDial->setAutoMask( FALSE );
    m_yawDial->setTracking( TRUE );
    m_yawDial->setWrapping( TRUE );
    m_yawDial->setNotchesVisible( TRUE );
    m_yawDial->setMinValue( -180 );
    m_yawDial->setMaxValue( 180 );
    m_yawDial->setLineStep( 10 );
    m_yawDial->setPageStep( 10 );
    m_yawDial->setValue( 0 );
    LayoutAllYaw->addWidget( m_yawDial );
    LayoutAllRotation->addLayout( LayoutAllYaw );

    Line1 = new QFrame( GroupBoxRotations, "Line1" );
    Line1->setFrameShape( QFrame::VLine );
    Line1->setFrameShadow( QFrame::Sunken );
    Line1->setFrameShape( QFrame::VLine );
    Line1->setFrameShape( QFrame::VLine );
    LayoutAllRotation->addWidget( Line1 );

    LayoutAllPitch = new QVBoxLayout( 0, 0, 6, "LayoutAllPitch"); 

    Layout2_2 = new QHBoxLayout( 0, 0, 6, "Layout2_2"); 

    PitchText = new QLabel( GroupBoxRotations, "PitchText" );
    PitchText->setAlignment( int( QLabel::AlignAuto | QLabel::AlignCenter ) );
    Layout2_2->addWidget( PitchText );

    m_pitchEdit = new QSpinBox( GroupBoxRotations, "m_pitchEdit" );
    m_pitchEdit->setMouseTracking( FALSE );
    m_pitchEdit->setButtonSymbols( QSpinBox::PlusMinus );
    m_pitchEdit->setMaxValue( 180 );
    m_pitchEdit->setMinValue( -180 );
    m_pitchEdit->setLineStep( 1 );
    Layout2_2->addWidget( m_pitchEdit );
    LayoutAllPitch->addLayout( Layout2_2 );

    m_pitchDial = new QDial( GroupBoxRotations, "m_pitchDial" );
    m_pitchDial->setAutoMask( FALSE );
    m_pitchDial->setTracking( TRUE );
    m_pitchDial->setWrapping( TRUE );
    m_pitchDial->setNotchesVisible( TRUE );
    m_pitchDial->setMinValue( -180 );
    m_pitchDial->setMaxValue( 180 );
    m_pitchDial->setLineStep( 10 );
    m_pitchDial->setPageStep( 10 );
    m_pitchDial->setValue( 0 );
    LayoutAllPitch->addWidget( m_pitchDial );
    LayoutAllRotation->addLayout( LayoutAllPitch );

    Line1_2 = new QFrame( GroupBoxRotations, "Line1_2" );
    Line1_2->setFrameShape( QFrame::VLine );
    Line1_2->setFrameShadow( QFrame::Sunken );
    Line1_2->setFrameShape( QFrame::VLine );
    Line1_2->setFrameShape( QFrame::VLine );
    LayoutAllRotation->addWidget( Line1_2 );

    LayoutAllRoll = new QVBoxLayout( 0, 0, 6, "LayoutAllRoll"); 

    Layout2_3 = new QHBoxLayout( 0, 0, 6, "Layout2_3"); 

    RollText = new QLabel( GroupBoxRotations, "RollText" );
    RollText->setAlignment( int( QLabel::AlignAuto | QLabel::AlignCenter ) );
    Layout2_3->addWidget( RollText );

    m_rollEdit = new QSpinBox( GroupBoxRotations, "m_rollEdit" );
    m_rollEdit->setMouseTracking( FALSE );
    m_rollEdit->setButtonSymbols( QSpinBox::PlusMinus );
    m_rollEdit->setMaxValue( 180 );
    m_rollEdit->setMinValue( -180 );
    m_rollEdit->setLineStep( 1 );
    Layout2_3->addWidget( m_rollEdit );
    LayoutAllRoll->addLayout( Layout2_3 );

    m_rollDial = new QDial( GroupBoxRotations, "m_rollDial" );
    m_rollDial->setAutoMask( FALSE );
    m_rollDial->setTracking( TRUE );
    m_rollDial->setWrapping( TRUE );
    m_rollDial->setNotchesVisible( TRUE );
    m_rollDial->setMinValue( -180 );
    m_rollDial->setMaxValue( 180 );
    m_rollDial->setLineStep( 10 );
    m_rollDial->setPageStep( 10 );
    m_rollDial->setValue( 0 );
    LayoutAllRoll->addWidget( m_rollDial );
    LayoutAllRotation->addLayout( LayoutAllRoll );

    GroupBoxRotationsLayout->addLayout( LayoutAllRotation, 0, 0 );

    BaseObjectTransformWindowLayout->addMultiCellWidget( GroupBoxRotations, 1, 1, 0, 2 );
    SpacerButtons = new QSpacerItem( 20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum );
    BaseObjectTransformWindowLayout->addItem( SpacerButtons, 2, 0 );

    m_okButton = new QPushButton( this, "m_okButton" );
    m_okButton->setMinimumSize( QSize( 64, 0 ) );

    BaseObjectTransformWindowLayout->addWidget( m_okButton, 2, 2 );

    m_cancelButton = new QPushButton( this, "m_cancelButton" );
    m_cancelButton->setMinimumSize( QSize( 64, 0 ) );

    BaseObjectTransformWindowLayout->addWidget( m_cancelButton, 2, 1 );
    languageChange();
    resize( QSize(444, 330).expandedTo(minimumSizeHint()) );
    clearWState( WState_Polished );

    // signals and slots connections
    connect( m_yawDial, SIGNAL( dialMoved(int) ), m_yawEdit, SLOT( setValue(int) ) );
    connect( m_yawEdit, SIGNAL( valueChanged(int) ), m_yawDial, SLOT( setValue(int) ) );
    connect( m_pitchDial, SIGNAL( valueChanged(int) ), m_pitchEdit, SLOT( setValue(int) ) );
    connect( m_pitchEdit, SIGNAL( valueChanged(int) ), m_pitchDial, SLOT( setValue(int) ) );
    connect( m_rollDial, SIGNAL( valueChanged(int) ), m_rollEdit, SLOT( setValue(int) ) );
    connect( m_rollEdit, SIGNAL( valueChanged(int) ), m_rollDial, SLOT( setValue(int) ) );
    connect( m_okButton, SIGNAL( pressed() ), this, SLOT( accept() ) );
    connect( m_cancelButton, SIGNAL( pressed() ), this, SLOT( reject() ) );
}

/*
 *  Destroys the object and frees any allocated resources
 */
BaseObjectTransformWindow::~BaseObjectTransformWindow()
{
    // no need to delete child widgets, Qt does it all for us
}

/*
 *  Sets the strings of the subwidgets using the current
 *  language.
 */
void BaseObjectTransformWindow::languageChange()
{
    setCaption( tr( "Object Transform" ) );
    GroupBoxTranslations->setTitle( tr( "Translations" ) );
    TextLabelXMeters->setText( tr( "meters" ) );
    TextLabelYMeters->setText( tr( "meters" ) );
    TextLabelY->setText( tr( "Y" ) );
    TextLabelZ->setText( tr( "Z" ) );
    TextLabelZMeters->setText( tr( "meters" ) );
    TextLabelX->setText( tr( "X" ) );
    GroupBoxRotations->setTitle( tr( "Rotations" ) );
    YawText->setText( tr( "Yaw" ) );
    m_yawEdit->setPrefix( QString::null );
    m_yawEdit->setSuffix( QString::null );
    m_yawEdit->setSpecialValueText( QString::null );
    PitchText->setText( tr( "Pitch" ) );
    m_pitchEdit->setPrefix( QString::null );
    m_pitchEdit->setSuffix( QString::null );
    m_pitchEdit->setSpecialValueText( QString::null );
    RollText->setText( tr( "Roll" ) );
    m_rollEdit->setPrefix( QString::null );
    m_rollEdit->setSuffix( QString::null );
    m_rollEdit->setSpecialValueText( QString::null );
    m_okButton->setText( tr( "Ok" ) );
    m_cancelButton->setText( tr( "Cancel" ) );
}

/****************************************************************************
** BaseObjectTransformWindow meta object code from reading C++ file 'BaseObjectTransformWindow.h'
**
** Created: Thu Jul 23 03:18:14 2026
**      by: The Qt MOC ($Id: $)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#undef QT_NO_COMPAT
#include "D:\titan\client\src\build\win32\x64\Release\BaseObjectTransformWindow.h"
#include <qmetaobject.h>
#include <qapplication.h>

#include <private/qucomextra_p.h>
#if !defined(Q_MOC_OUTPUT_REVISION) || (Q_MOC_OUTPUT_REVISION != 26)
#error "This file was generated using the moc from 3.3.4. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

const char *BaseObjectTransformWindow::className() const
{
    return "BaseObjectTransformWindow";
}

QMetaObject *BaseObjectTransformWindow::metaObj = 0;
static QMetaObjectCleanUp cleanUp_BaseObjectTransformWindow( "BaseObjectTransformWindow", &BaseObjectTransformWindow::staticMetaObject );

#ifndef QT_NO_TRANSLATION
QString BaseObjectTransformWindow::tr( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "BaseObjectTransformWindow", s, c, QApplication::DefaultCodec );
    else
	return QString::fromLatin1( s );
}
#ifndef QT_NO_TRANSLATION_UTF8
QString BaseObjectTransformWindow::trUtf8( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "BaseObjectTransformWindow", s, c, QApplication::UnicodeUTF8 );
    else
	return QString::fromUtf8( s );
}
#endif // QT_NO_TRANSLATION_UTF8

#endif // QT_NO_TRANSLATION

QMetaObject* BaseObjectTransformWindow::staticMetaObject()
{
    if ( metaObj )
	return metaObj;
    QMetaObject* parentObject = QDialog::staticMetaObject();
    static const QUMethod slot_0 = {"languageChange", 0, 0 };
    static const QMetaData slot_tbl[] = {
	{ "languageChange()", &slot_0, QMetaData::Protected }
    };
    metaObj = QMetaObject::new_metaobject(
	"BaseObjectTransformWindow", parentObject,
	slot_tbl, 1,
	0, 0,
#ifndef QT_NO_PROPERTIES
	0, 0,
	0, 0,
#endif // QT_NO_PROPERTIES
	0, 0 );
    cleanUp_BaseObjectTransformWindow.setMetaObject( metaObj );
    return metaObj;
}

void* BaseObjectTransformWindow::qt_cast( const char* clname )
{
    if ( !qstrcmp( clname, "BaseObjectTransformWindow" ) )
	return this;
    return QDialog::qt_cast( clname );
}

bool BaseObjectTransformWindow::qt_invoke( int _id, QUObject* _o )
{
    switch ( _id - staticMetaObject()->slotOffset() ) {
    case 0: languageChange(); break;
    default:
	return QDialog::qt_invoke( _id, _o );
    }
    return TRUE;
}

bool BaseObjectTransformWindow::qt_emit( int _id, QUObject* _o )
{
    return QDialog::qt_emit(_id,_o);
}
#ifndef QT_NO_PROPERTIES

bool BaseObjectTransformWindow::qt_property( int id, int f, QVariant* v)
{
    return QDialog::qt_property( id, f, v);
}

bool BaseObjectTransformWindow::qt_static_property( QObject* , int , int , QVariant* ){ return FALSE; }
#endif // QT_NO_PROPERTIES
