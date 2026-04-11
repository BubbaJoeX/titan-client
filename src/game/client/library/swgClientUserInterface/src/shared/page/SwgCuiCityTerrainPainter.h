// ======================================================================
//
// SwgCuiCityTerrainPainter.h
// copyright 2026 Titan
//
// UI for city terrain painting with dynamic shader selection
// ======================================================================

#ifndef INCLUDED_SwgCuiCityTerrainPainter_H
#define INCLUDED_SwgCuiCityTerrainPainter_H

#include "clientUserInterface/CuiMediator.h"
#include "UIEventCallback.h"

#include <vector>
#include <string>

class UIButton;
class UICheckbox;
class UIComboBox;
class UIList;
class UISliderbar;
class UIText;
class UITextbox;
class UIImage;
class UIPage;

// ======================================================================

class SwgCuiCityTerrainPainter :
	public CuiMediator,
	public UIEventCallback
{
public:
	explicit SwgCuiCityTerrainPainter(UIPage & page);
	virtual ~SwgCuiCityTerrainPainter();

	virtual void performActivate();
	virtual void performDeactivate();

	virtual void OnButtonPressed(UIWidget * context);
	virtual void OnSliderbarChanged(UIWidget * context);
	virtual void OnTextboxChanged(UIWidget * context);
	virtual void OnCheckboxSet(UIWidget * context);
	virtual void OnCheckboxUnset(UIWidget * context);
	virtual void OnGenericSelectionChanged(UIWidget * context);

	// Paint modes
	enum PaintMode
	{
		PM_CIRCLE = 0,
		PM_LINE = 1,
		PM_FLATTEN = 2
	};

	void setPaintMode(PaintMode mode);
	void setCityId(int32 cityId);
	void refreshShaderList();

	// Called when first/second marker is set for line mode
	void onFirstMarkerSet(float x, float z);
	void onSecondMarkerSet(float x, float z);

	// Preview
	void updatePreview();

	// Remove the region selected in the list (any order — not LIFO).
	void removeSelectedRegion();

	// LayerManager allows only one region-change callback; Terraforming clears it on close.
	// GameNetwork calls this after CityTerrainModifyMessage so the list stays in sync.
	static void onServerTerrainRegionsChanged(int32 cityId);
	// Re-register our callback if this window is still open after another UI cleared it.
	static void restoreLayerManagerRegionCallbackIfPainterActive();

private:
	SwgCuiCityTerrainPainter(const SwgCuiCityTerrainPainter &);
	SwgCuiCityTerrainPainter & operator=(const SwgCuiCityTerrainPainter &);

	void loadShaderList();
	void applyTerrainPaint();
	void cancelPaint();
	void sendTerrainModifyToServer();
	void restoreUIState();
	void saveUIState();
	void refreshRegionList();
	void updateRemoveRegionButtonState();
	void syncPaintTileGridOverlay();

	static void onCityRegionsChanged(int32 cityId);
	static void onPaintResponse(bool success, std::string const & regionId, std::string const & errorMessage);
	static void onOpenCityTerrainPainterWhileAlreadyActive();

	UIComboBox * m_comboShader;
	UIComboBox * m_comboPaintMode;
	UISliderbar * m_sliderRadius;
	UISliderbar * m_sliderWidth;
	UISliderbar * m_sliderBlend;
	UISliderbar * m_sliderHeight;
	UISliderbar * m_sliderFlattenRadius;
	UITextbox * m_textRadius;
	UITextbox * m_textWidth;
	UITextbox * m_textBlend;
	UITextbox * m_textHeight;
	UITextbox * m_textFlattenRadius;
	UIText * m_textInstructions;
	UIText * m_textStatus;
	UIList * m_listRegions;
	UIImage * m_imagePreview;
	UIButton * m_buttonApply;
	UIButton * m_buttonCancel;
	UIButton * m_buttonUndo;
	UIButton * m_buttonSetMarker;
	UIButton * m_buttonClose;
	UIButton * m_buttonUseCurrentHeight;
	UIButton * m_buttonUseCityRadius;
	UICheckbox * m_checkShowTileGrid;
	UIPage * m_pageCircleOptions;
	UIPage * m_pageLineOptions;
	UIPage * m_pageFlattenOptions;

	int32 m_cityId;
	PaintMode m_paintMode;
	std::string m_selectedShader;

	// Line mode markers
	float m_lineStartX;
	float m_lineStartZ;
	float m_lineEndX;
	float m_lineEndZ;
	bool m_hasFirstMarker;
	bool m_hasSecondMarker;

	// Shader list
	std::vector<std::string> m_shaderTemplates;
	std::vector<std::string> m_shaderNames;

	// Parallel to m_listRegions row order (for delete by selection)
	std::vector<std::string> m_regionListRowIds;

	static SwgCuiCityTerrainPainter * ms_activeInstance;

	// State preservation (to survive Alt toggle)
	static int32 ms_savedCityId;
	static int ms_savedPaintMode;
	static int ms_savedShaderIndex;
	static long ms_savedRadius;
	static long ms_savedBlend;
	static long ms_savedWidth;
	static long ms_savedHeight;
	static long ms_savedFlattenRadius;
	static bool ms_savedShowTileGrid;
	static bool ms_hasRestoredState;
};

// ======================================================================

#endif // INCLUDED_SwgCuiCityTerrainPainter_H

