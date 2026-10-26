"""Build the Titan Modern UI theme.

    py -3 client/tools/ui_modern/make_theme.py            # textures + themed UI copy into out/
    py -3 client/tools/ui_modern/make_theme.py --palette  # (re)write the Titan palette into ui_styles.inc (source)
    py -3 client/tools/ui_modern/publish_ui.py            # then copy out/ to the client overlay

Outputs (client/tools/ui_modern/out/):
  texture/<atlas>.dds   stock UI atlases with the chrome repainted (theme_spec.REPAINTS), uncompressed A8R8G8B8
  ui/**                 client/datasources/ui with theme_spec.INC_RULES applied (the source files are not touched)
  preview/<atlas>.png   before | after, for review

Stock atlases are read from data/sku.0/sys.client/compiled/game/texture (the newest copies; the .tre ones are older
subsets), or extracted from the client .tre files with nuna when only those have them.
"""
from __future__ import annotations

import argparse
import colorsys
import re
import shutil
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import paint  # noqa: E402
import theme_spec as spec  # noqa: E402
import uiinc  # noqa: E402

TITAN = Path("D:/titan")
UI_SRC = TITAN / "client/datasources/ui"
OUT = HERE / "out"
STOCK_DIRS = [TITAN / "data/sku.0/sys.client/compiled/game/texture"]
TRES = TITAN / "exe/win64_rel/tres"
NUNA = TITAN / "exe/win32_rel/nuna.exe"
# Atlases that only exist inside the client trees: (tree, path). Highest patch that carries them.
STOCK_IN_TRES = {
    "ui_rebel_new_style": "data_texture_07.tre",
}


# ----------------------------------------------------------------------------------------------------------------------
# Stock textures


def stock_texture_path(atlas: str) -> Path:
    for d in STOCK_DIRS:
        p = d / f"{atlas}.dds"
        if p.exists():
            return p
    cache = OUT / "_stock" / f"{atlas}.dds"
    if cache.exists():
        return cache
    tree = STOCK_IN_TRES.get(atlas)
    if not tree:
        raise FileNotFoundError(f"no stock copy of texture/{atlas}.dds (add it to STOCK_IN_TRES)")
    tmp = OUT / "_stock" / "_x"
    tmp.mkdir(parents=True, exist_ok=True)
    subprocess.run([str(NUNA), "unpack", str(TRES / tree), str(tmp), "-f", f"texture/{atlas}.dds", "-q", "-o"],
                   check=True, capture_output=True)
    shutil.move(str(tmp / "texture" / f"{atlas}.dds"), cache)
    return cache


# ----------------------------------------------------------------------------------------------------------------------
# Style family -> clusters


def family_rects(top: uiinc.Node, path: str):
    """Per atlas, one bounding box per nine-slice set: ImageStyles are grouped by the namespace that holds them
    (default / hover / activated ... each draw one element), so neighbouring elements in the atlas stay apart."""
    node = uiinc.resolve(top, path)
    if node is None:
        raise KeyError(f"style {path} not found")
    sets = defaultdict(list)
    for n in node.walk():
        if n.tag in ("ImageStyle", "Image") and "SourceRect" in n.attrs:
            src = (n.attrs.get("Source") or n.attrs.get("SourceResource") or "").split(":")[-1].lower()
            if not src:
                continue
            x0, y0, x1, y1 = (int(v) for v in n.attrs["SourceRect"].split(","))
            r = (min(x0, x1), min(y0, y1), max(x0, x1), max(y0, y1))
            if r[2] > r[0] and r[3] > r[1]:
                sets[(src, id(n.parent))].append(r)
    out = defaultdict(list)
    for (src, _), rs in sets.items():
        out[src].append((min(r[0] for r in rs), min(r[1] for r in rs), max(r[2] for r in rs), max(r[3] for r in rs)))
    return out


def clusters(rects):
    """Merge element boxes that overlap (sharing pixels, not merely touching) and return the bounding boxes."""
    boxes = [list(r) for r in rects]
    merged = True
    while merged:
        merged = False
        out = []
        while boxes:
            b = boxes.pop()
            i = 0
            while i < len(boxes):
                o = boxes[i]
                if not (o[0] >= b[2] or o[2] <= b[0] or o[1] >= b[3] or o[3] <= b[1]):
                    b = [min(b[0], o[0]), min(b[1], o[1]), max(b[2], o[2]), max(b[3], o[3])]
                    boxes.pop(i)
                    merged = True
                else:
                    i += 1
            out.append(b)
        boxes = out
    return [tuple(b) for b in sorted(boxes)]


# ----------------------------------------------------------------------------------------------------------------------
# Palette


def derive_color(hex_in: str) -> str:
    """Teal family -> azure with less saturation; greys get a faint slate cast; other hues keep their identity."""
    s = hex_in.lstrip("#")
    if len(s) != 6:
        return hex_in
    r, g, b = (int(s[i:i + 2], 16) / 255 for i in (0, 2, 4))
    h, l, sat = colorsys.rgb_to_hls(r, g, b)
    deg = h * 360
    if sat < 0.12 or (max(r, g, b) - min(r, g, b)) < 0.06:
        h, sat = 214 / 360, min(0.16, sat + 0.06)
    elif 140 <= deg <= 215:
        h, sat = (207 + (deg - 177) * 0.15) / 360, sat * 0.72
    else:
        sat *= 0.9
    r, g, b = colorsys.hls_to_rgb(h, l, sat)
    return "#%02X%02X%02X" % (round(r * 255), round(g * 255), round(b * 255))


def is_teal(hex_in: str) -> bool:
    """Saturated teal/cyan of the stock theme (140-200 degrees); derive_color's results land at 201+."""
    s = hex_in.lstrip("#")
    if len(s) != 6:
        return False
    try:
        r, g, b = (int(s[i:i + 2], 16) / 255 for i in (0, 2, 4))
    except ValueError:
        return False
    h, l, sat = colorsys.rgb_to_hls(r, g, b)
    return sat > 0.25 and 140 <= h * 360 < 200


def build_palette(top: uiinc.Node) -> dict[str, str]:
    pal_ns = uiinc.resolve(top, "/Styles.palette")
    base = next(p for p in pal_ns.children if p.tag == "Palette" and p.name == spec.PALETTE_BASE)
    out = {}
    for k, v in base.attrs.items():
        if k == "Name":
            continue
        out[k] = derive_color(v) if v.startswith("#") else v
    lower = {k.lower(): k for k in out}
    for k, v in spec.PALETTE_OVERRIDES.items():
        out[lower.get(k.lower(), k)] = v
    return out


def write_palette_source(top: uiinc.Node):
    """Insert or replace <Palette Name='Titan'> in the source ui_styles.inc, next to the stock palettes."""
    pal = build_palette(top)
    styles = UI_SRC / "ui_styles.inc"
    text = uiinc.read_raw(styles)
    nl = "\r\n" if "\r\n" in text[:4096] else "\n"
    attrs = sorted(list(pal.items()) + [("Name", spec.PALETTE_NAME)], key=lambda kv: kv[0].lower())
    indent = "\t\t\t"
    block = indent + "<Palette" + nl + "".join(f"{indent}\t{k}='{v}'{nl}" for k, v in attrs) + indent + "/>" + nl
    # Plain string search: a regex over the 1.7 MB file backtracks catastrophically.
    name_at = text.find(f"\tName='{spec.PALETTE_NAME}'{nl}")
    start = text.rfind("<Palette" + nl, 0, name_at) if name_at >= 0 else -1
    if start >= 0 and "/>" not in text[start:name_at]:
        start = text.rfind("\n", 0, start) + 1
        end = text.find("/>" + nl, name_at) + 2 + len(nl)
        text = text[:start] + block + text[end:]
    else:
        anchor = f"\t\t\tName='palette'{nl}\t\t>{nl}"
        at = text.find(anchor)
        if at < 0:
            raise RuntimeError("palette namespace not found in ui_styles.inc")
        text = text[:at + len(anchor)] + block + text[at + len(anchor):]
    uiinc.write_raw(styles, text)
    print(f"wrote palette {spec.PALETTE_NAME} ({len(pal)} colours) into {styles}")


# ----------------------------------------------------------------------------------------------------------------------
# Build


def build_textures(top: uiinc.Node):
    atlases: dict[str, np.ndarray] = {}
    originals: dict[str, np.ndarray] = {}
    painted: dict[str, list] = defaultdict(list)

    def atlas(name):
        if name not in atlases:
            arr = paint.read_texture(stock_texture_path(name))
            atlases[name] = arr.copy()
            originals[name] = arr
        return atlases[name]

    # everything the hand-written spec doesn't cover gets the generic treatment (auto_spec.py), painted after it
    import auto_spec
    import coverage
    def stock(name):
        atlas(name)
        return originals[name]

    auto = [(t, p) for t, p, _ in auto_spec.plan(top, coverage.painted_boxes(top), stock)]

    for entry in list(spec.REPAINTS) + auto:
        target, painter = entry[0], entry[1]
        if isinstance(target, tuple):        # (atlas, rect)
            groups = {target[0]: [target[1]]}
        else:
            groups = {a: clusters(rs) for a, rs in family_rects(top, target).items()}
        for name, boxes in groups.items():
            if name == spec.MODERN_ATLAS:      # already retargeted onto the theme atlas
                continue
            arr = atlas(name)
            for (x0, y0, x1, y1) in boxes:
                # An element inside one an earlier entry already painted is the same art seen through a
                # smaller window (tree_default uses 457..481 of the 457..482 hud button): painting it again at
                # its own size would leave the bigger element with a doubled rim. First entry wins.
                inside = [o for o in painted[name]
                          if o[0][0] <= x0 and o[0][1] <= y0 and o[0][2] >= x1 and o[0][3] >= y1]
                if inside:
                    continue
                for other in painted[name]:
                    if not (x1 <= other[0][0] or x0 >= other[0][2] or y1 <= other[0][1] or y0 >= other[0][3]):
                        print(f"  note: {target} {name}{(x0, y0, x1, y1)} overlaps {other[1]} {other[0]}")
                if getattr(painter, "needs_source", False):
                    arr[y0:y1, x0:x1] = painter(x1 - x0, y1 - y0, originals[name][y0:y1, x0:x1])
                else:
                    arr[y0:y1, x0:x1] = painter(x1 - x0, y1 - y0)
                painted[name].append(((x0, y0, x1, y1), target))
        print(f"painted {target}: " + ", ".join(f"{a} x{len(b)}" for a, b in groups.items()))

    (OUT / "texture").mkdir(parents=True, exist_ok=True)
    (OUT / "preview").mkdir(parents=True, exist_ok=True)
    for name, arr in atlases.items():
        paint.write_dds_argb8888(OUT / "texture" / f"{name}.dds", arr)
        both = np.concatenate([originals[name], np.zeros((arr.shape[0], 8, 4), np.float32), arr], axis=1)
        paint.to_png(both, OUT / "preview" / f"{name}.png")
        print(f"wrote texture/{name}.dds and preview/{name}.png")


PARTS = {"nw", "n", "ne", "w", "c", "e", "sw", "s", "se"}


def slice_rects(ex: int, ey: int, w: int, h: int, caps, parts: set[str]):
    """Source rectangles of each nine-slice part for an element drawn at (ex, ey, w, h) with caps (l, t, r, b)."""
    l, t, r, b = caps
    horizontal = not (parts & {"n", "s", "nw", "ne", "sw", "se"})     # w / c / e only
    vertical = not (parts & {"w", "e", "nw", "ne", "sw", "se"})       # n / c / s only
    xs = (ex, ex + l, ex + w - r, ex + w)
    ys = (ey, ey + t, ey + h - b, ey + h)
    if horizontal:
        ys = (ey, ey, ey + h, ey + h)
    if vertical:
        xs = (ex, ex, ex + w, ex + w)
    span = {"w": (0, 1), "n": (0, 1), "c": (1, 2), "e": (2, 3), "s": (2, 3)}
    out = {}
    for p in PARTS:
        col = "w" if p in ("nw", "w", "sw") else ("e" if p in ("ne", "e", "se") else "c")
        row = "n" if p in ("nw", "n", "ne") else ("s" if p in ("sw", "s", "se") else "c")
        if horizontal:
            row = "c"       # ys collapses to (ey, ey, ey+h, ey+h): the middle span is the full height
        if vertical:
            col = "c"
        x0, x1 = xs[span[col][0]], xs[span[col][1]]
        y0, y1 = ys[span[row][0]], ys[span[row][1]]
        out[p] = (x0, y0, max(x1, x0 + 1), max(y1, y0 + 1))
    return out


def part_name(node: uiinc.Node) -> str:
    """ImageStyle names are 'nw', 'c', or prefixed like 'default_c'; anything else is a whole image."""
    n = node.name.lower().rsplit("_", 1)[-1]
    return n if n in PARTS else ""


def build_modern_atlas(top: uiinc.Node, rw: uiinc.Rewriter):
    """Pack the RETARGETS art into ui_titan_modern.dds and point their ImageStyles at it."""
    size = spec.MODERN_ATLAS_SIZE
    arr = np.zeros((size, size, 4), np.float32)
    x = y = shelf = 2
    placed: dict[str, tuple[int, int]] = {}
    follow: dict = {"_done": set()}
    count = 0
    for rt in spec.RETARGETS:
        w, h = rt.size
        key = f"{rt.painter!r}|{w}x{h}"
        if key not in placed:
            if x + w + 2 > size:
                x, y, shelf = 2, y + shelf + 2, 0
            if y + h + 2 > size:
                raise RuntimeError("ui_titan_modern atlas is full")
            arr[y:y + h, x:x + w] = rt.painter(w, h)
            placed[key] = (x, y)
            x += w + 3
            shelf = max(shelf, h)
        ex, ey = placed[key]
        for path in rt.paths:
            node = uiinc.resolve(top, path)
            if node is None:
                if not rt.optional:
                    print(f"  retarget: {path} not found")
                continue
            images = [node] if node.tag == "ImageStyle" else [c for c in node.children if c.tag == "ImageStyle"]
            parts = {part_name(i) for i in images} - {""}
            rects = slice_rects(ex, ey, w, h, rt.caps, parts or {"c"})
            for img in images:
                p = part_name(img) or "whole"
                r = (ex, ey, ex + w, ey + h) if p == "whole" or not parts else rects[p]
                if retarget_image(img, r, rw, follow):
                    count += 1

    # Pages carry private copies of style definitions (Inventory.comp.vHud4, Inventory.comp.hud ...): any image
    # that names the same stock rectangle for the same slice part follows its retargeted original.
    copies = 0
    for n in top.walk():
        if n.tag not in ("ImageStyle", "Image") or "SourceRect" not in n.attrs:
            continue
        key = image_key(n)
        if key and key in follow and id(n) not in follow["_done"]:
            if retarget_image(n, follow[key], rw, None):
                copies += 1
    paint.write_dds_argb8888(OUT / "texture" / f"{spec.MODERN_ATLAS}.dds", arr)
    paint.to_png(arr, OUT / "preview" / f"{spec.MODERN_ATLAS}.png")
    print(f"retargeted {count} images onto {spec.MODERN_ATLAS} ({len(placed)} elements), {copies} page copies followed")


def stock_source(img: uiinc.Node):
    """(atlas, x0, y0, x1, y1) the image showed in the stock UI: its ThemeFrom record once retargeted."""
    if "ThemeFrom" in img.attrs:
        atlas, rect = img.attrs["ThemeFrom"].split(":")
    else:
        attr = "Source" if "Source" in img.attrs else "SourceResource"
        atlas, rect = img.attrs.get(attr, "").split(":")[-1], img.attrs.get("SourceRect", "")
    try:
        x0, y0, x1, y1 = (int(v) for v in rect.split(","))
    except ValueError:
        return None
    return atlas.lower(), x0, y0, x1, y1


def image_key(img: uiinc.Node):
    """(atlas, normalised stock rectangle, slice part) of an image, from its stock source."""
    src = stock_source(img)
    if not src or not src[0]:
        return None
    atlas, x0, y0, x1, y1 = src
    return atlas, min(x0, x1), min(y0, y1), max(x0, x1), max(y0, y1), part_name(img)


def retarget_image(img: uiinc.Node, r, rw: uiinc.Rewriter, follow) -> bool:
    """Point one image at rectangle r of the theme atlas, keeping a mirrored stock rectangle mirrored."""
    if "SourceRect" not in img.attrs:
        return False                         # a stock definition with no rectangle (vHud2.thumb.default.n)
    attr = "Source" if "Source" in img.attrs else "SourceResource"
    if attr not in img.attrs:
        return False
    stock = stock_source(img)
    if stock is None:
        return False
    if follow is not None:
        key = image_key(img)
        if key:
            follow.setdefault(key, r)
        follow.setdefault("_done", set()).add(id(img))
    atlas, sx0, sy0, sx1, sy1 = stock
    x0, y0, x1, y1 = r
    if sx1 < sx0:                            # keep mirrored parts mirrored (scrollbar start = flipped end)
        x0, x1 = x1, x0
    if sy1 < sy0:
        y0, y1 = y1, y0
    if "ThemeFrom" not in img.attrs:         # remember the stock image so later runs (and --revert) can find it
        rw.add(img, "SourceRect", "ThemeFrom", f"{atlas}:{sx0},{sy0},{sx1},{sy1}")
    rw.set(img, attr, spec.MODERN_ATLAS)
    rw.set(img, "SourceRect", f"{x0},{y0},{x1},{y1}")
    return True


def apply_source_rules(top: uiinc.Node):
    """Theme edits to client/datasources/ui itself (the live UI mirrors it): page rules, attribute overrides and
    the retargets. Re-running produces the same values, so it is idempotent."""
    rw = uiinc.Rewriter()
    hits = defaultdict(int)
    for n in top.walk():
        for i, (pred, attr, value) in enumerate(spec.INC_RULES):
            if not pred(n):
                continue
            if callable(value):                         # a value computed from the node itself
                value = value(n)
            if value is None or n.attrs.get(attr) == value:
                continue
            if attr in n.attrs:
                rw.set(n, attr, value)
            elif "Name" in n.attrs:
                rw.add(n, "Name", attr, value)          # e.g. Visible='false' on an object that had no Visible
            else:
                continue
            hits[i] += 1
    remapped = 0
    for n in top.walk():
        for attr, pal in spec.COLOR_REMAP.items():
            v = n.attrs.get(attr)
            if v and (pal is None or pal not in n.attrs) and is_teal(v):
                rw.set(n, attr, derive_color(v))
                remapped += 1
    print(f"hard-coded teal colours remapped: {remapped}")
    for path, attr, value in spec.ATTRS:
        node = uiinc.resolve(top, path)
        if node is None or attr not in node.attrs:
            print(f"  attr: {path}.{attr} not found")
            continue
        rw.set(node, attr, value)
    build_modern_atlas(top, rw)
    changed = []
    for f, edits in rw.edits.items():
        text = uiinc.read_raw(f)
        new = uiinc.Rewriter.splice(text, edits)
        if new != text:
            uiinc.write_raw(f, new)
            changed.append(f.name)
    for i, n in sorted(hits.items()):
        print(f"inc rule {i}: {n} objects")
    print("source files changed: " + (", ".join(sorted(changed)) or "none"))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--palette-only", action="store_true", help="only (re)write the Titan palette")
    args = ap.parse_args()
    write_palette_source(uiinc.load_tree(UI_SRC))
    if args.palette_only:
        return
    top = uiinc.load_tree(UI_SRC)
    build_textures(top)
    apply_source_rules(top)


if __name__ == "__main__":
    main()
