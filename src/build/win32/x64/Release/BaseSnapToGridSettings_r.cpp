/****************************************************************************
** Form implementation generated from reading ui file 'D:\titan\client\src\game\client\application\SwgGodClient\src\shared\ui\BaseSnapToGridSettings.ui'
**
** Created: Thu Jul 23 03:18:15 2026
**      by: The User Interface Compiler ($Id: qt/main.cpp   3.3.4   edited Nov 24 2003 $)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#include "D:\titan\client\src\build\win32\x64\Release\BaseSnapToGridSettings.h"

#include <qvariant.h>
#include <qpushbutton.h>
#include <qgroupbox.h>
#include <qcheckbox.h>
#include <qspinbox.h>
#include <qslider.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qwhatsthis.h>
#include <qimage.h>
#include <qpixmap.h>

/*
 *  Constructs a BaseSnapToGridSettings as a child of 'parent', with the
 *  name 'name' and widget flags set to 'f'.
 *
 *  The dialog will by default be modeless, unless you set 'modal' to
 *  TRUE to construct a modal dialog.
 */
BaseSnapToGridSettings::BaseSnapToGridSettings( QWidget* parent, const char* name, bool modal, WFlags fl )
    : QDialog( parent, name, modal, fl )
{
    if ( !name )
	setName( "BaseSnapToGridSettings" );
    BaseSnapToGridSettingsLayout = new QGridLayout( this, 1, 1, 11, 6, "BaseSnapToGridSettingsLayout"); 

    m_cancelButton = new QPushButton( this, "m_cancelButton" );

    BaseSnapToGridSettingsLayout->addWidget( m_cancelButton, 2, 0 );

    m_okButton = new QPushButton( this, "m_okButton" );

    BaseSnapToGridSettingsLayout->addWidget( m_okButton, 2, 1 );

    Horizontal = new QGroupBox( this, "Horizontal" );
    Horizontal->setColumnLayout(0, Qt::Vertical );
    Horizontal->layout()->setSpacing( 6 );
    Horizontal->layout()->setMargin( 11 );
    HorizontalLayout = new QGridLayout( Horizontal->layout() );
    HorizontalLayout->setAlignment( Qt::AlignTop );

    m_snapToHorizontalGrid = new QCheckBox( Horizontal, "m_snapToHorizontalGrid" );

    HorizontalLayout->addMultiCellWidget( m_snapToHorizontalGrid, 0, 0, 0, 2 );

    m_horizontalSensitivitySpinBox = new QSpinBox( Horizontal, "m_horizontalSensitivitySpinBox" );
    m_horizontalSensitivitySpinBox->setEnabled( FALSE );
    m_horizontalSensitivitySpinBox->setMaxValue( 10 );
    m_horizontalSensitivitySpinBox->setMinValue( 1 );

    HorizontalLayout->addWidget( m_horizontalSensitivitySpinBox, 3, 3 );

    m_horizontalSensitivitySlider = new QSlider( Horizontal, "m_horizontalSensitivitySlider" );
    m_horizontalSensitivitySlider->setEnabled( FALSE );
    m_horizontalSensitivitySlider->setMinValue( 1 );
    m_horizontalSensitivitySlider->setMaxValue( 10 );
    m_horizontalSensitivitySlider->setOrientation( QSlider::Horizontal );
    m_horizontalSensitivitySlider->setTickmarks( QSlider::Left );
    m_horizontalSensitivitySlider->setTickInterval( 1 );

    HorizontalLayout->addMultiCellWidget( m_horizontalSensitivitySlider, 3, 3, 1, 2 );

    TextLabel1 = new QLabel( Horizontal, "TextLabel1" );

    HorizontalLayout->addWidget( TextLabel1, 3, 0 );

    TextLabel2 = new QLabel( Horizontal, "TextLabel2" );

    HorizontalLayout->addMultiCellWidget( TextLabel2, 1, 1, 0, 1 );

    m_horizontalGridSize = new QLineEdit( Horizontal, "m_horizontalGridSize" );
    m_horizontalGridSize->setEnabled( FALSE );

    HorizontalLayout->addMultiCellWidget( m_horizontalGridSize, 1, 1, 2, 3 );

    m_horizontalGridSegments = new QLineEdit( Horizontal, "m_horizontalGridSegments" );
    m_horizontalGridSegments->setEnabled( FALSE );

    HorizontalLayout->addMultiCellWidget( m_horizontalGridSegments, 2, 2, 2, 3 );

    TextLabel3 = new QLabel( Horizontal, "TextLabel3" );

    HorizontalLayout->addMultiCellWidget( TextLabel3, 2, 2, 0, 1 );

    BaseSnapToGridSettingsLayout->addMultiCellWidget( Horizontal, 0, 0, 0, 1 );

    Vertical = new QGroupBox( this, "Vertical" );
    Vertical->setColumnLayout(0, Qt::Vertical );
    Vertical->layout()->setSpacing( 6 );
    Vertical->layout()->setMargin( 11 );
    VerticalLayout = new QGridLayout( Vertical->layout() );
    VerticalLayout->setAlignment( Qt::AlignTop );

    m_snapToVerticalGrid = new QCheckBox( Vertical, "m_snapToVerticalGrid" );

    VerticalLayout->addMultiCellWidget( m_snapToVerticalGrid, 0, 0, 0, 2 );

    TextLabel6 = new QLabel( Vertical, "TextLabel6" );

    VerticalLayout->addWidget( TextLabel6, 3, 0 );

    m_verticalSensitivitySlider = new QSlider( Vertical, "m_verticalSensitivitySlider" );
    m_verticalSensitivitySlider->setEnabled( FALSE );
    m_verticalSensitivitySlider->setMinValue( 1 );
    m_verticalSensitivitySlider->setMaxValue( 10 );
    m_verticalSensitivitySlider->setOrientation( QSlider::Horizontal );
    m_verticalSensitivitySlider->setTickmarks( QSlider::Left );
    m_verticalSensitivitySlider->setTickInterval( 1 );

    VerticalLayout->addMultiCellWidget( m_verticalSensitivitySlider, 3, 3, 1, 2 );

    m_verticalSensitivitySpinBox = new QSpinBox( Vertical, "m_verticalSensitivitySpinBox" );
    m_verticalSensitivitySpinBox->setEnabled( FALSE );
    m_verticalSensitivitySpinBox->setMaxValue( 10 );
    m_verticalSensitivitySpinBox->setMinValue( 1 );

    VerticalLayout->addWidget( m_verticalSensitivitySpinBox, 3, 3 );

    m_verticalGridSegments = new QLineEdit( Vertical, "m_verticalGridSegments" );
    m_verticalGridSegments->setEnabled( FALSE );

    VerticalLayout->addMultiCellWidget( m_verticalGridSegments, 2, 2, 2, 3 );

    m_verticalGridSize = new QLineEdit( Vertical, "m_verticalGridSize" );
    m_verticalGridSize->setEnabled( FALSE );

    VerticalLayout->addMultiCellWidget( m_verticalGridSize, 1, 1, 2, 3 );

    TextLabel5 = new QLabel( Vertical, "TextLabel5" );

    VerticalLayout->addMultiCellWidget( TextLabel5, 1, 1, 0, 1 );

    TextLabel4 = new QLabel( Vertical, "TextLabel4" );

    VerticalLayout->addMultiCellWidget( TextLabel4, 2, 2, 0, 1 );

    BaseSnapToGridSettingsLayout->addMultiCellWidget( Vertical, 1, 1, 0, 1 );
    languageChange();
    resize( QSize(323, 305).expandedTo(minimumSizeHint()) );
    clearWState( WState_Polished );

    // signals and slots connections
    connect( m_verticalSensitivitySlider, SIGNAL( valueChanged(int) ), m_verticalSensitivitySpinBox, SLOT( setValue(int) ) );
    connect( m_horizontalSensitivitySpinBox, SIGNAL( valueChanged(int) ), m_horizontalSensitivitySlider, SLOT( setValue(int) ) );
    connect( m_horizontalSensitivitySlider, SIGNAL( valueChanged(int) ), m_horizontalSensitivitySpinBox, SLOT( setValue(int) ) );
    connect( m_verticalSensitivitySpinBox, SIGNAL( valueChanged(int) ), m_verticalSensitivitySlider, SLOT( setValue(int) ) );
    connect( m_okButton, SIGNAL( clicked() ), this, SLOT( accept() ) );
    connect( m_cancelButton, SIGNAL( clicked() ), this, SLOT( reject() ) );
    connect( m_snapToHorizontalGrid, SIGNAL( toggled(bool) ), m_horizontalGridSize, SLOT( setEnabled(bool) ) );
    connect( m_snapToHorizontalGrid, SIGNAL( toggled(bool) ), m_horizontalGridSegments, SLOT( setEnabled(bool) ) );
    connect( m_snapToHorizontalGrid, SIGNAL( toggled(bool) ), m_horizontalSensitivitySlider, SLOT( setEnabled(bool) ) );
    connect( m_snapToHorizontalGrid, SIGNAL( toggled(bool) ), m_horizontalSensitivitySpinBox, SLOT( setEnabled(bool) ) );
    connect( m_snapToVerticalGrid, SIGNAL( toggled(bool) ), m_verticalGridSize, SLOT( setEnabled(bool) ) );
    connect( m_snapToVerticalGrid, SIGNAL( toggled(bool) ), m_verticalGridSegments, SLOT( setEnabled(bool) ) );
    connect( m_snapToVerticalGrid, SIGNAL( toggled(bool) ), m_verticalSensitivitySlider, SLOT( setEnabled(bool) ) );
    connect( m_snapToVerticalGrid, SIGNAL( toggled(bool) ), m_verticalSensitivitySpinBox, SLOT( setEnabled(bool) ) );
}

/*
 *  Destroys the object and frees any allocated resources
 */
BaseSnapToGridSettings::~BaseSnapToGridSettings()
{
    // no need to delete child widgets, Qt does it all for us
}

/*
 *  Sets the strings of the subwidgets using the current
 *  language.
 */
void BaseSnapToGridSettings::languageChange()
{
    setCaption( tr( "Snap to Grid Settings" ) );
    m_cancelButton->setText( tr( "Cancel" ) );
    m_okButton->setText( tr( "Ok" ) );
    Horizontal->setTitle( tr( "Horizontal" ) );
    m_snapToHorizontalGrid->setText( tr( "Snap To Grid" ) );
    TextLabel1->setText( tr( "Sensitivity" ) );
    TextLabel2->setText( tr( "Grid Block Size" ) );
    TextLabel3->setText( tr( "Horizontal Grid Segments" ) );
    Vertical->setTitle( tr( "Vertical" ) );
    m_snapToVerticalGrid->setText( tr( "Snap To Grid" ) );
    TextLabel6->setText( tr( "Sensitivity" ) );
    TextLabel5->setText( tr( "Grid Block Size" ) );
    TextLabel4->setText( tr( "Vertical Grid Segments" ) );
}

/****************************************************************************
** BaseSnapToGridSettings meta object code from reading C++ file 'BaseSnapToGridSettings.h'
**
** Created: Thu Jul 23 03:18:15 2026
**      by: The Qt MOC ($Id: $)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#undef QT_NO_COMPAT
#include "D:\titan\client\src\build\win32\x64\Release\BaseSnapToGridSettings.h"
#include <qmetaobject.h>
#include <qapplication.h>

#include <private/qucomextra_p.h>
#if !defined(Q_MOC_OUTPUT_REVISION) || (Q_MOC_OUTPUT_REVISION != 26)
#error "This file was generated using the moc from 3.3.4. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

const char *BaseSnapToGridSettings::className() const
{
    return "BaseSnapToGridSettings";
}

QMetaObject *BaseSnapToGridSettings::metaObj = 0;
static QMetaObjectCleanUp cleanUp_BaseSnapToGridSettings( "BaseSnapToGridSettings", &BaseSnapToGridSettings::staticMetaObject );

#ifndef QT_NO_TRANSLATION
QString BaseSnapToGridSettings::tr( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "BaseSnapToGridSettings", s, c, QApplication::DefaultCodec );
    else
	return QString::fromLatin1( s );
}
#ifndef QT_NO_TRANSLATION_UTF8
QString BaseSnapToGridSettings::trUtf8( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "BaseSnapToGridSettings", s, c, QApplication::UnicodeUTF8 );
    else
	return QString::fromUtf8( s );
}
#endif // QT_NO_TRANSLATION_UTF8

#endif // QT_NO_TRANSLATION

QMetaObject* BaseSnapToGridSettings::staticMetaObject()
{
    if ( metaObj )
	return metaObj;
    QMetaObject* parentObject = QDialog::staticMetaObject();
    static const QUMethod slot_0 = {"languageChange", 0, 0 };
    static const QMetaData slot_tbl[] = {
	{ "languageChange()", &slot_0, QMetaData::Protected }
    };
    metaObj = QMetaObject::new_metaobject(
	"BaseSnapToGridSettings", parentObject,
	slot_tbl, 1,
	0, 0,
#ifndef QT_NO_PROPERTIES
	0, 0,
	0, 0,
#endif // QT_NO_PROPERTIES
	0, 0 );
    cleanUp_BaseSnapToGridSettings.setMetaObject( metaObj );
    return metaObj;
}

void* BaseSnapToGridSettings::qt_cast( const char* clname )
{
    if ( !qstrcmp( clname, "BaseSnapToGridSettings" ) )
	return this;
    return QDialog::qt_cast( clname );
}

bool BaseSnapToGridSettings::qt_invoke( int _id, QUObject* _o )
{
    switch ( _id - staticMetaObject()->slotOffset() ) {
    case 0: languageChange(); break;
    default:
	return QDialog::qt_invoke( _id, _o );
    }
    return TRUE;
}

bool BaseSnapToGridSettings::qt_emit( int _id, QUObject* _o )
{
    return QDialog::qt_emit(_id,_o);
}
#ifndef QT_NO_PROPERTIES

bool BaseSnapToGridSettings::qt_property( int id, int f, QVariant* v)
{
    return QDialog::qt_property( id, f, v);
}

bool BaseSnapToGridSettings::qt_static_property( QObject* , int , int , QVariant* ){ return FALSE; }
#endif // QT_NO_PROPERTIES
