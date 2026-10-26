# Titan Modern UI

The player-facing UI theme: dark slate glass panels, hairline borders, small corner radii, one azure accent, flat
controls, a lighter HUD, automatic UI scale. Everything here is reproducible from the stock UI source and textures.

## Where the client reads the UI

The game clients (SwgTitan_d / SwgTitan_r) and SwgGodClient run from `exe/win64_rel`. TreeFile adds a
`SearchAbsolute` node above every search path (`TreeFile.cpp`, install), so `ui/...` and `texture/...` resolve
relative to the working directory first:

- `exe/win64_rel/ui/` is the live UI. It mirrors `client/datasources/ui` (UI work copies across).
- `exe/win64_rel/texture/` beats every `.tre` / `.titanpak` copy of a texture.

`tres/ux.titanpak` holds a January 2026 UI that is never used while those loose folders exist.

## Pipeline

```
py -3 client/tools/ui_modern/make_theme.py     # palette + textures + source rules (idempotent)
py -3 client/tools/ui_modern/publish_ui.py     # mirror datasources/ui and out/texture into exe/win64_rel
py -3 client/tools/ui_modern/publish_ui.py --revert   # put back exactly what publish replaced
```

`make_theme.py` does four things, all driven by `theme_spec.py`:

1. **Palette.** It writes `<Palette Name='Titan'>` into `datasources/ui/ui_styles.inc`, derived from the stock
   `Destroyer` palette by moving its teal family onto azure/slate, plus the hand-set entries in
   `PALETTE_OVERRIDES`. UI art is greyscale and the palette tints it (PalColor / PalBgTint / PalText...), so every
   palette in Options > Interface keeps working.
2. **Repaints** (`REPAINTS`). Regions of the stock atlases are redrawn in place: `ui_rebel_final`, `ui_space_hud`,
   `ui_rebel_new_style` and `hud_ground_skin`. A style family is painted one nine-slice set at a time (the
   ImageStyles under one namespace), so every page that reuses those rectangles changes with it. The output goes
   to `out/texture/<atlas>.dds` as uncompressed A8R8G8B8, which keeps 1px lines crisp. `out/preview/<atlas>.png`
   shows stock | themed side by side.
3. **Retargets** (`RETARGETS`). Controls whose stock pieces share pixels (the slider thumb, fill and disabled track
   are one 6x16 patch; the combo arrow is a mirrored scrollbar arrow; `fullFrameIn` holds the toolbar hover art) get
   new art in `out/texture/ui_titan_modern.dds`. Their ImageStyles are rewritten to point there, and a
   `ThemeFrom='atlas:x0,y0,x1,y1'` attribute keeps the stock rectangle. Page-local copies of a style (for example
   `Inventory.comp.vHud4`) follow automatically, matched on stock rectangle and slice part.
4. **Source rules** (`INC_RULES`, `ATTRS`, `COLOR_REMAP`). These are attribute edits in `datasources/ui`: the
   window-frame glow layer goes transparent, the chevron runners are hidden, the slider fill is set to accent, and
   hard-coded teal colours (hue 140-200 degrees, with no palette binding) move to azure. Every rule is idempotent: a
   second run changes nothing (the script prints `source files changed: none`).

## Tools

| Script | What it does |
|---|---|
| `auto_spec.py` | Coverage for everything `theme_spec.py` does not name: every used atlas rectangle no repaint covers is reassembled into elements and classified from its stock pixels. Nine-slice frames are flattened (`paint.Auto`, corner radius measured from the stock silhouette), single-tone line art, glyphs and glows are redrawn crisp (`paint.Crisp`), pictures and icons stay. `make_theme.py` paints these after the hand-written entries; run it alone to list the plan. |
| `uiinc.py` | Lenient .inc/.ui parser: element tree with attribute spans, UI path lookup, in-place rewriter. Line endings are preserved (most files are CRLF). |
| `paint.py` | Painters: `Panel`, `Outline`, `Track`, `Bar`, `Ring`, `Glyph`, `Auto` (flatten keeping outline vs fill), `Colored`, `Box`, `Stack`; DDS writer. |
| `atlas_usage.py` | Which rectangles of an atlas the UI uses, and by whom; `--png` draws them on the atlas. |
| `style_regions.py`, `style_users.py` | Atlas regions behind a style, and who uses a style (name, tint, opacity). |
| `coverage.py` | Rectangles the theme hasn't touched yet, ranked by use (icons legitimately stay). |
| `style_preview.py`, `mock_window.py` | Offline renders of styles / a mock window with the real art and palette. |
| `loading_pages.py` | Writes the SpaceLoading (first screen after sign-in), MustaLoading and HothLoading pages from one modern layout: centred picture + text card, accent progress ring, Exit bottom left. CodeData is unchanged. |
| `ui_client.py` | Drive an offline debug client on port 7111: `launch [--no-scene]`, `run`, `shot` (back-buffer screenshot, retried while the D3D device is lost), `quit`. |
| `tour.py` | Opens the main player windows one by one and screenshots each (`--only name,...`, `--scale`). |
| `ui_at.py` | Which visible widgets sit under a screen point (path, type, rect): from a screenshot to the .inc object. |
| `grab_window.py` | Parks the client window at the bottom of the z-order on the primary display (`--background`). Its PrintWindow capture is black for this D3D9 window; use the bridge screenshot. |

## Client side (C++)

- `ConfigClientUserInterface`: `paletteName` defaults to `Titan` and `uiScalePercent` to `0` (automatic).
- `CuiPreferences`:
  - Automatic scale is the screen height / 864, clamped to 1.0-3.0 in 5% steps: 1080p 125%, 1440p 165%, 2160p 250%.
  - `getUiScalePercent()` returns the effective percent.
  - Any change of the effective factor re-lays the root page out, including per-character options loaded with a
    scene (they bypass `setUiScalePercent`). Before, a character saved at another scale got a HUD squeezed into a
    corner after login or travel.
  - `uiThemeVersion` is reset to 0 before each character's options load (SwgCuiManager, like
    `uiSettingsVersion`): a character saved before the theme has no such key and used to inherit the previous
    character's version, skipping the move to Titan + automatic scale.
  - A per-character `uiThemeVersion` moves a character still on the stock defaults (palette Destroyer / Alpha_Blue,
    scale 100) to Titan + automatic once. Saved window layouts are not reset; `uiSettingsVersion` would reset them.
- Options > Interface: the UI Scale slider goes up to 300%, and Reset Defaults returns it to automatic.
- `UICanvas` (ui library): blits are clipped whenever the canvas matrix is axis-aligned, not only when it is
  translation-only. Before this, any UI scale other than 100% drew scrolled content (the planet map, long option
  lists, text) unclipped past its window. Rotated draws (radar arrows) still skip clipping, as in stock.
  `CuiLayer_EngineCanvas::RenderLineStrip` clips in UI units before scaling.
- UI scale vs camera pixels: UI widgets are in logical units, camera projections in render-target pixels. Fixed
  conversions in the 3D list viewer (camera aim: icons were empty and previews off-centre above 100%), the 3D object
  viewer's viewport, nameplates/status bars (SwgCuiAllTargets), selection brackets and targeting rays (SwgCuiHud), the
  radial and conversation menus, and pies (`CuiLayerRenderer::renderTriangles` scaled UIPie's already-transformed
  vertices a second time).
- Space HUD (`SwgCuiAllTargetsSpace`): the flight reticle's screen centre and dead zone (the virtual joystick's
  centre), the lead indicator and the waypoint indicators are now in UI units; the missile-lock and variable-reticle
  circles (`Graphics::drawCircle`, render-target pixels) convert the centre back to pixels.
- `UITextStyle::RemoveChild` erases glyphs above code 255 from the glyph map instead of leaving a null entry.
  Rebuilding a dynamic font style (applying a face a second time, "(Theme font)", Reset Defaults) used to crash in
  `AddChild` on the typographic quotes and dashes.
- `CuiDynamicUIFont`: the theme font is built as GDI atlases, `cuiuif_*` (regular) and `cuiuifb_*` (bold, used where
  the stock UI asks for "bold", built at semibold weight 600). The face comes from
  `[ClientUserInterface] themeFontFace` and defaults to `Segoe UI` (regular body text, semibold emphasis). The font picker lists "(Theme font)" (empty preference) and "(Stock game font)" (`stock`,
  the original bitmap fonts) ahead of the installed faces.
  - Applied at start-up after `uiManager->SetRootPage` (the `/Fonts` lookup needs it), then `refreshAllUiText`
    re-sets every `font` / `*textstyle*` property so already-loaded widgets pick it up (`UILowerString::get()`
    keeps the registered case, so names are compared lowercased).
  - Sizes match the stock fonts: the number in a font name is the em in UI pixels (not points at the system DPI),
    sizes are no longer pulled toward 14, and glyph quads skip the cell padding and two-thirds of the internal
    leading, so a 12 is a 13-pixel line like `verdana_12`. Quads start where the ink does (pad + A width), which
    stops the last letter of tightly sized tabs and labels from being clipped.
- `CuiMediator::loadSizeLocation` ignores a saved size for pages that are not `UserResizable`: their size is the
  design's, and a saved one kept stale layouts (calendar height, experience bar width) after a redesign.
- DevBridge (debug builds only):
  - `{"cmd":"ui","op":"tree|get|set|press|open|close|scale|fontscale|font"}` (`font`: a face name, `theme`, or `""`
    for the stock fonts)
  - Screenshots are read from the back buffer just before Present, so a covered or unfocused window still captures.
  - `state` includes `ui_scale` (automatic or not, percent, factor, canvas scale, frame buffer size).

## Testing

`ui_client.py launch` starts SwgTitan_d with `titanAdmin=true` (for `/ui reset` and `/ui palette`), loads
Tatooine offline (the 'simple' terrain's magenta sky tints translucent HUD purple) and keeps the window on the primary
display at the bottom of the z-order. Never park a test client on another monitor: the user works there.

- `/ui reset` reloads `.inc` files in place; textures need a client restart.
- Screenshots come from the back buffer just before Present (DevBridge), so the window can sit behind others.
- `screenshot_failed_readback_..._hr88760868` (D3DERR_DEVICELOST) means another app holds exclusive fullscreen on the
  GPU or the display sleeps; `capture()` retries.
- Screenshot paths resolve against the client's working directory and the bridge adds `.jpg`. `tour.py` and
  `ui_client.py shot` pass absolute paths without an extension.
- The game menu hides the HUD and only its Resume button brings it back, so the tour leaves through Resume.
  Inventory opens with the `inventoryClose` action (a toggle).
- Switch Options tabs with `{"cmd":"ui","op":"set","path":"/Opt.OptMain.comp.tabs","prop":"ActiveTab","value":"8"}`
  (8 = Interface).
- Test clients save `local_machine_options.iff` on exit. Back it up before changing machine-wide options (font, font
  scale) from a test.
