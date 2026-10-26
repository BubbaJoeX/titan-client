"""Kinect v2 body stream.

pykinect2 asserts on Python 3.13 (tagSTATSTG 80 vs 72). This module talks to
Kinect20.dll directly via GetDefaultKinectSensor + COM vtables. Simulator is
only used when the user allows it and the SDK/sensor cannot open.
"""

from __future__ import annotations

import ctypes
import math
import os
import time
from ctypes import POINTER, WINFUNCTYPE, byref, c_int, c_long, c_ubyte, c_ulong, c_void_p, c_float

# Do not use ctypes.HRESULT as restype — E_PENDING would raise OSError.
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple

from .mapping import KINECT_JOINTS
from .math3d import Vec3

CONF_NONE = 0
CONF_LOW = 1
CONF_MED = 2
CONF_HIGH = 3

BODY_COUNT = 6
JOINT_COUNT = 25
E_PENDING = 0x8000000A
S_OK = 0


@dataclass
class KinectJoint:
    name: str
    pos: Vec3
    confidence: int = CONF_HIGH


@dataclass
class BodyFrame:
    body_index: int
    tracked: bool
    joints: Dict[str, KinectJoint] = field(default_factory=dict)
    timestamp: float = 0.0


@dataclass
class KinectProbe:
    dll_path: str = ""
    dll_ok: bool = False
    sensor_ok: bool = False
    available: bool = False
    pykinect_ok: bool = False
    pykinect_error: str = ""
    devices: List[str] = field(default_factory=list)
    message: str = ""


class KinectSource:
    def poll(self) -> Optional[BodyFrame]:
        raise NotImplementedError

    def close(self) -> None:
        pass

    @property
    def backend(self) -> str:
        return "none"


def _release(ptr: Optional[int]) -> None:
    if not ptr:
        return
    vt = ctypes.cast(c_void_p(ptr), POINTER(POINTER(c_void_p))).contents
    WINFUNCTYPE(c_ulong, c_void_p)(vt[2])(ptr)


def _vt(ptr: int, index: int, restype, *argtypes):
    vtbl = ctypes.cast(c_void_p(ptr), POINTER(POINTER(c_void_p))).contents
    return WINFUNCTYPE(restype, c_void_p, *argtypes)(vtbl[index])


class CameraSpacePoint(ctypes.Structure):
    _fields_ = (("X", c_float), ("Y", c_float), ("Z", c_float))


class KinectJointRec(ctypes.Structure):
    _fields_ = (
        ("JointType", c_int),
        ("Position", CameraSpacePoint),
        ("TrackingState", c_int),
    )


def _kinect_dll():
    last = None
    for name in ("Kinect20.dll", r"C:\Windows\System32\Kinect20.dll"):
        try:
            return ctypes.WinDLL(name), name
        except OSError as e:
            last = e
    raise OSError("Kinect20.dll not found (install Kinect for Windows SDK 2.0). %s" % last)


def probe_kinect() -> KinectProbe:
    p = KinectProbe()
    for cand in (
        r"C:\Windows\System32\Kinect20.dll",
        r"C:\Program Files\Microsoft SDKs\Kinect\v2.0_1409\Assemblies\Microsoft.Kinect.dll",
    ):
        if os.path.isfile(cand):
            p.dll_path = cand
            break
    try:
        dll, loaded = _kinect_dll()
        p.dll_ok = True
        p.dll_path = p.dll_path or loaded
    except OSError as e:
        p.message = str(e)
        return p

    try:
        from pykinect2 import PyKinectRuntime  # noqa: F401
        p.pykinect_ok = True
    except Exception as e:
        p.pykinect_error = "%s: %s" % (type(e).__name__, e)

    try:
        import subprocess
        out = subprocess.check_output(
            ["powershell", "-NoProfile", "-Command",
             "Get-PnpDevice | Where-Object { $_.FriendlyName -match 'Kinect|Xbox NUI' } | "
             "ForEach-Object { $_.Status + '|' + $_.FriendlyName }"],
            text=True, timeout=8, stderr=subprocess.DEVNULL)
        p.devices = [ln.strip() for ln in out.splitlines() if ln.strip()]
    except Exception:
        p.devices = []

    sensor = c_void_p()
    dll.GetDefaultKinectSensor.argtypes = [POINTER(c_void_p)]
    dll.GetDefaultKinectSensor.restype = c_long
    hr = dll.GetDefaultKinectSensor(byref(sensor))
    if hr != S_OK or not sensor.value:
        p.message = "GetDefaultKinectSensor failed HRESULT=0x%08X (SDK present, sensor not enumerated)." % (hr & 0xFFFFFFFF)
        return p
    p.sensor_ok = True
    try:
        avail = c_ubyte(0)
        _vt(sensor.value, 9, c_long, POINTER(c_ubyte))(sensor.value, byref(avail))
        p.available = bool(avail.value)
    finally:
        _release(sensor.value)
    live = any(d.upper().startswith("OK|") and "KINECT" in d.upper() or d.upper().startswith("OK|XBOX") for d in p.devices)
    if p.available:
        p.message = "Kinect v2 SDK + sensor ready (Kinect20.dll)."
    elif live or p.sensor_ok:
        p.message = "Kinect v2 SDK detected. Hardware enumerated. Connect to open the body stream (stand in view)."
        p.available = True
    else:
        p.message = "Kinect20.dll loaded but no live Kinect/Xbox NUI device. Check USB 3 and Kinect Service."
    return p


class SimulatorSource(KinectSource):
    """Procedural T-pose + walk so mapping / Maya apply work without a sensor."""

    def __init__(self) -> None:
        self.t0 = time.perf_counter()

    @property
    def backend(self) -> str:
        return "simulator"

    def poll(self) -> Optional[BodyFrame]:
        t = time.perf_counter() - self.t0
        walk = math.sin(t * 3.2)
        bob = abs(math.sin(t * 6.4)) * 0.02
        sway = math.sin(t * 1.6) * 0.03
        joints: Dict[str, KinectJoint] = {}

        def put(name: str, x: float, y: float, z: float, conf: int = CONF_HIGH) -> None:
            joints[name] = KinectJoint(name, (x + sway, y + bob, z), conf)

        put("SpineBase", 0.0, 0.95, 2.2)
        put("SpineMid", 0.0, 1.20, 2.2)
        put("SpineShoulder", 0.0, 1.42, 2.2)
        put("Neck", 0.0, 1.52, 2.2)
        put("Head", 0.0, 1.68, 2.2)
        put("ShoulderLeft", -0.18, 1.40, 2.2)
        put("ElbowLeft", -0.22 + walk * 0.04, 1.12, 2.15)
        put("WristLeft", -0.20 + walk * 0.08, 0.88, 2.10)
        put("HandLeft", -0.20 + walk * 0.09, 0.80, 2.08)
        put("HandTipLeft", -0.20 + walk * 0.10, 0.74, 2.06)
        put("ThumbLeft", -0.16 + walk * 0.08, 0.82, 2.06)
        put("ShoulderRight", 0.18, 1.40, 2.2)
        put("ElbowRight", 0.22 - walk * 0.04, 1.12, 2.15)
        put("WristRight", 0.20 - walk * 0.08, 0.88, 2.10)
        put("HandRight", 0.20 - walk * 0.09, 0.80, 2.08)
        put("HandTipRight", 0.20 - walk * 0.10, 0.74, 2.06)
        put("ThumbRight", 0.16 - walk * 0.08, 0.82, 2.06)
        put("HipLeft", -0.09, 0.92, 2.2)
        put("KneeLeft", -0.10, 0.52 + walk * 0.06, 2.18 + walk * 0.04)
        put("AnkleLeft", -0.10, 0.10, 2.16 + walk * 0.08)
        put("FootLeft", -0.10, 0.02, 2.08 + walk * 0.10)
        put("HipRight", 0.09, 0.92, 2.2)
        put("KneeRight", 0.10, 0.52 - walk * 0.06, 2.18 - walk * 0.04)
        put("AnkleRight", 0.10, 0.10, 2.16 - walk * 0.08)
        put("FootRight", 0.10, 0.02, 2.08 + walk * 0.10)
        return BodyFrame(0, True, joints, time.time())


class Kinect20Source(KinectSource):
    """Native Kinect for Windows SDK 2.0 (Kinect20.dll)."""

    def __init__(self, body_index: int = 0) -> None:
        self.body_index = body_index
        self._dll, _ = _kinect_dll()
        self._dll.GetDefaultKinectSensor.argtypes = [POINTER(c_void_p)]
        self._dll.GetDefaultKinectSensor.restype = c_long
        self._sensor = c_void_p()
        hr = self._dll.GetDefaultKinectSensor(byref(self._sensor))
        if hr != S_OK or not self._sensor.value:
            raise RuntimeError("GetDefaultKinectSensor HRESULT=0x%08X" % (hr & 0xFFFFFFFF))
        hr = _vt(self._sensor.value, 6, c_long)(self._sensor.value)
        if hr != S_OK:
            _release(self._sensor.value)
            self._sensor = c_void_p()
            raise RuntimeError("IKinectSensor.Open HRESULT=0x%08X" % (hr & 0xFFFFFFFF))
        src = c_void_p()
        hr = _vt(self._sensor.value, 12, c_long, POINTER(c_void_p))(self._sensor.value, byref(src))
        if hr != S_OK or not src.value:
            self.close()
            raise RuntimeError("get_BodyFrameSource HRESULT=0x%08X" % (hr & 0xFFFFFFFF))
        self._source = src
        reader = c_void_p()
        # IBodyFrameSource: 6 get_IsActive, 7 get_BodyCount, 8 OpenReader
        hr = _vt(self._source.value, 8, c_long, POINTER(c_void_p))(self._source.value, byref(reader))
        if hr != S_OK or not reader.value:
            self.close()
            raise RuntimeError("IBodyFrameSource.OpenReader HRESULT=0x%08X" % (hr & 0xFFFFFFFF))
        self._reader = reader
        self._bodies = (c_void_p * BODY_COUNT)()

    @property
    def backend(self) -> str:
        return "kinect20"

    def poll(self) -> Optional[BodyFrame]:
        if not self._reader or not self._reader.value:
            return None
        frame = c_void_p()
        hr = _vt(self._reader.value, 6, c_long, POINTER(c_void_p))(self._reader.value, byref(frame))
        if (hr & 0xFFFFFFFF) == E_PENDING or hr != S_OK or not frame.value:
            return None
        try:
            hr = _vt(frame.value, 3, c_long, c_ulong, POINTER(c_void_p))(
                frame.value, BODY_COUNT, self._bodies)
            if hr != S_OK:
                return None
            tracked: List[Tuple[int, int]] = []
            for i in range(BODY_COUNT):
                body = self._bodies[i]
                if not body:
                    continue
                flag = c_ubyte(0)
                _vt(body, 15, c_long, POINTER(c_ubyte))(body, byref(flag))
                if flag.value:
                    tracked.append((i, body))
            if not tracked:
                return BodyFrame(self.body_index, False, {}, time.time())
            if self.body_index < len(tracked):
                body = tracked[self.body_index][1]
            else:
                body = tracked[0][1]
            recs = (KinectJointRec * JOINT_COUNT)()
            hr = _vt(body, 3, c_long, c_ulong, POINTER(KinectJointRec))(
                body, JOINT_COUNT, recs)
            if hr != S_OK:
                return BodyFrame(self.body_index, True, {}, time.time())
            joints: Dict[str, KinectJoint] = {}
            for idx, name in enumerate(KINECT_JOINTS):
                j = recs[idx]
                st = int(j.TrackingState)
                conf = CONF_HIGH if st >= 2 else (CONF_MED if st == 1 else CONF_NONE)
                joints[name] = KinectJoint(name, (float(j.Position.X), float(j.Position.Y), float(j.Position.Z)), conf)
            return BodyFrame(self.body_index, True, joints, time.time())
        finally:
            for i in range(BODY_COUNT):
                if self._bodies[i]:
                    _release(self._bodies[i])
                    self._bodies[i] = None
            _release(frame.value)

    def close(self) -> None:
        if getattr(self, "_reader", None) and self._reader.value:
            _release(self._reader.value)
            self._reader = c_void_p()
        if getattr(self, "_source", None) and self._source.value:
            _release(self._source.value)
            self._source = c_void_p()
        if getattr(self, "_sensor", None) and self._sensor.value:
            try:
                _vt(self._sensor.value, 7, c_long)(self._sensor.value)
            except Exception:
                pass
            _release(self._sensor.value)
            self._sensor = c_void_p()


class PyKinect2Source(KinectSource):
    def __init__(self, body_index: int = 0) -> None:
        from pykinect2 import PyKinectRuntime, PyKinectV2  # type: ignore
        self._v2 = PyKinectV2
        self._runtime = PyKinectRuntime.PyKinectRuntime(PyKinectV2.FrameSourceTypes_Body)
        self.body_index = body_index

    @property
    def backend(self) -> str:
        return "pykinect2"

    def poll(self) -> Optional[BodyFrame]:
        if not self._runtime.has_new_body_frame():
            return None
        bodies = self._runtime.get_last_body_frame()
        if bodies is None:
            return None
        tracked = []
        for i, b in enumerate(bodies.bodies):
            if b.is_tracked:
                tracked.append((i, b))
        if not tracked:
            return BodyFrame(self.body_index, False, {}, time.time())
        chosen = tracked[self.body_index][1] if self.body_index < len(tracked) else tracked[0][1]
        joints: Dict[str, KinectJoint] = {}
        for idx, name in enumerate(KINECT_JOINTS):
            j = chosen.joints[idx]
            joints[name] = KinectJoint(
                name,
                (float(j.Position.x), float(j.Position.y), float(j.Position.z)),
                int(j.TrackingState),
            )
        return BodyFrame(self.body_index, True, joints, time.time())

    def close(self) -> None:
        try:
            self._runtime.close()
        except Exception:
            pass


def open_kinect(body_index: int = 0, allow_simulator: bool = True) -> KinectSource:
    errors: List[str] = []
    try:
        return Kinect20Source(body_index)
    except Exception as e:
        errors.append("Kinect20.dll: %s" % e)
    try:
        return PyKinect2Source(body_index)
    except Exception as e:
        errors.append("pykinect2: %s" % e)
    if allow_simulator:
        return SimulatorSource()
    raise RuntimeError("Kinect v2 open failed. " + " | ".join(errors))
