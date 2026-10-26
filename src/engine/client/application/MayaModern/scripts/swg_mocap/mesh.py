"""Load SAT → LMG/MGN + MAIN textures for the live textured viewport."""

from __future__ import annotations

import struct
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Tuple

from .math3d import Quat, Vec3, qmul, qrot, vadd, vscale
from .skt import _read_cstrings, _walk_iff, resolve_tree_file


def _qt_gui():
    try:
        from PySide6 import QtGui
        return QtGui
    except ImportError:
        from PySide2 import QtGui  # type: ignore
        return QtGui


@dataclass
class MeshTri:
    a: int
    b: int
    c: int
    ua: float
    va: float
    ub: float
    vb: float
    uc: float
    vc: float
    shader: int


@dataclass
class SwgMesh:
    positions: List[Vec3] = field(default_factory=list)
    weights: List[List[Tuple[int, float]]] = field(default_factory=list)
    joint_names: List[str] = field(default_factory=list)
    tris: List[MeshTri] = field(default_factory=list)
    textures: List[object] = field(default_factory=list)
    shader_names: List[str] = field(default_factory=list)
    texture_alpha: List[bool] = field(default_factory=list)


def _children(data: bytes, off: int, end: int) -> List[Tuple[str, bytes, int, int]]:
    out: List[Tuple[str, bytes, int, int]] = []
    while off + 8 <= end:
        tag = data[off:off + 4]
        size = struct.unpack(">I", data[off + 4:off + 8])[0]
        payload = off + 8
        nxt = payload + size
        if nxt > len(data) + 16:
            break
        if tag == b"FORM":
            out.append(("FORM", data[payload:payload + 4], payload + 4, nxt))
        else:
            out.append(("CHUNK", tag, payload, nxt))
        off = nxt
    return out


def _i32(data: bytes, o: int) -> int:
    return struct.unpack_from("<i", data, o)[0]


def sat_mesh_paths(sat_path: str) -> List[str]:
    data = Path(sat_path).read_bytes()
    acc: list = []
    _walk_iff(data, 0, len(data), acc)
    found: List[str] = []
    for _i, kind, tag, payload, nxt in acc:
        if kind != "CHUNK":
            continue
        raw = data[payload:nxt]
        if b".mgn" not in raw and b".lmg" not in raw and b"mesh/" not in raw:
            continue
        for s in _read_cstrings(raw):
            sl = s.replace("\\", "/").lower()
            if sl.endswith(".mgn") or sl.endswith(".lmg") or sl.startswith("appearance/mesh"):
                found.append(s.replace("\\", "/"))
    return found


def _resolve_mgn(rel: str, sat_path: str) -> Optional[str]:
    p = resolve_tree_file(rel, sat_path, (".mgn", ".lmg", ".msh"))
    if not p:
        return None
    low = p.lower()
    if low.endswith(".mgn"):
        return p
    inner = _first_mgn_string(p)
    if inner:
        got = resolve_tree_file(inner, p, (".mgn",)) or resolve_tree_file(inner, sat_path, (".mgn",))
        if got:
            return got
    sib = Path(p).with_suffix(".mgn")
    if sib.is_file():
        return str(sib)
    return None


def _first_mgn_string(path: str) -> Optional[str]:
    data = Path(path).read_bytes()
    acc: list = []
    _walk_iff(data, 0, len(data), acc)
    fallback = None
    for _i, kind, _tag, payload, nxt in acc:
        if kind != "CHUNK":
            continue
        for s in _read_cstrings(data[payload:nxt]):
            sl = s.replace("\\", "/").lower()
            if sl.endswith(".mgn"):
                return s.replace("\\", "/")
            if fallback is None and ("mesh/" in sl or sl.endswith(".lmg") or sl.startswith("appearance/")):
                fallback = s.replace("\\", "/")
    return fallback


def _sht_main_texture(sht_path: str) -> Optional[str]:
    data = Path(sht_path).read_bytes()
    acc: list = []
    _walk_iff(data, 0, len(data), acc)
    pending_tag = b""
    main = None
    first_tex = None
    for _i, kind, tag, payload, nxt in acc:
        if kind != "CHUNK":
            continue
        name = tag.decode("latin1", "replace")
        raw = data[payload:nxt]
        if name == "DATA" and len(raw) >= 4:
            pending_tag = raw[:4]
        elif name == "NAME" and (b"texture" in raw or b".dds" in raw or b".tga" in raw):
            s = _read_cstrings(raw)
            if not s:
                continue
            path = s[0].replace("\\", "/")
            skip = pending_tag in (
                b"MVNE", b"ENVM", b"KSAM", b"MASK", b"MRNC", b"CNRM", b"CEPS", b"SPEC",
            ) or pending_tag[::-1] in (b"ENVM", b"MASK", b"CNRM", b"SPEC")
            if first_tex is None and not skip:
                first_tex = path
            if pending_tag in (b"NIAM", b"MAIN") or pending_tag[::-1] == b"MAIN":
                main = path
    return main or first_tex


def _rgb565(c: int) -> Tuple[int, int, int]:
    r = ((c >> 11) & 31) * 255 // 31
    g = ((c >> 5) & 63) * 255 // 63
    b = (c & 31) * 255 // 31
    return r, g, b


def _decode_dxt1_block(block: bytes, out: bytearray, x: int, y: int, width: int, height: int) -> None:
    c0, c1, bits = struct.unpack_from("<HHI", block, 0)
    p0 = _rgb565(c0)
    p1 = _rgb565(c1)
    if c0 > c1:
        palette = (
            p0, p1,
            ((2 * p0[0] + p1[0]) // 3, (2 * p0[1] + p1[1]) // 3, (2 * p0[2] + p1[2]) // 3),
            ((p0[0] + 2 * p1[0]) // 3, (p0[1] + 2 * p1[1]) // 3, (p0[2] + 2 * p1[2]) // 3),
        )
        alphas = (255, 255, 255, 255)
    else:
        palette = (
            p0, p1,
            ((p0[0] + p1[0]) // 2, (p0[1] + p1[1]) // 2, (p0[2] + p1[2]) // 2),
            (0, 0, 0),
        )
        alphas = (255, 255, 255, 0)
    for py in range(4):
        yy = y + py
        if yy >= height:
            break
        for px in range(4):
            xx = x + px
            if xx >= width:
                continue
            idx = (bits >> (2 * (py * 4 + px))) & 3
            o = (yy * width + xx) * 4
            r, g, b = palette[idx]
            out[o:o + 4] = bytes((r, g, b, alphas[idx]))


def _decode_dxt5_alpha_block(block: bytes) -> List[int]:
    a0, a1 = block[0], block[1]
    bits = int.from_bytes(block[2:8], "little")
    if a0 > a1:
        pal = [a0, a1] + [((6 - i) * a0 + (i + 1) * a1) // 7 for i in range(6)]
    else:
        pal = [a0, a1] + [((4 - i) * a0 + (i + 1) * a1) // 5 for i in range(4)] + [0, 255]
    return [pal[(bits >> (3 * i)) & 7] for i in range(16)]


def _decode_dds(data: bytes):
    if data[:4] != b"DDS " or len(data) < 128:
        return None
    height, width = struct.unpack_from("<II", data, 12)
    if width <= 0 or height <= 0 or width > 4096 or height > 4096:
        return None
    fourcc = data[84:88]
    bits = struct.unpack_from("<I", data, 88)[0]
    if fourcc in (b"DXT1", b"DXT2", b"DXT3", b"DXT4", b"DXT5"):
        block_size = 8 if fourcc == b"DXT1" else 16
        src = data[128:]
        out = bytearray(width * height * 4)
        bx = (width + 3) // 4
        by = (height + 3) // 4
        off = 0
        need = bx * by * block_size
        if len(src) < need:
            return None
        for y in range(by):
            for x in range(bx):
                blk = src[off:off + block_size]
                off += block_size
                if fourcc == b"DXT1":
                    _decode_dxt1_block(blk, out, x * 4, y * 4, width, height)
                else:
                    alphas = _decode_dxt5_alpha_block(blk[:8])
                    _decode_dxt1_block(blk[8:], out, x * 4, y * 4, width, height)
                    for py in range(4):
                        yy = y * 4 + py
                        if yy >= height:
                            break
                        for px in range(4):
                            xx = x * 4 + px
                            if xx >= width:
                                continue
                            out[(yy * width + xx) * 4 + 3] = alphas[py * 4 + px]
        return width, height, bytes(out)
    if bits == 32:
        px = data[128:128 + width * height * 4]
        if len(px) < width * height * 4:
            return None
        rgba = bytearray(width * height * 4)
        for i in range(width * height):
            b, g, r, a = px[i * 4:i * 4 + 4]
            o = i * 4
            rgba[o:o + 4] = bytes((r, g, b, a))
        return width, height, bytes(rgba)
    return None


def _decode_tga(data: bytes):
    if len(data) < 18:
        return None
    id_len = data[0]
    img_type = data[2]
    width = data[12] | (data[13] << 8)
    height = data[14] | (data[15] << 8)
    bpp = data[16]
    desc = data[17]
    if img_type != 2 or bpp not in (24, 32) or width <= 0 or height <= 0:
        return None
    off = 18 + id_len
    stride = width * (bpp // 8)
    need = off + stride * height
    if len(data) < need:
        return None
    src = data[off:need]
    top_left = (desc & 0x20) != 0
    out = bytearray(width * height * 4)
    for y in range(height):
        sy = y if top_left else (height - 1 - y)
        row = src[sy * stride:(sy + 1) * stride]
        dst = y * width * 4
        if bpp == 32:
            for x in range(width):
                b, g, r, a = row[x * 4:x * 4 + 4]
                o = dst + x * 4
                out[o:o + 4] = bytes((r, g, b, a))
        else:
            for x in range(width):
                b, g, r = row[x * 3:x * 3 + 3]
                o = dst + x * 4
                out[o:o + 4] = bytes((r, g, b, 255))
    return width, height, bytes(out)


def shader_keeps_alpha(shader_name: str) -> bool:
    n = shader_name.replace("\\", "/").lower()
    return any(tok in n for tok in ("hair", "alpha", "leaf", "grass", "tree", "fence"))


def _texture_file(rel: str, sat_path: str) -> Optional[str]:
    """Prefer DDS — game TGAs keep DXT5 gloss in alpha; we decode RGB ourselves."""
    rel = rel.replace("\\", "/")
    if not Path(rel).suffix:
        rel += ".dds"
    dds = resolve_tree_file(rel, sat_path, (".dds",))
    if dds and dds.lower().endswith(".dds"):
        return dds
    return resolve_tree_file(rel, sat_path, (".tga", ".dds"))


def _flatten_rgba(rgba: bytes) -> bytes:
    out = bytearray(rgba)
    for i in range(3, len(out), 4):
        out[i] = 255
    return bytes(out)


def _load_image(path: str, flatten_alpha: bool = True):
    """Decode MAIN maps ourselves. Qt's QImage(DDS) is garbage; TGA alpha is gloss."""
    try:
        QtGui = _qt_gui()
    except Exception:
        QtGui = None
    if QtGui is None:
        return None
    p = Path(path)
    decoded = None
    if p.is_file():
        raw = p.read_bytes()
        ext = p.suffix.lower()
        if ext == ".dds":
            decoded = _decode_dds(raw)
        elif ext == ".tga":
            decoded = _decode_tga(raw)
    if decoded:
        width, height, rgba = decoded
        if flatten_alpha:
            rgba = _flatten_rgba(rgba)
        img = QtGui.QImage(rgba, width, height, width * 4, QtGui.QImage.Format_RGBA8888)
        return img.copy()
    img = QtGui.QImage(path)
    if img.isNull():
        return None
    img = img.convertToFormat(QtGui.QImage.Format_RGBA8888)
    if flatten_alpha:
        img = img.convertToFormat(QtGui.QImage.Format_RGB32).convertToFormat(QtGui.QImage.Format_RGBA8888)
    return img


def write_png(out_path: str, width: int, height: int, rgba: bytes, flatten: bool = True) -> str:
    """8-bit PNG. flatten=True writes RGB (no alpha) so Maya cannot treat gloss as opacity."""
    import struct
    import zlib

    def chunk(tag: bytes, data: bytes) -> bytes:
        crc = zlib.crc32(tag)
        crc = zlib.crc32(data, crc) & 0xFFFFFFFF
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", crc)

    raw = bytearray()
    for y in range(height):
        raw.append(0)
        src = y * width * 4
        for x in range(width):
            o = src + x * 4
            raw.extend(rgba[o:o + 3] if flatten else rgba[o:o + 4])
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 2 if flatten else 6, 0, 0, 0)
    dest = Path(out_path)
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_bytes(
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", ihdr)
        + chunk(b"IDAT", zlib.compress(bytes(raw), 6))
        + chunk(b"IEND", b""))
    return str(dest)


def write_maya_tga(out_path: str, width: int, height: int, rgba: bytes, flatten: bool = True) -> str:
    """Bottom-left 0x08 BGRA TGA. Never overwrite game texture/ in place."""
    header = bytearray(18)
    header[2] = 2
    header[12] = width & 255
    header[13] = (width >> 8) & 255
    header[14] = height & 255
    header[15] = (height >> 8) & 255
    header[16] = 32
    header[17] = 8
    dest = Path(out_path)
    dest.parent.mkdir(parents=True, exist_ok=True)
    payload = bytearray()
    for y in range(height - 1, -1, -1):
        src = y * width * 4
        for x in range(width):
            o = src + x * 4
            r, g, b, a = rgba[o], rgba[o + 1], rgba[o + 2], rgba[o + 3]
            payload.extend((b, g, r, 255 if flatten else a))
    dest.write_bytes(bytes(header) + bytes(payload))
    return str(dest)


def maya_safe_albedo_path(path: str, keep_alpha: bool = False) -> str:
    """Opaque RGB PNG for MAIN maps. Maya 2027 VP2 rejects DXT and misreads TGA alpha as opacity."""
    p = Path(path)
    if not p.is_file():
        return path
    if p.suffix.lower() == ".png" and "_opaque" in p.stem:
        return str(p)
    raw = p.read_bytes()
    ext = p.suffix.lower()
    decoded = _decode_dds(raw) if ext == ".dds" else _decode_tga(raw) if ext == ".tga" else None
    if not decoded:
        return path
    width, height, rgba = decoded
    if not keep_alpha:
        rgba = _flatten_rgba(rgba)
    import tempfile
    out_dir = Path(tempfile.gettempdir()) / "SwgMayaEditor" / "png"
    suffix = "_alpha" if keep_alpha else "_opaque"
    out = out_dir / (p.stem + suffix + ".png")
    return write_png(str(out), width, height, rgba, flatten=not keep_alpha)


def _companion_sats(sat_path: str) -> List[str]:
    """Hair / default wearable SAT sitting next to the body SAT (not a face rig)."""
    p = Path(sat_path)
    out: List[str] = []
    hair = p.with_name(p.stem + "_hair_s01.sat")
    if hair.is_file():
        out.append(str(hair))
    return out


def merge_meshes(meshes: List[SwgMesh]) -> SwgMesh:
    out = SwgMesh()
    name_to_i: Dict[str, int] = {}
    for mesh in meshes:
        voff = len(out.positions)
        jremap: List[int] = []
        for name in mesh.joint_names:
            key = name.lower()
            if key not in name_to_i:
                name_to_i[key] = len(out.joint_names)
                out.joint_names.append(name)
            jremap.append(name_to_i[key])
        out.positions.extend(mesh.positions)
        for wts in mesh.weights:
            remapped: List[Tuple[int, float]] = []
            for ji, w in wts:
                if 0 <= ji < len(jremap):
                    remapped.append((jremap[ji], w))
            out.weights.append(remapped)
        soff = len(out.textures)
        out.textures.extend(mesh.textures)
        out.shader_names.extend(mesh.shader_names)
        out.texture_alpha.extend(mesh.texture_alpha)
        for tri in mesh.tris:
            out.tris.append(MeshTri(
                tri.a + voff, tri.b + voff, tri.c + voff,
                tri.ua, tri.va, tri.ub, tri.vb, tri.uc, tri.vc,
                tri.shader + soff,
            ))
    return out


def load_mesh_from_sat(sat_path: str) -> Optional[SwgMesh]:
    """Load every SAT MSGN (body + head + hair companion), not just the first LMG."""
    rels = list(sat_mesh_paths(sat_path))
    for extra in _companion_sats(sat_path):
        rels.extend(sat_mesh_paths(extra))
    meshes: List[SwgMesh] = []
    seen = set()
    for rel in rels:
        mgn = _resolve_mgn(rel, sat_path)
        if not mgn:
            continue
        key = mgn.replace("\\", "/").lower()
        if key in seen:
            continue
        seen.add(key)
        mesh = load_mgn(mgn, sat_path)
        if mesh and mesh.positions and mesh.tris:
            meshes.append(mesh)
    if not meshes:
        return None
    return merge_meshes(meshes) if len(meshes) > 1 else meshes[0]


def load_mgn(mgn_path: str, tree_anchor: str = "") -> Optional[SwgMesh]:
    data = Path(mgn_path).read_bytes()
    acc: list = []
    _walk_iff(data, 0, len(data), acc)
    inner_off = inner_end = None
    for _i, kind, tag, payload, nxt in acc:
        if kind == "FORM" and tag in (b"0002", b"0003", b"0004"):
            inner_off, inner_end = payload, nxt
            break
    if inner_off is None:
        return None
    kids = _children(data, inner_off, inner_end)
    info = None
    for kind, tag, payload, nxt in kids:
        if kind == "CHUNK" and tag == b"INFO":
            info = data[payload:nxt]
            break
    if not info or len(info) < 36:
        return None
    _maxV, _maxS, skelN, xfnmN, posN, twdtN, _normN, shaderN, _bltN = struct.unpack_from("<9i", info, 0)
    mesh = SwgMesh()
    # Sequential named chunks
    by_tag: Dict[bytes, List[Tuple[int, int]]] = {}
    forms: List[Tuple[bytes, int, int]] = []
    for kind, tag, payload, nxt in kids:
        if kind == "CHUNK":
            by_tag.setdefault(tag, []).append((payload, nxt))
        else:
            forms.append((tag, payload, nxt))

    if b"XFNM" in by_tag:
        p, n = by_tag[b"XFNM"][0]
        mesh.joint_names = _read_cstrings(data[p:n])[:xfnmN]
    if b"POSN" in by_tag:
        p, n = by_tag[b"POSN"][0]
        raw = data[p:n]
        for i in range(posN):
            x, y, z = struct.unpack_from("<3f", raw, i * 12)
            mesh.positions.append((x, y, z))
    headers: List[int] = []
    if b"TWHD" in by_tag:
        p, n = by_tag[b"TWHD"][0]
        raw = data[p:n]
        headers = [_i32(raw, i * 4) for i in range(min(posN, len(raw) // 4))]
    weight_pairs: List[Tuple[int, float]] = []
    if b"TWDT" in by_tag:
        p, n = by_tag[b"TWDT"][0]
        raw = data[p:n]
        for i in range(min(twdtN, len(raw) // 8)):
            ji = _i32(raw, i * 8)
            w = struct.unpack_from("<f", raw, i * 8 + 4)[0]
            weight_pairs.append((ji, w))
    cursor = 0
    for h in headers:
        chunk = weight_pairs[cursor:cursor + max(0, h)]
        cursor += max(0, h)
        mesh.weights.append(chunk)
    while len(mesh.weights) < len(mesh.positions):
        mesh.weights.append([])

    # PSDT forms
    psdts = [(p, n) for t, p, n in forms if t == b"PSDT"]
    if not psdts:
        # nested: some files wrap PSDT as FORM name inside unnamed
        for t, p, n in forms:
            sub = _children(data, p, n)
            for sk, st, sp, sn in sub:
                if sk == "FORM" and st == b"PSDT":
                    psdts.append((sp, sn))
    # Walker may have listed PSDT at any depth
    if not psdts:
        for _i, kind, tag, payload, nxt in acc:
            if kind == "FORM" and tag == b"PSDT":
                psdts.append((payload, nxt))

    anchor = tree_anchor or mgn_path
    for si, (p, n) in enumerate(psdts[: max(1, shaderN)]):
        sub = _children(data, p, n)
        shader_name = ""
        pidx: List[int] = []
        uvs: List[Tuple[float, float]] = []
        tris: List[Tuple[int, int, int]] = []
        for sk, st, sp, sn in sub:
            raw = data[sp:sn]
            if sk == "CHUNK" and st == b"NAME":
                ss = _read_cstrings(raw)
                if ss:
                    shader_name = ss[0]
            elif sk == "CHUNK" and st == b"PIDX" and len(raw) >= 4:
                cnt = _i32(raw, 0)
                pidx = [_i32(raw, 4 + i * 4) for i in range(max(0, min(cnt, (len(raw) - 4) // 4)))]
            elif sk == "CHUNK" and st == b"TCSD" and not uvs:
                # first UV set: pairs of floats, count = len(pidx) if known
                nfloat = len(raw) // 4
                # TXCI may say dim=2 → 2 floats per vert
                if pidx:
                    dim = max(2, nfloat // max(1, len(pidx)))
                    for i in range(len(pidx)):
                        u = struct.unpack_from("<f", raw, i * dim * 4)[0]
                        v = struct.unpack_from("<f", raw, i * dim * 4 + 4)[0]
                        uvs.append((u, 1.0 - v))
            elif sk == "FORM" and st == b"TCSF":
                tcs = _children(data, sp, sn)
                for tk, tt, tp, tn in tcs:
                    if tk == "CHUNK" and tt == b"TCSD" and not uvs:
                        raw = data[tp:tn]
                        if pidx:
                            dim = max(2, (len(raw) // 4) // max(1, len(pidx)))
                            for i in range(len(pidx)):
                                u = struct.unpack_from("<f", raw, i * dim * 4)[0]
                                v = struct.unpack_from("<f", raw, i * dim * 4 + 4)[0]
                                uvs.append((u, 1.0 - v))
            elif sk == "FORM" and st == b"PRIM":
                prims = _children(data, sp, sn)
                for pk, pt, pp, pn in prims:
                    if pk != "CHUNK":
                        continue
                    raw = data[pp:pn]
                    # TAG3(I,T,L) is stored as b"ITL " (space-padded).
                    ptn = pt.rstrip()
                    if ptn == b"ITL" and len(raw) >= 4:
                        tc = _i32(raw, 0)
                        for t in range(max(0, min(tc, (len(raw) - 4) // 12))):
                            a, b, c = struct.unpack_from("<3i", raw, 4 + t * 12)
                            tris.append((a, b, c))
                    elif ptn == b"OITL" and len(raw) >= 4:
                        tc = _i32(raw, 0)
                        stride = 14
                        for t in range(max(0, min(tc, (len(raw) - 4) // stride))):
                            o = 4 + t * stride + 2
                            a, b, c = struct.unpack_from("<3i", raw, o)
                            tris.append((a, b, c))
        img = None
        keep_a = shader_keeps_alpha(shader_name)
        if shader_name:
            sht = resolve_tree_file(shader_name.replace("\\", "/"), anchor, (".sht",))
            if sht:
                texrel = _sht_main_texture(sht)
                if texrel:
                    texpath = _texture_file(texrel, anchor)
                    if texpath:
                        img = _load_image(texpath, flatten_alpha=not keep_a)
        mesh.shader_names.append(shader_name)
        mesh.textures.append(img)
        mesh.texture_alpha.append(keep_a)
        for a, b, c in tris:
            def uv_at(idx: int) -> Tuple[float, float]:
                if 0 <= idx < len(uvs):
                    return uvs[idx]
                return (0.0, 0.0)

            def pos_at(idx: int) -> int:
                if pidx and 0 <= idx < len(pidx):
                    return pidx[idx]
                return idx

            mesh.tris.append(MeshTri(
                pos_at(a), pos_at(b), pos_at(c),
                uv_at(a)[0], uv_at(a)[1], uv_at(b)[0], uv_at(b)[1], uv_at(c)[0], uv_at(c)[1],
                si,
            ))
    return mesh if mesh.positions and mesh.tris else None


def skin_positions(mesh: SwgMesh, joint_world: Dict[str, Vec3],
                   joint_rot: Dict[str, Quat], bind_world: Dict[str, Vec3],
                   bind_rot: Dict[str, Quat]) -> List[Vec3]:
    # MGN XFNM is lowercase (larm); SKT short names are mixed (lArm).
    live_w = {k.lower(): v for k, v in joint_world.items()}
    live_r = {k.lower(): v for k, v in joint_rot.items()}
    rest_w = {k.lower(): v for k, v in bind_world.items()}
    rest_r = {k.lower(): v for k, v in bind_rot.items()}
    name_of = {i: n.lower() for i, n in enumerate(mesh.joint_names)}
    from .math3d import qconj, vsub
    out: List[Vec3] = []
    for i, p in enumerate(mesh.positions):
        wts = mesh.weights[i] if i < len(mesh.weights) else []
        if not wts:
            out.append(p)
            continue
        acc = (0.0, 0.0, 0.0)
        wsum = 0.0
        for ji, w in wts:
            if w <= 0.0:
                continue
            jn = name_of.get(ji, "")
            lw = live_w.get(jn)
            lr = live_r.get(jn)
            bw = rest_w.get(jn)
            br = rest_r.get(jn)
            if not lw or not lr or not bw or not br:
                continue
            local = qrot(qconj(br), vsub(p, bw))
            acc = vadd(acc, vscale(vadd(lw, qrot(lr, local)), w))
            wsum += w
        if wsum > 1e-6:
            out.append(vscale(acc, 1.0 / wsum))
        else:
            out.append(p)
    return out
