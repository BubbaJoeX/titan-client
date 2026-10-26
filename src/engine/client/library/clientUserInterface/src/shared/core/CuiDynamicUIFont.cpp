//======================================================================
//
// CuiDynamicUIFont.cpp
//
//======================================================================

#include "clientUserInterface/FirstClientUserInterface.h"
#include "clientUserInterface/CuiDynamicUIFont.h"

#include "clientUserInterface/CuiChatManager.h"
#include "clientUserInterface/CuiDataDrivenPageManager.h"
#include "clientUserInterface/CuiManager.h"
#include "clientUserInterface/CuiPreferences.h"
#include "clientUserInterface/CuiTextManager.h"
#include "clientUserInterface/CuiWidget3dObjectListViewer.h"
#include "clientUserInterface/CuiWidget3dObjectViewer.h"
#include "clientUserInterface/CuiWidgetGroundRadar.h"
#include "clientGraphics/Texture.h"
#include "clientGraphics/TextureList.h"
#include "sharedFoundation/ConfigFile.h"

#include "UIBaseObject.h"
#include "UIString.h"
#include "UIFontCharacter.h"
#include "UIManager.h"
#include "UINamespace.h"
#include "UIPage.h"
#include "UITextStyle.h"
#include "UITextStyleManager.h"
#include "UIUtils.h"
#include "UnicodeUtils.h"

#include <algorithm>
#include <cstring>
#include <set>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#pragma comment(lib, "gdi32.lib")
#endif

//======================================================================

namespace
{
	char const * const CUI_UIF_FIXED_FACE = "cuiuif";
	char const * const CUI_UIF_BOLD_FACE = "cuiuifb";       // bold weight, for the logical "bold" face
	char const * const CUI_UIF_STOCK = "stock";             // face preference meaning the shipped bitmap fonts

	/// The theme font used while the face preference is empty (Titan Modern, 2026-10-25).
	std::string themeFontFace ()
	{
		return ConfigFile::getKeyString ("ClientUserInterface", "themeFontFace", "Segoe UI");
	}

	/// Family name for the bold atlases: weight-named families ("Segoe UI Semibold") have no bold of their own, so
	/// bold comes from the base family.
	Unicode::String boldFaceFor (Unicode::String const &faceW)
	{
		static char const * const s_weightSuffixes[] = { " Semibold", " SemiBold", " Semilight", " SemiLight", " Light", " Black" };
		std::string const narrow = Unicode::wideToNarrow (faceW);
		for (size_t i = 0; i < sizeof (s_weightSuffixes) / sizeof (s_weightSuffixes[0]); ++i)
		{
			size_t const n = strlen (s_weightSuffixes[i]);
			if (narrow.size () > n && !_stricmp (narrow.c_str () + narrow.size () - n, s_weightSuffixes[i]))
				return Unicode::narrowToWide (narrow.substr (0, narrow.size () - n));
		}
		return faceW;
	}

	bool isAcceptableFontFaceUtf8 (std::string const &s)
	{
		if (s.empty () || s.size () > 260)
			return false;
		for (size_t i = 0; i < s.size (); ++i)
		{
			unsigned char const c = static_cast<unsigned char>(s[i]);
			if (c < 0x20u || c == 0x7fu)
				return false;
		}
		return true;
	}

	int nextPow2 (int v)
	{
		int p = 1;
		while (p < v)
			p <<= 1;
		return p;
	}

	// Atlas code point coverage: ASCII 32..127, Latin-1 supplement 0xA0..0xFF, plus common
	// typography marks (smart quotes, dashes, ellipsis) that show up in chat and in schematic
	// description strings.  Order is fixed so cell layout is deterministic.
	void buildAtlasCodePoints (std::vector<int> &outCodePoints)
	{
		outCodePoints.clear ();
		outCodePoints.reserve (200);
		for (int cp = 32; cp < 128; ++cp)
			outCodePoints.push_back (cp);
		for (int cp = 0xA0; cp <= 0xFF; ++cp)
			outCodePoints.push_back (cp);
		static int const s_typographyCodePoints[] = { 0x2018, 0x2019, 0x201C, 0x201D, 0x2013, 0x2014, 0x2026 };
		for (size_t i = 0; i < sizeof (s_typographyCodePoints) / sizeof (s_typographyCodePoints[0]); ++i)
			outCodePoints.push_back (s_typographyCodePoints[i]);
	}

	typedef std::vector<Texture const *> AtlasTextureVector;

	// List refs of the cui_uifont_* named textures registered by the current generation.
	// Names embed the generation counter, so the replace-by-name path in
	// TextureList::registerNamedPixelTexture never reclaims them; we must drop the named-list
	// ref ourselves when a new generation supersedes them.  Canvases/shaders still holding refs
	// keep the texture alive (refcounted), so this only stops the leak.
	AtlasTextureVector s_currentGenAtlasTextures;

	void releaseAtlasTextures (AtlasTextureVector &textures)
	{
		for (size_t i = 0; i < textures.size (); ++i)
		{
			if (textures[i])
				textures[i]->release ();
		}
		textures.clear ();
	}

	bool utf8FromWide (wchar_t const *w, std::string &outUtf8)
	{
		if (!w || !w[0])
		{
			outUtf8.clear ();
			return true;
		}
		Unicode::String ws;
		ws.assign (reinterpret_cast<char16_t const *>(w), wcslen (w));
		outUtf8 = Unicode::wideToUTF8 (ws);
		return true;
	}

	UIBaseObject * findFontsRoot ()
	{
		return UIManager::gUIManager ().GetObjectFromPath ("/Fonts", TUINamespace);
	}

	struct EnumFontCtx
	{
		std::set<std::string> *facesUtf8;
	};

	// FONTENUMPROCW uses LOGFONTW/TEXTMETRICW; the API passes ENUMLOGFONTEXW (LOGFONTW is its first member).
	int CALLBACK enumFontFamExProc (LOGFONTW const *lplf, TEXTMETRICW const * /*lptm*/, DWORD /*fontType*/, LPARAM lParam)
	{
		ENUMLOGFONTEXW const *const elf = reinterpret_cast<ENUMLOGFONTEXW const *>(lplf);
		EnumFontCtx *const ctx = reinterpret_cast<EnumFontCtx *>(lParam);
		if (!elf || !ctx || !ctx->facesUtf8)
			return 1;
		wchar_t const *const face = elf->elfLogFont.lfFaceName;
		if (!face[0])
			return 1;
		// Skip @ vertical fonts
		if (face[0] == L'@')
			return 1;
		std::string u8;
		if (utf8FromWide (face, u8))
			ctx->facesUtf8->insert (u8);
		return 1;
	}

	bool buildPointSize (Unicode::String const &faceW, int pointSize, int atlasGen, UIBaseObject *fontsRoot, AtlasTextureVector &outAtlasTextures,
		int weight = FW_NORMAL, char const *family = CUI_UIF_FIXED_FACE)
	{
		// The size in a font name is the em in UI pixels, as in the stock bitmap fonts (verdana_bold_12 is a 13 pixel
		// line); the UI scale magnifies from there. Converting it as points at the system DPI made every theme font
		// a third larger than the stock one it replaces (more on high-DPI screens) and clipped it in stock-sized rows.
		int const height = -pointSize;
		HFONT const hFont = CreateFontW (height, 0, 0, 0, weight, FALSE, FALSE, FALSE,
			DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
			FF_DONTCARE | DEFAULT_PITCH, reinterpret_cast<LPCWSTR>(faceW.c_str ()));
		if (!hFont)
			return false;

		HDC const hdc = CreateCompatibleDC (0);
		if (!hdc)
		{
			DeleteObject (hFont);
			return false;
		}
		SelectObject (hdc, hFont);
		SetBkMode (hdc, TRANSPARENT);
		SetTextColor (hdc, RGB (0, 0, 0));

		std::vector<int> codePoints;
		buildAtlasCodePoints (codePoints);
		int const glyphCount = static_cast<int>(codePoints.size ());

		int maxCellW = 1;
		int maxCellH = 1;
		for (int gi = 0; gi < glyphCount; ++gi)
		{
			wchar_t wch = static_cast<wchar_t>(codePoints[static_cast<size_t>(gi)]);
			SIZE sz = {0, 0};
			if (!GetTextExtentPoint32W (hdc, &wch, 1, &sz))
				continue;
			maxCellW = std::max (maxCellW, static_cast<int>(sz.cx));
			maxCellH = std::max (maxCellH, static_cast<int>(sz.cy));
		}

		// Advance widths (ABC) are also used to keep glyphs with negative A-width (e.g. italic
		// overhang) from painting into the neighboring cell: shift the draw origin right by -A.
		std::vector<int> advArr (static_cast<size_t>(glyphCount), 1);
		std::vector<int> advPreArr (static_cast<size_t>(glyphCount), 0);
		std::vector<int> drawShiftArr (static_cast<size_t>(glyphCount), 0);
		std::vector<int> inkArr (static_cast<size_t>(glyphCount), 1);
		for (int gi = 0; gi < glyphCount; ++gi)
		{
			size_t const ix = static_cast<size_t>(gi);
			wchar_t wch = static_cast<wchar_t>(codePoints[ix]);
			ABCFLOAT abc;
			if (GetCharABCWidthsFloatW (hdc, wch, wch, &abc))
			{
				float const w = abc.abcfA + abc.abcfB + abc.abcfC;
				advArr[ix] = std::max (1, static_cast<int>(w + 0.5f));
				advPreArr[ix] = std::max (0, static_cast<int>(abc.abcfA + 0.5f));
				drawShiftArr[ix] = std::max (0, static_cast<int>(-abc.abcfA + 0.5f));
				inkArr[ix] = std::max (1, static_cast<int>(abc.abcfB + 0.999f));
			}
			else
			{
				SIZE sz = {0, 0};
				GetTextExtentPoint32W (hdc, &wch, 1, &sz);
				advArr[ix] = std::max (1, static_cast<int>(sz.cx));
				advPreArr[ix] = 0;
				inkArr[ix] = advArr[ix];
			}
		}

		int const pad = 2;
		int const cellW = maxCellW + pad * 2;
		int const cellH = maxCellH + pad * 2;
		int const cols = 16;
		int const rows = (glyphCount + cols - 1) / cols;
		int const aw = cols * cellW;
		int const ah = rows * cellH;
		int const texW = nextPow2 (aw);
		int const texH = nextPow2 (ah);

		std::vector<uint32> rgba (static_cast<size_t>(texW * texH), 0u);
		BITMAPINFO bi;
		ZeroMemory (&bi, sizeof (bi));
		bi.bmiHeader.biSize = sizeof (BITMAPINFOHEADER);
		bi.bmiHeader.biWidth = texW;
		bi.bmiHeader.biHeight = -texH;
		bi.bmiHeader.biPlanes = 1;
		bi.bmiHeader.biBitCount = 32;
		bi.bmiHeader.biCompression = BI_RGB;

		void *bits = 0;
		HBITMAP const dib = CreateDIBSection (hdc, &bi, DIB_RGB_COLORS, &bits, 0, 0);
		if (!dib || !bits)
		{
			DeleteDC (hdc);
			DeleteObject (hFont);
			return false;
		}
		HBITMAP const oldBmp = static_cast<HBITMAP>(SelectObject (hdc, dib));
		SelectObject (hdc, hFont);
		SetBkMode (hdc, TRANSPARENT);
		RECT clearRc = { 0, 0, texW, texH };
		HBRUSH wbrush = static_cast<HBRUSH>(GetStockObject (WHITE_BRUSH));
		FillRect (hdc, &clearRc, wbrush);
		SetTextColor (hdc, RGB (0, 0, 0));

		TEXTMETRICW tm;
		ZeroMemory (&tm, sizeof (tm));
		GetTextMetricsW (hdc, &tm);
		// Glyph quads start below the cell padding and most of the internal leading (the accent room above the
		// capitals; Segoe UI's is a third of the em), so the line is as tall as a stock font's of the same size
		// name and text fits the rows the stock pages were laid out for. A third of it stays for accented capitals.
		int const trimTop = (static_cast<int>(tm.tmInternalLeading) * 2) / 3;
		int const leading = static_cast<int>(tm.tmHeight + tm.tmExternalLeading) - trimTop;

		for (int gi = 0; gi < glyphCount; ++gi)
		{
			size_t const ix = static_cast<size_t>(gi);
			int const col = gi % cols;
			int const row = gi / cols;
			int const x0 = col * cellW + pad;
			int const y0 = row * cellH + pad;
			RECT cellRc = { col * cellW, row * cellH, col * cellW + cellW, row * cellH + cellH };
			FillRect (hdc, &cellRc, wbrush);
			wchar_t wch = static_cast<wchar_t>(codePoints[ix]);
			// ETO_CLIPPED keeps any remaining overhang pixels inside this glyph's cell.
			ExtTextOutW (hdc, x0 + drawShiftArr[ix], y0, ETO_CLIPPED, &cellRc, &wch, 1, 0);
		}

		uint8 const *src = static_cast<uint8 const *>(bits);
		int const srcPitch = texW * 4;
		for (int y = 0; y < texH; ++y)
		{
			uint8 const *row = src + y * srcPitch;
			for (int x = 0; x < texW; ++x)
			{
				uint8 const b = row[x * 4 + 0];
				uint8 const g = row[x * 4 + 1];
				uint8 const r = row[x * 4 + 2];
				int const lum = (static_cast<int>(r) + static_cast<int>(g) + static_cast<int>(b)) / 3;
				uint8 const a = static_cast<uint8>(255 - lum);
				uint32 const pix = (static_cast<uint32>(a) << 24) | (static_cast<uint32>(0xFF) << 16) | (static_cast<uint32>(0xFF) << 8) | static_cast<uint32>(0xFF);
				rgba[static_cast<size_t>(y * texW + x)] = pix;
			}
		}

		char texName[256];
		_snprintf (texName, sizeof (texName), "texture/cui_uifont_%08x_%s_%d.dds", atlasGen, family, pointSize);
		texName[sizeof (texName) - 1] = 0;
		Texture const * const atlasTexture = TextureList::registerNamedPixelTexture (texName, &rgba[0], TF_ARGB_8888, texW, texH);
		if (atlasTexture)
			outAtlasTextures.push_back (atlasTexture);

		SelectObject (hdc, oldBmp);
		DeleteObject (dib);
		DeleteDC (hdc);
		DeleteObject (hFont);

		char styleName[64];
		_snprintf (styleName, sizeof (styleName), "%s_%d", family, pointSize);
		styleName[sizeof (styleName) - 1] = 0;

		UITextStyle *style = dynamic_cast<UITextStyle *>(fontsRoot->GetChild (styleName));
		if (!style)
		{
			style = new UITextStyle;
			style->SetName (styleName);
			IGNORE_RETURN (fontsRoot->AddChild (style));
		}
		else
		{
			UIBaseObject::UIObjectList gch;
			style->GetChildren (gch);
			for (UIBaseObject::UIObjectList::iterator git = gch.begin (); git != gch.end (); ++git)
			{
				UIBaseObject *const g = *git;
				IGNORE_RETURN (style->RemoveChild (g));
				g->Destroy ();
			}
		}

		Unicode::String texNameWide = Unicode::narrowToWide (texName);
		// Strip "texture/" and ".dds" for canvas factory short name
		Unicode::String shortName = texNameWide;
		static Unicode::String const pref = Unicode::narrowToWide ("texture/");
		static Unicode::String const suf = Unicode::narrowToWide (".dds");
		if (shortName.size () > pref.size () && !UIUnicode::nicmp (shortName.c_str (), pref.c_str (), pref.size ()))
			shortName = shortName.substr (pref.size ());
		if (shortName.size () > suf.size () && !UIUnicode::icmp (shortName.c_str () + shortName.size () - suf.size (), suf.c_str ()))
			shortName.resize (shortName.size () - suf.size ());

		// Index -1 emits the code-0 glyph.  Stock .inc fonts always ship a code-0 "default box"
		// glyph and UITextStyle::GetCharacter falls back to mGlyphArray[0] for any unmapped code
		// point; without it GetCharacter returns NULL for the cuiuif styles.  Clone the space
		// cell (blank pixels, space advance) so unmapped characters stay deterministic.
		for (int gi = -1; gi < glyphCount; ++gi)
		{
			size_t const ix = (gi < 0) ? 0 : static_cast<size_t>(gi);
			int const cp = (gi < 0) ? 0 : codePoints[ix];
			int const adv = advArr[ix];
			int const advPre = advPreArr[ix];

			int const cellIndex = static_cast<int>(ix);
			int const col = cellIndex % cols;
			int const row = cellIndex / cols;
			// UITextStyle draws the quad at pen + AdvancePre, so it starts where the ink does (after the cell padding
			// and the A width), and is as wide as the ink: starting at the cell edge put every glyph pad + A pixels
			// right of where MeasureText expects it, and the last letter of tightly sized labels was clipped.
			int const sx = col * cellW + pad + advPre;
			int const sy = row * cellH;
			int const inkW = std::max (1, std::min (cellW - pad - advPre, std::max (adv - advPre, inkArr[ix]) + 1));

			UIRect srcRect (UIPoint (sx, sy + pad + trimTop), UISize (inkW, std::max (1, cellH - pad - trimTop)));

			UIFontCharacter *ch = new UIFontCharacter;
			ch->SetCharacterCode (static_cast<Unicode::unicode_char_t>(cp));

			Unicode::String val;
			IGNORE_RETURN (UIUtils::FormatLong (val, static_cast<long>(cp)));
			IGNORE_RETURN (ch->SetProperty (UIFontCharacter::PropertyName::Code, val));

			IGNORE_RETURN (UIUtils::FormatLong (val, static_cast<long>(adv)));
			IGNORE_RETURN (ch->SetProperty (UIFontCharacter::PropertyName::Advance, val));

			IGNORE_RETURN (UIUtils::FormatLong (val, static_cast<long>(advPre)));
			IGNORE_RETURN (ch->SetProperty (UIFontCharacter::PropertyName::AdvancePre, val));

			IGNORE_RETURN (UIUtils::FormatRect (val, srcRect));
			IGNORE_RETURN (ch->SetProperty (UIFontCharacter::PropertyName::SourceRect, val));

			IGNORE_RETURN (ch->SetProperty (UIFontCharacter::PropertyName::SourceFile, shortName));

			IGNORE_RETURN (style->AddChild (ch));
		}

		Unicode::String leadVal;
		IGNORE_RETURN (UIUtils::FormatLong (leadVal, static_cast<long>(leading)));
		IGNORE_RETURN (style->SetProperty (UITextStyle::PropertyName::Leading, leadVal));

		return true;
	}
}

//----------------------------------------------------------------------

namespace
{
	/// Re-resolve every font property in the UI (pages and the /Styles tree) from the logical font name each object
	/// asked for. Fonts are resolved once, when the property is set, so without this a font that becomes active after
	/// the UI loads (the theme font at start-up, any face from the picker) only reached objects created later: windows
	/// mixed stock Verdana with the theme font, at sizes borrowed from whichever name last touched a shared style.
	int reresolveFontProperties (UIBaseObject &obj)
	{
		if (obj.IsA (TUITextStyle))
			return 0;                          // glyphs only
		int count = 0;
		UIBaseObject::UIPropertyNameVector names;
		obj.GetPropertyNames (names, false);
		for (UIBaseObject::UIPropertyNameVector::const_iterator it = names.begin (); it != names.end (); ++it)
		{
			// get () returns the name as first registered ("Font", "TextStyleNormal"), not lowercased
			std::string n = it->get ();
			std::transform (n.begin (), n.end (), n.begin (), ::tolower);
			if (n != "font" && n.find ("textstyle") == std::string::npos)
				continue;
			UIString v;
			if (obj.GetProperty (*it, v) && !v.empty () && obj.SetProperty (*it, v))
				++count;
		}
		UIBaseObject::UIObjectList children;
		obj.GetChildren (children);
		for (UIBaseObject::UIObjectList::const_iterator c = children.begin (); c != children.end (); ++c)
			if (*c)
				count += reresolveFontProperties (**c);
		return count;
	}

	void reapplyTextStyleProperty (UIBaseObject *obj, UILowerString const &prop)
	{
		if (!obj)
			return;
		UIString v;
		if (obj->GetProperty (prop, v) && !v.empty ())
			IGNORE_RETURN (obj->SetProperty (prop, v));
	}

	void refreshCachedWidgetTextStyles (UIBaseObject *obj)
	{
		if (!obj)
			return;
		char const *const tn = obj->GetTypeName ();
		if (tn)
		{
			if (!strcmp (tn, CuiWidgetGroundRadar::TypeName))
				reapplyTextStyleProperty (obj, CuiWidgetGroundRadar::PropertyName::TextStyle);
			else if (!strcmp (tn, CuiWidget3dObjectViewer::TypeName))
				reapplyTextStyleProperty (obj, CuiWidget3dObjectViewer::PropertyName::TextStyle);
			else if (!strcmp (tn, CuiWidget3dObjectListViewer::TypeName))
			{
				reapplyTextStyleProperty (obj, CuiWidget3dObjectListViewer::PropertyName::TextStyleBottom);
				reapplyTextStyleProperty (obj, CuiWidget3dObjectListViewer::PropertyName::TextStyleTop);
			}
		}

		if (obj->IsA (TUIPage))
		{
			UIPage *const p = static_cast<UIPage *>(obj);
			UIBaseObject::UIObjectList const &ch = p->GetChildrenRef ();
			UIBaseObject::UIObjectList chSnapshot (ch.begin (), ch.end ());
			for (UIBaseObject::UIObjectList::const_iterator it = chSnapshot.begin (); it != chSnapshot.end (); ++it)
			{
				if (*it)
					refreshCachedWidgetTextStyles (*it);
			}
		}
	}
}

//======================================================================

void CuiDynamicUIFont::refreshAllUiText ()
{
	UIPage *const root = UIManager::gUIManager ().GetRootPage ();
	if (root)
		IGNORE_RETURN (reresolveFontProperties (*root));
	if (root)
		root->ResetLocalizedStrings ();

	if (root)
	{
		if (UIPage *const gh = static_cast<UIPage *>(root->GetChild ("GroundHud")))
			gh->ResetLocalizedStrings ();
		if (UIPage *const hs = static_cast<UIPage *>(root->GetChild ("HudSpace")))
			hs->ResetLocalizedStrings ();
	}

	CuiDataDrivenPageManager::resetLocalizedStringsForAllPages ();

	CuiTextManager::refreshFontStyleCache ();

	CuiChatManager::refreshChatWindowStylesIfInstalled ();

	if (root)
		refreshCachedWidgetTextStyles (root);
}

//======================================================================

void CuiDynamicUIFont::enumFontFacesUtf8 (std::vector<std::string> &outUtf8Faces)
{
	outUtf8Faces.clear ();
#if !defined(_WIN32)
	UNREF (outUtf8Faces);
	return;
#else
	std::set<std::string> faces;
	HDC const hdc = GetDC (0);
	EnumFontCtx ctx;
	ctx.facesUtf8 = &faces;
	LOGFONTW lf;
	ZeroMemory (&lf, sizeof (lf));
	lf.lfCharSet = DEFAULT_CHARSET;
	EnumFontFamiliesExW (hdc, &lf, enumFontFamExProc, reinterpret_cast<LPARAM>(&ctx), 0);
	ReleaseDC (0, hdc);
	outUtf8Faces.assign (faces.begin (), faces.end ());
#endif
}

//----------------------------------------------------------------------

bool CuiDynamicUIFont::applyFontFaceUtf8 (std::string const &utf8Face)
{
#if !defined(_WIN32)
	UNREF (utf8Face);
	return false;
#else
	if (!UITextStyleManager::GetInstance () || !UITextStyleManager::GetInstance ()->IsA (TUITextStyleManager))
		return false;

	if (utf8Face.empty ())
	{
		clearUserFont ();
		return true;
	}

	if (!isAcceptableFontFaceUtf8 (utf8Face))
		return false;

	Unicode::String const faceW = Unicode::utf8ToWide (utf8Face);
	if (faceW.empty ())
		return false;

	UIBaseObject *const fontsRoot = findFontsRoot ();
	if (!fontsRoot)
		return false;

	static int s_gen = 1;
	int const gen = s_gen++;

	// Keep /Fonts.cuiuif_<pt> object identities stable for the lifetime of the UI. Many legacy
	// widgets cache raw UITextStyle pointers, and there is no complete registry with which to rebind
	// every cache before deleting these objects.

	// Sizes used across HUD, options, and SUI; must cover scaled lookups (font slider + UI scale).
	// Include every integer 10–40 plus 9 and common larger sizes so lookups never miss after scaling.
	static int const s_pointSizes[] =
	{
		9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
		25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
		41, 42, 44, 48
	};
	AtlasTextureVector newAtlasTextures;
	for (size_t i = 0; i < sizeof (s_pointSizes) / sizeof (s_pointSizes[0]); ++i)
	{
		if (!buildPointSize (faceW, s_pointSizes[i], gen, fontsRoot, newAtlasTextures)
			|| !buildPointSize (boldFaceFor (faceW), s_pointSizes[i], gen, fontsRoot, newAtlasTextures, FW_SEMIBOLD, CUI_UIF_BOLD_FACE))
		{
			UITextStyleManager::GetInstance ()->clearUserDefaultFontFace ();
			CuiPreferences::setUiDefaultFontFaceUtf8 ("");
			refreshAllUiText ();
			// A failed generation may already be referenced by a partially rebuilt style. Retain its
			// named texture refs until the next complete generation replaces every style.
			s_currentGenAtlasTextures.insert (s_currentGenAtlasTextures.end (), newAtlasTextures.begin (), newAtlasTextures.end ());
			return false;
		}
	}

	// Only after all atlases are rebuilt: point the resolver at the new face (empty was handled above).
	UITextStyleManager::GetInstance ()->setUserDefaultFontFaceUtf8 (utf8Face);
	CuiPreferences::setUiDefaultFontFaceUtf8 (utf8Face);

	// Drop the named-texture list refs of the superseded generation (names embed the gen counter,
	// so registerNamedPixelTexture's replace-by-name never reclaims them).
	releaseAtlasTextures (s_currentGenAtlasTextures);
	s_currentGenAtlasTextures.swap (newAtlasTextures);

	CuiManager::scheduleUiScaleLayoutUpdate ();
	refreshAllUiText ();
	return true;
#endif
}

//----------------------------------------------------------------------

void CuiDynamicUIFont::clearUserFont ()
{
	CuiPreferences::setUiDefaultFontFaceUtf8 (CUI_UIF_STOCK);
#if defined(_WIN32)
	UITextStyleManager *const mgr = UITextStyleManager::GetInstance ();
	if (mgr)
		mgr->clearUserDefaultFontFace ();
	// Rebind known caches to stock fonts, but deliberately keep generated styles and their current
	// atlases alive. Unregistered legacy widgets may still hold raw UITextStyle pointers.
	refreshAllUiText ();
	CuiManager::scheduleUiScaleLayoutUpdate ();
#endif
}

//----------------------------------------------------------------------

void CuiDynamicUIFont::resetToThemeFont ()
{
	CuiPreferences::setUiDefaultFontFaceUtf8 ("");
	applySavedPreferenceIfAny ();
}

//----------------------------------------------------------------------

void CuiDynamicUIFont::applySavedPreferenceIfAny ()
{
#if defined(_WIN32)
	UITextStyleManager *const mgr = UITextStyleManager::GetInstance ();
	if (mgr)
	{
		mgr->setFontScalePercent (CuiPreferences::getUiFontScalePercent ());
		mgr->setUserFontFullReplace (CuiPreferences::getUiFontFullReplace ());
	}
	std::string const s = CuiPreferences::getUiDefaultFontFaceUtf8 ();
	if (s == CUI_UIF_STOCK)
		return;
	if (!s.empty ())
	{
		IGNORE_RETURN (applyFontFaceUtf8 (s));
		return;
	}
	//-- no choice saved: the theme font, without recording it as the player's choice
	std::string const theme = themeFontFace ();
	if (!theme.empty () && theme != CUI_UIF_STOCK && applyFontFaceUtf8 (theme))
		CuiPreferences::setUiDefaultFontFaceUtf8 ("");
#endif
}

//======================================================================
