"""
Split selected polygon mesh into four pieces using two perpendicular cutting planes
through the world-space bounding box center.

Default planes are perpendicular to X and Z (four quadrants on the ground plane, Y-up).
Use axis_pair=('x', 'y') for quadrants in the XY plane.

Each piece gets open cut borders filled with polyCloseBorder (planar caps).

Usage (Script Editor, Python):
    import swgSplitMeshFourQuadrants
    swgSplitMeshFourQuadrants.run()

Or with explicit axes (world space):
    swgSplitMeshFourQuadrants.run(axis_pair=('x', 'y'))
"""

from __future__ import annotations

import maya.api.OpenMaya as om2
import maya.cmds as cmds

_AXIS_INDEX = {"x": 0, "y": 1, "z": 2}


def _shape_from_transform(node: str) -> str:
    shapes = cmds.listRelatives(node, s=True, ni=True, path=True, type="mesh")
    if not shapes:
        raise RuntimeError("Select a polygon mesh transform (no mesh shape found on '%s')." % node)
    return shapes[0]


def _bbox_center_world(node: str) -> tuple[float, float, float]:
    bb = cmds.xform(node, q=True, ws=True, boundingBox=True)
    cx = (bb[0] + bb[3]) * 0.5
    cy = (bb[1] + bb[4]) * 0.5
    cz = (bb[2] + bb[5]) * 0.5
    return cx, cy, cz


def _bbox_diagonal(bb: tuple[float, float, float, float, float, float]) -> float:
    dx = bb[3] - bb[0]
    dy = bb[4] - bb[1]
    dz = bb[5] - bb[2]
    return (dx * dx + dy * dy + dz * dz) ** 0.5


def _classify_faces(shape: str, center: tuple[float, float, float], ia: int, ib: int) -> dict[int, list[int]]:
    """Map quadrant 0..3 -> list of face indices (qa + 2*qb)."""
    buckets: dict[int, list[int]] = {0: [], 1: [], 2: [], 3: []}
    n = cmds.polyEvaluate(shape, face=True)
    c = (center[0], center[1], center[2])
    for fi in range(n):
        p = cmds.pointPosition("%s.f[%d]" % (shape, fi), w=True)
        qa = 1 if p[ia] >= c[ia] else 0
        qb = 1 if p[ib] >= c[ib] else 0
        q = qa + 2 * qb
        buckets[q].append(fi)
    return buckets


def _close_cut_holes(
    shape: str,
    center: tuple[float, float, float],
    ia: int,
    ib: int,
    bbox_diag: float,
    max_iters: int = 512,
) -> None:
    """
    Fill holes along the bisecting planes only (avoids capping unrelated open borders).
    Uses edge midpoints near the split planes; epsilon scales with mesh size.
    """
    eps = max(bbox_diag * 1.0e-5, 1.0e-4)
    ca = center[ia]
    cb = center[ib]
    for _ in range(max_iters):
        sl = om2.MSelectionList()
        sl.add(shape)
        dag = sl.getDagPath(0)
        dag.extendToShape()
        it = om2.MItMeshEdge(dag)
        pick: int | None = None
        while not it.isDone():
            if not it.onBoundary():
                it.next()
                continue
            pt = it.center(om2.MSpace.kWorld)
            on_cut = abs(pt[ia] - ca) < eps or abs(pt[ib] - cb) < eps
            if on_cut:
                pick = it.index()
                break
            it.next()
        if pick is None:
            return
        cmds.select("%s.e[%d]" % (shape, pick), r=True)
        cmds.polyCloseBorder(ch=False)


def _resolve_transform_from_selection() -> str:
    sel = cmds.ls(sl=True, long=True)
    if not sel:
        raise RuntimeError("Select one polygon mesh transform or shape.")
    node = sel[0]
    t = cmds.nodeType(node)
    if t == "mesh":
        parents = cmds.listRelatives(node, p=True, path=True, type="transform")
        if not parents:
            raise RuntimeError("Mesh has no transform parent.")
        return parents[0]
    if t == "transform":
        return node
    raise RuntimeError("Selection must be a mesh or transform (got '%s')." % t)


def run(axis_pair: tuple[str, str] | None = None, prefix: str = "quad") -> list[str]:
    """
    Split first selected mesh into four meshes, cap cut holes.

    axis_pair: e.g. ('x','z') for Y-up floor quadrants, ('x','y') for vertical XY split.
    """
    a, b = axis_pair or ("x", "z")
    a = a.lower()
    b = b.lower()
    if a not in _AXIS_INDEX or b not in _AXIS_INDEX:
        raise ValueError("axis_pair entries must be 'x', 'y', or 'z'.")
    if a == b:
        raise ValueError("axis_pair must be two different axes.")
    ia = _AXIS_INDEX[a]
    ib = _AXIS_INDEX[b]

    root = _resolve_transform_from_selection()
    shape = _shape_from_transform(root)
    center = _bbox_center_world(root)
    bb = cmds.xform(root, q=True, ws=True, boundingBox=True)
    bbox_diag = _bbox_diagonal(bb)
    faces_by_q = _classify_faces(shape, center, ia, ib)

    out: list[str] = []
    for q in range(4):
        if not faces_by_q[q]:
            cmds.warning("Quadrant %d has no faces; skipping." % q)
            continue
        dup = cmds.duplicate(root, rr=True, name="%s_%d" % (prefix, q))[0]
        dup_shape = _shape_from_transform(dup)
        nf = cmds.polyEvaluate(dup_shape, face=True)
        delete_faces = ["%s.f[%d]" % (dup_shape, j) for j in range(nf) if j not in set(faces_by_q[q])]
        if delete_faces:
            cmds.delete(delete_faces)
        dup_shape = _shape_from_transform(dup)
        _close_cut_holes(dup_shape, center, ia, ib, bbox_diag)
        out.append(dup)

    cmds.select(out, r=True)
    return out
