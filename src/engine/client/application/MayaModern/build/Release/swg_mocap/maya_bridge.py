"""Drive Maya SWG joints and export ANS / FBX through MayaModern."""

from __future__ import annotations

from typing import Dict, List, Optional, Tuple

from .retarget import PoseSample
from .skt import SktSkeleton


def _cmds():
    try:
        import maya.cmds as cmds  # type: ignore
        return cmds
    except Exception:
        return None


def in_maya() -> bool:
    return _cmds() is not None


def data_root() -> str:
    cmds = _cmds()
    if not cmds:
        return r"D:/titan/data/sku.0/sys.client/compiled/game"
    try:
        r = cmds.getDataRootDir()
        if r:
            return r.replace("\\", "/")
    except Exception:
        pass
    return r"D:/titan/data/sku.0/sys.client/compiled/game"


def import_skt(path: str) -> str:
    cmds = _cmds()
    if not cmds:
        raise RuntimeError("Maya is not available")
    cmds.importSkeleton(i=path)
    return path


def import_sat(path: str) -> str:
    cmds = _cmds()
    if not cmds:
        raise RuntimeError("Maya is not available")
    cmds.importSat(i=path)
    rebind_swg_textures()
    enable_textured_viewport()
    return path


def _force_opaque_lambert(cmds, file_node: str) -> None:
    try:
        dests = cmds.listConnections(file_node + ".outColor", d=True, s=False) or []
    except Exception:
        dests = []
    for shader in dests:
        plug = shader + ".transparency"
        if cmds.objExists(plug):
            try:
                cmds.setAttr(plug, 0.0, 0.0, 0.0, type="double3")
            except Exception:
                pass
        for tplug in (shader + ".transparency", shader + ".outTransparency"):
            try:
                srcs = cmds.listConnections(tplug, s=True, d=False, p=True) or []
                for src in srcs:
                    cmds.disconnectAttr(src, tplug)
            except Exception:
                pass


def rebind_swg_textures() -> int:
    """Restore MAIN albedo, flatten gloss-alpha, force opaque Lambert, sRGB - Texture."""
    cmds = _cmds()
    if not cmds:
        return 0
    from .mesh import maya_safe_albedo_path, shader_keeps_alpha
    n = 0
    for node in cmds.ls(type="file") or []:
        plug = node + ".swgResolvedTexture"
        if not cmds.objExists(plug):
            continue
        try:
            tex = cmds.getAttr(plug) or ""
        except Exception:
            continue
        if not tex:
            continue
        low = tex.replace("\\", "/").lower()
        if any(tok in low for tok in ("cnrm", "nrml", "_n.", "spec", "envm", "mask")):
            continue
        keep_a = shader_keeps_alpha(tex)
        try:
            safe = maya_safe_albedo_path(tex, keep_alpha=keep_a)
        except Exception:
            safe = tex
        try:
            cmds.setAttr(node + ".fileTextureName", safe, type="string")
            if cmds.objExists(node + ".ignoreColorSpaceFileRules"):
                cmds.setAttr(node + ".ignoreColorSpaceFileRules", 1)
            if cmds.objExists(node + ".colorSpace"):
                cmds.setAttr(node + ".colorSpace", "sRGB - Texture", type="string")
            if cmds.objExists(node + ".alphaIsLuminance"):
                cmds.setAttr(node + ".alphaIsLuminance", 0)
            if cmds.objExists(node + ".disableFileLoad"):
                cmds.setAttr(node + ".disableFileLoad", 1)
                cmds.setAttr(node + ".disableFileLoad", 0)
            _force_opaque_lambert(cmds, node)
            n += 1
        except Exception:
            pass
    return n


def enable_textured_viewport() -> None:
    """VP2 shaded+textured. Arnold viewport override paints Lamberts neon green."""
    cmds = _cmds()
    if not cmds:
        return
    try:
        cmds.setAttr("defaultRenderGlobals.currentRenderer", "mayaHardware2", type="string")
    except Exception:
        pass
    try:
        cmds.DisplayShadedAndTextured()
    except Exception:
        pass
    try:
        for panel in cmds.getPanel(type="modelPanel") or []:
            try:
                cmds.modelEditor(panel, e=True, rendererName="vp2Renderer")
            except Exception:
                try:
                    cmds.modelEditor(panel, e=True, rnm="vp2Renderer")
                except Exception:
                    pass
            try:
                cmds.modelEditor(panel, e=True, useDefaultMaterial=False)
            except Exception:
                pass
            cmds.modelEditor(
                panel, e=True,
                displayAppearance="smoothShaded",
                displayTextures=True,
                udm=False,
            )
    except Exception:
        pass
    try:
        cmds.displayPref(wireframeOnShaded=False)
    except Exception:
        pass
    try:
        cmds.refresh(force=True)
    except Exception:
        pass


def revert_bind() -> None:
    cmds = _cmds()
    if not cmds:
        return
    try:
        cmds.swgRevertToBindPose()
    except Exception:
        pass


def _joint_short(path: str) -> str:
    short = path.split("|")[-1]
    if ":" in short:
        short = short.split(":")[-1]
    return short.split("__")[0]


def _find_joint_named(cmds, name: str) -> str:
    want = name.lower()
    for node in cmds.ls(type="joint", long=True) or []:
        if _joint_short(node).lower() == want:
            return node
    return ""


def _is_under(child: str, ancestor: str) -> bool:
    return bool(child) and bool(ancestor) and (
        child == ancestor or child.startswith(ancestor + "|"))


def _recapture_skin_bind(cmds, joints: List[str]) -> int:
    """After reparenting, bindPreMatrix must match the new world inverse."""
    n = 0
    joint_set = set(joints)
    for sc in cmds.ls(type="skinCluster") or []:
        try:
            infs = cmds.skinCluster(sc, q=True, inf=True) or []
        except Exception:
            continue
        long_infs = []
        for inf in infs:
            try:
                long_infs.append(cmds.ls(inf, long=True)[0])
            except Exception:
                long_infs.append(inf)
        for idx, inf in enumerate(long_infs):
            if inf not in joint_set and _joint_short(inf).lower() not in {
                    _joint_short(j).lower() for j in joints}:
                continue
            try:
                inv = cmds.getAttr(inf + ".worldInverseMatrix[0]")
                if inv and isinstance(inv[0], (list, tuple)):
                    inv = inv[0]
                cmds.setAttr("%s.bindPreMatrix[%d]" % (sc, idx), *inv, type="matrix")
                n += 1
            except Exception:
                pass
    return n


def attach_face_joints_to_head() -> str:
    """Parent face_root under Head and recapture skin bind (game SKTI attach).

    Face SKT locals stay at bind. If ImportSat left face_root at world origin,
    eyes/lips stay put when Head rotates. relative parent keeps SKT bind offsets.
    """
    cmds = _cmds()
    if not cmds:
        return ""
    head = _find_joint_named(cmds, "Head") or _find_joint_named(cmds, "head")
    face_root = _find_joint_named(cmds, "face_root")
    if not head or not face_root:
        return ""
    if _is_under(face_root, head):
        return face_root
    try:
        cmds.parent(face_root, head, relative=True)
    except Exception:
        try:
            cmds.parent(face_root, head)
            cmds.xform(face_root, objectSpace=True, translation=(0.0, 0.0, 0.0))
        except Exception:
            return ""
    try:
        descendants = cmds.listRelatives(face_root, ad=True, type="joint", fullPath=True) or []
    except Exception:
        descendants = []
    face_joints = [face_root] + list(descendants)
    _recapture_skin_bind(cmds, face_joints)
    return face_root


def find_maya_joints(skt: SktSkeleton) -> Dict[str, str]:
    """Map SKT short name → Maya DAG path."""
    cmds = _cmds()
    out: Dict[str, str] = {}
    if not cmds:
        return out
    try:
        nodes = cmds.ls(type="joint", long=True) or []
    except Exception:
        return out
    wanted = {j.name.lower(): j.name for j in skt.joints}
    for node in nodes:
        short = node.split("|")[-1]
        if ":" in short:
            short = short.split(":")[-1]
        base = short.split("__")[0]
        key = wanted.get(base.lower())
        if key and key not in out:
            out[key] = node
    return out


def apply_pose(skt: SktSkeleton, sample: PoseSample, joint_map: Dict[str, str]) -> int:
    cmds = _cmds()
    if not cmds:
        return 0
    from .math3d import engine_quat_to_maya_euler_deg
    n = 0
    # Write every SKT joint so leftover live rotates (clav / fingers / root) reset.
    for joint in skt.joints:
        path = joint_map.get(joint.name)
        if not path:
            continue
        euler = sample.joint_maya_euler.get(joint.name)
        if euler is None:
            euler = engine_quat_to_maya_euler_deg(joint.bind_q)
        try:
            cmds.setAttr(path + ".rotate", euler[0], euler[1], euler[2], type="double3")
            n += 1
        except Exception:
            pass
    root = joint_map.get("root")
    if root:
        try:
            t = sample.root_maya_t
            # Additive on current bind translate: set translate to bind + delta via get.
            bt = cmds.getAttr(root + ".translate")[0]
            # During live apply we overwrite rotate; translate uses bind *plus* delta
            # stored as Maya-space delta. ImportAnimation uses bind+delta. Read bind from SKT.
            rj = skt.joint_by_name("root")
            bind = (0.0, 0.0, 0.0)
            if rj:
                from .math3d import engine_vec_to_maya
                bind = engine_vec_to_maya(rj.bind_t)
            cmds.setAttr(root + ".translate", bind[0] + t[0], bind[1] + t[1], bind[2] + t[2], type="double3")
        except Exception:
            pass
    return n


def set_key(skt: SktSkeleton, sample: PoseSample, joint_map: Dict[str, str], frame: float) -> None:
    cmds = _cmds()
    if not cmds:
        return
    apply_pose(skt, sample, joint_map)
    for joint in skt.joints:
        path = joint_map.get(joint.name)
        if path:
            try:
                cmds.setKeyframe(path, attribute="rotate", t=frame)
            except Exception:
                pass
    root = joint_map.get("root")
    if root:
        try:
            cmds.setKeyframe(root, attribute="translate", t=frame)
        except Exception:
            pass


def export_ans(path: str) -> bool:
    cmds = _cmds()
    if not cmds:
        return False
    path = path.replace("\\", "/")
    try:
        cmds.file(path, force=True, options="", type="SwgAns", preserveReferences=True, exportSelected=False, exportAll=True)
        return True
    except Exception:
        try:
            cmds.file(path, force=True, type="SwgAns", exportAll=True)
            return True
        except Exception as e:
            print("export ANS failed:", e)
            return False


def export_fbx(path: str) -> bool:
    cmds = _cmds()
    if not cmds:
        return False
    path = path.replace("\\", "/")
    try:
        if not cmds.pluginInfo("fbxmaya", q=True, loaded=True):
            cmds.loadPlugin("fbxmaya")
        cmds.file(path, force=True, type="FBX export", exportAll=True)
        return True
    except Exception as e:
        print("export FBX failed:", e)
        return False


def scene_joint_names() -> List[str]:
    cmds = _cmds()
    if not cmds:
        return []
    return cmds.ls(type="joint") or []


def _attr_string(node: str, attr: str) -> str:
    cmds = _cmds()
    if not cmds or not cmds.objExists(node + "." + attr):
        return ""
    try:
        val = cmds.getAttr(node + "." + attr)
        return (val or "").replace("\\", "/")
    except Exception:
        return ""


def parse_smat_payload(payload: str) -> Tuple[List[str], List[str]]:
    """Return (mesh generator rels, skeleton template rels) from swgSmatRoundTrip."""
    meshes: List[str] = []
    skels: List[str] = []
    lines = [ln.rstrip("\r") for ln in (payload or "").splitlines()]
    i = 0
    while i < len(lines):
        parts = lines[i].split()
        if not parts:
            i += 1
            continue
        key = parts[0]
        if key == "mesh" and len(parts) >= 2:
            n = int(parts[1])
            i += 1
            for _ in range(n):
                if i < len(lines) and lines[i]:
                    meshes.append(lines[i].replace("\\", "/"))
                i += 1
            continue
        if key == "skel" and len(parts) >= 2:
            n = int(parts[1])
            i += 1
            for _ in range(n):
                if i < len(lines):
                    skels.append(lines[i].split("\t")[0].replace("\\", "/"))
                i += 1
            continue
        i += 1
    return meshes, skels


def find_scene_sats() -> List[dict]:
    """SAT appearances already imported into Maya (importSat metadata)."""
    cmds = _cmds()
    out: List[dict] = []
    if not cmds:
        return out
    root = data_root().replace("\\", "/")
    seen = set()
    nodes = []
    try:
        nodes = cmds.ls(long=True) or []
    except Exception:
        return out
    for node in nodes:
        payload = _attr_string(node, "swgSmatRoundTrip")
        src = _attr_string(node, "swgSatSourcePath")
        if not payload and not src:
            continue
        short = node.split("|")[-1]
        if ":" in short:
            short = short.split(":")[-1]
        meshes, skels = parse_smat_payload(payload)
        sat = src
        if not sat:
            cand = "%s/appearance/%s.sat" % (root, short)
            if __import__("os").path.isfile(cand):
                sat = cand
        key = (sat or short).lower()
        if key in seen:
            continue
        seen.add(key)
        out.append({
            "node": node,
            "name": short,
            "sat": sat,
            "meshes": meshes,
            "skeletons": skels,
        })
    return out


def guess_sat_from_meshes() -> List[str]:
    """If SAT metadata is missing, guess appearance/<mesh>.sat from imported meshes."""
    cmds = _cmds()
    found: List[str] = []
    if not cmds:
        return found
    root = data_root().replace("\\", "/")
    import os
    for mesh in cmds.ls(type="mesh", long=True) or []:
        short = mesh.split("|")[-1].split(":")[-1]
        if short.endswith("Shape"):
            short = short[:-5]
        if short.endswith("Orig"):
            continue
        cand = os.path.join(root, "appearance", short + ".sat")
        if os.path.isfile(cand):
            found.append(cand.replace("\\", "/"))
    return found


def _reconnect_file_to_lambert(cmds, file_node: str) -> None:
    try:
        dests = cmds.listConnections(file_node + ".outColor", d=True, s=False) or []
    except Exception:
        dests = []
    if dests:
        return
    short = file_node.split(":")[-1]
    base = short[:-4] if short.endswith("_tex") else short
    for cand in cmds.ls(type="lambert") or []:
        cs = cand.split(":")[-1]
        if cs == base or cs.startswith(base) or base.startswith(cs):
            try:
                cmds.connectAttr(file_node + ".outColor", cand + ".color", force=True)
                return
            except Exception:
                pass


def fix_sat_viewport() -> str:
    """Force VP2, bind opaque MAIN PNGs, reconnect file→lambert. Safe on a stale plugin."""
    cmds = _cmds()
    if not cmds:
        return "not in Maya"
    enable_textured_viewport()
    n = rebind_swg_textures()
    from .mesh import maya_safe_albedo_path, shader_keeps_alpha
    for node in cmds.ls(type="file") or []:
        path = ""
        for plug in (node + ".swgResolvedTexture", node + ".fileTextureName"):
            if cmds.objExists(plug):
                try:
                    path = cmds.getAttr(plug) or ""
                except Exception:
                    path = ""
                if path:
                    break
        if not path:
            continue
        low = path.replace("\\", "/").lower()
        if any(tok in low for tok in ("cnrm", "nrml", "spec", "envm", "mask", "_n.")):
            continue
        keep_a = shader_keeps_alpha(path) or shader_keeps_alpha(node)
        try:
            safe = maya_safe_albedo_path(path, keep_alpha=keep_a)
            cmds.setAttr(node + ".fileTextureName", safe, type="string")
            if cmds.objExists(node + ".ignoreColorSpaceFileRules"):
                cmds.setAttr(node + ".ignoreColorSpaceFileRules", 1)
            if cmds.objExists(node + ".colorSpace"):
                cmds.setAttr(node + ".colorSpace", "sRGB - Texture", type="string")
            if cmds.objExists(node + ".alphaIsLuminance"):
                cmds.setAttr(node + ".alphaIsLuminance", 0)
            if cmds.objExists(node + ".defaultColor"):
                cmds.setAttr(node + ".defaultColor", 0.82, 0.76, 0.70, type="double3")
            if cmds.objExists(node + ".disableFileLoad"):
                cmds.setAttr(node + ".disableFileLoad", 1)
                cmds.setAttr(node + ".disableFileLoad", 0)
            _force_opaque_lambert(cmds, node)
            _reconnect_file_to_lambert(cmds, node)
            n += 1
        except Exception:
            pass
    try:
        cmds.refresh(force=True)
    except Exception:
        pass
    return "rebound %d file nodes (opaque MAIN PNG + VP2)" % n


def prepare_imported_sat_viewport() -> int:
    msg = fix_sat_viewport()
    print("swg_mocap:", msg)
    return 1 if "rebound" in msg else 0
