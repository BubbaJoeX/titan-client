//======================================================================
//
// CuiColorPicker.cpp
// copyright(c) 2002 Sony Online Entertainment
//
//======================================================================

#include "clientUserInterface/FirstClientUserInterface.h"
#include "clientUserInterface/CuiColorPicker.h"

#include "UIButton.h"
#include "UIData.h"
#include "UIDataSource.h"
#include "UIImage.h"
#include "UIMessage.h"
#include "UIPage.h"
#include "UISliderbar.h"
#include "UIText.h"
#include "UITextbox.h"
#include "UIUtils.h"
#include "UIVolumePage.h"
#include "UnicodeUtils.h"
#include "clientGame/TangibleObject.h"
#include "clientUserInterface/CuiUtils.h"
#include "sharedFoundation/CrcLowerString.h"
#include "sharedGame/CustomizationManager.h"
#include "sharedMath/PaletteArgb.h"
#include "sharedObject/CachedNetworkId.h"
#include "sharedObject/NetworkIdManager.h"
#include "sharedObject/CustomizationData.h"
#include "sharedObject/PaletteColorCustomizationVariable.h"

#include <list>
#include <map>
#include <cmath>
#include <algorithm>

//======================================================================

namespace CuiColorPickerNamespace
{
	namespace PathPrefixes
	{
		const std::string shared_owner = "/shared_owner/";
		const std::string priv = "/private/";
	}

	CustomizationVariable * findVariable(CustomizationData & cdata, const std::string & partialName, CuiColorPicker::PathFlags flags)
	{
		CustomizationVariable * cv = cdata.findVariable(partialName);

		if (!cv)
		{
			if ((flags & CuiColorPicker::PF_shared) != 0)
				cv = cdata.findVariable(PathPrefixes::shared_owner + partialName);

			if (!cv &&(flags & CuiColorPicker::PF_private) != 0)
				cv = cdata.findVariable(PathPrefixes::priv + partialName);
		}

		return cv;
	}

//	typedef CuiColorPicker::StringIntMap StringIntMap;
//	StringIntMap s_paletteColumnData;
	std::map<std::string, CustomizationManager::PaletteColumns> s_paletteColumnDataTableData;

	std::string getBasename(const std::string & path)
	{
		std::string basename(path);

		const size_t slashpos = basename.rfind('/');
		const size_t dotpos = basename.rfind('.');

		if (slashpos == std::string::npos) //lint !e650 !e737
			return basename.substr(0, dotpos);
		else if (dotpos == std::string::npos) //lint !e650 !e737
			return basename.substr(slashpos + 1);
		else
			return basename.substr(slashpos + 1,(dotpos - slashpos) - 1);
	}
}

using namespace CuiColorPickerNamespace;

//----------------------------------------------------------------------

const UILowerString CuiColorPicker::Properties::AutoSizePaletteCells = UILowerString("AutoSizePaletteCells");

//----------------------------------------------------------------------

const UILowerString CuiColorPicker::DataProperties::SelectedIndex = UILowerString("SelectedIndex");
const UILowerString CuiColorPicker::DataProperties::TargetNetworkId = UILowerString("TargetNetworkId");
const UILowerString CuiColorPicker::DataProperties::TargetRangeMin = UILowerString("TargetRangeMin");
const UILowerString CuiColorPicker::DataProperties::TargetRangeMax = UILowerString("TargetRangeMax");
const UILowerString CuiColorPicker::DataProperties::TargetValue = UILowerString("TargetValue");
const UILowerString CuiColorPicker::DataProperties::TargetVariable = UILowerString("TargetVariable");


//----------------------------------------------------------------------

CuiColorPicker::CuiColorPicker(UIPage & page) :
CuiMediator("CuiColorPicker", page),
UIEventCallback(),
m_volumePage(0),
m_buttonCancel(0),
m_buttonRevert(0),
m_buttonClose(0),
m_pageSample(0),
m_originalSelection(0),
m_rangeMin(0),
m_rangeMax(0),
m_palette(0),
m_targetObject(new ObjectWatcher),
m_targetVariable(),
m_sampleElement(0),
m_linkedObjects(new ObjectWatcherVector),
m_autoSizePaletteCells(false),
m_text(0),
m_forceColumns(0),
m_autoForceColumns(false),
m_changed(false),
m_lastSize(),
m_paletteSource(PS_target),
m_colorWheelMode(false),
m_buttonToggleMode(0),
m_pageColorWheel(0),
m_pagePaletteGrid(0),
m_textboxHtml(0),
m_pageMatchedColor(0),
m_directColor(PackedArgb::solidWhite),
m_useDirectColor(false),
m_originalDirectColor(PackedArgb::solidWhite),
m_pageWheel(0),
m_cursorWheel(0),
m_textboxR(0),
m_textboxG(0),
m_textboxB(0),
m_hue(0.0f),
m_saturation(1.0f),
m_colorValue(1.0f),
m_draggingWheel(false)
{
	getCodeDataObject(TUIVolumePage, m_volumePage, "volumePage");
	getCodeDataObject(TUIButton, m_buttonCancel, "buttonCancel", true);
	getCodeDataObject(TUIButton, m_buttonRevert, "buttonRevert", true);
	getCodeDataObject(TUIPage, m_pageSample, "pageSample", true);
	getCodeDataObject(TUIWidget, m_sampleElement, "sampleElement", true);
	getCodeDataObject(TUIWidget, m_text, "text", true);

	// Color wheel / HTML color support (optional UI elements)
	getCodeDataObject(TUIPage, m_pageColorWheel, "pageColorWheel", true);
	getCodeDataObject(TUIPage, m_pagePaletteGrid, "pagePaletteGrid", true);
	getCodeDataObject(TUITextbox, m_textboxHtml, "textboxHtml", true);
	getCodeDataObject(TUIPage, m_pageMatchedColor, "pageMatchedColor", true);

	// Color wheel interactive elements
	getCodeDataObject(TUIPage, m_pageWheel, "pageWheel", true);
	getCodeDataObject(TUIWidget, m_cursorWheel, "cursorWheel", true);
	getCodeDataObject(TUITextbox, m_textboxR, "textR", true);
	getCodeDataObject(TUITextbox, m_textboxG, "textG", true);
	getCodeDataObject(TUITextbox, m_textboxB, "textB", true);

	if(getButtonClose())
		m_buttonClose = getButtonClose();

	m_volumePage->Clear();

	// Show color wheel mode by default if available
	if (m_pageColorWheel)
	{
		m_pageColorWheel->SetVisible(true);
		m_colorWheelMode = true;
	}

	IGNORE_RETURN(setState(MS_closeable));
}

//----------------------------------------------------------------------

CuiColorPicker::~CuiColorPicker()
{
	revert();
	reset();

	m_volumePage = 0;
	m_buttonCancel = 0;
	m_buttonRevert = 0;
	m_buttonClose = 0;
	m_pageSample = 0;
	m_text = 0;
	m_buttonToggleMode = 0;
	m_pageColorWheel = 0;
	m_pagePaletteGrid = 0;
	m_textboxHtml = 0;
	m_pageMatchedColor = 0;
	m_pageWheel = 0;
	m_cursorWheel = 0;
	m_textboxR = 0;
	m_textboxG = 0;
	m_textboxB = 0;

	if (m_palette)
	{
		m_palette->release();
		m_palette = 0;
	}

	delete m_targetObject;
	m_targetObject = 0;

	delete m_linkedObjects;
	m_linkedObjects = 0;
}

//----------------------------------------------------------------------

void CuiColorPicker::performActivate()
{
	handleMediatorPropertiesChanged();

	// Register toggle mode button callback (optional; GM UI may omit)
	if (m_buttonToggleMode)
		m_buttonToggleMode->AddCallback(this);

	// Register HTML textbox callback
	if (m_textboxHtml)
		m_textboxHtml->AddCallback(this);

	// Register RGB textbox callbacks
	if (m_textboxR)
		m_textboxR->AddCallback(this);
	if (m_textboxG)
		m_textboxG->AddCallback(this);
	if (m_textboxB)
		m_textboxB->AddCallback(this);

	// Register wheel for mouse input
	if (m_pageWheel)
	{
		m_pageWheel->AddCallback(this);
		m_pageWheel->SetAbsorbsInput(true);
		m_pageWheel->SetGetsInput(true);
	}

	// Register parent page to track mouse during drag
	getPage().AddCallback(this);
	getPage().SetGetsInput(true);

	setIsUpdating(true);
}

//----------------------------------------------------------------------

void CuiColorPicker::performDeactivate()
{
	setIsUpdating(false);

	// Unregister toggle mode button callback
	if (m_buttonToggleMode)
		m_buttonToggleMode->RemoveCallback(this);

	// Unregister HTML textbox callback
	if (m_textboxHtml)
		m_textboxHtml->RemoveCallback(this);

	// Unregister RGB textbox callbacks
	if (m_textboxR)
		m_textboxR->RemoveCallback(this);
	if (m_textboxG)
		m_textboxG->RemoveCallback(this);
	if (m_textboxB)
		m_textboxB->RemoveCallback(this);

	// Unregister wheel callbacks
	if (m_pageWheel)
		m_pageWheel->RemoveCallback(this);

	// Unregister parent page callback
	getPage().RemoveCallback(this);

	if (m_buttonCancel)
		m_buttonCancel->RemoveCallback(this);
	if (m_buttonRevert)
		m_buttonRevert->RemoveCallback(this);

	m_volumePage->RemoveCallback(this);

	storeProperties();

	reset();
}

//----------------------------------------------------------------------

void CuiColorPicker::OnButtonPressed(UIWidget *context)
{
	if (context == m_buttonCancel || context == m_buttonClose)
	{
		revert();
	}
	else if (context == m_buttonRevert)
	{
		revert();
	}
	else if (context == m_buttonToggleMode)
	{
		// Toggle between palette grid and color wheel mode
		setColorWheelMode(!m_colorWheelMode);
	}
}

//----------------------------------------------------------------------

void CuiColorPicker::OnVolumePageSelectionChanged(UIWidget * context)
{
	if (context == m_volumePage)
	{
		const int index = m_volumePage->GetLastSelectedIndex();

		updateValue(index);

		// When selecting from palette, clear direct color mode
		m_useDirectColor = false;
		updateHtmlTextbox();

		if (checkAndResetChanged())
			m_userChanged = true;
	}
}

//----------------------------------------------------------------------

void CuiColorPicker::OnTextboxChanged(UIWidget * context)
{
	if (context == m_textboxHtml)
	{
		Unicode::String text;
		m_textboxHtml->GetLocalText(text);
		std::string htmlColor = Unicode::wideToNarrow(text);

		if (PackedArgb::isValidHtmlColor(htmlColor.c_str()))
		{
			PackedArgb color = PackedArgb::fromHtmlString(htmlColor.c_str());
			setDirectColor(color);
		}
	}
	else if (context == m_textboxR || context == m_textboxG || context == m_textboxB)
	{
		// Get RGB values from textboxes
		uint8 r = m_directColor.getR();
		uint8 g = m_directColor.getG();
		uint8 b = m_directColor.getB();

		if (m_textboxR)
		{
			Unicode::String text;
			m_textboxR->GetLocalText(text);
			int val = atoi(Unicode::wideToNarrow(text).c_str());
			r = static_cast<uint8>(std::max(0, std::min(255, val)));
		}
		if (m_textboxG)
		{
			Unicode::String text;
			m_textboxG->GetLocalText(text);
			int val = atoi(Unicode::wideToNarrow(text).c_str());
			g = static_cast<uint8>(std::max(0, std::min(255, val)));
		}
		if (m_textboxB)
		{
			Unicode::String text;
			m_textboxB->GetLocalText(text);
			int val = atoi(Unicode::wideToNarrow(text).c_str());
			b = static_cast<uint8>(std::max(0, std::min(255, val)));
		}

		m_directColor.setArgb(255, r, g, b);
		m_useDirectColor = true;
		rgbToHsv(r, g, b, m_hue, m_saturation, m_colorValue);

		updateWheelCursor();
		updateHtmlTextbox();

		if (m_pageSample)
			m_pageSample->SetBackgroundTint(UIColor(r, g, b));

		// Apply to target
		TangibleObject * const object = m_targetObject->getPointer();
		if (object)
			updateValueDirectColor(*object, m_directColor);

		// Apply to linked objects
		for (ObjectWatcherVector::iterator it = m_linkedObjects->begin(); it != m_linkedObjects->end(); ++it)
		{
			TangibleObject * const linked_object = *it;
			if (linked_object)
				updateValueDirectColor(*linked_object, m_directColor, PF_private);
		}

		m_changed = true;
		m_userChanged = true;
	}
}

//----------------------------------------------------------------------

void CuiColorPicker::reset()
{
	if (m_palette)
	{
		m_rangeMax = std::min (m_rangeMax, m_palette->getEntryCount ());
		UIBaseObject::UIObjectList olist;
		m_volumePage->GetChildren(olist);

		char buf[64];

		const int num_children = static_cast<int>(olist.size());
		int count = 0;

		{
			UIBaseObject::UIObjectList::iterator it = olist.begin();
			for (int i = m_rangeMin; i < m_rangeMax; ++i, ++count)
			{
				bool error = false;
				const PackedArgb & pargb = m_palette->getEntry(i, error);
				WARNING(error, ("CuiColorPicker reset error"));

				UIWidget * element = 0;

				if (count >= num_children)
				{
					if (m_sampleElement)
						element = safe_cast<UIWidget *>(m_sampleElement->DuplicateObject());
					else
					{
						element = new UIImage;
					}

					NOT_NULL(element);
					m_volumePage->AddChild(element);
				}
				else
					element = dynamic_cast<UIWidget *>(*(it++));

				NOT_NULL (element);

				snprintf (buf, sizeof (buf), "%d", i);

				element->SetName              (buf);
				element->SetGetsInput         (true);
				element->SetBackgroundOpacity (1.0f);
				element->SetBackgroundColor   (UIColor::white);
				element->SetEnabled           (true);
				element->SetGetsInput         (true);
				element->SetVisible           (true);

				const UIColor & tint = CuiUtils::convertColor (pargb);
				element->SetBackgroundTint    (tint);
			}
		}

		if (count < num_children)
		{
			UIBaseObject::UIObjectList::iterator it = olist.begin();
			std::advance(it, count);

			for (; it != olist.end(); ++it)
			{
				m_volumePage->RemoveChild(*it);
			}
		}

		updateCellSizes ();

		m_volumePage->Link ();
	}

	else
		m_volumePage->Clear();
}

//----------------------------------------------------------------------

int CuiColorPicker::getValue() const
{
	TangibleObject * const object = m_targetObject->getPointer();

	if (object)
	{
		CustomizationData * const cdata = object->fetchCustomizationData();
		if (cdata)
		{
			const PaletteColorCustomizationVariable * const var = dynamic_cast<PaletteColorCustomizationVariable *>(findVariable(*cdata, m_targetVariable, PF_any));
			int value = -1;

			if (var)
				value = var->getValue();

			cdata->release();
			return value;
		}
	}
	else if (m_paletteSource == PS_palette)
	{
		return m_volumePage->GetLastSelectedIndex();
	}

	return -1;
}

//----------------------------------------------------------------------

void CuiColorPicker::updateValue(TangibleObject & obj, int index, PathFlags flags)
{
	CustomizationData * const cdata = obj.fetchCustomizationData();
	if (cdata)
	{
		PaletteColorCustomizationVariable * const var = dynamic_cast<PaletteColorCustomizationVariable *>(findVariable(*cdata, m_targetVariable, flags));

		if (var)
		{
			if (var->getValue() != index)
			{
				var->setValue(index);
				m_changed = true;
			}

		}
		cdata->release();
	}

	//store the property so that the subscription can grab it
	char buf [256];
	_itoa(index, buf, 10);
	std::string b = buf;
	getPage().SetProperty(DataProperties::SelectedIndex, Unicode::narrowToWide(b));
} //lint !e1762 //not const

//----------------------------------------------------------------------

void CuiColorPicker::updateValue(int index)
{
	if (index >= 0)
	{
		TangibleObject * const object = m_targetObject->getPointer();

		if (object)
		{
			updateValue(*object, index);
		}
		else if (m_paletteSource == PS_palette)
		{
			m_changed = true;
		}

		for (ObjectWatcherVector::iterator it = m_linkedObjects->begin(); it != m_linkedObjects->end(); ++it)
		{
			TangibleObject * const linked_object = *it;
			if (linked_object)
				updateValue(*linked_object, index, PF_private);
		}

		if (m_palette && m_pageSample)
		{
			bool error = false;
			const PackedArgb & pargb = m_palette->getEntry(index, error);
			WARNING(error, ("CuiColorPicker updateValue error"));
			const UIColor tint(CuiUtils::convertColor(pargb));
			m_pageSample->SetBackgroundTint(tint);
		}

		if (m_buttonRevert)
			m_buttonRevert->SetEnabled(m_originalSelection != index);

		Unicode::String str;
		UIUtils::FormatInteger(str, index);
		getPage().SetProperty(DataProperties::TargetValue, str);

	}
}

//----------------------------------------------------------------------

void CuiColorPicker::revert()
{
	updateValue(m_originalSelection);
}

//----------------------------------------------------------------------

void CuiColorPicker::setTarget(const NetworkId & id, const std::string & var, int rangeMin, int rangeMax)
{
	setTarget(NetworkIdManager::getObjectById(id), var, rangeMin, rangeMax);
}

//----------------------------------------------------------------------

void CuiColorPicker::setTarget(Object * obj, const std::string & var, int rangeMin, int rangeMax)
{
	m_paletteSource = PS_target;

	*m_targetObject = dynamic_cast<TangibleObject *>(obj);


	if (m_palette)
	{
		m_palette->release();
		m_palette = 0;
	}

	int actualRangeMin = 0;
	int actualRangeMax = 0;

	m_targetVariable = var;

	if (!m_targetVariable.empty ())
	{
		TangibleObject * const object = m_targetObject->getPointer ();

		if (object)
		{
			WARNING(rangeMax <= rangeMin,("CuiColorPicker::setTarget invalid range for [%s]:[%s], rangeMin=%d, rangeMax=%d", Unicode::wideToNarrow(object->getLocalizedName()).c_str(), var.c_str(), rangeMin, rangeMax));

			CustomizationData * const cdata = object->fetchCustomizationData ();
			if (cdata)
			{
				PaletteColorCustomizationVariable * const cvar = dynamic_cast<PaletteColorCustomizationVariable *>(findVariable (*cdata, m_targetVariable, PF_any));
				if (cvar)
				{
					m_palette           = cvar->fetchPalette ();
					m_originalSelection = cvar->getValue ();
					cvar->getRange (actualRangeMin, actualRangeMax);
				}
				else
					WARNING (true, ("color picker could not find variable '%s'", m_targetVariable.c_str ()));

				cdata->release ();
			}
		}
	}

	m_rangeMin = std::max (rangeMin, actualRangeMin);
	m_rangeMax = std::min (rangeMax, actualRangeMax);

	storeProperties();

	m_linkedObjects->clear();

	reset();

	m_volumePage->SetSelectionIndex(m_originalSelection);

	updateValue(m_originalSelection);

	const UIWidget * const child = m_volumePage->GetLastSelectedChild();

	if (child)
		m_volumePage->CenterChild(*child);
	else
		m_volumePage->SetScrollLocation(UIPoint(0L, 0L));
}

//----------------------------------------------------------------------

void CuiColorPicker::setLinkedObjects(const ObjectWatcherVector & v, bool doUpdate)
{
	*m_linkedObjects = v;
	const int index = m_volumePage->GetLastSelectedIndex();
	if (doUpdate)
		updateValue(index);
}

//----------------------------------------------------------------------

void CuiColorPicker::setPalette(const PaletteArgb *palette)
{
	// Clean up the previous palette

	if (m_palette)
	{
		m_palette->release();
		m_palette = 0;
	}

	m_paletteSource = PS_palette;

	// Set the new palette

	palette->fetch();
	m_palette = palette;

	if (m_autoForceColumns && palette)
	{
		m_forceColumns = 0;
	}

	m_autoSizePaletteCells = true;

	// What does all this code do?

	if (palette != NULL)
	{
		m_rangeMin = 0;
		m_rangeMax = palette->getEntryCount();
	}

	storeProperties();

	m_linkedObjects->clear();

	reset();

	m_volumePage->SetSelectionIndex(m_originalSelection);

	const UIWidget * const child = m_volumePage->GetLastSelectedChild();

	if (child)
		m_volumePage->CenterChild(*child);
	else
		m_volumePage->SetScrollLocation(UIPoint(0L, 0L));
}

//----------------------------------------------------------------------

PaletteArgb const * CuiColorPicker::getPalette() const
{
	return m_palette;
}

//----------------------------------------------------------------------

void CuiColorPicker::updateCellSizes()
{
	getPage().ForcePackChildren();

	if (m_autoSizePaletteCells && m_palette)
	{
		int const count = std::min(m_rangeMax - m_rangeMin, m_palette->getEntryCount());
		
		if (count) 
		{
			if (m_forceColumns)
			{
				long const width = m_volumePage->GetWidth();
				long const height = m_volumePage->GetHeight();
			
				if (!width || !height)
					return;
				UIPoint cellSize;
				
				int const ny = (count + m_forceColumns - 1) / m_forceColumns;
				
				cellSize.x = width / m_forceColumns;
				if (ny)
					cellSize.y = height / ny;
				else
					cellSize.y = 1;
				
				m_volumePage->SetCellSize(cellSize);
				m_volumePage->SetCellPadding(UISize::zero);
			}
			else
			{
				IGNORE_RETURN(m_volumePage->OptimizeChildSpacing(count));
			}
		}
	}
}

//----------------------------------------------------------------------

void CuiColorPicker::setText(const Unicode::String & str)
{
	if (m_text)
		m_text->SetText (str);
	else
		WARNING(true,("Attempt to set text on CuiColorPicker with no text widget."));
}

//----------------------------------------------------------------------

void CuiColorPicker::setForceColumns(int cols)
{
	m_forceColumns = cols;
}

//----------------------------------------------------------------------

void CuiColorPicker::setAutoForceColumns(bool b)
{
	m_autoForceColumns = b;
	m_forceColumns = 0;
}


//----------------------------------------------------------------------

void CuiColorPicker::setupPaletteColumnData(std::map<std::string, CustomizationManager::PaletteColumns> const & data)
{
	s_paletteColumnDataTableData = data;
}

//----------------------------------------------------------------------

void CuiColorPicker::storeProperties()
{
	getPage().SetPropertyNarrow(DataProperties::TargetVariable, m_targetVariable);
	std::string networkIdString;
	if (*m_targetObject != NULL)
		networkIdString = m_targetObject->getPointer()->getNetworkId().getValueString();

	getPage().SetPropertyNarrow(DataProperties::TargetNetworkId, networkIdString);
	getPage().SetPropertyInteger(DataProperties::TargetRangeMin, m_rangeMin);
	getPage().SetPropertyInteger(DataProperties::TargetRangeMax, m_rangeMax);
}

//----------------------------------------------------------------------

void CuiColorPicker::update(float deltaTimeSecs)
{
	CuiMediator::update(deltaTimeSecs);
	updateCellSizes();
}

//----------------------------------------------------------------------

void CuiColorPicker::setIndex(int index)
{
	m_volumePage->SetSelectionIndex(index);
}

//----------------------------------------------------------------------

void CuiColorPicker::setMaximumPaletteIndex(int const index)
{
	m_rangeMax = index;
}

//----------------------------------------------------------------------

void CuiColorPicker::handleMediatorPropertiesChanged()
{
	std::string str;
	IGNORE_RETURN(getPage().GetPropertyBoolean(Properties::AutoSizePaletteCells, m_autoSizePaletteCells));
	
	if (getPage().GetPropertyNarrow(DataProperties::TargetNetworkId, str))
		*m_targetObject = dynamic_cast<TangibleObject *>(NetworkIdManager::getObjectById(NetworkId(str)));
	else
		*m_targetObject = NULL;

	m_targetVariable.clear();

	getPage ().ForcePackChildren ();
	getPage().GetPropertyNarrow(DataProperties::TargetVariable, m_targetVariable);
	getPage().GetPropertyInteger(DataProperties::TargetRangeMin, m_rangeMin);
	m_rangeMin = std::max(0, m_rangeMin);
	getPage().GetPropertyInteger(DataProperties::TargetRangeMax, m_rangeMax);

	if (m_paletteSource == PS_target)
	{
		setTarget(m_targetObject->getPointer(), m_targetVariable, m_rangeMin, m_rangeMax);
	}
	else if (m_paletteSource == PS_palette)
	{
		setPalette(m_palette);
	}

	if (m_buttonCancel)
		m_buttonCancel->AddCallback(this);
	if (m_buttonRevert)
		m_buttonRevert->AddCallback(this);

	m_volumePage->AddCallback(this);

	m_volumePage->SetSelectionIndex(m_originalSelection);

	m_changed = m_userChanged = false;
	updateCellSizes ();
}

//----------------------------------------------------------------------

void CuiColorPicker::setColorWheelMode(bool enabled)
{
	m_colorWheelMode = enabled;

	// Toggle visibility of palette grid vs color wheel
	if (m_pagePaletteGrid)
		m_pagePaletteGrid->SetVisible(!enabled);
	else if (m_volumePage)
		m_volumePage->SetVisible(!enabled);

	if (m_pageColorWheel)
		m_pageColorWheel->SetVisible(enabled);

	// Update button text
	if (m_buttonToggleMode)
	{
		if (enabled)
			m_buttonToggleMode->SetText(UIString(Unicode::narrowToWide("Palette Mode")));
		else
			m_buttonToggleMode->SetText(UIString(Unicode::narrowToWide("Color Wheel")));
	}
}

//----------------------------------------------------------------------

bool CuiColorPicker::isColorWheelMode() const
{
	return m_colorWheelMode;
}

//----------------------------------------------------------------------

void CuiColorPicker::setColorFromHtml(const char * htmlColor)
{
	if (!htmlColor || !PackedArgb::isValidHtmlColor(htmlColor))
		return;

	PackedArgb color = PackedArgb::fromHtmlString(htmlColor);
	setDirectColor(color);
}

//----------------------------------------------------------------------

void CuiColorPicker::getColorAsHtml(char * buffer, int bufferSize) const
{
	if (m_useDirectColor)
	{
		m_directColor.toHtmlString(buffer, bufferSize, false);
	}
	else if (m_palette)
	{
		bool error = false;
		int index = m_volumePage ? m_volumePage->GetLastSelectedIndex() : 0;
		index = std::max(0, std::min(index, m_palette->getEntryCount() - 1));
		PackedArgb const & color = m_palette->getEntry(index, error);
		if (!error)
			color.toHtmlString(buffer, bufferSize, false);
	}
}

//----------------------------------------------------------------------

void CuiColorPicker::setDirectColor(const PackedArgb & color)
{
	m_directColor = color;
	m_useDirectColor = true;

	// Update the preview
	if (m_pageSample)
	{
		m_pageSample->SetBackgroundTint(UIColor(color.getR(), color.getG(), color.getB()));
	}

	// Update HTML textbox
	updateHtmlTextbox();

	// Find closest palette match and show it
	if (m_palette)
	{
		int matchedIndex = m_palette->findClosestMatch(color);

		// Show matched color in the matched color preview
		if (m_pageMatchedColor)
		{
			bool error = false;
			PackedArgb const & matchedColor = m_palette->getEntry(matchedIndex, error);
			if (!error)
			{
				m_pageMatchedColor->SetBackgroundTint(UIColor(matchedColor.getR(), matchedColor.getG(), matchedColor.getB()));
			}
		}

		// Update the target object with the direct color
		TangibleObject * const object = m_targetObject->getPointer();
		if (object)
		{
			updateValueDirectColor(*object, color);
		}

		// Update linked objects
		for (ObjectWatcherVector::iterator it = m_linkedObjects->begin(); it != m_linkedObjects->end(); ++it)
		{
			TangibleObject * const linked_object = *it;
			if (linked_object)
				updateValueDirectColor(*linked_object, color, PF_private);
		}
	}

	m_changed = true;
	m_userChanged = true;
}

//----------------------------------------------------------------------

const PackedArgb & CuiColorPicker::getDirectColor() const
{
	return m_directColor;
}

//----------------------------------------------------------------------

bool CuiColorPicker::hasDirectColor() const
{
	return m_useDirectColor;
}

//----------------------------------------------------------------------

void CuiColorPicker::updateValueDirectColor(TangibleObject & obj, const PackedArgb & color, PathFlags flags)
{
	CustomizationData * const cdata = obj.fetchCustomizationData();
	if (cdata)
	{
		PaletteColorCustomizationVariable * const var = dynamic_cast<PaletteColorCustomizationVariable *>(findVariable(*cdata, m_targetVariable, flags));

		if (var)
		{
			var->setDirectColor(color);
			m_changed = true;
		}
		cdata->release();
	}
}

//----------------------------------------------------------------------

void CuiColorPicker::onColorWheelChanged(const PackedArgb & color)
{
	setDirectColor(color);
}

//----------------------------------------------------------------------

void CuiColorPicker::updateHtmlTextbox()
{
	if (m_textboxHtml)
	{
		char htmlColor[16];
		if (m_useDirectColor)
		{
			m_directColor.toHtmlString(htmlColor, sizeof(htmlColor), false);
		}
		else if (m_palette)
		{
			bool error = false;
			int index = m_volumePage ? m_volumePage->GetLastSelectedIndex() : 0;
			index = std::max(0, std::min(index, m_palette->getEntryCount() - 1));
			PackedArgb const & color = m_palette->getEntry(index, error);
			if (!error)
				color.toHtmlString(htmlColor, sizeof(htmlColor), false);
			else
				snprintf(htmlColor, sizeof(htmlColor), "#000000");
		}
		else
		{
			snprintf(htmlColor, sizeof(htmlColor), "#000000");
		}
		m_textboxHtml->SetLocalText(Unicode::narrowToWide(htmlColor));
	}
}

//----------------------------------------------------------------------

void CuiColorPicker::updateRgbTextboxes()
{
	if (m_textboxR)
	{
		char buf[8];
		snprintf(buf, sizeof(buf), "%d", m_directColor.getR());
		m_textboxR->SetLocalText(Unicode::narrowToWide(buf));
	}
	if (m_textboxG)
	{
		char buf[8];
		snprintf(buf, sizeof(buf), "%d", m_directColor.getG());
		m_textboxG->SetLocalText(Unicode::narrowToWide(buf));
	}
	if (m_textboxB)
	{
		char buf[8];
		snprintf(buf, sizeof(buf), "%d", m_directColor.getB());
		m_textboxB->SetLocalText(Unicode::narrowToWide(buf));
	}
}

//----------------------------------------------------------------------

bool CuiColorPicker::OnMessage(UIWidget * context, const UIMessage & msg)
{
	// Handle mouse input on the color wheel image or parent page during drag
	bool isWheelContext = (context == m_pageWheel);
	bool isParentDrag = (m_draggingWheel && (context == &getPage()));

	if (isWheelContext || isParentDrag)
	{
		if (msg.Type == UIMessage::LeftMouseDown && isWheelContext)
		{
			m_draggingWheel = true;
			handleWheelInput(msg.MouseCoords.x, msg.MouseCoords.y);
			return false;
		}
		else if (msg.Type == UIMessage::LeftMouseUp)
		{
			if (m_draggingWheel)
			{
				// Convert coords to wheel-local if needed
				UIPoint localCoords = msg.MouseCoords;
				if (!isWheelContext && m_pageWheel)
				{
					UIPoint wheelScreenPos;
					m_pageWheel->GetWorldLocation(wheelScreenPos);
					UIPoint contextScreenPos;
					if (context)
						context->GetWorldLocation(contextScreenPos);
					localCoords.x = msg.MouseCoords.x + contextScreenPos.x - wheelScreenPos.x;
					localCoords.y = msg.MouseCoords.y + contextScreenPos.y - wheelScreenPos.y;
				}
				handleWheelInput(localCoords.x, localCoords.y);
				m_draggingWheel = false;
			}
			return false;
		}
		else if (msg.Type == UIMessage::MouseMove)
		{
			// Auto-update color preview as cursor moves while dragging
			if (m_draggingWheel)
			{
				// Convert coords to wheel-local if needed
				UIPoint localCoords = msg.MouseCoords;
				if (!isWheelContext && m_pageWheel)
				{
					UIPoint wheelScreenPos;
					m_pageWheel->GetWorldLocation(wheelScreenPos);
					UIPoint contextScreenPos;
					if (context)
						context->GetWorldLocation(contextScreenPos);
					localCoords.x = msg.MouseCoords.x + contextScreenPos.x - wheelScreenPos.x;
					localCoords.y = msg.MouseCoords.y + contextScreenPos.y - wheelScreenPos.y;
				}
				handleWheelInput(localCoords.x, localCoords.y);
			}
			return false;
		}
	}

	return true;
}

//----------------------------------------------------------------------

void CuiColorPicker::rgbToHsv(uint8 r, uint8 g, uint8 b, float & h, float & s, float & v)
{
	float rf = r / 255.0f;
	float gf = g / 255.0f;
	float bf = b / 255.0f;

	float maxC = std::max(rf, std::max(gf, bf));
	float minC = std::min(rf, std::min(gf, bf));
	float delta = maxC - minC;

	v = maxC;

	if (maxC > 0.0f)
		s = delta / maxC;
	else
		s = 0.0f;

	if (delta < 0.00001f)
	{
		h = 0.0f;
	}
	else
	{
		if (rf >= maxC)
			h = (gf - bf) / delta;
		else if (gf >= maxC)
			h = 2.0f + (bf - rf) / delta;
		else
			h = 4.0f + (rf - gf) / delta;

		h *= 60.0f;
		if (h < 0.0f)
			h += 360.0f;
	}
}

//----------------------------------------------------------------------

void CuiColorPicker::hsvToRgb(float h, float s, float v, uint8 & r, uint8 & g, uint8 & b)
{
	if (s <= 0.0f)
	{
		r = g = b = static_cast<uint8>(v * 255.0f);
		return;
	}

	float hh = h;
	if (hh >= 360.0f)
		hh = 0.0f;
	hh /= 60.0f;

	int i = static_cast<int>(hh);
	float ff = hh - i;
	float p = v * (1.0f - s);
	float q = v * (1.0f - (s * ff));
	float t = v * (1.0f - (s * (1.0f - ff)));

	float rf, gf, bf;
	switch (i)
	{
	case 0:
		rf = v; gf = t; bf = p;
		break;
	case 1:
		rf = q; gf = v; bf = p;
		break;
	case 2:
		rf = p; gf = v; bf = t;
		break;
	case 3:
		rf = p; gf = q; bf = v;
		break;
	case 4:
		rf = t; gf = p; bf = v;
		break;
	default:
		rf = v; gf = p; bf = q;
		break;
	}

	r = static_cast<uint8>(rf * 255.0f);
	g = static_cast<uint8>(gf * 255.0f);
	b = static_cast<uint8>(bf * 255.0f);
}

//----------------------------------------------------------------------

void CuiColorPicker::updateFromHSV()
{
	uint8 r, g, b;
	hsvToRgb(m_hue, m_saturation, m_colorValue, r, g, b);
	m_directColor.setArgb(255, r, g, b);
	m_useDirectColor = true;

	updateRgbTextboxes();
	updateWheelCursor();
	updateHtmlTextbox();

	if (m_pageSample)
		m_pageSample->SetBackgroundTint(UIColor(r, g, b));

	// Apply to target
	TangibleObject * const object = m_targetObject->getPointer();
	if (object)
		updateValueDirectColor(*object, m_directColor);

	m_changed = true;
	m_userChanged = true;
}

//----------------------------------------------------------------------

void CuiColorPicker::updateFromRGB()
{
	rgbToHsv(m_directColor.getR(), m_directColor.getG(), m_directColor.getB(),
	         m_hue, m_saturation, m_colorValue);
	updateFromHSV();
}

//----------------------------------------------------------------------

void CuiColorPicker::updateWheelCursor()
{
	if (!m_cursorWheel || !m_pageWheel)
		return;

	// Get the actual size of the wheel page
	UISize wheelSize = m_pageWheel->GetSize();
	const float imgCenterX = wheelSize.x * 0.5f;
	const float imgCenterY = wheelSize.y * 0.5f;
	const float imgRadius = std::min(imgCenterX, imgCenterY);

	// Place cursor at (hue angle, saturation distance) on the wheel
	// Red is at bottom (hue 0 = positive Y direction)
	float angle = m_hue * 3.14159265f / 180.0f;
	float radius = m_saturation * imgRadius;

	// For red at bottom: X = -sin(angle), Y = cos(angle)
	int cursorX = static_cast<int>(imgCenterX - radius * sinf(angle));
	int cursorY = static_cast<int>(imgCenterY + radius * cosf(angle));

	UISize cursorSize = m_cursorWheel->GetSize();
	m_cursorWheel->SetLocation(cursorX - cursorSize.x / 2, cursorY - cursorSize.y / 2);
}


//----------------------------------------------------------------------

void CuiColorPicker::handleWheelInput(int x, int y)
{
	if (!m_pageWheel)
		return;

	// Get the actual size of the wheel page
	UISize wheelSize = m_pageWheel->GetSize();
	const float imgCenterX = wheelSize.x * 0.5f;
	const float imgCenterY = wheelSize.y * 0.5f;
	const float imgRadius  = std::min(imgCenterX, imgCenterY);

	const float dx = static_cast<float>(x) - imgCenterX;
	const float dy = static_cast<float>(y) - imgCenterY;

	// Normalized distance from center: 0.0 = dead center, 1.0 = edge
	float dist = sqrtf(dx * dx + dy * dy) / imgRadius;

	// When not dragging, ignore clicks clearly outside the wheel circle
	// When dragging, clamp to edge to allow smooth selection at maximum saturation
	if (!m_draggingWheel && dist > 1.05f)
		return;

	// Clamp to circle edge
	if (dist > 1.0f)
		dist = 1.0f;

	// Angle from center - red is at bottom of wheel
	// atan2 gives angle from positive X axis, we need angle from positive Y axis (bottom)
	float angle = atan2f(-dx, dy);  // Swapped and negated for bottom=0 (red)
	if (angle < 0.0f)
		angle += 2.0f * 3.14159265f;

	// Convert to degrees (0-360)
	m_hue        = angle * 180.0f / 3.14159265f;
	m_saturation = dist;
	m_colorValue = 1.0f;

	// Convert HSV to RGB
	uint8 r, g, b;
	hsvToRgb(m_hue, m_saturation, m_colorValue, r, g, b);

	// Set the direct color
	m_directColor.setArgb(255, r, g, b);
	m_useDirectColor = true;

	// Update all UI elements
	updateRgbTextboxes();
	updateWheelCursor();
	updateHtmlTextbox();

	if (m_pageSample)
		m_pageSample->SetBackgroundTint(UIColor(r, g, b));

	// Apply to target object immediately
	TangibleObject * const object = m_targetObject->getPointer();
	if (object)
		updateValueDirectColor(*object, m_directColor);

	// Update linked objects
	for (ObjectWatcherVector::iterator it = m_linkedObjects->begin(); it != m_linkedObjects->end(); ++it)
	{
		TangibleObject * const linked_object = *it;
		if (linked_object)
			updateValueDirectColor(*linked_object, m_directColor, PF_private);
	}

	m_changed = true;
	m_userChanged = true;
}

//======================================================================
