"""Which UI widgets are under a screen point of the running debug client (DevBridge 'ui tree').

    py -3 client/tools/ui_modern/ui_at.py x,y [x,y ...] [--root /GroundHUD] [--depth 9]

Screen pixels are divided by the UI scale the bridge reports, then every visible widget whose world rect contains the
point is listed deepest first, with its path - the quickest way from "that thing on the screenshot" to the .inc
object and its style.
"""
from __future__ import annotations

import argparse
import sys

sys.path.insert(0, "D:/titan/support/MCP/SwgMayaMCP")
from swg_maya_mcp import client  # noqa: E402


def walk(obj, path, out, shown=True):
    """Flatten the tree; a widget only counts as visible when every ancestor is visible too."""
    p = path + "." + obj["name"] if path else obj["name"]
    shown = shown and obj.get("visible", True)
    out.append((p, obj, shown))
    for c in obj.get("children", []):
        walk(c, p, out, shown)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("points", nargs="+")
    ap.add_argument("--root", default="/GroundHUD")
    ap.add_argument("--depth", type=int, default=9)
    ap.add_argument("--port", type=int, default=7111)
    args = ap.parse_args()
    b = client.ClientBridge(args.port)
    r = b.request("ui", op="tree", path=args.root, depth=args.depth, max=20000, timeout=60)["result"]
    scale = r.get("scale", 1.0)
    nodes = []
    walk(r["object"], "", nodes)
    for pt in args.points:
        sx, sy = (int(v) for v in pt.split(","))
        ux, uy = sx / scale, sy / scale
        hits = []
        for path, o, shown in nodes:
            rect = o.get("rect")
            if not rect or not shown:
                continue
            if rect[0] <= ux < rect[2] and rect[1] <= uy < rect[3]:
                hits.append((path.count("."), path, o["type"], rect))
        print(f"== screen {sx},{sy} (ui {ux:.0f},{uy:.0f})")
        for depth, path, typ, rect in sorted(hits, reverse=True)[:8]:
            print(f"  {typ:12s} {path}  {rect}")


if __name__ == "__main__":
    main()
