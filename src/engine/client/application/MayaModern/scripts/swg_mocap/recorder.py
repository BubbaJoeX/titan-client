"""Record retargeted frames at SWG 30 fps for bake / ANS export."""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import List

from .retarget import PoseSample


@dataclass
class Clip:
    fps: float = 30.0
    frames: List[PoseSample] = field(default_factory=list)

    def clear(self) -> None:
        self.frames.clear()

    def append(self, sample: PoseSample) -> None:
        self.frames.append(sample)

    @property
    def seconds(self) -> float:
        return len(self.frames) / max(1.0, self.fps)
