/****************************************************************************
** Form implementation generated from reading ui file 'D:\titan\client\src\game\client\application\SwgGodClient\src\shared\ui\BaseStackerTool.ui'
**
** Created: Thu Jul 23 03:18:11 2026
**      by: The User Interface Compiler ($Id: qt/main.cpp   3.3.4   edited Nov 24 2003 $)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#include "D:\titan\client\src\build\win32\x64\Release\BaseStackerTool.h"

#include <qvariant.h>
#include <qpushbutton.h>
#include <qlabel.h>
#include <qspinbox.h>
#include <qcombobox.h>
#include <qlineedit.h>
#include <qcheckbox.h>
#include <qgroupbox.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qwhatsthis.h>

/*
 *  Constructs a BaseStackerTool as a child of 'parent', with the
 *  name 'name' and widget flags set to 'f'.
 */
BaseStackerTool::BaseStackerTool( QWidget* parent, const char* name, WFlags fl )
    : QWidget( parent, name, fl )
{
    if ( !name )
	setName( "BaseStackerTool" );
    BaseStackerToolLayout = new QGridLayout( this, 1, 1, 11, 6, "BaseStackerToolLayout"); 

    m_countLabel = new QLabel( this, "m_countLabel" );

    BaseStackerToolLayout->addWidget( m_countLabel, 0, 0 );

    m_countSpinBox = new QSpinBox( this, "m_countSpinBox" );
    m_countSpinBox->setMinValue( 2 );
    m_countSpinBox->setMaxValue( 100 );
    m_countSpinBox->setValue( 2 );

    BaseStackerToolLayout->addWidget( m_countSpinBox, 0, 1 );

    m_orientationLabel = new QLabel( this, "m_orientationLabel" );

    BaseStackerToolLayout->addWidget( m_orientationLabel, 1, 0 );

    m_orientationCombo = new QComboBox( FALSE, this, "m_orientationCombo" );

    BaseStackerToolLayout->addWidget( m_orientationCombo, 1, 1 );

    m_distanceLabel = new QLabel( this, "m_distanceLabel" );

    BaseStackerToolLayout->addWidget( m_distanceLabel, 2, 0 );

    m_distanceEdit = new QLineEdit( this, "m_distanceEdit" );

    BaseStackerToolLayout->addWidget( m_distanceEdit, 2, 1 );

    m_useMeshExtentCheck = new QCheckBox( this, "m_useMeshExtentCheck" );

    BaseStackerToolLayout->addMultiCellWidget( m_useMeshExtentCheck, 3, 3, 0, 1 );

    m_gimbalGroup = new QGroupBox( this, "m_gimbalGroup" );
    m_gimbalGroup->setAlignment( int( QGroupBox::AlignCenter ) );
    m_gimbalGroup->setColumnLayout(0, Qt::Vertical );
    m_gimbalGroup->layout()->setSpacing( 4 );
    m_gimbalGroup->layout()->setMargin( 6 );
    m_gimbalGroupLayout = new QGridLayout( m_gimbalGroup->layout() );
    m_gimbalGroupLayout->setAlignment( Qt::AlignTop );

    m_gimbalLegend = new QLabel( m_gimbalGroup, "m_gimbalLegend" );
    m_gimbalLegend->setAlignment( int( QLabel::AlignCenter ) );

    m_gimbalGroupLayout->addMultiCellWidget( m_gimbalLegend, 0, 0, 0, 5 );

    BaseStackerToolLayout->addMultiCellWidget( m_gimbalGroup, 4, 4, 0, 1 );

    m_stackButton = new QPushButton( this, "m_stackButton" );

    BaseStackerToolLayout->addMultiCellWidget( m_stackButton, 5, 5, 0, 1 );
    languageChange();
    resize( QSize(260, 220).expandedTo(minimumSizeHint()) );
    clearWState( WState_Polished );
}

/*
 *  Destroys the object and frees any allocated resources
 */
BaseStackerTool::~BaseStackerTool()
{
    // no need to delete child widgets, Qt does it all for us
}

/*
 *  Sets the strings of the subwidgets using the current
 *  language.
 */
void BaseStackerTool::languageChange()
{
    setCaption( tr( "Stacker Tool" ) );
    m_countLabel->setText( tr( "Count:" ) );
    m_orientationLabel->setText( tr( "Orientation:" ) );
    m_orientationCombo->clear();
    m_orientationCombo->insertItem( tr( "+X" ) );
    m_orientationCombo->insertItem( tr( "-X" ) );
    m_orientationCombo->insertItem( tr( "+Y" ) );
    m_orientationCombo->insertItem( tr( "-Y" ) );
    m_orientationCombo->insertItem( tr( "+Z" ) );
    m_orientationCombo->insertItem( tr( "-Z" ) );
    m_distanceLabel->setText( tr( "Distance (m):" ) );
    m_distanceEdit->setText( tr( "0" ) );
    m_useMeshExtentCheck->setText( tr( "Stack against object mesh (extent) - for seamless wall stacking" ) );
    m_gimbalGroup->setTitle( tr( "Axis Legend (Gimbal)" ) );
    m_gimbalLegend->setText( tr( "X (red)  Y (green)  Z (blue)  --  + = forward, - = backward" ) );
    m_stackButton->setText( tr( "Stack" ) );
}

/****************************************************************************
** BaseStackerTool meta object code from reading C++ file 'BaseStackerTool.h'
**
** Created: Thu Jul 23 03:18:11 2026
**      by: The Qt MOC ($Id: $)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#undef QT_NO_COMPAT
#include "D:\titan\client\src\build\win32\x64\Release\BaseStackerTool.h"
#include <qmetaobject.h>
#include <qapplication.h>

#include <private/qucomextra_p.h>
#if !defined(Q_MOC_OUTPUT_REVISION) || (Q_MOC_OUTPUT_REVISION != 26)
#error "This file was generated using the moc from 3.3.4. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

const char *BaseStackerTool::className() const
{
    return "BaseStackerTool";
}

QMetaObject *BaseStackerTool::metaObj = 0;
static QMetaObjectCleanUp cleanUp_BaseStackerTool( "BaseStackerTool", &BaseStackerTool::staticMetaObject );

#ifndef QT_NO_TRANSLATION
QString BaseStackerTool::tr( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "BaseStackerTool", s, c, QApplication::DefaultCodec );
    else
	return QString::fromLatin1( s );
}
#ifndef QT_NO_TRANSLATION_UTF8
QString BaseStackerTool::trUtf8( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "BaseStackerTool", s, c, QApplication::UnicodeUTF8 );
    else
	return QString::fromUtf8( s );
}
#endif // QT_NO_TRANSLATION_UTF8

#endif // QT_NO_TRANSLATION

QMetaObject* BaseStackerTool::staticMetaObject()
{
    if ( metaObj )
	return metaObj;
    QMetaObject* parentObject = QWidget::staticMetaObject();
    static const QUMethod slot_0 = {"languageChange", 0, 0 };
    static const QMetaData slot_tbl[] = {
	{ "languageChange()", &slot_0, QMetaData::Protected }
    };
    metaObj = QMetaObject::new_metaobject(
	"BaseStackerTool", parentObject,
	slot_tbl, 1,
	0, 0,
#ifndef QT_NO_PROPERTIES
	0, 0,
	0, 0,
#endif // QT_NO_PROPERTIES
	0, 0 );
    cleanUp_BaseStackerTool.setMetaObject( metaObj );
    return metaObj;
}

void* BaseStackerTool::qt_cast( const char* clname )
{
    if ( !qstrcmp( clname, "BaseStackerTool" ) )
	return this;
    return QWidget::qt_cast( clname );
}

bool BaseStackerTool::qt_invoke( int _id, QUObject* _o )
{
    switch ( _id - staticMetaObject()->slotOffset() ) {
    case 0: languageChange(); break;
    default:
	return QWidget::qt_invoke( _id, _o );
    }
    return TRUE;
}

bool BaseStackerTool::qt_emit( int _id, QUObject* _o )
{
    return QWidget::qt_emit(_id,_o);
}
#ifndef QT_NO_PROPERTIES

bool BaseStackerTool::qt_property( int id, int f, QVariant* v)
{
    return QWidget::qt_property( id, f, v);
}

bool BaseStackerTool::qt_static_property( QObject* , int , int , QVariant* ){ return FALSE; }
#endif // QT_NO_PROPERTIES
