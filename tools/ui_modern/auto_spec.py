"""Theme coverage for everything theme_spec.py doesn't name: every atlas rectangle the UI uses that no hand-written
repaint covers gets a generic modern treatment, so no stock chrome is left behind.

    py -3 client/tools/ui_modern/auto_spec.py            # list what would be painted, by treatment
    (make_theme.py appends auto_repaints() after spec.REPAINTS; hand-written entries always win)

Elements are reassembled first: ImageStyles that are nine-slice parts (nw..se / c, or prefixed like default_c) are
grouped with their siblings, so a frame is treated as one element. Then, from the stock pixels:
  - nine-slice frames, bars and buttons      -> paint.Auto   (flat panel or hairline outline, same opacity intent)
  - single-tone line art, glyphs and glows   -> paint.Crisp  (same silhouette, no halo or bevel shading)
  - multi-tone or coloured pictures, icons   -> left stock   (portraits, logos, ability icons, planet art)
An element that overlaps anything a hand-written entry paints is skipped.
"""
from __future__ import annotations

import re
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import atlas_usage  # noqa: E402
import paint  # noqa: E402

ATLASES = ("ui_rebel_final", "ui_space_hud", "ui_rebel_new_style")
RADIUS = 3

PART_RE = re.compile(r"(?:^|_)(nw|ne|sw|se|n|s|w|e|c)$", re.I)

# names of pictures that stay as they are, whatever the pixels look like
KEEP_RE = re.compile(r"icon|cpyright|copyright|logo|splash|portrait|species|gender|planet|flag|faction|skill|"
                     r"rebel|imperial|survey|reward|emblem|badge|holo|map\b", re.I)


def _element_key(path: str) -> tuple[str, bool]:
    """'Styles.New.foo.default.nw (ui_styles)' -> ('Styles.New.foo.default', True); whole images keep their path."""
    p = path.split(" (")[0]
    head, _, leaf = p.rpartition(".")
    m = PART_RE.search(leaf)
    if head and m:
        # a lone centre slice is a whole picture (icons are stored as 'c'); frames have corners or edges
        return head, m.group(1).lower() != "c"
    return p, False


def _touching_clusters(rects, gap: int = 1):
    """Bounding boxes of groups of rectangles that overlap or lie within `gap` pixels of each other."""
    boxes = [list(r) for r in rects]
    changed = True
    while changed:
        changed = False
        out = []
        while boxes:
            b = boxes.pop()
            i = 0
            while i < len(boxes):
                o = boxes[i]
                if not (o[0] > b[2] + gap or o[2] < b[0] - gap or o[1] > b[3] + gap or o[3] < b[1] - gap):
                    b = [min(b[0], o[0]), min(b[1], o[1]), max(b[2], o[2]), max(b[3], o[3])]
                    boxes.pop(i)
                    changed = True
                else:
                    i += 1
            out.append(b)
        boxes = out
    return [tuple(b) for b in boxes]


def _overlaps(r, boxes):
    return any(not (r[2] <= b[0] or r[0] >= b[2] or r[3] <= b[1] or r[1] >= b[3]) for b in boxes)


def plan(top, hand_boxes: dict[str, list], stock) -> list[tuple[tuple, object, str]]:
    """[((atlas, rect), painter, label)] for the untouched elements. stock(atlas) -> RGBA float array."""
    usage = atlas_usage.scan(set(ATLASES))
    out = []
    for atlas in ATLASES:
        rects = usage.get(atlas, {})
        covered = hand_boxes.get(atlas, [])
        elements: dict[str, list] = defaultdict(list)
        sliced: dict[str, bool] = {}
        names: dict[str, set] = defaultdict(set)
        for r, paths in rects.items():
            if any(b[0] <= r[0] and b[1] <= r[1] and b[2] >= r[2] and b[3] >= r[3] for b in covered):
                continue
            for p in paths:
                key, is_part = _element_key(p)
                elements[key].append(r)
                sliced[key] = sliced.get(key, False) or is_part
                names[key].add(p)
        # an element's parts that touch form one box (a nine-slice set); parts far apart stay separate boxes,
        # then boxes of different elements that share pixels merge (several styles naming the same art)
        boxes = []
        for key, rs in elements.items():
            for box in _touching_clusters(set(rs)):
                boxes.append((box, key))
        merged: list[list] = []
        for box, key in sorted(boxes):
            for m in merged:
                b = m[0]
                if not (box[2] <= b[0] or box[0] >= b[2] or box[3] <= b[1] or box[1] >= b[3]):
                    m[0] = (min(b[0], box[0]), min(b[1], box[1]), max(b[2], box[2]), max(b[3], box[3]))
                    m[1].append(key)
                    break
            else:
                merged.append([box, [key]])
        arr = stock(atlas)
        h, w = arr.shape[:2]
        for box, keys in merged:
            x0, y0, x1, y1 = box
            if x1 - x0 < 2 or y1 - y0 < 2 or x1 > w or y1 > h:
                continue
            if _overlaps(box, covered):
                continue
            label = keys[0]
            if all(KEEP_RE.search(k) for k in keys):
                continue
            kind = paint.classify(arr[y0:y1, x0:x1])
            if kind == "empty":
                continue
            nine = any(sliced.get(k) for k in keys)
            if nine and not all(sliced.get(k) for k in keys):
                continue                   # a frame that borrows another picture's pixels: repainting breaks one
            if nine and min(x1 - x0, y1 - y0) >= 6:
                out.append(((atlas, box), paint.Auto(RADIUS), label))
            elif kind == "mono":
                out.append(((atlas, box), paint.Crisp(), label))
    return out


def main():
    import make_theme
    import coverage
    import uiinc
    top = uiinc.load_tree(make_theme.UI_SRC)
    hand = coverage.painted_boxes(top)
    entries = plan(top, hand, lambda a: paint.read_texture(make_theme.stock_texture_path(a)))
    kinds = defaultdict(int)
    for (atlas, box), painter, label in entries:
        kinds[(atlas, type(painter).__name__)] += 1
        print(f"{atlas:20s} {type(painter).__name__:6s} {box}  {label}")
    for k, n in sorted(kinds.items()):
        print(f"== {k[0]} {k[1]}: {n}")


if __name__ == "__main__":
    main()
