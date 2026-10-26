"""Retarget Kinect positions onto an SKT without rewriting RPRE/RPST.

Each driven SWG joint aims its *outgoing child bone* (child BPTR) at a Kinect
bone. Aim is computed in the parent joint's current space, then converted to
an ANS delta:

    R0          = post * bind * pre
    restParent  = R0 * child.bind_t
    liveParent  = inv(parentWorldQ) * (kinectChild - kinectParent)
    aim         = quat_from_to(restParent, liveParent)   [calibrated vs T-pose]
    delta       = inv(post) * aim * post

Maya rotate = euler(delta * bind). jointOrient / rotateAxis keep RPST / RPRE.

Root is translation + yaw only. Clavicles, fingers, hold_*, and Head stay at
bind. Ulnae copy the forearm delta so the twist bone does not kink.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple

from .kinect import CONF_MED, BodyFrame
from .mapping import drives_for_names, follow_for_names, lock_for_names
from .math3d import (
    Quat,
    Vec3,
    engine_quat_to_maya_euler_deg,
    engine_vec_to_maya,
    kinect_to_engine,
    qconj,
    qmul,
    qnorm,
    qrot,
    quat_from_to,
    slerp,
    vadd,
    vlen,
    vnorm,
    vscale,
    vsub,
)
from .skt import SktJoint, SktSkeleton

IDENT: Quat = (1.0, 0.0, 0.0, 0.0)


@dataclass
class PoseSample:
    """One frame of SWG-space pose ready for Maya / viewport / ANS."""
    joint_delta_q: Dict[str, Quat] = field(default_factory=dict)
    joint_maya_euler: Dict[str, Vec3] = field(default_factory=dict)
    root_delta_t: Vec3 = (0.0, 0.0, 0.0)
    root_maya_t: Vec3 = (0.0, 0.0, 0.0)
    swg_world: Dict[str, Vec3] = field(default_factory=dict)
    swg_rot: Dict[str, Quat] = field(default_factory=dict)
    kinect_world: Dict[str, Vec3] = field(default_factory=dict)
    confidence: Dict[str, int] = field(default_factory=dict)


@dataclass
class _Drive:
    k_parent: str
    k_child: str
    joint: SktJoint
    bone_child: SktJoint


class OneEuro:
    def __init__(self, min_cutoff: float = 1.2, beta: float = 0.02) -> None:
        self.min_cutoff = min_cutoff
        self.beta = beta
        self._x: Optional[Quat] = None

    def filter(self, q: Quat, dt: float) -> Quat:
        if self._x is None:
            self._x = q
            return q
        t = min(1.0, dt * (self.min_cutoff + self.beta))
        self._x = slerp(self._x, q, t)
        return self._x


def _local_bind_q(j: SktJoint) -> Quat:
    return qmul(j.post_q, qmul(j.bind_q, j.pre_q))


def _bind_fk(skt: SktSkeleton) -> Tuple[Dict[int, Vec3], Dict[int, Quat]]:
    world_t: Dict[int, Vec3] = {}
    world_q: Dict[int, Quat] = {}
    for j in skt.joints:
        lq = _local_bind_q(j)
        if j.parent < 0:
            world_t[j.index] = j.bind_t
            world_q[j.index] = lq
        else:
            pq = world_q[j.parent]
            world_t[j.index] = vadd(world_t[j.parent], qrot(pq, j.bind_t))
            world_q[j.index] = qmul(pq, lq)
    return world_t, world_q


def _pick_bone_child(skt: SktSkeleton, joint: SktJoint, preferred: str) -> Optional[SktJoint]:
    if preferred:
        c = skt.joint_by_name(preferred)
        if c and c.parent == joint.index:
            return c
    # spine3 has neck + both clavs — never aim a clav.
    named = {skt.joints[i].name.lower(): skt.joints[i] for i in joint.children}
    for key in ("neck", "lforearm", "rforearm", "lshin", "rshin",
                "lulna", "rulna", "lwrist", "rwrist", "head",
                "lankle", "rankle", "ltoe", "rtoe", "lindex01"):
        if key in named:
            return named[key]
    for i in joint.children:
        c = skt.joints[i]
        if not c.is_hardpoint:
            return c
    return None


def _flatten_xz(v: Vec3) -> Optional[Vec3]:
    f = (v[0], 0.0, v[2])
    if vlen(f) < 1e-5:
        return None
    return vnorm(f)


def _yaw_between(from_dir: Vec3, to_dir: Vec3) -> Quat:
    a = _flatten_xz(from_dir)
    b = _flatten_xz(to_dir)
    if a is None or b is None:
        return IDENT
    return quat_from_to(a, b)


class Retargeter:
    def __init__(self, skt: SktSkeleton, mapping: Dict[str, str]) -> None:
        self.skt = skt
        self.mapping = dict(mapping)
        self.scale = 1.0
        self.mirror = False
        self.lock_root = False
        self.floor_y = True
        self.min_confidence = CONF_MED
        self.smooth = 0.35
        self._prev: Dict[str, Quat] = {}
        self._t_pose_kinect: Dict[str, Vec3] = {}
        self._root_offset: Vec3 = (0.0, 0.0, 0.0)
        self._calibrated = False
        self._bind_t, self._bind_q = _bind_fk(skt)
        self._drives: List[_Drive] = []
        self._drive_names: set = set()
        names = skt.short_names()
        for ka, kb, rot, child in drives_for_names(names):
            j = skt.joint_by_name(rot)
            if not j:
                continue
            bone = _pick_bone_child(skt, j, child)
            if not bone or vlen(bone.bind_t) < 1e-5:
                continue
            self._drives.append(_Drive(ka, kb, j, bone))
            self._drive_names.add(j.name)
        self._follow = follow_for_names(names)
        self._lock = set(lock_for_names(names))
        self._lock.update(n for n in names if skt.joint_by_name(n) and skt.joint_by_name(n).is_hardpoint)

    def calibrate(self, frame: BodyFrame) -> None:
        """Capture Kinect T-pose. Actor should match SKT bind (A/T)."""
        self._t_pose_kinect.clear()
        for name, j in frame.joints.items():
            self._t_pose_kinect[name] = kinect_to_engine(j.pos)
        spine = frame.joints.get("SpineBase")
        if spine:
            e = self._kinect_pos(frame, "SpineBase")
            self._root_offset = e if e else (0.0, 0.0, 0.0)
        self._calibrated = True
        self._prev.clear()

    def _kinect_pos(self, frame: BodyFrame, name: str) -> Optional[Vec3]:
        j = frame.joints.get(name)
        if not j or j.confidence < self.min_confidence:
            return None
        p = kinect_to_engine(j.pos)
        if self.mirror:
            p = (-p[0], p[1], p[2])
        return vscale(p, self.scale)

    def _tpose_pos(self, name: str) -> Optional[Vec3]:
        p = self._t_pose_kinect.get(name)
        if p is None:
            return None
        if self.mirror:
            p = (-p[0], p[1], p[2])
        return vscale(p, self.scale)

    def _root_yaw(self, frame: BodyFrame) -> Quat:
        for left, right in (("ShoulderLeft", "ShoulderRight"), ("HipLeft", "HipRight")):
            live_l = self._kinect_pos(frame, left)
            live_r = self._kinect_pos(frame, right)
            if live_l is None or live_r is None:
                continue
            live_side = vsub(live_r, live_l)
            if self._calibrated:
                t_l = self._tpose_pos(left)
                t_r = self._tpose_pos(right)
                if t_l is None or t_r is None:
                    continue
                return _yaw_between(vsub(t_r, t_l), live_side)
            return _yaw_between((1.0, 0.0, 0.0), live_side)
        return IDENT

    def _aim_delta(self, drive: _Drive, parent_q_live: Quat, live_dir: Vec3,
                   tpose_dir: Optional[Vec3]) -> Quat:
        j = drive.joint
        r0 = _local_bind_q(j)
        rest_parent = qrot(r0, drive.bone_child.bind_t)
        if vlen(rest_parent) < 1e-5 or vlen(live_dir) < 1e-5:
            return IDENT
        live_parent = qrot(qconj(parent_q_live), live_dir)
        if vlen(live_parent) < 1e-5:
            return IDENT
        aim_live = quat_from_to(rest_parent, live_parent)
        if self._calibrated and tpose_dir is not None and vlen(tpose_dir) > 1e-5:
            pq_bind = IDENT if j.parent < 0 else self._bind_q.get(j.parent, IDENT)
            tpose_parent = qrot(qconj(pq_bind), tpose_dir)
            if vlen(tpose_parent) > 1e-5:
                aim_t = quat_from_to(rest_parent, tpose_parent)
                aim = qmul(aim_live, qconj(aim_t))
            else:
                aim = aim_live
        else:
            aim = aim_live
        # Conjugate through RPST so right-side 180° posts do not invert the limb.
        return qnorm(qmul(qconj(j.post_q), qmul(aim, j.post_q)))

    def apply(self, frame: BodyFrame, dt: float = 1.0 / 30.0) -> PoseSample:
        sample = PoseSample()
        for name, j in frame.joints.items():
            sample.kinect_world[name] = kinect_to_engine(j.pos)
            sample.confidence[name] = j.confidence

        spine = self._kinect_pos(frame, "SpineBase")
        root_j = self.skt.joint_by_name("root")
        if spine and root_j and not self.lock_root:
            dtv = vsub(spine, self._root_offset if self._calibrated else spine)
            if self.floor_y and self._calibrated:
                dtv = (dtv[0], 0.0, dtv[2])
            sample.root_delta_t = dtv
            sample.root_maya_t = engine_vec_to_maya(dtv)

        deltas: Dict[str, Quat] = {}
        for j in self.skt.joints:
            if j.name in self._lock or j.is_hardpoint:
                deltas[j.name] = IDENT

        drive_by_name = {d.joint.name: d for d in self._drives}
        live_q: Dict[int, Quat] = {}

        for j in self.skt.joints:
            if j.parent < 0:
                parent_q_live = IDENT
            else:
                parent_q_live = live_q.get(j.parent, IDENT)

            if j.name.lower() == "root":
                delta = self._root_yaw(frame) if not self.lock_root else IDENT
            elif j.name in self._lock or j.is_hardpoint:
                delta = IDENT
            elif j.name in drive_by_name:
                drive = drive_by_name[j.name]
                live_c = self._kinect_pos(frame, drive.k_child)
                live_p = self._kinect_pos(frame, drive.k_parent)
                if live_c is None or live_p is None:
                    delta = self._prev.get(j.name, IDENT)
                else:
                    live_dir = vsub(live_c, live_p)
                    tdir = None
                    if self._calibrated:
                        tc = self._tpose_pos(drive.k_child)
                        tp = self._tpose_pos(drive.k_parent)
                        if tc is not None and tp is not None:
                            tdir = vsub(tc, tp)
                    delta = self._aim_delta(drive, parent_q_live, live_dir, tdir)
            else:
                delta = IDENT

            prev = self._prev.get(j.name)
            if prev and self.smooth > 0.0 and j.name not in self._lock:
                delta = slerp(prev, delta, 1.0 - min(0.95, self.smooth))
            self._prev[j.name] = delta
            deltas[j.name] = delta

            local_q = qmul(j.post_q, qmul(qmul(delta, j.bind_q), j.pre_q))
            if j.parent < 0:
                live_q[j.index] = local_q
            else:
                live_q[j.index] = qmul(parent_q_live, local_q)

        for follower, source in self._follow.items():
            if source in deltas:
                deltas[follower] = deltas[source]

        for j in self.skt.joints:
            delta = qnorm(deltas.get(j.name, IDENT))
            sample.joint_delta_q[j.name] = delta
            sample.joint_maya_euler[j.name] = engine_quat_to_maya_euler_deg(
                qmul(delta, j.bind_q))

        sample.swg_world, sample.swg_rot = fk_pose(self.skt, sample)
        return sample


def fk_pose(skt: SktSkeleton, sample: PoseSample) -> Tuple[Dict[str, Vec3], Dict[str, Quat]]:
    world: Dict[str, Vec3] = {}
    world_q: Dict[str, Quat] = {}
    idx_q: Dict[int, Quat] = {}
    for j in skt.joints:
        delta = sample.joint_delta_q.get(j.name, IDENT)
        local_q = qmul(j.post_q, qmul(qmul(delta, j.bind_q), j.pre_q))
        local_t = j.bind_t
        if j.name.lower() == "root":
            local_t = vadd(local_t, sample.root_delta_t)
        if j.parent < 0:
            world[j.name] = local_t
            world_q[j.name] = local_q
            idx_q[j.index] = local_q
        else:
            pw = world.get(skt.joints[j.parent].name, (0.0, 0.0, 0.0))
            pq = idx_q.get(j.parent, IDENT)
            world[j.name] = vadd(pw, qrot(pq, local_t))
            wq = qmul(pq, local_q)
            world_q[j.name] = wq
            idx_q[j.index] = wq
    return world, world_q
