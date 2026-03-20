// ======================================================================
//
//  Globals File
//  All header includes / first header
//
// ======================================================================

#ifndef INCLUDED_FoundationTypes_H
#define INCLUDED_FoundationTypes_H

#include <cfloat>
#include <iostream>
#include <windows.h>

#include "../third/backward.hpp"
#include "../third/loguru.hpp"

// ======================================================================

typedef unsigned char          byte;
typedef unsigned int           uint;
typedef unsigned long          ulong;
typedef unsigned short         ushort;

typedef unsigned char          uint8;
typedef unsigned short         uint16;
typedef unsigned long          uint32;
typedef unsigned __int64       uint64;
typedef signed char            int8;
typedef signed short           int16;
typedef signed long            int32;
typedef signed __int64         int64;
typedef int                    FILE_HANDLE;

// No-operation
#define NOP              (static_cast<void>(0))   //lint !e1923 //could become const variable

// To prevent "Unreference" compiler warning
#define UNREF(a)         (static_cast<void>(a))

// To prevent "Ignoring return value" lint warnings
#define IGNORE_RETURN(a) (static_cast<void>(a))

// int version of the sizeof operator
#define isizeof(a) static_cast<int>(sizeof(a))

// ======================================================================

// overrides of original swg fatal/warnings to Maya Console

#define NON_NULL(a) (a)
#define NOT_NULL(a) UNREF(a)
#define IS_NULL(a)  UNREF(a)

#define DEBUG_FATAL(a, b)   NOP

// allows us to capture origination point of warning/error
#define LINEINFO(a) fprintf(stderr, "\n%s in %s() file %s:%d \n", a, __FUNCTION__, __FILE__ , __LINE__)

static void formatMessage(char *buffer, size_t bufferLength, int stackDepth, const char *type, const char *format, va_list va)
{

    // make sure the buffer is always nullptr terminated
    buffer[--bufferLength] = '\0';

    _snprintf(buffer, bufferLength, "    (%08x): ", static_cast<int>(0x00));
    const size_t length = strlen(buffer);
    buffer += length;
    bufferLength -= length;

    // add the user message
    vsnprintf(buffer, bufferLength, format, va);
    std::cerr << buffer << std::endl;
    const size_t ln = strlen(buffer);
    buffer += ln;
    bufferLength -= ln;

    // add the newline
    if (bufferLength)
    {
        *buffer = '\n';
        ++buffer;
        --bufferLength;
        *buffer = '\0';
    }

    if (bufferLength == 0)
    {
        buffer[-2] = '+';
        buffer[-1] = '\n';
    }

}

static void InternalMessage(const char *format, int extraFlags, va_list va, int stackDepth = 0)
{
    char buffer[4 * 1024];
    formatMessage(buffer, sizeof(buffer), stackDepth, "WARNING", format, va);
}


// warning -> an issue that isn't bad, so we'll just write about it to the output window
#define WARNING(a, b) ((a) ? LINEINFO("[SWG MAYA PLUG-IN] ***WARNING***"), Warning b : NOP)
static void Warning(const char *format, ...)
{
    std::cerr << format << std::endl;
    LOG_F(ERROR, "%s", format);
}

// error -> a bug that is bad and will halt the current progress then show a popup
// code behind FATAL is responsible for actually cancelling the process!!
#define FATAL(a, b) ((a) ? LINEINFO("[SWG MAYA PLUG-IN] ***ERROR***"), Fatal b : NOP) // we'll let old SWG FATALs work this way too
static void Fatal(const char *format, ...)
{
    va_list va;
    va_start(va, format);
    InternalMessage(format, 0, va);
    va_end(va);
    LOG_F(ERROR, "%s", format);

    backward::StackTrace st;
    st.load_here(12);
    backward::Printer p;
    p.print(st);
    MessageBoxA( // trigger an actual popup window because this is a bad thing we need to tell people about
            nullptr,
            "ERROR: SWG Maya Plug-In encountered an error. Check Output Window for more information.",
            "SWG Maya Plug-In",
            MB_ICONERROR | MB_OK
    );
}

// ======================================================================
/**
 * Fatal if the specified var does not satisfy (low <= var < high).
 *
 * The parameter types must be integral and must be convertable to
 * an int.
 */

template <class T>
inline void ValidateRangeInclusiveExclusive(const T &low, const T &var, const T &high, const char *varName)
{
    FATAL(low > high, ("range check low [%d] > high [%d]", static_cast<int>(low), static_cast<int>(high)));
    FATAL( (var < low) || (var >= high), ("%s [%d] out of valid range [%d..%d)", varName, static_cast<int>(var), static_cast<int>(low), static_cast<int>(high)));
}

#ifdef _DEBUG
#define VALIDATE_RANGE_INCLUSIVE_EXCLUSIVE(low, var, high)    ValidateRangeInclusiveExclusive(low, var, high, #var)
#else
#define VALIDATE_RANGE_INCLUSIVE_EXCLUSIVE(low, var, high)    NOP
#endif

// ======================================================================
/**
 * Fatal if the specified var does not satisfy (low <= var <= high).
 *
 * The parameter types must be integral and must be convertable to
 * an int.
 */

template <class T>
inline void ValidateRangeInclusiveInclusive(const T &low, const T &var, const T &high, const char *varName)
{
    FATAL(low > high, ("range check low [%d] > high [%d]", static_cast<int>(low), static_cast<int>(high)));
    FATAL( (var < low) || (var > high), ("%s [%d] out of valid range [%d..%d]", varName, static_cast<int>(var), static_cast<int>(low), static_cast<int>(high)));
}

#ifdef _DEBUG
#define VALIDATE_RANGE_INCLUSIVE_INCLUSIVE(low, var, high)  ValidateRangeInclusiveInclusive(low, var, high, #var)
#else
#define VALIDATE_RANGE_INCLUSIVE_INCLUSIVE(low, var, high)  NOP
#endif

// ======================================================================

// deprecated stuff that should really go away, but hasn't!

#define CONST_REAL(a)  static_cast<float>(a)
#define RECIP(a)       (1.0f / (a))

/// Matches legacy SWG `real` (float) for ported exporter/writer code.
typedef float real;

const float REAL_MIN = FLT_MIN;
const float REAL_MAX = FLT_MAX;

// ======================================================================

// from FirstPlatform bc why not

#define DLLEXPORT __declspec(dllexport)

// ======================================================================

#endif

