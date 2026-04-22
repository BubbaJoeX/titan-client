// ======================================================================
//
// FloatingPointUnit.cpp
// copyright 1999 Bootprint Entertainment
// copyright 2001 Sony Online Entertainment
//
// ======================================================================

#include "sharedFoundation/FirstSharedFoundation.h"
#include "sharedFoundation/FloatingPointUnit.h"

#include "sharedFoundation/ConfigSharedFoundation.h"

#include <float.h>
#include <xmmintrin.h>

// ======================================================================

int                          FloatingPointUnit::updateNumber;
ushort                       FloatingPointUnit::status;
FloatingPointUnit::Precision FloatingPointUnit::precision;
FloatingPointUnit::Rounding  FloatingPointUnit::rounding;
bool                         FloatingPointUnit::exceptionEnabled[E_max];
unsigned int                 FloatingPointUnit::mxcsrStatus;

// ======================================================================

const WORD PRECISION_MASK        = BINARY4(0000,0011,0000,0000);
const WORD PRECISION_24          = BINARY4(0000,0000,0000,0000);
const WORD PRECISION_53          = BINARY4(0000,0010,0000,0000);
const WORD PRECISION_64          = BINARY4(0000,0011,0000,0000);

const WORD ROUND_MASK            = BINARY4(0000,1100,0000,0000);
const WORD ROUND_NEAREST         = BINARY4(0000,0000,0000,0000);
const WORD ROUND_CHOP            = BINARY4(0000,1100,0000,0000);
const WORD ROUND_DOWN            = BINARY4(0000,0100,0000,0000);
const WORD ROUND_UP              = BINARY4(0000,1000,0000,0000);

const WORD EXCEPTION_PRECISION   = BINARY4(0000,0000,0010,0000);
const WORD EXCEPTION_UNDERFLOW   = BINARY4(0000,0000,0001,0000);
const WORD EXCEPTION_OVERFLOW    = BINARY4(0000,0000,0000,1000);
const WORD EXCEPTION_ZERO_DIVIDE = BINARY4(0000,0000,0000,0100);
const WORD EXCEPTION_DENORMAL    = BINARY4(0000,0000,0000,0010);
const WORD EXCEPTION_INVALID     = BINARY4(0000,0000,0000,0001);
const WORD EXCEPTION_ALL         = BINARY4(0000,0000,0011,1111);

#if defined(_WIN64)
// 16-bit x87 semantics in (WORD) status vs. MSVC's 32-bit _controlfp_s layout; never cast
// between them. A raw x87 value with _controlfp_s(…, 0xFFFF) was a startup hang in x64.
namespace
{
unsigned msvcFpuValueFromX87(WORD w)
{
	unsigned t = 0;
	const WORD p = w & PRECISION_MASK;
	if (p == PRECISION_64)
		t |= _PC_64;
	else if (p == PRECISION_53)
		t |= _PC_53;
	else
		t |= _PC_24;

	const WORD r = w & ROUND_MASK;
	if (r == ROUND_DOWN)
		t |= _RC_DOWN;
	else if (r == ROUND_UP)
		t |= _RC_UP;
	else if (r == ROUND_CHOP)
		t |= _RC_CHOP;
	else
		t |= _RC_NEAR;

	if (w & EXCEPTION_INVALID)
		t |= _EM_INVALID;
	if (w & EXCEPTION_DENORMAL)
		t |= _EM_DENORMAL;
	if (w & EXCEPTION_ZERO_DIVIDE)
		t |= _EM_ZERODIVIDE;
	if (w & EXCEPTION_OVERFLOW)
		t |= _EM_OVERFLOW;
	if (w & EXCEPTION_UNDERFLOW)
		t |= _EM_UNDERFLOW;
	if (w & EXCEPTION_PRECISION)
		t |= _EM_INEXACT;
	return t;
}

WORD x87ValueFromMsvcFpu(unsigned t)
{
	WORD w = 0;
	const unsigned pc = t & _MCW_PC;
	if (pc == _PC_64)
		w |= static_cast<WORD>(PRECISION_64);
	else if (pc == _PC_53)
		w |= static_cast<WORD>(PRECISION_53);
	else
		w |= static_cast<WORD>(PRECISION_24);

	const unsigned rc = t & _MCW_RC;
	if (rc == _RC_CHOP)
		w |= static_cast<WORD>(ROUND_CHOP);
	else if (rc == _RC_UP)
		w |= static_cast<WORD>(ROUND_UP);
	else if (rc == _RC_DOWN)
		w |= static_cast<WORD>(ROUND_DOWN);

	if (t & _EM_INVALID)
		w |= EXCEPTION_INVALID;
	if (t & _EM_DENORMAL)
		w |= EXCEPTION_DENORMAL;
	if (t & _EM_ZERODIVIDE)
		w |= EXCEPTION_ZERO_DIVIDE;
	if (t & _EM_OVERFLOW)
		w |= EXCEPTION_OVERFLOW;
	if (t & _EM_UNDERFLOW)
		w |= EXCEPTION_UNDERFLOW;
	if (t & _EM_INEXACT)
		w |= EXCEPTION_PRECISION;
	return w;
}
} // namespace
#endif

// MXCSR exception mask bits (bits 7-12 mask SSE exceptions when set to 1)
const unsigned int MXCSR_EXCEPTION_MASK = 0x1F80;  // Mask all SSE exceptions
const unsigned int MXCSR_DAZ            = 0x0040;  // Denormals Are Zero
const unsigned int MXCSR_FTZ            = 0x8000;  // Flush To Zero

// ======================================================================

void FloatingPointUnit::install(void)
{
	precision = P_24;
	rounding  = R_roundToNearestOrEven;
	memset(exceptionEnabled, 0, sizeof(exceptionEnabled));

	// preserve all other bits
	status  = getControlWord();
	status &= ~(PRECISION_MASK | ROUND_MASK | EXCEPTION_ALL);

	// set to single precision, rounding, and all exceptions masked
	status |= PRECISION_24 | ROUND_NEAREST | EXCEPTION_ALL;

	// check the config platform flags to see if we should enable some exceptions
	if (ConfigSharedFoundation::getFpuExceptionPrecision())
	{
		exceptionEnabled[E_precision] = true;
		status &= ~EXCEPTION_PRECISION;
	}

	if (ConfigSharedFoundation::getFpuExceptionUnderflow())
	{
		exceptionEnabled[E_underflow] = true;
		status &= ~EXCEPTION_UNDERFLOW;
	}

	if (ConfigSharedFoundation::getFpuExceptionOverflow())
	{
		exceptionEnabled[E_overflow] = true;
		status &= ~EXCEPTION_OVERFLOW;
	}

	if (ConfigSharedFoundation::getFpuExceptionZeroDivide())
	{
		exceptionEnabled[E_zeroDivide] = true;
		status &= ~EXCEPTION_ZERO_DIVIDE;
	}

	if (ConfigSharedFoundation::getFpuExceptionDenormal())
	{
		exceptionEnabled[E_denormal] = true;
		status &= ~EXCEPTION_DENORMAL;
	}

	if (ConfigSharedFoundation::getFpuExceptionInvalid())
	{
		exceptionEnabled[E_invalid] = true;
		status &= ~EXCEPTION_INVALID;
	}

	setControlWord(status);

	// Also configure MXCSR for SSE exceptions - mask all exceptions
	// and enable flush-to-zero and denormals-are-zero for performance
	mxcsrStatus = _mm_getcsr();
	mxcsrStatus |= MXCSR_EXCEPTION_MASK;  // Mask all SSE exceptions
	mxcsrStatus |= MXCSR_FTZ;             // Flush denormal results to zero
	mxcsrStatus |= MXCSR_DAZ;             // Treat denormal inputs as zero
	_mm_setcsr(mxcsrStatus);
}

// ----------------------------------------------------------------------

void FloatingPointUnit::update(void)
{
	WORD currentStatus = getControlWord();

	if (currentStatus != status)
	{
//		DEBUG_REPORT_LOG_PRINT(true, ("FPU: update=%d, in mode=%04x, should be in mode=%04x\n", updateNumber, static_cast<int>(currentStatus), static_cast<int>(status)));
		setControlWord(status);
	}

	// Also restore MXCSR if it was changed by external code
	unsigned int currentMxcsr = _mm_getcsr();
	if (currentMxcsr != mxcsrStatus)
	{
		_mm_setcsr(mxcsrStatus);
	}

	++updateNumber;
}

// ----------------------------------------------------------------------

WORD FloatingPointUnit::getControlWord(void)
{
#ifdef _WIN64
	unsigned t = 0;
	_controlfp_s(&t, 0, 0);
	return x87ValueFromMsvcFpu(t);
#else
	WORD controlWord = 0;
	__asm fnstcw controlWord;
	return controlWord;
#endif
}

// ----------------------------------------------------------------------

void FloatingPointUnit::setControlWord(WORD controlWord)
{
#ifdef _WIN64
	const unsigned t = msvcFpuValueFromX87(controlWord);
	unsigned         cur = 0;
	IGNORE_RETURN(_controlfp_s(&cur, t, _MCW_EM | _MCW_RC | _MCW_PC));
#else
	UNREF(controlWord);
	__asm fldcw controlWord;
#endif
}

// ----------------------------------------------------------------------

void FloatingPointUnit::setPrecision(Precision newPrecision)
{
	WORD bits = 0;

	switch (precision)
	{
		case P_24:
			bits = PRECISION_24;
			break;

		case P_53:
			bits = PRECISION_53;
			break;

		case P_64:
			bits = PRECISION_64;
			break;

		case P_max:
		default:
			DEBUG_FATAL(true, ("bad case"));
	}

	// record the current state
	precision = newPrecision;

	// set the proper bit pattern
	status &= ~PRECISION_MASK;
	status |= bits;

	// slam it into the FPU
	setControlWord(status);
}

// ----------------------------------------------------------------------

void FloatingPointUnit::setRounding(Rounding newRounding)
{
	WORD bits = 0;

	switch (newRounding)
	{
		case R_roundToNearestOrEven:
			bits = ROUND_NEAREST;
			break;

		case R_chop:
			bits = ROUND_CHOP;
			break;

		case R_roundDown:
			bits = ROUND_DOWN;
			break;

		case R_roundUp:
			bits = ROUND_UP;
			break;

		case R_max:
		default:
			DEBUG_FATAL(true, ("bad case"));
	}

	// record the current state
	rounding = newRounding;

	// set the proper bit pattern
	status &= ~ROUND_MASK;
	status |= bits;

	// slam it into the FPU
	setControlWord(status);
}

// ----------------------------------------------------------------------

void FloatingPointUnit::setExceptionEnabled(Exception exception, bool enabled)
{
	WORD bits = 0;

	switch (exception)
	{
		case E_precision:
			bits = EXCEPTION_PRECISION;
			break;

		case E_underflow:
			bits = EXCEPTION_UNDERFLOW;
			break;

		case E_overflow:
			bits = EXCEPTION_OVERFLOW;
			break;

		case E_zeroDivide:
			bits = EXCEPTION_ZERO_DIVIDE;
			break;

		case E_denormal:
			bits = EXCEPTION_DENORMAL;
			break;

		case E_invalid:
			bits = EXCEPTION_INVALID;
			break;

		case E_max:
		default:
			DEBUG_FATAL(true, ("bad case"));
	}

	// record the current state
	exceptionEnabled[exception] = enabled;

	// twiddle the bit appropriately.  these bits masks, so set the bit to disable the exception, clear the bit to enable it.
	if (enabled)
		status &= ~bits;
	else
		status |= bits;

	// slam it into the FPU
	setControlWord(status);
}

// ======================================================================
