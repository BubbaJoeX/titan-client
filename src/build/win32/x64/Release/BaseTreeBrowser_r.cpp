/****************************************************************************
** Form implementation generated from reading ui file 'D:\titan\client\src\game\client\application\SwgGodClient\src\shared\ui\BaseTreeBrowser.ui'
**
** Created: Thu Jul 23 03:18:16 2026
**      by: The User Interface Compiler ($Id: qt/main.cpp   3.3.4   edited Nov 24 2003 $)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#include "D:\titan\client\src\build\win32\x64\Release\BaseTreeBrowser.h"

#include <qvariant.h>
#include <qpushbutton.h>
#include <qtabwidget.h>
#include <qheader.h>
#include <qlistview.h>
#include <qlineedit.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qwhatsthis.h>
#include <qimage.h>
#include <qpixmap.h>

#include "ScriptListView.h"
#include "ServerTemplateListView.h"
#include "ClientTemplateListView.h"
#include "BuildoutAreaListView.h"
/*
 *  Constructs a BaseTreeBrowser as a child of 'parent', with the
 *  name 'name' and widget flags set to 'f'.
 */
BaseTreeBrowser::BaseTreeBrowser( QWidget* parent, const char* name, WFlags fl )
    : QWidget( parent, name, fl )
{
    if ( !name )
	setName( "BaseTreeBrowser" );
    setIcon( QPixmap::fromMimeSource( "hi16_action_edit" ) );
    BaseTreeBrowserLayout = new QGridLayout( this, 1, 1, 11, 2, "BaseTreeBrowserLayout"); 

    m_templateTabs = new QTabWidget( this, "m_templateTabs" );
    m_templateTabs->setTabShape( QTabWidget::Rounded );

    ObjectTab = new QWidget( m_templateTabs, "ObjectTab" );
    ObjectTabLayout = new QGridLayout( ObjectTab, 1, 1, 4, 2, "ObjectTabLayout"); 

    m_objectList = new QListView( ObjectTab, "m_objectList" );
    m_objectList->addColumn( tr( "Name" ) );
    m_objectList->addColumn( tr( "Range" ) );
    m_objectList->setMargin( 2 );
    m_objectList->setSelectionMode( QListView::Extended );
    m_objectList->setAllColumnsShowFocus( TRUE );
    m_objectList->setShowSortIndicator( TRUE );
    m_objectList->setItemMargin( 2 );
    m_objectList->setRootIsDecorated( TRUE );
    m_objectList->setResizeMode( QListView::AllColumns );

    ObjectTabLayout->addMultiCellWidget( m_objectList, 1, 1, 0, 4 );

    m_refreshButton = new QPushButton( ObjectTab, "m_refreshButton" );

    ObjectTabLayout->addWidget( m_refreshButton, 0, 0 );

    m_objectSearchEdit = new QLineEdit( ObjectTab, "m_objectSearchEdit" );

    ObjectTabLayout->addWidget( m_objectSearchEdit, 0, 1 );

    m_objectSearchClearButton = new QPushButton( ObjectTab, "m_objectSearchClearButton" );
    m_objectSearchClearButton->setMaximumSize( QSize( 24, 32767 ) );

    ObjectTabLayout->addWidget( m_objectSearchClearButton, 0, 2 );
    objectsSpacer = new QSpacerItem( 20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum );
    ObjectTabLayout->addItem( objectsSpacer, 0, 3 );
    m_templateTabs->insertTab( ObjectTab, QString::fromLatin1("") );

    ScriptsTab = new QWidget( m_templateTabs, "ScriptsTab" );
    ScriptsTabLayout = new QGridLayout( ScriptsTab, 1, 1, 4, 2, "ScriptsTabLayout"); 

    m_scriptRefreshButton = new QPushButton( ScriptsTab, "m_scriptRefreshButton" );

    ScriptsTabLayout->addWidget( m_scriptRefreshButton, 0, 0 );

    m_scriptSearchEdit = new QLineEdit( ScriptsTab, "m_scriptSearchEdit" );

    ScriptsTabLayout->addWidget( m_scriptSearchEdit, 0, 1 );

    m_scriptSearchClearButton = new QPushButton( ScriptsTab, "m_scriptSearchClearButton" );
    m_scriptSearchClearButton->setMaximumSize( QSize( 24, 32767 ) );

    ScriptsTabLayout->addWidget( m_scriptSearchClearButton, 0, 2 );
    scriptsSpacer = new QSpacerItem( 20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum );
    ScriptsTabLayout->addItem( scriptsSpacer, 0, 3 );

    m_scriptList = new ScriptListView( ScriptsTab, "m_scriptList" );

    ScriptsTabLayout->addMultiCellWidget( m_scriptList, 1, 1, 0, 3 );
    m_templateTabs->insertTab( ScriptsTab, QString::fromLatin1("") );

    ServerTemplatesTab = new QWidget( m_templateTabs, "ServerTemplatesTab" );
    ServerTemplatesTabLayout = new QGridLayout( ServerTemplatesTab, 1, 1, 4, 2, "ServerTemplatesTabLayout"); 

    m_serverTemplateRefreshButton = new QPushButton( ServerTemplatesTab, "m_serverTemplateRefreshButton" );

    ServerTemplatesTabLayout->addWidget( m_serverTemplateRefreshButton, 0, 0 );

    m_serverTemplateSearchEdit = new QLineEdit( ServerTemplatesTab, "m_serverTemplateSearchEdit" );

    ServerTemplatesTabLayout->addWidget( m_serverTemplateSearchEdit, 0, 1 );

    m_serverTemplateSearchClearButton = new QPushButton( ServerTemplatesTab, "m_serverTemplateSearchClearButton" );
    m_serverTemplateSearchClearButton->setMaximumSize( QSize( 24, 32767 ) );

    ServerTemplatesTabLayout->addWidget( m_serverTemplateSearchClearButton, 0, 2 );
    serverTemplateSpacer = new QSpacerItem( 20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum );
    ServerTemplatesTabLayout->addItem( serverTemplateSpacer, 0, 3 );

    m_serverTemplateList = new ServerTemplateListView( ServerTemplatesTab, "m_serverTemplateList" );

    ServerTemplatesTabLayout->addMultiCellWidget( m_serverTemplateList, 1, 1, 0, 3 );
    m_templateTabs->insertTab( ServerTemplatesTab, QString::fromLatin1("") );

    ClientTemplatesTab = new QWidget( m_templateTabs, "ClientTemplatesTab" );
    ClientTemplatesTabLayout = new QGridLayout( ClientTemplatesTab, 1, 1, 4, 2, "ClientTemplatesTabLayout"); 

    m_clientTemplateRefreshButton = new QPushButton( ClientTemplatesTab, "m_clientTemplateRefreshButton" );

    ClientTemplatesTabLayout->addWidget( m_clientTemplateRefreshButton, 0, 0 );

    m_clientTemplateSearchEdit = new QLineEdit( ClientTemplatesTab, "m_clientTemplateSearchEdit" );

    ClientTemplatesTabLayout->addWidget( m_clientTemplateSearchEdit, 0, 1 );

    m_clientTemplateSearchClearButton = new QPushButton( ClientTemplatesTab, "m_clientTemplateSearchClearButton" );
    m_clientTemplateSearchClearButton->setMaximumSize( QSize( 24, 32767 ) );

    ClientTemplatesTabLayout->addWidget( m_clientTemplateSearchClearButton, 0, 2 );
    clientTemplateSpacer = new QSpacerItem( 20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum );
    ClientTemplatesTabLayout->addItem( clientTemplateSpacer, 0, 3 );

    m_clientTemplateList = new ClientTemplateListView( ClientTemplatesTab, "m_clientTemplateList" );

    ClientTemplatesTabLayout->addMultiCellWidget( m_clientTemplateList, 1, 1, 0, 3 );
    m_templateTabs->insertTab( ClientTemplatesTab, QString::fromLatin1("") );

    BuildoutAreasTab = new QWidget( m_templateTabs, "BuildoutAreasTab" );
    BuildoutAreasTabLayout = new QGridLayout( BuildoutAreasTab, 1, 1, 4, 2, "BuildoutAreasTabLayout"); 

    m_buildoutAreaRefreshButton = new QPushButton( BuildoutAreasTab, "m_buildoutAreaRefreshButton" );

    BuildoutAreasTabLayout->addWidget( m_buildoutAreaRefreshButton, 0, 0 );
    buildoutAreaSpacer = new QSpacerItem( 20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum );
    BuildoutAreasTabLayout->addItem( buildoutAreaSpacer, 0, 1 );

    m_buildoutAreaList = new BuildoutAreaListView( BuildoutAreasTab, "m_buildoutAreaList" );

    BuildoutAreasTabLayout->addMultiCellWidget( m_buildoutAreaList, 1, 1, 0, 1 );
    m_templateTabs->insertTab( BuildoutAreasTab, QString::fromLatin1("") );

    BaseTreeBrowserLayout->addWidget( m_templateTabs, 0, 0 );
    languageChange();
    resize( QSize(472, 327).expandedTo(minimumSizeHint()) );
    clearWState( WState_Polished );
}

/*
 *  Destroys the object and frees any allocated resources
 */
BaseTreeBrowser::~BaseTreeBrowser()
{
    // no need to delete child widgets, Qt does it all for us
}

/*
 *  Sets the strings of the subwidgets using the current
 *  language.
 */
void BaseTreeBrowser::languageChange()
{
    setCaption( tr( "Tree Browser" ) );
    m_objectList->header()->setLabel( 0, tr( "Name" ) );
    m_objectList->header()->setLabel( 1, tr( "Range" ) );
    m_objectList->clear();
    QListViewItem * item = new QListViewItem( m_objectList, 0 );
    item->setText( 0, tr( "New Item" ) );

    QWhatsThis::add( m_objectList, tr( "The object tree allows you to see what objects are in the\n"
"world.  You may select one or may objects in the tree in\n"
"the usual way.\n"
"\n"
"Double-clicking an object in the list will center the camera\n"
"on the object in the world." ) );
    m_refreshButton->setText( tr( "Refresh" ) );
    m_objectSearchEdit->setText( QString::null );
    QToolTip::add( m_objectSearchEdit, tr( "Type to filter objects (case-insensitive)" ) );
    QWhatsThis::add( m_objectSearchEdit, tr( "Live search filter - type to filter the object list by template name." ) );
    m_objectSearchClearButton->setText( tr( "X" ) );
    QToolTip::add( m_objectSearchClearButton, tr( "Clear search filter" ) );
    m_templateTabs->changeTab( ObjectTab, tr( "Objects" ) );
    m_scriptRefreshButton->setText( tr( "Refresh" ) );
    m_scriptSearchEdit->setText( QString::null );
    QToolTip::add( m_scriptSearchEdit, tr( "Type to filter scripts (case-insensitive)" ) );
    m_scriptSearchClearButton->setText( tr( "X" ) );
    QToolTip::add( m_scriptSearchClearButton, tr( "Clear search filter" ) );
    m_templateTabs->changeTab( ScriptsTab, tr( "Scripts" ) );
    m_serverTemplateRefreshButton->setText( tr( "Refresh" ) );
    m_serverTemplateSearchEdit->setText( QString::null );
    QToolTip::add( m_serverTemplateSearchEdit, tr( "Type to filter templates (case-insensitive)" ) );
    QWhatsThis::add( m_serverTemplateSearchEdit, tr( "Live search filter - type to filter the template tree. Matching items and their parent folders will remain visible." ) );
    m_serverTemplateSearchClearButton->setText( tr( "X" ) );
    QToolTip::add( m_serverTemplateSearchClearButton, tr( "Clear search filter" ) );
    m_templateTabs->changeTab( ServerTemplatesTab, tr( "ServerTemplates" ) );
    m_clientTemplateRefreshButton->setText( tr( "Refresh" ) );
    m_clientTemplateSearchEdit->setText( QString::null );
    QToolTip::add( m_clientTemplateSearchEdit, tr( "Type to filter templates (case-insensitive)" ) );
    m_clientTemplateSearchClearButton->setText( tr( "X" ) );
    QToolTip::add( m_clientTemplateSearchClearButton, tr( "Clear search filter" ) );
    m_templateTabs->changeTab( ClientTemplatesTab, tr( "ClientTemplates" ) );
    m_buildoutAreaRefreshButton->setText( tr( "Refresh" ) );
    m_templateTabs->changeTab( BuildoutAreasTab, tr( "Buildout Areas" ) );
}

/****************************************************************************
** BaseTreeBrowser meta object code from reading C++ file 'BaseTreeBrowser.h'
**
** Created: Thu Jul 23 03:18:16 2026
**      by: The Qt MOC ($Id: $)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#undef QT_NO_COMPAT
#include "D:\titan\client\src\build\win32\x64\Release\BaseTreeBrowser.h"
#include <qmetaobject.h>
#include <qapplication.h>

#include <private/qucomextra_p.h>
#if !defined(Q_MOC_OUTPUT_REVISION) || (Q_MOC_OUTPUT_REVISION != 26)
#error "This file was generated using the moc from 3.3.4. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

const char *BaseTreeBrowser::className() const
{
    return "BaseTreeBrowser";
}

QMetaObject *BaseTreeBrowser::metaObj = 0;
static QMetaObjectCleanUp cleanUp_BaseTreeBrowser( "BaseTreeBrowser", &BaseTreeBrowser::staticMetaObject );

#ifndef QT_NO_TRANSLATION
QString BaseTreeBrowser::tr( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "BaseTreeBrowser", s, c, QApplication::DefaultCodec );
    else
	return QString::fromLatin1( s );
}
#ifndef QT_NO_TRANSLATION_UTF8
QString BaseTreeBrowser::trUtf8( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "BaseTreeBrowser", s, c, QApplication::UnicodeUTF8 );
    else
	return QString::fromUtf8( s );
}
#endif // QT_NO_TRANSLATION_UTF8

#endif // QT_NO_TRANSLATION

QMetaObject* BaseTreeBrowser::staticMetaObject()
{
    if ( metaObj )
	return metaObj;
    QMetaObject* parentObject = QWidget::staticMetaObject();
    static const QUMethod slot_0 = {"languageChange", 0, 0 };
    static const QMetaData slot_tbl[] = {
	{ "languageChange()", &slot_0, QMetaData::Protected }
    };
    metaObj = QMetaObject::new_metaobject(
	"BaseTreeBrowser", parentObject,
	slot_tbl, 1,
	0, 0,
#ifndef QT_NO_PROPERTIES
	0, 0,
	0, 0,
#endif // QT_NO_PROPERTIES
	0, 0 );
    cleanUp_BaseTreeBrowser.setMetaObject( metaObj );
    return metaObj;
}

void* BaseTreeBrowser::qt_cast( const char* clname )
{
    if ( !qstrcmp( clname, "BaseTreeBrowser" ) )
	return this;
    return QWidget::qt_cast( clname );
}

bool BaseTreeBrowser::qt_invoke( int _id, QUObject* _o )
{
    switch ( _id - staticMetaObject()->slotOffset() ) {
    case 0: languageChange(); break;
    default:
	return QWidget::qt_invoke( _id, _o );
    }
    return TRUE;
}

bool BaseTreeBrowser::qt_emit( int _id, QUObject* _o )
{
    return QWidget::qt_emit(_id,_o);
}
#ifndef QT_NO_PROPERTIES

bool BaseTreeBrowser::qt_property( int id, int f, QVariant* v)
{
    return QWidget::qt_property( id, f, v);
}

bool BaseTreeBrowser::qt_static_property( QObject* , int , int , QVariant* ){ return FALSE; }
#endif // QT_NO_PROPERTIES
