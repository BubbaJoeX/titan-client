/****************************************************************************
** Form implementation generated from reading ui file 'D:\titan\client\src\game\client\application\SwgGodClient\src\shared\ui\BaseGameWindow.ui'
**
** Created: Thu Jul 23 03:18:13 2026
**      by: The User Interface Compiler ($Id: qt/main.cpp   3.3.4   edited Nov 24 2003 $)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#include "D:\titan\client\src\build\win32\x64\Release\BaseGameWindow.h"

#include <qvariant.h>
#include <qpushbutton.h>
#include <qlcdnumber.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qwhatsthis.h>
#include <qimage.h>
#include <qpixmap.h>

#include "GameWidget.h"
/*
 *  Constructs a BaseGameWindow as a child of 'parent', with the
 *  name 'name' and widget flags set to 'f'.
 */
BaseGameWindow::BaseGameWindow( QWidget* parent, const char* name, WFlags fl )
    : QWidget( parent, name, fl )
{
    if ( !name )
	setName( "BaseGameWindow" );
    setIcon( QPixmap::fromMimeSource( "hi16_action_drop_to_terrain" ) );
    BaseGameWindowLayout = new QGridLayout( this, 1, 1, 4, 3, "BaseGameWindowLayout"); 
    Spacer4 = new QSpacerItem( 20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum );
    BaseGameWindowLayout->addItem( Spacer4, 1, 0 );
    Spacer4_2 = new QSpacerItem( 20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum );
    BaseGameWindowLayout->addItem( Spacer4_2, 1, 2 );
    Spacer1 = new QSpacerItem( 20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding );
    BaseGameWindowLayout->addItem( Spacer1, 3, 1 );
    Spacer1_2 = new QSpacerItem( 20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding );
    BaseGameWindowLayout->addItem( Spacer1_2, 0, 1 );

    m_gameWidget = new GameWidget( this, "m_gameWidget" );
    m_gameWidget->setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)1, (QSizePolicy::SizeType)1, 0, 0, m_gameWidget->sizePolicy().hasHeightForWidth() ) );
    m_gameWidget->setMinimumSize( QSize( 800, 600 ) );
    m_gameWidget->setMaximumSize( QSize( 800, 600 ) );
    m_gameWidget->setAcceptDrops( TRUE );

    BaseGameWindowLayout->addWidget( m_gameWidget, 1, 1 );

    layout3 = new QHBoxLayout( 0, 0, 3, "layout3"); 

    m_fpsLCD = new QLCDNumber( this, "m_fpsLCD" );
    m_fpsLCD->setSegmentStyle( QLCDNumber::Flat );
    m_fpsLCD->setProperty( "intValue", 42 );
    layout3->addWidget( m_fpsLCD );

    m_gameButton = new QPushButton( this, "m_gameButton" );
    m_gameButton->setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)1, (QSizePolicy::SizeType)1, 0, 0, m_gameButton->sizePolicy().hasHeightForWidth() ) );
    m_gameButton->setToggleButton( TRUE );
    m_gameButton->setOn( FALSE );
    layout3->addWidget( m_gameButton );

    m_focusLabel = new QLabel( this, "m_focusLabel" );
    m_focusLabel->setFrameShape( QLabel::Box );
    m_focusLabel->setFrameShadow( QLabel::Raised );
    m_focusLabel->setMargin( 1 );
    layout3->addWidget( m_focusLabel );

    m_distanceLabel = new QLabel( this, "m_distanceLabel" );
    QFont m_distanceLabel_font(  m_distanceLabel->font() );
    m_distanceLabel_font.setFamily( "Courier New" );
    m_distanceLabel->setFont( m_distanceLabel_font ); 
    m_distanceLabel->setFrameShape( QLabel::Box );
    m_distanceLabel->setFrameShadow( QLabel::Raised );
    m_distanceLabel->setMidLineWidth( 0 );
    layout3->addWidget( m_distanceLabel );

    m_terrainGameStatusPanel = new QWidget( this, "m_terrainGameStatusPanel" );
    m_terrainGameStatusPanelLayout = new QHBoxLayout( m_terrainGameStatusPanel, 4, 2, "m_terrainGameStatusPanelLayout"); 
    m_terrainGameStatusSpacerLeft = new QSpacerItem( 40, 8, QSizePolicy::Expanding, QSizePolicy::Minimum );
    m_terrainGameStatusPanelLayout->addItem( m_terrainGameStatusSpacerLeft );

    m_terrainGameStatusLabel = new QLabel( m_terrainGameStatusPanel, "m_terrainGameStatusLabel" );
    m_terrainGameStatusLabel->setFrameShape( QLabel::NoFrame );
    m_terrainGameStatusLabel->setFrameShadow( QLabel::Plain );
    m_terrainGameStatusLabel->setMargin( 0 );
    m_terrainGameStatusPanelLayout->addWidget( m_terrainGameStatusLabel );

    m_clearRegionTerrainToolsButton = new QPushButton( m_terrainGameStatusPanel, "m_clearRegionTerrainToolsButton" );
    m_terrainGameStatusPanelLayout->addWidget( m_clearRegionTerrainToolsButton );
    m_terrainGameStatusSpacerRight = new QSpacerItem( 40, 8, QSizePolicy::Expanding, QSizePolicy::Minimum );
    m_terrainGameStatusPanelLayout->addItem( m_terrainGameStatusSpacerRight );
    layout3->addWidget( m_terrainGameStatusPanel );

    m_buildoutRegionLabel = new QLabel( this, "m_buildoutRegionLabel" );
    m_buildoutRegionLabel->setFrameShape( QLabel::Box );
    m_buildoutRegionLabel->setFrameShadow( QLabel::Raised );
    m_buildoutRegionLabel->setMargin( 1 );
    layout3->addWidget( m_buildoutRegionLabel );

    m_positionLabel = new QLabel( this, "m_positionLabel" );
    m_positionLabel->setFrameShape( QLabel::Box );
    m_positionLabel->setFrameShadow( QLabel::Raised );
    layout3->addWidget( m_positionLabel );

    BaseGameWindowLayout->addLayout( layout3, 2, 1 );
    languageChange();
    resize( QSize(886, 710).expandedTo(minimumSizeHint()) );
    clearWState( WState_Polished );
}

/*
 *  Destroys the object and frees any allocated resources
 */
BaseGameWindow::~BaseGameWindow()
{
    // no need to delete child widgets, Qt does it all for us
}

/*
 *  Sets the strings of the subwidgets using the current
 *  language.
 */
void BaseGameWindow::languageChange()
{
    setCaption( tr( "Game Window" ) );
    m_gameButton->setText( tr( "&Game" ) );
    m_focusLabel->setText( tr( "Game Focus" ) );
    m_distanceLabel->setText( tr( "Distance: 0000.00" ) );
    m_terrainGameStatusLabel->setText( trUtf8( "\xe2\x80\x94" ) );
    m_clearRegionTerrainToolsButton->setText( tr( "Clear" ) );
    m_buildoutRegionLabel->setText( tr( "Buildout Region: <unknown>" ) );
    m_positionLabel->setText( tr( "Position: (0.0, 0.0, 0.0)" ) );
}

/****************************************************************************
** BaseGameWindow meta object code from reading C++ file 'BaseGameWindow.h'
**
** Created: Thu Jul 23 03:18:13 2026
**      by: The Qt MOC ($Id: $)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#undef QT_NO_COMPAT
#include "D:\titan\client\src\build\win32\x64\Release\BaseGameWindow.h"
#include <qmetaobject.h>
#include <qapplication.h>

#include <private/qucomextra_p.h>
#if !defined(Q_MOC_OUTPUT_REVISION) || (Q_MOC_OUTPUT_REVISION != 26)
#error "This file was generated using the moc from 3.3.4. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

const char *BaseGameWindow::className() const
{
    return "BaseGameWindow";
}

QMetaObject *BaseGameWindow::metaObj = 0;
static QMetaObjectCleanUp cleanUp_BaseGameWindow( "BaseGameWindow", &BaseGameWindow::staticMetaObject );

#ifndef QT_NO_TRANSLATION
QString BaseGameWindow::tr( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "BaseGameWindow", s, c, QApplication::DefaultCodec );
    else
	return QString::fromLatin1( s );
}
#ifndef QT_NO_TRANSLATION_UTF8
QString BaseGameWindow::trUtf8( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "BaseGameWindow", s, c, QApplication::UnicodeUTF8 );
    else
	return QString::fromUtf8( s );
}
#endif // QT_NO_TRANSLATION_UTF8

#endif // QT_NO_TRANSLATION

QMetaObject* BaseGameWindow::staticMetaObject()
{
    if ( metaObj )
	return metaObj;
    QMetaObject* parentObject = QWidget::staticMetaObject();
    static const QUMethod slot_0 = {"languageChange", 0, 0 };
    static const QMetaData slot_tbl[] = {
	{ "languageChange()", &slot_0, QMetaData::Protected }
    };
    metaObj = QMetaObject::new_metaobject(
	"BaseGameWindow", parentObject,
	slot_tbl, 1,
	0, 0,
#ifndef QT_NO_PROPERTIES
	0, 0,
	0, 0,
#endif // QT_NO_PROPERTIES
	0, 0 );
    cleanUp_BaseGameWindow.setMetaObject( metaObj );
    return metaObj;
}

void* BaseGameWindow::qt_cast( const char* clname )
{
    if ( !qstrcmp( clname, "BaseGameWindow" ) )
	return this;
    return QWidget::qt_cast( clname );
}

bool BaseGameWindow::qt_invoke( int _id, QUObject* _o )
{
    switch ( _id - staticMetaObject()->slotOffset() ) {
    case 0: languageChange(); break;
    default:
	return QWidget::qt_invoke( _id, _o );
    }
    return TRUE;
}

bool BaseGameWindow::qt_emit( int _id, QUObject* _o )
{
    return QWidget::qt_emit(_id,_o);
}
#ifndef QT_NO_PROPERTIES

bool BaseGameWindow::qt_property( int id, int f, QVariant* v)
{
    return QWidget::qt_property( id, f, v);
}

bool BaseGameWindow::qt_static_property( QObject* , int , int , QVariant* ){ return FALSE; }
#endif // QT_NO_PROPERTIES
