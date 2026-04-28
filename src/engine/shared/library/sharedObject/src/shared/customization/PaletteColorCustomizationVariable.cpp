// ======================================================================
//
// PaletteColorCustomizationVariable.cpp
// Copyright 2002 Sony Online Entertainment Inc.
// All Rights Reserved.
//
// ======================================================================

#include "sharedObject/FirstSharedObject.h"
#include "sharedObject/PaletteColorCustomizationVariable.h"

#include "sharedFoundation/ExitChain.h"
#include "sharedFoundation/MemoryBlockManager.h"
#include "sharedMath/PackedArgb.h"
#include "sharedMath/PaletteArgb.h"
#include "sharedObject/ObjectTemplateCustomizationDataWriter.h"

#include <string>
#include <cstdio>
#include <vector>

// ======================================================================

MEMORY_BLOCK_MANAGER_IMPLEMENTATION_WITH_INSTALL(PaletteColorCustomizationVariable, true, 0, 0, 0);

// ======================================================================

PaletteColorCustomizationVariable::PaletteColorCustomizationVariable(const PaletteArgb *palette, int selectedIndex)
:	RangedIntCustomizationVariable(),
	m_palette(palette),
	m_paletteIndex(selectedIndex),
	m_useDirectColor(false),
	m_directColor(PackedArgb::solidWhite)
{
	NOT_NULL(palette);
	palette->fetch();

	int const paletteEntryCount = m_palette->getEntryCount();

	if ((m_paletteIndex < 0) || (m_paletteIndex >= paletteEntryCount))
	{
		int const whiteIndex = m_palette->findClosestMatch(PackedArgb::solidWhite);
		DEBUG_WARNING(true, ("Initializing palette var for palette=[%s] with out-of-range value [%d], valid range is [0..%d], defaulting to white index [%d].", m_palette->getName().getString(), m_paletteIndex, paletteEntryCount - 1, whiteIndex));
		m_paletteIndex = whiteIndex;
	}
}

// ----------------------------------------------------------------------

PaletteColorCustomizationVariable::~PaletteColorCustomizationVariable()
{
	//lint -esym(1540, PaletteColorCustomizationVariable::m_palette) // not freed or zero'ed  // yes, but released
	m_palette->release();
}

// ----------------------------------------------------------------------

int PaletteColorCustomizationVariable::getValue() const
{
	// Always return the palette index (closest match when using direct color).
	// Direct color rendering is handled separately through the customization data
	// version 3 format and the shader system's direct color override path.
	return m_paletteIndex;
}

// ----------------------------------------------------------------------

bool PaletteColorCustomizationVariable::setValue(int value)
{
	if (m_palette && (value >= 0) && (value < m_palette->getEntryCount()))
	{
		m_paletteIndex = value;
		m_useDirectColor = false;
		signalVariableModified();
	}
	else if (!m_palette)
	{
		m_paletteIndex = value;
	}
	else
	{
		DEBUG_WARNING(true, ("attempted to set PaletteColorCustomizationVariable to %d, outside valid range [%d,%d).", value, 0, m_palette->getEntryCount()));
		return false;
	}

	return RangedIntCustomizationVariable::setValue(m_paletteIndex);
}

// ----------------------------------------------------------------------

const PackedArgb &PaletteColorCustomizationVariable::getValueAsColor() const
{
	// If using direct color, return the actual direct color for rendering
	if (m_useDirectColor)
		return m_directColor;

	// Otherwise return the palette color
	if (!m_palette)
		return PackedArgb::solidMagenta;

	int const usePaletteIndex = clamp(0, m_paletteIndex, m_palette->getEntryCount() - 1);
	bool error = false;
	PackedArgb const & color = m_palette->getEntry(usePaletteIndex, error);

	WARNING(error, ("PaletteColorCustomizationVariable::getValueAsColor error"));
	return color;
}

// ----------------------------------------------------------------------

void PaletteColorCustomizationVariable::getRange(int &minRangeInclusive, int &maxRangeExclusive) const
{
	minRangeInclusive = 0;
	maxRangeExclusive = m_palette->getEntryCount();
}

// ----------------------------------------------------------------------

const PaletteArgb *PaletteColorCustomizationVariable::fetchPalette() const
{
	m_palette->fetch();
	return m_palette;
}

// ----------------------------------------------------------------------

void PaletteColorCustomizationVariable::setClosestColor(const PackedArgb &targetColor)
{
	setValue(m_palette->findClosestMatch(targetColor));
}

// ----------------------------------------------------------------------

void PaletteColorCustomizationVariable::writeObjectTemplateExportString(const std::string &variablePathName, ObjectTemplateCustomizationDataWriter &writer) const
{
#if !(USE_OBJ_TEMPLATE_CUSTOM_VAR_WRITER)
	UNREF(variablePathName);
	UNREF(writer);
#else
	//-- this assumes that the current value is the default value --- if the user changed values at all, they
	//   will not be the artist's built in default.
	writer.addPaletteColorCustomizationVariable(variablePathName, m_palette->getName().getString(), m_paletteIndex);
#endif
}

// ----------------------------------------------------------------------

int PaletteColorCustomizationVariable::getPersistedDataByteCount() const
{
	// Always save the matched palette index - direct color is stored in objvar
	int const value = m_paletteIndex;

	// Check if palette is available
	if (!m_palette)
		return 1;

	int minInclusive = 0;
	int maxExclusive = m_palette->getEntryCount();

	if ((minInclusive >= 0) && (maxExclusive <= 256))
	{
		// Value fits in an unsigned 8-bit byte.
		return 1;
	}
	else
	{
		// Value fits in a signed 16-bit int.
		return 2;
	}
}

// ----------------------------------------------------------------------

void PaletteColorCustomizationVariable::saveToByteVector(ByteVector &data) const
{
	// Always save the matched palette index - direct color is stored in objvar
	int const byteCount = getPersistedDataByteCount();
	switch (byteCount)
	{
		case 1:
		{
			byte const value = static_cast<byte>(m_paletteIndex);
			data.push_back(value);
			break;
		}

		case 2:
		{
			int16 const value = static_cast<int16>(m_paletteIndex);
			byte const lowerByte = static_cast<byte>(static_cast<uint16>(value) & 0x00ff);
			data.push_back(lowerByte);
			byte const upperByte = static_cast<byte>((static_cast<uint16>(value) >> 8) & 0x00ff);
			data.push_back(upperByte);
			break;
		}

		default:
			break;
	}
}

// ----------------------------------------------------------------------

bool PaletteColorCustomizationVariable::restoreFromByteVector(ByteVector const &data, int startIndex, int length)
{
	if (length < 1 || startIndex < 0 || static_cast<int>(data.size()) < startIndex + length)
		return false;

	// Parse palette index based on length
	int value = 0;
	switch (length)
	{
		case 1:
		{
			value = static_cast<int>(data[static_cast<size_t>(startIndex)]);
			break;
		}

		case 2:
		{
			byte const lowerByte = data[static_cast<size_t>(startIndex)];
			byte const upperByte = data[static_cast<size_t>(startIndex + 1)];
			value = static_cast<int>(static_cast<int16>(static_cast<uint16>(lowerByte) | (static_cast<uint16>(upperByte) << 8)));
			break;
		}

		default:
			return false;
	}

	// Restore palette index (direct color is restored separately from objvar)
	m_paletteIndex = value;
	m_useDirectColor = false;

	return true;
}

// ----------------------------------------------------------------------

#ifdef _DEBUG

std::string PaletteColorCustomizationVariable::debugToString() const
{
	char buffer[1024];

	if (m_useDirectColor)
	{
		char htmlColor[16];
		m_directColor.toHtmlString(htmlColor, sizeof(htmlColor), false);
		sprintf(buffer, "palette (%s): direct color %s -> matched index %d", m_palette->getName().getString(), htmlColor, m_paletteIndex);
	}
	else
	{
		sprintf(buffer, "palette (%s): index %d", m_palette->getName().getString(), m_paletteIndex);
	}
	return std::string(buffer);
}

#endif

// ----------------------------------------------------------------------
/**
 * Set a direct RGB color, bypassing the palette.
 * The color will be auto-matched to the closest palette entry for rendering.
 * @param color  The direct color to set
 * @return  true if successful
 */
bool PaletteColorCustomizationVariable::setDirectColor(const PackedArgb &color)
{
	m_directColor = color;
	m_useDirectColor = true;
	if (m_palette)
		m_paletteIndex = m_palette->findClosestMatch(color);
	else
		m_paletteIndex = 0;
	signalVariableModified();
	return true;
}

// ----------------------------------------------------------------------
/**
 * Set a direct color from an HTML color string.
 * @param htmlColor  The HTML color string (e.g., "#FF5500")
 * @return  true if the string was valid and color was set
 */
bool PaletteColorCustomizationVariable::setDirectColorHtml(const char *htmlColor)
{
	if (!PackedArgb::isValidHtmlColor(htmlColor))
		return false;

	PackedArgb color = PackedArgb::fromHtmlString(htmlColor);
	return setDirectColor(color);
}

// ----------------------------------------------------------------------
/**
 * Check if this variable is using a direct color vs palette index.
 * @return  true if using direct color
 */
bool PaletteColorCustomizationVariable::hasDirectColor() const
{
	return m_useDirectColor;
}

// ----------------------------------------------------------------------
/**
 * Get the direct color value (the actual selected color, not the matched palette color).
 * @return  The direct color, or the palette color if not using direct color
 */
const PackedArgb &PaletteColorCustomizationVariable::getDirectColor() const
{
	if (m_useDirectColor)
		return m_directColor;

	// Return the palette color
	return getValueAsColor();
}

// ----------------------------------------------------------------------
/**
 * Get the palette index that was matched to the direct color.
 * @return  The matched palette index
 */
int PaletteColorCustomizationVariable::getMatchedPaletteIndex() const
{
	return m_paletteIndex;
}

// ----------------------------------------------------------------------
/**
 * Check if an index is a special color-encoded index.
 * @param index  The index to check
 * @return  true if this is a special color index (negative)
 */
bool PaletteColorCustomizationVariable::isSpecialColorIndex(int index)
{
	return PackedArgb::isSpecialColorIndex(index);
}

// ----------------------------------------------------------------------
/**
 * Encode a color as a special negative palette index.
 * @param color  The color to encode
 * @return  The special encoded index
 */
int PaletteColorCustomizationVariable::encodeColorAsIndex(const PackedArgb &color)
{
	return PackedArgb::encodeAsSpecialIndex(color);
}

// ----------------------------------------------------------------------
/**
 * Decode a color from a special encoded index.
 * @param specialIndex  The special index to decode
 * @return  The decoded color
 */
PackedArgb PaletteColorCustomizationVariable::decodeColorFromIndex(int specialIndex)
{
	return PackedArgb::decodeFromSpecialIndex(specialIndex);
}

// ======================================================================
