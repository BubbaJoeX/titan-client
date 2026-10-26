"""Parse SWG SKT (SLOD/SKTM) for 1:1 mocap.

Compiled game skeletons store chunk payloads little-endian (INFO, PRNT, floats).
IFF FORM sizes stay big-endian. Matches MayaModern ImportSkeleton / Iff::read_misc.

Runtime (Skeleton.cpp):
    animatedRotation   = animationDelta * bindPoseRotation
    localTranslation   = bindPoseTranslation + animationDeltaTranslation
    localToParentRot   = postMultiply * (animatedRotation * preMultiply)

ANS stores only the animation deltas. RPRE/RPST stay on the SKT.
"""

from __future__ import annotations

import struct
from dataclasses import dataclass, field
from pathlib import Path
from typing import List, Optional, Tuple


def _walk_iff(data: bytes, off: int, end: int, acc: list, indent: int = 0) -> None:
    while off + 8 <= end:
        tag = data[off:off + 4]
        size = struct.unpack(">I", data[off + 4:off + 8])[0]
        payload = off + 8
        nxt = payload + size
        if nxt > len(data) + 8:
            break
        if tag == b"FORM":
            if payload + 4 > end:
                break
            form = data[payload:payload + 4]
            acc.append((indent, "FORM", form, payload + 4, nxt))
            _walk_iff(data, payload + 4, min(nxt, end), acc, indent + 1)
        else:
            acc.append((indent, "CHUNK", tag, payload, nxt))
        off = nxt


@dataclass
class SktJoint:
    name: str
    index: int
    parent: int
    bind_t: Tuple[float, float, float] = (0.0, 0.0, 0.0)
    bind_q: Tuple[float, float, float, float] = (1.0, 0.0, 0.0, 0.0)  # wxyz
    pre_q: Tuple[float, float, float, float] = (1.0, 0.0, 0.0, 0.0)
    post_q: Tuple[float, float, float, float] = (1.0, 0.0, 0.0, 0.0)
    rot_order: int = 0
    children: List[int] = field(default_factory=list)

    @property
    def is_hardpoint(self) -> bool:
        n = self.name.lower()
        return n.startswith("hold_") or n.startswith("hp_") or n.startswith("swghardpoint")


@dataclass
class SktSkeleton:
    path: str
    basename: str
    joints: List[SktJoint] = field(default_factory=list)

    def joint_by_name(self, name: str) -> Optional[SktJoint]:
        want = name.lower()
        for j in self.joints:
            if j.name.lower() == want:
                return j
        return None

    def short_names(self) -> List[str]:
        return [j.name for j in self.joints]


def _read_cstrings(blob: bytes) -> List[str]:
    out: List[str] = []
    i = 0
    while i < len(blob):
        z = blob.find(b"\x00", i)
        if z < 0:
            if blob[i:]:
                out.append(blob[i:].decode("latin1", "replace"))
            break
        if z > i:
            out.append(blob[i:z].decode("latin1", "replace"))
        i = z + 1
    return out


def _quat_chunk(data: bytes, payload: int, nxt: int, count: int) -> List[Tuple[float, float, float, float]]:
    raw = data[payload:nxt]
    out = []
    for i in range(count):
        o = i * 16
        if o + 16 > len(raw):
            out.append((1.0, 0.0, 0.0, 0.0))
            continue
        w, x, y, z = struct.unpack_from("<4f", raw, o)
        out.append((w, x, y, z))
    return out


def load_skt(path: str) -> SktSkeleton:
    p = Path(path)
    data = p.read_bytes()
    acc: list = []
    _walk_iff(data, 0, len(data), acc)
    names: List[str] = []
    parents: List[int] = []
    bptr: List[Tuple[float, float, float]] = []
    bpro: List[Tuple[float, float, float, float]] = []
    rpre: List[Tuple[float, float, float, float]] = []
    rpst: List[Tuple[float, float, float, float]] = []
    jror: List[int] = []
    count = 0

    for _ind, kind, tag, payload, nxt in acc:
        if kind != "CHUNK":
            continue
        name = tag.decode("latin1", "replace")
        raw = data[payload:nxt]
        if name == "INFO" and len(raw) >= 4 and count == 0:
            count = struct.unpack_from("<i", raw, 0)[0]
        elif name == "NAME" and not names:
            names = _read_cstrings(raw)
        elif name == "PRNT" and not parents:
            n = len(raw) // 4
            parents = [struct.unpack_from("<i", raw, i * 4)[0] for i in range(n)]
        elif name == "BPTR" and not bptr:
            n = len(raw) // 12
            bptr = [struct.unpack_from("<3f", raw, i * 12) for i in range(n)]
        elif name == "BPRO" and not bpro:
            bpro = _quat_chunk(data, payload, nxt, max(count, len(names)))
        elif name == "RPRE" and not rpre:
            rpre = _quat_chunk(data, payload, nxt, max(count, len(names)))
        elif name == "RPST" and not rpst:
            rpst = _quat_chunk(data, payload, nxt, max(count, len(names)))
        elif name == "JROR" and not jror:
            n = len(raw) // 4
            jror = [struct.unpack_from("<I", raw, i * 4)[0] for i in range(n)]

    if count <= 0:
        count = len(names)
    joints: List[SktJoint] = []
    for i in range(count):
        nm = names[i] if i < len(names) else "joint%d" % i
        par = parents[i] if i < len(parents) else -1
        if par < -1 or par >= count:
            par = -1
        t = bptr[i] if i < len(bptr) else (0.0, 0.0, 0.0)
        joints.append(SktJoint(
            name=nm,
            index=i,
            parent=par,
            bind_t=t,
            bind_q=bpro[i] if i < len(bpro) else (1.0, 0.0, 0.0, 0.0),
            pre_q=rpre[i] if i < len(rpre) else (1.0, 0.0, 0.0, 0.0),
            post_q=rpst[i] if i < len(rpst) else (1.0, 0.0, 0.0, 0.0),
            rot_order=jror[i] if i < len(jror) else 0,
        ))
    for j in joints:
        if 0 <= j.parent < len(joints):
            joints[j.parent].children.append(j.index)

    return SktSkeleton(path=str(p), basename=p.stem, joints=joints)


def _read_cstrings_keep_empty(blob: bytes) -> List[str]:
    """SKTI stores (skeleton, attachment) pairs; primary attachment is empty."""
    out: List[str] = []
    i = 0
    while i < len(blob):
        z = blob.find(b"\x00", i)
        if z < 0:
            if i < len(blob):
                out.append(blob[i:].decode("latin1", "replace"))
            break
        out.append(blob[i:z].decode("latin1", "replace"))
        i = z + 1
    return out


def skeleton_paths_from_sat(sat_path: str) -> List[str]:
    """SKTI skeleton paths only (first of each pair)."""
    return [p for p, _a in skti_pairs_from_sat(sat_path)]


def skti_pairs_from_sat(sat_path: str) -> List[Tuple[str, str]]:
    """SMAT SKTI: (skeletonTemplate, attachmentTransform) per slot.

    Game load_0003 reads two strings per skeleton. Primary attach is \"\";
    face is appearance/skeleton/hum_m_face.skt + \"head\".
    """
    data = Path(sat_path).read_bytes()
    acc: list = []
    _walk_iff(data, 0, len(data), acc)
    pairs: List[Tuple[str, str]] = []
    for _ind, kind, tag, payload, nxt in acc:
        if kind != "CHUNK":
            continue
        name = tag.decode("latin1", "replace")
        if name != "SKTI":
            continue
        raw = data[payload:nxt]
        strs = _read_cstrings_keep_empty(raw)
        i = 0
        while i < len(strs):
            sk = strs[i].replace("\\", "/")
            att = strs[i + 1].replace("\\", "/") if i + 1 < len(strs) else ""
            if sk.lower().endswith(".skt") or "skeleton" in sk.lower():
                pairs.append((sk, att))
                i += 2
            else:
                i += 1
        break
    return pairs


def graft_skeleton(base: SktSkeleton, add: SktSkeleton, attach_name: str) -> SktSkeleton:
    """Attach add's root under base's attach joint (game Skeleton::attachSkeletonSegment).

    Face joints keep their local bind under face_root; face_root is parented to Head
    so eyes/lips/jaw follow the head without a separate face mocap rig.
    """
    if not attach_name:
        return base
    parent = base.joint_by_name(attach_name)
    if parent is None:
        parent = base.joint_by_name("Head") or base.joint_by_name("head")
    if parent is None:
        return base

    existing = {j.name.lower() for j in base.joints}
    new_joints: List[SktJoint] = []
    for j in base.joints:
        new_joints.append(SktJoint(
            name=j.name, index=j.index, parent=j.parent,
            bind_t=j.bind_t, bind_q=j.bind_q, pre_q=j.pre_q, post_q=j.post_q,
            rot_order=j.rot_order, children=[],
        ))

    index_map: dict = {}
    for j in add.joints:
        if j.name.lower() in existing and j.parent >= 0:
            # Duplicate body names on a face SKT (rare) stay on the body joint.
            hit = base.joint_by_name(j.name)
            if hit:
                index_map[j.index] = hit.index
            continue
        ni = len(new_joints)
        index_map[j.index] = ni
        if j.parent < 0:
            par = parent.index
        elif j.parent in index_map:
            par = index_map[j.parent]
        else:
            par = parent.index
        new_joints.append(SktJoint(
            name=j.name, index=ni, parent=par,
            bind_t=j.bind_t, bind_q=j.bind_q, pre_q=j.pre_q, post_q=j.post_q,
            rot_order=j.rot_order, children=[],
        ))
        existing.add(j.name.lower())

    for j in new_joints:
        j.children = []
    for j in new_joints:
        if 0 <= j.parent < len(new_joints):
            new_joints[j.parent].children.append(j.index)

    return SktSkeleton(
        path=base.path,
        basename=base.basename + "+" + add.basename,
        joints=new_joints,
    )


def resolve_tree_file(rel: str, anchor: str, extra_exts: Tuple[str, ...] = ()) -> Optional[str]:
    """Resolve a SWG tree path (shader/…, appearance/mesh/…, texture/…) next to an anchor file."""
    rel = rel.replace("\\", "/").lstrip("/")
    variants = [rel]
    suffix = Path(rel).suffix
    if extra_exts and not suffix:
        for ext in extra_exts:
            e = ext if ext.startswith(".") else "." + ext
            variants.append(rel + e)
    anchor_path = Path(anchor)
    parents = [anchor_path.parent]
    p = anchor_path.parent
    for _ in range(8):
        p = p.parent
        if p == p.parent:
            break
        parents.append(p)
    candidates = []
    for parent in parents:
        for r in variants:
            candidates.append(parent / r)
            candidates.append(parent / Path(r).name)
            if r.startswith("appearance/"):
                candidates.append(parent / "appearance" / Path(r).name)
            if r.startswith("shader/"):
                candidates.append(parent / "shader" / Path(r).name)
            if r.startswith("texture/"):
                candidates.append(parent / "texture" / Path(r).name)
                candidates.append(parent / "exported" / "texture" / Path(r).name)
    seen = set()
    for c in candidates:
        key = str(c).lower()
        if key in seen:
            continue
        seen.add(key)
        if c.is_file():
            return str(c)
    return None


def resolve_tree_skt(rel: str, sat_path: str) -> Optional[str]:
    rel = rel.replace("\\", "/")
    if not rel.lower().endswith(".skt"):
        rel += ".skt"
    return resolve_tree_file(rel, sat_path)
