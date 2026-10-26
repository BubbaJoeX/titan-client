"""Check character LOD selection: one NPC in the offline Tatooine scene, the free camera moved to a series of distances
from it (in and back out), and the detail level the client drew at each (0 = most detailed).

usage: python lod_sweep.py [--exe SwgTitan_d.exe] [--port 7102] [extra client args, e.g. -s ClientSkeletalAnimation forceHighestLod=false]"""
import argparse
import json
import os
import subprocess
import sys
import tempfile
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent

parser = argparse.ArgumentParser()
parser.add_argument("--exe", default="SwgTitan_d.exe")
parser.add_argument("--port", type=int, default=7102)
args, extra = parser.parse_known_args()

state = Path(tempfile.gettempdir()) / "swg_perf_bench"
state.mkdir(exist_ok=True)
os.environ["LOCALAPPDATA"] = str(state)
os.environ["TITAN_CLIENT_EXE"] = args.exe
os.environ["MSYS_NO_PATHCONV"] = "1"
sys.path.insert(0, str(HERE.parents[2] / "support" / "MCP" / "SwgMayaMCP"))
from swg_maya_mcp import client  # noqa: E402

NPC = "object/mobile/shared_dressed_commoner_tatooine_rodian_male_02.iff"
DISTANCES = [2, 4, 8, 12, 20, 35, 60, 35, 20, 12, 8, 4, 2]

info = client.launch(port=args.port, extra_args=extra)
bridge = client.ClientBridge(args.port)
rows = []
try:
    time.sleep(4)
    bridge.request("loadscene", timeout=120.0, terrain="terrain/tatooine.trn")
    deadline = time.time() + 240
    while time.time() < deadline:
        st = bridge.request("state", timeout=60.0).get("result", {})
        if not st.get("scene_loading") and st.get("player"):
            break
        time.sleep(2)
    time.sleep(6)
    px, py, pz = bridge.request("state").get("result")["player"]["pos_w"]
    nx, nz = px, pz + 10
    bridge.request("exec", line="/devtest spawnat %s %.1f %.1f 180" % (NPC, nx, nz), timeout=60.0)
    time.sleep(8)
    for d in DISTANCES:
        bridge.request("camera", mode="free", pos=[nx + d * 0.6, py + 1.6, nz - d * 0.8], look=[nx, py + 1.0, nz])
        time.sleep(1.5)
        lods = [o for o in bridge.request("objects", radius=40).get("result") or [] if o.get("template", "").endswith(NPC.split("/")[-1])]
        rows.append({"distance": d, "lod": lods[0]["lod"] if lods else None, "lods": lods[0]["lods"] if lods else None})
        print(json.dumps(rows[-1]))
finally:
    try:
        bridge.request("exec", line="/quit", timeout=5.0)
    except Exception:
        pass
    time.sleep(4)
    subprocess.run(["taskkill", "/PID", str(info["pid"]), "/F"], capture_output=True)
