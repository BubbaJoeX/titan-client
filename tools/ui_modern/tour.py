"""Screenshot tour of the player-facing windows, for checking the theme everywhere at once.

    py -3 client/tools/ui_modern/tour.py out_dir [--only name,name] [--scale 0]

Needs a debug client from `ui_client.py launch` (DevBridge with the 'ui' command and deferred back-buffer
screenshots). Each entry opens a window (a UI action or a mediator), waits a few frames, saves out_dir/<name>.jpg
from the back buffer and closes it again. --scale sets the UI scale first (0 = automatic).
"""
from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path

sys.path.insert(0, "D:/titan/support/MCP/SwgMayaMCP")
from swg_maya_mcp import client  # noqa: E402

HERE = Path(__file__).resolve().parent

# (name, how to open, how to close). "action:x" runs /ui action x; "open:X" activates mediator X; "press:/path" presses a
# button; "exec:line".
TOUR = [
    ("hud", None, None),
    ("options", "action:options", "action:options"),
    ("character", "action:characterSheet", "action:characterSheet"),
    ("inventory", "action:inventoryClose", "action:inventoryClose"),      # the action toggles the inventory
    ("datapad", "action:dataPad", "action:dataPad"),
    ("expertise", "action:expertise", "action:expertise"),
    ("quests", "action:questJournal", "action:questJournal"),
    ("map", "action:planetMap", "action:planetMap"),
    ("community", "action:community", "action:community"),
    ("collections", "action:collections", "action:collections"),
    ("commands", "action:commandBrowser", "action:commandBrowser"),
    ("mail", "action:persistentMessageBrowser", "action:persistentMessageBrowser"),
    ("chatrooms", "action:chatRooms", "action:chatRooms"),
    ("roadmap", "action:roadmap", "action:roadmap"),
    ("calendar", "open:WS_Calendar", "close:WS_Calendar"),
    ("gcw", "action:gcwInfo", "action:gcwInfo"),
    ("appearance", "action:appearanceTab", "action:appearanceTab"),
    ("service", "action:service", "action:service"),
    ("game_menu", "action:gameMenuActivate", "press:/GameMenu.menu.resumebutton"),  # Resume brings the HUD back
]


def step(b, how):
    kind, _, value = how.partition(":")
    if kind == "action":
        return b.request("exec", line=f"/ui action {value}")
    if kind in ("open", "close"):
        return b.request("ui", op=kind, value=value)
    if kind == "press":
        return b.request("ui", op="press", path=value)
    if kind == "exec":
        return b.request("exec", line=value)
    raise ValueError(how)


def capture(b, path, patience=180.0):
    """Back-buffer screenshot, retried while the D3D device is lost (D3DERR_DEVICELOST 88760868: another app holds
    exclusive fullscreen on the GPU, the display sleeps) or mid-reset after a launch; the client recovers by itself."""
    end = time.time() + patience
    while True:
        res = b.request("screenshot", path=path, timeout=30).get("result", {})
        if not isinstance(res, dict) or res.get("ok", True) or time.time() > end:
            return res
        time.sleep(5)


def frames(b, n):
    end = time.time() + n / 20.0 + 0.5
    while time.time() < end:
        b.request("ping")
        time.sleep(0.05)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out", type=Path)
    ap.add_argument("--only", default="")
    ap.add_argument("--scale", type=int, default=None)
    ap.add_argument("--port", type=int, default=7111)
    args = ap.parse_args()
    args.out = args.out.resolve()                           # the client resolves relative paths against its own cwd
    args.out.mkdir(parents=True, exist_ok=True)
    b = client.ClientBridge(args.port)
    if args.scale is not None:
        print(json.dumps(b.request("ui", op="scale", value=args.scale)))
        frames(b, 30)
    only = {x for x in args.only.split(",") if x}
    b.request("ui", op="close", value="Inventory")          # profiles reopen it; it would sit behind every shot
    frames(b, 10)
    for name, opener, closer in TOUR:
        if only and name not in only:
            continue
        if opener:
            r = step(b, opener)
            if not r.get("ok", True):
                print(f"{name}: open failed {r}")
        frames(b, 50)                                       # windows fade in through an opacity effector
        res = capture(b, str(args.out / name))
        print(f"{name}: {res.get('file') if isinstance(res, dict) else res} {'' if res.get('ok', True) else res.get('error')}")
        if closer:
            step(b, closer)
            frames(b, 8)


if __name__ == "__main__":
    main()
