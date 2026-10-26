"""Drive an offline debug client for UI checks: launch, load the scene, run console lines, capture windows.

    py -3 client/tools/ui_modern/ui_client.py launch [--port 7111]          # titanAdmin on (/ui reset, /ui activate)
    py -3 client/tools/ui_modern/ui_client.py run "/ui action options" ... # console lines, in order
    py -3 client/tools/ui_modern/ui_client.py shot out.jpg                 # back-buffer screenshot (DevBridge)
    py -3 client/tools/ui_modern/ui_client.py quit

Uses the SwgMayaMCP ClientBridge (pinned token per port). The window stays on the primary display at the bottom of
the z-order (grab_window.py --background), so it never covers the user's or another session's windows; screenshots
come from the back buffer, so a covered window still captures. 'launch' closes this tool's previous client first if
it is still around (a quit that logged warnings waits on a 'Report' dialog), by its recorded pid only.
"""
from __future__ import annotations

import argparse
import json
import subprocess
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, "D:/titan/support/MCP/SwgMayaMCP")
from swg_maya_mcp import client  # noqa: E402

STATE = HERE / "out" / "_client.json"


def load_state():
    return json.loads(STATE.read_text()) if STATE.exists() else {}


def save_state(d):
    STATE.parent.mkdir(parents=True, exist_ok=True)
    STATE.write_text(json.dumps(d))


def pid_alive(pid: int) -> bool:
    out = subprocess.run(["tasklist", "/FI", f"PID eq {pid}", "/NH"], capture_output=True, text=True).stdout
    return "SwgTitan_d.exe" in out


def bridge(port: int):
    return client.ClientBridge(port)


def cmd_launch(args):
    st = load_state()
    old = st.get("pid")
    if old and pid_alive(old):
        try:
            bridge(args.port).request("exec", line="/quit", timeout=5)
            time.sleep(8)
        except Exception:
            pass
        if pid_alive(old):
            subprocess.run(["taskkill", "/PID", str(old), "/F"], capture_output=True)
            time.sleep(3)
    info = client.launch(args.port, extra_args=["titanAdmin=true"])
    save_state({"pid": info["pid"], "port": args.port})
    b = bridge(args.port)
    if args.no_scene:                                   # stay on the login screen
        park(info["pid"])
        print(json.dumps(info))
        return
    b.request("loadscene", terrain=args.terrain) if args.terrain else b.request("loadscene")
    deadline = time.time() + 240
    while time.time() < deadline:
        s = b.request("state")
        res = s.get("result", s) if isinstance(s.get("result", s), dict) else s
        if not res.get("scene_loading", True) and res.get("player"):
            break
        time.sleep(2)
    for line in ["/ui palette " + args.palette]:
        b.request("exec", line=line)
    park(info["pid"])
    print(json.dumps(info))


def park(pid):
    subprocess.run([sys.executable, str(HERE / "grab_window.py"), str(pid), str(HERE / "out" / "_park.png"),
                    "--background"], capture_output=True)


def cmd_run(args):
    st = load_state()
    b = bridge(st.get("port", args.port))
    for line in args.lines:
        r = b.request("exec", line=line)
        out = r.get("output") or (r.get("result") or {}).get("output", "") if isinstance(r.get("result"), dict) else r.get("output", "")
        print(f"> {line}\n{out}".rstrip())
        b.request("settle", frames=args.frames) if args.frames else None


def cmd_shot(args):
    """Back-buffer screenshot through the bridge (retried while the device is lost), copied to args.out."""
    st = load_state()
    sys.path.insert(0, str(HERE))
    import tour
    b = bridge(st.get("port", args.port))
    if args.frames:
        b.request("settle", frames=args.frames)
    stem = Path(args.out).resolve().with_suffix("")      # the bridge adds .jpg and resolves against the client's cwd
    print(tour.capture(b, str(stem)))


def cmd_quit(args):
    st = load_state()
    try:
        bridge(st.get("port", args.port)).request("exec", line="/quit", timeout=5)
    except Exception as e:
        print(e)
    time.sleep(8)
    if st.get("pid") and pid_alive(st["pid"]):
        subprocess.run(["taskkill", "/PID", str(st["pid"]), "/F"], capture_output=True)
    save_state({})


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=7111)
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("launch")
    p.add_argument("--palette", default="Titan")
    p.add_argument("--terrain", default="terrain/tatooine.trn")
    p.add_argument("--no-scene", action="store_true", help="stop at the login screen")
    p.set_defaults(fn=cmd_launch)
    p = sub.add_parser("run")
    p.add_argument("lines", nargs="+")
    p.add_argument("--frames", type=int, default=10)
    p.set_defaults(fn=cmd_run)
    p = sub.add_parser("shot")
    p.add_argument("out")
    p.add_argument("--frames", type=int, default=5)
    p.set_defaults(fn=cmd_shot)
    p = sub.add_parser("quit")
    p.set_defaults(fn=cmd_quit)
    args = ap.parse_args()
    args.fn(args)


if __name__ == "__main__":
    main()
