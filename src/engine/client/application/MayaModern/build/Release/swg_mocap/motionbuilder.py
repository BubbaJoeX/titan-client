"""MotionBuilder characterization + Kinect device notes.

SWG joints use short SKT names. MotionBuilder HumanIK slots need a characterization
file so plot → FBX → Maya lands on the same joints MayaModern imported from the SKT.

This writes:
  - <clip>.swgchar.json   mapping Kinect / HIK slots → SKT names
  - swg_mocap_mobu.py     run inside MotionBuilder (pyfbsdk)
"""

from __future__ import annotations

import json
from pathlib import Path
from typing import Dict

from .skt import SktSkeleton

HIK_SLOTS = {
    "Hips": "root",
    "LeftUpLeg": "lThigh",
    "LeftLeg": "lShin",
    "LeftFoot": "lAnkle",
    "LeftToeBase": "lToe",
    "RightUpLeg": "rThigh",
    "RightLeg": "rShin",
    "RightFoot": "rAnkle",
    "RightToeBase": "rToe",
    "Spine": "spine1",
    "Spine1": "spine2",
    "Spine2": "spine3",
    "Neck": "neck",
    "Head": "Head",
    "LeftShoulder": "lClav",
    "LeftArm": "lArm",
    "LeftForeArm": "lForeArm",
    "LeftHand": "lWrist",
    "RightShoulder": "rClav",
    "RightArm": "rArm",
    "RightForeArm": "rForeArm",
    "RightHand": "rWrist",
}


MOBU_SCRIPT = r'''
# SWG Mocap — run in MotionBuilder Python Editor after importing the FBX
# exported from Maya (importSat / importSkeleton first).
# Usage: exec(open(r"PATH/swg_mocap_mobu.py").read())

from pyfbsdk import *
import json
import os

CHAR_JSON = os.environ.get("SWG_MOCAP_CHAR", "")
if not CHAR_JSON:
    raise RuntimeError("Set SWG_MOCAP_CHAR to the .swgchar.json written by the Maya suite")

with open(CHAR_JSON, "r") as f:
    spec = json.load(f)

system = FBSystem()
scene = system.Scene
app = FBApplication()

def find_model(name):
    name_l = name.lower()
    for m in scene.Components:
        if not isinstance(m, FBModel):
            continue
        n = m.Name.split(":")[-1].split("__")[0]
        if n.lower() == name_l:
            return m
    return None

char_name = spec.get("character", "SWG_Character")
character = FBCharacter(char_name)

slots = spec.get("hik", {})
linked = 0
for hik, swg in slots.items():
    model = find_model(swg)
    if model is None:
        print("SWG MoBu: missing joint", swg, "for", hik)
        continue
    prop = character.PropertyList.Find(hik + "Link")
    if prop:
        prop.append(model)
        linked += 1

character.SetCharacterizeOn(True)
print("SWG MoBu: characterized", char_name, "links", linked)
print("Next: Character Controls → Source = your Kinect / optical device, then Plot All.")
'''


def write_characterization(skt: SktSkeleton, mapping: Dict[str, str], dest: str) -> str:
    dest_p = Path(dest)
    dest_p.parent.mkdir(parents=True, exist_ok=True)
    names = {j.name.lower(): j.name for j in skt.joints}
    hik = {}
    for slot, default in HIK_SLOTS.items():
        if default.lower() in names:
            hik[slot] = names[default.lower()]
            continue
        # battle_droid style
        alt = {
            "lThigh": "lthigh", "lShin": "lknee", "lAnkle": "lankle", "lToe": "ltoe",
            "rThigh": "rthigh", "rShin": "rknee", "rAnkle": "rankle", "rToe": "rtoe",
            "spine1": "spine", "spine2": "spine", "spine3": "spine",
            "Head": "head", "lClav": "larm", "lArm": "larm", "lForeArm": "lforearm",
            "lWrist": "lwrist", "rClav": "rarm", "rArm": "rarm", "rForeArm": "rforearm",
            "rWrist": "rwrist",
        }.get(default)
        if alt and alt in names:
            hik[slot] = names[alt]
    payload = {
        "skeleton": skt.path,
        "basename": skt.basename,
        "character": "SWG_" + skt.basename,
        "kinectToSwg": mapping,
        "hik": hik,
        "notes": [
            "Import SAT/SKT in Maya first so jointOrient/rotateOrientation match SKT RPRE/RPST.",
            "Export FBX from the suite, import FBX in MotionBuilder, run swg_mocap_mobu.py.",
            "Plot to skeleton, send FBX back to Maya, File > Export All as SwgAns.",
            "Do not characterize hold_* sockets.",
        ],
    }
    dest_p.write_text(json.dumps(payload, indent=2), encoding="utf-8")
    script = dest_p.with_name("swg_mocap_mobu.py")
    script.write_text(MOBU_SCRIPT, encoding="utf-8")
    return str(dest_p)
