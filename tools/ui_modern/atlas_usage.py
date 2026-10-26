"""Map which parts of the UI texture atlases the .inc pages use, and what for.

    py -3 client/tools/ui_modern/atlas_usage.py [atlas ...] [--png out_dir] [--json out.json]

For every element that names a Source (canvas = texture/<name>.dds, optionally 'shader:texture') and a SourceRect, it
records the rectangle, the element path (namespace/page names) and how often it is used. With --png it writes
<atlas>_usage.png: the atlas over a dark checker with every used rectangle outlined and numbered, plus a legend .txt,
which is how the repaint regions in make_theme.py were chosen.
"""
from __future__ import annotations

import argparse
import json
import re
from collections import defaultdict
from pathlib import Path

UI_DIR = Path("D:/titan/client/datasources/ui")
TEX_DIRS = [Path("D:/titan/data/sku.0/sys.client/compiled/game/texture")]

TAG_RE = re.compile(r"<(/?)([A-Za-z0-9_]+)((?:\s+[A-Za-z0-9_]+='[^']*')*)\s*(/?)>", re.S)
ATTR_RE = re.compile(r"([A-Za-z0-9_]+)='([^']*)'")


def parse_rect(s: str):
    try:
        x0, y0, x1, y1 = (int(v) for v in s.split(","))
    except ValueError:
        return None
    if x1 <= x0 or y1 <= y0:
        return None
    return x0, y0, x1, y1


def scan(atlases: set[str] | None):
    """Returns {atlas: {rect: [paths...]}} for every Source/SourceRect pair in the UI source."""
    usage: dict[str, dict[tuple, list[str]]] = defaultdict(lambda: defaultdict(list))
    for f in sorted(UI_DIR.glob("*.inc")) + sorted(UI_DIR.glob("*.ui")):
        text = f.read_text(encoding="latin-1")
        stack: list[str] = [f.stem]
        for m in TAG_RE.finditer(text):
            closing, tag, attrs, selfclose = m.groups()
            if closing:
                if len(stack) > 1:
                    stack.pop()
                continue
            a = dict(ATTR_RE.findall(attrs))
            name = a.get("Name", tag)
            src = a.get("Source") or a.get("SourceResource")
            rect = a.get("SourceRect")
            if src and rect:
                atlas = src.split(":")[-1].lower()
                r = parse_rect(rect)
                if r and (atlases is None or atlas in atlases):
                    usage[atlas][r].append(".".join(stack[1:] + [name]) + f" ({f.stem})")
            if not selfclose and tag not in ("include",):
                stack.append(name)
    return usage


def find_texture(atlas: str) -> Path | None:
    for d in TEX_DIRS:
        p = d / f"{atlas}.dds"
        if p.exists():
            return p
    return None


def render(atlas: str, rects: dict[tuple, list[str]], out_dir: Path):
    from PIL import Image, ImageDraw

    tex = find_texture(atlas)
    if not tex:
        print(f"{atlas}: texture not found")
        return
    im = Image.open(tex).convert("RGBA")
    scale = 2
    w, h = im.size
    bg = Image.new("RGBA", im.size, (24, 24, 28, 255))
    checker = ImageDraw.Draw(bg)
    for y in range(0, h, 8):
        for x in range(0, w, 8):
            if (x // 8 + y // 8) % 2:
                checker.rectangle([x, y, x + 7, y + 7], fill=(34, 34, 40, 255))
    bg.alpha_composite(im)
    big = bg.resize((w * scale, h * scale), Image.NEAREST)
    d = ImageDraw.Draw(big)
    legend = []
    for i, (r, paths) in enumerate(sorted(rects.items(), key=lambda kv: (kv[0][1], kv[0][0]))):
        x0, y0, x1, y1 = r
        d.rectangle([x0 * scale, y0 * scale, x1 * scale - 1, y1 * scale - 1], outline=(255, 60, 200, 255))
        d.text((x0 * scale + 1, y0 * scale), str(i), fill=(255, 255, 0, 255))
        uniq = sorted(set(paths))
        legend.append(f"{i:4d} {r} x{len(paths):<4d} " + " | ".join(uniq[:6]) + (" ..." if len(uniq) > 6 else ""))
    out_dir.mkdir(parents=True, exist_ok=True)
    big.save(out_dir / f"{atlas}_usage.png")
    (out_dir / f"{atlas}_usage.txt").write_text("\n".join(legend), encoding="utf-8")
    print(f"{atlas}: {len(rects)} rects -> {out_dir / (atlas + '_usage.png')}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("atlases", nargs="*")
    ap.add_argument("--png", type=Path)
    ap.add_argument("--json", type=Path)
    args = ap.parse_args()
    wanted = {a.lower() for a in args.atlases} or None
    usage = scan(wanted)
    if args.json:
        args.json.write_text(json.dumps({a: {",".join(map(str, r)): p for r, p in rs.items()} for a, rs in usage.items()},
                                        indent=1), encoding="utf-8")
    for atlas, rects in sorted(usage.items(), key=lambda kv: -sum(len(v) for v in kv[1].values())):
        if args.png:
            render(atlas, rects, args.png)
        elif wanted is None:
            print(f"{sum(len(v) for v in rects.values()):6d} uses {len(rects):5d} rects  {atlas}")
        else:
            print(f"{atlas}: {len(rects)} rects, {sum(len(v) for v in rects.values())} uses")


if __name__ == "__main__":
    main()
