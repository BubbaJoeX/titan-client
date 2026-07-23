/****************************************************************************
** Form implementation generated from reading ui file 'D:\titan\client\src\game\client\application\SwgGodClient\src\shared\ui\BaseFilterWindow.ui'
**
** Created: Thu Jul 23 03:18:12 2026
**      by: The User Interface Compiler ($Id: qt/main.cpp   3.3.4   edited Nov 24 2003 $)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#include "D:\titan\client\src\build\win32\x64\Release\BaseFilterWindow.h"

#include <qvariant.h>
#include <qpushbutton.h>
#include <qtabwidget.h>
#include <qcheckbox.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qwhatsthis.h>
#include <qimage.h>
#include <qpixmap.h>

#include "ServerTemplateListView.h"
#include "ClientTemplateListView.h"
/*
 *  Constructs a BaseFilterWindow as a child of 'parent', with the
 *  name 'name' and widget flags set to 'f'.
 */
BaseFilterWindow::BaseFilterWindow( QWidget* parent, const char* name, WFlags fl )
    : QWidget( parent, name, fl )
{
    if ( !name )
	setName( "BaseFilterWindow" );
    BaseFilterWindowLayout = new QGridLayout( this, 1, 1, 4, 2, "BaseFilterWindowLayout"); 

    m_tabWidget = new QTabWidget( this, "m_tabWidget" );

    NetworkIdTab = new QWidget( m_tabWidget, "NetworkIdTab" );
    NetworkIdTabLayout = new QGridLayout( NetworkIdTab, 1, 1, 11, 6, "NetworkIdTabLayout"); 

    m_networkIdFilterCheck = new QCheckBox( NetworkIdTab, "m_networkIdFilterCheck" );

    NetworkIdTabLayout->addMultiCellWidget( m_networkIdFilterCheck, 0, 0, 0, 1 );

    TextLabel1_2 = new QLabel( NetworkIdTab, "TextLabel1_2" );

    NetworkIdTabLayout->addWidget( TextLabel1_2, 2, 0 );

    TextLabel1 = new QLabel( NetworkIdTab, "TextLabel1" );

    NetworkIdTabLayout->addWidget( TextLabel1, 1, 0 );

    m_networkIdLowerBoundEdit = new QLineEdit( NetworkIdTab, "m_networkIdLowerBoundEdit" );
    m_networkIdLowerBoundEdit->setEnabled( FALSE );

    NetworkIdTabLayout->addWidget( m_networkIdLowerBoundEdit, 1, 1 );

    m_networkIdUpperBoundEdit = new QLineEdit( NetworkIdTab, "m_networkIdUpperBoundEdit" );
    m_networkIdUpperBoundEdit->setEnabled( FALSE );

    NetworkIdTabLayout->addWidget( m_networkIdUpperBoundEdit, 2, 1 );
    m_tabWidget->insertTab( NetworkIdTab, QString::fromLatin1("") );

    DistanceTab = new QWidget( m_tabWidget, "DistanceTab" );
    DistanceTabLayout = new QGridLayout( DistanceTab, 1, 1, 11, 6, "DistanceTabLayout"); 

    m_radiusFilterCheck = new QCheckBox( DistanceTab, "m_radiusFilterCheck" );

    DistanceTabLayout->addMultiCellWidget( m_radiusFilterCheck, 0, 0, 0, 2 );

    m_maxDistanceEdit = new QLineEdit( DistanceTab, "m_maxDistanceEdit" );
    m_maxDistanceEdit->setEnabled( FALSE );

    DistanceTabLayout->addWidget( m_maxDistanceEdit, 1, 1 );

    TextLabel2_3 = new QLabel( DistanceTab, "TextLabel2_3" );

    DistanceTabLayout->addWidget( TextLabel2_3, 1, 0 );

    TextLabel3 = new QLabel( DistanceTab, "TextLabel3" );

    DistanceTabLayout->addWidget( TextLabel3, 2, 0 );

    m_minDistanceEdit = new QLineEdit( DistanceTab, "m_minDistanceEdit" );
    m_minDistanceEdit->setEnabled( FALSE );

    DistanceTabLayout->addWidget( m_minDistanceEdit, 2, 1 );

    TextLabel2_2 = new QLabel( DistanceTab, "TextLabel2_2" );

    DistanceTabLayout->addWidget( TextLabel2_2, 2, 2 );

    TextLabel2 = new QLabel( DistanceTab, "TextLabel2" );

    DistanceTabLayout->addWidget( TextLabel2, 1, 2 );
    m_tabWidget->insertTab( DistanceTab, QString::fromLatin1("") );

    tab = new QWidget( m_tabWidget, "tab" );
    tabLayout = new QGridLayout( tab, 1, 1, 11, 6, "tabLayout"); 

    m_clientTemplateListView = new ClientTemplateListView( tab, "m_clientTemplateListView" );
    m_clientTemplateListView->setEnabled( FALSE );

    tabLayout->addWidget( m_clientTemplateListView, 3, 0 );

    m_serverTemplateListView = new ServerTemplateListView( tab, "m_serverTemplateListView" );
    m_serverTemplateListView->setEnabled( FALSE );

    tabLayout->addWidget( m_serverTemplateListView, 1, 0 );

    m_objectIdFilterCheck_3_2 = new QCheckBox( tab, "m_objectIdFilterCheck_3_2" );

    tabLayout->addWidget( m_objectIdFilterCheck_3_2, 2, 0 );

    m_objectIdFilterCheck_3 = new QCheckBox( tab, "m_objectIdFilterCheck_3" );

    tabLayout->addWidget( m_objectIdFilterCheck_3, 0, 0 );
    m_tabWidget->insertTab( tab, QString::fromLatin1("") );

    BaseFilterWindowLayout->addWidget( m_tabWidget, 0, 0 );
    languageChange();
    resize( QSize(337, 290).expandedTo(minimumSizeHint()) );
    clearWState( WState_Polished );

    // signals and slots connections
    connect( m_networkIdFilterCheck, SIGNAL( toggled(bool) ), m_networkIdLowerBoundEdit, SLOT( setEnabled(bool) ) );
    connect( m_networkIdFilterCheck, SIGNAL( toggled(bool) ), m_networkIdUpperBoundEdit, SLOT( setEnabled(bool) ) );
    connect( m_objectIdFilterCheck_3_2, SIGNAL( toggled(bool) ), m_clientTemplateListView, SLOT( setEnabled(bool) ) );
    connect( m_objectIdFilterCheck_3, SIGNAL( toggled(bool) ), m_serverTemplateListView, SLOT( setEnabled(bool) ) );
    connect( m_radiusFilterCheck, SIGNAL( toggled(bool) ), m_maxDistanceEdit, SLOT( setEnabled(bool) ) );
    connect( m_radiusFilterCheck, SIGNAL( toggled(bool) ), m_minDistanceEdit, SLOT( setEnabled(bool) ) );
}

/*
 *  Destroys the object and frees any allocated resources
 */
BaseFilterWindow::~BaseFilterWindow()
{
    // no need to delete child widgets, Qt does it all for us
}

/*
 *  Sets the strings of the subwidgets using the current
 *  language.
 */
void BaseFilterWindow::languageChange()
{
    setCaption( tr( "Filter Settings" ) );
    m_networkIdFilterCheck->setText( tr( "Filter by NetworkId" ) );
    TextLabel1_2->setText( tr( "ID Upper Bound" ) );
    TextLabel1->setText( tr( "ID Lower Bound" ) );
    m_tabWidget->changeTab( NetworkIdTab, tr( "NetworkId" ) );
    m_radiusFilterCheck->setText( tr( "Filter by Distance from Avatar" ) );
    TextLabel2_3->setText( tr( "Within" ) );
    TextLabel3->setText( tr( "Further" ) );
    TextLabel2_2->setText( tr( "meters from avatar" ) );
    TextLabel2->setText( tr( "meters of avatar" ) );
    m_tabWidget->changeTab( DistanceTab, tr( "Distance" ) );
    m_objectIdFilterCheck_3_2->setText( tr( "Filter by the ClientObjectTemplate" ) );
    m_objectIdFilterCheck_3->setText( tr( "Filter by the ServerObjectTemplate" ) );
    m_tabWidget->changeTab( tab, tr( "Object Type" ) );
}

/****************************************************************************
** BaseFilterWindow meta object code from reading C++ file 'BaseFilterWindow.h'
**
** Created: Thu Jul 23 03:18:12 2026
**      by: The Qt MOC ($Id: $)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#undef QT_NO_COMPAT
#include "D:\titan\client\src\build\win32\x64\Release\BaseFilterWindow.h"
#include <qmetaobject.h>
#include <qapplication.h>

#include <private/qucomextra_p.h>
#if !defined(Q_MOC_OUTPUT_REVISION) || (Q_MOC_OUTPUT_REVISION != 26)
#error "This file was generated using the moc from 3.3.4. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

const char *BaseFilterWindow::className() const
{
    return "BaseFilterWindow";
}

QMetaObject *BaseFilterWindow::metaObj = 0;
static QMetaObjectCleanUp cleanUp_BaseFilterWindow( "BaseFilterWindow", &BaseFilterWindow::staticMetaObject );

#ifndef QT_NO_TRANSLATION
QString BaseFilterWindow::tr( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "BaseFilterWindow", s, c, QApplication::DefaultCodec );
    else
	return QString::fromLatin1( s );
}
#ifndef QT_NO_TRANSLATION_UTF8
QString BaseFilterWindow::trUtf8( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "BaseFilterWindow", s, c, QApplication::UnicodeUTF8 );
    else
	return QString::fromUtf8( s );
}
#endif // QT_NO_TRANSLATION_UTF8

#endif // QT_NO_TRANSLATION

QMetaObject* BaseFilterWindow::staticMetaObject()
{
    if ( metaObj )
	return metaObj;
    QMetaObject* parentObject = QWidget::staticMetaObject();
    static const QUMethod slot_0 = {"languageChange", 0, 0 };
    static const QMetaData slot_tbl[] = {
	{ "languageChange()", &slot_0, QMetaData::Protected }
    };
    metaObj = QMetaObject::new_metaobject(
	"BaseFilterWindow", parentObject,
	slot_tbl, 1,
	0, 0,
#ifndef QT_NO_PROPERTIES
	0, 0,
	0, 0,
#endif // QT_NO_PROPERTIES
	0, 0 );
    cleanUp_BaseFilterWindow.setMetaObject( metaObj );
    return metaObj;
}

void* BaseFilterWindow::qt_cast( const char* clname )
{
    if ( !qstrcmp( clname, "BaseFilterWindow" ) )
	return this;
    return QWidget::qt_cast( clname );
}

bool BaseFilterWindow::qt_invoke( int _id, QUObject* _o )
{
    switch ( _id - staticMetaObject()->slotOffset() ) {
    case 0: languageChange(); break;
    default:
	return QWidget::qt_invoke( _id, _o );
    }
    return TRUE;
}

bool BaseFilterWindow::qt_emit( int _id, QUObject* _o )
{
    return QWidget::qt_emit(_id,_o);
}
#ifndef QT_NO_PROPERTIES

bool BaseFilterWindow::qt_property( int id, int f, QVariant* v)
{
    return QWidget::qt_property( id, f, v);
}

bool BaseFilterWindow::qt_static_property( QObject* , int , int , QVariant* ){ return FALSE; }
#endif // QT_NO_PROPERTIES
