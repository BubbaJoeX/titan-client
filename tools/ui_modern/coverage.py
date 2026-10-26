"""What the theme hasn't touched yet: atlas rectangles the UI uses that no repaint covers and no retarget moved,
ranked by how many objects use them, with the style / page paths that use them.

    py -3 client/tools/ui_modern/coverage.py [atlas ...] [--top 60] [--png out_dir]

--png writes <atlas>_todo.png: the themed atlas with the untouched rectangles outlined, numbered as in the list.
Icons legitimately stay stock; frames, bars and decorations in the list are candidates for theme_spec.py.
"""
from __future__ import annotations

import argparse
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import atlas_usage  # noqa: E402
import make_theme  # noqa: E402
import theme_spec as spec  # noqa: E402
import uiinc  # noqa: E402


def painted_boxes(top):
    boxes = defaultdict(list)
    for entry in spec.REPAINTS:
        target = entry[0]
        if isinstance(target, tuple):
            boxes[target[0]].append(target[1])
        else:
            for a, rs in make_theme.family_rects(top, target).items():
                boxes[a] += make_theme.clusters(rs)
    return boxes


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("atlases", nargs="*", default=["ui_rebel_final", "hud_ground_skin", "ui_rebel_new_style"])
    ap.add_argument("--top", type=int, default=60)
    ap.add_argument("--png", type=Path)
    args = ap.parse_args()
    top = uiinc.load_tree(make_theme.UI_SRC)
    covered = painted_boxes(top)
    usage = atlas_usage.scan({a.lower() for a in args.atlases})
    for atlas in args.atlases:
        rects = usage.get(atlas.lower(), {})
        todo = []
        for r, paths in rects.items():
            inside = any(b[0] <= r[0] and b[1] <= r[1] and b[2] >= r[2] and b[3] >= r[3] for b in covered.get(atlas, []))
            if not inside:
                todo.append((len(paths), r, sorted(set(paths))))
        todo.sort(key=lambda t: -t[0])
        print(f"== {atlas}: {len(todo)} of {len(rects)} used rectangles untouched")
        for i, (n, r, paths) in enumerate(todo[:args.top]):
            print(f"{i:3d} x{n:<4d} {r}  " + " | ".join(paths[:3]) + (" ..." if len(paths) > 3 else ""))
        if args.png:
            from PIL import Image, ImageDraw
            tex = make_theme.OUT / "texture" / f"{atlas}.dds"
            src = tex if tex.exists() else make_theme.stock_texture_path(atlas)
            im = Image.open(src).convert("RGBA")
            bg = Image.new("RGBA", im.size, (24, 26, 30, 255))
            bg.alpha_composite(im)
            big = bg.resize((im.size[0] * 2, im.size[1] * 2), Image.NEAREST)
            d = ImageDraw.Draw(big)
            for i, (n, r, _) in enumerate(todo[:args.top]):
                d.rectangle([r[0] * 2, r[1] * 2, r[2] * 2 - 1, r[3] * 2 - 1], outline=(255, 80, 60, 255))
                d.text((r[0] * 2 + 1, r[1] * 2), str(i), fill=(255, 255, 0, 255))
            args.png.mkdir(parents=True, exist_ok=True)
            big.convert("RGB").save(args.png / f"{atlas}_todo.png")


if __name__ == "__main__":
    main()
