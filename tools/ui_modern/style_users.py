"""Who uses a style: for each style path, count the UI objects that reference it, grouped by the object's name and
tint, with one example path each. Used to check a repaint won't hit an unexpected role (a 'glow' style that some
page uses as its only background, say).

    py -3 client/tools/ui_modern/style_users.py /Styles.space.boxes.generic4 [...]
"""
from __future__ import annotations

import collections
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import uiinc  # noqa: E402

top = uiinc.load_tree(Path("D:/titan/client/datasources/ui"))
for want in sys.argv[1:]:
    low = want.lower()
    c = collections.Counter()
    ex = {}
    for n in top.walk():
        for k, v in n.attrs.items():
            if (k.startswith("RStyle") or k == "Style") and v.lower().startswith(low):
                key = (n.tag, n.name, n.attrs.get("PalBgTint") or n.attrs.get("PalColor") or "-",
                       n.attrs.get("BackgroundOpacity", "-"))
                c[key] += 1
                ex.setdefault(key, n.path())
    print(f"== {want}: {sum(c.values())} users")
    for k, v in c.most_common(15):
        print(f"  {v:4d} {k}  e.g. {ex[k][:100]}")
