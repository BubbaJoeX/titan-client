// ======================================================================
//
// SwgCuiTerraforming.h
// copyright 2026 Titan
//
// Comprehensive city terraforming UI with shader painting, height editing,
// road creation, affectors, and region management.
// ======================================================================

#ifndef INCLUDED_SwgCuiTerraforming_H
#define INCLUDED_SwgCuiTerraforming_H

#include "clientUserInterface/CuiMediator.h"
#include "UIEventCallback.h"
#include <vector>
#include <string>

class UIButton;
class UICheckbox;
class UIComboBox;
class UIImage;
class UIPage;
class UITreeView;
class UISliderbar;
class UIText;
class UITextbox;
class CuiMessageBox;

namespace MessageDispatch
{
	class Callback;
}

// ======================================================================

class SwgCuiTerraforming : public CuiMediator, public UIEventCallback
{
public:
	// Tab identifiers
	enum TabId
	{
		TAB_SHADER_PAINT = 0,
		TAB_HEIGHT_EDIT,
		TAB_ROADS,
		TAB_AFFECTORS,
		TAB_REGIONS,
		TAB_COUNT
	};

	// Height edit modes
	enum HeightMode
	{
		HM_FLATTEN = 0,
		HM_RAISE,
		HM_LOWER,
		HM_SMOOTH,
		HM_NOISE
	};

	// Affector types
	enum AffectorType
	{
		AT_HEIGHT_CONSTANT = 0,
		AT_HEIGHT_FRACTAL,
		AT_COLOR_CONSTANT,
		AT_COLOR_RAMP_HEIGHT,
		AT_FLORA_DENSITY,
		AT_EXCLUDE,
		AT_WATER_PLANE,
		AT_PASSABLE,
		AT_NON_PASSABLE
	};

	// Region info for list display
	struct RegionInfo
	{
		std::string regionId;
		std::string displayName;
		std::string type;
		float centerX;
		float centerZ;
		float radius;
		bool active;
		int32 cityId;
	};

public:
	explicit SwgCuiTerraforming(UIPage & page);

	static SwgCuiTerraforming * createInto(UIPage & parent);

	// Re-register layer-manager hooks when the terrain painter reclaims the primary callback.
	static void restoreLayerManagerHooksIfTerraformingActive();

	void setCityId(int32 cityId);
	int32 getCityId() const { return m_cityId; }

	void setCityName(const std::string & name);
	void setCityRadius(int32 radius);

protected:
	virtual ~SwgCuiTerraforming();

	virtual void performActivate();
	virtual void performDeactivate();

	virtual void OnButtonPressed(UIWidget * context);
	virtual void OnCheckboxSet(UIWidget * context);
	virtual void OnCheckboxUnset(UIWidget * context);
	virtual void OnSliderbarChanged(UIWidget * context);
	virtual void OnGenericSelectionChanged(UIWidget * context);

private:
	SwgCuiTerraforming(const SwgCuiTerraforming &);
	SwgCuiTerraforming & operator=(const SwgCuiTerraforming &);

	// Tab switching
	void switchToTab(TabId tab);
	void updateTabVisuals();

	// Shader loading
	void loadShaderList();
	void updateShaderPreview();

	// Slider/textbox synchronization
	void syncSliderToText(UISliderbar * slider, UITextbox * textbox);
	void syncTextToSlider(UITextbox * textbox, UISliderbar * slider);

	// Apply functions for each tab
	void applyShaderPaint();
	void applyHeightEdit();
	void applyRoad();
	void applyAffector();

	// Road marker functions
	void setRoadStart();
	void setRoadEnd();
	void updateRoadStatus();
	void clearRoadMarkers();

	// Region management
	void refreshRegionList();
	void deleteSelectedRegion();
	void deleteAllRegions();
	void suspendSelectedRegion();
	void resumeSelectedRegion();
	void updateRegionInfo();
	void updateRegionActionButtons();

	// Reset terrain
	void showResetTerrainConfirmation();
	void resetTerrain();
	void onResetTerrainConfirmationClosed(const CuiMessageBox & box);

	// Undo functionality
	void undoLastModification();

	// Height sampling
	void sampleCurrentHeight();

	// Validation
	bool validateCityPermission() const;
	bool validatePosition(float & outX, float & outZ) const;

	// Status display
	void setStatus(const std::string & message, bool isError = false);
	void clearStatus();

	// State preservation
	void saveUIState();
	void restoreUIState();

	// Send terrain modification to server
	void sendTerrainModification(int32 modType, const std::string & shaderTemplate,
		float centerX, float centerZ, float radius, float endX, float endZ,
		float width, float height, float blendDistance);

	void sendTerrainRemove(int32 cityId, const std::string & regionId);

	int getSelectedRegionIndex() const;
	int32 getCityIdForRegionMessage() const;

	// Generate unique region ID
	std::string generateRegionId() const;

private:
	// Tab pages
	UIPage * m_pageShaderPaint;
	UIPage * m_pageHeightEdit;
	UIPage * m_pageRoads;
	UIPage * m_pageAffectors;
	UIPage * m_pageRegions;

	// Tab buttons
	UIButton * m_tabShaderPaint;
	UIButton * m_tabHeightEdit;
	UIButton * m_tabRoads;
	UIButton * m_tabAffectors;
	UIButton * m_tabRegions;

	// Header
	UIText * m_textCityName;
	UIButton * m_buttonClose;

	// Shader Paint tab
	UIComboBox * m_comboShader;
	UISliderbar * m_sliderPaintRadius;
	UITextbox * m_textPaintRadius;
	UISliderbar * m_sliderPaintBlend;
	UITextbox * m_textPaintBlend;
	UISliderbar * m_sliderPaintFeather;
	UITextbox * m_textPaintFeather;
	UICheckbox * m_checkPaintPreview;
	UIImage * m_imageShaderPreview;
	UIButton * m_buttonPaintApply;

	// Height Edit tab
	UIComboBox * m_comboHeightMode;
	UISliderbar * m_sliderHeightValue;
	UITextbox * m_textHeightValue;
	UISliderbar * m_sliderHeightRadius;
	UITextbox * m_textHeightRadius;
	UISliderbar * m_sliderHeightStrength;
	UITextbox * m_textHeightStrength;
	UISliderbar * m_sliderHeightFeather;
	UITextbox * m_textHeightFeather;
	UIButton * m_buttonHeightSample;
	UIButton * m_buttonHeightApply;

	// Roads tab
	UISliderbar * m_sliderRoadWidth;
	UITextbox * m_textRoadWidth;
	UIComboBox * m_comboRoadShader;
	UICheckbox * m_checkRoadSmooth;
	UICheckbox * m_checkRoadFlatten;
	UIButton * m_buttonRoadStart;
	UIButton * m_buttonRoadEnd;
	UIButton * m_buttonRoadApply;
	UIText * m_textRoadStatus;
	UIText * m_textStartPos;
	UIText * m_textEndPos;

	// Affectors tab
	UIComboBox * m_comboAffectorType;
	UISliderbar * m_sliderAffectorRadius;
	UITextbox * m_textAffectorRadius;
	UISliderbar * m_sliderAffectorIntensity;
	UITextbox * m_textAffectorIntensity;
	UISliderbar * m_sliderAffectorFeather;
	UITextbox * m_textAffectorFeather;
	UIComboBox * m_comboAffectorParam;
	UISliderbar * m_sliderAffectorParam;
	UITextbox * m_textAffectorParam;
	UIButton * m_buttonAffectorApply;

	// Regions tab
	UITreeView * m_treeRegions;
	UIButton * m_buttonRegionSuspend;
	UIButton * m_buttonRegionResume;
	UIButton * m_buttonRegionDelete;
	UIButton * m_buttonRegionDeleteAll;
	UIText * m_textRegionInfo;

	// Common controls
	UIText * m_textStatus;
	UIButton * m_buttonUndo;
	UIButton * m_buttonResetTerrain;

	// State
	int32 m_cityId;
	int32 m_cityRadius;
	std::string m_cityName;
	TabId m_currentTab;

	// Shader data
	std::vector<std::string> m_shaderTemplates;
	std::vector<std::string> m_shaderNames;

	// Road markers
	float m_roadStartX;
	float m_roadStartZ;
	float m_roadEndX;
	float m_roadEndZ;
	bool m_hasRoadStart;
	bool m_hasRoadEnd;

	// Region tracking
	std::vector<RegionInfo> m_regions;
	std::vector<std::string> m_appliedRegionIds;

	// Callback for message boxes
	MessageDispatch::Callback * m_callback;

	// Static callback for region changes
	static void onRegionChangeCallback(int32 cityId);
	static void onServerTerrainRegionsChanged(int32 cityId);
	static void onOpenTerraformingWhileAlreadyActive();
	static SwgCuiTerraforming * ms_activeInstance;

	// Static state preservation
	static int32 ms_savedCityId;
	static TabId ms_savedTab;
	static int ms_savedShaderIndex;
	static long ms_savedPaintRadius;
	static long ms_savedPaintBlend;
	static long ms_savedPaintFeather;
	static int ms_savedHeightMode;
	static long ms_savedHeightValue;
	static long ms_savedHeightRadius;
	static long ms_savedHeightStrength;
	static long ms_savedHeightFeather;
	static long ms_savedRoadWidth;
	static int ms_savedRoadShaderIndex;
	static int ms_savedAffectorType;
	static long ms_savedAffectorRadius;
	static long ms_savedAffectorIntensity;
	static long ms_savedAffectorFeather;
	static bool ms_hasRestoredState;
};

// ======================================================================

#endif // INCLUDED_SwgCuiTerraforming_H

