/****************************************************************************
** Form implementation generated from reading ui file 'D:\titan\client\src\game\client\application\SwgGodClient\src\shared\ui\BaseObjectEditor.ui'
**
** Created: Thu Jul 23 03:18:14 2026
**      by: The User Interface Compiler ($Id: qt/main.cpp   3.3.4   edited Nov 24 2003 $)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#include "D:\titan\client\src\build\win32\x64\Release\BaseObjectEditor.h"

#include <qvariant.h>
#include <qpushbutton.h>
#include <qtabwidget.h>
#include <qheader.h>
#include <qlistview.h>
#include <qlistbox.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qwhatsthis.h>
#include <qimage.h>
#include <qpixmap.h>

/*
 *  Constructs a BaseObjectEditor as a child of 'parent', with the
 *  name 'name' and widget flags set to 'f'.
 */
BaseObjectEditor::BaseObjectEditor( QWidget* parent, const char* name, WFlags fl )
    : QWidget( parent, name, fl )
{
    if ( !name )
	setName( "BaseObjectEditor" );
    BaseObjectEditorLayout = new QGridLayout( this, 1, 1, 4, 6, "BaseObjectEditorLayout"); 

    m_propertyTabs = new QTabWidget( this, "m_propertyTabs" );
    m_propertyTabs->setTabShape( QTabWidget::Rounded );

    AttributesTab = new QWidget( m_propertyTabs, "AttributesTab" );
    AttributesTabLayout = new QGridLayout( AttributesTab, 1, 1, 4, 2, "AttributesTabLayout"); 

    m_attributesList = new QListView( AttributesTab, "m_attributesList" );
    m_attributesList->addColumn( tr( "Name" ) );
    m_attributesList->header()->setClickEnabled( FALSE, m_attributesList->header()->count() - 1 );
    m_attributesList->addColumn( tr( "Value" ) );
    m_attributesList->header()->setClickEnabled( FALSE, m_attributesList->header()->count() - 1 );
    m_attributesList->setMargin( 2 );
    m_attributesList->setAllColumnsShowFocus( TRUE );
    m_attributesList->setShowSortIndicator( FALSE );
    m_attributesList->setItemMargin( 2 );
    m_attributesList->setRootIsDecorated( TRUE );
    m_attributesList->setResizeMode( QListView::LastColumn );

    AttributesTabLayout->addWidget( m_attributesList, 0, 0 );
    m_propertyTabs->insertTab( AttributesTab, QString::fromLatin1("") );

    tab = new QWidget( m_propertyTabs, "tab" );
    tabLayout = new QGridLayout( tab, 1, 1, 4, 2, "tabLayout"); 

    m_scriptsList = new QListView( tab, "m_scriptsList" );
    m_scriptsList->addColumn( tr( "Script" ) );
    m_scriptsList->setMargin( 2 );
    m_scriptsList->setShowSortIndicator( TRUE );
    m_scriptsList->setItemMargin( 2 );
    m_scriptsList->setRootIsDecorated( TRUE );

    tabLayout->addWidget( m_scriptsList, 0, 0 );
    m_propertyTabs->insertTab( tab, QString::fromLatin1("") );

    tab_2 = new QWidget( m_propertyTabs, "tab_2" );
    tabLayout_2 = new QGridLayout( tab_2, 1, 1, 4, 2, "tabLayout_2"); 

    m_objVarsList = new QListView( tab_2, "m_objVarsList" );
    m_objVarsList->addColumn( tr( "Name" ) );
    m_objVarsList->addColumn( tr( "Type" ) );
    m_objVarsList->addColumn( tr( "Value" ) );
    m_objVarsList->setEnabled( TRUE );
    m_objVarsList->setMargin( 2 );
    m_objVarsList->setShowSortIndicator( FALSE );
    m_objVarsList->setItemMargin( 2 );
    m_objVarsList->setRootIsDecorated( TRUE );

    tabLayout_2->addWidget( m_objVarsList, 0, 0 );
    m_propertyTabs->insertTab( tab_2, QString::fromLatin1("") );

    tab_3 = new QWidget( m_propertyTabs, "tab_3" );
    tabLayout_3 = new QHBoxLayout( tab_3, 11, 6, "tabLayout_3"); 

    m_creatureSkills = new QListBox( tab_3, "m_creatureSkills" );
    tabLayout_3->addWidget( m_creatureSkills );
    m_propertyTabs->insertTab( tab_3, QString::fromLatin1("") );

    BaseObjectEditorLayout->addWidget( m_propertyTabs, 0, 0 );
    languageChange();
    resize( QSize(746, 568).expandedTo(minimumSizeHint()) );
    clearWState( WState_Polished );

    // signals and slots connections
    init();
}

/*
 *  Destroys the object and frees any allocated resources
 */
BaseObjectEditor::~BaseObjectEditor()
{
    destroy();
    // no need to delete child widgets, Qt does it all for us
}

/*
 *  Sets the strings of the subwidgets using the current
 *  language.
 */
void BaseObjectEditor::languageChange()
{
    setCaption( tr( "Object Editor" ) );
    m_attributesList->header()->setLabel( 0, tr( "Name" ) );
    m_attributesList->header()->setLabel( 1, tr( "Value" ) );
    m_propertyTabs->changeTab( AttributesTab, tr( "Attributes" ) );
    m_scriptsList->header()->setLabel( 0, tr( "Script" ) );
    m_scriptsList->clear();
    QListViewItem * item = new QListViewItem( m_scriptsList, 0 );
    item->setText( 0, tr( "New Item" ) );

    m_propertyTabs->changeTab( tab, tr( "Scripts" ) );
    m_objVarsList->header()->setLabel( 0, tr( "Name" ) );
    m_objVarsList->header()->setLabel( 1, tr( "Type" ) );
    m_objVarsList->header()->setLabel( 2, tr( "Value" ) );
    m_objVarsList->clear();
    item = new QListViewItem( m_objVarsList, 0 );
    item->setText( 0, tr( "Item1" ) );

    item = new QListViewItem( m_objVarsList, item );
    item->setText( 0, tr( "Item2" ) );

    item = new QListViewItem( m_objVarsList, item );
    item->setText( 0, tr( "Item2" ) );

    item = new QListViewItem( m_objVarsList, item );
    item->setText( 0, tr( "Item" ) );

    item = new QListViewItem( m_objVarsList, item );
    item->setText( 0, tr( "Item" ) );

    item = new QListViewItem( m_objVarsList, item );
    item->setText( 0, tr( "Item12" ) );

    item = new QListViewItem( m_objVarsList, item );
    item->setText( 0, tr( "Item14" ) );

    item = new QListViewItem( m_objVarsList, item );
    item->setText( 0, tr( "New Item" ) );

    m_propertyTabs->changeTab( tab_2, tr( "ObjVars" ) );
    m_propertyTabs->changeTab( tab_3, tr( "Skills" ) );
}

void BaseObjectEditor::init()
{
}

void BaseObjectEditor::destroy()
{
}

/****************************************************************************
** BaseObjectEditor meta object code from reading C++ file 'BaseObjectEditor.h'
**
** Created: Thu Jul 23 03:18:14 2026
**      by: The Qt MOC ($Id: $)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#undef QT_NO_COMPAT
#include "D:\titan\client\src\build\win32\x64\Release\BaseObjectEditor.h"
#include <qmetaobject.h>
#include <qapplication.h>

#include <private/qucomextra_p.h>
#if !defined(Q_MOC_OUTPUT_REVISION) || (Q_MOC_OUTPUT_REVISION != 26)
#error "This file was generated using the moc from 3.3.4. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

const char *BaseObjectEditor::className() const
{
    return "BaseObjectEditor";
}

QMetaObject *BaseObjectEditor::metaObj = 0;
static QMetaObjectCleanUp cleanUp_BaseObjectEditor( "BaseObjectEditor", &BaseObjectEditor::staticMetaObject );

#ifndef QT_NO_TRANSLATION
QString BaseObjectEditor::tr( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "BaseObjectEditor", s, c, QApplication::DefaultCodec );
    else
	return QString::fromLatin1( s );
}
#ifndef QT_NO_TRANSLATION_UTF8
QString BaseObjectEditor::trUtf8( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "BaseObjectEditor", s, c, QApplication::UnicodeUTF8 );
    else
	return QString::fromUtf8( s );
}
#endif // QT_NO_TRANSLATION_UTF8

#endif // QT_NO_TRANSLATION

QMetaObject* BaseObjectEditor::staticMetaObject()
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
	"BaseObjectEditor", parentObject,
	slot_tbl, 3,
	0, 0,
#ifndef QT_NO_PROPERTIES
	0, 0,
	0, 0,
#endif // QT_NO_PROPERTIES
	0, 0 );
    cleanUp_BaseObjectEditor.setMetaObject( metaObj );
    return metaObj;
}

void* BaseObjectEditor::qt_cast( const char* clname )
{
    if ( !qstrcmp( clname, "BaseObjectEditor" ) )
	return this;
    return QWidget::qt_cast( clname );
}

bool BaseObjectEditor::qt_invoke( int _id, QUObject* _o )
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

bool BaseObjectEditor::qt_emit( int _id, QUObject* _o )
{
    return QWidget::qt_emit(_id,_o);
}
#ifndef QT_NO_PROPERTIES

bool BaseObjectEditor::qt_property( int id, int f, QVariant* v)
{
    return QWidget::qt_property( id, f, v);
}

bool BaseObjectEditor::qt_static_property( QObject* , int , int , QVariant* ){ return FALSE; }
#endif // QT_NO_PROPERTIES
