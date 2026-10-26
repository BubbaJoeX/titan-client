"""Kinect v2 (25 joints) → SWG SKT.

Landmark table (ALL_B / BATTLE_DROID) is for UI, overlay, and MoBu slots:
Kinect joint position ≈ that SWG joint.

Retarget does NOT 1:1-aim those landmarks. Humanoids use all_b.skt and an
explicit drive list: rotate a parent SWG joint so its outgoing child bone
follows a Kinect bone. Clavicles, ulnae, fingers, and hold_* stay at bind
(or follow a sibling). Right-side all_b joints carry 180° RPST — that is
handled in retarget, not by flipping the name table.
"""

from __future__ import annotations

from typing import Dict, List, Optional, Sequence, Tuple

# Official Kinect v2 Body joint order (JointType enum).
KINECT_JOINTS: List[str] = [
    "SpineBase", "SpineMid", "Neck", "Head",
    "ShoulderLeft", "ElbowLeft", "WristLeft", "HandLeft",
    "ShoulderRight", "ElbowRight", "WristRight", "HandRight",
    "HipLeft", "KneeLeft", "AnkleLeft", "FootLeft",
    "HipRight", "KneeRight", "AnkleRight", "FootRight",
    "SpineShoulder", "HandTipLeft", "ThumbLeft", "HandTipRight", "ThumbRight",
]

KINECT_BONES: List[Tuple[str, str]] = [
    ("SpineBase", "SpineMid"),
    ("SpineMid", "SpineShoulder"),
    ("SpineShoulder", "Neck"),
    ("Neck", "Head"),
    ("SpineShoulder", "ShoulderLeft"),
    ("ShoulderLeft", "ElbowLeft"),
    ("ElbowLeft", "WristLeft"),
    ("WristLeft", "HandLeft"),
    ("HandLeft", "HandTipLeft"),
    ("WristLeft", "ThumbLeft"),
    ("SpineShoulder", "ShoulderRight"),
    ("ShoulderRight", "ElbowRight"),
    ("ElbowRight", "WristRight"),
    ("WristRight", "HandRight"),
    ("HandRight", "HandTipRight"),
    ("WristRight", "ThumbRight"),
    ("SpineBase", "HipLeft"),
    ("HipLeft", "KneeLeft"),
    ("KneeLeft", "AnkleLeft"),
    ("AnkleLeft", "FootLeft"),
    ("SpineBase", "HipRight"),
    ("HipRight", "KneeRight"),
    ("KneeRight", "AnkleRight"),
    ("AnkleRight", "FootRight"),
]

# Landmark correspondence for all_b.skt (38 joints). Shoulder* is the arm,
# not the clavicle. Hands / tips are omitted so fingers stay at bind.
ALL_B: Dict[str, str] = {
    "SpineBase": "root",
    "SpineMid": "spine1",
    "SpineShoulder": "spine3",
    "Neck": "neck",
    "Head": "Head",
    "ShoulderLeft": "lArm",
    "ElbowLeft": "lForeArm",
    "WristLeft": "lWrist",
    "ShoulderRight": "rArm",
    "ElbowRight": "rForeArm",
    "WristRight": "rWrist",
    "HipLeft": "lThigh",
    "KneeLeft": "lShin",
    "AnkleLeft": "lAnkle",
    "FootLeft": "lToe",
    "HipRight": "rThigh",
    "KneeRight": "rShin",
    "AnkleRight": "rAnkle",
    "FootRight": "rToe",
}

# (kinect_parent, kinect_child, swg_rotate, swg_outgoing_child)
# Rotate swg_rotate so outgoing child.bind_t follows the Kinect bone.
ALL_B_DRIVES: List[Tuple[str, str, str, str]] = [
    ("SpineBase", "SpineMid", "spine1", "spine2"),
    ("SpineMid", "SpineShoulder", "spine2", "spine3"),
    ("SpineShoulder", "Neck", "spine3", "neck"),
    ("Neck", "Head", "neck", "Head"),
    ("ShoulderLeft", "ElbowLeft", "lArm", "lForeArm"),
    ("ElbowLeft", "WristLeft", "lForeArm", "lUlna"),
    ("WristLeft", "HandLeft", "lWrist", "lIndex01"),
    ("ShoulderRight", "ElbowRight", "rArm", "rForeArm"),
    ("ElbowRight", "WristRight", "rForeArm", "rUlna"),
    ("WristRight", "HandRight", "rWrist", "rIndex01"),
    ("HipLeft", "KneeLeft", "lThigh", "lShin"),
    ("KneeLeft", "AnkleLeft", "lShin", "lAnkle"),
    ("AnkleLeft", "FootLeft", "lAnkle", "lToe"),
    ("HipRight", "KneeRight", "rThigh", "rShin"),
    ("KneeRight", "AnkleRight", "rShin", "rAnkle"),
    ("AnkleRight", "FootRight", "rAnkle", "rToe"),
]

# Offset / twist bones stay at bind. Do not copy forearm onto ulna — that
# would rotate lWrist's parent after the wrist has already been aimed.
ALL_B_FOLLOW: Dict[str, str] = {}

ALL_B_LOCK: Tuple[str, ...] = (
    "lClav", "rClav",
    "lThumb01", "lThumb02", "lIndex01", "lIndex02", "lRing01", "lRing02",
    "rThumb01", "rThumb02", "rIndex01", "rIndex02", "rRing01", "rRing02",
    "Head",
    "face_root", "jaw",
    "lEye", "rEye", "lLid", "rLid",
    "lBrow1", "lBrow2", "rBrow1", "rBrow2",
    "lSmile", "rSmile", "upperCenterLip", "lLowerLip", "rLowerLip",
)

BATTLE_DROID: Dict[str, str] = {
    "SpineBase": "root",
    "SpineMid": "spine",
    "Neck": "neck",
    "Head": "head",
    "ShoulderLeft": "larm",
    "ElbowLeft": "lforearm",
    "WristLeft": "lwrist",
    "HandLeft": "lknuckle",
    "ShoulderRight": "rarm",
    "ElbowRight": "rforearm",
    "WristRight": "rwrist",
    "HandRight": "rknuckle",
    "HipLeft": "lthigh",
    "KneeLeft": "lknee",
    "AnkleLeft": "lankle",
    "FootLeft": "ltoe",
    "HipRight": "rthigh",
    "KneeRight": "rknee",
    "AnkleRight": "rankle",
    "FootRight": "rtoe",
}

BATTLE_DROID_DRIVES: List[Tuple[str, str, str, str]] = [
    ("SpineBase", "SpineMid", "spine", "neck"),
    ("Neck", "Head", "neck", "head"),
    ("ShoulderLeft", "ElbowLeft", "larm", "lforearm"),
    ("ElbowLeft", "WristLeft", "lforearm", "lwrist"),
    ("WristLeft", "HandLeft", "lwrist", "lknuckle"),
    ("ShoulderRight", "ElbowRight", "rarm", "rforearm"),
    ("ElbowRight", "WristRight", "rforearm", "rwrist"),
    ("WristRight", "HandRight", "rwrist", "rknuckle"),
    ("HipLeft", "KneeLeft", "lthigh", "lknee"),
    ("KneeLeft", "AnkleLeft", "lknee", "lankle"),
    ("AnkleLeft", "FootLeft", "lankle", "ltoe"),
    ("HipRight", "KneeRight", "rthigh", "rknee"),
    ("KneeRight", "AnkleRight", "rknee", "rankle"),
    ("AnkleRight", "FootRight", "rankle", "rtoe"),
]

BATTLE_DROID_FOLLOW: Dict[str, str] = {}

BATTLE_DROID_LOCK: Tuple[str, ...] = (
    "lthumb1", "lindex", "rthumb1", "rindex", "head",
)

# Kinect bones used when synthesizing drives for an unknown SKT.
_GENERIC_AIM_BONES: List[Tuple[str, str]] = [
    ("SpineBase", "SpineMid"),
    ("SpineMid", "SpineShoulder"),
    ("SpineShoulder", "Neck"),
    ("Neck", "Head"),
    ("ShoulderLeft", "ElbowLeft"),
    ("ElbowLeft", "WristLeft"),
    ("WristLeft", "HandLeft"),
    ("ShoulderRight", "ElbowRight"),
    ("ElbowRight", "WristRight"),
    ("WristRight", "HandRight"),
    ("HipLeft", "KneeLeft"),
    ("KneeLeft", "AnkleLeft"),
    ("AnkleLeft", "FootLeft"),
    ("HipRight", "KneeRight"),
    ("KneeRight", "AnkleRight"),
    ("AnkleRight", "FootRight"),
]

ALIASES = {
    "lclavicle": "lclav",
    "rclavicle": "rclav",
    "lupperarm": "larm",
    "rupperarm": "rarm",
    "lhand": "lwrist",
    "rhand": "rwrist",
    "lfoot": "lankle",
    "rfoot": "rankle",
    "lcalf": "lshin",
    "rcalf": "rshin",
}


def preset_for_names(joint_names: List[str]) -> Dict[str, str]:
    lower = {n.lower() for n in joint_names}
    if "lthigh" in lower and "spine1" in lower:
        return dict(ALL_B)
    if "lthigh" in lower or "lknee" in lower:
        return dict(BATTLE_DROID)
    return auto_map(joint_names)


def auto_map(joint_names: List[str]) -> Dict[str, str]:
    """Fuzzy map Kinect landmarks onto whatever SKT we loaded."""
    by_lower = {n.lower(): n for n in joint_names}
    out: Dict[str, str] = {}
    guesses = {
        "SpineBase": ("root", "hips", "pelvis", "bn_root"),
        "SpineMid": ("spine1", "spine", "spine_1", "chest"),
        "SpineShoulder": ("spine3", "spine2", "spine_3", "spine"),
        "Neck": ("neck",),
        "Head": ("head",),
        "ShoulderLeft": ("larm", "lupperarm", "lshoulder"),
        "ElbowLeft": ("lforearm", "larm"),
        "WristLeft": ("lwrist", "lulna", "lhand"),
        "ShoulderRight": ("rarm", "rupperarm", "rshoulder"),
        "ElbowRight": ("rforearm", "rarm"),
        "WristRight": ("rwrist", "rulna", "rhand"),
        "HipLeft": ("lthigh",),
        "KneeLeft": ("lshin", "lknee", "lcalf"),
        "AnkleLeft": ("lankle", "lfoot"),
        "FootLeft": ("ltoe", "lfoot"),
        "HipRight": ("rthigh",),
        "KneeRight": ("rshin", "rknee", "rcalf"),
        "AnkleRight": ("rankle", "rfoot"),
        "FootRight": ("rtoe", "rfoot"),
    }
    used = set()
    for kj, cands in guesses.items():
        for c in cands:
            c = ALIASES.get(c, c)
            if c in by_lower and by_lower[c] not in used:
                out[kj] = by_lower[c]
                used.add(by_lower[c])
                break
    return out


def _resolve_name(skt_names: Sequence[str], want: str) -> Optional[str]:
    low = want.lower()
    for n in skt_names:
        if n.lower() == low:
            return n
    return None


def drives_for_names(joint_names: List[str]) -> List[Tuple[str, str, str, str]]:
    """Kinect bone → (SWG rotator, SWG outgoing child), names as in the SKT."""
    lower = {n.lower() for n in joint_names}
    if "spine1" in lower and "larm" in lower:
        src = ALL_B_DRIVES
    elif "lknee" in lower or ("lthigh" in lower and "spine" in lower):
        src = BATTLE_DROID_DRIVES
    else:
        src = []
        mapping = preset_for_names(list(joint_names))
        used = set()
        for ka, kb in _GENERIC_AIM_BONES:
            rot = mapping.get(ka)
            if not rot or rot.lower() == "root" or rot.lower() in used:
                continue
            child = None
            # Prefer the mapped Kinect child as the outgoing bone when it is
            # actually a child of rot; otherwise retarget picks one.
            mapped_child = mapping.get(kb)
            src.append((ka, kb, rot, mapped_child or ""))
            used.add(rot.lower())
        return src

    out: List[Tuple[str, str, str, str]] = []
    for ka, kb, rot, child in src:
        r = _resolve_name(joint_names, rot)
        c = _resolve_name(joint_names, child) if child else None
        if r:
            out.append((ka, kb, r, c or ""))
    return out


def follow_for_names(joint_names: List[str]) -> Dict[str, str]:
    lower = {n.lower() for n in joint_names}
    src = ALL_B_FOLLOW if "lulna" in lower else BATTLE_DROID_FOLLOW
    out: Dict[str, str] = {}
    for a, b in src.items():
        ra = _resolve_name(joint_names, a)
        rb = _resolve_name(joint_names, b)
        if ra and rb:
            out[ra] = rb
    return out


def lock_for_names(joint_names: List[str]) -> List[str]:
    lower = {n.lower() for n in joint_names}
    src = ALL_B_LOCK if "lclav" in lower else BATTLE_DROID_LOCK
    out: List[str] = []
    for n in src:
        r = _resolve_name(joint_names, n)
        if r:
            out.append(r)
    for n in joint_names:
        ln = n.lower()
        if ln.startswith("hold_") or ln.startswith("hp_"):
            if n not in out:
                out.append(n)
        elif ln == "face_root" or ln == "jaw" or any(
                h in ln for h in ("eye", "lid", "lip", "brow", "smile")):
            if n not in out:
                out.append(n)
    return out
