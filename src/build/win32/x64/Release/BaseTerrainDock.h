/****************************************************************************
** Form interface generated from reading ui file 'D:\titan\client\src\game\client\application\SwgGodClient\src\shared\ui\BaseTerrainDock.ui'
**
** Created: Thu Jul 23 03:18:11 2026
**      by: The User Interface Compiler ($Id: qt/main.cpp   3.3.4   edited Nov 24 2003 $)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#ifndef BASETERRAINDOCK_H
#define BASETERRAINDOCK_H

#include <qvariant.h>
#include <qwidget.h>

class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;
class QSpacerItem;
class QScrollView;
class QGroupBox;
class QLabel;
class QPushButton;
class QSlider;
class QComboBox;
class QTabWidget;
class QCheckBox;
class QListView;
class QListViewItem;
class QLineEdit;
class QSpinBox;

class BaseTerrainDock : public QWidget
{
    Q_OBJECT

public:
    BaseTerrainDock( QWidget* parent = 0, const char* name = 0, WFlags fl = 0 );
    ~BaseTerrainDock();

    QScrollView* m_contentScrollView;
    QWidget* m_scrollAreaContents;
    QGroupBox* m_fileGroup;
    QLabel* m_terrainFileLabel;
    QPushButton* m_loadButton;
    QPushButton* m_saveButton;
    QPushButton* m_saveAsButton;
    QPushButton* m_publishButton;
    QPushButton* m_refreshButton;
    QGroupBox* m_toolsGroup;
    QPushButton* m_toolRaise;
    QPushButton* m_toolLower;
    QPushButton* m_toolFlatten;
    QPushButton* m_toolSmooth;
    QPushButton* m_toolNoise;
    QPushButton* m_toolSetHeight;
    QGroupBox* m_raiseLowerTuneGroup;
    QLabel* m_raiseLowerSpeedLabel;
    QLabel* m_raiseLowerSpeedValue;
    QSlider* m_raiseLowerSpeedSlider;
    QLabel* m_raiseLowerBiasLabel;
    QLabel* m_raiseLowerBiasValue;
    QSlider* m_raiseLowerBiasSlider;
    QLabel* m_raiseLowerClickLabel;
    QLabel* m_raiseLowerClickValue;
    QSlider* m_raiseLowerClickSlider;
    QLabel* m_raiseLowerJitterLabel;
    QLabel* m_raiseLowerJitterValue;
    QSlider* m_raiseLowerJitterSlider;
    QGroupBox* m_brushGroup;
    QLabel* m_sizeLabel;
    QLabel* m_brushSizeValue;
    QSlider* m_brushSizeSlider;
    QLabel* m_strengthLabel;
    QLabel* m_brushStrengthValue;
    QSlider* m_brushStrengthSlider;
    QLabel* m_shapeLabel;
    QComboBox* m_brushShapeCombo;
    QLabel* m_falloffLabel;
    QComboBox* m_falloffCombo;
    QLabel* m_featherLabel;
    QLabel* m_brushFeatherValue;
    QSlider* m_brushFeatherSlider;
    QGroupBox* m_regionGroup;
    QPushButton* m_selectRegionButton;
    QPushButton* m_copyRegionButton;
    QPushButton* m_pasteRegionButton;
    QPushButton* m_fillRegionButton;
    QPushButton* m_saveRegionLayButton;
    QPushButton* m_loadRegionLayButton;
    QPushButton* m_importRegionLayAtCursorButton;
    QLabel* m_regionShapeLabel;
    QComboBox* m_regionShapeCombo;
    QLabel* m_mapParametersLabel;
    QGroupBox* m_mapTemplateEditorGroup;
    QLabel* m_mapTemplateHintLabel;
    QPushButton* m_mapTemplateSettingsButton;
    QPushButton* m_addProcHeightConstButton;
    QPushButton* m_addProcShaderConstButton;
    QPushButton* m_addProcExcludeRegionButton;
    QPushButton* m_toolExcludeTerrain;
    QPushButton* m_toolBoundaryPolygon;
    QPushButton* m_toolBoundaryPolyline;
    QPushButton* m_toolBoundaryPolyRoad;
    QGroupBox* m_regionPolygonCommitGroup;
    QPushButton* m_regionPolyFinishButton;
    QPushButton* m_regionPolyCancelButton;
    QTabWidget* m_editorTabs;
    QWidget* m_shaderTab;
    QPushButton* m_toolPaintShader;
    QCheckBox* m_shaderColorConstantCheck;
    QPushButton* m_shaderColorPickButton;
    QLabel* m_shaderColorSummaryLabel;
    QLabel* m_sceneShaderHeaderLabel;
    QListView* m_shaderList;
    QPushButton* m_openShaderFamilyEditorButton;
    QLabel* m_globalShaderHeaderLabel;
    QPushButton* m_btnRescanGlobalShaders;
    QPushButton* m_btnAddTerrainScanFolder;
    QPushButton* m_btnClearTerrainScanFolders;
    QPushButton* m_btnImportShaderFamily;
    QListView* m_globalShaderList;
    QWidget* m_waterTab;
    QPushButton* m_toolPlaceWater;
    QLabel* m_waterHeightLabel;
    QLineEdit* m_waterHeightEdit;
    QLabel* m_waterShaderLabel;
    QComboBox* m_waterShaderCombo;
    QPushButton* m_applyWaterButton;
    QWidget* m_floraTab;
    QPushButton* m_toolPaintFlora;
    QLabel* m_floraFamilyLabel;
    QComboBox* m_floraFamilyCombo;
    QPushButton* m_openFloraFamilyEditorButton;
    QPushButton* m_toolPlaceRadial;
    QLabel* m_radialGroupLabel;
    QComboBox* m_radialGroupCombo;
    QPushButton* m_openRadialFamilyEditorButton;
    QWidget* m_advancedToolsTab;
    QGroupBox* m_polylineToolsGroup;
    QPushButton* m_toolPlaceRibbon;
    QPushButton* m_toolPlaceRoad;
    QLabel* m_polylineWidthLabel;
    QSpinBox* m_polylineWidthSpin;
    QLabel* m_polylineFeatherLabel;
    QSpinBox* m_polylineFeatherSpin;
    QCheckBox* m_polylineFixedHeightsCheck;
    QLabel* m_polylineShaderLabel;
    QComboBox* m_polylineShaderCombo;
    QPushButton* m_polylineAddPointButton;
    QPushButton* m_polylineFinishButton;
    QPushButton* m_polylineCancelButton;
    QGroupBox* m_envZoneGroup;
    QPushButton* m_toolPlaceEnvironment;
    QLabel* m_environmentFamilyLabel;
    QComboBox* m_environmentFamilyCombo;
    QPushButton* m_toolApplyEnvironmentRegion;
    QPushButton* m_openEnvironmentEditorButton;
    QPushButton* m_applyEnvironmentToRegionButton;
    QPushButton* m_envZoneFinishButton;
    QPushButton* m_envZoneCancelButton;
    QGroupBox* m_bitmapStampGroup;
    QPushButton* m_toolStampBitmap;
    QLabel* m_bitmapStampLabel;
    QComboBox* m_bitmapStampCombo;
    QPushButton* m_openBitmapFamilyEditorButton;
    QLabel* m_bitmapRotationLabel;
    QLabel* m_bitmapRotationValue;
    QSlider* m_bitmapRotationSlider;
    QLabel* m_bitmapScaleLabel;
    QLabel* m_bitmapScaleValue;
    QSlider* m_bitmapScaleSlider;
    QCheckBox* m_bitmapAffectsHeightCheck;
    QCheckBox* m_bitmapAffectsShaderCheck;
    QWidget* m_layersTab;
    QListView* m_layerList;
    QPushButton* m_layerToggleActiveButton;
    QPushButton* m_layerPromoteButton;
    QPushButton* m_layerDemoteButton;
    QPushButton* m_layerRenameButton;
    QGroupBox* m_visualGroup;
    QCheckBox* m_wireframeCheck;
    QCheckBox* m_heightColorsCheck;
    QCheckBox* m_chunkGridCheck;
    QCheckBox* m_brushPreviewCheck;
    QGroupBox* m_undoGroup;
    QPushButton* m_undoButton;
    QPushButton* m_redoButton;
    QPushButton* m_clearHistoryButton;

protected:
    QVBoxLayout* BaseTerrainDockLayout;
    QVBoxLayout* m_scrollAreaContentsLayout;
    QVBoxLayout* m_fileGroupLayout;
    QVBoxLayout* m_fileButtonLayout;
    QVBoxLayout* m_toolsGroupLayout;
    QVBoxLayout* m_raiseLowerTuneGroupLayout;
    QVBoxLayout* m_raiseLowerSpeedRow;
    QHBoxLayout* m_raiseLowerSpeedHeaderRow;
    QSpacerItem* m_raiseLowerSpeedHeaderSpacer;
    QVBoxLayout* m_raiseLowerBiasRow;
    QHBoxLayout* m_raiseLowerBiasHeaderRow;
    QSpacerItem* m_raiseLowerBiasHeaderSpacer;
    QVBoxLayout* m_raiseLowerClickRow;
    QHBoxLayout* m_raiseLowerClickHeaderRow;
    QSpacerItem* m_raiseLowerClickHeaderSpacer;
    QVBoxLayout* m_raiseLowerJitterRow;
    QHBoxLayout* m_raiseLowerJitterHeaderRow;
    QSpacerItem* m_raiseLowerJitterHeaderSpacer;
    QVBoxLayout* m_brushGroupLayout;
    QVBoxLayout* m_brushSizeRow;
    QHBoxLayout* m_brushSizeHeaderRow;
    QSpacerItem* m_brushSizeHeaderSpacer;
    QVBoxLayout* m_brushStrengthRow;
    QHBoxLayout* m_brushStrengthHeaderRow;
    QSpacerItem* m_brushStrengthHeaderSpacer;
    QVBoxLayout* m_brushShapeRow;
    QVBoxLayout* m_brushFalloffRow;
    QVBoxLayout* m_brushFeatherRow;
    QHBoxLayout* m_brushFeatherHeaderRow;
    QSpacerItem* m_brushFeatherHeaderSpacer;
    QVBoxLayout* m_regionGroupLayout;
    QVBoxLayout* m_regionShapeLayout;
    QVBoxLayout* m_mapTemplateEditorGroupLayout;
    QVBoxLayout* m_regionPolygonCommitGroupLayout;
    QVBoxLayout* m_shaderTabLayout;
    QHBoxLayout* m_shaderColorPickRow;
    QVBoxLayout* m_globalShaderToolbar;
    QVBoxLayout* m_waterTabLayout;
    QSpacerItem* m_waterSpacer;
    QVBoxLayout* m_waterHeightLayout;
    QVBoxLayout* m_floraTabLayout;
    QSpacerItem* m_floraSpacer;
    QVBoxLayout* m_advancedToolsTabLayout;
    QSpacerItem* m_advancedSpacer;
    QVBoxLayout* m_polylineToolsGroupLayout;
    QVBoxLayout* m_polylineToolButtonLayout;
    QVBoxLayout* m_polylineWidthLayout;
    QVBoxLayout* m_polylineFeatherLayout;
    QVBoxLayout* m_polylineActionLayout;
    QVBoxLayout* m_envZoneGroupLayout;
    QVBoxLayout* m_envZoneActionLayout;
    QVBoxLayout* m_bitmapStampGroupLayout;
    QVBoxLayout* m_bitmapRotationLayout;
    QHBoxLayout* m_bitmapRotationHeaderRow;
    QSpacerItem* m_bitmapRotationHeaderSpacer;
    QVBoxLayout* m_bitmapScaleLayout;
    QHBoxLayout* m_bitmapScaleHeaderRow;
    QSpacerItem* m_bitmapScaleHeaderSpacer;
    QVBoxLayout* m_layersTabLayout;
    QVBoxLayout* vbox;
    QVBoxLayout* m_visualGroupLayout;
    QVBoxLayout* m_undoGroupLayout;

protected slots:
    virtual void languageChange();

};

#endif // BASETERRAINDOCK_H
