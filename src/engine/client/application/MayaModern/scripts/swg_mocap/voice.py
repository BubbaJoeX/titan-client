"""Hands-free SAPI 5 voice commands (Kinect mic array or default Windows input)."""

from __future__ import annotations

import os
import tempfile
from typing import Callable, Dict, List, Optional, Tuple

try:
    from PySide6 import QtCore
except ImportError:
    from PySide2 import QtCore  # type: ignore

COMMANDS: List[Tuple[str, List[str]]] = [
    ("connect", ["connect", "connect kinect", "start kinect"]),
    ("disconnect", ["disconnect", "stop kinect"]),
    ("calibrate", ["calibrate", "t pose", "tee pose", "capture pose", "calibrate pose"]),
    ("record", ["record", "start recording", "begin recording"]),
    ("stop", ["stop", "stop recording", "end recording"]),
    ("export ans", ["export ans", "export animation", "bake animation"]),
    ("export fbx", ["export fbx", "export motion builder"]),
    ("adopt", ["use maya sat", "load scene", "adopt scene", "use imported sat"]),
    ("live on", ["live apply", "live on", "apply to maya"]),
    ("live off", ["live off", "stop apply"]),
    ("mesh on", ["show mesh", "mesh on"]),
    ("mesh off", ["hide mesh", "mesh off"]),
    ("simulator", ["simulator", "use simulator"]),
    ("revert", ["revert", "bind pose", "reset pose"]),
]


def match_command(phrase: str) -> Optional[str]:
    text = " ".join((phrase or "").lower().replace("-", " ").split())
    if not text:
        return None
    for cmd, aliases in COMMANDS:
        for a in aliases:
            if a == text or a in text:
                return cmd
    return None


def _grammar_xml() -> str:
    phrases = []
    seen = set()
    for _cmd, aliases in COMMANDS:
        for a in aliases:
            if a not in seen:
                seen.add(a)
                phrases.append("        <P>%s</P>" % a)
    return (
        '<?xml version="1.0" encoding="utf-8"?>\n'
        '<GRAMMAR LANGID="409">\n'
        '  <RULE NAME="swgcmd" TOPLEVEL="ACTIVE">\n'
        "    <L>\n"
        + "\n".join(phrases) +
        "\n    </L>\n"
        "  </RULE>\n"
        "</GRAMMAR>\n"
    )


class _SapiWorker(QtCore.QThread):
    heard = QtCore.Signal(str)
    status = QtCore.Signal(str)

    def __init__(self) -> None:
        super().__init__()
        self._stop = False

    def stop(self) -> None:
        self._stop = True

    def run(self) -> None:
        try:
            from comtypes.client import CreateObject, GetEvents
            import comtypes.client
        except Exception as e:
            self.status.emit("Voice unavailable (comtypes): %s" % e)
            return
        xml_path = os.path.join(tempfile.gettempdir(), "swg_mocap_voice.xml")
        try:
            with open(xml_path, "w", encoding="utf-8") as f:
                f.write(_grammar_xml())
        except Exception as e:
            self.status.emit("Voice grammar write failed: %s" % e)
            return

        class Sink:
            def __init__(self, emit):
                self._emit = emit

            def OnRecognition(self, _sn, _sp, _rt, result):
                try:
                    phrase = result.PhraseInfo.GetText(0, -1, True)
                except Exception:
                    phrase = ""
                if phrase:
                    self._emit(phrase)

        reco = ctx = gram = hook = None
        try:
            reco = CreateObject("SAPI.SpInprocRecognizer")
            ctx = reco.CreateRecoContext()
            gram = ctx.CreateGrammar()
            try:
                gram.CmdLoadFromFile(xml_path)
                gram.CmdSetRuleState("", 1)
            except Exception:
                gram.DictationSetState(1)
            sink = Sink(self.heard.emit)
            hook = GetEvents(ctx, sink)
            self.status.emit("Voice listening (SAPI). Say: connect, calibrate, record, stop, use maya sat.")
            while not self._stop:
                try:
                    comtypes.client.PumpEvents(0.15)
                except Exception:
                    self.msleep(80)
        except Exception as e:
            self.status.emit("Voice start failed: %s" % e)
        finally:
            hook = None
            gram = ctx = reco = None


class VoiceController(QtCore.QObject):
    command = QtCore.Signal(str, str)  # cmd, raw phrase
    status = QtCore.Signal(str)

    def __init__(self, parent=None) -> None:
        super().__init__(parent)
        self._worker: Optional[_SapiWorker] = None

    @property
    def active(self) -> bool:
        return self._worker is not None and self._worker.isRunning()

    def start(self) -> None:
        self.stop()
        self._worker = _SapiWorker()
        self._worker.heard.connect(self._on_heard)
        self._worker.status.connect(self.status)
        self._worker.start()

    def stop(self) -> None:
        if self._worker:
            self._worker.stop()
            self._worker.wait(800)
            self._worker = None

    def _on_heard(self, phrase: str) -> None:
        cmd = match_command(phrase)
        if cmd:
            self.command.emit(cmd, phrase)
        else:
            self.status.emit("Heard “%s” (no command)" % phrase)
