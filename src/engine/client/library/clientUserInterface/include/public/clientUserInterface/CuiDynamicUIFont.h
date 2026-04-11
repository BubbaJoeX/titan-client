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

	/// Build GPU atlases + /Fonts.cuiuif_<pt> styles; empty string clears to stock default.
	static bool applyFontFaceUtf8 (std::string const &utf8Face);

	static void clearUserFont ();

	/// After CuiManager + fonts are installed; uses CuiPreferences.
	static void applySavedPreferenceIfAny ();

	/// Re-run text style binding on root, GroundHud/HudSpace, and all open SUI pages.
	static void refreshAllUiText ();
};

//======================================================================

#endif
