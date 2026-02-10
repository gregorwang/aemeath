from __future__ import annotations

from enum import Enum, auto

try:
    from ai.gaze_tracker import GazeData
except ModuleNotFoundError:
    from ..ai.gaze_tracker import GazeData


class PresenceState(Enum):
    PRESENT_ACTIVE = auto()
    PRESENT_PASSIVE = auto()
    ABSENT = auto()
    UNKNOWN = auto()


class PresenceDetector:
    """Fuse idle time and camera face detection to infer presence."""

    IDLE_THRESHOLD_MS = 300_000
    FACE_ABSENT_FRAMES = 30

    def __init__(self):
        self._face_absent_count = 0

    def determine_presence(self, idle_time_ms: int, gaze_data: GazeData | None) -> PresenceState:
        if idle_time_ms < 60_000:
            return PresenceState.PRESENT_ACTIVE

        if gaze_data is None:
            return PresenceState.UNKNOWN

        if not gaze_data.face_detected:
            self._face_absent_count += 1
        else:
            self._face_absent_count = 0

        if idle_time_ms >= self.IDLE_THRESHOLD_MS:
            if self._face_absent_count >= self.FACE_ABSENT_FRAMES:
                return PresenceState.ABSENT
            if gaze_data.face_detected:
                return PresenceState.PRESENT_PASSIVE

        return PresenceState.UNKNOWN
