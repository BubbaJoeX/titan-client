// ======================================================================
//
// GodClientPen.cpp
// Pen tablet support for the God Client (Huion Kamvas, Wacom, any Windows Ink pen).
//
// ======================================================================

#include "SwgGodClient/FirstSwgGodClient.h"
#include "GodClientPen.h"

#include <qsettings.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

// WindowsWrapper.h builds windows.h with NOATOM; the Ink property opt-out wants a global atom.
extern "C" __declspec(dllimport) ATOM WINAPI GlobalAddAtomA(LPCSTR lpString);
extern "C" __declspec(dllimport) ATOM WINAPI GlobalDeleteAtom(ATOM nAtom);

// ======================================================================

namespace GodClientPenNamespace
{
	// Windows 8 pointer API.  Qt 3 pins _WIN32_WINNT to 0x0500, so none of this is in the
	// headers the god client sees; the layouts below mirror POINTER_INFO / POINTER_PEN_INFO.

	UINT const kWmPointerUpdate         = 0x0245;
	UINT const kWmPointerDown           = 0x0246;
	UINT const kWmPointerUp             = 0x0247;
	UINT const kWmPointerEnter          = 0x0249;
	UINT const kWmPointerLeave          = 0x024A;
	UINT const kWmPointerCaptureChanged = 0x024C;

	DWORD const kPointerTypePen = 3; // PT_PEN

	UINT32 const kPointerFlagInRange   = 0x00000002;
	UINT32 const kPointerFlagInContact = 0x00000004;

	UINT32 const kPenFlagBarrel   = 0x00000001;
	UINT32 const kPenFlagInverted = 0x00000002;
	UINT32 const kPenFlagEraser   = 0x00000004;

	UINT32 const kPenMaskPressure = 0x00000001;
	UINT32 const kPenMaskRotation = 0x00000002;
	UINT32 const kPenMaskTiltX    = 0x00000004;
	UINT32 const kPenMaskTiltY    = 0x00000008;

	float const kMaximumPressure = 1024.0f;

	struct PointerInfo
	{
		DWORD  pointerType;
		UINT32 pointerId;
		UINT32 frameId;
		UINT32 pointerFlags;
		HANDLE sourceDevice;
		HWND   hwndTarget;
		POINT  ptPixelLocation;
		POINT  ptHimetricLocation;
		POINT  ptPixelLocationRaw;
		POINT  ptHimetricLocationRaw;
		DWORD  dwTime;
		UINT32 historyCount;
		INT32  inputData;
		DWORD  dwKeyStates;
		UINT64 performanceCount;
		int    buttonChangeType;
	};

	struct PointerPenInfo
	{
		PointerInfo pointerInfo;
		UINT32      penFlags;
		UINT32      penMask;
		UINT32      pressure;
		UINT32      rotation;
		INT32       tiltX;
		INT32       tiltY;
	};

	typedef BOOL (WINAPI * GetPointerTypeFunction)(UINT32 pointerId, DWORD * pointerType);
	typedef BOOL (WINAPI * GetPointerPenInfoFunction)(UINT32 pointerId, PointerPenInfo * penInfo);
	typedef BOOL (WINAPI * SetWindowFeedbackSettingFunction)(HWND window, int feedback, DWORD flags, UINT32 size, void const * configuration);

	// Mouse messages promoted from pen input carry this signature in GetMessageExtraInfo;
	// bit 0x80 set means touch instead of pen.
	DWORD const kPenSignatureMask = 0xFFFFFF80;
	DWORD const kPenSignature     = 0xFF515700;

	// Per-window Ink gesture opt-out (tpcshrd.h).
	char const * const kTabletPenServiceProperty = "MicrosoftTabletPenServiceProperty";
	DWORD const kTabletDisablePressAndHold      = 0x00000001;
	DWORD const kTabletDisablePenTapFeedback    = 0x00000008;
	DWORD const kTabletDisablePenBarrelFeedback = 0x00000010;
	DWORD const kTabletDisableFlicks            = 0x00010000;
	DWORD const kTabletDisableFlickFallbackKeys = 0x00100000;

	// FEEDBACK_TYPE values for the pen visuals SetWindowFeedbackSetting can switch off.
	int const kPenFeedbackTypes[] = { 2, 3, 4, 5, 6 }; // barrel, tap, double tap, press and hold, right tap

	// A promoted mouse message without the signature still counts as pen input when a pen
	// packet arrived this recently (some drivers inject their own extra info).
	DWORD const kPenRecentMs = 100;

	char const kSettingsGroup[] = "Tablet";

	bool                             s_installed;
	GetPointerTypeFunction           s_getPointerType;
	GetPointerPenInfoFunction        s_getPointerPenInfo;
	SetWindowFeedbackSettingFunction s_setWindowFeedbackSetting;

	GodClientPen::State    s_state;
	GodClientPen::Settings s_settings;
	std::vector<HWND>      s_windows;

	// ----------------------------------------------------------------------

	void applyInkGestures(HWND const window)
	{
		if (!window || !IsWindow(window))
			return;

		bool const disable = s_settings.enabled && s_settings.disableInkGestures;

		if (disable)
		{
			DWORD const flags =
				kTabletDisablePressAndHold |
				kTabletDisablePenTapFeedback |
				kTabletDisablePenBarrelFeedback |
				kTabletDisableFlicks |
				kTabletDisableFlickFallbackKeys;

			ATOM const atom = GlobalAddAtomA(kTabletPenServiceProperty);
			IGNORE_RETURN(SetPropA(window, kTabletPenServiceProperty, reinterpret_cast<HANDLE>(static_cast<DWORD_PTR>(flags))));
			IGNORE_RETURN(GlobalDeleteAtom(atom));
		}
		else
			IGNORE_RETURN(RemovePropA(window, kTabletPenServiceProperty));

		if (s_setWindowFeedbackSetting)
		{
			BOOL const off = FALSE;
			for (size_t i = 0; i < sizeof(kPenFeedbackTypes) / sizeof(kPenFeedbackTypes[0]); ++i)
			{
				if (disable)
					IGNORE_RETURN(s_setWindowFeedbackSetting(window, kPenFeedbackTypes[i], 0, sizeof(off), &off));
				else
					IGNORE_RETURN(s_setWindowFeedbackSetting(window, kPenFeedbackTypes[i], 0, 0, 0));
			}
		}
	}

	// ----------------------------------------------------------------------

	float clamp01(float const value)
	{
		return std::max(0.0f, std::min(1.0f, value));
	}

	// ----------------------------------------------------------------------

	void loadSettings()
	{
		GodClientPen::Settings const defaults(GodClientPen::getDefaultSettings());

		QSettings settings;
		settings.insertSearchPath(QSettings::Windows, "/SOE/SwgGodClient");
		settings.beginGroup(kSettingsGroup);
		s_settings.enabled            = settings.readBoolEntry("enabled",            defaults.enabled);
		s_settings.disableInkGestures = settings.readBoolEntry("disableInkGestures", defaults.disableInkGestures);
		s_settings.barrelNavigation   = settings.readBoolEntry("barrelNavigation",   defaults.barrelNavigation);
		s_settings.pressureToStrength = settings.readBoolEntry("pressureToStrength", defaults.pressureToStrength);
		s_settings.pressureToSize     = settings.readBoolEntry("pressureToSize",     defaults.pressureToSize);
		s_settings.minimumScale       = static_cast<float>(settings.readDoubleEntry("minimumScale",  defaults.minimumScale));
		s_settings.pressureCurve      = static_cast<float>(settings.readDoubleEntry("pressureCurve", defaults.pressureCurve));
		s_settings.eraserInvertsBrush = settings.readBoolEntry("eraserInvertsBrush", defaults.eraserInvertsBrush);
		settings.endGroup();

		s_settings.minimumScale  = clamp01(s_settings.minimumScale);
		s_settings.pressureCurve = std::max(0.2f, std::min(5.0f, s_settings.pressureCurve));
	}

	// ----------------------------------------------------------------------

	void saveSettings()
	{
		QSettings settings;
		settings.insertSearchPath(QSettings::Windows, "/SOE/SwgGodClient");
		settings.beginGroup(kSettingsGroup);
		IGNORE_RETURN(settings.writeEntry("enabled",            s_settings.enabled));
		IGNORE_RETURN(settings.writeEntry("disableInkGestures", s_settings.disableInkGestures));
		IGNORE_RETURN(settings.writeEntry("barrelNavigation",   s_settings.barrelNavigation));
		IGNORE_RETURN(settings.writeEntry("pressureToStrength", s_settings.pressureToStrength));
		IGNORE_RETURN(settings.writeEntry("pressureToSize",     s_settings.pressureToSize));
		IGNORE_RETURN(settings.writeEntry("minimumScale",       static_cast<double>(s_settings.minimumScale)));
		IGNORE_RETURN(settings.writeEntry("pressureCurve",      static_cast<double>(s_settings.pressureCurve)));
		IGNORE_RETURN(settings.writeEntry("eraserInvertsBrush", s_settings.eraserInvertsBrush));
		settings.endGroup();
	}
}

using namespace GodClientPenNamespace;

// ======================================================================

void GodClientPen::install()
{
	if (s_installed)
		return;

	memset(&s_state, 0, sizeof(s_state));
	s_settings = getDefaultSettings();

	HMODULE const user32 = GetModuleHandleA("user32.dll");
	if (user32)
	{
		s_getPointerType            = reinterpret_cast<GetPointerTypeFunction>(GetProcAddress(user32, "GetPointerType"));
		s_getPointerPenInfo         = reinterpret_cast<GetPointerPenInfoFunction>(GetProcAddress(user32, "GetPointerPenInfo"));
		s_setWindowFeedbackSetting  = reinterpret_cast<SetWindowFeedbackSettingFunction>(GetProcAddress(user32, "SetWindowFeedbackSetting"));
	}

	loadSettings();

	s_installed = true;
}

// ----------------------------------------------------------------------

void GodClientPen::remove()
{
	if (!s_installed)
		return;

	for (std::vector<HWND>::const_iterator it = s_windows.begin(); it != s_windows.end(); ++it)
	{
		if (IsWindow(*it))
			IGNORE_RETURN(RemovePropA(*it, kTabletPenServiceProperty));
	}
	s_windows.clear();

	s_getPointerType           = 0;
	s_getPointerPenInfo        = 0;
	s_setWindowFeedbackSetting = 0;
	s_installed                = false;
}

// ----------------------------------------------------------------------

bool GodClientPen::isPointerApiAvailable()
{
	return s_getPointerType != 0 && s_getPointerPenInfo != 0;
}

// ----------------------------------------------------------------------

void GodClientPen::attachWindow(HWND const window)
{
	if (!s_installed || !window)
		return;

	if (std::find(s_windows.begin(), s_windows.end(), window) == s_windows.end())
		s_windows.push_back(window);

	applyInkGestures(window);
}

// ----------------------------------------------------------------------

void GodClientPen::detachWindow(HWND const window)
{
	std::vector<HWND>::iterator const it = std::find(s_windows.begin(), s_windows.end(), window);
	if (it == s_windows.end())
		return;

	s_windows.erase(it);
	if (IsWindow(window))
		IGNORE_RETURN(RemovePropA(window, kTabletPenServiceProperty));
}

// ----------------------------------------------------------------------

void GodClientPen::handleWindowsMessage(MSG const & message)
{
	switch (message.message)
	{
	case kWmPointerUpdate:
	case kWmPointerDown:
	case kWmPointerUp:
	case kWmPointerEnter:
	case kWmPointerLeave:
	case kWmPointerCaptureChanged:
		break;

	default:
		return;
	}

	if (!s_installed || !isPointerApiAvailable())
		return;

	UINT32 const pointerId = static_cast<UINT32>(LOWORD(message.wParam));

	DWORD pointerType = 0;
	if (!s_getPointerType(pointerId, &pointerType) || pointerType != kPointerTypePen)
		return;

	PointerPenInfo info;
	memset(&info, 0, sizeof(info));
	if (!s_getPointerPenInfo(pointerId, &info))
	{
		// The pointer is already gone (left range while capture moved); treat it as lifted away.
		if (message.message == kWmPointerLeave || message.message == kWmPointerCaptureChanged)
		{
			s_state.inRange   = false;
			s_state.inContact = false;
			s_state.pressure  = 0.0f;
		}
		return;
	}

	UINT32 const pointerFlags = info.pointerInfo.pointerFlags;

	// WM_POINTERLEAVE also arrives when the pen moves onto another window; the next update
	// from that window sets it back in range.
	s_state.inRange   = (pointerFlags & kPointerFlagInRange) != 0 && message.message != kWmPointerLeave;
	s_state.inContact = (pointerFlags & kPointerFlagInContact) != 0 && message.message != kWmPointerUp;
	s_state.barrel    = (info.penFlags & kPenFlagBarrel) != 0;
	s_state.eraser    = (info.penFlags & (kPenFlagInverted | kPenFlagEraser)) != 0;

	if (!s_state.inContact)
		s_state.pressure = 0.0f;
	else if ((info.penMask & kPenMaskPressure) != 0)
		s_state.pressure = clamp01(static_cast<float>(info.pressure) / kMaximumPressure);
	else
		s_state.pressure = 1.0f; // a pen without pressure is always fully pressed

	s_state.tiltX    = (info.penMask & kPenMaskTiltX)    != 0 ? static_cast<int>(info.tiltX)    : 0;
	s_state.tiltY    = (info.penMask & kPenMaskTiltY)    != 0 ? static_cast<int>(info.tiltY)    : 0;
	s_state.rotation = (info.penMask & kPenMaskRotation) != 0 ? static_cast<int>(info.rotation) : 0;
	s_state.screenX  = static_cast<int>(info.pointerInfo.ptPixelLocation.x);
	s_state.screenY  = static_cast<int>(info.pointerInfo.ptPixelLocation.y);
	s_state.timeMs   = GetTickCount();
	++s_state.packets;
}

// ----------------------------------------------------------------------

GodClientPen::State const & GodClientPen::getState()
{
	return s_state;
}

// ----------------------------------------------------------------------

bool GodClientPen::hasSeenPen()
{
	return s_state.packets != 0;
}

// ----------------------------------------------------------------------

/**
* True while handling a mouse message that Windows promoted from pen input.
* Only meaningful inside a Qt mouse event handler.
*/
bool GodClientPen::isPenEvent()
{
	DWORD const extraInfo = static_cast<DWORD>(GetMessageExtraInfo());
	if ((extraInfo & kPenSignatureMask) == kPenSignature)
		return true;

	return s_state.inRange && (GetTickCount() - s_state.timeMs) < kPenRecentMs;
}

// ----------------------------------------------------------------------

/**
* True while handling a mouse message from a pen that is touching the surface
* and pen features are enabled: the case where pressure means something.
*/
bool GodClientPen::isPenStroke()
{
	return s_settings.enabled && s_state.inContact && isPenEvent();
}

// ----------------------------------------------------------------------

float GodClientPen::getCurvedPressure()
{
	return clamp01(static_cast<float>(std::pow(static_cast<double>(s_state.pressure), static_cast<double>(s_settings.pressureCurve))));
}

// ----------------------------------------------------------------------

float GodClientPen::getBrushStrengthScale()
{
	if (!s_settings.pressureToStrength)
		return 1.0f;

	return s_settings.minimumScale + (1.0f - s_settings.minimumScale) * getCurvedPressure();
}

// ----------------------------------------------------------------------

float GodClientPen::getBrushSizeScale()
{
	if (!s_settings.pressureToSize)
		return 1.0f;

	return s_settings.minimumScale + (1.0f - s_settings.minimumScale) * getCurvedPressure();
}

// ----------------------------------------------------------------------

bool GodClientPen::isBrushInverted()
{
	return s_settings.eraserInvertsBrush && s_state.eraser;
}

// ----------------------------------------------------------------------

GodClientPen::Settings const & GodClientPen::getSettings()
{
	return s_settings;
}

// ----------------------------------------------------------------------

GodClientPen::Settings GodClientPen::getDefaultSettings()
{
	Settings settings;
	settings.enabled            = true;
	settings.disableInkGestures = true;
	settings.barrelNavigation   = true;
	settings.pressureToStrength = true;
	settings.pressureToSize     = false;
	settings.minimumScale       = 0.1f;
	settings.pressureCurve      = 1.0f;
	settings.eraserInvertsBrush = true;
	return settings;
}

// ----------------------------------------------------------------------

void GodClientPen::setSettings(Settings const & settings)
{
	s_settings = settings;
	s_settings.minimumScale  = clamp01(s_settings.minimumScale);
	s_settings.pressureCurve = std::max(0.2f, std::min(5.0f, s_settings.pressureCurve));

	saveSettings();

	for (std::vector<HWND>::const_iterator it = s_windows.begin(); it != s_windows.end(); ++it)
		applyInkGestures(*it);
}

// ======================================================================
