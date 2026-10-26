"""Titan Modern: the design, as data. make_theme.py turns it into textures, a palette and themed UI pages.

Design system
-------------
Dark slate glass panels, soft 1px hairline borders, 4-5px corner radii, one azure accent, flat controls (no bevel
highlight or shade), underline tabs, regular-weight Segoe UI with semibold for emphasis, no grids, glows or notched
corners. Colour lives in the palette (UI images are greyscale and tinted in game), so every repaint below gives
shape and luminance only. Whatever this file doesn't name is covered by auto_spec.py: frames are flattened, line art
and glyphs are redrawn crisp, pictures stay.

    neutrals  #0B0F14 #121820 #18202A #202A36 #2A3644 #364455 #4A5B70 #6E8096 #9AABBF #C9D5E2 #EEF3F8
    accent    #1E5A8C (deep)  #2F8BD8 (base)  #5AB0F5 (light)  #8FD0FF (glow)
    semantic  success #4CC38A  warning #F0B44C  danger #E5534B  gold #E2B455
"""
from __future__ import annotations

from dataclasses import dataclass, field

from paint import Auto, Bar, Box, Clear, Colored, Glyph, HLine, Outline, Panel, Ring, Solid, Stack, Track

# ----------------------------------------------------------------------------------------------------------------------
# Palette. 'Titan' is derived from the stock 'Destroyer' palette (the old default) by remapping its teal family onto
# the azure/slate scheme; the entries the chrome actually leans on are then set by hand.

PALETTE_NAME = "Titan"
PALETTE_BASE = "Destroyer"

PALETTE_OVERRIDES = {
    # surfaces (multiplied by panel art whose fill luminance is ~0.5 and border 1.0)
    "back1": "#33465C", "back2": "#3C5068", "back3": "#2A3848", "back4": "#2E3D4E", "back5": "#263241",
    "backDrop": "#7FA3C8", "BaseDark": "#1A2230", "BaseLight": "#3D5068", "forceBackground": "#7FA3C8",
    "AccentDark": "#1A2533", "AccentLight": "#5AB0F5",
    "titleBar": "#2F8BD8", "bottomBar": "#2A3644", "titleText": "#EEF3F8",
    # lines and outlines
    "line1": "#40546C", "line2": "#4E6884", "line3": "#5AB0F5", "line4": "#5AB0F5", "line5": "#5AB0F5",
    "outline1": "#40546C", "outline2": "#40546C", "outline3": "#40546C", "outline4": "#40546C", "outline5": "#40546C",
    "Runner": "#40546C", "runnerDark": "#000000", "glow": "#5AB0F5", "anim": "#5AB0F5", "lightning": "#5AB0F5",
    # text
    "text1": "#D3DDE8", "text2": "#EEF3F8", "text3": "#5AB0F5", "text4": "#EEF3F8", "text5": "#0B0F14",
    "header": "#8CC4F2", "textServer": "#EEF3F8", "TextLight": "#9AABBF", "TextDark": "#121820", "textMed": "#6E8096",
    "listtext": "#D3DDE8", "textskill": "#E2B455", "textMagic": "#B48CF2", "textinsured": "#F0B44C",
    "textdefault": "#DCE6F0", "textHover": "#FFFFFF", "textActivated": "#FFFFFF", "textSelected": "#FFFFFF",
    "textDisabled": "#6B7A8C",
    "tableHeaderTextDefault": "#9AABBF", "tableHeaderTextHover": "#FFFFFF", "tableHeaderTextActivated": "#EEF3F8",
    "tableHeaderTextSelected": "#EEF3F8", "tableHeaderTextDisabled": "#6B7A8C",
    "head1": "#8CC4F2", "head2": "#8CC4F2", "head3": "#8CC4F2", "head4": "#8CC4F2", "head5": "#8CC4F2",
    # controls (button art: flat fill ~0.9, rim 1.0)
    "default": "#2C3A4B", "hover": "#3A5878", "Activated": "#2A6FB0", "selected": "#2F7FC4", "disabled": "#222A34",
    "Highlight": "#5AB0F5", "contrast1": "#5AB0F5", "contrast2": "#F0B44C", "contrast3": "#EEF3F8",
    "contrast3a": "#0B0F14", "contrast3h": "#2F8BD8", "contrast4": "#EEF3F8", "contrast4a": "#8FD0FF",
    "contrast5": "#1A2533", "contrast5a": "#EEF3F8",
    "Icon": "#9FC6EC", "icondefault": "#9FC6EC", "iconhover": "#FFFFFF", "iconactivated": "#5AB0F5",
    "icondisabled": "#4A5868", "iconGlow": "#8FD0FF",
    "closeNormal": "#9AABBF", "closeHover": "#FFFFFF", "closeActivated": "#E5534B",
    "closeDarkNormal": "#9AABBF", "closeDarkHover": "#FFFFFF", "closeDarkActivated": "#E5534B",
    "arrowdefault": "#9AABBF", "arrowHover": "#FFFFFF", "arrowActivated": "#5AB0F5", "arrowSelected": "#5AB0F5",
    "arrowDisabled": "#364455",
    "scrolldefault": "#4A5B70", "scrollhover": "#7F93AA", "scrollactivated": "#5AB0F5", "scrollselected": "#5AB0F5",
    "scrolldisabled": "#2A3644",
    "slot": "#33465C", "equipped": "#C9D5E2", "listback": "#2A3644", "listHilite": "#2F8BD8", "listline": "#40546C",
    "selectionBack": "#1E5A8C",
    "numDefault": "#2C3A4B", "numHover": "#3A5878", "numActivated": "#2A6FB0",
    "numTextDefault": "#DCE6F0", "numTextHover": "#FFFFFF", "numTextActivated": "#FFFFFF",
    "raddefault": "#22303F", "radhover": "#2F7FC4", "radactivated": "#2A6FB0", "raddisabled": "#3A4656",
    "radialTextDark": "#EEF3F8", "radialTextLight": "#EEF3F8", "radspin": "#5AB0F5", "radar": "#5AB0F5",
    "skilldefault": "#2F7FC4", "skillhover": "#3A5878", "skillactivated": "#2F8BD8", "skillselected": "#2F8BD8",
    "skilldisabled": "#2C3A4B", "skillTextDefault": "#FFFFFF", "skillTextActivated": "#FFFFFF",
    "skillAcquiredDefault": "#B88A2E", "skillAcquiredHover": "#D4A040", "skillAcquiredActivated": "#B88A2E",
    "skillAcquiredSelected": "#B88A2E", "skillAcquiredDisabled": "#7A6233",
    "exp": "#E2B455", "loadBack": "#2A3644", "loadBar": "#121820", "loadbarFill": "#2F8BD8",
    "triangleOn": "#5AB0F5", "triangleOff": "#4A5B70", "triangleHover": "#9FC6EC",
    "tab1": "#4CC38A", "tab2": "#4CC38A", "tab3": "#4CC38A", "tab4": "#4CC38A", "tab5": "#F0B44C",
    "OverlayDark": "#2F8BD8", "overlayLight": "#C9D5E2", "spaceGlow": "#FFFFFF", "zoneback": "#FFFFFF",
    "IconPositive": "#4CC38A", "IconNegative": "#E5534B", "health": "#E5534B", "stamina": "#4CC38A",
    "holocron": "#5AB0F5", "helpBox": "#121820",
}


# ----------------------------------------------------------------------------------------------------------------------
# Atlas repaints: (style family or (atlas, rect), painter[, options]).
# A style family is repainted cluster by cluster: its ImageStyles' rectangles are grouped where they touch, and each
# group's bounding box is painted as one element. Families that share pixels are listed once.

W_RADIUS = 5      # windows
C_RADIUS = 4      # controls and boxes

panel_window = Panel(radius=W_RADIUS, fill=0.48, fill_a=0.90, border=0.95, border_a=0.55)
panel_box = Panel(radius=C_RADIUS, fill=0.62, fill_a=1.0, border=1.0, border_a=0.60)
panel_soft = Panel(radius=C_RADIUS, fill=0.55, fill_a=0.85, border=0.9, border_a=0.55)
panel_inner = Panel(radius=C_RADIUS, fill=0.42, fill_a=0.70, border=0.85, border_a=0.45)
# flat: no bevel highlight or shade; the state colour (palette default / hover / activated / selected) is the fill
button = Panel(radius=C_RADIUS, fill=0.92, fill_a=1.0, border=1.0, border_a=0.35, highlight=0.0, shade=0.0)
hairline = Outline(radius=C_RADIUS, lum=1.0, alpha=1.0, width=1.0)

REPAINTS = [
    # --- window chrome -----------------------------------------------------------------------------------------------
    ("/Styles.New.thin_in", panel_window),
    ("/Styles.New.main_TR", Panel(radius=W_RADIUS, fill=0.30, fill_a=0.96, border=1.0, border_a=0.0,
                                  title_h=0, round_corners=(True, True, False, False), bottom_rule=1.0, inset=0.0)),
    ("/Styles.New.main_BR", Panel(radius=W_RADIUS, fill=0.60, fill_a=0.0, border=1.0, border_a=0.0,
                                  round_corners=(False, False, True, True))),
    ("/Styles.space.boxes.boxlineTopLeft", Clear()),
    ("/Styles.space.boxes.boxlineTopLeft2", Clear()),
    ("/Styles.space.boxes.generic4", Panel(radius=C_RADIUS, fill=0.55, fill_a=0.85, border=1.0, border_a=0.9)),
    ("/Styles.New.thin_mmc", Clear()),
    # --- boxes, wells and outlines -----------------------------------------------------------------------------------
    ("/Styles.New.whitebox", panel_box),
    ("/Styles.New.whiteOutline", hairline),
    ("/Styles.New.fatFrameInside", panel_inner),
    ("/Styles.New.pill_med", panel_soft),
    ("/Styles.New.pill_lrg", panel_soft),
    ("/Styles.New.pill_smallest", panel_soft),
    ("/Styles.New.pill_stack", panel_soft),
    ("/Styles.New.whitebox_square", Panel(radius=1, fill=0.62, border=1.0)),
    ("/Styles.New.fullFrame", Auto(C_RADIUS)),
    ("/Styles.New.skinnyInside", Auto(C_RADIUS)),
    ("/Styles.New.tool.tool_tl_in", Auto(C_RADIUS)),
    ("/Styles.New.tool.tool_tip", Auto(C_RADIUS)),
    ("/Styles.window.mfd.mfd5", panel_soft),
    # --- buttons -------------------------------------------------------------------------------------------------------
    ("/Styles.New.buttons.hud", button),
    ("/Styles.New.buttons.exp", button),
    ("/Styles.New.riskRound", button),
    ("/Styles.New.buttons.numbers", button),
    ("/Styles.New.buttons.newInFrame2", button),
    ("/Styles.New.buttons.tree2", button),
    ("/Styles.New.tree_default", button),
    ("/Styles.New.tree_acquired", button),
    ("/Styles.buttons.button_prev", button),
    ("/Styles.buttons.button_next", button),
    ("/Styles.New.combo.combo1", panel_soft),
    # list rows: hover / selected tint the whole row instead of underlining it
    ("/Styles.New.text_blank.activated", Solid(lum=1.0, alpha=0.30)),
    # --- glyphs ----------------------------------------------------------------------------------------------------------
    ("/Styles.Frame.buttonClose", Glyph("x", stroke=1.7, pad=3.5)),
]

# ----------------------------------------------------------------------------------------------------------------------
# Page rules applied to the generated copy of the UI (the source .inc files stay stock).
# Each rule: (predicate on node, attribute, new value). Values starting with '=' are copied from another attribute.

TRANSPARENT_RS = "/Styles.New.text_blank.default.rs"

INC_RULES = [
    # The 'lines' layer of every window frame drew a white glow over the body; modern windows don't have it.
    (lambda n: n.name == "lines" and n.attrs.get("RStyleDefault", "").lower().startswith("/styles.space.boxes.generic4"),
     "RStyleDefault", TRANSPARENT_RS),
]

# ----------------------------------------------------------------------------------------------------------------------
# Controls get new art in a theme atlas of their own (texture/ui_titan_modern.dds) instead of a repaint: their stock
# pieces share pixels (the slider thumb, fill and disabled track are one 6x16 patch; the combo arrow is a mirrored
# scrollbar arrow), so they can't be redrawn in place. Each Retarget paints one element and points the ImageStyles
# under `paths` at its slices (caps = left, top, right, bottom slice sizes; mirrored stock parts stay mirrored).

MODERN_ATLAS = "ui_titan_modern"
MODERN_ATLAS_SIZE = 256

ACCENT = "#2F8BD8"
ACCENT_LIGHT = "#5AB0F5"
CONTROL_RIM = "#7F93AA"


@dataclass
class Retarget:
    paths: list
    size: tuple
    painter: object
    caps: tuple = (0, 0, 0, 0)
    optional: bool = False      # paths generated for style variants that may lack some parts


SLIDER = "/Styles.New.Slider.default.horizontal"
SLIDERS = [SLIDER, "/Styles.New.Slider.default.map"]
SCROLL_V = ["/Styles.scrollbar.default.vHud4", "/Styles.scrollbar.default.vHud3", "/Styles.scrollbar.default.vHud",
            "/Styles.scrollbar.default.vHud2", "/Styles.scrollbar.default.comboall", "/Styles.scrollbar.default.green"]
SCROLL_H = ["/Styles.New.scrollbar.H2", "/Styles.scrollbar.default.H2"]
STATES = ("default", "hover", "activated")

checkbox_off = Box(Colored(Panel(radius=2.5, fill=0.0, fill_a=0.45, border=1.0, border_a=1.0), CONTROL_RIM), 1, 0, 14, 13)
checkbox_on = Box(Stack(Colored(Panel(radius=2.5, fill=1.0, border=1.0), ACCENT),
                        Glyph("check", lum=1.0, stroke=1.6, pad=1.5)), 1, 0, 14, 13)
radio_off = Box(Colored(Ring(width=1.3, inset=0.5, fill_a=0.45, fill=0.0), CONTROL_RIM), 8, 4, 21, 17)
radio_on = Box(Stack(Colored(Ring(width=1.3, inset=0.5, fill_a=0.45, fill=0.0), ACCENT_LIGHT),
                     Colored(Glyph("dot", stroke=1.0, pad=2.0), ACCENT_LIGHT)), 8, 4, 21, 17)
knob = Box(Panel(radius=7, fill=1.0, border=0.62, border_a=1.0), 6, 0, 20, 14)

RETARGETS = [
    # slider: thin track, accent fill, round knob, small chevrons for the step buttons
    Retarget([f"{p}.track" for p in SLIDERS], (12, 10), Track(thick=2, lum=1.0, alpha=0.55), (3, 0, 3, 0), True),
    Retarget([f"{p}.disabledTrack.default" for p in SLIDERS], (12, 10), Track(thick=2, lum=1.0, alpha=0.30),
             (3, 0, 3, 0), True),
    Retarget([f"{p}.overlay" for p in SLIDERS], (12, 12), Track(thick=3, lum=1.0, alpha=1.0), (3, 0, 3, 0), True),
    Retarget([f"{p}.thumb.{s}" for p in SLIDERS for s in STATES], (27, 14), knob, (13, 0, 13, 0), True),
    Retarget([f"{p}.left.{s}" for p in SLIDERS for s in STATES], (11, 8), Glyph("chevron_left", stroke=1.3, pad=0.5),
             optional=True),
    Retarget([f"{p}.right.{s}" for p in SLIDERS for s in STATES], (11, 8), Glyph("chevron_right", stroke=1.3, pad=0.5),
             optional=True),
    # scrollbars: slim pill thumb on a faint rail; arrows become chevrons (the stock up/down art is one image,
    # mirrored for the other end, and that stays so)
    Retarget([f"{p}.thumb.{s}" for p in SCROLL_V for s in STATES], (10, 12),
             Box(Panel(radius=3, fill=1.0, border=1.0, border_a=0.0), 2, 0, 8, 12), (0, 4, 0, 4), True),
    Retarget([f"{p}.track" for p in SCROLL_V], (10, 8), Track(thick=2, lum=1.0, alpha=0.35, vertical=True), (0, 3, 0, 3),
             True),
    Retarget([f"{p}.{e}.{s}" for p in SCROLL_V for e in ("start", "end") for s in STATES],
             (10, 12), Glyph("chevron_up", stroke=1.4, pad=1.5), optional=True),
    Retarget([f"{p}.thumb.{s}" for p in SCROLL_H for s in STATES], (12, 8),
             Box(Panel(radius=3, fill=1.0, border=1.0, border_a=0.0), 0, 1, 12, 7), (4, 0, 4, 0), True),
    Retarget([f"{p}.track" for p in SCROLL_H], (12, 8), Track(thick=2, lum=1.0, alpha=0.35), (3, 0, 3, 0), True),
    Retarget([f"{p}.{e}.{s}" for p in SCROLL_H for e in ("start", "end") for s in STATES],
             (13, 10), Glyph("chevron_right", stroke=1.4, pad=1.5), optional=True),
    Retarget([f"/Styles.New.buttons.combo_arrow.{s}" for s in ("default", "hover", "activated", "selected", "disabled")],
             (10, 10), Glyph("chevron_down", stroke=1.4, pad=1.0)),
    # check boxes and radio buttons keep their stock image sizes (TextOffset and layouts depend on them)
    Retarget(["/Styles.New.checkbox.uncheckedImage", "/Styles.New.checkbox2.uncheckedImage"], (18, 13), checkbox_off),
    Retarget(["/Styles.New.checkbox.checkedImage", "/Styles.New.checkbox2.checkedImage"], (18, 13), checkbox_on),
    Retarget(["/Styles.New.radiobutton.uncheckedImage"], (29, 21), radio_off),
    Retarget(["/Styles.New.radiobutton.checkedImage"], (29, 21), radio_on),
]

# Single attribute overrides on style objects: (path, attribute, value).
ATTRS = [
    (f"{SLIDER}.overlay.rs", "PalColor", "contrast1"),            # filled part of a slider: accent
    (f"{SLIDER}.thumb.default.rs", "PalColor", "text1"),          # light knob
    (f"{SLIDER}.thumb.hover.rs", "PalColor", "textHover"),
    (f"{SLIDER}.thumb.activated.rs", "PalColor", "contrast1"),
]

# ----------------------------------------------------------------------------------------------------------------------
# Ground HUD skin (texture/hud_ground_skin.dds). These Images are drawn untinted, so the art carries its colour.
# Rectangles were measured from the stock alpha (connected components) and the pages' layouts: the status plates
# put the role icon at (3,18) 41x40 inside the ring and the HAM bars at (40,27) 202x20 to its right
# (ui_ground_hud_mfd_status_skinned.inc), which is where the new name/bar panel and ring sit.

HUD = "hud_ground_skin"
GLASS = "#0B1118"      # HUD glass fill
RIM = "#B8C7D9"        # HUD hairlines


def plate(w, h, panel, ring):
    """Status plate: dark glass panel behind the name and bars, ring around the role icon (drawn on top)."""
    px0, py0, px1, py1 = panel
    cx, cy, rx, ry = ring
    return Stack(
        Box(Colored(Panel(radius=5, fill=0.06, fill_a=0.58, border=0.75, border_a=0.30), RIM), px0, py0, px1, py1),
        Box(Colored(Ring(width=1.5, inset=0.75, fill_a=0.70, fill=0.05), RIM), cx - rx, cy - ry, cx + rx, cy + ry),
    )


def ring_button(fill, rim, rim_w=1.5):
    """Round HUD button: a filled disc in one colour under a rim in another."""
    return Stack(Colored(Ring(width=rim_w, inset=0.75, fill_a=0.88, fill=1.0, alpha=0.0), fill),
                 Colored(Ring(width=rim_w, inset=0.75, alpha=0.95), rim))


HUD_REPAINTS = [
    # chat window: the metal frame goes; the chat panel's own dark background carries it
    ((HUD, (0, 0, 312, 100)), Clear()),
    # the skinned composite tiles the 7px fill images (339..346) as "fillerObject"s; bilinear sampling at each tile
    # edge reads the neighbouring columns, so clear a wide margin around them (nothing else lives there above y 139)
    ((HUD, (312, 0, 460, 138)), Clear()),
    ((HUD, (0, 108, 312, 192)), Clear()),
    ((HUD, (339, 108, 346, 151)), Clear()),
    ((HUD, (461, 0, 512, 18)), Colored(Panel(radius=4, fill=0.55, fill_a=0.85, border=1.0, border_a=0.75,
                                             round_corners=(True, True, False, False)), "#3A4D63")),
    # status plates: player / target / secondary target, jedi, pet
    ((HUD, (260, 269, 502, 324)), plate(242, 55, (30, 2, 242, 42), (22.5, 37.5, 18.5, 17))),
    ((HUD, (0, 195, 207, 248)), plate(207, 53, (28, 2, 207, 44), (20.5, 34.5, 18.5, 17.5))),
    ((HUD, (368, 213, 509, 266)), plate(141, 53, (28, 1, 141, 40), (19.5, 36.0, 18.0, 16.5))),
    # HAM bar art (bar background and fill share it; the game tints each)
    ((HUD, (224, 339, 324, 352)), Bar(radius=2.5, lum=1.0, sheen=0.22, inset=1.0)),
    # toolbar tray (used whole, as top/middle/bottom strips and as the pet bar's caps), pane selector, big slot
    ((HUD, (0, 370, 499, 420)), Colored(Panel(radius=7, fill=0.06, fill_a=0.50, border=0.75, border_a=0.22), RIM)),
    ((HUD, (205, 261, 255, 325)), Colored(Panel(radius=7, fill=0.06, fill_a=0.62, border=0.75, border_a=0.30,
                                                inset=2), RIM)),
    ((HUD, (7, 427, 79, 507)), Stack(Colored(Panel(radius=9, fill=0.06, fill_a=0.62, border=1.0, border_a=0.0, inset=4), RIM),
                                     Colored(Outline(radius=9, width=2.0, alpha=0.95, inset=4), "#5AB0F5"))),
    # button bar: enabled-hover / enabled / disabled-hover / disabled
    ((HUD, (0, 259, 46, 307)), Box(ring_button("#1E3A57", "#5AB0F5", 2.0), 6, 12, 40, 46)),
    ((HUD, (46, 259, 92, 307)), Box(ring_button("#16202B", "#8FB4D8"), 6, 12, 40, 46)),
    ((HUD, (92, 259, 140, 307)), Box(ring_button("#121820", "#6E8096"), 7, 12, 41, 46)),
    ((HUD, (140, 259, 186, 307)), Box(ring_button("#0E1318", "#4A5B70"), 4, 7, 42, 45)),
    # location display: three coordinate slots
    ((HUD, (391, 495, 510, 510)), Stack(*[Box(Colored(Panel(radius=3, fill=0.06, fill_a=0.62, border=0.75, border_a=0.30), RIM),
                                              1 + i * 40, 1, 1 + i * 40 + 38, 15) for i in range(3)])),
    # vehicle throttle and the experience monitor
    ((HUD, (158, 481, 341, 503)), Colored(Panel(radius=4, fill=0.06, fill_a=0.55, border=0.75, border_a=0.25, inset=1), RIM)),
    ((HUD, (170, 469, 324, 481)), Colored(Bar(radius=2.5, lum=1.0, sheen=0.22, inset=2.0), "#5AB0F5")),
    ((HUD, (159, 420, 420, 435)), Colored(Panel(radius=3, fill=0.06, fill_a=0.55, border=0.75, border_a=0.25, inset=1), RIM)),
    ((HUD, (218, 175, 321, 189)), Colored(Bar(radius=2.5, lum=1.0, sheen=0.22, inset=2.0), "#E2B455")),
]
REPAINTS += HUD_REPAINTS

# Hard-coded teal (attributes with no palette binding next to them) moves onto the azure scheme with the same
# transform as the palette (make_theme.derive_color). Only hues 140-200 degrees match and the results land at 201+,
# so a re-run never remaps a colour twice.
COLOR_REMAP = {"TextColor": "PalText", "BackgroundTint": "PalBgTint", "Color": "PalColor", "BackgroundColor": None,
               "NormalTextColor": "PalNormalText", "MouseOverTextColor": "PalHoverText",
               "PressedTextColor": "PalActivatedText", "SelectedTextColor": "PalSelectedText",
               "DisabledTextColor": "PalDisabledText", "NormalIconColor": "PalNormalIcon",
               "MouseOverIconColor": "PalHoverIcon", "SelectedIconColor": "PalSelectedIcon",
               "DisabledIconColor": "PalDisabledIcon", "PressedIconColor": "PalActivatedIcon"}

RETARGETS += [
    # login / character-flow card back (its stock corners also hold the toolbar hover art, so no repaint in place)
    Retarget(["/Styles.New.fullFrameIn.default"], (48, 48),
             Panel(radius=6, fill=0.50, fill_a=0.88, border=1.0, border_a=0.90), (12, 12, 12, 12)),
    # BACK / NEXT pods behind the wizard buttons: the buttons carry it now
    Retarget(["/Styles.New.nextFrame.default", "/Styles.New.PrevFrame.default"], (12, 12), Clear(), (4, 0, 4, 0)),
    # the loading-screen progress pie (only the loading pages use Pie.Res): a solid patch instead of the green tick,
    # so the pie draws a clean ring tinted by PieColor. The pie samples the image's whole source rectangle, so the
    # image is the inner 8x8 of a 24x24 patch (caps 8): no filtering seams between the ring's segments.
    Retarget(["/Styles.space.Pie.Res.c"], (24, 24), Solid(), (8, 8, 8, 8)),
]

INC_RULES += [
    # the animated chevron runners (>>>>>> behind the BACK / NEXT captions of the login and character flow)
    (lambda n: n.tag == "Runner" and any(c.attrs.get("RStyleDefault", "").lower().startswith(
        "/styles.mfd.statusbars.arrows_right") for c in n.children),
     "Visible", "false"),
]

# Radar (texture/hud_ground_radar.dds, untinted): hairline rings at the stock radius, an accent north marker, round
# zoom / con-mode buttons, and the elite target plate with a gold ring (ring position measured from the stock alpha).
RADAR = "hud_ground_radar"
GOLD = "#E2B455"


def radar_ring(inset, width):
    return Colored(Ring(width=width, inset=inset, alpha=0.70), RIM)


def plate_ring(panel, ring, rim):
    px0, py0, px1, py1 = panel
    cx, cy, rx, ry = ring
    return Stack(
        Box(Colored(Panel(radius=5, fill=0.06, fill_a=0.58, border=0.75, border_a=0.30), RIM), px0, py0, px1, py1),
        Box(Stack(Colored(Ring(width=1.8, inset=0.9, fill_a=0.70, fill=0.05, alpha=0.0), RIM),
                  Colored(Ring(width=1.8, inset=0.9, alpha=0.95), rim)), cx - rx, cy - ry, cx + rx, cy + ry),
    )


REPAINTS += [
    ((RADAR, (12, 146, 147, 281)), radar_ring(2.0, 1.5)),
    ((RADAR, (3, 307, 206, 508)), radar_ring(2.0, 1.6)),
    ((RADAR, (242, 244, 512, 512)), radar_ring(3.0, 1.8)),
    ((RADAR, (241, 5, 270, 48)), Colored(Glyph("triangle_down", stroke=1.0, pad=6), "#5AB0F5")),
    ((RADAR, (135, 9, 151, 33)), Colored(Glyph("triangle_down", stroke=1.0, pad=2), "#5AB0F5")),
    ((RADAR, (20, 11, 41, 37)), Colored(Glyph("triangle_down", stroke=1.0, pad=4), "#5AB0F5")),
    ((RADAR, (436, 18, 457, 38)), Stack(ring_button("#16202B", "#8FB4D8", 1.4), Colored(Glyph("plus", stroke=1.6, pad=4), "#DCE6F0"))),
    ((RADAR, (435, 44, 455, 65)), Stack(ring_button("#16202B", "#8FB4D8", 1.4), Colored(Glyph("minus", stroke=1.6, pad=4), "#DCE6F0"))),
    ((RADAR, (437, 81, 456, 99)), ring_button("#16202B", "#6E8096", 1.4)),
    ((RADAR, (436, 117, 455, 135)), Stack(ring_button("#1E3A57", "#5AB0F5", 1.4), Colored(Glyph("dot", pad=3), "#5AB0F5"))),
    ((RADAR, (25, 54, 290, 121)), plate_ring((36, 2, 262, 44), (33, 42, 22, 21), GOLD)),
]

# 3D viewer backdrop (character sheet, inventory paper doll...): its 4px centre strip is tiled vertically, which drew
# the scan lines; a uniform panel tiles cleanly. Tinted by the page (PalBgTint slot / back1).
REPAINTS += [(("ui_rebel_viewer", (0, 0, 128, 128)), Panel(radius=4, fill=0.32, fill_a=0.92, border=0.70, border_a=0.85))]

INC_RULES += [
    # hard-coded scrollbar backgrounds (#42FF00 lime behind the chat scrollbar): a dark track instead
    (lambda n: n.tag == "Scrollbar" and "BackgroundTint" in n.attrs and "PalBgTint" not in n.attrs
     and n.attrs["BackgroundTint"].upper() in ("#42FF00", "#1D69AD"), "BackgroundTint", "#0B1118"),
]

INC_RULES += [
    # Combo dropdowns open over other controls, and at the stock 60-80 % the page underneath read through the entries.
    # UIComboBox gives its dropdown list the ComboBox's own BackgroundOpacity, which the list draws its background
    # rectangle style with; the ListStyle's BackgroundOpacity only counts for a list style without one (style_sui).
    (lambda n: n.tag == "ComboBox" and float(n.attrs.get("BackgroundOpacity", "1")) < 1.0,
     "BackgroundOpacity", "1.00"),
    (lambda n: n.tag == "ListStyle" and n.name in ("style_all", "style_sui")
     and n.path().lower().endswith(("styles.list.combo." + n.name.lower(), "styles.space.list.combo." + n.name.lower())),
     "BackgroundOpacity", "1.00"),
]

# The root page's palette is what the login, server and character screens use before a character's options load.
ATTRS += [("/", "palette", "/styles.palette.Titan")]          # "/" = the root page in ui_root.ui
# The ground loading screen's root was PackSize '1,1' and stopped short of the right and bottom edges at UI scales
# above 100 % (the uncleared back buffer showed through). 'a,a' grows it with the root like every other full page.
ATTRS += [("/Load2", "PackSize", "a,a")]

INC_RULES += [
    # the 40 scan-line pages ("original.light1..40", New.line) drawn over every 3D viewer (character sheet, crafting,
    # inventory paper doll): a clean viewer instead. The galaxy map's light## travel routes are not under 'original'.
    (lambda n: n.tag == "Page" and n.parent is not None and n.parent.name == "original"
     and n.name.startswith("light") and n.name[5:].isdigit(), "Visible", "false"),
]

# experience bar segment ticks (expMon.tick): hairlines instead of the stock metal pips
REPAINTS += [((HUD, (227, 448, 240, 461)), Colored(Track(thick=1.4, lum=1.0, alpha=0.55, vertical=True), RIM))]

REPAINTS += [
    ("/Styles.New.whiteOutlineBroken", Outline(radius=C_RADIUS, lum=1.0, alpha=0.9, width=1.0)),
    ("/Styles.New.whiteOutlineThick", Outline(radius=C_RADIUS, lum=1.0, alpha=1.0, width=2.0)),
    # metal end caps of the experience bar: the bar's own panel frames it now
    ((HUD, (382, 139, 389, 155)), Clear()),
    ((HUD, (486, 139, 493, 155)), Clear()),
]

# message boxes: the blue bracket corners go; their chat-style panels get the soft panel
REPAINTS += [
    ("/Styles.New.corner_chatTL", Clear()),
    ("/Styles.New.thin_mmc2", Clear()),
    ("/Styles.New.chat.small", panel_soft),
    ("/Styles.New.chat.big", panel_soft),
]

# ----------------------------------------------------------------------------------------------------------------------
# Tabs, redesigned: no boxes. Every tab sits on one faint shared baseline; hover lifts a soft fill; the selected tab
# gets a 2px underline (the palette's 'selected' colour, the accent) and bright text. Retargeted per state so each
# state has its own shape.

def tab_art(state: str):
    base = HLine(lum=1.0, alpha=0.22, thick=1.0, y=None)
    if state in ("selected", "activated"):
        return Stack(Solid(lum=1.0, alpha=0.07), HLine(lum=1.0, alpha=1.0, thick=2.0, y=None))
    if state == "hover":
        return Stack(Solid(lum=1.0, alpha=0.10), HLine(lum=1.0, alpha=0.55, thick=1.0, y=None))
    if state == "disabled":
        return HLine(lum=1.0, alpha=0.10, thick=1.0, y=None)
    return base


class _Bottom:
    """Draw `painter`'s line at the bottom edge (HLine centres by default)."""
    def __init__(self, painter):
        self.painter = painter

    def __repr__(self):
        return f"_Bottom({self.painter!r})"

    def __call__(self, w, h, slices=None):
        p = self.painter
        if isinstance(p, HLine):
            return HLine(p.lum, p.alpha, p.thick, h - p.thick, p.fade)(w, h)
        if isinstance(p, Stack):
            return Stack(*[_Bottom(q) if isinstance(q, HLine) else q for q in p.painters])(w, h)
        return p(w, h)


TAB_STATES = ("default", "hover", "activated", "selected", "disabled")
RETARGETS += [
    Retarget([f"/Styles.New.tabs.horizontal_10.middlebutton.{s}"], (24, 16), _Bottom(tab_art(s)), (8, 0, 8, 0))
    for s in TAB_STATES
]
# tabs.hud is left as is: its art is the same pixels as /Styles.New.buttons.hud, and every page-local copy of
# those buttons would follow a retarget.

# ----------------------------------------------------------------------------------------------------------------------
# Window-frame decoration lines. Every window frame has a 'lines' layer whose children draw corner rules
# (boxline*); v1 blanked two of them, the rest drew a stray rule across the bottom of windows, behind any buttons
# placed there (the game menu's Exit / Resume row). Modern frames have no such rules.
REPAINTS += [(f"/Styles.space.boxes.{n}", Clear()) for n in (
    "boxlineTopRight", "boxlineTopRight2", "boxlinebottomLeft", "boxlinebottomLeft2", "boxlinebottomRight",
    "boxlinebottomRight2", "boxlineglowBL", "boxlineglowBR", "boxlineglowTL", "boxlineglowTR")]

# Game menu: 'Locations' hung below the window. The menu grows by 30 px and the button moves inside it, between
# 'Return to Character Selection' and the Exit / Resume row.
_GM_GROW = {"menu": 168, "bg": 168, "frame": 168, "lines": 168, "back": 167}   # new heights (+30)


def _in_game_menu(n):
    p = n
    while p is not None:
        if p.name == "GameMenu":
            return True
        p = p.parent
    return False


def _gm_box(n):
    return n.tag == "Page" and n.name in _GM_GROW and _in_game_menu(n) and n.name != "GameMenu"


def _gm_height(n, attr):
    if attr not in n.attrs:
        return None
    return f"{n.attrs[attr].split(',')[0]},{_GM_GROW[n.name]}"


INC_RULES += [
    (lambda n: _gm_box(n), "Size", lambda n: _gm_height(n, "Size")),
    (lambda n: _gm_box(n), "ScrollExtent", lambda n: _gm_height(n, "ScrollExtent")),
]
ATTRS += [
    ("/GameMenu.menu.bg.frame.bottom", "Location", "0,145"),
    ("/GameMenu.menu.exitButton", "Location", "16,143"),
    ("/GameMenu.menu.resumeButton", "Location", "159,143"),
    ("/GameMenu.locationsButton", "Location", "399,411"),
    ("/GameMenu.locationsButton", "Size", "231,23"),            # as wide as the other menu buttons
    ("/GameMenu.menu", "MaximumSize", "280,190"),              # stock 280,160 clamped the grown menu
]
# Experience bar: stock packs it across the whole screen bottom (cfc, a), where it ran through the toolbar's key
# numbers like a ruler. Left-anchored and proportional, it sits under the chat column and stops short of the toolbar
# (627 of the 1024 design width = 940 of 1536).
ATTRS += [
    ("/PDA.expMon", "PackLocation", "nfn,fff"),
    ("/PDA.expMon", "PackSize", "p,f"),
    ("/PDA.expMon", "Size", "627,13"),
    ("/PDA.expMon", "ScrollExtent", "627,13"),
]
# Calendar: the window ran 48 units past its Close button (content ends at 502)
ATTRS += [
    (f"/CalendarSystem.calendarWindow{p}", a, "750,516")
    for p in ("", ".bg") for a in (("MinimumSize", "ScrollExtent", "Size") if not p else ("ScrollExtent", "Size"))
]
