/****************************************************************************
** Form implementation generated from reading ui file 'D:\titan\client\src\game\client\application\SwgGodClient\src\shared\ui\BaseTerrainDock.ui'
**
** Created: Thu Jul 23 03:18:11 2026
**      by: The User Interface Compiler ($Id: qt/main.cpp   3.3.4   edited Nov 24 2003 $)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#include "D:\titan\client\src\build\win32\x64\Release\BaseTerrainDock.h"

#include <qvariant.h>
#include <qpushbutton.h>
#include <qscrollview.h>
#include <qgroupbox.h>
#include <qlabel.h>
#include <qslider.h>
#include <qcombobox.h>
#include <qtabwidget.h>
#include <qcheckbox.h>
#include <qheader.h>
#include <qlistview.h>
#include <qlineedit.h>
#include <qspinbox.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qwhatsthis.h>

/*
 *  Constructs a BaseTerrainDock as a child of 'parent', with the
 *  name 'name' and widget flags set to 'f'.
 */
BaseTerrainDock::BaseTerrainDock( QWidget* parent, const char* name, WFlags fl )
    : QWidget( parent, name, fl )
{
    if ( !name )
	setName( "BaseTerrainDock" );
    setMaximumSize( QSize( 525, 16777215 ) );
    BaseTerrainDockLayout = new QVBoxLayout( this, 3, 2, "BaseTerrainDockLayout"); 

    m_contentScrollView = new QScrollView( this, "m_contentScrollView" );
    m_contentScrollView->setVScrollBarMode( QScrollView::Auto );
    m_contentScrollView->setHScrollBarMode( QScrollView::AlwaysOff );

    m_scrollAreaContents = new QWidget( m_contentScrollView, "m_scrollAreaContents" );
    m_scrollAreaContentsLayout = new QVBoxLayout( m_scrollAreaContents, 0, 2, "m_scrollAreaContentsLayout"); 

    m_fileGroup = new QGroupBox( m_scrollAreaContents, "m_fileGroup" );
    m_fileGroup->setColumnLayout(0, Qt::Vertical );
    m_fileGroup->layout()->setSpacing( 2 );
    m_fileGroup->layout()->setMargin( 3 );
    m_fileGroupLayout = new QVBoxLayout( m_fileGroup->layout() );
    m_fileGroupLayout->setAlignment( Qt::AlignTop );

    m_terrainFileLabel = new QLabel( m_fileGroup, "m_terrainFileLabel" );
    m_fileGroupLayout->addWidget( m_terrainFileLabel );

    m_fileButtonLayout = new QVBoxLayout( 0, 0, 3, "m_fileButtonLayout"); 

    m_loadButton = new QPushButton( m_fileGroup, "m_loadButton" );
    m_fileButtonLayout->addWidget( m_loadButton );

    m_saveButton = new QPushButton( m_fileGroup, "m_saveButton" );
    m_fileButtonLayout->addWidget( m_saveButton );

    m_saveAsButton = new QPushButton( m_fileGroup, "m_saveAsButton" );
    m_fileButtonLayout->addWidget( m_saveAsButton );

    m_publishButton = new QPushButton( m_fileGroup, "m_publishButton" );
    m_fileButtonLayout->addWidget( m_publishButton );

    m_refreshButton = new QPushButton( m_fileGroup, "m_refreshButton" );
    m_fileButtonLayout->addWidget( m_refreshButton );
    m_fileGroupLayout->addLayout( m_fileButtonLayout );
    m_scrollAreaContentsLayout->addWidget( m_fileGroup );

    m_toolsGroup = new QGroupBox( m_scrollAreaContents, "m_toolsGroup" );
    m_toolsGroup->setColumnLayout(0, Qt::Vertical );
    m_toolsGroup->layout()->setSpacing( 2 );
    m_toolsGroup->layout()->setMargin( 3 );
    m_toolsGroupLayout = new QVBoxLayout( m_toolsGroup->layout() );
    m_toolsGroupLayout->setAlignment( Qt::AlignTop );

    m_toolRaise = new QPushButton( m_toolsGroup, "m_toolRaise" );
    m_toolRaise->setToggleButton( TRUE );
    m_toolsGroupLayout->addWidget( m_toolRaise );

    m_toolLower = new QPushButton( m_toolsGroup, "m_toolLower" );
    m_toolLower->setToggleButton( TRUE );
    m_toolsGroupLayout->addWidget( m_toolLower );

    m_toolFlatten = new QPushButton( m_toolsGroup, "m_toolFlatten" );
    m_toolFlatten->setToggleButton( TRUE );
    m_toolsGroupLayout->addWidget( m_toolFlatten );

    m_toolSmooth = new QPushButton( m_toolsGroup, "m_toolSmooth" );
    m_toolSmooth->setToggleButton( TRUE );
    m_toolsGroupLayout->addWidget( m_toolSmooth );

    m_toolNoise = new QPushButton( m_toolsGroup, "m_toolNoise" );
    m_toolNoise->setToggleButton( TRUE );
    m_toolsGroupLayout->addWidget( m_toolNoise );

    m_toolSetHeight = new QPushButton( m_toolsGroup, "m_toolSetHeight" );
    m_toolSetHeight->setToggleButton( TRUE );
    m_toolsGroupLayout->addWidget( m_toolSetHeight );
    m_scrollAreaContentsLayout->addWidget( m_toolsGroup );

    m_raiseLowerTuneGroup = new QGroupBox( m_scrollAreaContents, "m_raiseLowerTuneGroup" );
    m_raiseLowerTuneGroup->setColumnLayout(0, Qt::Vertical );
    m_raiseLowerTuneGroup->layout()->setSpacing( 4 );
    m_raiseLowerTuneGroup->layout()->setMargin( 3 );
    m_raiseLowerTuneGroupLayout = new QVBoxLayout( m_raiseLowerTuneGroup->layout() );
    m_raiseLowerTuneGroupLayout->setAlignment( Qt::AlignTop );

    m_raiseLowerSpeedRow = new QVBoxLayout( 0, 0, 2, "m_raiseLowerSpeedRow"); 

    m_raiseLowerSpeedHeaderRow = new QHBoxLayout( 0, 0, 4, "m_raiseLowerSpeedHeaderRow"); 

    m_raiseLowerSpeedLabel = new QLabel( m_raiseLowerTuneGroup, "m_raiseLowerSpeedLabel" );
    m_raiseLowerSpeedHeaderRow->addWidget( m_raiseLowerSpeedLabel );
    m_raiseLowerSpeedHeaderSpacer = new QSpacerItem( 0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum );
    m_raiseLowerSpeedHeaderRow->addItem( m_raiseLowerSpeedHeaderSpacer );

    m_raiseLowerSpeedValue = new QLabel( m_raiseLowerTuneGroup, "m_raiseLowerSpeedValue" );
    m_raiseLowerSpeedHeaderRow->addWidget( m_raiseLowerSpeedValue );
    m_raiseLowerSpeedRow->addLayout( m_raiseLowerSpeedHeaderRow );

    m_raiseLowerSpeedSlider = new QSlider( m_raiseLowerTuneGroup, "m_raiseLowerSpeedSlider" );
    m_raiseLowerSpeedSlider->setMinValue( 1 );
    m_raiseLowerSpeedSlider->setMaxValue( 200 );
    m_raiseLowerSpeedSlider->setValue( 13 );
    m_raiseLowerSpeedSlider->setOrientation( QSlider::Horizontal );
    m_raiseLowerSpeedRow->addWidget( m_raiseLowerSpeedSlider );
    m_raiseLowerTuneGroupLayout->addLayout( m_raiseLowerSpeedRow );

    m_raiseLowerBiasRow = new QVBoxLayout( 0, 0, 2, "m_raiseLowerBiasRow"); 

    m_raiseLowerBiasHeaderRow = new QHBoxLayout( 0, 0, 4, "m_raiseLowerBiasHeaderRow"); 

    m_raiseLowerBiasLabel = new QLabel( m_raiseLowerTuneGroup, "m_raiseLowerBiasLabel" );
    m_raiseLowerBiasHeaderRow->addWidget( m_raiseLowerBiasLabel );
    m_raiseLowerBiasHeaderSpacer = new QSpacerItem( 0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum );
    m_raiseLowerBiasHeaderRow->addItem( m_raiseLowerBiasHeaderSpacer );

    m_raiseLowerBiasValue = new QLabel( m_raiseLowerTuneGroup, "m_raiseLowerBiasValue" );
    m_raiseLowerBiasHeaderRow->addWidget( m_raiseLowerBiasValue );
    m_raiseLowerBiasRow->addLayout( m_raiseLowerBiasHeaderRow );

    m_raiseLowerBiasSlider = new QSlider( m_raiseLowerTuneGroup, "m_raiseLowerBiasSlider" );
    m_raiseLowerBiasSlider->setMinValue( -100 );
    m_raiseLowerBiasSlider->setMaxValue( 100 );
    m_raiseLowerBiasSlider->setValue( 0 );
    m_raiseLowerBiasSlider->setOrientation( QSlider::Horizontal );
    m_raiseLowerBiasRow->addWidget( m_raiseLowerBiasSlider );
    m_raiseLowerTuneGroupLayout->addLayout( m_raiseLowerBiasRow );

    m_raiseLowerClickRow = new QVBoxLayout( 0, 0, 2, "m_raiseLowerClickRow"); 

    m_raiseLowerClickHeaderRow = new QHBoxLayout( 0, 0, 4, "m_raiseLowerClickHeaderRow"); 

    m_raiseLowerClickLabel = new QLabel( m_raiseLowerTuneGroup, "m_raiseLowerClickLabel" );
    m_raiseLowerClickHeaderRow->addWidget( m_raiseLowerClickLabel );
    m_raiseLowerClickHeaderSpacer = new QSpacerItem( 0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum );
    m_raiseLowerClickHeaderRow->addItem( m_raiseLowerClickHeaderSpacer );

    m_raiseLowerClickValue = new QLabel( m_raiseLowerTuneGroup, "m_raiseLowerClickValue" );
    m_raiseLowerClickHeaderRow->addWidget( m_raiseLowerClickValue );
    m_raiseLowerClickRow->addLayout( m_raiseLowerClickHeaderRow );

    m_raiseLowerClickSlider = new QSlider( m_raiseLowerTuneGroup, "m_raiseLowerClickSlider" );
    m_raiseLowerClickSlider->setMinValue( 25 );
    m_raiseLowerClickSlider->setMaxValue( 400 );
    m_raiseLowerClickSlider->setValue( 100 );
    m_raiseLowerClickSlider->setOrientation( QSlider::Horizontal );
    m_raiseLowerClickRow->addWidget( m_raiseLowerClickSlider );
    m_raiseLowerTuneGroupLayout->addLayout( m_raiseLowerClickRow );

    m_raiseLowerJitterRow = new QVBoxLayout( 0, 0, 2, "m_raiseLowerJitterRow"); 

    m_raiseLowerJitterHeaderRow = new QHBoxLayout( 0, 0, 4, "m_raiseLowerJitterHeaderRow"); 

    m_raiseLowerJitterLabel = new QLabel( m_raiseLowerTuneGroup, "m_raiseLowerJitterLabel" );
    m_raiseLowerJitterHeaderRow->addWidget( m_raiseLowerJitterLabel );
    m_raiseLowerJitterHeaderSpacer = new QSpacerItem( 0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum );
    m_raiseLowerJitterHeaderRow->addItem( m_raiseLowerJitterHeaderSpacer );

    m_raiseLowerJitterValue = new QLabel( m_raiseLowerTuneGroup, "m_raiseLowerJitterValue" );
    m_raiseLowerJitterHeaderRow->addWidget( m_raiseLowerJitterValue );
    m_raiseLowerJitterRow->addLayout( m_raiseLowerJitterHeaderRow );

    m_raiseLowerJitterSlider = new QSlider( m_raiseLowerTuneGroup, "m_raiseLowerJitterSlider" );
    m_raiseLowerJitterSlider->setMinValue( 0 );
    m_raiseLowerJitterSlider->setMaxValue( 100 );
    m_raiseLowerJitterSlider->setValue( 0 );
    m_raiseLowerJitterSlider->setOrientation( QSlider::Horizontal );
    m_raiseLowerJitterRow->addWidget( m_raiseLowerJitterSlider );
    m_raiseLowerTuneGroupLayout->addLayout( m_raiseLowerJitterRow );
    m_scrollAreaContentsLayout->addWidget( m_raiseLowerTuneGroup );

    m_brushGroup = new QGroupBox( m_scrollAreaContents, "m_brushGroup" );
    m_brushGroup->setColumnLayout(0, Qt::Vertical );
    m_brushGroup->layout()->setSpacing( 4 );
    m_brushGroup->layout()->setMargin( 3 );
    m_brushGroupLayout = new QVBoxLayout( m_brushGroup->layout() );
    m_brushGroupLayout->setAlignment( Qt::AlignTop );

    m_brushSizeRow = new QVBoxLayout( 0, 0, 2, "m_brushSizeRow"); 

    m_brushSizeHeaderRow = new QHBoxLayout( 0, 0, 4, "m_brushSizeHeaderRow"); 

    m_sizeLabel = new QLabel( m_brushGroup, "m_sizeLabel" );
    m_brushSizeHeaderRow->addWidget( m_sizeLabel );
    m_brushSizeHeaderSpacer = new QSpacerItem( 0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum );
    m_brushSizeHeaderRow->addItem( m_brushSizeHeaderSpacer );

    m_brushSizeValue = new QLabel( m_brushGroup, "m_brushSizeValue" );
    m_brushSizeHeaderRow->addWidget( m_brushSizeValue );
    m_brushSizeRow->addLayout( m_brushSizeHeaderRow );

    m_brushSizeSlider = new QSlider( m_brushGroup, "m_brushSizeSlider" );
    m_brushSizeSlider->setMinValue( 1 );
    m_brushSizeSlider->setMaxValue( 256 );
    m_brushSizeSlider->setValue( 32 );
    m_brushSizeSlider->setOrientation( QSlider::Horizontal );
    m_brushSizeRow->addWidget( m_brushSizeSlider );
    m_brushGroupLayout->addLayout( m_brushSizeRow );

    m_brushStrengthRow = new QVBoxLayout( 0, 0, 2, "m_brushStrengthRow"); 

    m_brushStrengthHeaderRow = new QHBoxLayout( 0, 0, 4, "m_brushStrengthHeaderRow"); 

    m_strengthLabel = new QLabel( m_brushGroup, "m_strengthLabel" );
    m_brushStrengthHeaderRow->addWidget( m_strengthLabel );
    m_brushStrengthHeaderSpacer = new QSpacerItem( 0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum );
    m_brushStrengthHeaderRow->addItem( m_brushStrengthHeaderSpacer );

    m_brushStrengthValue = new QLabel( m_brushGroup, "m_brushStrengthValue" );
    m_brushStrengthHeaderRow->addWidget( m_brushStrengthValue );
    m_brushStrengthRow->addLayout( m_brushStrengthHeaderRow );

    m_brushStrengthSlider = new QSlider( m_brushGroup, "m_brushStrengthSlider" );
    m_brushStrengthSlider->setMinValue( 1 );
    m_brushStrengthSlider->setMaxValue( 100 );
    m_brushStrengthSlider->setValue( 50 );
    m_brushStrengthSlider->setOrientation( QSlider::Horizontal );
    m_brushStrengthRow->addWidget( m_brushStrengthSlider );
    m_brushGroupLayout->addLayout( m_brushStrengthRow );

    m_brushShapeRow = new QVBoxLayout( 0, 0, 2, "m_brushShapeRow"); 

    m_shapeLabel = new QLabel( m_brushGroup, "m_shapeLabel" );
    m_brushShapeRow->addWidget( m_shapeLabel );

    m_brushShapeCombo = new QComboBox( FALSE, m_brushGroup, "m_brushShapeCombo" );
    m_brushShapeRow->addWidget( m_brushShapeCombo );
    m_brushGroupLayout->addLayout( m_brushShapeRow );

    m_brushFalloffRow = new QVBoxLayout( 0, 0, 2, "m_brushFalloffRow"); 

    m_falloffLabel = new QLabel( m_brushGroup, "m_falloffLabel" );
    m_brushFalloffRow->addWidget( m_falloffLabel );

    m_falloffCombo = new QComboBox( FALSE, m_brushGroup, "m_falloffCombo" );
    m_brushFalloffRow->addWidget( m_falloffCombo );
    m_brushGroupLayout->addLayout( m_brushFalloffRow );

    m_brushFeatherRow = new QVBoxLayout( 0, 0, 2, "m_brushFeatherRow"); 

    m_brushFeatherHeaderRow = new QHBoxLayout( 0, 0, 4, "m_brushFeatherHeaderRow"); 

    m_featherLabel = new QLabel( m_brushGroup, "m_featherLabel" );
    m_brushFeatherHeaderRow->addWidget( m_featherLabel );
    m_brushFeatherHeaderSpacer = new QSpacerItem( 0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum );
    m_brushFeatherHeaderRow->addItem( m_brushFeatherHeaderSpacer );

    m_brushFeatherValue = new QLabel( m_brushGroup, "m_brushFeatherValue" );
    m_brushFeatherHeaderRow->addWidget( m_brushFeatherValue );
    m_brushFeatherRow->addLayout( m_brushFeatherHeaderRow );

    m_brushFeatherSlider = new QSlider( m_brushGroup, "m_brushFeatherSlider" );
    m_brushFeatherSlider->setMinValue( 0 );
    m_brushFeatherSlider->setMaxValue( 100 );
    m_brushFeatherSlider->setValue( 100 );
    m_brushFeatherSlider->setOrientation( QSlider::Horizontal );
    m_brushFeatherRow->addWidget( m_brushFeatherSlider );
    m_brushGroupLayout->addLayout( m_brushFeatherRow );
    m_scrollAreaContentsLayout->addWidget( m_brushGroup );

    m_regionGroup = new QGroupBox( m_scrollAreaContents, "m_regionGroup" );
    m_regionGroup->setColumnLayout(0, Qt::Vertical );
    m_regionGroup->layout()->setSpacing( 2 );
    m_regionGroup->layout()->setMargin( 2 );
    m_regionGroupLayout = new QVBoxLayout( m_regionGroup->layout() );
    m_regionGroupLayout->setAlignment( Qt::AlignTop );

    m_selectRegionButton = new QPushButton( m_regionGroup, "m_selectRegionButton" );
    m_selectRegionButton->setToggleButton( TRUE );
    m_regionGroupLayout->addWidget( m_selectRegionButton );

    m_copyRegionButton = new QPushButton( m_regionGroup, "m_copyRegionButton" );
    m_regionGroupLayout->addWidget( m_copyRegionButton );

    m_pasteRegionButton = new QPushButton( m_regionGroup, "m_pasteRegionButton" );
    m_regionGroupLayout->addWidget( m_pasteRegionButton );

    m_fillRegionButton = new QPushButton( m_regionGroup, "m_fillRegionButton" );
    m_regionGroupLayout->addWidget( m_fillRegionButton );

    m_saveRegionLayButton = new QPushButton( m_regionGroup, "m_saveRegionLayButton" );
    m_regionGroupLayout->addWidget( m_saveRegionLayButton );

    m_loadRegionLayButton = new QPushButton( m_regionGroup, "m_loadRegionLayButton" );
    m_regionGroupLayout->addWidget( m_loadRegionLayButton );

    m_importRegionLayAtCursorButton = new QPushButton( m_regionGroup, "m_importRegionLayAtCursorButton" );
    m_regionGroupLayout->addWidget( m_importRegionLayAtCursorButton );

    m_regionShapeLayout = new QVBoxLayout( 0, 0, 2, "m_regionShapeLayout"); 

    m_regionShapeLabel = new QLabel( m_regionGroup, "m_regionShapeLabel" );
    m_regionShapeLayout->addWidget( m_regionShapeLabel );

    m_regionShapeCombo = new QComboBox( FALSE, m_regionGroup, "m_regionShapeCombo" );
    m_regionShapeLayout->addWidget( m_regionShapeCombo );
    m_regionGroupLayout->addLayout( m_regionShapeLayout );

    m_mapParametersLabel = new QLabel( m_regionGroup, "m_mapParametersLabel" );
    m_regionGroupLayout->addWidget( m_mapParametersLabel );

    m_mapTemplateEditorGroup = new QGroupBox( m_regionGroup, "m_mapTemplateEditorGroup" );
    m_mapTemplateEditorGroup->setColumnLayout(0, Qt::Vertical );
    m_mapTemplateEditorGroup->layout()->setSpacing( 2 );
    m_mapTemplateEditorGroup->layout()->setMargin( 2 );
    m_mapTemplateEditorGroupLayout = new QVBoxLayout( m_mapTemplateEditorGroup->layout() );
    m_mapTemplateEditorGroupLayout->setAlignment( Qt::AlignTop );

    m_mapTemplateHintLabel = new QLabel( m_mapTemplateEditorGroup, "m_mapTemplateHintLabel" );
    m_mapTemplateHintLabel->setAlignment( int( QLabel::AlignTop | QLabel::AlignLeft ) );
    m_mapTemplateEditorGroupLayout->addWidget( m_mapTemplateHintLabel );

    m_mapTemplateSettingsButton = new QPushButton( m_mapTemplateEditorGroup, "m_mapTemplateSettingsButton" );
    m_mapTemplateEditorGroupLayout->addWidget( m_mapTemplateSettingsButton );

    m_addProcHeightConstButton = new QPushButton( m_mapTemplateEditorGroup, "m_addProcHeightConstButton" );
    m_mapTemplateEditorGroupLayout->addWidget( m_addProcHeightConstButton );

    m_addProcShaderConstButton = new QPushButton( m_mapTemplateEditorGroup, "m_addProcShaderConstButton" );
    m_mapTemplateEditorGroupLayout->addWidget( m_addProcShaderConstButton );

    m_addProcExcludeRegionButton = new QPushButton( m_mapTemplateEditorGroup, "m_addProcExcludeRegionButton" );
    m_mapTemplateEditorGroupLayout->addWidget( m_addProcExcludeRegionButton );
    m_regionGroupLayout->addWidget( m_mapTemplateEditorGroup );

    m_toolExcludeTerrain = new QPushButton( m_regionGroup, "m_toolExcludeTerrain" );
    m_toolExcludeTerrain->setToggleButton( TRUE );
    m_regionGroupLayout->addWidget( m_toolExcludeTerrain );

    m_toolBoundaryPolygon = new QPushButton( m_regionGroup, "m_toolBoundaryPolygon" );
    m_toolBoundaryPolygon->setToggleButton( TRUE );
    m_regionGroupLayout->addWidget( m_toolBoundaryPolygon );

    m_toolBoundaryPolyline = new QPushButton( m_regionGroup, "m_toolBoundaryPolyline" );
    m_toolBoundaryPolyline->setToggleButton( TRUE );
    m_regionGroupLayout->addWidget( m_toolBoundaryPolyline );

    m_toolBoundaryPolyRoad = new QPushButton( m_regionGroup, "m_toolBoundaryPolyRoad" );
    m_toolBoundaryPolyRoad->setToggleButton( TRUE );
    m_regionGroupLayout->addWidget( m_toolBoundaryPolyRoad );

    m_regionPolygonCommitGroup = new QGroupBox( m_regionGroup, "m_regionPolygonCommitGroup" );
    m_regionPolygonCommitGroup->setColumnLayout(0, Qt::Vertical );
    m_regionPolygonCommitGroup->layout()->setSpacing( 2 );
    m_regionPolygonCommitGroup->layout()->setMargin( 2 );
    m_regionPolygonCommitGroupLayout = new QVBoxLayout( m_regionPolygonCommitGroup->layout() );
    m_regionPolygonCommitGroupLayout->setAlignment( Qt::AlignTop );

    m_regionPolyFinishButton = new QPushButton( m_regionPolygonCommitGroup, "m_regionPolyFinishButton" );
    m_regionPolygonCommitGroupLayout->addWidget( m_regionPolyFinishButton );

    m_regionPolyCancelButton = new QPushButton( m_regionPolygonCommitGroup, "m_regionPolyCancelButton" );
    m_regionPolygonCommitGroupLayout->addWidget( m_regionPolyCancelButton );
    m_regionGroupLayout->addWidget( m_regionPolygonCommitGroup );
    m_scrollAreaContentsLayout->addWidget( m_regionGroup );

    m_editorTabs = new QTabWidget( m_scrollAreaContents, "m_editorTabs" );

    m_shaderTab = new QWidget( m_editorTabs, "m_shaderTab" );
    m_shaderTabLayout = new QVBoxLayout( m_shaderTab, 6, 4, "m_shaderTabLayout"); 

    m_toolPaintShader = new QPushButton( m_shaderTab, "m_toolPaintShader" );
    m_toolPaintShader->setToggleButton( TRUE );
    m_shaderTabLayout->addWidget( m_toolPaintShader );

    m_shaderColorConstantCheck = new QCheckBox( m_shaderTab, "m_shaderColorConstantCheck" );
    m_shaderTabLayout->addWidget( m_shaderColorConstantCheck );

    m_shaderColorPickRow = new QHBoxLayout( 0, 0, 6, "m_shaderColorPickRow"); 

    m_shaderColorPickButton = new QPushButton( m_shaderTab, "m_shaderColorPickButton" );
    m_shaderColorPickRow->addWidget( m_shaderColorPickButton );

    m_shaderColorSummaryLabel = new QLabel( m_shaderTab, "m_shaderColorSummaryLabel" );
    m_shaderColorPickRow->addWidget( m_shaderColorSummaryLabel );
    m_shaderTabLayout->addLayout( m_shaderColorPickRow );

    m_sceneShaderHeaderLabel = new QLabel( m_shaderTab, "m_sceneShaderHeaderLabel" );
    m_shaderTabLayout->addWidget( m_sceneShaderHeaderLabel );

    m_shaderList = new QListView( m_shaderTab, "m_shaderList" );
    m_shaderList->addColumn( tr( "Family ID" ) );
    m_shaderList->addColumn( tr( "Name" ) );
    m_shaderList->setSelectionMode( QListView::Single );
    m_shaderTabLayout->addWidget( m_shaderList );

    m_openShaderFamilyEditorButton = new QPushButton( m_shaderTab, "m_openShaderFamilyEditorButton" );
    m_shaderTabLayout->addWidget( m_openShaderFamilyEditorButton );

    m_globalShaderHeaderLabel = new QLabel( m_shaderTab, "m_globalShaderHeaderLabel" );
    m_shaderTabLayout->addWidget( m_globalShaderHeaderLabel );

    m_globalShaderToolbar = new QVBoxLayout( 0, 0, 4, "m_globalShaderToolbar"); 

    m_btnRescanGlobalShaders = new QPushButton( m_shaderTab, "m_btnRescanGlobalShaders" );
    m_globalShaderToolbar->addWidget( m_btnRescanGlobalShaders );

    m_btnAddTerrainScanFolder = new QPushButton( m_shaderTab, "m_btnAddTerrainScanFolder" );
    m_globalShaderToolbar->addWidget( m_btnAddTerrainScanFolder );

    m_btnClearTerrainScanFolders = new QPushButton( m_shaderTab, "m_btnClearTerrainScanFolders" );
    m_globalShaderToolbar->addWidget( m_btnClearTerrainScanFolders );

    m_btnImportShaderFamily = new QPushButton( m_shaderTab, "m_btnImportShaderFamily" );
    m_globalShaderToolbar->addWidget( m_btnImportShaderFamily );
    m_shaderTabLayout->addLayout( m_globalShaderToolbar );

    m_globalShaderList = new QListView( m_shaderTab, "m_globalShaderList" );
    m_globalShaderList->addColumn( tr( "Family ID" ) );
    m_globalShaderList->addColumn( tr( "Shader" ) );
    m_globalShaderList->addColumn( tr( "Source .trn" ) );
    m_globalShaderList->setSelectionMode( QListView::Single );
    m_shaderTabLayout->addWidget( m_globalShaderList );
    m_editorTabs->insertTab( m_shaderTab, QString::fromLatin1("") );

    m_waterTab = new QWidget( m_editorTabs, "m_waterTab" );
    m_waterTabLayout = new QVBoxLayout( m_waterTab, 6, 4, "m_waterTabLayout"); 

    m_toolPlaceWater = new QPushButton( m_waterTab, "m_toolPlaceWater" );
    m_toolPlaceWater->setToggleButton( TRUE );
    m_waterTabLayout->addWidget( m_toolPlaceWater );

    m_waterHeightLayout = new QVBoxLayout( 0, 0, 2, "m_waterHeightLayout"); 

    m_waterHeightLabel = new QLabel( m_waterTab, "m_waterHeightLabel" );
    m_waterHeightLayout->addWidget( m_waterHeightLabel );

    m_waterHeightEdit = new QLineEdit( m_waterTab, "m_waterHeightEdit" );
    m_waterHeightLayout->addWidget( m_waterHeightEdit );
    m_waterTabLayout->addLayout( m_waterHeightLayout );

    m_waterShaderLabel = new QLabel( m_waterTab, "m_waterShaderLabel" );
    m_waterTabLayout->addWidget( m_waterShaderLabel );

    m_waterShaderCombo = new QComboBox( FALSE, m_waterTab, "m_waterShaderCombo" );
    m_waterTabLayout->addWidget( m_waterShaderCombo );

    m_applyWaterButton = new QPushButton( m_waterTab, "m_applyWaterButton" );
    m_waterTabLayout->addWidget( m_applyWaterButton );
    m_waterSpacer = new QSpacerItem( 0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding );
    m_waterTabLayout->addItem( m_waterSpacer );
    m_editorTabs->insertTab( m_waterTab, QString::fromLatin1("") );

    m_floraTab = new QWidget( m_editorTabs, "m_floraTab" );
    m_floraTabLayout = new QVBoxLayout( m_floraTab, 6, 4, "m_floraTabLayout"); 

    m_toolPaintFlora = new QPushButton( m_floraTab, "m_toolPaintFlora" );
    m_toolPaintFlora->setToggleButton( TRUE );
    m_floraTabLayout->addWidget( m_toolPaintFlora );

    m_floraFamilyLabel = new QLabel( m_floraTab, "m_floraFamilyLabel" );
    m_floraTabLayout->addWidget( m_floraFamilyLabel );

    m_floraFamilyCombo = new QComboBox( FALSE, m_floraTab, "m_floraFamilyCombo" );
    m_floraTabLayout->addWidget( m_floraFamilyCombo );

    m_openFloraFamilyEditorButton = new QPushButton( m_floraTab, "m_openFloraFamilyEditorButton" );
    m_floraTabLayout->addWidget( m_openFloraFamilyEditorButton );

    m_toolPlaceRadial = new QPushButton( m_floraTab, "m_toolPlaceRadial" );
    m_toolPlaceRadial->setToggleButton( TRUE );
    m_floraTabLayout->addWidget( m_toolPlaceRadial );

    m_radialGroupLabel = new QLabel( m_floraTab, "m_radialGroupLabel" );
    m_floraTabLayout->addWidget( m_radialGroupLabel );

    m_radialGroupCombo = new QComboBox( FALSE, m_floraTab, "m_radialGroupCombo" );
    m_floraTabLayout->addWidget( m_radialGroupCombo );

    m_openRadialFamilyEditorButton = new QPushButton( m_floraTab, "m_openRadialFamilyEditorButton" );
    m_floraTabLayout->addWidget( m_openRadialFamilyEditorButton );
    m_floraSpacer = new QSpacerItem( 0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding );
    m_floraTabLayout->addItem( m_floraSpacer );
    m_editorTabs->insertTab( m_floraTab, QString::fromLatin1("") );

    m_advancedToolsTab = new QWidget( m_editorTabs, "m_advancedToolsTab" );
    m_advancedToolsTabLayout = new QVBoxLayout( m_advancedToolsTab, 6, 4, "m_advancedToolsTabLayout"); 

    m_polylineToolsGroup = new QGroupBox( m_advancedToolsTab, "m_polylineToolsGroup" );
    m_polylineToolsGroup->setColumnLayout(0, Qt::Vertical );
    m_polylineToolsGroup->layout()->setSpacing( 4 );
    m_polylineToolsGroup->layout()->setMargin( 6 );
    m_polylineToolsGroupLayout = new QVBoxLayout( m_polylineToolsGroup->layout() );
    m_polylineToolsGroupLayout->setAlignment( Qt::AlignTop );

    m_polylineToolButtonLayout = new QVBoxLayout( 0, 0, 4, "m_polylineToolButtonLayout"); 

    m_toolPlaceRibbon = new QPushButton( m_polylineToolsGroup, "m_toolPlaceRibbon" );
    m_toolPlaceRibbon->setToggleButton( TRUE );
    m_polylineToolButtonLayout->addWidget( m_toolPlaceRibbon );

    m_toolPlaceRoad = new QPushButton( m_polylineToolsGroup, "m_toolPlaceRoad" );
    m_toolPlaceRoad->setToggleButton( TRUE );
    m_polylineToolButtonLayout->addWidget( m_toolPlaceRoad );
    m_polylineToolsGroupLayout->addLayout( m_polylineToolButtonLayout );

    m_polylineWidthLayout = new QVBoxLayout( 0, 0, 2, "m_polylineWidthLayout"); 

    m_polylineWidthLabel = new QLabel( m_polylineToolsGroup, "m_polylineWidthLabel" );
    m_polylineWidthLayout->addWidget( m_polylineWidthLabel );

    m_polylineWidthSpin = new QSpinBox( m_polylineToolsGroup, "m_polylineWidthSpin" );
    m_polylineWidthSpin->setMinValue( 1 );
    m_polylineWidthSpin->setMaxValue( 128 );
    m_polylineWidthSpin->setValue( 8 );
    m_polylineWidthLayout->addWidget( m_polylineWidthSpin );
    m_polylineToolsGroupLayout->addLayout( m_polylineWidthLayout );

    m_polylineFeatherLayout = new QVBoxLayout( 0, 0, 2, "m_polylineFeatherLayout"); 

    m_polylineFeatherLabel = new QLabel( m_polylineToolsGroup, "m_polylineFeatherLabel" );
    m_polylineFeatherLayout->addWidget( m_polylineFeatherLabel );

    m_polylineFeatherSpin = new QSpinBox( m_polylineToolsGroup, "m_polylineFeatherSpin" );
    m_polylineFeatherSpin->setMinValue( 0 );
    m_polylineFeatherSpin->setMaxValue( 64 );
    m_polylineFeatherSpin->setValue( 4 );
    m_polylineFeatherLayout->addWidget( m_polylineFeatherSpin );
    m_polylineToolsGroupLayout->addLayout( m_polylineFeatherLayout );

    m_polylineFixedHeightsCheck = new QCheckBox( m_polylineToolsGroup, "m_polylineFixedHeightsCheck" );
    m_polylineToolsGroupLayout->addWidget( m_polylineFixedHeightsCheck );

    m_polylineShaderLabel = new QLabel( m_polylineToolsGroup, "m_polylineShaderLabel" );
    m_polylineToolsGroupLayout->addWidget( m_polylineShaderLabel );

    m_polylineShaderCombo = new QComboBox( FALSE, m_polylineToolsGroup, "m_polylineShaderCombo" );
    m_polylineToolsGroupLayout->addWidget( m_polylineShaderCombo );

    m_polylineActionLayout = new QVBoxLayout( 0, 0, 4, "m_polylineActionLayout"); 

    m_polylineAddPointButton = new QPushButton( m_polylineToolsGroup, "m_polylineAddPointButton" );
    m_polylineActionLayout->addWidget( m_polylineAddPointButton );

    m_polylineFinishButton = new QPushButton( m_polylineToolsGroup, "m_polylineFinishButton" );
    m_polylineActionLayout->addWidget( m_polylineFinishButton );

    m_polylineCancelButton = new QPushButton( m_polylineToolsGroup, "m_polylineCancelButton" );
    m_polylineActionLayout->addWidget( m_polylineCancelButton );
    m_polylineToolsGroupLayout->addLayout( m_polylineActionLayout );
    m_advancedToolsTabLayout->addWidget( m_polylineToolsGroup );

    m_envZoneGroup = new QGroupBox( m_advancedToolsTab, "m_envZoneGroup" );
    m_envZoneGroup->setColumnLayout(0, Qt::Vertical );
    m_envZoneGroup->layout()->setSpacing( 4 );
    m_envZoneGroup->layout()->setMargin( 6 );
    m_envZoneGroupLayout = new QVBoxLayout( m_envZoneGroup->layout() );
    m_envZoneGroupLayout->setAlignment( Qt::AlignTop );

    m_toolPlaceEnvironment = new QPushButton( m_envZoneGroup, "m_toolPlaceEnvironment" );
    m_toolPlaceEnvironment->setToggleButton( TRUE );
    m_envZoneGroupLayout->addWidget( m_toolPlaceEnvironment );

    m_environmentFamilyLabel = new QLabel( m_envZoneGroup, "m_environmentFamilyLabel" );
    m_envZoneGroupLayout->addWidget( m_environmentFamilyLabel );

    m_environmentFamilyCombo = new QComboBox( FALSE, m_envZoneGroup, "m_environmentFamilyCombo" );
    m_envZoneGroupLayout->addWidget( m_environmentFamilyCombo );

    m_toolApplyEnvironmentRegion = new QPushButton( m_envZoneGroup, "m_toolApplyEnvironmentRegion" );
    m_toolApplyEnvironmentRegion->setToggleButton( TRUE );
    m_envZoneGroupLayout->addWidget( m_toolApplyEnvironmentRegion );

    m_openEnvironmentEditorButton = new QPushButton( m_envZoneGroup, "m_openEnvironmentEditorButton" );
    m_envZoneGroupLayout->addWidget( m_openEnvironmentEditorButton );

    m_applyEnvironmentToRegionButton = new QPushButton( m_envZoneGroup, "m_applyEnvironmentToRegionButton" );
    m_envZoneGroupLayout->addWidget( m_applyEnvironmentToRegionButton );

    m_envZoneActionLayout = new QVBoxLayout( 0, 0, 4, "m_envZoneActionLayout"); 

    m_envZoneFinishButton = new QPushButton( m_envZoneGroup, "m_envZoneFinishButton" );
    m_envZoneActionLayout->addWidget( m_envZoneFinishButton );

    m_envZoneCancelButton = new QPushButton( m_envZoneGroup, "m_envZoneCancelButton" );
    m_envZoneActionLayout->addWidget( m_envZoneCancelButton );
    m_envZoneGroupLayout->addLayout( m_envZoneActionLayout );
    m_advancedToolsTabLayout->addWidget( m_envZoneGroup );

    m_bitmapStampGroup = new QGroupBox( m_advancedToolsTab, "m_bitmapStampGroup" );
    m_bitmapStampGroup->setColumnLayout(0, Qt::Vertical );
    m_bitmapStampGroup->layout()->setSpacing( 4 );
    m_bitmapStampGroup->layout()->setMargin( 6 );
    m_bitmapStampGroupLayout = new QVBoxLayout( m_bitmapStampGroup->layout() );
    m_bitmapStampGroupLayout->setAlignment( Qt::AlignTop );

    m_toolStampBitmap = new QPushButton( m_bitmapStampGroup, "m_toolStampBitmap" );
    m_toolStampBitmap->setToggleButton( TRUE );
    m_bitmapStampGroupLayout->addWidget( m_toolStampBitmap );

    m_bitmapStampLabel = new QLabel( m_bitmapStampGroup, "m_bitmapStampLabel" );
    m_bitmapStampGroupLayout->addWidget( m_bitmapStampLabel );

    m_bitmapStampCombo = new QComboBox( FALSE, m_bitmapStampGroup, "m_bitmapStampCombo" );
    m_bitmapStampGroupLayout->addWidget( m_bitmapStampCombo );

    m_openBitmapFamilyEditorButton = new QPushButton( m_bitmapStampGroup, "m_openBitmapFamilyEditorButton" );
    m_bitmapStampGroupLayout->addWidget( m_openBitmapFamilyEditorButton );

    m_bitmapRotationLayout = new QVBoxLayout( 0, 0, 2, "m_bitmapRotationLayout"); 

    m_bitmapRotationHeaderRow = new QHBoxLayout( 0, 0, 4, "m_bitmapRotationHeaderRow"); 

    m_bitmapRotationLabel = new QLabel( m_bitmapStampGroup, "m_bitmapRotationLabel" );
    m_bitmapRotationHeaderRow->addWidget( m_bitmapRotationLabel );
    m_bitmapRotationHeaderSpacer = new QSpacerItem( 0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum );
    m_bitmapRotationHeaderRow->addItem( m_bitmapRotationHeaderSpacer );

    m_bitmapRotationValue = new QLabel( m_bitmapStampGroup, "m_bitmapRotationValue" );
    m_bitmapRotationHeaderRow->addWidget( m_bitmapRotationValue );
    m_bitmapRotationLayout->addLayout( m_bitmapRotationHeaderRow );

    m_bitmapRotationSlider = new QSlider( m_bitmapStampGroup, "m_bitmapRotationSlider" );
    m_bitmapRotationSlider->setMinValue( 0 );
    m_bitmapRotationSlider->setMaxValue( 360 );
    m_bitmapRotationSlider->setValue( 0 );
    m_bitmapRotationSlider->setOrientation( QSlider::Horizontal );
    m_bitmapRotationLayout->addWidget( m_bitmapRotationSlider );
    m_bitmapStampGroupLayout->addLayout( m_bitmapRotationLayout );

    m_bitmapScaleLayout = new QVBoxLayout( 0, 0, 2, "m_bitmapScaleLayout"); 

    m_bitmapScaleHeaderRow = new QHBoxLayout( 0, 0, 4, "m_bitmapScaleHeaderRow"); 

    m_bitmapScaleLabel = new QLabel( m_bitmapStampGroup, "m_bitmapScaleLabel" );
    m_bitmapScaleHeaderRow->addWidget( m_bitmapScaleLabel );
    m_bitmapScaleHeaderSpacer = new QSpacerItem( 0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum );
    m_bitmapScaleHeaderRow->addItem( m_bitmapScaleHeaderSpacer );

    m_bitmapScaleValue = new QLabel( m_bitmapStampGroup, "m_bitmapScaleValue" );
    m_bitmapScaleHeaderRow->addWidget( m_bitmapScaleValue );
    m_bitmapScaleLayout->addLayout( m_bitmapScaleHeaderRow );

    m_bitmapScaleSlider = new QSlider( m_bitmapStampGroup, "m_bitmapScaleSlider" );
    m_bitmapScaleSlider->setMinValue( 10 );
    m_bitmapScaleSlider->setMaxValue( 500 );
    m_bitmapScaleSlider->setValue( 100 );
    m_bitmapScaleSlider->setOrientation( QSlider::Horizontal );
    m_bitmapScaleLayout->addWidget( m_bitmapScaleSlider );
    m_bitmapStampGroupLayout->addLayout( m_bitmapScaleLayout );

    m_bitmapAffectsHeightCheck = new QCheckBox( m_bitmapStampGroup, "m_bitmapAffectsHeightCheck" );
    m_bitmapAffectsHeightCheck->setChecked( TRUE );
    m_bitmapStampGroupLayout->addWidget( m_bitmapAffectsHeightCheck );

    m_bitmapAffectsShaderCheck = new QCheckBox( m_bitmapStampGroup, "m_bitmapAffectsShaderCheck" );
    m_bitmapStampGroupLayout->addWidget( m_bitmapAffectsShaderCheck );
    m_advancedToolsTabLayout->addWidget( m_bitmapStampGroup );
    m_advancedSpacer = new QSpacerItem( 0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding );
    m_advancedToolsTabLayout->addItem( m_advancedSpacer );
    m_editorTabs->insertTab( m_advancedToolsTab, QString::fromLatin1("") );

    m_layersTab = new QWidget( m_editorTabs, "m_layersTab" );
    m_layersTabLayout = new QVBoxLayout( m_layersTab, 6, 4, "m_layersTabLayout"); 

    m_layerList = new QListView( m_layersTab, "m_layerList" );
    m_layerList->addColumn( tr( "Layer" ) );
    m_layerList->addColumn( tr( "Type" ) );
    m_layerList->addColumn( tr( "Active" ) );
    m_layerList->setSelectionMode( QListView::Single );
    m_layersTabLayout->addWidget( m_layerList );
    vbox = new QVBoxLayout( 0, 0, 4, "vbox"); 

    m_layerToggleActiveButton = new QPushButton( m_layersTab, "m_layerToggleActiveButton" );
    vbox->addWidget( m_layerToggleActiveButton );

    m_layerPromoteButton = new QPushButton( m_layersTab, "m_layerPromoteButton" );
    vbox->addWidget( m_layerPromoteButton );

    m_layerDemoteButton = new QPushButton( m_layersTab, "m_layerDemoteButton" );
    vbox->addWidget( m_layerDemoteButton );

    m_layerRenameButton = new QPushButton( m_layersTab, "m_layerRenameButton" );
    vbox->addWidget( m_layerRenameButton );
    m_layersTabLayout->addLayout( vbox );
    m_editorTabs->insertTab( m_layersTab, QString::fromLatin1("") );
    m_scrollAreaContentsLayout->addWidget( m_editorTabs );

    m_visualGroup = new QGroupBox( m_scrollAreaContents, "m_visualGroup" );
    m_visualGroup->setColumnLayout(0, Qt::Vertical );
    m_visualGroup->layout()->setSpacing( 4 );
    m_visualGroup->layout()->setMargin( 6 );
    m_visualGroupLayout = new QVBoxLayout( m_visualGroup->layout() );
    m_visualGroupLayout->setAlignment( Qt::AlignTop );

    m_wireframeCheck = new QCheckBox( m_visualGroup, "m_wireframeCheck" );
    m_visualGroupLayout->addWidget( m_wireframeCheck );

    m_heightColorsCheck = new QCheckBox( m_visualGroup, "m_heightColorsCheck" );
    m_visualGroupLayout->addWidget( m_heightColorsCheck );

    m_chunkGridCheck = new QCheckBox( m_visualGroup, "m_chunkGridCheck" );
    m_visualGroupLayout->addWidget( m_chunkGridCheck );

    m_brushPreviewCheck = new QCheckBox( m_visualGroup, "m_brushPreviewCheck" );
    m_brushPreviewCheck->setChecked( TRUE );
    m_visualGroupLayout->addWidget( m_brushPreviewCheck );
    m_scrollAreaContentsLayout->addWidget( m_visualGroup );

    m_undoGroup = new QGroupBox( m_scrollAreaContents, "m_undoGroup" );
    m_undoGroup->setColumnLayout(0, Qt::Vertical );
    m_undoGroup->layout()->setSpacing( 4 );
    m_undoGroup->layout()->setMargin( 6 );
    m_undoGroupLayout = new QVBoxLayout( m_undoGroup->layout() );
    m_undoGroupLayout->setAlignment( Qt::AlignTop );

    m_undoButton = new QPushButton( m_undoGroup, "m_undoButton" );
    m_undoButton->setEnabled( FALSE );
    m_undoGroupLayout->addWidget( m_undoButton );

    m_redoButton = new QPushButton( m_undoGroup, "m_redoButton" );
    m_redoButton->setEnabled( FALSE );
    m_undoGroupLayout->addWidget( m_redoButton );

    m_clearHistoryButton = new QPushButton( m_undoGroup, "m_clearHistoryButton" );
    m_undoGroupLayout->addWidget( m_clearHistoryButton );
    m_scrollAreaContentsLayout->addWidget( m_undoGroup );
    BaseTerrainDockLayout->addWidget( m_contentScrollView );
    languageChange();
    resize( QSize(500, 720).expandedTo(minimumSizeHint()) );
    clearWState( WState_Polished );
}

/*
 *  Destroys the object and frees any allocated resources
 */
BaseTerrainDock::~BaseTerrainDock()
{
    // no need to delete child widgets, Qt does it all for us
}

/*
 *  Sets the strings of the subwidgets using the current
 *  language.
 */
void BaseTerrainDock::languageChange()
{
    setCaption( tr( "Terrain Editor" ) );
    m_fileGroup->setTitle( tr( "Terrain File" ) );
    m_terrainFileLabel->setText( tr( "No terrain loaded" ) );
    m_loadButton->setText( tr( "Load..." ) );
    m_saveButton->setText( tr( "Save" ) );
    m_saveAsButton->setText( tr( "Save As..." ) );
    m_publishButton->setText( tr( "Publish" ) );
    m_refreshButton->setText( tr( "Refresh" ) );
    m_toolsGroup->setTitle( tr( "Height Tools" ) );
    m_toolRaise->setText( tr( "Raise" ) );
    m_toolLower->setText( tr( "Lower" ) );
    m_toolFlatten->setText( tr( "Flatten" ) );
    m_toolSmooth->setText( tr( "Smooth" ) );
    m_toolNoise->setText( tr( "Noise" ) );
    m_toolSetHeight->setText( tr( "Set Height" ) );
    m_raiseLowerTuneGroup->setTitle( tr( "Raise / Lower tuning" ) );
    m_raiseLowerSpeedLabel->setText( tr( "Speed (m/dab):" ) );
    m_raiseLowerSpeedValue->setText( tr( "0.25m" ) );
    m_raiseLowerBiasLabel->setText( tr( "Bias (rel. avg):" ) );
    m_raiseLowerBiasValue->setText( tr( "Center" ) );
    m_raiseLowerClickLabel->setText( tr( "Click rate:" ) );
    m_raiseLowerClickValue->setText( tr( "100%" ) );
    m_raiseLowerJitterLabel->setText( tr( "Jitter:" ) );
    m_raiseLowerJitterValue->setText( tr( "0%" ) );
    m_brushGroup->setTitle( tr( "Brush Settings" ) );
    m_sizeLabel->setText( tr( "Size:" ) );
    m_brushSizeValue->setText( tr( "32m" ) );
    m_strengthLabel->setText( tr( "Strength:" ) );
    m_brushStrengthValue->setText( tr( "50%" ) );
    m_shapeLabel->setText( tr( "Shape:" ) );
    m_brushShapeCombo->clear();
    m_brushShapeCombo->insertItem( tr( "Circle" ) );
    m_brushShapeCombo->insertItem( tr( "Square" ) );
    m_falloffLabel->setText( tr( "Falloff:" ) );
    m_falloffCombo->clear();
    m_falloffCombo->insertItem( tr( "Linear" ) );
    m_falloffCombo->insertItem( tr( "Smooth" ) );
    m_falloffCombo->insertItem( tr( "Sharp" ) );
    m_falloffCombo->insertItem( tr( "Flat" ) );
    m_featherLabel->setText( tr( "Feather:" ) );
    m_brushFeatherValue->setText( tr( "100%" ) );
    m_regionGroup->setTitle( tr( "Region Operations" ) );
    m_selectRegionButton->setText( tr( "Select" ) );
    m_copyRegionButton->setText( tr( "Copy" ) );
    m_pasteRegionButton->setText( tr( "Paste" ) );
    m_fillRegionButton->setText( tr( "Fill" ) );
    m_saveRegionLayButton->setText( tr( "Save .lay" ) );
    m_loadRegionLayButton->setText( tr( "Load .lay" ) );
    m_importRegionLayAtCursorButton->setText( tr( "Import .lay @ cursor" ) );
    m_regionShapeLabel->setText( tr( "Region selection shape" ) );
    m_mapParametersLabel->setText( trUtf8( "\x4d\x61\x70\x3a\x20\xe2\x80\x94" ) );
    m_mapTemplateEditorGroup->setTitle( tr( "Map template & procedural layers" ) );
    m_mapTemplateHintLabel->setText( tr( "Map size, global water, and environment cycle are edited in a dialog. OK saves the .trn and reloads terrain. Procedural layer shortcuts stay below." ) );
    m_mapTemplateSettingsButton->setText( tr( "Map Parameters" ) );
    m_addProcHeightConstButton->setText( tr( "Height Constant" ) );
    m_addProcShaderConstButton->setText( tr( "Shader Constant" ) );
    m_addProcExcludeRegionButton->setText( tr( "Add exclude layer from current region" ) );
    m_toolExcludeTerrain->setText( tr( "Exclude terrain" ) );
    m_toolBoundaryPolygon->setText( tr( "Boundary mask" ) );
    m_toolBoundaryPolyline->setText( tr( "Boundary path" ) );
    m_toolBoundaryPolyRoad->setText( tr( "Boundary corridor" ) );
    m_regionPolygonCommitGroup->setTitle( tr( "Loop" ) );
    m_regionPolyFinishButton->setText( tr( "Create" ) );
    m_regionPolyCancelButton->setText( tr( "Discard" ) );
    m_toolPaintShader->setText( tr( "Paint Shader" ) );
    m_shaderColorConstantCheck->setText( tr( "Color constant paint (nearest family preview color)" ) );
    m_shaderColorPickButton->setText( trUtf8( "\x50\x69\x63\x6b\x20\x70\x61\x69\x6e\x74\x20\x63\x6f\x6c\x6f\x72\xe2\x80\xa6" ) );
    m_shaderColorSummaryLabel->setText( tr( "No color picked." ) );
    m_sceneShaderHeaderLabel->setText( tr( "Scene shaders" ) );
    m_shaderList->header()->setLabel( 0, tr( "Family ID" ) );
    m_shaderList->header()->setLabel( 1, tr( "Name" ) );
    m_openShaderFamilyEditorButton->setText( trUtf8( "\x45\x64\x69\x74\x20\x73\x68\x61\x64\x65\x72\x20\x66\x61\x6d\x69\x6c\x69\x65\x73\xe2\x80\xa6" ) );
    m_globalShaderHeaderLabel->setText( tr( "Global shaders (other .trn planets)" ) );
    m_btnRescanGlobalShaders->setText( tr( "Rescan..." ) );
    m_btnAddTerrainScanFolder->setText( tr( "Add scan folder..." ) );
    m_btnClearTerrainScanFolders->setText( tr( "Clear scan folders" ) );
    m_btnImportShaderFamily->setText( tr( "Merge into scene" ) );
    m_globalShaderList->header()->setLabel( 0, tr( "Family ID" ) );
    m_globalShaderList->header()->setLabel( 1, tr( "Shader" ) );
    m_globalShaderList->header()->setLabel( 2, tr( "Source .trn" ) );
    m_editorTabs->changeTab( m_shaderTab, tr( "Shaders" ) );
    m_toolPlaceWater->setText( tr( "Place Water Plane" ) );
    m_waterHeightLabel->setText( tr( "Water Height:" ) );
    m_waterHeightEdit->setText( tr( "0.0" ) );
    m_waterShaderLabel->setText( tr( "Water Shader:" ) );
    m_applyWaterButton->setText( tr( "Apply Water Changes" ) );
    m_editorTabs->changeTab( m_waterTab, tr( "Water" ) );
    m_toolPaintFlora->setText( tr( "Paint Flora" ) );
    m_floraFamilyLabel->setText( tr( "Flora Family:" ) );
    m_openFloraFamilyEditorButton->setText( trUtf8( "\x45\x64\x69\x74\x20\x66\x6c\x6f\x72\x61\x20\x66\x61\x6d\x69\x6c\x69\x65\x73\xe2\x80\xa6" ) );
    m_toolPlaceRadial->setText( tr( "Place Radial Group" ) );
    m_radialGroupLabel->setText( tr( "Radial Group:" ) );
    m_openRadialFamilyEditorButton->setText( trUtf8( "\x45\x64\x69\x74\x20\x72\x61\x64\x69\x61\x6c\x20\x66\x61\x6d\x69\x6c\x69\x65\x73\xe2\x80\xa6" ) );
    m_editorTabs->changeTab( m_floraTab, tr( "Flora" ) );
    m_polylineToolsGroup->setTitle( tr( "Roads / Ribbons" ) );
    m_toolPlaceRibbon->setText( tr( "Ribbon" ) );
    m_toolPlaceRoad->setText( tr( "Road" ) );
    m_polylineWidthLabel->setText( tr( "Width:" ) );
    m_polylineFeatherLabel->setText( tr( "Feather:" ) );
    m_polylineFixedHeightsCheck->setText( tr( "Use Fixed Heights" ) );
    m_polylineShaderLabel->setText( tr( "Shader:" ) );
    m_polylineAddPointButton->setText( tr( "Add Point" ) );
    m_polylineFinishButton->setText( tr( "Finish" ) );
    m_polylineCancelButton->setText( tr( "Cancel" ) );
    m_envZoneGroup->setTitle( tr( "Environment Zones" ) );
    m_toolPlaceEnvironment->setText( tr( "Place Environment Zone" ) );
    m_environmentFamilyLabel->setText( tr( "Environment Family:" ) );
    m_toolApplyEnvironmentRegion->setText( tr( "Region environment (LMB / Fill)" ) );
    m_openEnvironmentEditorButton->setText( trUtf8( "\x45\x64\x69\x74\x20\x65\x6e\x76\x69\x72\x6f\x6e\x6d\x65\x6e\x74\x20\x66\x61\x6d\x69\x6c\x69\x65\x73\xe2\x80\xa6" ) );
    m_applyEnvironmentToRegionButton->setText( tr( "Apply environment to region" ) );
    m_envZoneFinishButton->setText( tr( "Finish Zone" ) );
    m_envZoneCancelButton->setText( tr( "Cancel" ) );
    m_bitmapStampGroup->setTitle( tr( "Bitmap Stamps" ) );
    m_toolStampBitmap->setText( tr( "Stamp Bitmap" ) );
    m_bitmapStampLabel->setText( tr( "Bitmap:" ) );
    m_openBitmapFamilyEditorButton->setText( trUtf8( "\x45\x64\x69\x74\x20\x62\x69\x74\x6d\x61\x70\x20\x73\x74\x61\x6d\x70\x20\x66\x61\x6d\x69\x6c\x69\x65\x73\xe2\x80\xa6" ) );
    m_bitmapRotationLabel->setText( tr( "Rotation:" ) );
    m_bitmapRotationValue->setText( trUtf8( "\x30\xc2\xb0" ) );
    m_bitmapScaleLabel->setText( tr( "Scale:" ) );
    m_bitmapScaleValue->setText( tr( "100%" ) );
    m_bitmapAffectsHeightCheck->setText( tr( "Affects Height" ) );
    m_bitmapAffectsShaderCheck->setText( tr( "Affects Shader" ) );
    m_editorTabs->changeTab( m_advancedToolsTab, tr( "Advanced" ) );
    m_layerList->header()->setLabel( 0, tr( "Layer" ) );
    m_layerList->header()->setLabel( 1, tr( "Type" ) );
    m_layerList->header()->setLabel( 2, tr( "Active" ) );
    m_layerToggleActiveButton->setText( tr( "Toggle active" ) );
    m_layerPromoteButton->setText( tr( "Priority up" ) );
    m_layerDemoteButton->setText( tr( "Priority down" ) );
    m_layerRenameButton->setText( tr( "Rename" ) );
    m_editorTabs->changeTab( m_layersTab, tr( "Layers" ) );
    m_visualGroup->setTitle( tr( "Visualization" ) );
    m_wireframeCheck->setText( tr( "Wireframe Overlay" ) );
    m_heightColorsCheck->setText( tr( "Height Colors" ) );
    m_chunkGridCheck->setText( tr( "Chunk Grid" ) );
    m_brushPreviewCheck->setText( tr( "Brush Preview" ) );
    m_undoGroup->setTitle( tr( "History" ) );
    m_undoButton->setText( tr( "Undo" ) );
    m_redoButton->setText( tr( "Redo" ) );
    m_clearHistoryButton->setText( tr( "Clear History" ) );
}

/****************************************************************************
** BaseTerrainDock meta object code from reading C++ file 'BaseTerrainDock.h'
**
** Created: Thu Jul 23 03:18:11 2026
**      by: The Qt MOC ($Id: $)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#undef QT_NO_COMPAT
#include "D:\titan\client\src\build\win32\x64\Release\BaseTerrainDock.h"
#include <qmetaobject.h>
#include <qapplication.h>

#include <private/qucomextra_p.h>
#if !defined(Q_MOC_OUTPUT_REVISION) || (Q_MOC_OUTPUT_REVISION != 26)
#error "This file was generated using the moc from 3.3.4. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

const char *BaseTerrainDock::className() const
{
    return "BaseTerrainDock";
}

QMetaObject *BaseTerrainDock::metaObj = 0;
static QMetaObjectCleanUp cleanUp_BaseTerrainDock( "BaseTerrainDock", &BaseTerrainDock::staticMetaObject );

#ifndef QT_NO_TRANSLATION
QString BaseTerrainDock::tr( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "BaseTerrainDock", s, c, QApplication::DefaultCodec );
    else
	return QString::fromLatin1( s );
}
#ifndef QT_NO_TRANSLATION_UTF8
QString BaseTerrainDock::trUtf8( const char *s, const char *c )
{
    if ( qApp )
	return qApp->translate( "BaseTerrainDock", s, c, QApplication::UnicodeUTF8 );
    else
	return QString::fromUtf8( s );
}
#endif // QT_NO_TRANSLATION_UTF8

#endif // QT_NO_TRANSLATION

QMetaObject* BaseTerrainDock::staticMetaObject()
{
    if ( metaObj )
	return metaObj;
    QMetaObject* parentObject = QWidget::staticMetaObject();
    static const QUMethod slot_0 = {"languageChange", 0, 0 };
    static const QMetaData slot_tbl[] = {
	{ "languageChange()", &slot_0, QMetaData::Protected }
    };
    metaObj = QMetaObject::new_metaobject(
	"BaseTerrainDock", parentObject,
	slot_tbl, 1,
	0, 0,
#ifndef QT_NO_PROPERTIES
	0, 0,
	0, 0,
#endif // QT_NO_PROPERTIES
	0, 0 );
    cleanUp_BaseTerrainDock.setMetaObject( metaObj );
    return metaObj;
}

void* BaseTerrainDock::qt_cast( const char* clname )
{
    if ( !qstrcmp( clname, "BaseTerrainDock" ) )
	return this;
    return QWidget::qt_cast( clname );
}

bool BaseTerrainDock::qt_invoke( int _id, QUObject* _o )
{
    switch ( _id - staticMetaObject()->slotOffset() ) {
    case 0: languageChange(); break;
    default:
	return QWidget::qt_invoke( _id, _o );
    }
    return TRUE;
}

bool BaseTerrainDock::qt_emit( int _id, QUObject* _o )
{
    return QWidget::qt_emit(_id,_o);
}
#ifndef QT_NO_PROPERTIES

bool BaseTerrainDock::qt_property( int id, int f, QVariant* v)
{
    return QWidget::qt_property( id, f, v);
}

bool BaseTerrainDock::qt_static_property( QObject* , int , int , QVariant* ){ return FALSE; }
#endif // QT_NO_PROPERTIES
