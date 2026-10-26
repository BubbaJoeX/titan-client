"""Repeatable FPS / CPU benchmark for the offline development client (SwgTitan_d, DevBridge builds only).

Launches the client with a private LOCALAPPDATA and its own bridge port, loads the offline Tatooine scene through the
bridge (Load Scene: no sign-in), spawns a crowd of dressed NPCs, wandering dewbacks and three buildings, then measures
an aerial and a street-level view. For each view it reports frames per second, main-thread CPU milliseconds per
frame and work milliseconds per frame (time outside Present and the frame rate limiter, from stack samples). FPS in a
window is capped by DWM composition (and drops when the display sleeps or the window is hidden), and the driver can
spin inside Present, so work ms/frame is the number to compare between builds.

usage: python client_bench.py <label> [--exe SwgTitan_d.exe] [--npcs 64] [--port 7101] [--profile SECONDS]
                              [--profile-view aerial|street] [--shots] [extra client args, e.g. -s Direct3d9 allowTearing=1]
Results (with the number of characters drawn at each detail level per view) are appended to results.jsonl next to
this script; --profile also writes profile_<label>.txt and stacks_<label>.pkl (see analyze.py). --shots grabs each
view from the back buffer, which works while the client window is behind other windows."""
import argparse
import ctypes
import ctypes.wintypes as wt
import json
import os
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import sampler  # noqa: E402

parser = argparse.ArgumentParser()
parser.add_argument("label")
parser.add_argument("--exe", default="SwgTitan_d.exe")
parser.add_argument("--npcs", type=int, default=64)
parser.add_argument("--port", type=int, default=7101)
parser.add_argument("--profile", type=float, default=0.0)
parser.add_argument("--profile-view", choices=("aerial", "street"), default="aerial")
parser.add_argument("--shots", action="store_true", help="screenshot each view (shot_<label>_<view>.jpg)")
args, extra = parser.parse_known_args()
if args.shots:
    # read the back buffer (needs it lockable, so no antialiasing): the front buffer is black behind other windows
    extra += ["-s", "Direct3d9", "screenShotBackBuffer=1", "-s", "Direct3d9", "antiAlias=0"]

# private LOCALAPPDATA: the bridge token file is not shared with other sessions' clients
state = Path(tempfile.gettempdir()) / "swg_perf_bench"
state.mkdir(exist_ok=True)
os.environ["LOCALAPPDATA"] = str(state)
os.environ["TITAN_CLIENT_EXE"] = args.exe
os.environ["MSYS_NO_PATHCONV"] = "1"
sys.path.insert(0, str(HERE.parents[2] / "support" / "MCP" / "SwgMayaMCP"))   # D:/titan/support/MCP/SwgMayaMCP
from swg_maya_mcp import client  # noqa: E402

NPC = ["object/mobile/shared_dressed_commoner_tatooine_%s.iff" % n for n in (
    "bith_male_01", "rodian_male_02", "nikto_male_03", "sullustan_male_01", "devaronian_male_02",
    "aqualish_female_03", "bith_female_02", "rodian_female_01", "ishitib_male_01", "nikto_male_05")]

k32 = ctypes.WinDLL("kernel32")
k32.OpenThread.restype = wt.HANDLE


def thread_cpu_seconds(handle):
    times = [ctypes.c_uint64() for _ in range(4)]
    k32.GetThreadTimes(handle, *[ctypes.byref(t) for t in times])
    return (times[2].value + times[3].value) / 1e7


def main_thread(pid):
    """Handle of the client's main thread (the game loop): the first thread the process created. At the login screen
    the game loop mostly waits in Present, so "the busiest thread" can be a loader thread instead."""
    ids = subprocess.run(["powershell", "-NoProfile", "-Command", "(Get-Process -Id %d).Threads | ForEach-Object { $_.Id }" % pid],
                         capture_output=True, text=True).stdout.split()
    handles = {int(t): k32.OpenThread(0x0040 | 0x0800, False, int(t)) for t in ids}

    def created(handle):
        times = [ctypes.c_uint64() for _ in range(4)]
        k32.GetThreadTimes(handle, *[ctypes.byref(t) for t in times])
        return times[0].value

    return handles[min((t for t in handles if handles[t]), key=lambda t: created(handles[t]))]


def state_of(bridge):
    reply = bridge.request("state", timeout=60.0)
    return reply.get("result", reply)


def measure(bridge, thread, seconds=10.0):
    """fps; main-thread CPU ms per frame (includes any spinning inside Present); work ms per frame: the frame time
    the game loop spent outside Present and the frame rate limiter, from stack samples over the same window."""
    sampled = {}
    worker = threading.Thread(target=lambda: sampled.update(raw=sampler.sample(pid, seconds)))
    f0, c0, t0 = state_of(bridge)["frame"], thread_cpu_seconds(thread), time.perf_counter()
    worker.start()
    time.sleep(seconds)
    f1, c1, t1 = state_of(bridge)["frame"], thread_cpu_seconds(thread), time.perf_counter()
    worker.join()
    frames = max(1, f1 - f0)
    work = sampler.work_fraction(sampler.symbolize(*sampled["raw"]))
    return {"fps": round(frames / (t1 - t0), 1), "cpu_ms_per_frame": round(1000.0 * (c1 - c0) / frames, 2),
            "work_ms_per_frame": round(1000.0 * work * (t1 - t0) / frames, 2)}


def run(line):
    return bridge.request("exec", line=line, timeout=60.0)


info = client.launch(port=args.port, extra_args=extra)
pid = info["pid"]
bridge = client.ClientBridge(args.port)
result = {"label": args.label, "exe": args.exe, "extra": extra, "npcs": args.npcs, "time": time.strftime("%Y-%m-%d %H:%M")}
try:
    time.sleep(4)
    thread = main_thread(pid)
    result["login"] = measure(bridge, thread, 6)
    bridge.request("loadscene", timeout=120.0, terrain="terrain/tatooine.trn")
    deadline = time.time() + 240
    while time.time() < deadline:
        st = state_of(bridge)
        if not st.get("scene_loading") and st.get("player"):
            break
        time.sleep(2)
    time.sleep(8)
    p = state_of(bridge)["player"]["pos_w"]
    px, py, pz = p
    side = int(args.npcs ** 0.5 + 0.999)
    for k in range(args.npcs):
        gx, gz = k % side, k // side
        run("/devtest spawnat %s %.1f %.1f %d" % (NPC[k % len(NPC)], px + (gx - side / 2) * 3.0, pz + 20 + gz * 3.0, (k * 37) % 360))
    for _ in range(8):
        run("/scene wanderCreate object/mobile/shared_dewback.iff")
    run("/devtest spawnat object/building/tatooine/shared_cantina_tatooine.iff %.1f %.1f 0" % (px, pz + 75))
    run("/devtest spawnat object/building/tatooine/shared_filler_building_tatt_style01_05.iff %.1f %.1f 90" % (px - 35, pz + 40))
    run("/devtest spawnat object/building/tatooine/shared_filler_building_tatt_style01_05.iff %.1f %.1f 270" % (px + 35, pz + 40))
    time.sleep(20)  # appearances, animation controllers and terrain settle
    centre = [px, py + 1, pz + 20 + side * 1.5]
    views = {"aerial": [px + 25, py + 22, pz - 15], "street": [px + 2, py + 1.8, pz + 8]}
    for view, pos in views.items():
        bridge.request("camera", mode="free", pos=pos, look=centre)
        time.sleep(5)
        result[view] = measure(bridge, thread)
        # characters per detail level drawn (0 = most detailed), and a frame to look at
        objects = bridge.request("objects", radius=250).get("result") or []
        lods = {}
        for o in objects:
            if "lod" in o:
                lods[str(o["lod"])] = lods.get(str(o["lod"]), 0) + 1
        result[view]["characters_per_lod"] = dict(sorted(lods.items()))
        if args.shots:
            shot = HERE / ("shot_%s_%s" % (args.label, view))
            bridge.request("screenshot", path=str(shot).replace("\\", "/"))
    if args.profile:
        bridge.request("camera", mode="free", pos=views[args.profile_view], look=centre)
        time.sleep(3)
        out = subprocess.run([sys.executable, str(HERE / "sampler.py"), str(pid), str(args.profile), "60",
                              str(HERE / ("stacks_%s.pkl" % args.label))], capture_output=True, text=True, timeout=1200)
        (HERE / ("profile_%s.txt" % args.label)).write_text(out.stdout + out.stderr, encoding="utf-8")
finally:
    try:
        run("/quit")
    except Exception:
        pass
    time.sleep(4)
    subprocess.run(["taskkill", "/PID", str(pid), "/F"], capture_output=True)

print(json.dumps(result, indent=1))
with open(HERE / "results.jsonl", "a", encoding="utf-8") as f:
    f.write(json.dumps(result) + "\n")
