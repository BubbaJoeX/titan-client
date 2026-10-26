"""Quaternions and vectors for SWG engine space (Y-up, X mirrored vs Maya)."""

from __future__ import annotations

import math
from typing import Tuple

Quat = Tuple[float, float, float, float]  # w, x, y, z
Vec3 = Tuple[float, float, float]


def vadd(a: Vec3, b: Vec3) -> Vec3:
    return (a[0] + b[0], a[1] + b[1], a[2] + b[2])


def vsub(a: Vec3, b: Vec3) -> Vec3:
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def vscale(a: Vec3, s: float) -> Vec3:
    return (a[0] * s, a[1] * s, a[2] * s)


def vdot(a: Vec3, b: Vec3) -> float:
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def vlen(a: Vec3) -> float:
    return math.sqrt(max(1e-12, vdot(a, a)))


def vnorm(a: Vec3) -> Vec3:
    l = vlen(a)
    return (a[0] / l, a[1] / l, a[2] / l)


def vcross(a: Vec3, b: Vec3) -> Vec3:
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def qmul(a: Quat, b: Quat) -> Quat:
    aw, ax, ay, az = a
    bw, bx, by, bz = b
    return (
        aw * bw - ax * bx - ay * by - az * bz,
        aw * bx + ax * bw + ay * bz - az * by,
        aw * by - ax * bz + ay * bw + az * bx,
        aw * bz + ax * by - ay * bx + az * bw,
    )


def qconj(q: Quat) -> Quat:
    return (q[0], -q[1], -q[2], -q[3])


def qnorm(q: Quat) -> Quat:
    n = math.sqrt(max(1e-12, q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]))
    return (q[0] / n, q[1] / n, q[2] / n, q[3] / n)


def qrot(q: Quat, v: Vec3) -> Vec3:
    q = qnorm(q)
    u = (0.0, v[0], v[1], v[2])
    r = qmul(qmul(q, u), qconj(q))
    return (r[1], r[2], r[3])


def quat_from_to(a: Vec3, b: Vec3) -> Quat:
    an = vnorm(a)
    bn = vnorm(b)
    d = vdot(an, bn)
    if d > 0.99999:
        return (1.0, 0.0, 0.0, 0.0)
    if d < -0.99999:
        axis = vcross(an, (1.0, 0.0, 0.0))
        if vlen(axis) < 1e-6:
            axis = vcross(an, (0.0, 1.0, 0.0))
        axis = vnorm(axis)
        return (0.0, axis[0], axis[1], axis[2])
    c = vcross(an, bn)
    w = math.sqrt((vlen(an) ** 2) * (vlen(bn) ** 2)) + d
    return qnorm((w, c[0], c[1], c[2]))


def quat_look(direction: Vec3, up: Vec3 = (0.0, 1.0, 0.0)) -> Quat:
    """Rotation that takes +Y (SWG bone default aim in many bipeds) toward direction."""
    return quat_from_to((0.0, 1.0, 0.0), direction)


def slerp(a: Quat, b: Quat, t: float) -> Quat:
    a = qnorm(a)
    b = qnorm(b)
    dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3]
    if dot < 0.0:
        b = (-b[0], -b[1], -b[2], -b[3])
        dot = -dot
    if dot > 0.9995:
        return qnorm((a[0] + t * (b[0] - a[0]), a[1] + t * (b[1] - a[1]),
                      a[2] + t * (b[2] - a[2]), a[3] + t * (b[3] - a[3])))
    theta = math.acos(min(1.0, dot))
    s = math.sin(theta)
    w0 = math.sin((1.0 - t) * theta) / s
    w1 = math.sin(t * theta) / s
    return qnorm((a[0] * w0 + b[0] * w1, a[1] * w0 + b[1] * w1,
                  a[2] * w0 + b[2] * w1, a[3] * w0 + b[3] * w1))


def quat_to_euler_xyz(q: Quat) -> Vec3:
    """Engine quat → XYZ euler radians (MayaConversions: Y,Z negated for Maya)."""
    w, x, y, z = qnorm(q)
    sinr = 2.0 * (w * x + y * z)
    cosr = 1.0 - 2.0 * (x * x + y * y)
    rx = math.atan2(sinr, cosr)
    sinp = 2.0 * (w * y - z * x)
    if abs(sinp) >= 1.0:
        ry = math.copysign(math.pi / 2.0, sinp)
    else:
        ry = math.asin(sinp)
    siny = 2.0 * (w * z + x * y)
    cosy = 1.0 - 2.0 * (y * y + z * z)
    rz = math.atan2(siny, cosy)
    return (rx, ry, rz)


def engine_quat_to_maya_euler_deg(q: Quat) -> Vec3:
    rx, ry, rz = quat_to_euler_xyz(q)
    return (math.degrees(rx), math.degrees(-ry), math.degrees(-rz))


def engine_vec_to_maya(v: Vec3) -> Vec3:
    return (-v[0], v[1], v[2])


def maya_vec_to_engine(v: Vec3) -> Vec3:
    return (-v[0], v[1], v[2])


def kinect_to_engine(v: Vec3) -> Vec3:
    """Kinect camera space (X right, Y up, Z toward camera) → SWG engine (X right-negated Maya)."""
    return (v[0], v[1], -v[2])
