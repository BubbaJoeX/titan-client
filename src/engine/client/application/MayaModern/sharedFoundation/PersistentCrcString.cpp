// ======================================================================
//
// PersistentCrcString.cpp
// Copyright 2002, Sony Online Entertainment Inc.
// All Rights Reserved.
//
// ======================================================================

#include "PersistentCrcString.h"
#include "Globals.h"

#include "Crc.h"
#include "Os.h"

// ======================================================================

namespace PersistentCrcStringNamespace
{
    void remove();
}
using namespace PersistentCrcStringNamespace;

// ======================================================================

PersistentCrcString const PersistentCrcString::empty("", CC_true);

// ======================================================================

void PersistentCrcString::install()
{
}

// ----------------------------------------------------------------------

void PersistentCrcStringNamespace::remove()
{
}

// ======================================================================

PersistentCrcString::PersistentCrcString()
        : CrcString(),
          m_buffer(nullptr)
{
}

// ----------------------------------------------------------------------

PersistentCrcString::PersistentCrcString(CrcString const &rhs)
        : CrcString(),
          m_buffer(nullptr)
{
    if (!rhs.isEmpty())
        PersistentCrcString::set(rhs.getString(), rhs.getCrc());
}

// ----------------------------------------------------------------------

PersistentCrcString::PersistentCrcString(PersistentCrcString const &rhs)
        : CrcString(), //lint !e1738 // non copy constructor used to initialize base class // this appears to be intentional.
          m_buffer(nullptr)
{
    if (!rhs.isEmpty())
        PersistentCrcString::set(rhs.getString(), rhs.getCrc());
}

// ----------------------------------------------------------------------

PersistentCrcString::PersistentCrcString(char const * string, bool applyNormalize)
        : CrcString(),
          m_buffer(nullptr)
{
    if (string)
        PersistentCrcString::set(string, applyNormalize);
}

// ----------------------------------------------------------------------

PersistentCrcString::PersistentCrcString(char const * string, uint32 crc)
        : CrcString(),
          m_buffer(nullptr)
{
    NOT_NULL(string);
    PersistentCrcString::set(string, crc);
}

// ----------------------------------------------------------------------

PersistentCrcString::PersistentCrcString(char const * string, ConstChar)
        : CrcString(),
          m_buffer(const_cast<char *>(string))
{
    calculateCrc();
}

// ----------------------------------------------------------------------

PersistentCrcString::~PersistentCrcString()
{
    internalFree();
}

// ----------------------------------------------------------------------

char const * PersistentCrcString::getString() const
{
    return m_buffer ? m_buffer : "";
}

// ----------------------------------------------------------------------

void PersistentCrcString::internalFree()
{
    if (m_buffer)
    {
        delete [] m_buffer;
        m_buffer = nullptr;
    }
}

// ----------------------------------------------------------------------

void PersistentCrcString::clear()
{
    internalFree();
    m_crc = Crc::crcNull;
}

// ----------------------------------------------------------------------

void PersistentCrcString::internalSet(char const * string, bool applyNormalize)
{
    internalFree();

    const int stringLength = static_cast<int>(strlen(string) + 1);
    DEBUG_FATAL(stringLength > Os::MAX_PATH_LENGTH, ("string too long for a filename"));
    m_buffer = new char[static_cast<size_t>(stringLength)];

    if (applyNormalize)
        normalize(m_buffer, string);
    else
        strcpy_s(m_buffer, stringLength, string);
}

// ----------------------------------------------------------------------

void PersistentCrcString::set(char const * string, bool applyNormalize)
{
    NOT_NULL(string);
    internalSet(string, applyNormalize);
    calculateCrc();
}

// ----------------------------------------------------------------------

void PersistentCrcString::set(char const * string, uint32 crc)
{
    NOT_NULL(string);
    internalSet(string, false);
    m_crc = crc;
}

//----------------------------------------------------------------------

PersistentCrcString const & PersistentCrcString::operator=(PersistentCrcString const & rhs)
{
    if (this != &rhs)
        CrcString::set(rhs);

    return *this;
} //lint !e1539 //m_buffer not assigned

// ======================================================================
