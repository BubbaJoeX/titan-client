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
#include "UIText.h"
#include "UITextbox.h"
#include "UIUtils.h"
#include "UIVolumePage.h"
#include "UnicodeUtils.h"
#include "clientGame/TangibleObject.h"
#include "clientUserInterface/CuiUtils.h"
#include "sharedFoundation/CrcLowerString.h"
#include "sharedGame/CustomizationManager.h"
#include "sharedMath/PackedArgb.h"
#include "sharedMath/PaletteArgb.h"
#include "sharedObject/CachedNetworkId.h"
#include "sharedObject/NetworkIdManager.h"
#include "sharedObject/CustomizationData.h"
#include "sharedObject/PaletteColorCustomizationVariable.h"

#include <list>
#include <map>
#include <math.h>
#include <stdio.h>
#include <string.h>

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
		if (!cv && !partialName.empty())
		{
			if (partialName[0] == '/')
				cv = cdata.findVariable(partialName.substr(1));
			else
				cv = cdata.findVariable("/" + partialName);
		}

		bool const hasNamespace =
			partialName.compare(0, PathPrefixes::shared_owner.size(), PathPrefixes::shared_owner) == 0 ||
			partialName.compare(0, PathPrefixes::priv.size(), PathPrefixes::priv) == 0 ||
			partialName.compare(0, PathPrefixes::shared_owner.size() - 1, PathPrefixes::shared_owner.substr(1)) == 0 ||
			partialName.compare(0, PathPrefixes::priv.size() - 1, PathPrefixes::priv.substr(1)) == 0;

		if (!cv && !hasNamespace)
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

	float const cs_pi = 3.14159265358979323846f;

	void rgbToHsv(uint8 r, uint8 g, uint8 b, float &h, float &s, float &v)
	{
		float const rf = static_cast<float>(r) / 255.0f;
		float const gf = static_cast<float>(g) / 255.0f;
		float const bf = static_cast<float>(b) / 255.0f;
		float const maximum = std::max(rf, std::max(gf, bf));
		float const minimum = std::min(rf, std::min(gf, bf));
		float const delta = maximum - minimum;

		v = maximum;
		s = maximum > 0.0f ? delta / maximum : 0.0f;
		if (delta <= 0.00001f)
			h = 0.0f;
		else
		{
			if (maximum == rf)
				h = (gf - bf) / delta;
			else if (maximum == gf)
				h = 2.0f + (bf - rf) / delta;
			else
				h = 4.0f + (rf - gf) / delta;
			h *= 60.0f;
			if (h < 0.0f)
				h += 360.0f;
		}
	}

	PackedArgb hsvToColor(float h, float s)
	{
		float const sector = h / 60.0f;
		int const i = static_cast<int>(sector) % 6;
		float const f = sector - static_cast<float>(static_cast<int>(sector));
		float const p = 1.0f - s;
		float const q = 1.0f - s * f;
		float const t = 1.0f - s * (1.0f - f);
		float r = 1.0f;
		float g = 1.0f;
		float b = 1.0f;

		switch (i)
		{
			case 0: r = 1.0f; g = t;    b = p;    break;
			case 1: r = q;    g = 1.0f; b = p;    break;
			case 2: r = p;    g = 1.0f; b = t;    break;
			case 3: r = p;    g = q;    b = 1.0f; break;
			case 4: r = t;    g = p;    b = 1.0f; break;
			default:r = 1.0f; g = p;    b = q;    break;
		}

		return PackedArgb(255, static_cast<uint8>(r * 255.0f + 0.5f), static_cast<uint8>(g * 255.0f + 0.5f), static_cast<uint8>(b * 255.0f + 0.5f));
	}

	bool parseHtmlColor(std::string const &text, PackedArgb &color)
	{
		char const *value = text.c_str();
		if (*value == '#')
			++value;
		if (strlen(value) != 6)
			return false;

		unsigned int rgb = 0;
		char trailing = 0;
		if (sscanf(value, "%6x%c", &rgb, &trailing) != 1)
			return false;

		color = PackedArgb(255, static_cast<uint8>((rgb >> 16) & 0xff), static_cast<uint8>((rgb >> 8) & 0xff), static_cast<uint8>(rgb & 0xff));
		return true;
	}

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
m_pageWheel(0),
m_cursorWheel(0),
m_textboxHtml(0),
m_textR(0),
m_textG(0),
m_textB(0),
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
m_userChanged(false),
m_draggingWheel(false),
m_updatingColorControls(false),
m_hasValidTarget(false),
m_lastSize(),
m_paletteSource(PS_target)
{
	getCodeDataObject(TUIVolumePage, m_volumePage, "volumePage");
	getCodeDataObject(TUIButton, m_buttonCancel, "buttonCancel", true);
	getCodeDataObject(TUIButton, m_buttonRevert, "buttonRevert", true);
	getCodeDataObject(TUIPage, m_pageSample, "pageSample", true);
	getCodeDataObject(TUIWidget, m_sampleElement, "sampleElement", true);
	getCodeDataObject(TUIWidget, m_text, "text", true);
	getCodeDataObject(TUIPage, m_pageWheel, "pageWheel", true);
	getCodeDataObject(TUIWidget, m_cursorWheel, "cursorWheel", true);
	getCodeDataObject(TUITextbox, m_textboxHtml, "textboxHtml", true);
	getCodeDataObject(TUITextbox, m_textR, "textR", true);
	getCodeDataObject(TUITextbox, m_textG, "textG", true);
	getCodeDataObject(TUITextbox, m_textB, "textB", true);

	if (m_pageWheel)
	{
		m_pageWheel->SetGetsInput(true);
		m_pageWheel->SetAbsorbsInput(true);
		if (m_cursorWheel)
			IGNORE_RETURN(m_pageWheel->MoveChild(m_cursorWheel, UIBaseObject::Top));
	}

	if(getButtonClose())
		m_buttonClose = getButtonClose();
	
	m_volumePage->Clear();

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
	m_pageWheel = 0;
	m_cursorWheel = 0;
	m_textboxHtml = 0;
	m_textR = 0;
	m_textG = 0;
	m_textB = 0;
	m_text = 0;

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
	if (m_pageWheel)
		m_pageWheel->AddCallback(this);
	if (m_textboxHtml)
		m_textboxHtml->AddCallback(this);
	if (m_textR)
		m_textR->AddCallback(this);
	if (m_textG)
		m_textG->AddCallback(this);
	if (m_textB)
		m_textB->AddCallback(this);
	setIsUpdating(true);
}

//----------------------------------------------------------------------

void CuiColorPicker::performDeactivate()
{
	setIsUpdating(false);

	if (m_buttonCancel)
		m_buttonCancel->RemoveCallback(this);
	if (m_buttonRevert)
		m_buttonRevert->RemoveCallback(this);

	m_volumePage->RemoveCallback(this);
	if (m_pageWheel)
		m_pageWheel->RemoveCallback(this);
	if (m_textboxHtml)
		m_textboxHtml->RemoveCallback(this);
	if (m_textR)
		m_textR->RemoveCallback(this);
	if (m_textG)
		m_textG->RemoveCallback(this);
	if (m_textB)
		m_textB->RemoveCallback(this);
	m_draggingWheel = false;

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
}

//----------------------------------------------------------------------

void CuiColorPicker::OnVolumePageSelectionChanged(UIWidget * context)
{
	if (context == m_volumePage)
	{
		const int index = m_volumePage->GetLastSelectedIndex();

		updateValue(index);

		if (checkAndResetChanged())
			m_userChanged = true;
	}
}

//----------------------------------------------------------------------

bool CuiColorPicker::OnMessage(UIWidget *context, UIMessage const &msg)
{
	if (context != m_pageWheel)
		return true;

	if (msg.Type == UIMessage::LeftMouseDown)
	{
		m_draggingWheel = true;
		updateWheelSelection(msg.MouseCoords.x, msg.MouseCoords.y);
		return false;
	}
	if (msg.Type == UIMessage::MouseMove && m_draggingWheel)
	{
		if (!msg.Modifiers.LeftMouseDown)
			m_draggingWheel = false;
		else
			updateWheelSelection(msg.MouseCoords.x, msg.MouseCoords.y);
		return false;
	}
	if (msg.Type == UIMessage::LeftMouseUp)
	{
		if (m_draggingWheel)
			updateWheelSelection(msg.MouseCoords.x, msg.MouseCoords.y);
		m_draggingWheel = false;
		return false;
	}

	return true;
}

//----------------------------------------------------------------------

void CuiColorPicker::OnTextboxChanged(UIWidget *context)
{
	if (!m_updatingColorControls)
		updateSelectionFromTextboxes(context);
}

//----------------------------------------------------------------------

void CuiColorPicker::updateSelectionFromTextboxes(UIWidget *context)
{
	if (!m_palette || !m_hasValidTarget)
		return;

	PackedArgb requestedColor(PackedArgb::solidWhite);
	if (context == m_textboxHtml && m_textboxHtml)
	{
		Unicode::String text;
		m_textboxHtml->GetLocalText(text);
		if (!parseHtmlColor(Unicode::wideToNarrow(text), requestedColor))
			return;
	}
	else if ((context == m_textR || context == m_textG || context == m_textB) && m_textR && m_textG && m_textB)
	{
		Unicode::String redText;
		Unicode::String greenText;
		Unicode::String blueText;
		m_textR->GetLocalText(redText);
		m_textG->GetLocalText(greenText);
		m_textB->GetLocalText(blueText);
		int const red = atoi(Unicode::wideToNarrow(redText).c_str());
		int const green = atoi(Unicode::wideToNarrow(greenText).c_str());
		int const blue = atoi(Unicode::wideToNarrow(blueText).c_str());
		if (red < 0 || red > 255 || green < 0 || green > 255 || blue < 0 || blue > 255)
			return;
		requestedColor = PackedArgb(255, static_cast<uint8>(red), static_cast<uint8>(green), static_cast<uint8>(blue));
	}
	else
		return;

	int bestIndex = -1;
	unsigned long bestDistance = 0xffffffffUL;
	for (int i = m_rangeMin; i < m_rangeMax; ++i)
	{
		bool error = false;
		PackedArgb const &candidate = m_palette->getEntry(i, error);
		if (error)
			continue;
		int const dr = static_cast<int>(candidate.getR()) - static_cast<int>(requestedColor.getR());
		int const dg = static_cast<int>(candidate.getG()) - static_cast<int>(requestedColor.getG());
		int const db = static_cast<int>(candidate.getB()) - static_cast<int>(requestedColor.getB());
		unsigned long const distance = static_cast<unsigned long>(dr * dr + dg * dg + db * db);
		if (bestIndex < 0 || distance < bestDistance)
		{
			bestIndex = i;
			bestDistance = distance;
		}
	}

	if (bestIndex >= 0)
	{
		m_volumePage->SetSelectionIndex(bestIndex);
		updateValue(bestIndex);
		m_userChanged = true;
	}
	else
		WARNING(true, ("CuiColorPicker: palette [%s] has no usable entries in range [%d,%d).", m_palette->getName().getString(), m_rangeMin, m_rangeMax));
}

//----------------------------------------------------------------------

void CuiColorPicker::updateWheelSelection(long x, long y)
{
	if (!m_pageWheel || !m_palette || !m_hasValidTarget)
		return;

	UISize const size = m_pageWheel->GetSize();
	float const centerX = static_cast<float>(size.x) * 0.5f;
	float const centerY = static_cast<float>(size.y) * 0.5f;
	float const radius = std::max(1.0f, static_cast<float>(std::min(size.x, size.y)) * 0.5f - 3.0f);
	float const dx = static_cast<float>(x) - centerX;
	float const dy = centerY - static_cast<float>(y);
	float angle = atan2f(dy, dx);
	if (angle < 0.0f)
		angle += 2.0f * cs_pi;
	float const saturation = std::min(1.0f, sqrtf(dx * dx + dy * dy) / radius);
	PackedArgb const requestedColor = hsvToColor(angle * 180.0f / cs_pi, saturation);

	int bestIndex = -1;
	unsigned long bestDistance = 0xffffffffUL;
	for (int i = m_rangeMin; i < m_rangeMax; ++i)
	{
		bool error = false;
		PackedArgb const &candidate = m_palette->getEntry(i, error);
		if (error)
			continue;
		int const dr = static_cast<int>(candidate.getR()) - static_cast<int>(requestedColor.getR());
		int const dg = static_cast<int>(candidate.getG()) - static_cast<int>(requestedColor.getG());
		int const db = static_cast<int>(candidate.getB()) - static_cast<int>(requestedColor.getB());
		unsigned long const distance = static_cast<unsigned long>(dr * dr + dg * dg + db * db);
		if (bestIndex < 0 || distance < bestDistance)
		{
			bestIndex = i;
			bestDistance = distance;
		}
	}

	if (bestIndex >= 0)
	{
		m_volumePage->SetSelectionIndex(bestIndex);
		updateValue(bestIndex);
		m_userChanged = true;

		if (m_cursorWheel)
		{
			float const cursorDistance = saturation * radius;
			UISize const cursorSize = m_cursorWheel->GetSize();
			long const cursorX = static_cast<long>(centerX + cosf(angle) * cursorDistance) - cursorSize.x / 2;
			long const cursorY = static_cast<long>(centerY - sinf(angle) * cursorDistance) - cursorSize.y / 2;
			m_cursorWheel->SetLocation(cursorX, cursorY);
		}
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
				if (var->setValue(index) && var->getValue() == index)
					m_changed = true;
				else
					WARNING(true, ("CuiColorPicker: rejected value [%d] for variable [%s].", index, m_targetVariable.c_str()));
			}
		}
		else
			WARNING(true, ("CuiColorPicker: variable [%s] disappeared while applying preview.", m_targetVariable.c_str()));
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
	if (index >= m_rangeMin && index < m_rangeMax)
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
		updateWheelFromIndex(index);

	}
	else
		WARNING(true, ("CuiColorPicker: rejected index [%d], allowed range is [%d,%d).", index, m_rangeMin, m_rangeMax));
}

//----------------------------------------------------------------------

void CuiColorPicker::updateWheelFromIndex(int index)
{
	if (!m_palette || index < 0 || index >= m_palette->getEntryCount())
		return;

	bool error = false;
	PackedArgb const &color = m_palette->getEntry(index, error);
	if (error)
	{
		WARNING(true, ("CuiColorPicker: palette [%s] failed to return index [%d].", m_palette->getName().getString(), index));
		return;
	}

	m_updatingColorControls = true;
	char buffer[16];
	if (m_textR)
	{
		snprintf(buffer, sizeof(buffer), "%u", static_cast<unsigned int>(color.getR()));
		m_textR->SetLocalText(Unicode::narrowToWide(buffer));
	}
	if (m_textG)
	{
		snprintf(buffer, sizeof(buffer), "%u", static_cast<unsigned int>(color.getG()));
		m_textG->SetLocalText(Unicode::narrowToWide(buffer));
	}
	if (m_textB)
	{
		snprintf(buffer, sizeof(buffer), "%u", static_cast<unsigned int>(color.getB()));
		m_textB->SetLocalText(Unicode::narrowToWide(buffer));
	}
	if (m_textboxHtml)
	{
		snprintf(buffer, sizeof(buffer), "#%02X%02X%02X", color.getR(), color.getG(), color.getB());
		m_textboxHtml->SetLocalText(Unicode::narrowToWide(buffer));
	}
	m_updatingColorControls = false;

	if (m_pageSample)
		m_pageSample->SetBackgroundTint(CuiUtils::convertColor(color));

	if (m_pageWheel && m_cursorWheel && !m_draggingWheel)
	{
		float hue = 0.0f;
		float saturation = 0.0f;
		float value = 0.0f;
		rgbToHsv(color.getR(), color.getG(), color.getB(), hue, saturation, value);
		UISize const size = m_pageWheel->GetSize();
		float const centerX = static_cast<float>(size.x) * 0.5f;
		float const centerY = static_cast<float>(size.y) * 0.5f;
		float const radius = std::max(1.0f, static_cast<float>(std::min(size.x, size.y)) * 0.5f - 3.0f);
		float const angle = hue * cs_pi / 180.0f;
		UISize const cursorSize = m_cursorWheel->GetSize();
		m_cursorWheel->SetLocation(
			static_cast<long>(centerX + cosf(angle) * saturation * radius) - cursorSize.x / 2,
			static_cast<long>(centerY - sinf(angle) * saturation * radius) - cursorSize.y / 2);
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
	m_hasValidTarget = false;

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
					m_hasValidTarget = true;
				}
				else
					WARNING (true, ("color picker could not find variable '%s'", m_targetVariable.c_str ()));

				cdata->release ();
			}
		}
	}

	m_rangeMin = std::max (rangeMin, actualRangeMin);
	m_rangeMax = std::min (rangeMax, actualRangeMax);
	if (m_hasValidTarget && m_rangeMax <= m_rangeMin)
	{
		WARNING(true, ("CuiColorPicker::setTarget rejected empty range [%d,%d) for variable [%s].", m_rangeMin, m_rangeMax, m_targetVariable.c_str()));
		m_hasValidTarget = false;
	}

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

//======================================================================
