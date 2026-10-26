//======================================================================
//
// CuiDynamicUIFont.h
//
//======================================================================

#ifndef INCLUDED_CuiDynamicUIFont_H
#define INCLUDED_CuiDynamicUIFont_H

#include <string>
#include <vector>

//----------------------------------------------------------------------

class CuiDynamicUIFont
{
public:
	/// UTF-8 encoded family names (e.g. from EnumFontFamiliesEx).
	static void enumFontFacesUtf8 (std::vector<std::string> &outUtf8Faces);

	/// Build/apply GPU font atlases and persist the selection; empty resets to game default.
	static bool applyFontFaceUtf8 (std::string const &utf8Face);

	/// Switch to the shipped bitmap fonts (persisted as "stock") without invalidating generated style pointers.
	static void clearUserFont ();

	/// Back to the theme font ([ClientUserInterface] themeFontFace) with an empty preference (Reset Defaults).
	static void resetToThemeFont ();

	/// Initialize font face, replacement mode, and scale from CuiPreferences. An empty face preference means
	/// the theme font; "stock" means the shipped bitmap fonts.
	static void applySavedPreferenceIfAny ();

	/// Re-run text style binding on root, GroundHud/HudSpace, and all open SUI pages.
	static void refreshAllUiText ();
};

//======================================================================

#endif
