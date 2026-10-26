"""Render UI RectangleStyles (nine-slice) and ButtonStyles offline, the way UIRectangleStyle draws them.

    py -3 client/tools/ui_modern/style_preview.py out.png /Styles.New.whitebox.rs_default /Styles.New.buttons.hud.style ...
        [--ui DIR] [--palette NAME] [--size 180x40] [--bg #20262e]

Corners draw at their source size, edges stretch along one axis, the centre stretches both ways; the style's colour
(PalColor through the palette, else Color) multiplies the texture. ButtonStyles render one swatch per state with the
state's text colour. Textures come from the theme output first (out/texture), then the stock client texture folder.
It is how make_theme.py's art was checked before going into the client.
"""
from __future__ import annotations

import argparse
import sys
from functools import lru_cache
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, str(Path(__file__).resolve().parent))
import uiinc  # noqa: E402

HERE = Path(__file__).resolve().parent
TEX_DIRS = [HERE / "out/texture", Path("D:/titan/data/sku.0/sys.client/compiled/game/texture")]
PARTS = ["NorthWest", "North", "NorthEast", "West", "Center", "East", "SouthWest", "South", "SouthEast"]


@lru_cache(maxsize=None)
def texture(name: str) -> Image.Image | None:
    for d in TEX_DIRS:
        p = d / f"{name.lower()}.dds"
        if p.exists():
            return Image.open(p).convert("RGBA")
    return None


def hex_color(s: str) -> tuple[int, int, int, int]:
    s = s.lstrip("#")
    if len(s) == 6:
        return int(s[0:2], 16), int(s[2:4], 16), int(s[4:6], 16), 255
    if len(s) == 8:
        return int(s[2:4], 16), int(s[4:6], 16), int(s[6:8], 16), int(s[0:2], 16)
    return 255, 255, 255, 255


class Theme:
    def __init__(self, ui_dir: Path, palette: str):
        self.top = uiinc.load_tree(ui_dir)
        pal_ns = uiinc.resolve(self.top, "/Styles.palette")
        self.palette = {}
        for p in (pal_ns.children if pal_ns else []):
            if p.tag == "Palette" and p.name.lower() == palette.lower():
                self.palette = {k.lower(): v for k, v in p.attrs.items()}

    def image(self, node: uiinc.Node, ref: str) -> Image.Image | None:
        """An ImageStyle referenced relative to the RectangleStyle's namespace (default.c, c, ../x.y)."""
        base = node.parent
        target = base
        for part in ref.split("."):
            target = target.child(part) if target else None
        if target is None or "SourceRect" not in target.attrs:
            return None
        src = target.attrs.get("Source", "").split(":")[-1]
        tex = texture(src)
        if tex is None:
            return None
        x0, y0, x1, y1 = (int(v) for v in target.attrs["SourceRect"].split(","))
        # A reversed rectangle mirrors the image along that axis.
        im = tex.crop((min(x0, x1), min(y0, y1), max(x0, x1), max(y0, y1)))
        if x1 < x0:
            im = im.transpose(Image.FLIP_LEFT_RIGHT)
        if y1 < y0:
            im = im.transpose(Image.FLIP_TOP_BOTTOM)
        return im if im.size[0] and im.size[1] else None

    def tint(self, node: uiinc.Node, override: str | None = None):
        pal = (override or node.attrs.get("PalColor", "")).lower()
        if pal and pal in self.palette:
            return hex_color(self.palette[pal])
        return hex_color(node.attrs.get("Color", "#FFFFFF"))

    def render_rect(self, node: uiinc.Node, w: int, h: int, tint=None) -> Image.Image:
        parts = {p: (self.image(node, node.attrs[p]) if p in node.attrs else None) for p in PARTS}
        out = Image.new("RGBA", (w, h), (0, 0, 0, 0))

        def size(p):
            return parts[p].size if parts[p] else (0, 0)

        lw = max(size("NorthWest")[0], size("West")[0], size("SouthWest")[0])
        rw = max(size("NorthEast")[0], size("East")[0], size("SouthEast")[0])
        th = max(size("NorthWest")[1], size("North")[1], size("NorthEast")[1])
        bh = max(size("SouthWest")[1], size("South")[1], size("SouthEast")[1])
        cw, ch = max(w - lw - rw, 0), max(h - th - bh, 0)
        boxes = {
            "NorthWest": (0, 0, lw, th), "North": (lw, 0, cw, th), "NorthEast": (lw + cw, 0, rw, th),
            "West": (0, th, lw, ch), "Center": (lw, th, cw, ch), "East": (lw + cw, th, rw, ch),
            "SouthWest": (0, th + ch, lw, bh), "South": (lw, th + ch, cw, bh), "SouthEast": (lw + cw, th + ch, rw, bh),
        }
        for p, (x, y, bw, bh2) in boxes.items():
            im = parts[p]
            if im is None or bw <= 0 or bh2 <= 0:
                continue
            out.alpha_composite(im.resize((bw, bh2), Image.NEAREST), (x, y))
        r, g, b, a = tint or self.tint(node)
        px = out.load()
        for y in range(h):
            for x in range(w):
                pr, pg, pb, pa = px[x, y]
                px[x, y] = (pr * r // 255, pg * g // 255, pb * b // 255, pa * a // 255)
        return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out", type=Path)
    ap.add_argument("paths", nargs="+")
    ap.add_argument("--ui", type=Path, default=Path("D:/titan/client/datasources/ui"))
    ap.add_argument("--palette", default="Destroyer")
    ap.add_argument("--size", default="180x40")
    ap.add_argument("--bg", default="#20262e")
    args = ap.parse_args()
    w, h = (int(v) for v in args.size.split("x"))
    theme = Theme(args.ui, args.palette)
    font = ImageFont.load_default()

    rows = []
    for path in args.paths:
        node = uiinc.resolve(theme.top, path)
        if node is None:
            rows.append((path, [("missing", None)]))
            continue
        if node.tag == "RectangleStyle":
            rows.append((path, [("", theme.render_rect(node, w, h))]))
        elif node.tag == "ButtonStyle":
            cells = []
            for state, key, txt in [("normal", "RStyleDefault", "PalNormalText"), ("hover", "RStyleMouseOver", "PalHoverText"),
                                    ("pressed", "RStyleActivated", "PalActivatedText"),
                                    ("selected", "RStyleSelected", "PalSelectedText"),
                                    ("disabled", "RStyleDisabled", "PalDisabledText")]:
                ref = node.attrs.get(key)
                rs = None
                if ref:
                    t = node.parent
                    for part in ref.split("."):
                        t = t.child(part) if t else None
                    rs = t
                im = theme.render_rect(rs, w, h) if rs is not None and rs.tag == "RectangleStyle" else Image.new("RGBA", (w, h))
                pal = node.attrs.get(txt, "").lower()
                col = hex_color(theme.palette[pal]) if pal in theme.palette else (255, 255, 255, 255)
                ImageDraw.Draw(im).text((8, h // 2 - 5), state.upper(), fill=col, font=font)
                cells.append((state, im))
            rows.append((path, cells))
        else:
            rows.append((path + f" ({node.tag})", [("?", None)]))

    pad, label_h = 10, 14
    ncols = max(len(c) for _, c in rows)
    sheet = Image.new("RGBA", (pad + ncols * (w + pad), pad + len(rows) * (h + label_h + pad)), hex_color(args.bg))
    d = ImageDraw.Draw(sheet)
    for i, (path, cells) in enumerate(rows):
        y = pad + i * (h + label_h + pad)
        d.text((pad, y), path, fill=(200, 200, 200, 255), font=font)
        for j, (_, im) in enumerate(cells):
            if im is not None:
                sheet.alpha_composite(im, (pad + j * (w + pad), y + label_h))
    sheet.convert("RGB").save(args.out)
    print(args.out)


if __name__ == "__main__":
    main()
