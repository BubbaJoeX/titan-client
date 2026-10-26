"""Live 3D viewport: OpenGL textured mesh (fast) + Kinect / SKT overlay."""

from __future__ import annotations

import math
import struct
from typing import List, Optional, Tuple

from .kinect import BodyFrame
from .mapping import KINECT_BONES
from .math3d import Vec3, engine_vec_to_maya
from .mesh import SwgMesh, skin_positions
from .retarget import PoseSample, fk_pose
from .skt import SktSkeleton

try:
    from PySide6 import QtCore, QtGui, QtWidgets
except ImportError:
    from PySide2 import QtCore, QtGui, QtWidgets  # type: ignore

try:
    from PySide6.QtOpenGLWidgets import QOpenGLWidget
    from PySide6.QtOpenGL import QOpenGLBuffer, QOpenGLShader, QOpenGLShaderProgram, QOpenGLTexture
    _HAS_GL = True
except Exception:
    try:
        from PySide2.QtWidgets import QOpenGLWidget
        from PySide2.QtGui import QOpenGLBuffer, QOpenGLShader, QOpenGLShaderProgram, QOpenGLTexture
        _HAS_GL = True
    except Exception:
        QOpenGLWidget = QtWidgets.QWidget  # type: ignore
        _HAS_GL = False

_VS = """
attribute vec3 aPos;
attribute vec2 aUv;
uniform mat4 uMvp;
varying vec2 vUv;
void main() {
    vUv = aUv;
    gl_Position = uMvp * vec4(aPos, 1.0);
}
"""

_FS = """
varying vec2 vUv;
uniform sampler2D uTex;
uniform vec3 uTint;
uniform float uHasTex;
uniform float uAlphaCut;
void main() {
    vec4 c = vec4(uTint, 1.0);
    if (uHasTex > 0.5)
        c = texture2D(uTex, vUv);
    if (c.a < uAlphaCut)
        discard;
    gl_FragColor = vec4(c.rgb, 1.0);
}
"""


def _lookat(ex, ey, ez, tx, ty, tz, ux, uy, uz) -> List[float]:
    fx, fy, fz = tx - ex, ty - ey, tz - ez
    fl = math.sqrt(fx * fx + fy * fy + fz * fz) or 1.0
    fx, fy, fz = fx / fl, fy / fl, fz / fl
    sx = fy * uz - fz * uy
    sy = fz * ux - fx * uz
    sz = fx * uy - fy * ux
    sl = math.sqrt(sx * sx + sy * sy + sz * sz) or 1.0
    sx, sy, sz = sx / sl, sy / sl, sz / sl
    rx = sy * fz - sz * fy
    ry = sz * fx - sx * fz
    rz = sx * fy - sy * fx
    return [
        sx, rx, -fx, 0.0,
        sy, ry, -fy, 0.0,
        sz, rz, -fz, 0.0,
        -(sx * ex + sy * ey + sz * ez),
        -(rx * ex + ry * ey + rz * ez),
        -(-fx * ex + -fy * ey + -fz * ez),
        1.0,
    ]


def _perspective(fovy_deg: float, aspect: float, znear: float, zfar: float) -> List[float]:
    f = 1.0 / math.tan(math.radians(fovy_deg) * 0.5)
    nf = 1.0 / (znear - zfar)
    return [
        f / max(0.05, aspect), 0, 0, 0,
        0, f, 0, 0,
        0, 0, (zfar + znear) * nf, -1,
        0, 0, (2 * zfar * znear) * nf, 0,
    ]


def _mul4(a: List[float], b: List[float]) -> List[float]:
    out = [0.0] * 16
    for c in range(4):
        for r in range(4):
            out[c * 4 + r] = (
                a[0 * 4 + r] * b[c * 4 + 0] +
                a[1 * 4 + r] * b[c * 4 + 1] +
                a[2 * 4 + r] * b[c * 4 + 2] +
                a[3 * 4 + r] * b[c * 4 + 3])
    return out


class MocapViewport(QOpenGLWidget if _HAS_GL else QtWidgets.QWidget):
    def __init__(self, parent=None) -> None:
        super().__init__(parent)
        self.setMinimumSize(520, 420)
        self.setFocusPolicy(QtCore.Qt.StrongFocus)
        self._yaw = 0.6
        self._pitch = 0.35
        self._dist = 3.4
        self._last = None
        self.kinect: Optional[BodyFrame] = None
        self.sample: Optional[PoseSample] = None
        self.skt: Optional[SktSkeleton] = None
        self.show_kinect = True
        self.show_skt = True
        self.show_mesh = True
        self.show_textures = True
        self.mesh: Optional[SwgMesh] = None
        self.bind_world = {}
        self.bind_rot = {}
        self._skinned = None
        self._fps = 0.0
        self._status = "idle"
        self._prog = None
        self._gl_texs: list = []
        self._mesh_id = None
        self._gl_ready = False

    def set_status(self, text: str, fps: float = 0.0) -> None:
        self._status = text
        self._fps = fps
        self.update()

    def mousePressEvent(self, e) -> None:
        self._last = e.position().toPoint() if hasattr(e, "position") else e.pos()

    def mouseMoveEvent(self, e) -> None:
        p = e.position().toPoint() if hasattr(e, "position") else e.pos()
        if self._last is None:
            self._last = p
            return
        dx = p.x() - self._last.x()
        dy = p.y() - self._last.y()
        self._yaw += dx * 0.01
        self._pitch = max(-1.2, min(1.2, self._pitch + dy * 0.01))
        self._last = p
        self.update()

    def wheelEvent(self, e) -> None:
        delta = e.angleDelta().y()
        self._dist = max(1.2, min(12.0, self._dist * (0.9 if delta > 0 else 1.1)))
        self.update()

    def initializeGL(self) -> None:
        if not _HAS_GL:
            return
        try:
            self._prog = QOpenGLShaderProgram(self)
            self._prog.addShaderFromSourceCode(QOpenGLShader.Vertex, _VS)
            self._prog.addShaderFromSourceCode(QOpenGLShader.Fragment, _FS)
            self._prog.bindAttributeLocation("aPos", 0)
            self._prog.bindAttributeLocation("aUv", 1)
            self._gl_ready = self._prog.link()
        except Exception:
            self._gl_ready = False

    def resizeGL(self, w: int, h: int) -> None:
        if _HAS_GL and self.context():
            fn = self.context().functions()
            fn.glViewport(0, 0, max(1, w), max(1, h))

    def _mvp(self) -> List[float]:
        cy, sy = math.cos(self._yaw), math.sin(self._yaw)
        cp, sp = math.cos(self._pitch), math.sin(self._pitch)
        tx, ty, tz = 0.0, 1.0, 0.0
        ex = tx + self._dist * sy * cp
        ey = ty + self._dist * sp
        ez = tz + self._dist * cy * cp
        view = _lookat(ex, ey, ez, tx, ty, tz, 0.0, 1.0, 0.0)
        proj = _perspective(45.0, max(0.2, self.width() / max(1.0, float(self.height()))), 0.15, 40.0)
        return _mul4(proj, view)

    def _ensure_gl_textures(self) -> None:
        if not self.mesh or not self._gl_ready:
            return
        mid = id(self.mesh)
        if mid == self._mesh_id and self._gl_texs:
            return
        self._gl_texs = []
        self._mesh_id = mid
        for img in self.mesh.textures:
            if img is None or img.isNull():
                self._gl_texs.append(None)
                continue
            tex = QOpenGLTexture(QOpenGLTexture.Target2D)
            q = img.convertToFormat(QtGui.QImage.Format_RGBA8888)
            tex.setData(q)
            tex.setMinificationFilter(QOpenGLTexture.Linear)
            tex.setMagnificationFilter(QOpenGLTexture.Linear)
            tex.setWrapMode(QOpenGLTexture.Repeat)
            self._gl_texs.append(tex)

    def _skinned_maya(self) -> List[Vec3]:
        if not self.bind_world and self.skt:
            empty = PoseSample()
            self.bind_world, self.bind_rot = fk_pose(self.skt, empty)
        if self.sample and self.sample.swg_world:
            live_w, live_r = self.sample.swg_world, self.sample.swg_rot
        else:
            live_w, live_r = self.bind_world, self.bind_rot
        try:
            if self.mesh and live_w and live_r:
                skinned = skin_positions(self.mesh, live_w, live_r, self.bind_world, self.bind_rot)
            else:
                skinned = list(self.mesh.positions) if self.mesh else []
        except Exception:
            skinned = list(self.mesh.positions) if self.mesh else []
        self._skinned = skinned
        return [engine_vec_to_maya(p) for p in skinned]

    def paintGL(self) -> None:
        if not _HAS_GL or not self._gl_ready:
            self._paint_fallback()
            return
        fn = self.context().functions()
        fn.glClearColor(0.07, 0.07, 0.08, 1.0)
        fn.glClear(0x00004000 | 0x00000100)  # COLOR | DEPTH
        fn.glEnable(0x0B71)  # DEPTH_TEST
        fn.glDisable(0x0B44)  # CULL_FACE

        mvp = self._mvp()
        self._prog.bind()
        loc_mvp = self._prog.uniformLocation("uMvp")
        loc_tex = self._prog.uniformLocation("uTex")
        loc_tint = self._prog.uniformLocation("uTint")
        loc_has = self._prog.uniformLocation("uHasTex")
        loc_cut = self._prog.uniformLocation("uAlphaCut")
        self._prog.setUniformValue(loc_mvp, QtGui.QMatrix4x4(*mvp).transposed())

        if self.show_mesh and self.mesh:
            self._draw_mesh_gl(fn, loc_tex, loc_tint, loc_has, loc_cut)

        self._prog.release()

        p = QtGui.QPainter(self)
        p.setRenderHint(QtGui.QPainter.Antialiasing, True)
        if self.show_kinect and self.kinect and self.kinect.tracked:
            self._draw_overlay_lines(p, True)
        if self.show_skt and self.sample and self.skt:
            self._draw_overlay_lines(p, False)
        p.setPen(QtGui.QColor(210, 210, 210))
        p.drawText(12, 20, "SWG live  ·  OpenGL MAIN textures  ·  cyan=Kinect  amber=SKT  ·  drag orbit")
        p.drawText(12, 38, "%s   %.1f fps" % (self._status, self._fps))
        if self.mesh:
            ntex = sum(1 for t in self.mesh.textures if t is not None)
            p.drawText(12, 56, "mesh %d verts  %d tris  MAIN %d/%d" % (
                len(self.mesh.positions), len(self.mesh.tris), ntex, len(self.mesh.textures)))
        p.end()

    def _draw_mesh_gl(self, fn, loc_tex, loc_tint, loc_has, loc_cut) -> None:
        verts = self._skinned_maya()
        npos = len(verts)
        if npos == 0:
            return
        self._ensure_gl_textures()
        nslot = max(1, len(self.mesh.textures))
        by_shader: List[List] = [[] for _ in range(nslot)]
        for tri in self.mesh.tris:
            if tri.a >= npos or tri.b >= npos or tri.c >= npos:
                continue
            si = tri.shader if 0 <= tri.shader < nslot else 0
            by_shader[si].append(tri)
        alphas = getattr(self.mesh, "texture_alpha", [])
        GL_FLOAT = 0x1406
        GL_TRIANGLES = 0x0004
        fn.glEnableVertexAttribArray(0)
        fn.glEnableVertexAttribArray(1)
        vbo = QOpenGLBuffer(QOpenGLBuffer.VertexBuffer)
        vbo.create()
        vbo.bind()
        for si, tris in enumerate(by_shader):
            if not tris:
                continue
            keep_a = 0 <= si < len(alphas) and alphas[si]
            tex = self._gl_texs[si] if si < len(self._gl_texs) else None
            has = 1.0 if (self.show_textures and tex is not None) else 0.0
            self._prog.setUniformValue(loc_has, has)
            self._prog.setUniformValue(loc_cut, 0.15 if keep_a else 0.0)
            self._prog.setUniformValue(loc_tint, 0.82, 0.76, 0.70)
            self._prog.setUniformValue(loc_tex, 0)
            packed = bytearray()
            for tri in tris:
                for idx, u, v in ((tri.a, tri.ua, tri.va), (tri.b, tri.ub, tri.vb), (tri.c, tri.uc, tri.vc)):
                    x, y, z = verts[idx]
                    packed.extend(struct.pack("<5f", x, y, z, u % 1.0, v % 1.0))
            vbo.allocate(bytes(packed))
            fn.glVertexAttribPointer(0, 3, GL_FLOAT, False, 20, 0)
            fn.glVertexAttribPointer(1, 2, GL_FLOAT, False, 20, 12)
            if has > 0.5:
                tex.bind()
            fn.glDrawArrays(GL_TRIANGLES, 0, len(tris) * 3)
            if has > 0.5:
                tex.release()
        vbo.release()
        vbo.destroy()
        fn.glDisableVertexAttribArray(0)
        fn.glDisableVertexAttribArray(1)

    def _project(self, p: Vec3) -> Optional[QtCore.QPointF]:
        m = engine_vec_to_maya(p)
        x, y, z = m[0], m[1] - 1.0, m[2]
        cy, sy = math.cos(self._yaw), math.sin(self._yaw)
        cp, sp = math.cos(self._pitch), math.sin(self._pitch)
        x1 = x * cy + z * sy
        z1 = -x * sy + z * cy
        y2 = y * cp - z1 * sp
        z2 = y * sp + z1 * cp
        z2 += self._dist
        if z2 < 0.15:
            return None
        f = 520.0 / z2
        w, h = float(self.width()), float(self.height())
        return QtCore.QPointF(w * 0.5 + x1 * f, h * 0.62 - y2 * f)

    def _draw_overlay_lines(self, p: QtGui.QPainter, kinect: bool) -> None:
        if kinect:
            joints = self.kinect.joints
            p.setPen(QtGui.QPen(QtGui.QColor(80, 200, 220), 2))
            p.setBrush(QtGui.QColor(80, 200, 220))
            for a, b in KINECT_BONES:
                if a not in joints or b not in joints:
                    continue
                pa = self._project(self.sample.kinect_world.get(a, joints[a].pos) if self.sample else joints[a].pos)
                pb = self._project(self.sample.kinect_world.get(b, joints[b].pos) if self.sample else joints[b].pos)
                if pa and pb:
                    p.drawLine(pa, pb)
            return
        world = self.sample.swg_world
        p.setPen(QtGui.QPen(QtGui.QColor(230, 170, 70), 2))
        for j in self.skt.joints:
            if j.parent < 0:
                continue
            parent = self.skt.joints[j.parent]
            a, b = world.get(parent.name), world.get(j.name)
            if not a or not b:
                continue
            pa, pb = self._project(a), self._project(b)
            if pa and pb:
                p.drawLine(pa, pb)

    def _paint_fallback(self) -> None:
        p = QtGui.QPainter(self)
        p.fillRect(self.rect(), QtGui.QColor(18, 18, 20))
        p.setPen(QtGui.QColor(200, 200, 200))
        p.drawText(12, 20, "SWG live  ·  OpenGL unavailable — install PySide OpenGL or use Maya VP2")
        p.drawText(12, 38, "%s   %.1f fps" % (self._status, self._fps))
        p.end()

    def paintEvent(self, e) -> None:
        if _HAS_GL and type(self) is not QtWidgets.QWidget:
            super().paintEvent(e)
        else:
            self._paint_fallback()
