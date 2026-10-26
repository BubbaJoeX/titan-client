// ======================================================================
//
// GodClientPen.h
// Pen tablet support for the God Client (Huion Kamvas, Wacom, any Windows Ink pen).
//
// ======================================================================

#ifndef INCLUDED_GodClientPen_H
#define INCLUDED_GodClientPen_H

// ======================================================================

/**
* GodClientPen tracks the stylus of a Windows Ink pen tablet.
*
* Windows promotes pen input to ordinary mouse messages, so Qt already sees the pen as a
* mouse: the tip is the left button and the barrel button (with the tip down) is the right
* button.  What the promoted messages lose is pressure, tilt and the eraser.  GodClientPen
* reads those from the WM_POINTER messages that precede the promotion (fed to it from
* GodClientApplication::winEventFilter) and keeps the latest stylus state, so a mouse
* handler can ask whether its event came from the pen and how hard the pen is pressed.
*
* It also opts the 3D view out of the Windows Ink gestures that get in the way of
* dragging: press-and-hold right click, flicks and the tap/barrel ripples.
*
* The pointer API needs Windows 8 or later and is loaded at run time; without it, or with
* a tablet driver in mouse mode, the pen simply stays a mouse.
*/

class GodClientPen
{
public:

	struct State
	{
		bool          inRange;   // hovering or touching over a god client window
		bool          inContact; // tip (or eraser) on the surface
		bool          barrel;    // barrel button held
		bool          eraser;    // eraser end, or a button mapped to eraser
		float         pressure;  // 0..1 as reported by the tablet
		int           tiltX;     // degrees, -90..90
		int           tiltY;     // degrees, -90..90
		int           rotation;  // degrees, 0..359
		int           screenX;
		int           screenY;
		unsigned long packets;   // pen updates seen since install
		unsigned long timeMs;    // GetTickCount of the last update
	};

	struct Settings
	{
		bool  enabled;            // master switch for everything below
		bool  disableInkGestures; // no press-and-hold right click, flicks or ripples in the 3D view
		bool  barrelNavigation;   // barrel-button drag orbits the camera, Shift pans, Ctrl zooms
		bool  pressureToStrength; // terrain brush strength follows pen pressure
		bool  pressureToSize;     // terrain brush size follows pen pressure
		float minimumScale;       // brush scale at the lightest touch, 0..1
		float pressureCurve;      // exponent: below 1 is soft (light touches count more), above 1 is firm
		bool  eraserInvertsBrush; // eraser swaps raise and lower
	};

public:

	static void install();
	static void remove();

	static bool isPointerApiAvailable();

	static void attachWindow(HWND window);
	static void detachWindow(HWND window);
	static void handleWindowsMessage(MSG const & message);

	static State const & getState();
	static bool          hasSeenPen();
	static bool          isPenEvent();
	static bool          isPenStroke();

	static float getCurvedPressure();
	static float getBrushStrengthScale();
	static float getBrushSizeScale();
	static bool  isBrushInverted();

	static Settings const & getSettings();
	static Settings         getDefaultSettings();
	static void             setSettings(Settings const & settings);

private:

	GodClientPen();
	GodClientPen(GodClientPen const &);
	GodClientPen & operator=(GodClientPen const &);
};

// ======================================================================

#endif
