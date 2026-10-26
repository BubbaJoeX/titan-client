"""Offline mock of a standard UI window built from the real styles, palette and (themed) atlases, for judging the
theme without starting the client. Layers follow the stock window anatomy (ui_options.inc 'OptMain', ui_voice.inc):

    bg.back   thin_in   PalBgTint back1             bg.frame  thin_in   PalBgTint baselight
    frame.top main_TR   PalColor titleBar (29px)    frame.bottom main_BR PalColor bottomBar (23px)
    content   pill_med back1, fatFrameInside_palW back2 @0.5, whitebox_palW back3, whiteOutline_palW line1 @0.7
    buttons   New.buttons.hud (default / hover / pressed / selected / disabled), list rows (text_blank)

    py -3 client/tools/ui_modern/mock_window.py out.png [--palette Titan] [--scene grass|dark]
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, str(Path(__file__).resolve().parent))
import style_preview as sp  # noqa: E402
import uiinc  # noqa: E402


def font(size: int, bold: bool = False):
    for name in (["segoeuib.ttf", "verdanab.ttf"] if bold else ["segoeui.ttf", "verdana.ttf"]):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            pass
    return ImageFont.load_default()


def rect_style(theme: sp.Theme, path: str):
    n = uiinc.resolve(theme.top, path)
    if n is None or n.tag != "RectangleStyle":
        raise KeyError(path)
    return n


def layer(canvas: Image.Image, theme: sp.Theme, path: str, x, y, w, h, tint=None, opacity=1.0):
    n = rect_style(theme, path)
    col = sp.hex_color(theme.palette[tint.lower()]) if tint else None
    im = theme.render_rect(n, w, h, col)
    if opacity < 1:
        a = np.asarray(im).astype(np.float32)
        a[..., 3] *= opacity
        im = Image.fromarray(a.astype(np.uint8))
    canvas.alpha_composite(im, (x, y))


def pal(theme, key):
    return sp.hex_color(theme.palette[key.lower()])


def button(canvas, theme, x, y, w, h, label, state):
    ns = {"normal": "default", "hover": "hover", "pressed": "activated", "selected": "selected", "disabled": "disabled"}[state]
    n = uiinc.resolve(theme.top, f"/Styles.New.buttons.hud.{ns}.rs")
    canvas.alpha_composite(theme.render_rect(n, w, h), (x, y))
    txt = {"normal": "textdefault", "hover": "texthover", "pressed": "textactivated", "selected": "textselected",
           "disabled": "textdisabled"}[state]
    d = ImageDraw.Draw(canvas)
    f = font(12, True)
    tw = d.textlength(label, font=f)
    d.text((x + (w - tw) / 2, y + h / 2 - 8), label, fill=pal(theme, txt), font=f)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out", type=Path)
    ap.add_argument("--palette", default="Titan")
    ap.add_argument("--scene", default="grass")
    args = ap.parse_args()
    theme = sp.Theme(Path("D:/titan/client/datasources/ui"), args.palette)
    W, H = 760, 520
    if args.scene == "grass":
        yy = np.linspace(0, 1, H)[:, None, None]
        bg = (np.array([70, 110, 60]) * (1 - yy) + np.array([95, 120, 70]) * yy) * np.ones((H, W, 3))
        bg[: H // 3] = [120, 150, 190]
        canvas = Image.fromarray(bg.astype(np.uint8)).convert("RGBA")
    else:
        canvas = Image.new("RGBA", (W, H), (18, 20, 24, 255))
    d = ImageDraw.Draw(canvas)

    x, y, w, h = 40, 30, 680, 460
    layer(canvas, theme, "/Styles.New.thin_in.rs_default", x, y, w, h, "back1")
    layer(canvas, theme, "/Styles.New.thin_in.rs_default", x + 1, y, w - 1, h, "baselight")
    layer(canvas, theme, "/Styles.New.main_TR.rs_default", x - 1, y, w + 1, 29)
    layer(canvas, theme, "/Styles.New.main_BR.rs_default", x, y + h - 23, w, 23)
    d.text((x + 12, y + 6), "CHARACTER", fill=pal(theme, "titletext"), font=font(14, True))
    close = uiinc.resolve(theme.top, "/Styles.Frame.buttonClose.rs_default")
    canvas.alpha_composite(theme.render_rect(close, 16, 16), (x + w - 24, y + 6))

    # tab strip
    tx = x + 14
    for i, t in enumerate(["Status", "Personal", "Factions", "Skill Mods", "GCW"]):
        tw = int(d.textlength(t, font=font(12, True))) + 22
        button(canvas, theme, tx, y + 38, tw, 22, t, "selected" if i == 0 else "normal")
        tx += tw + 4

    # left list well
    lx, ly, lw, lh = x + 14, y + 70, 220, 330
    layer(canvas, theme, "/Styles.New.pill_med.rs_default", lx, ly, lw, lh, "back1")
    layer(canvas, theme, "/Styles.New.fatFrameInside_palW.rs_default", lx, ly, lw, lh, "back2", 0.5)
    layer(canvas, theme, "/Styles.New.whiteOutline_palW.rs_default", lx, ly, lw, lh, "line1", 0.7)
    d.text((lx + 10, ly + 8), "Attributes", fill=pal(theme, "header"), font=font(13, True))
    rows = ["Health", "Action", "Strength", "Constitution", "Stamina", "Precision", "Agility", "Luck"]
    for i, r in enumerate(rows):
        ry = ly + 34 + i * 24
        if i == 2:
            layer(canvas, theme, "/Styles.New.text_blank.activated.rs", lx + 4, ry - 2, lw - 8, 22)
        if i == 4:
            layer(canvas, theme, "/Styles.New.text_blank.hover.rs", lx + 4, ry - 2, lw - 8, 22)
        d.text((lx + 12, ry + 1), r, fill=pal(theme, "text1"), font=font(12))
        d.text((lx + lw - 50, ry + 1), str(100 + i * 7), fill=pal(theme, "contrast1"), font=font(12, True))

    # right detail box
    rx, ry, rw, rh = lx + lw + 12, ly, w - lw - 40, 200
    layer(canvas, theme, "/Styles.New.whitebox_palW.rs_default", rx, ry, rw, rh, "back3", 0.7)
    layer(canvas, theme, "/Styles.New.whiteOutline_palW.rs_default", rx, ry, rw, rh, "line1", 0.7)
    d.text((rx + 12, ry + 10), "Description", fill=pal(theme, "header"), font=font(13, True))
    d.multiline_text((rx + 12, ry + 34), "A seasoned smuggler from the Outer Rim.\nPrefers blasters, fast ships and\nfaster exits.",
                     fill=pal(theme, "text1"), font=font(12), spacing=5)

    # buttons row
    bx, by = rx, ry + rh + 16
    for i, st in enumerate(["normal", "hover", "pressed", "selected", "disabled"]):
        button(canvas, theme, bx + i * 82, by, 76, 22, st.capitalize(), st)

    # footer buttons
    button(canvas, theme, x + 14, y + h - 34, 150, 22, "OK", "normal")
    button(canvas, theme, x + w - 164, y + h - 34, 150, 22, "Cancel", "normal")
    canvas.convert("RGB").save(args.out)
    print(args.out)


if __name__ == "__main__":
    main()
