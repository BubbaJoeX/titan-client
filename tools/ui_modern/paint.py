"""Drawing primitives for the Titan Modern UI theme: anti-aliased rounded panels, outlines, bars, rings and glyphs
painted straight into regions of the stock UI atlases, plus DDS read/write.

Everything is greyscale + alpha on purpose. The UI multiplies each image by a palette colour (PalColor / PalBgTint),
so art supplies shape and luminance and the palette supplies colour; every palette the player can pick keeps working.

Nine-slice rule: RectangleStyles stretch their edge and centre slices, so whatever is painted must be constant
along the stretched axis across slice boundaries. Panels therefore keep a uniform fill and put title bands,
highlights and shadows only inside the top / bottom corner slices (see Region.slices).
"""
from __future__ import annotations

import struct
from dataclasses import dataclass
from pathlib import Path

import numpy as np
from PIL import Image

# ----------------------------------------------------------------------------------------------------------------------
# DDS


def read_texture(path: Path) -> np.ndarray:
    """RGBA float array (h, w, 4) in 0..1."""
    return np.asarray(Image.open(path).convert("RGBA"), dtype=np.float32) / 255.0


def write_dds_argb8888(path: Path, rgba: np.ndarray):
    """Uncompressed A8R8G8B8 (DDS_RGB | DDS_ALPHA), no mipmaps - the format Texture.cpp maps to TF_ARGB_8888.
    Uncompressed keeps 1px borders and glyph edges crisp; DXT5 would smear them into 4x4 blocks."""
    h, w = rgba.shape[:2]
    px = np.clip(np.rint(rgba * 255.0), 0, 255).astype(np.uint8)
    bgra = px[:, :, [2, 1, 0, 3]].tobytes()
    header = struct.pack(
        "<4sIIIIIII44sIIIIIIIIIIIII",
        b"DDS ", 124, 0x1 | 0x2 | 0x4 | 0x8 | 0x1000, h, w, w * 4, 0, 0, b"\0" * 44,
        32, 0x41, 0, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000,
        0x1000, 0, 0, 0, 0)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(header + bgra)


def to_png(rgba: np.ndarray, path: Path, bg=(0.11, 0.12, 0.14)):
    """Preview on a dark checker (premultiplied over the background)."""
    h, w = rgba.shape[:2]
    yy, xx = np.mgrid[0:h, 0:w]
    checker = ((xx // 8 + yy // 8) % 2)[..., None] * 0.04
    base = np.array(bg, dtype=np.float32)[None, None, :] + checker
    a = rgba[..., 3:4]
    out = rgba[..., :3] * a + base * (1 - a)
    Image.fromarray(np.clip(out * 255, 0, 255).astype(np.uint8)).save(path)


# ----------------------------------------------------------------------------------------------------------------------
# Signed distance helpers (pixel centres, 1px anti-aliasing)


def _grid(w: int, h: int):
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    return xx + 0.5, yy + 0.5


def sd_round_rect(w: int, h: int, x0: float, y0: float, x1: float, y1: float, r: float,
                  corners=(True, True, True, True)):
    """Signed distance to the rounded rectangle [x0,x1]x[y0,y1] with corner radius r (negative inside).
    corners = which of (top-left, top-right, bottom-right, bottom-left) are rounded; the others stay square."""
    px, py = _grid(w, h)
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    if not all(corners):
        tl, tr, br, bl = corners
        left, top = px < cx, py < cy
        rounded = np.where(top, np.where(left, tl, tr), np.where(left, bl, br))
        r = np.where(rounded, r, 0.0).astype(np.float32)
    hx, hy = (x1 - x0) / 2 - r, (y1 - y0) / 2 - r
    qx, qy = np.abs(px - cx) - hx, np.abs(py - cy) - hy
    outside = np.hypot(np.maximum(qx, 0), np.maximum(qy, 0))
    inside = np.minimum(np.maximum(qx, qy), 0)
    return outside + inside - r


def coverage(d, offset: float = 0.0):
    return np.clip(0.5 - (d + offset), 0.0, 1.0)


# ----------------------------------------------------------------------------------------------------------------------
# Layers: (lum, alpha) pairs composited "over"


class Layer:
    def __init__(self, w: int, h: int):
        self.w, self.h = w, h
        self.lum = np.zeros((h, w), np.float32)
        self.a = np.zeros((h, w), np.float32)

    def over(self, lum, alpha):
        """Composite a (lum, alpha) layer over this one. lum/alpha are scalars or (h, w) arrays."""
        lum = np.broadcast_to(np.asarray(lum, np.float32), (self.h, self.w))
        alpha = np.broadcast_to(np.asarray(alpha, np.float32), (self.h, self.w))
        a_out = alpha + self.a * (1 - alpha)
        safe = np.where(a_out > 1e-6, a_out, 1)
        self.lum = np.where(a_out > 1e-6, (lum * alpha + self.lum * self.a * (1 - alpha)) / safe, 0)
        self.a = a_out

    def rgba(self) -> np.ndarray:
        out = np.zeros((self.h, self.w, 4), np.float32)
        out[..., 0] = out[..., 1] = out[..., 2] = self.lum
        out[..., 3] = self.a
        return out


# ----------------------------------------------------------------------------------------------------------------------
# Region painters. Each returns an RGBA float array for a w x h region.


@dataclass
class Panel:
    """Rounded panel: uniform fill, 1px border, optional title band / top highlight / bottom shade.
    Luminances are multiplied by the palette colour in game, so fill < border gives a lighter rim."""
    radius: float = 3
    fill: float = 0.45
    fill_a: float = 1.0
    border: float = 1.0
    border_a: float = 1.0
    border_w: float = 1.0
    title_h: int = 0            # title band height (inside the top slice)
    title: float = 0.62
    title_a: float = 1.0
    title_rule: float = 0.0     # luminance of a 1px rule under the title band (0 = none)
    highlight: float = 0.0      # 1px highlight inside the top border (alpha)
    shade: float = 0.0          # darken the bottom 2px (alpha of black)
    inset: float = 0.0          # shrink the shape inside its region (px)
    round_corners: tuple = (True, True, True, True)   # top-left, top-right, bottom-right, bottom-left
    bottom_rule: float = 0.0    # luminance of a 1px rule along the bottom edge (0 = none)

    def __call__(self, w: int, h: int, slices=None) -> np.ndarray:
        L = Layer(w, h)
        i = self.inset
        d = sd_round_rect(w, h, i, i, w - i, h - i, self.radius, self.round_corners)
        fill_cov = coverage(d, self.border_w) if self.border_a > 0 else coverage(d)
        L.over(self.fill, self.fill_a * fill_cov)
        if self.title_h:
            _, py = _grid(w, h)
            band = (py < i + self.title_h).astype(np.float32) * fill_cov
            L.over(self.title, self.title_a * band)
            if self.title_rule:
                rule = ((py >= i + self.title_h) & (py < i + self.title_h + 1)).astype(np.float32) * fill_cov
                L.over(self.title_rule, rule)
        if self.highlight:
            inner = coverage(d, self.border_w) - coverage(d, self.border_w + 1)
            _, py = _grid(w, h)
            top = (py < i + self.border_w + self.radius + 1).astype(np.float32)
            L.over(1.0, self.highlight * inner * top)
        if self.shade:
            _, py = _grid(w, h)
            bottom = (py > h - i - self.border_w - 2).astype(np.float32) * fill_cov
            L.over(0.0, self.shade * bottom)
        if self.border_a > 0:
            ring = coverage(d) - coverage(d, self.border_w)
            L.over(self.border, self.border_a * ring)
        if self.bottom_rule:
            _, py = _grid(w, h)
            rule = (py > h - i - 1).astype(np.float32) * coverage(d)
            L.over(self.bottom_rule, rule)
        return L.rgba()


@dataclass
class Outline:
    radius: float = 3
    lum: float = 1.0
    alpha: float = 1.0
    width: float = 1.0
    inset: float = 0.0
    fill_a: float = 0.0         # optional faint fill
    fill: float = 1.0

    def __call__(self, w: int, h: int, slices=None) -> np.ndarray:
        L = Layer(w, h)
        i = self.inset
        d = sd_round_rect(w, h, i, i, w - i, h - i, self.radius)
        if self.fill_a:
            L.over(self.fill, self.fill_a * coverage(d, self.width))
        L.over(self.lum, self.alpha * (coverage(d) - coverage(d, self.width)))
        return L.rgba()


@dataclass
class Solid:
    lum: float = 1.0
    alpha: float = 1.0

    def __call__(self, w: int, h: int, slices=None) -> np.ndarray:
        L = Layer(w, h)
        L.over(self.lum, self.alpha)
        return L.rgba()


@dataclass
class Clear:
    def __call__(self, w: int, h: int, slices=None) -> np.ndarray:
        return np.zeros((h, w, 4), np.float32)


@dataclass
class HLine:
    """A horizontal rule `thick` px high centred vertically (or at `y`), optionally fading toward one end."""
    lum: float = 1.0
    alpha: float = 1.0
    thick: float = 1.0
    y: float | None = None
    fade: str = ""              # "", "right", "left"

    def __call__(self, w: int, h: int, slices=None) -> np.ndarray:
        L = Layer(w, h)
        px, py = _grid(w, h)
        yc = h / 2 if self.y is None else self.y + self.thick / 2
        cov = np.clip(self.thick / 2 + 0.5 - np.abs(py - yc), 0, 1)
        if self.fade == "right":
            cov = cov * np.clip(1 - (px / w), 0, 1)
        elif self.fade == "left":
            cov = cov * np.clip(px / w, 0, 1)
        L.over(self.lum, self.alpha * cov)
        return L.rgba()


@dataclass
class Ring:
    lum: float = 1.0
    alpha: float = 1.0
    width: float = 1.5
    inset: float = 0.5
    fill_a: float = 0.0
    fill: float = 0.3

    def __call__(self, w: int, h: int, slices=None) -> np.ndarray:
        L = Layer(w, h)
        px, py = _grid(w, h)
        cx, cy = w / 2, h / 2
        rx, ry = w / 2 - self.inset, h / 2 - self.inset
        # ellipse distance (approximate, fine for near-circles)
        k = np.hypot((px - cx) / rx, (py - cy) / ry)
        d = (k - 1) * min(rx, ry)
        if self.fill_a:
            L.over(self.fill, self.fill_a * coverage(d, self.width))
        L.over(self.lum, self.alpha * (coverage(d) - coverage(d, self.width)))
        return L.rgba()


@dataclass
class Glyph:
    """Vector glyphs on a transparent background, drawn as thick anti-aliased strokes."""
    kind: str                   # check, x, minus, plus, chevron_up/down/left/right, triangle_up/down/left/right, dot
    lum: float = 1.0
    alpha: float = 1.0
    stroke: float = 1.6
    pad: float = 3.0
    box: Panel | Outline | None = None   # optional background shape

    def __call__(self, w: int, h: int, slices=None) -> np.ndarray:
        base = self.box(w, h) if self.box else np.zeros((h, w, 4), np.float32)
        L = Layer(w, h)
        L.lum, L.a = base[..., 0].copy(), base[..., 3].copy()
        px, py = _grid(w, h)
        p = self.pad
        x0, y0, x1, y1 = p, p, w - p, h - p
        cx, cy = w / 2, h / 2
        s = min(x1 - x0, y1 - y0)

        def seg(ax, ay, bx, by):
            vx, vy = bx - ax, by - ay
            t = np.clip(((px - ax) * vx + (py - ay) * vy) / max(vx * vx + vy * vy, 1e-6), 0, 1)
            return np.hypot(px - (ax + t * vx), py - (ay + t * vy)) - self.stroke / 2

        def poly(pts):
            # filled polygon via even-odd crossing + AA from edge distance
            inside = np.zeros_like(px, bool)
            n = len(pts)
            for i in range(n):
                (ax, ay), (bx, by) = pts[i], pts[(i + 1) % n]
                cond = ((ay > py) != (by > py)) & (px < (bx - ax) * (py - ay) / (by - ay + 1e-9) + ax)
                inside ^= cond
            dist = np.min([seg(*pts[i], *pts[(i + 1) % n]) + self.stroke / 2 for i in range(n)], axis=0)
            return np.where(inside, -dist, dist)

        k = self.kind
        if k == "check":
            d = np.minimum(seg(cx - s * 0.36, cy + s * 0.02, cx - s * 0.10, cy + s * 0.28),
                           seg(cx - s * 0.10, cy + s * 0.28, cx + s * 0.38, cy - s * 0.26))
        elif k == "x":
            r = s * 0.34
            d = np.minimum(seg(cx - r, cy - r, cx + r, cy + r), seg(cx - r, cy + r, cx + r, cy - r))
        elif k == "minus":
            d = seg(cx - s * 0.32, cy, cx + s * 0.32, cy)
        elif k == "plus":
            d = np.minimum(seg(cx - s * 0.32, cy, cx + s * 0.32, cy), seg(cx, cy - s * 0.32, cx, cy + s * 0.32))
        elif k.startswith("chevron_"):
            r = s * 0.22
            pts = {"up": [(cx - 2 * r, cy + r), (cx, cy - r), (cx + 2 * r, cy + r)],
                   "down": [(cx - 2 * r, cy - r), (cx, cy + r), (cx + 2 * r, cy - r)],
                   "left": [(cx + r, cy - 2 * r), (cx - r, cy), (cx + r, cy + 2 * r)],
                   "right": [(cx - r, cy - 2 * r), (cx + r, cy), (cx - r, cy + 2 * r)]}[k[8:]]
            d = np.minimum(seg(*pts[0], *pts[1]), seg(*pts[1], *pts[2]))
        elif k.startswith("triangle_"):
            r = s * 0.30
            pts = {"up": [(cx - r, cy + r * 0.6), (cx, cy - r * 0.7), (cx + r, cy + r * 0.6)],
                   "down": [(cx - r, cy - r * 0.6), (cx + r, cy - r * 0.6), (cx, cy + r * 0.7)],
                   "left": [(cx + r * 0.6, cy - r), (cx + r * 0.6, cy + r), (cx - r * 0.7, cy)],
                   "right": [(cx - r * 0.6, cy - r), (cx + r * 0.7, cy), (cx - r * 0.6, cy + r)]}[k[9:]]
            d = poly(pts) - 0.0
        elif k == "dot":
            d = np.hypot(px - cx, py - cy) - s * 0.22
        else:
            raise ValueError(k)
        L.over(self.lum, self.alpha * coverage(d))
        return L.rgba()


@dataclass
class Bar:
    """Status/progress bar fill: flat with a soft top sheen, rounded ends, optional glow halo."""
    radius: float = 2
    lum: float = 1.0
    alpha: float = 1.0
    sheen: float = 0.18
    glow: float = 0.0
    inset: float = 0.0

    def __call__(self, w: int, h: int, slices=None) -> np.ndarray:
        L = Layer(w, h)
        i = self.inset
        d = sd_round_rect(w, h, i, i, w - i, h - i, self.radius)
        if self.glow:
            L.over(self.lum, self.glow * np.clip(1 - np.maximum(d, 0) / max(i, 1), 0, 1) * (d > 0))
        L.over(self.lum, self.alpha * coverage(d))
        if self.sheen:
            _, py = _grid(w, h)
            top = np.clip(1 - (py - i) / max((h - 2 * i) * 0.5, 1), 0, 1)
            L.over(1.0, self.sheen * top * coverage(d, 1))
        return L.rgba()


@dataclass
class Track:
    """A rounded line `thick` px across, centred in the region, horizontal or vertical (slider / scrollbar tracks)."""
    thick: float = 2.0
    lum: float = 1.0
    alpha: float = 1.0
    vertical: bool = False

    def __call__(self, w: int, h: int, slices=None) -> np.ndarray:
        L = Layer(w, h)
        t = self.thick
        if self.vertical:
            d = sd_round_rect(w, h, w / 2 - t / 2, 0, w / 2 + t / 2, h, t / 2)
        else:
            d = sd_round_rect(w, h, 0, h / 2 - t / 2, w, h / 2 + t / 2, t / 2)
        L.over(self.lum, self.alpha * coverage(d))
        return L.rgba()


@dataclass
class Colored:
    """Give a greyscale painter a fixed colour (for art that is drawn untinted, like checkbox images)."""
    painter: object
    color: str = "#FFFFFF"

    def __call__(self, w: int, h: int, slices=None) -> np.ndarray:
        out = self.painter(w, h)
        c = self.color.lstrip("#")
        rgb = np.array([int(c[i:i + 2], 16) / 255 for i in (0, 2, 4)], np.float32)
        out[..., :3] = out[..., :3] * rgb
        return out


@dataclass
class Box:
    """Paint `painter` into the sub-rectangle (x0, y0, x1, y1) of the region; the rest stays transparent."""
    painter: object
    x0: float = 0
    y0: float = 0
    x1: float = 0
    y1: float = 0

    def __call__(self, w: int, h: int, slices=None) -> np.ndarray:
        out = np.zeros((h, w, 4), np.float32)
        ix0, iy0, ix1, iy1 = int(self.x0), int(self.y0), int(np.ceil(self.x1)), int(np.ceil(self.y1))
        out[iy0:iy1, ix0:ix1] = self.painter(ix1 - ix0, iy1 - iy0)
        return out


class Stack:
    """Composite painters bottom to top (premultiplied 'over')."""

    def __init__(self, *painters):
        self.painters = painters

    def __repr__(self):
        return "Stack(" + ", ".join(map(repr, self.painters)) + ")"

    def __call__(self, w: int, h: int, slices=None) -> np.ndarray:
        out = np.zeros((h, w, 4), np.float32)
        for p in self.painters:
            top = p(w, h)
            a_t, a_b = top[..., 3:4], out[..., 3:4]
            a = a_t + a_b * (1 - a_t)
            rgb = np.where(a > 1e-6, (top[..., :3] * a_t + out[..., :3] * a_b * (1 - a_t)) / np.maximum(a, 1e-6), 0)
            out = np.concatenate([rgb, a], axis=2)
        return out


@dataclass
class Auto:
    """flatten_auto as a painter: the builder hands it the stock pixels of the element (needs_source)."""
    radius: float = 3
    needs_source = True

    def __call__(self, w: int, h: int, source: np.ndarray | None = None) -> np.ndarray:
        return flatten_auto(source, self.radius)


def _corner_radius(a: np.ndarray, limit: float) -> float:
    """Corner radius of the stock silhouette, from how far along the diagonals the alpha stays empty (a rounded
    corner of radius r leaves r * (1 - 1/sqrt 2) empty pixels on the diagonal); round buttons stay round."""
    h, w = a.shape
    peak = float(a.max())
    if peak <= 0:
        return 0.0
    steps = []
    for ys, xs in ((1, 1), (1, -1), (-1, 1), (-1, -1)):
        y, x, n = (0 if ys > 0 else h - 1), (0 if xs > 0 else w - 1), 0
        while 0 <= y < h and 0 <= x < w and a[y, x] < 0.5 * peak and n < min(w, h) // 2:
            y, x, n = y + ys, x + xs, n + 1
        steps.append(n)
    d = sorted(steps)[1]                   # second smallest: ignore one odd corner
    return float(min(limit, d / 0.293)) if d > 1 else 0.0


def flatten_auto(region: np.ndarray, radius: float = 3) -> np.ndarray:
    """Redraw a stock element as a clean flat shape that keeps its opacity and outline-vs-fill intent:
    the rim band and the interior are measured separately; a near-empty interior becomes an Outline."""
    h, w = region.shape[:2]
    a = region[..., 3]
    lum = region[..., :3].mean(axis=2)
    radius = max(radius, _corner_radius(a, min(w, h) / 2.0))
    band = max(1, min(w, h) // 6)
    rim = np.zeros_like(a, bool)
    rim[:band, :] = rim[-band:, :] = True
    rim[:, :band] = rim[:, -band:] = True
    inner = ~rim
    ra = float(np.percentile(a[rim], 90)) if rim.any() else 0
    ia = float(np.median(a[inner])) if inner.any() else 0
    if ra < 0.05 and ia < 0.05:
        return region
    if ia < 0.35 * ra:
        rl = float(np.median(lum[rim & (a > ra * 0.5)])) if (rim & (a > ra * 0.5)).any() else 1.0
        return Outline(radius=radius, lum=max(rl, 0.6), alpha=min(1.0, ra), fill_a=ia, fill=0.5)(w, h)
    il = float(np.median(lum[inner & (a > 0.05)])) if (inner & (a > 0.05)).any() else 0.5
    rl = float(np.percentile(lum[rim & (a > 0.05)], 90)) if (rim & (a > 0.05)).any() else il
    return Panel(radius=radius, fill=il, fill_a=min(1.0, ia), border=max(rl, il), border_a=min(1.0, ra))(w, h)


@dataclass
class Crisp:
    """Modernise stock line art and glyphs in place (needs_source): keep the silhouette, drop the soft glow halo and
    the bevel shading. Alpha is re-thresholded relative to the element's own peak, so a glow fades out where the
    stock art was only haze and a stroke gets a clean anti-aliased edge; luminance becomes one flat value (the
    palette tints it), so gradients and highlights read as flat line art."""
    lum: float | None = None        # None: keep the element's own (median) brightness
    lo: float = 0.30                # fraction of the peak alpha where the new edge starts
    hi: float = 0.60                # ... and where it is fully opaque
    needs_source = True

    def __call__(self, w: int, h: int, source: np.ndarray | None = None) -> np.ndarray:
        if source is None:
            return np.zeros((h, w, 4), np.float32)
        a = source[..., 3]
        peak = float(a.max())
        if peak < 0.05:
            return source
        t = np.clip((a / peak - self.lo) / max(1e-3, self.hi - self.lo), 0.0, 1.0)
        t = t * t * (3.0 - 2.0 * t)
        lum_src = source[..., :3].mean(axis=2)
        solid = a > 0.5 * peak
        flat = self.lum if self.lum is not None else float(np.median(lum_src[solid])) if solid.any() else 1.0
        out = np.zeros_like(source)
        out[..., 0] = out[..., 1] = out[..., 2] = max(flat, 0.55)
        out[..., 3] = t * min(1.0, peak)
        return out


def classify(region: np.ndarray) -> str:
    """'empty', 'mono' (single-tone line art, glow or glyph: safe to Crisp) or 'art' (multi-tone image: leave)."""
    a = region[..., 3]
    if a.max() < 0.05:
        return "empty"
    lum = region[..., :3].mean(axis=2)
    sat = region[..., :3].max(axis=2) - region[..., :3].min(axis=2)
    solid = a > 0.4 * a.max()
    if not solid.any():
        return "mono"
    if float(np.median(sat[solid])) > 0.12:
        return "art"                 # coloured: a picture, not palette-tinted chrome
    return "mono" if float(np.std(lum[solid])) < 0.16 else "art"
