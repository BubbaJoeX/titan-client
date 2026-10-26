"""SWG Mocap Suite GUI — Kinect v2 live retarget onto imported SKT."""

from __future__ import annotations

import os
import sys
import time
from pathlib import Path
from typing import Dict, Optional

from . import __version__
from .kinect import KinectSource, SimulatorSource, open_kinect, probe_kinect
from .mapping import KINECT_JOINTS, auto_map, preset_for_names
from .maya_bridge import (
    apply_pose,
    attach_face_joints_to_head,
    data_root,
    enable_textured_viewport,
    export_ans,
    export_fbx,
    find_maya_joints,
    find_scene_sats,
    guess_sat_from_meshes,
    import_sat,
    import_skt,
    in_maya,
    prepare_imported_sat_viewport,
    rebind_swg_textures,
    revert_bind,
    fix_sat_viewport,
    set_key,
)
from .mesh import load_mesh_from_sat
from .motionbuilder import write_characterization
from .recorder import Clip
from .retarget import PoseSample, Retargeter
from .skt import SktSkeleton, graft_skeleton, load_skt, resolve_tree_skt, skti_pairs_from_sat
from .viewport import MocapViewport
from .voice import VoiceController

try:
    from PySide6 import QtCore, QtGui, QtWidgets
except ImportError:
    from PySide2 import QtCore, QtGui, QtWidgets  # type: ignore

_WINDOW = None


def _qt_app():
    app = QtWidgets.QApplication.instance()
    if app is None:
        app = QtWidgets.QApplication(sys.argv)
    return app


class MocapWindow(QtWidgets.QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle("SWG Mocap Suite  ·  Kinect v2 → SKT → ANS  v%s" % __version__)
        self.resize(1180, 720)
        self.source: Optional[KinectSource] = None
        self.skt: Optional[SktSkeleton] = None
        self.retarget: Optional[Retargeter] = None
        self.mapping: Dict[str, str] = {}
        self.maya_joints: Dict[str, str] = {}
        self.clip = Clip()
        self.recording = False
        self.live_maya = False
        self._last_t = time.perf_counter()
        self._frames = 0
        self._fps_t = time.perf_counter()
        self._fps = 0.0

        root = QtWidgets.QWidget()
        self.setCentralWidget(root)
        split = QtWidgets.QSplitter()
        split.addWidget(self._build_controls())
        self.view = MocapViewport()
        split.addWidget(self.view)
        split.setStretchFactor(1, 1)
        lay = QtWidgets.QHBoxLayout(root)
        lay.setContentsMargins(8, 8, 8, 8)
        lay.addWidget(split)

        self.timer = QtCore.QTimer(self)
        self.timer.setInterval(33)
        self.timer.timeout.connect(self._tick)
        self.voice = VoiceController(self)
        self.voice.command.connect(self._on_voice_command)
        self.voice.status.connect(self._log)

        self._log("Import the SAT in Maya (SwgSat / importSat), then this suite adopts it — mesh, MAIN textures, SKT.")
        self._log("Voice: connect, calibrate, record, stop, use maya sat, live apply, export ans.")
        QtCore.QTimer.singleShot(0, self._startup)

    def _build_controls(self) -> QtWidgets.QWidget:
        w = QtWidgets.QWidget()
        w.setMaximumWidth(400)
        v = QtWidgets.QVBoxLayout(w)
        v.setSpacing(6)

        g = QtWidgets.QGroupBox("Device")
        f = QtWidgets.QFormLayout(g)
        self.backend_lbl = QtWidgets.QLabel("disconnected")
        self.backend_lbl.setWordWrap(True)
        self.sdk_lbl = QtWidgets.QLabel("probing…")
        self.sdk_lbl.setWordWrap(True)
        self.body_spin = QtWidgets.QSpinBox()
        self.body_spin.setRange(0, 5)
        self.sim_chk = QtWidgets.QCheckBox("Allow simulator if no Kinect")
        self.sim_chk.setChecked(False)
        self.voice_chk = QtWidgets.QCheckBox("Voice commands (SAPI / Kinect mic)")
        self.voice_chk.toggled.connect(self._toggle_voice)
        btn_row = QtWidgets.QHBoxLayout()
        self.btn_connect = QtWidgets.QPushButton("Connect Kinect v2")
        self.btn_disconnect = QtWidgets.QPushButton("Disconnect")
        self.btn_connect.clicked.connect(self.connect_device)
        self.btn_disconnect.clicked.connect(self.disconnect_device)
        btn_row.addWidget(self.btn_connect)
        btn_row.addWidget(self.btn_disconnect)
        f.addRow(btn_row)
        f.addRow("SDK", self.sdk_lbl)
        f.addRow("Backend", self.backend_lbl)
        f.addRow("Body index", self.body_spin)
        f.addRow(self.sim_chk)
        f.addRow(self.voice_chk)
        v.addWidget(g)

        g = QtWidgets.QGroupBox("SWG skeleton")
        f = QtWidgets.QFormLayout(g)
        self.skt_lbl = QtWidgets.QLabel("(none)")
        self.skt_lbl.setWordWrap(True)
        b = QtWidgets.QHBoxLayout()
        b1 = QtWidgets.QPushButton("Import SKT…")
        b2 = QtWidgets.QPushButton("Import SAT…")
        b3 = QtWidgets.QPushButton("Load SKT file…")
        b4 = QtWidgets.QPushButton("Use Maya SAT")
        b1.clicked.connect(lambda: self._pick_import("skt"))
        b2.clicked.connect(lambda: self._pick_import("sat"))
        b3.clicked.connect(lambda: self._pick_import("file"))
        b4.clicked.connect(self.adopt_maya_scene)
        b5 = QtWidgets.QPushButton("Fix Maya textures")
        b5.setToolTip("VP2 + opaque MAIN PNG. Use if the Maya view is neon green.")
        b5.clicked.connect(self._fix_maya_textures)
        b.addWidget(b1)
        b.addWidget(b2)
        b.addWidget(b3)
        f.addRow(b)
        f.addRow(b4)
        f.addRow(b5)
        f.addRow("Loaded", self.skt_lbl)
        v.addWidget(g)

        g = QtWidgets.QGroupBox("Kinect → SKT map")
        gl = QtWidgets.QVBoxLayout(g)
        self.map_table = QtWidgets.QTableWidget(0, 2)
        self.map_table.setHorizontalHeaderLabels(["Kinect v2", "SWG joint"])
        self.map_table.horizontalHeader().setStretchLastSection(True)
        self.map_table.setMaximumHeight(220)
        gl.addWidget(self.map_table)
        mb = QtWidgets.QHBoxLayout()
        ba = QtWidgets.QPushButton("Auto map")
        ba.clicked.connect(self.rebuild_map)
        mb.addWidget(ba)
        gl.addLayout(mb)
        v.addWidget(g)

        g = QtWidgets.QGroupBox("Retarget (1:1)")
        f = QtWidgets.QFormLayout(g)
        self.scale = QtWidgets.QDoubleSpinBox()
        self.scale.setRange(0.2, 4.0)
        self.scale.setValue(1.0)
        self.scale.setSingleStep(0.05)
        self.smooth = QtWidgets.QSlider(QtCore.Qt.Horizontal)
        self.smooth.setRange(0, 90)
        self.smooth.setValue(35)
        self.lock_root = QtWidgets.QCheckBox("Lock root translation")
        self.floor = QtWidgets.QCheckBox("Plant root on floor after calibrate")
        self.floor.setChecked(True)
        self.mirror = QtWidgets.QCheckBox("Mirror X")
        self.live_maya_chk = QtWidgets.QCheckBox("Live apply to Maya joints")
        self.live_maya_chk.setChecked(in_maya())
        self.show_mesh_chk = QtWidgets.QCheckBox("Live render textured mesh")
        self.show_mesh_chk.setChecked(True)
        self.show_tex_chk = QtWidgets.QCheckBox("Use SAT MAIN textures")
        self.show_tex_chk.setChecked(True)
        self.show_skel_chk = QtWidgets.QCheckBox("Show SKT overlay")
        self.show_skel_chk.setChecked(True)
        self.show_kin_chk = QtWidgets.QCheckBox("Show Kinect overlay")
        self.show_kin_chk.setChecked(True)
        for chk in (self.show_mesh_chk, self.show_tex_chk, self.show_skel_chk, self.show_kin_chk):
            chk.toggled.connect(self._sync_view_flags)
        f.addRow("Actor scale", self.scale)
        f.addRow("Smooth", self.smooth)
        f.addRow(self.lock_root)
        f.addRow(self.floor)
        f.addRow(self.mirror)
        f.addRow(self.live_maya_chk)
        f.addRow(self.show_mesh_chk)
        f.addRow(self.show_tex_chk)
        f.addRow(self.show_skel_chk)
        f.addRow(self.show_kin_chk)
        cal = QtWidgets.QPushButton("Calibrate T-pose")
        cal.clicked.connect(self.calibrate)
        f.addRow(cal)
        v.addWidget(g)

        g = QtWidgets.QGroupBox("Record / export")
        f = QtWidgets.QVBoxLayout(g)
        row = QtWidgets.QHBoxLayout()
        self.btn_rec = QtWidgets.QPushButton("Record")
        self.btn_stop = QtWidgets.QPushButton("Stop")
        self.btn_rec.clicked.connect(self.start_record)
        self.btn_stop.clicked.connect(self.stop_record)
        row.addWidget(self.btn_rec)
        row.addWidget(self.btn_stop)
        f.addLayout(row)
        self.rec_lbl = QtWidgets.QLabel("0 frames")
        f.addWidget(self.rec_lbl)
        self.btn_ans = QtWidgets.QPushButton("Bake + export ANS…")
        self.btn_fbx = QtWidgets.QPushButton("Export FBX + MotionBuilder char…")
        self.btn_bind = QtWidgets.QPushButton("Revert bind pose")
        self.btn_ans.clicked.connect(self.do_export_ans)
        self.btn_fbx.clicked.connect(self.do_export_mobu)
        self.btn_bind.clicked.connect(lambda: revert_bind())
        f.addWidget(self.btn_ans)
        f.addWidget(self.btn_fbx)
        f.addWidget(self.btn_bind)
        v.addWidget(g)

        self.log = QtWidgets.QPlainTextEdit()
        self.log.setReadOnly(True)
        self.log.setMaximumHeight(140)
        v.addWidget(self.log, 1)
        return w

    def _log(self, msg: str) -> None:
        self.log.appendPlainText(msg)

    def _startup(self) -> None:
        probe = probe_kinect()
        self.sdk_lbl.setText(probe.message)
        self._log(probe.message)
        if probe.devices:
            self._log("Devices: " + "; ".join(probe.devices))
        if probe.pykinect_error:
            self._log("pykinect2 skipped (%s) — using Kinect20.dll." % probe.pykinect_error)
        if in_maya():
            if self.adopt_maya_scene() and probe.dll_ok:
                self.connect_device()
        elif probe.dll_ok:
            self.connect_device()

    def _toggle_voice(self, on: bool) -> None:
        if on:
            self.voice.start()
        else:
            self.voice.stop()
            self._log("Voice off.")

    def _on_voice_command(self, cmd: str, phrase: str) -> None:
        self._log("Voice “%s” → %s" % (phrase, cmd))
        if cmd == "connect":
            self.connect_device()
        elif cmd == "disconnect":
            self.disconnect_device()
        elif cmd == "calibrate":
            self.calibrate()
        elif cmd == "record":
            self.start_record()
        elif cmd == "stop":
            self.stop_record()
        elif cmd == "export ans":
            self.do_export_ans()
        elif cmd == "export fbx":
            self.do_export_mobu()
        elif cmd == "adopt":
            self.adopt_maya_scene()
        elif cmd == "live on":
            self.live_maya_chk.setChecked(True)
        elif cmd == "live off":
            self.live_maya_chk.setChecked(False)
        elif cmd == "mesh on":
            self.show_mesh_chk.setChecked(True)
        elif cmd == "mesh off":
            self.show_mesh_chk.setChecked(False)
        elif cmd == "simulator":
            self.sim_chk.setChecked(True)
            self.connect_device()
        elif cmd == "revert":
            revert_bind()

    def _fix_maya_textures(self) -> None:
        if not in_maya():
            self._log("Not inside Maya.")
            return
        self._log(fix_sat_viewport())

    def adopt_maya_scene(self) -> bool:
        if not in_maya():
            self._log("Not inside Maya. Import SAT from the file buttons, or run swgMocapSuite after importSat.")
            return False
        rebound = prepare_imported_sat_viewport()
        hits = find_scene_sats()
        sat_path = ""
        for h in hits:
            if h.get("sat"):
                sat_path = h["sat"]
                self._log("Maya SAT %s  (node %s)" % (sat_path, h["name"]))
                break
            if h.get("skeletons") and not sat_path:
                rel = h["skeletons"][0]
                skt = resolve_tree_skt(rel, data_root() + "/appearance/x.sat")
                if skt:
                    self._log("Maya SKT from SAT metadata: %s" % skt)
                    self._load_skt(skt)
        if not sat_path:
            guesses = guess_sat_from_meshes()
            if guesses:
                sat_path = guesses[0]
                self._log("SAT from imported mesh name: %s" % sat_path)
        if not sat_path:
            if self.skt:
                self._log("SKT adopted from Maya; no SAT path for live mesh.")
                return True
            self._log("No imported SAT in this scene. File>Import type SwgSat (not ACIS), then Use Maya SAT.")
            return False
        self._attach_sat(sat_path, import_into_maya=False)
        self._log("Adopted SAT. Rebound %d file textures. Viewport is shaded+textured (Maya 6)." % rebound)
        return True

    def connect_device(self) -> None:
        self.disconnect_device()
        try:
            self.source = open_kinect(self.body_spin.value(), self.sim_chk.isChecked())
        except Exception as e:
            self._log("Kinect open failed: %s" % e)
            probe = probe_kinect()
            self.sdk_lbl.setText(probe.message)
            self._log(probe.message)
            if self.sim_chk.isChecked():
                self.source = SimulatorSource()
            else:
                self.backend_lbl.setText("no sensor")
                return
        self.backend_lbl.setText(self.source.backend)
        self.timer.start()
        if self.source.backend == "simulator":
            self._log("Simulator (no live body). Kinect20.dll is the real backend — uncheck simulator and Connect.")
        else:
            self._log("Connected %s. Stand in the Kinect view, then Calibrate T-pose." % self.source.backend)

    def disconnect_device(self) -> None:
        self.timer.stop()
        if self.source:
            self.source.close()
        self.source = None
        self.backend_lbl.setText("disconnected")

    def _pick_import(self, kind: str) -> None:
        root = data_root()
        start = str(Path(root) / "appearance" / ("skeleton" if kind != "sat" else ""))
        if kind == "sat":
            start = str(Path(root) / "appearance")
            filt = "SWG SAT (*.sat)"
        else:
            filt = "SWG SKT (*.skt)"
        path, _ = QtWidgets.QFileDialog.getOpenFileName(self, "Open SWG asset", start, filt)
        if not path:
            return
        try:
            if kind == "sat":
                self._attach_sat(path, import_into_maya=in_maya())
            else:
                if in_maya() and kind == "skt":
                    import_skt(path)
                    self._log("importSkeleton %s" % path)
                self._load_skt(path)
        except Exception as e:
            self._log("Import failed: %s" % e)

    def _attach_sat(self, sat_path: str, import_into_maya: bool) -> None:
        if import_into_maya and in_maya():
            import_sat(sat_path)
            rebound = rebind_swg_textures()
            enable_textured_viewport()
            self._log("importSat %s  rebound %d file textures" % (sat_path, rebound))
        pairs = skti_pairs_from_sat(sat_path)
        resolved = None
        grafts: list = []
        for rel, attach in pairs:
            path = resolve_tree_skt(rel, sat_path)
            if not path:
                continue
            if resolved is None:
                resolved = path
            else:
                att = attach or ("Head" if "face" in rel.lower() else "")
                if att:
                    grafts.append((path, att))
        if not resolved:
            resolved = self._skt_from_sat_dialog(data_root())
        if resolved:
            self._load_skt(resolved)
            for fpath, attach in grafts:
                extra = load_skt(fpath)
                self.skt = graft_skeleton(self.skt, extra, attach)
                self._log("Grafted %s onto %s (%d joints)" % (
                    extra.basename, attach, len(self.skt.joints)))
            if grafts:
                self.skt_lbl.setText("%s  (%d joints)" % (self.skt.basename, len(self.skt.joints)))
                self._make_retarget()
                self.view.skt = self.skt
                self._invalidate_bind()
                if in_maya():
                    self.maya_joints = find_maya_joints(self.skt)
            if in_maya():
                attached = attach_face_joints_to_head()
                if attached:
                    self._log("Maya face_root parented under Head: %s" % attached)
                    self.maya_joints = find_maya_joints(self.skt)
        else:
            self._log("SAT has no SKTI skeleton — mesh preview uses bind verts.")
        mesh = load_mesh_from_sat(sat_path)
        self.view.mesh = mesh
        if mesh:
            self._log("Live mesh %d verts %d tris  shaders=%s  textures=%s" % (
                len(mesh.positions), len(mesh.tris),
                ",".join(mesh.shader_names),
                ",".join("yes" if t is not None else "no" for t in mesh.textures)))
            self._ensure_bind_sample()
            self._sync_view_flags()
        else:
            self._log("SAT mesh parse failed for %s" % sat_path)

    def _skt_from_sat_dialog(self, root: str) -> Optional[str]:
        path, _ = QtWidgets.QFileDialog.getOpenFileName(
            self, "SKT used by that SAT (for live FK)", str(Path(root) / "appearance" / "skeleton"),
            "SWG SKT (*.skt)")
        return path or None

    def _load_skt(self, path: str) -> None:
        self.skt = load_skt(path)
        self.skt_lbl.setText("%s  (%d joints)" % (self.skt.basename, len(self.skt.joints)))
        self.mapping = preset_for_names(self.skt.short_names())
        if not self.mapping:
            self.mapping = auto_map(self.skt.short_names())
        self._fill_table()
        self._make_retarget()
        self.view.skt = self.skt
        if in_maya():
            self.maya_joints = find_maya_joints(self.skt)
            self._log("Maya joints matched: %d / %d" % (len(self.maya_joints), len(self.skt.joints)))
        self._log("SKT %s  joints=%s" % (path, ", ".join(self.skt.short_names())))

    def _fill_table(self) -> None:
        names = self.skt.short_names() if self.skt else []
        self.map_table.setRowCount(len(KINECT_JOINTS))
        for i, kj in enumerate(KINECT_JOINTS):
            self.map_table.setItem(i, 0, QtWidgets.QTableWidgetItem(kj))
            combo = QtWidgets.QComboBox()
            combo.addItem("")
            combo.addItems(names)
            cur = self.mapping.get(kj, "")
            ix = combo.findText(cur)
            if ix >= 0:
                combo.setCurrentIndex(ix)
            combo.currentTextChanged.connect(lambda _t, k=kj: self._on_map_edit())
            self.map_table.setCellWidget(i, 1, combo)

    def _on_map_edit(self) -> None:
        m: Dict[str, str] = {}
        for i, kj in enumerate(KINECT_JOINTS):
            w = self.map_table.cellWidget(i, 1)
            if w and w.currentText():
                m[kj] = w.currentText()
        self.mapping = m
        self._make_retarget()

    def rebuild_map(self) -> None:
        if not self.skt:
            return
        self.mapping = auto_map(self.skt.short_names())
        self._fill_table()
        self._make_retarget()

    def _make_retarget(self) -> None:
        if not self.skt:
            self.retarget = None
            return
        self.retarget = Retargeter(self.skt, self.mapping)
        self._sync_retarget_opts()
        drives = getattr(self.retarget, "_drives", [])
        if drives:
            bits = ["%s→%s rotates %s (aim %s)" % (d.k_parent, d.k_child, d.joint.name, d.bone_child.name)
                    for d in drives]
            self._log("Retarget drives (%d): %s" % (len(drives), "; ".join(bits)))
            locks = sorted(getattr(self.retarget, "_lock", []))
            if locks:
                self._log("Bind-locked: %s" % ", ".join(locks))

    def _sync_retarget_opts(self) -> None:
        if self.retarget:
            self.retarget.scale = float(self.scale.value())
            self.retarget.smooth = self.smooth.value() / 100.0
            self.retarget.lock_root = self.lock_root.isChecked()
            self.retarget.floor_y = self.floor.isChecked()
            self.retarget.mirror = self.mirror.isChecked()
        self._sync_view_flags()

    def _sync_view_flags(self) -> None:
        self.view.show_mesh = self.show_mesh_chk.isChecked()
        self.view.show_textures = self.show_tex_chk.isChecked()
        self.view.show_skt = self.show_skel_chk.isChecked()
        self.view.show_kinect = self.show_kin_chk.isChecked()
        self.view.update()

    def _invalidate_bind(self) -> None:
        self.view.sample = None
        self.view.bind_world = {}
        self.view.bind_rot = {}

    def _ensure_bind_sample(self, force: bool = False) -> None:
        if not force and self.view.sample and self.view.sample.swg_world:
            return
        from .retarget import PoseSample, fk_pose
        sample = PoseSample()
        if self.skt:
            sample.swg_world, sample.swg_rot = fk_pose(self.skt, sample)
        self.view.sample = sample
        self.view.bind_world = dict(sample.swg_world)
        self.view.bind_rot = dict(sample.swg_rot)

    def calibrate(self) -> None:
        if not self.retarget or not self.source:
            self._log("Connect Kinect and load an SKT first.")
            return
        frame = self.source.poll()
        if not frame or not frame.tracked:
            self._log("No tracked body to calibrate.")
            return
        self._sync_retarget_opts()
        self.retarget.calibrate(frame)
        self._log("T-pose captured. Identity delta = this stance vs SKT bind.")

    def start_record(self) -> None:
        self.clip.clear()
        self.recording = True
        self._log("Recording at 30 fps…")

    def stop_record(self) -> None:
        self.recording = False
        self._log("Recorded %d frames (%.2fs)" % (len(self.clip.frames), self.clip.seconds))

    def do_export_ans(self) -> None:
        if not in_maya():
            self._log("ANS export requires Maya + SwgMayaEditor.")
            return
        if not self.skt:
            self._log("Load SKT first.")
            return
        if not self.clip.frames:
            self._log("Record a clip first.")
            return
        dest, _ = QtWidgets.QFileDialog.getSaveFileName(
            self, "Export ANS", str(Path(data_root()) / "appearance" / "animation" / "mocap.ans"),
            "SWG ANS (*.ans)")
        if not dest:
            return
        self.maya_joints = find_maya_joints(self.skt)
        revert_bind()
        for i, sample in enumerate(self.clip.frames):
            set_key(self.skt, sample, self.maya_joints, float(i))
        try:
            import maya.cmds as cmds  # type: ignore
            cmds.playbackOptions(min=0, max=max(1, len(self.clip.frames) - 1))
            cmds.currentTime(0)
        except Exception:
            pass
        ok = export_ans(dest)
        self._log("ANS export %s → %s" % ("OK" if ok else "FAILED", dest))

    def do_export_mobu(self) -> None:
        if not self.skt:
            self._log("Load SKT first.")
            return
        dest, _ = QtWidgets.QFileDialog.getSaveFileName(
            self, "MotionBuilder characterization",
            str(Path(os.environ.get("TEMP", ".")) / "swg_mocap" / (self.skt.basename + ".swgchar.json")),
            "SWG char (*.swgchar.json)")
        if not dest:
            return
        char = write_characterization(self.skt, self.mapping, dest)
        self._log("Wrote %s and swg_mocap_mobu.py" % char)
        fbx = str(Path(dest).with_suffix(".fbx"))
        if in_maya() and export_fbx(fbx):
            self._log("FBX %s" % fbx)
        else:
            self._log("FBX skipped (load fbxmaya in Maya, or import SAT then export FBX manually).")
        self._log("MotionBuilder: import FBX, set SWG_MOCAP_CHAR, run swg_mocap_mobu.py, plot, send FBX back, export SwgAns.")

    def _tick(self) -> None:
        if not self.source:
            return
        frame = self.source.poll()
        now = time.perf_counter()
        dt = max(1e-3, now - self._last_t)
        self._last_t = now
        self._frames += 1
        if now - self._fps_t >= 0.5:
            self._fps = self._frames / (now - self._fps_t)
            self._frames = 0
            self._fps_t = now

        sample: Optional[PoseSample] = None
        if frame and self.retarget:
            self._sync_retarget_opts()
            if frame.tracked:
                sample = self.retarget.apply(frame, dt)
                if self.recording:
                    self.clip.append(sample)
                    self.rec_lbl.setText("%d frames  %.2fs" % (len(self.clip.frames), self.clip.seconds))
                if self.live_maya_chk.isChecked() and in_maya() and self.skt:
                    if not self.maya_joints:
                        self.maya_joints = find_maya_joints(self.skt)
                    apply_pose(self.skt, sample, self.maya_joints)

        self.view.kinect = frame
        if sample:
            self.view.sample = sample
        elif self.view.mesh and self.skt and (not self.view.sample or not self.view.sample.swg_world):
            self._ensure_bind_sample()
        self.view.skt = self.skt
        backend = self.source.backend if self.source else "?"
        tracked = "tracked" if frame and frame.tracked else "no body"
        self.view.set_status("%s  %s  joints=%s" % (backend, tracked, len(self.skt.joints) if self.skt else 0), self._fps)

    def closeEvent(self, e) -> None:
        self.voice.stop()
        self.disconnect_device()
        super().closeEvent(e)


def show() -> MocapWindow:
    global _WINDOW
    _qt_app()
    if _WINDOW is not None:
        try:
            _WINDOW.show()
            _WINDOW.raise_()
            return _WINDOW
        except Exception:
            _WINDOW = None
    _WINDOW = MocapWindow()
    _WINDOW.show()
    return _WINDOW


def main() -> None:
    app = _qt_app()
    show()
    if not in_maya():
        sys.exit(app.exec() if hasattr(app, "exec") else app.exec_())


if __name__ == "__main__":
    main()
