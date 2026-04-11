// ======================================================================
//
// SwgCuiCityTerrainPainter.cpp
// copyright 2026 Titan
//
// UI for city terrain painting with dynamic shader selection
// ======================================================================

#include "swgClientUserInterface/FirstSwgClientUserInterface.h"
#include "swgClientUserInterface/SwgCuiCityTerrainPainter.h"
#include "swgClientUserInterface/SwgCuiTerraforming.h"

#include "clientGame/Game.h"
#include "clientGame/GameNetwork.h"
#include "clientTerrain/CityTerrainLayerManager.h"
#include "clientGraphics/ShaderTemplateList.h"
#include "clientGraphics/StaticShader.h"
#include "clientGraphics/StaticShaderTemplate.h"
#include "clientGraphics/Texture.h"
#include "clientUserInterface/CuiManager.h"
#include "clientUserInterface/CuiMessageBox.h"
#include "sharedFile/TreeFile.h"
#include "sharedFoundation/Tag.h"
#include "sharedNetworkMessages/CityTerrainMessages.h"
#include "sharedObject/Object.h"

#include <cstdlib>
#include <string>
#include <vector>

#include "UIButton.h"
#include "UICheckbox.h"
#include "UIComboBox.h"
#include "UIData.h"
#include "UIList.h"
#include "UIImage.h"
#include "UIMessage.h"
#include "UIPage.h"
#include "UISliderbar.h"
#include "UIText.h"
#include "UITextbox.h"
#include "Unicode.h"

// ======================================================================

namespace
{
	bool syncSliderFromTextbox(UITextbox * const textbox, UISliderbar * const slider)
	{
		if (!textbox || !slider)
			return false;

		Unicode::String wide;
		textbox->GetLocalText(wide);
		std::string const narrow(Unicode::wideToNarrow(wide));
		size_t const start = narrow.find_first_not_of(" \t");
		if (start == std::string::npos)
			return false;
		size_t const end = narrow.find_last_not_of(" \t");
		std::string const trimmed(narrow.substr(start, end - start + 1));
		if (trimmed.empty() || trimmed == "-" || trimmed == "+")
			return false;

		char * endPtr = nullptr;
		long const parsed = strtol(trimmed.c_str(), &endPtr, 10);
		if (endPtr == trimmed.c_str())
			return false;

		long clamped = parsed;
		long const lo = slider->GetLowerLimit();
		long const hi = slider->GetUpperLimit();
		if (clamped < lo)
			clamped = lo;
		if (clamped > hi)
			clamped = hi;

		slider->SetValue(clamped, false);

		if (clamped != parsed)
		{
			char buf[32];
			snprintf(buf, sizeof(buf), "%ld", clamped);
			textbox->SetText(Unicode::narrowToWide(buf));
		}

		return true;
	}

	// Resolve a virtual TreeFile path suitable for UIImage::SetSourceResource (diffuse / first stage).
	bool resolveShaderTextureForPreview(char const * const shaderTemplatePath, std::string & outTextureVirtualPath)
	{
		outTextureVirtualPath.clear();
		if (!shaderTemplatePath || !shaderTemplatePath[0])
			return false;

		const Shader * const shader = ShaderTemplateList::fetchShader(shaderTemplatePath);
		if (!shader)
			return false;

		const StaticShader & staticShader = shader->prepareToView();
		{
			static Tag const TAG_MAIN = TAG(M, A, I, N);
			const Texture * tex = nullptr;
			if (staticShader.getTexture(TAG_MAIN, tex) && tex)
			{
				char const * const n = tex->getName();
				if (n && n[0])
					outTextureVirtualPath.assign(n);
			}

			if (outTextureVirtualPath.empty())
			{
				std::vector<Tag> tags;
				staticShader.getTextureTags(tags);
				for (size_t i = 0; i < tags.size(); ++i)
				{
					tex = nullptr;
					if (!staticShader.getTexture(tags[i], tex) || !tex)
						continue;
					char const * const n = tex->getName();
					if (n && n[0])
					{
						outTextureVirtualPath.assign(n);
						break;
					}
				}
			}

			if (outTextureVirtualPath.empty())
			{
				std::vector<Tag> tags;
				staticShader.getTextureTags(tags);
				for (size_t i = 0; i < tags.size(); ++i)
				{
					StaticShaderTemplate::TextureData td;
					if (!staticShader.getTextureData(tags[i], td) || !td.texture)
						continue;
					char const * const n = td.texture->getName();
					if (n && n[0])
					{
						outTextureVirtualPath.assign(n);
						break;
					}
				}
			}
		}

		shader->release();

		if (!outTextureVirtualPath.empty() && TreeFile::exists(outTextureVirtualPath.c_str()))
			return true;

		outTextureVirtualPath.clear();
		std::string base(shaderTemplatePath);
		size_t slash = base.rfind('/');
		if (slash != std::string::npos)
			base = base.substr(slash + 1);
		size_t dot = base.rfind('.');
		if (dot != std::string::npos)
			base = base.substr(0, dot);

		static char const * const extensions[] = { ".dds", ".tga" };
		for (int e = 0; e < 2; ++e)
		{
			std::string const candidate = std::string("texture/") + base + extensions[e];
			if (TreeFile::exists(candidate.c_str()))
			{
				outTextureVirtualPath = candidate;
				return true;
			}
		}

		return false;
	}
}

// ======================================================================

// Static member initialization for state preservation
int32 SwgCuiCityTerrainPainter::ms_savedCityId = 0;
int SwgCuiCityTerrainPainter::ms_savedPaintMode = 0;
int SwgCuiCityTerrainPainter::ms_savedShaderIndex = 0;
long SwgCuiCityTerrainPainter::ms_savedRadius = 10;
long SwgCuiCityTerrainPainter::ms_savedBlend = 5;
long SwgCuiCityTerrainPainter::ms_savedWidth = 4;
long SwgCuiCityTerrainPainter::ms_savedHeight = 0;
long SwgCuiCityTerrainPainter::ms_savedFlattenRadius = 100;
bool SwgCuiCityTerrainPainter::ms_savedShowTileGrid = false;
bool SwgCuiCityTerrainPainter::ms_hasRestoredState = false;
SwgCuiCityTerrainPainter * SwgCuiCityTerrainPainter::ms_activeInstance = 0;

// ======================================================================

SwgCuiCityTerrainPainter::SwgCuiCityTerrainPainter(UIPage & page) :
	CuiMediator("SwgCuiCityTerrainPainter", page),
	UIEventCallback(),
	m_comboShader(nullptr),
	m_comboPaintMode(nullptr),
	m_sliderRadius(nullptr),
	m_sliderWidth(nullptr),
	m_sliderBlend(nullptr),
	m_sliderHeight(nullptr),
	m_sliderFlattenRadius(nullptr),
	m_textRadius(nullptr),
	m_textWidth(nullptr),
	m_textBlend(nullptr),
	m_textHeight(nullptr),
	m_textFlattenRadius(nullptr),
	m_textInstructions(nullptr),
	m_textStatus(nullptr),
	m_listRegions(nullptr),
	m_imagePreview(nullptr),
	m_buttonApply(nullptr),
	m_buttonCancel(nullptr),
	m_buttonUndo(nullptr),
	m_buttonSetMarker(nullptr),
	m_buttonClose(nullptr),
	m_buttonUseCurrentHeight(nullptr),
	m_buttonUseCityRadius(nullptr),
	m_checkShowTileGrid(nullptr),
	m_pageCircleOptions(nullptr),
	m_pageLineOptions(nullptr),
	m_pageFlattenOptions(nullptr),
	m_cityId(0),
	m_paintMode(PM_CIRCLE),
	m_selectedShader(),
	m_lineStartX(0.0f),
	m_lineStartZ(0.0f),
	m_lineEndX(0.0f),
	m_lineEndZ(0.0f),
	m_hasFirstMarker(false),
	m_hasSecondMarker(false),
	m_shaderTemplates(),
	m_shaderNames(),
	m_regionListRowIds()
{
	getCodeDataObject(TUIComboBox, m_comboShader, "comboShader", true);
	getCodeDataObject(TUIComboBox, m_comboPaintMode, "comboPaintMode", true);
	getCodeDataObject(TUISliderbar, m_sliderRadius, "sliderRadius", true);
	getCodeDataObject(TUISliderbar, m_sliderBlend, "sliderBlend", true);
	getCodeDataObject(TUITextbox, m_textRadius, "textRadius", true);
	getCodeDataObject(TUITextbox, m_textBlend, "textBlend", true);
	getCodeDataObject(TUIText, m_textInstructions, "textInstructions", true);
	getCodeDataObject(TUIText, m_textStatus, "textStatus", false);
	getCodeDataObject(TUIList, m_listRegions, "listRegions", true);
	getCodeDataObject(TUIImage, m_imagePreview, "imagePreview", false);
	getCodeDataObject(TUIButton, m_buttonApply, "buttonApply", true);
	getCodeDataObject(TUIButton, m_buttonCancel, "buttonCancel", true);
	getCodeDataObject(TUIButton, m_buttonUndo, "buttonUndo", false);
	getCodeDataObject(TUIPage, m_pageCircleOptions, "pageCircleOptions", true);
	getCodeDataObject(TUIPage, m_pageLineOptions, "pageLineOptions", true);
	getCodeDataObject(TUIPage, m_pageFlattenOptions, "pageFlattenOptions", true);

	// Nested widgets in pageLineOptions
	getCodeDataObject(TUISliderbar, m_sliderWidth, "sliderWidth", false);
	getCodeDataObject(TUITextbox, m_textWidth, "textWidth", false);
	getCodeDataObject(TUIButton, m_buttonSetMarker, "buttonSetMarker", false);

	// Nested widgets in pageFlattenOptions
	getCodeDataObject(TUISliderbar, m_sliderHeight, "sliderHeight", false);
	getCodeDataObject(TUITextbox, m_textHeight, "textHeight", false);
	getCodeDataObject(TUISliderbar, m_sliderFlattenRadius, "sliderFlattenRadius", false);
	getCodeDataObject(TUITextbox, m_textFlattenRadius, "textFlattenRadius", false);
	getCodeDataObject(TUIButton, m_buttonUseCurrentHeight, "buttonUseCurrentHeight", false);
	getCodeDataObject(TUIButton, m_buttonUseCityRadius, "buttonUseCityRadius", false);

	// Close button
	getCodeDataObject(TUIButton, m_buttonClose, "buttonClose", false);
	getCodeDataObject(TUICheckbox, m_checkShowTileGrid, "checkShowTileGrid", false);

	REPORT_LOG(true, ("[Titan] SwgCuiCityTerrainPainter: constructor - comboShader=%p, comboPaintMode=%p, buttonApply=%p, buttonClose=%p\n",
		m_comboShader, m_comboPaintMode, m_buttonApply, m_buttonClose));

	if (m_buttonClose)
		registerMediatorObject(*m_buttonClose, true);
	if (m_buttonUndo)
		registerMediatorObject(*m_buttonUndo, true);
	if (m_listRegions)
		registerMediatorObject(*m_listRegions, true);

	if (m_comboShader)
		registerMediatorObject(*m_comboShader, true);
	if (m_comboPaintMode)
		registerMediatorObject(*m_comboPaintMode, true);
	if (m_sliderRadius)
		registerMediatorObject(*m_sliderRadius, true);
	if (m_sliderWidth)
		registerMediatorObject(*m_sliderWidth, true);
	if (m_sliderBlend)
		registerMediatorObject(*m_sliderBlend, true);
	if (m_sliderHeight)
		registerMediatorObject(*m_sliderHeight, true);
	if (m_sliderFlattenRadius)
		registerMediatorObject(*m_sliderFlattenRadius, true);
	if (m_buttonApply)
		registerMediatorObject(*m_buttonApply, true);
	if (m_buttonCancel)
		registerMediatorObject(*m_buttonCancel, true);
	if (m_buttonSetMarker)
		registerMediatorObject(*m_buttonSetMarker, true);
	if (m_buttonUseCurrentHeight)
		registerMediatorObject(*m_buttonUseCurrentHeight, true);
	if (m_buttonUseCityRadius)
		registerMediatorObject(*m_buttonUseCityRadius, true);

	if (m_textRadius)
		registerMediatorObject(*m_textRadius, true);
	if (m_textWidth)
		registerMediatorObject(*m_textWidth, true);
	if (m_textBlend)
		registerMediatorObject(*m_textBlend, true);
	if (m_textHeight)
		registerMediatorObject(*m_textHeight, true);
	if (m_textFlattenRadius)
		registerMediatorObject(*m_textFlattenRadius, true);
	if (m_checkShowTileGrid)
		registerMediatorObject(*m_checkShowTileGrid, true);

	// Setup paint mode combo
	if (m_comboPaintMode)
	{
		m_comboPaintMode->Clear();
		m_comboPaintMode->AddItem(Unicode::narrowToWide("Circle"), "0");
		m_comboPaintMode->AddItem(Unicode::narrowToWide("Line/Road"), "1");
		m_comboPaintMode->AddItem(Unicode::narrowToWide("Flatten"), "2");
		m_comboPaintMode->SetSelectedIndex(0);
		REPORT_LOG(true, ("[Titan] SwgCuiCityTerrainPainter: populated paint mode combo with 3 items\n"));
	}
	else
	{
		REPORT_LOG(true, ("[Titan] SwgCuiCityTerrainPainter: ERROR - m_comboPaintMode is null!\n"));
	}

	// Setup default slider values
	if (m_sliderRadius)
	{
		m_sliderRadius->SetLowerLimit(5);
		m_sliderRadius->SetUpperLimit(50);
		m_sliderRadius->SetValue(10, false);
	}

	if (m_sliderWidth)
	{
		m_sliderWidth->SetLowerLimit(2);
		m_sliderWidth->SetUpperLimit(20);
		m_sliderWidth->SetValue(4, false);
	}

	if (m_sliderBlend)
	{
		m_sliderBlend->SetLowerLimit(0);
		m_sliderBlend->SetUpperLimit(20);
		m_sliderBlend->SetValue(5, false);
	}

	if (m_sliderHeight)
	{
		m_sliderHeight->SetLowerLimit(-100);
		m_sliderHeight->SetUpperLimit(500);
		m_sliderHeight->SetValue(0, false);
	}

	if (m_sliderFlattenRadius)
	{
		m_sliderFlattenRadius->SetLowerLimit(10);
		m_sliderFlattenRadius->SetUpperLimit(500);
		m_sliderFlattenRadius->SetValue(100, false);
	}

	auto syncTextFromSlider = [](UISliderbar * const s, UITextbox * const t)
	{
		if (!s || !t)
			return;
		char buf[32];
		snprintf(buf, sizeof(buf), "%ld", s->GetValue());
		t->SetText(Unicode::narrowToWide(buf));
	};
	syncTextFromSlider(m_sliderRadius, m_textRadius);
	syncTextFromSlider(m_sliderBlend, m_textBlend);
	syncTextFromSlider(m_sliderWidth, m_textWidth);
	syncTextFromSlider(m_sliderHeight, m_textHeight);
	syncTextFromSlider(m_sliderFlattenRadius, m_textFlattenRadius);

	updateRemoveRegionButtonState();
}

// ----------------------------------------------------------------------

SwgCuiCityTerrainPainter::~SwgCuiCityTerrainPainter()
{
	m_comboShader = nullptr;
	m_comboPaintMode = nullptr;
	m_sliderRadius = nullptr;
	m_sliderWidth = nullptr;
	m_sliderBlend = nullptr;
	m_sliderHeight = nullptr;
	m_sliderFlattenRadius = nullptr;
	m_textRadius = nullptr;
	m_textWidth = nullptr;
	m_textBlend = nullptr;
	m_textHeight = nullptr;
	m_textFlattenRadius = nullptr;
	m_textInstructions = nullptr;
	m_textStatus = nullptr;
	m_listRegions = nullptr;
	m_imagePreview = nullptr;
	m_buttonApply = nullptr;
	m_buttonCancel = nullptr;
	m_buttonUndo = nullptr;
	m_buttonSetMarker = nullptr;
	m_buttonClose = nullptr;
	m_buttonUseCurrentHeight = nullptr;
	m_buttonUseCityRadius = nullptr;
	m_checkShowTileGrid = nullptr;
	m_pageCircleOptions = nullptr;
	m_pageLineOptions = nullptr;
	m_pageFlattenOptions = nullptr;
}

// ----------------------------------------------------------------------

void SwgCuiCityTerrainPainter::performActivate()
{
	CuiMediator::performActivate();

	ms_activeInstance = this;
	if (CityTerrainLayerManager::isInstalled())
	{
		CityTerrainLayerManager::setRegionChangeCallback(&SwgCuiCityTerrainPainter::onCityRegionsChanged);
		CityTerrainLayerManager::setPaintResponseCallback(&SwgCuiCityTerrainPainter::onPaintResponse);
		CityTerrainLayerManager::setCityTerrainUiRefreshFn(&SwgCuiCityTerrainPainter::onServerTerrainRegionsChanged);
		CityTerrainLayerManager::setOpenCityTerrainPainterAlreadyActiveFn(&SwgCuiCityTerrainPainter::onOpenCityTerrainPainterWhileAlreadyActive);
	}

	// Get city ID from CityTerrainLayerManager pending value
	int32 pendingCityId = CityTerrainLayerManager::getPendingCityId();
	if (pendingCityId != 0)
	{
		m_cityId = pendingCityId;
		ms_savedCityId = pendingCityId;
	}
	else if (ms_savedCityId != 0)
	{
		m_cityId = ms_savedCityId;
	}

	loadShaderList();

	// Restore UI state if we have saved state (e.g., after Alt toggle)
	if (ms_hasRestoredState)
	{
		restoreUIState();
	}
	else
	{
		setPaintMode(PM_CIRCLE);
	}

	updatePreview();

	// Pull authoritative regions from game server (objvars on city hall)
	if (m_cityId != 0 && CityTerrainLayerManager::isInstalled())
	{
		CityTerrainSyncRequestMessage const syncMsg(m_cityId);
		GameNetwork::send(syncMsg, true);
	}

	refreshRegionList();
	syncPaintTileGridOverlay();
}

// ----------------------------------------------------------------------

void SwgCuiCityTerrainPainter::performDeactivate()
{
	// Save UI state before deactivating
	saveUIState();
	if (CityTerrainLayerManager::isInstalled())
	{
		CityTerrainLayerManager::setPaintTileGridOverlay(false, 0);
		CityTerrainLayerManager::clearRegionChangeCallback();
		CityTerrainLayerManager::clearPaintResponseCallback();
		CityTerrainLayerManager::clearCityTerrainUiRefreshFn();
		CityTerrainLayerManager::clearOpenCityTerrainPainterAlreadyActiveFn();
	}
	if (ms_activeInstance == this)
		ms_activeInstance = 0;
	CuiMediator::performDeactivate();
}

// ----------------------------------------------------------------------

void SwgCuiCityTerrainPainter::OnButtonPressed(UIWidget * context)
{
	if (context == m_buttonApply)
	{
		applyTerrainPaint();
	}
	else if (context == m_buttonCancel || context == m_buttonClose)
	{
		cancelPaint();
	}
	else if (context == m_buttonUndo)
	{
		removeSelectedRegion();
	}
	else if (context == m_buttonSetMarker)
	{
		// Handle setting markers for line mode
		if (m_paintMode == PM_LINE)
		{
			Object const * const player = Game::getPlayer();
			if (player)
			{
				Vector const & pos = player->getPosition_w();
				if (!m_hasFirstMarker)
				{
					onFirstMarkerSet(pos.x, pos.z);
				}
				else if (!m_hasSecondMarker)
				{
					onSecondMarkerSet(pos.x, pos.z);
				}
			}
		}
	}
	else if (context == m_buttonUseCurrentHeight)
	{
		// Set height slider to player's current terrain height
		Object const * const player = Game::getPlayer();
		if (player && m_sliderHeight && m_textHeight)
		{
			float currentHeight = player->getPosition_w().y;
			long heightValue = static_cast<long>(currentHeight);
			m_sliderHeight->SetValue(heightValue, false);
			char buf[32];
			snprintf(buf, sizeof(buf), "%ld", heightValue);
			m_textHeight->SetText(Unicode::narrowToWide(buf));
		}
	}
	else if (context == m_buttonUseCityRadius)
	{
		// Set flatten radius slider to city radius
		if (m_sliderFlattenRadius && m_textFlattenRadius && m_cityId > 0)
		{
			int32 cityRadius = CityTerrainLayerManager::getCityRadius(m_cityId);
			if (cityRadius > 0)
			{
				m_sliderFlattenRadius->SetValue(cityRadius, false);
				char buf[32];
				snprintf(buf, sizeof(buf), "%d", cityRadius);
				m_textFlattenRadius->SetText(Unicode::narrowToWide(buf));
			}
		}
	}
}

// ----------------------------------------------------------------------

void SwgCuiCityTerrainPainter::OnSliderbarChanged(UIWidget * context)
{
	if (context == m_sliderRadius && m_textRadius)
	{
		char buf[32];
		snprintf(buf, sizeof(buf), "%ld", m_sliderRadius->GetValue());
		m_textRadius->SetText(Unicode::narrowToWide(buf));
	}
	else if (context == m_sliderWidth && m_textWidth)
	{
		char buf[32];
		snprintf(buf, sizeof(buf), "%ld", m_sliderWidth->GetValue());
		m_textWidth->SetText(Unicode::narrowToWide(buf));
	}
	else if (context == m_sliderBlend && m_textBlend)
	{
		char buf[32];
		snprintf(buf, sizeof(buf), "%ld", m_sliderBlend->GetValue());
		m_textBlend->SetText(Unicode::narrowToWide(buf));
	}
	else if (context == m_sliderHeight && m_textHeight)
	{
		char buf[32];
		snprintf(buf, sizeof(buf), "%ld", m_sliderHeight->GetValue());
		m_textHeight->SetText(Unicode::narrowToWide(buf));
	}
	else if (context == m_sliderFlattenRadius && m_textFlattenRadius)
	{
		char buf[32];
		snprintf(buf, sizeof(buf), "%ld", m_sliderFlattenRadius->GetValue());
		m_textFlattenRadius->SetText(Unicode::narrowToWide(buf));
	}

	updatePreview();
}

// ----------------------------------------------------------------------

void SwgCuiCityTerrainPainter::OnTextboxChanged(UIWidget * context)
{
	bool const didSync =
		(context == m_textRadius && syncSliderFromTextbox(m_textRadius, m_sliderRadius)) ||
		(context == m_textWidth && syncSliderFromTextbox(m_textWidth, m_sliderWidth)) ||
		(context == m_textBlend && syncSliderFromTextbox(m_textBlend, m_sliderBlend)) ||
		(context == m_textHeight && syncSliderFromTextbox(m_textHeight, m_sliderHeight)) ||
		(context == m_textFlattenRadius && syncSliderFromTextbox(m_textFlattenRadius, m_sliderFlattenRadius));

	if (didSync)
		updatePreview();
}

// ----------------------------------------------------------------------

void SwgCuiCityTerrainPainter::OnCheckboxSet(UIWidget * context)
{
	if (context == m_checkShowTileGrid)
		syncPaintTileGridOverlay();
}

// ----------------------------------------------------------------------

void SwgCuiCityTerrainPainter::OnCheckboxUnset(UIWidget * context)
{
	if (context == m_checkShowTileGrid)
		syncPaintTileGridOverlay();
}

// ----------------------------------------------------------------------

void SwgCuiCityTerrainPainter::OnGenericSelectionChanged(UIWidget * context)
{
	if (context == m_comboPaintMode)
	{
		int const selectedIndex = m_comboPaintMode->GetSelectedIndex();
		setPaintMode(static_cast<PaintMode>(selectedIndex));
	}
	else if (context == m_comboShader)
	{
		int const selectedIndex = m_comboShader->GetSelectedIndex();
		if (selectedIndex >= 0 && selectedIndex < static_cast<int>(m_shaderTemplates.size()))
		{
			m_selectedShader = m_shaderTemplates[selectedIndex];
			updatePreview();
		}
	}
	else if (context == m_listRegions)
	{
		updateRemoveRegionButtonState();
	}
}

// ----------------------------------------------------------------------

void SwgCuiCityTerrainPainter::setPaintMode(PaintMode mode)
{
	m_paintMode = mode;
	m_hasFirstMarker = false;
	m_hasSecondMarker = false;

	// Show/hide relevant UI pages based on mode
	if (m_pageCircleOptions)
	{
		m_pageCircleOptions->SetVisible(mode == PM_CIRCLE);
	}
	if (m_pageLineOptions)
	{
		m_pageLineOptions->SetVisible(mode == PM_LINE);
	}
	if (m_pageFlattenOptions)
	{
		m_pageFlattenOptions->SetVisible(mode == PM_FLATTEN);
	}

	// Update instructions
	if (m_textInstructions)
	{
		Unicode::String instructions;
		switch (mode)
		{
		case PM_CIRCLE:
			instructions = Unicode::narrowToWide("Select a shader and radius, then stand at the center and click Apply.");
			break;
		case PM_LINE:
			instructions = Unicode::narrowToWide("Select a shader, walk to start point and click Set Marker, then walk to end point and click Set Marker again, then Apply.");
			break;
		case PM_FLATTEN:
			instructions = Unicode::narrowToWide("Set the target height and flatten radius, then stand at the center and click Apply.");
			break;
		}
		m_textInstructions->SetText(instructions);
	}

	updatePreview();
}

// ----------------------------------------------------------------------

void SwgCuiCityTerrainPainter::setCityId(int32 cityId)
{
	m_cityId = cityId;
	ms_savedCityId = cityId;
	if (!isActive())
		return;
	if (cityId != 0 && CityTerrainLayerManager::isInstalled())
	{
		CityTerrainSyncRequestMessage const syncMsg(cityId);
		GameNetwork::send(syncMsg, true);
	}
	refreshShaderList();
	refreshRegionList();
	updatePreview();
	syncPaintTileGridOverlay();
}

// ----------------------------------------------------------------------

void SwgCuiCityTerrainPainter::refreshShaderList()
{
	loadShaderList();
}

// ----------------------------------------------------------------------

void SwgCuiCityTerrainPainter::onFirstMarkerSet(float x, float z)
{
	m_lineStartX = x;
	m_lineStartZ = z;
	m_hasFirstMarker = true;

	if (m_textStatus)
	{
		char buf[128];
		snprintf(buf, sizeof(buf), "First marker set at (%.1f, %.1f). Walk to end point and click Set Marker.", x, z);
		m_textStatus->SetText(Unicode::narrowToWide(buf));
	}
}

// ----------------------------------------------------------------------

void SwgCuiCityTerrainPainter::onSecondMarkerSet(float x, float z)
{
	m_lineEndX = x;
	m_lineEndZ = z;
	m_hasSecondMarker = true;

	if (m_textStatus)
	{
		char buf[128];
		snprintf(buf, sizeof(buf), "Line defined from (%.1f, %.1f) to (%.1f, %.1f). Click Apply to paint.",
				 m_lineStartX, m_lineStartZ, x, z);
		m_textStatus->SetText(Unicode::narrowToWide(buf));
	}

	updatePreview();
}

// ----------------------------------------------------------------------

void SwgCuiCityTerrainPainter::updatePreview()
{
	if (!m_imagePreview)
		return;

	if (m_selectedShader.empty())
	{
		m_imagePreview->SetSourceResource(Unicode::String());
		return;
	}

	std::string texturePath;
	if (resolveShaderTextureForPreview(m_selectedShader.c_str(), texturePath))
		m_imagePreview->SetSourceResource(Unicode::narrowToWide(texturePath));
	else
		m_imagePreview->SetSourceResource(Unicode::String());
}

// ----------------------------------------------------------------------

void SwgCuiCityTerrainPainter::loadShaderList()
{
	m_shaderTemplates.clear();
	m_shaderNames.clear();

	// Use CityTerrainLayerManager to enumerate available shaders
	CityTerrainLayerManager::getInstance().enumerateAvailableShaders(m_shaderTemplates, m_shaderNames);

	REPORT_LOG(true, ("[Titan] SwgCuiCityTerrainPainter::loadShaderList - found %d shaders, comboShader=%p\n",
		static_cast<int>(m_shaderNames.size()), m_comboShader));

	// Populate combo box
	if (m_comboShader)
	{
		m_comboShader->Clear();
		for (size_t i = 0; i < m_shaderNames.size(); ++i)
		{
			m_comboShader->AddItem(Unicode::narrowToWide(m_shaderNames[i]), m_shaderTemplates[i].c_str());
			REPORT_LOG(true, ("[Titan] SwgCuiCityTerrainPainter::loadShaderList - added shader [%d]: %s\n",
				static_cast<int>(i), m_shaderNames[i].c_str()));
		}

		if (!m_shaderTemplates.empty())
		{
			m_comboShader->SetSelectedIndex(0);
			m_selectedShader = m_shaderTemplates[0];
		}
	}

	updatePreview();
}

// ----------------------------------------------------------------------

void SwgCuiCityTerrainPainter::applyTerrainPaint()
{
	if (m_paintMode == PM_LINE && (!m_hasFirstMarker || !m_hasSecondMarker))
	{
		CuiMessageBox::createInfoBox(Unicode::narrowToWide("You must set both start and end markers for line mode."));
		return;
	}

	if ((m_paintMode == PM_CIRCLE || m_paintMode == PM_LINE) && m_selectedShader.empty())
	{
		CuiMessageBox::createInfoBox(Unicode::narrowToWide("You must select a shader."));
		return;
	}

	if (m_cityId == 0)
	{
		CuiMessageBox::createInfoBox(Unicode::narrowToWide("No city ID — reopen the terrain painter from the city terminal."));
		return;
	}

	// Do not apply a client-only region here: the server assigns the real region id and broadcasts
	// CityTerrainModifyMessage. A duplicate local id prevented shaders/heights from matching saves and broke undo.
	sendTerrainModifyToServer();

	// Reset markers for next use
	m_hasFirstMarker = false;
	m_hasSecondMarker = false;

	if (m_textStatus)
	{
		m_textStatus->SetText(Unicode::narrowToWide("Sent to server — waiting for confirmation..."));
	}
}

// ----------------------------------------------------------------------

void SwgCuiCityTerrainPainter::cancelPaint()
{
	m_hasFirstMarker = false;
	m_hasSecondMarker = false;
	deactivate();
}

// ----------------------------------------------------------------------

void SwgCuiCityTerrainPainter::sendTerrainModifyToServer()
{
	Object const * const player = Game::getPlayer();
	if (!player)
		return;

	Vector const & pos = player->getPosition_w();

	float centerX = pos.x;
	float centerZ = pos.z;
	float radius = m_sliderRadius ? static_cast<float>(m_sliderRadius->GetValue()) : 10.0f;
	float width = m_sliderWidth ? static_cast<float>(m_sliderWidth->GetValue()) : 4.0f;
	float blendDist = m_sliderBlend ? static_cast<float>(m_sliderBlend->GetValue()) : 5.0f;
	float height = m_sliderHeight ? static_cast<float>(m_sliderHeight->GetValue()) : 0.0f;
	float flattenRadius = m_sliderFlattenRadius ? static_cast<float>(m_sliderFlattenRadius->GetValue()) : 100.0f;
	float endX = m_lineEndX;
	float endZ = m_lineEndZ;

	int32 modType = 0;
	switch (m_paintMode)
	{
	case PM_CIRCLE:
		modType = CityTerrainModificationType::MT_SHADER_CIRCLE;
		break;
	case PM_LINE:
		modType = CityTerrainModificationType::MT_SHADER_LINE;
		centerX = m_lineStartX;
		centerZ = m_lineStartZ;
		break;
	case PM_FLATTEN:
		modType = CityTerrainModificationType::MT_FLATTEN;
		radius = flattenRadius; // Use flatten radius for flatten mode
		break;
	}

	REPORT_LOG(true, ("[Titan] SwgCuiCityTerrainPainter::sendTerrainModifyToServer: cityId=%d modType=%d shader=%s center=(%.1f,%.1f) radius=%.1f height=%.1f\n",
		m_cityId, modType, m_selectedShader.c_str(), centerX, centerZ, radius, height));

	CityTerrainPaintRequestMessage const msg(
		m_cityId,
		modType,
		m_selectedShader,
		centerX,
		centerZ,
		radius,
		endX,
		endZ,
		width,
		height,
		blendDist
	);

	GameNetwork::send(msg, true);
}

// ----------------------------------------------------------------------

void SwgCuiCityTerrainPainter::saveUIState()
{
	ms_savedCityId = m_cityId;
	ms_savedPaintMode = static_cast<int>(m_paintMode);

	if (m_comboShader)
		ms_savedShaderIndex = m_comboShader->GetSelectedIndex();

	if (m_sliderRadius)
		ms_savedRadius = m_sliderRadius->GetValue();

	if (m_sliderBlend)
		ms_savedBlend = m_sliderBlend->GetValue();

	if (m_sliderWidth)
		ms_savedWidth = m_sliderWidth->GetValue();

	if (m_sliderHeight)
		ms_savedHeight = m_sliderHeight->GetValue();

	if (m_sliderFlattenRadius)
		ms_savedFlattenRadius = m_sliderFlattenRadius->GetValue();

	if (m_checkShowTileGrid)
		ms_savedShowTileGrid = m_checkShowTileGrid->IsChecked();

	ms_hasRestoredState = true;
}

// ----------------------------------------------------------------------

void SwgCuiCityTerrainPainter::restoreUIState()
{
	// Restore paint mode
	if (m_comboPaintMode && ms_savedPaintMode >= 0 && ms_savedPaintMode <= 2)
	{
		m_comboPaintMode->SetSelectedIndex(ms_savedPaintMode);
		setPaintMode(static_cast<PaintMode>(ms_savedPaintMode));
	}

	// Restore shader selection
	if (m_comboShader && ms_savedShaderIndex >= 0 &&
		ms_savedShaderIndex < static_cast<int>(m_shaderTemplates.size()))
	{
		m_comboShader->SetSelectedIndex(ms_savedShaderIndex);
		m_selectedShader = m_shaderTemplates[ms_savedShaderIndex];
	}

	// Restore slider values
	if (m_sliderRadius)
	{
		m_sliderRadius->SetValue(ms_savedRadius, false);
		if (m_textRadius)
		{
			char buf[32];
			snprintf(buf, sizeof(buf), "%ld", ms_savedRadius);
			m_textRadius->SetText(Unicode::narrowToWide(buf));
		}
	}

	if (m_sliderBlend)
	{
		m_sliderBlend->SetValue(ms_savedBlend, false);
		if (m_textBlend)
		{
			char buf[32];
			snprintf(buf, sizeof(buf), "%ld", ms_savedBlend);
			m_textBlend->SetText(Unicode::narrowToWide(buf));
		}
	}

	if (m_sliderWidth)
	{
		m_sliderWidth->SetValue(ms_savedWidth, false);
		if (m_textWidth)
		{
			char buf[32];
			snprintf(buf, sizeof(buf), "%ld", ms_savedWidth);
			m_textWidth->SetText(Unicode::narrowToWide(buf));
		}
	}

	if (m_sliderHeight)
	{
		m_sliderHeight->SetValue(ms_savedHeight, false);
		if (m_textHeight)
		{
			char buf[32];
			snprintf(buf, sizeof(buf), "%ld", ms_savedHeight);
			m_textHeight->SetText(Unicode::narrowToWide(buf));
		}
	}

	if (m_sliderFlattenRadius)
	{
		m_sliderFlattenRadius->SetValue(ms_savedFlattenRadius, false);
		if (m_textFlattenRadius)
		{
			char buf[32];
			snprintf(buf, sizeof(buf), "%ld", ms_savedFlattenRadius);
			m_textFlattenRadius->SetText(Unicode::narrowToWide(buf));
		}
	}

	if (m_checkShowTileGrid)
		m_checkShowTileGrid->SetChecked(ms_savedShowTileGrid, false);
	syncPaintTileGridOverlay();
}

// ----------------------------------------------------------------------

void SwgCuiCityTerrainPainter::updateRemoveRegionButtonState()
{
	if (!m_buttonUndo)
		return;
	bool const canRemove =
		m_listRegions != nullptr &&
		m_listRegions->GetLastSelectedRow() >= 0 &&
		m_listRegions->GetLastSelectedRow() < static_cast<int>(m_regionListRowIds.size());
	m_buttonUndo->SetEnabled(canRemove);
}

// ----------------------------------------------------------------------

void SwgCuiCityTerrainPainter::removeSelectedRegion()
{
	if (!m_listRegions || m_cityId == 0)
	{
		if (m_textStatus)
			m_textStatus->SetText(Unicode::narrowToWide("Select a region in the list, then click Remove."));
		return;
	}

	int const selectedRow = m_listRegions->GetLastSelectedRow();
	if (selectedRow < 0 || selectedRow >= static_cast<int>(m_regionListRowIds.size()))
	{
		if (m_textStatus)
			m_textStatus->SetText(Unicode::narrowToWide("Select a region to remove."));
		return;
	}

	std::string const regionIdToRemove = m_regionListRowIds[static_cast<size_t>(selectedRow)];

	if (!CityTerrainLayerManager::isInstalled())
		return;

	CityTerrainLayerManager::getInstance().removeRegion(regionIdToRemove);

	CityTerrainRemoveRequestMessage const msg(m_cityId, regionIdToRemove);
	GameNetwork::send(msg, true);

	refreshRegionList();

	if (m_textStatus)
	{
		char buf[128];
		snprintf(buf, sizeof(buf), "Removed region: %s", regionIdToRemove.c_str());
		m_textStatus->SetText(Unicode::narrowToWide(buf));
	}

	REPORT_LOG(true, ("[Titan] SwgCuiCityTerrainPainter::removeSelectedRegion: %s\n", regionIdToRemove.c_str()));
}

// ----------------------------------------------------------------------

void SwgCuiCityTerrainPainter::refreshRegionList()
{
	m_regionListRowIds.clear();

	if (!m_listRegions)
		return;

	m_listRegions->Clear();

	if (m_cityId == 0 || !CityTerrainLayerManager::isInstalled())
	{
		updateRemoveRegionButtonState();
		return;
	}

	std::vector<CityTerrainLayerManager::TerrainRegion const *> regions;
	CityTerrainLayerManager::getInstance().getRegionsForCity(m_cityId, regions);

	for (size_t i = 0; i < regions.size(); ++i)
	{
		CityTerrainLayerManager::TerrainRegion const * const r = regions[i];
		if (!r)
			continue;

		char const * typeStr = "Circle";
		std::string displaySuffix;
		if (r->type == CityTerrainLayerManager::RT_FLATTEN)
		{
			typeStr = "Flatten";
			char hbuf[48];
			snprintf(hbuf, sizeof(hbuf), " h=%.0f", r->height);
			displaySuffix = hbuf;
		}
		else if (r->type == CityTerrainLayerManager::RT_SHADER_LINE)
			typeStr = "Line";

		char rowText[384];
		snprintf(rowText, sizeof(rowText), "%s  |  %s  |  %s  |  (%.0f, %.0f)  r=%.0f  w=%.0f  blend=%.0f%s",
			r->regionId.c_str(),
			typeStr,
			r->shaderTemplate.c_str(),
			r->centerX, r->centerZ, r->radius, r->width, r->blendDistance,
			displaySuffix.c_str());

		m_regionListRowIds.push_back(r->regionId);
		m_listRegions->AddRow(Unicode::narrowToWide(rowText), r->regionId);
	}

	updateRemoveRegionButtonState();
}

// ----------------------------------------------------------------------

void SwgCuiCityTerrainPainter::syncPaintTileGridOverlay()
{
	if (!CityTerrainLayerManager::isInstalled())
		return;
	bool const show = m_checkShowTileGrid && m_checkShowTileGrid->IsChecked();
	CityTerrainLayerManager::setPaintTileGridOverlay(show, m_cityId);
}

// ----------------------------------------------------------------------

void SwgCuiCityTerrainPainter::onCityRegionsChanged(int32 cityId)
{
	if (!ms_activeInstance || ms_activeInstance->m_cityId != cityId)
		return;
	ms_activeInstance->refreshRegionList();
}

// ----------------------------------------------------------------------

void SwgCuiCityTerrainPainter::onServerTerrainRegionsChanged(int32 cityId)
{
	if (!ms_activeInstance || !ms_activeInstance->isActive())
		return;
	// cityId == 0: refresh after paint response (message has no city id)
	if (cityId != 0 && ms_activeInstance->m_cityId != 0 && cityId != ms_activeInstance->m_cityId)
		return;
	ms_activeInstance->refreshRegionList();
}

// ----------------------------------------------------------------------

void SwgCuiCityTerrainPainter::restoreLayerManagerRegionCallbackIfPainterActive()
{
	if (!ms_activeInstance || !ms_activeInstance->isActive() || !CityTerrainLayerManager::isInstalled())
		return;
	CityTerrainLayerManager::setRegionChangeCallback(&SwgCuiCityTerrainPainter::onCityRegionsChanged);
	CityTerrainLayerManager::setPaintResponseCallback(&SwgCuiCityTerrainPainter::onPaintResponse);
	CityTerrainLayerManager::setCityTerrainUiRefreshFn(&SwgCuiCityTerrainPainter::onServerTerrainRegionsChanged);
	CityTerrainLayerManager::setOpenCityTerrainPainterAlreadyActiveFn(&SwgCuiCityTerrainPainter::onOpenCityTerrainPainterWhileAlreadyActive);
	SwgCuiTerraforming::restoreLayerManagerHooksIfTerraformingActive();
}

// ----------------------------------------------------------------------

void SwgCuiCityTerrainPainter::onOpenCityTerrainPainterWhileAlreadyActive()
{
	if (!ms_activeInstance || !ms_activeInstance->isActive())
		return;

	int32 const cityId = CityTerrainLayerManager::getPendingCityId();
	if (cityId == 0)
		return;

	ms_activeInstance->m_cityId = cityId;
	ms_savedCityId = cityId;

	if (CityTerrainLayerManager::isInstalled())
	{
		CityTerrainSyncRequestMessage const syncMsg(cityId);
		GameNetwork::send(syncMsg, true);
	}

	ms_activeInstance->refreshShaderList();
	ms_activeInstance->refreshRegionList();
	ms_activeInstance->updatePreview();
	ms_activeInstance->syncPaintTileGridOverlay();
}

// ----------------------------------------------------------------------

void SwgCuiCityTerrainPainter::onPaintResponse(bool success, std::string const & regionId, std::string const & errorMessage)
{
	if (!ms_activeInstance)
		return;

	if (!success)
	{
		if (!errorMessage.empty())
			CuiMessageBox::createInfoBox(Unicode::narrowToWide(errorMessage));
		else
			CuiMessageBox::createInfoBox(Unicode::narrowToWide("Terrain modification was rejected by the server."));
		return;
	}

	ms_activeInstance->refreshRegionList();

	if (ms_activeInstance->m_textStatus)
	{
		char buf[256];
		snprintf(buf, sizeof(buf), "Saved: %s", regionId.c_str());
		ms_activeInstance->m_textStatus->SetText(Unicode::narrowToWide(buf));
	}

	if (CityTerrainLayerManager::isInstalled())
		CityTerrainLayerManager::getInstance().flushTerrainUpdates();
}

// ======================================================================




