/****************************************************************************
** Form implementation generated from reading ui file 'D:\titan\client\src\game\client\application\SwgGodClient\src\shared\ui\BaseRegionBrowser.ui'
**
** Created: Thu Jul 23 03:18:15 2026
**      by: The User Interface Compiler ($Id: qt/main.cpp   3.3.4   edited Nov 24 2003 $)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#include "D:\titan\client\src\build\win32\x64\Release\BaseRegionBrowser.h"

#include <qvariant.h>
#include <qpushbutton.h>
#include <qlabel.h>
#include <qcheckbox.h>
#include <qheader.h>
#include <qlistview.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qwhatsthis.h>
#include <qimage.h>
#include <qpixmap.h>

#include "RegionRenderer.h"
/*
 *  Constructs a BaseRegionBrowser as a child of 'parent', with the
 *  name 'name' and widget flags set to 'f'.
 */
BaseRegionBrowser::BaseRegionBrowser( QWidget* parent, const char* name, WFlags fl )
    : QWidget( parent, name, fl )
{
    if ( !name )
	setName( "BaseRegionBrowser" );
    BaseRegionBrowserLayout = new QGridLayout( this, 1, 1, 11, 6, "BaseRegionBrowserLayout"); 

    Layout8 = new QHBoxLayout( 0, 0, 6, "Layout8"); 

    TextLabel1 = new QLabel( this, "TextLabel1" );
    Layout8->addWidget( TextLabel1 );
    Spacer6 = new QSpacerItem( 0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum );
    Layout8->addItem( Spacer6 );

    m_pvpCheckBox = new QCheckBox( this, "m_pvpCheckBox" );
    Layout8->addWidget( m_pvpCheckBox );

    m_municipalCheckBox = new QCheckBox( this, "m_municipalCheckBox" );
    Layout8->addWidget( m_municipalCheckBox );

    m_buildableCheckBox = new QCheckBox( this, "m_buildableCheckBox" );
    Layout8->addWidget( m_buildableCheckBox );

    m_geographicCheckBox = new QCheckBox( this, "m_geographicCheckBox" );
    Layout8->addWidget( m_geographicCheckBox );

    m_difficultyCheckBox = new QCheckBox( this, "m_difficultyCheckBox" );
    Layout8->addWidget( m_difficultyCheckBox );

    m_spawnableCheckBox = new QCheckBox( this, "m_spawnableCheckBox" );
    Layout8->addWidget( m_spawnableCheckBox );

    m_missionCheckBox = new QCheckBox( this, "m_missionCheckBox" );
    Layout8->addWidget( m_missionCheckBox );

    BaseRegionBrowserLayout->addLayout( Layout8, 1, 0 );

    LayoutMapRow = new QHBoxLayout( 0, 0, 6, "LayoutMapRow"); 

    m_regionTree = new QListView( this, "m_regionTree" );
    m_regionTree->setMinimumSize( QSize( 220, 120 ) );
    LayoutMapRow->addWidget( m_regionTree );

    m_regionRenderer = new RegionRenderer( this, "m_regionRenderer" );
    LayoutMapRow->addWidget( m_regionRenderer );

    BaseRegionBrowserLayout->addLayout( LayoutMapRow, 0, 0 );
    languageChange();
    resize( QSize(683, 619).expandedTo(minimumSizeHint()) );
    clearWState( WState_Polished );
}

/*
 *  Destroys the object and frees any allocated resources
 */
BaseRegionBrowser::~BaseRegionBrowser()
{
    // no need to delete child widgets, Qt does it all for us
}

/*
 *  Sets the strings of the subwidgets using the current
 *  language.
 */
void BaseRegionBrowser::languageChange()
{
    setCaption( tr( "RegionBrowser" ) );
    TextLabel1->setText( tr( "Show only: (value != 0)" ) );
    m_pvpCheckBox->setText( tr( "PvP" ) );
    m_municipalCheckBox->setText( tr( "Municipal" ) );
    m_buildableCheckBox->setText( tr( "Buildable" ) );
    m_geographicCheckBox->setText( tr( "Geographical" ) );
    m_difficultyCheckBox->setText( tr( "Difficulty" ) );
    m_spawnableCheckBox->setText( tr( "Spawnable" ) );
    m_missionCheckBox->setText( tr( "Mission" ) );
}

void BaseRegionBrowser::onMissionCheck(bool)
{
    qWarning( "BaseRegionBrowser::onMissionCheck(bool): Not implemented yet" );
}

void BaseRegionBrowser::onBuildableCheck(bool)
{
    qWarning( "BaseRegionBrowser::onBuildableCheck(bool): Not implemented yet" );
}

void BaseRegionBrowser::onDifficultyCheck(bool)
{
    qWarning( "BaseRegionBrowser::onDifficultyCheck(bool): Not implemented yet" );
}

void BaseRegionBrowser::onGeographicCheck(bool)
{
    qWarning( "BaseRegionBrowser::onGeographicCheck(bool): Not implemented yet" );
}

void BaseRegionBrowser::onMunicipalCheck(bool)
{
    qWarning( "BaseRegionBrowser::onMunicipalCheck(bool): Not implemented yet" );
}

void BaseRegionBrowser::onPvPCheck(bool)
{
    qWarning( "BaseRegionBrowser::onPvPCheck(bool): Not implemented yet" );
}

void BaseRegionBrowser::onSpawnableCheck(bool)
{
    qWarning( "BaseRegionBrowser::onSpawnableCheck(bool): Not implemented yet" );
}

/****************************************************************************
** BaseRegionBrowser meta object code from reading C++ file 'BaseRegionBrowser.h'
**
** Created: Thu Jul 23 03:18:15 2026
**      by: The Qt MOC ($Id: $)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#undef QT_NO_COMPAT
#include "D:\titan\client\src\build\win32\x64\Release\BaseRegionBrowser.h"
#include <qmetaobject.h>
#include <qapplication.h>

#include <private/qucomextra_p.h>
#if !defined(Q_MOC_OUTPUT_REVISION) || (Q_MOC_OUTPUT_REVISION != 26)
#error "This file was generated using the moc from 3.3.4. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

const char *BaseRegionBrowser::className() const
{
    return "BaseRegionBrowser";
}

QMetaObject *BaseRegionBrowser::metaObj = 0;
static QMetaObjectCleanUp cleanUp_BaseRegionBrowser( "BaseRegionBrowser", &BaseRegionBrowser::staticMetaObject );

#ifndef QT_NO_TRANSLATION
QString BaseRegionBrowser::tr( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "BaseRegionBrowser", s, c, QApplication::DefaultCodec );
    else
	return QString::fromLatin1( s );
}
#ifndef QT_NO_TRANSLATION_UTF8
QString BaseRegionBrowser::trUtf8( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "BaseRegionBrowser", s, c, QApplication::UnicodeUTF8 );
    else
	return QString::fromUtf8( s );
}
#endif // QT_NO_TRANSLATION_UTF8

#endif // QT_NO_TRANSLATION

QMetaObject* BaseRegionBrowser::staticMetaObject()
{
    if ( metaObj )
	return metaObj;
    QMetaObject* parentObject = QWidget::staticMetaObject();
    static const QUParameter param_slot_0[] = {
	{ 0, &static_QUType_bool, 0, QUParameter::In }
    };
    static const QUMethod slot_0 = {"onMissionCheck", 1, param_slot_0 };
    static const QUParameter param_slot_1[] = {
	{ 0, &static_QUType_bool, 0, QUParameter::In }
    };
    static const QUMethod slot_1 = {"onBuildableCheck", 1, param_slot_1 };
    static const QUParameter param_slot_2[] = {
	{ 0, &static_QUType_bool, 0, QUParameter::In }
    };
    static const QUMethod slot_2 = {"onDifficultyCheck", 1, param_slot_2 };
    static const QUParameter param_slot_3[] = {
	{ 0, &static_QUType_bool, 0, QUParameter::In }
    };
    static const QUMethod slot_3 = {"onGeographicCheck", 1, param_slot_3 };
    static const QUParameter param_slot_4[] = {
	{ 0, &static_QUType_bool, 0, QUParameter::In }
    };
    static const QUMethod slot_4 = {"onMunicipalCheck", 1, param_slot_4 };
    static const QUParameter param_slot_5[] = {
	{ 0, &static_QUType_bool, 0, QUParameter::In }
    };
    static const QUMethod slot_5 = {"onPvPCheck", 1, param_slot_5 };
    static const QUParameter param_slot_6[] = {
	{ 0, &static_QUType_bool, 0, QUParameter::In }
    };
    static const QUMethod slot_6 = {"onSpawnableCheck", 1, param_slot_6 };
    static const QUMethod slot_7 = {"languageChange", 0, 0 };
    static const QMetaData slot_tbl[] = {
	{ "onMissionCheck(bool)", &slot_0, QMetaData::Public },
	{ "onBuildableCheck(bool)", &slot_1, QMetaData::Public },
	{ "onDifficultyCheck(bool)", &slot_2, QMetaData::Public },
	{ "onGeographicCheck(bool)", &slot_3, QMetaData::Public },
	{ "onMunicipalCheck(bool)", &slot_4, QMetaData::Public },
	{ "onPvPCheck(bool)", &slot_5, QMetaData::Public },
	{ "onSpawnableCheck(bool)", &slot_6, QMetaData::Public },
	{ "languageChange()", &slot_7, QMetaData::Protected }
    };
    metaObj = QMetaObject::new_metaobject(
	"BaseRegionBrowser", parentObject,
	slot_tbl, 8,
	0, 0,
#ifndef QT_NO_PROPERTIES
	0, 0,
	0, 0,
#endif // QT_NO_PROPERTIES
	0, 0 );
    cleanUp_BaseRegionBrowser.setMetaObject( metaObj );
    return metaObj;
}

void* BaseRegionBrowser::qt_cast( const char* clname )
{
    if ( !qstrcmp( clname, "BaseRegionBrowser" ) )
	return this;
    return QWidget::qt_cast( clname );
}

bool BaseRegionBrowser::qt_invoke( int _id, QUObject* _o )
{
    switch ( _id - staticMetaObject()->slotOffset() ) {
    case 0: onMissionCheck((bool)static_QUType_bool.get(_o+1)); break;
    case 1: onBuildableCheck((bool)static_QUType_bool.get(_o+1)); break;
    case 2: onDifficultyCheck((bool)static_QUType_bool.get(_o+1)); break;
    case 3: onGeographicCheck((bool)static_QUType_bool.get(_o+1)); break;
    case 4: onMunicipalCheck((bool)static_QUType_bool.get(_o+1)); break;
    case 5: onPvPCheck((bool)static_QUType_bool.get(_o+1)); break;
    case 6: onSpawnableCheck((bool)static_QUType_bool.get(_o+1)); break;
    case 7: languageChange(); break;
    default:
	return QWidget::qt_invoke( _id, _o );
    }
    return TRUE;
}

bool BaseRegionBrowser::qt_emit( int _id, QUObject* _o )
{
    return QWidget::qt_emit(_id,_o);
}
#ifndef QT_NO_PROPERTIES

bool BaseRegionBrowser::qt_property( int id, int f, QVariant* v)
{
    return QWidget::qt_property( id, f, v);
}

bool BaseRegionBrowser::qt_static_property( QObject* , int , int , QVariant* ){ return FALSE; }
#endif // QT_NO_PROPERTIES
