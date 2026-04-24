// ======================================================================
//
// TemporaryCrcString.cpp
// Copyright 2002, Sony Online Entertainment Inc.
// All Rights Reserved.
//
// ======================================================================

#include "sharedFoundation/FirstSharedFoundation.h"
#include "sharedFoundation/TemporaryCrcString.h"

#include "sharedFoundation/Fatal.h"
#include "sharedFoundation/Os.h"
#include "sharedFoundation/Crc.h"

#include <cstring>

// ======================================================================

TemporaryCrcString::TemporaryCrcString()
: CrcString()
{
	// this is to avoid including Os in the header file
	DEBUG_FATAL(static_cast<int>(BUFFER_SIZE) != static_cast<int>(Os::MAX_PATH_LENGTH), ("Os::MAX_PATH_LENGTH and BUFFER_SIZE differ"));

	m_buffer[0] = '\0';
}

// ----------------------------------------------------------------------
/**
 * Copy constructor.
 *
 * TRF needed this to allow useful sanity checking with std::set<TemporaryCrcString>.
 */
TemporaryCrcString::TemporaryCrcString(const TemporaryCrcString &rhs)
: CrcString()
{
	set(rhs.getString(), rhs.getCrc());
}

// ----------------------------------------------------------------------

TemporaryCrcString::TemporaryCrcString(char const * string, bool applyNormalize)
: CrcString()
{
	// this is to avoid including Os in the header file
	DEBUG_FATAL(static_cast<int>(BUFFER_SIZE) != static_cast<int>(Os::MAX_PATH_LENGTH), ("Os::MAX_PATH_LENGTH and BUFFER_SIZE differ"));

	set(string, applyNormalize);
}

// ----------------------------------------------------------------------

TemporaryCrcString::TemporaryCrcString(char const * string, uint32 crc)
: CrcString()
{
	// this is to avoid including Os in the header file
	DEBUG_FATAL(static_cast<int>(BUFFER_SIZE) != static_cast<int>(Os::MAX_PATH_LENGTH), ("Os::MAX_PATH_LENGTH and BUFFER_SIZE differ"));

	set(string, crc);
}

// ----------------------------------------------------------------------

TemporaryCrcString::~TemporaryCrcString()
{
}

// ----------------------------------------------------------------------

char const * TemporaryCrcString::getString() const
{
	return m_buffer;
}

// ----------------------------------------------------------------------

void TemporaryCrcString::clear()
{
	m_buffer[0] = '\0';
	m_crc = Crc::crcNull;
}

// ----------------------------------------------------------------------

void TemporaryCrcString::internalSet(char const * string, bool applyNormalize)
{
	size_t const stringLengthWithNull = std::strlen (string) + static_cast<size_t> (1);
	// Release used to strip DEBUG_FATAL here; strcpy then overflowed m_buffer (512) and corrupted stack/heap
	// (e.g. AppearanceManager::install with a long appearance_table path).
	FATAL (
		stringLengthWithNull > static_cast<size_t> (BUFFER_SIZE),
		("TemporaryCrcString: string too long %zu/%d bytes (including NUL). Shorten path or raise TemporaryCrcString::BUFFER_SIZE.",
		 stringLengthWithNull, BUFFER_SIZE));
	if (applyNormalize)
		normalize(m_buffer, string);
	else
		strcpy(m_buffer, string);
}

// ----------------------------------------------------------------------

void TemporaryCrcString::set(char const * string, bool applyNormalize)
{
	// Release: NOT_NULL is a no-op; strlen(nullptr) in internalSet -> 0xC0000005.
	FATAL (!string, ("TemporaryCrcString::set: null string"));
	internalSet(string, applyNormalize);
	calculateCrc();
}

// ----------------------------------------------------------------------

void TemporaryCrcString::set(char const * string, uint32 crc)
{
	FATAL (!string, ("TemporaryCrcString::set(crc): null string"));
	internalSet(string, false);
	m_crc = crc;
}

// ======================================================================
