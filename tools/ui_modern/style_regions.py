"""List the atlas regions behind UI styles: for each style path, the union rectangle of every ImageStyle under it,
per texture. The repaint recipes in theme_spec.py were picked from this output.

    py -3 client/tools/ui_modern/style_regions.py /Styles.New.whitebox /Styles.New.buttons.hud ...
    py -3 client/tools/ui_modern/style_regions.py --top 80      (the most used RectangleStyle/Button styles)
"""
from __future__ import annotations

import argparse
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import uiinc  # noqa: E402

UI = Path("D:/titan/client/datasources/ui")


def regions(node: uiinc.Node):
    out: dict[str, list[tuple[int, int, int, int]]] = defaultdict(list)
    for n in node.walk():
        if "SourceRect" in n.attrs and ("Source" in n.attrs or "SourceResource" in n.attrs):
            src = (n.attrs.get("Source") or n.attrs.get("SourceResource")).split(":")[-1].lower()
            try:
                x0, y0, x1, y1 = (int(v) for v in n.attrs["SourceRect"].split(","))
            except ValueError:
                continue
            out[src].append((min(x0, x1), min(y0, y1), max(x0, x1), max(y0, y1)))
    return {s: (min(r[0] for r in rs), min(r[1] for r in rs), max(r[2] for r in rs), max(r[3] for r in rs), len(rs))
            for s, rs in out.items()}


def style_family(path: str) -> str:
    """'/styles.new.whitebox.rs_default' -> '/styles.new.whitebox' (drop the style object's own name)."""
    return path.rsplit(".", 1)[0]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("paths", nargs="*")
    ap.add_argument("--top", type=int, default=0)
    args = ap.parse_args()
    top = uiinc.load_tree(UI)
    paths = list(args.paths)
    if args.top:
        counts = Counter()
        for f in UI.glob("*.inc"):
            for m in re.finditer(r"(?:RStyle\w*|Style)='(/[^']*)'", f.read_text(encoding="latin-1")):
                counts[style_family(m.group(1)).lower()] += 1
        paths += [p for p, _ in counts.most_common(args.top)]
    for p in paths:
        node = uiinc.resolve(top, p)
        if node is None:
            print(f"{p:60s} NOT FOUND")
            continue
        regs = regions(node)
        desc = "  ".join(f"{s}:{r[0]},{r[1]},{r[2]},{r[3]}(x{r[4]})" for s, r in sorted(regs.items()))
        print(f"{p:60s} {desc}")


if __name__ == "__main__":
    main()
